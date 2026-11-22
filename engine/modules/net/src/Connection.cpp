#include "engine/net/Connection.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "ByteCursor.hpp"
#include "engine/base/Assert.hpp"

namespace GYO::Engine::Net {
namespace {

// Sent packets remembered for their acknowledgement, and received packet ids remembered for telling a
// duplicate apart. Both are far beyond the 33 packets one header acknowledges.
constexpr std::size_t kSentHistory = 1024;
constexpr std::size_t kReceiveHistory = 1024;

// An entry: channel u8, flags u8, id u16, [fragment index u16, fragment count u16], length u16, bytes.
constexpr std::uint8_t kFragmentFlag = 0x01;
constexpr std::size_t kWholeEntryOverhead = 6;
constexpr std::size_t kFragmentEntryOverhead = 10;

struct Entry final {
    std::uint8_t channel = 0;
    bool fragment = false;
    std::uint16_t id = 0;
    std::uint16_t index = 0;
    std::uint16_t count = 0;
    std::span<const std::byte> bytes;
};

struct EntryLimits final {
    std::size_t channelCount = 256;
    std::size_t maxWhole = 0xFFFF;
    std::size_t maxFragment = 0xFFFF;
    std::size_t maxFragments = 0xFFFF;
};

NetFailure Malformed(const char* what, std::string detail) {
    return NetFailure::Make(NetFailureCode::MalformedPayload, what, std::move(detail));
}

NetResult<void> ParseEntries(const std::span<const std::byte> payload, const EntryLimits& limits,
                             std::vector<Entry>& out) {
    Detail::ByteReader reader(payload);
    while (reader.Remaining() > 0) {
        Entry entry;
        std::uint8_t flags = 0;
        std::uint16_t length = 0;
        if (!reader.U8(entry.channel) || !reader.U8(flags) || !reader.U16(entry.id)) {
            return Base::Err(NetFailure::Make(NetFailureCode::Truncated, "an entry header is cut short"));
        }
        if ((flags & ~kFragmentFlag) != 0) return Base::Err(Malformed("unknown entry flags", "flags=" + std::to_string(flags)));
        if (entry.channel >= limits.channelCount) {
            return Base::Err(Malformed("an entry names an unknown channel", "channel=" + std::to_string(entry.channel)));
        }
        entry.fragment = (flags & kFragmentFlag) != 0;
        if (entry.fragment) {
            if (!reader.U16(entry.index) || !reader.U16(entry.count)) {
                return Base::Err(NetFailure::Make(NetFailureCode::Truncated, "a fragment entry header is cut short"));
            }
            if (entry.count < 2 || entry.index >= entry.count || entry.count > limits.maxFragments) {
                return Base::Err(Malformed("a fragment index or count is out of range",
                                           "index=" + std::to_string(entry.index) + "; count=" + std::to_string(entry.count)));
            }
        }
        if (!reader.U16(length)) return Base::Err(NetFailure::Make(NetFailureCode::Truncated, "an entry length is cut short"));
        if (length > (entry.fragment ? limits.maxFragment : limits.maxWhole)) {
            return Base::Err(Malformed("an entry is longer than a piece can be", "length=" + std::to_string(length)));
        }
        if (!reader.Bytes(length, entry.bytes)) {
            return Base::Err(NetFailure::Make(NetFailureCode::Truncated, "an entry's bytes are cut short",
                                              "length=" + std::to_string(length)));
        }
        out.push_back(entry);
    }
    return {};
}

std::vector<std::byte> Concatenate(const std::vector<std::vector<std::byte>>& pieces) {
    std::vector<std::byte> bytes;
    for (const auto& piece : pieces) bytes.insert(bytes.end(), piece.begin(), piece.end());
    return bytes;
}

bool IsPacketLevel(const DropReason reason) noexcept {
    switch (reason) {
    case DropReason::DecodeFailed:
    case DropReason::ConnectionMismatch:
    case DropReason::DuplicatePacket:
    case DropReason::TooOld:
    case DropReason::ReliableWindowFull:
    case DropReason::UnknownSource:
    case DropReason::Unexpected: return true;
    default: return false;
    }
}

} // namespace

const char* ToString(const DropReason reason) noexcept {
    switch (reason) {
    case DropReason::DecodeFailed: return "DecodeFailed";
    case DropReason::ConnectionMismatch: return "ConnectionMismatch";
    case DropReason::DuplicatePacket: return "DuplicatePacket";
    case DropReason::TooOld: return "TooOld";
    case DropReason::ReliableWindowFull: return "ReliableWindowFull";
    case DropReason::DuplicateMessage: return "DuplicateMessage";
    case DropReason::FragmentTimeout: return "FragmentTimeout";
    case DropReason::BufferFull: return "BufferFull";
    case DropReason::SendRejected: return "SendRejected";
    case DropReason::UnknownSource: return "UnknownSource";
    case DropReason::Unexpected: return "Unexpected";
    }
    return "Unknown";
}

Connection::Connection(ConnectionSettings settings) : settings_(std::move(settings)) {
    GYO_ASSERT(settings_.channels.size() <= 255);
    GYO_ASSERT(settings_.mtu > kPacketHeaderSize + kFragmentEntryOverhead);
    GYO_ASSERT(settings_.reliableWindow >= 1 && settings_.reliableWindow <= 0x8000u);
    maxWholePayload_ = std::min<std::size_t>(settings_.mtu - kPacketHeaderSize - kWholeEntryOverhead, 0xFFFF);
    fragmentPayload_ = std::min<std::size_t>(settings_.mtu - kPacketHeaderSize - kFragmentEntryOverhead, 0xFFFF);
    GYO_ASSERT((settings_.maxMessageBytes + fragmentPayload_ - 1) / fragmentPayload_ <= 0xFFFF);

    channels_.resize(settings_.channels.size());
    stats_.channels.resize(settings_.channels.size());
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        channels_[i].settings = settings_.channels[i];
        if (channels_[i].settings.kind == ChannelKind::OrderedReliable) {
            channels_[i].window.resize(settings_.reliableWindow);
        }
        priorityOrder_.push_back(i);
    }
    std::stable_sort(priorityOrder_.begin(), priorityOrder_.end(), [this](std::size_t a, std::size_t b) {
        return channels_[a].settings.priority > channels_[b].settings.priority;
    });
    sent_.resize(kSentHistory);
    received_.resize(kReceiveHistory);
}

void Connection::Drop(const DropReason reason, const std::uint64_t units) noexcept {
    stats_.drops[static_cast<std::size_t>(reason)] += units;
}

void Connection::DropPieces(const std::size_t channel, const DropReason reason, const std::uint64_t pieces) noexcept {
    stats_.channels[channel].piecesDropped[static_cast<std::size_t>(reason)] += pieces;
    if (!IsPacketLevel(reason)) Drop(reason, pieces);
}

NetResult<void> Connection::Send(const std::size_t channelIndex, const std::span<const std::byte> message) {
    GYO_ASSERT(channelIndex < channels_.size());
    auto& channel = channels_[channelIndex];
    if (message.size() > settings_.maxMessageBytes || channel.outgoing.size() >= settings_.sendQueueCapacity) {
        Drop(DropReason::SendRejected, 1);
        return Base::Err(NetFailure::Make(
            NetFailureCode::MessageRejected,
            message.size() > settings_.maxMessageBytes ? "the message is larger than maxMessageBytes"
                                                       : "the channel's send queue is full",
            "channel=" + std::to_string(channelIndex) + "; bytes=" + std::to_string(message.size())));
    }
    OutMessage out;
    const bool reliable = channel.settings.kind == ChannelKind::OrderedReliable;
    const bool fragmented = message.size() > maxWholePayload_;
    out.fragmentCount =
        fragmented ? static_cast<std::uint16_t>((message.size() + fragmentPayload_ - 1) / fragmentPayload_) : 0;
    out.id = (reliable || fragmented) ? channel.nextSendId++ : 0;
    out.bytes.assign(message.begin(), message.end());
    out.pieces.resize(fragmented ? out.fragmentCount : 1);
    out.open = out.pieces.size();
    channel.outgoing.push_back(std::move(out));
    ++stats_.channels[channelIndex].messagesSent;
    return {};
}

bool Connection::WasReceived(const std::uint16_t id) const noexcept {
    const auto& slot = received_[id % kReceiveHistory];
    return slot.used && slot.id == id;
}

void Connection::MarkReceived(const std::uint16_t id) noexcept {
    if (!newestReceived_) {
        newestReceived_ = id;
    } else if (SequenceNewer(id, *newestReceived_)) {
        // Forget the slots the newest id skips over, so a slot never answers for an id a wrap ago.
        const std::size_t skipped =
            std::min<std::size_t>(static_cast<std::uint16_t>(id - *newestReceived_), kReceiveHistory);
        for (std::size_t i = 1; i <= skipped; ++i) {
            received_[static_cast<std::uint16_t>(*newestReceived_ + i) % kReceiveHistory].used = false;
        }
        newestReceived_ = id;
    }
    received_[id % kReceiveHistory] = ReceivedSlot{true, id};
}

void Connection::Acknowledge(const std::uint16_t id, const NetTime now) {
    auto& packet = sent_[id % kSentHistory];
    if (!packet.used || packet.id != id || packet.acked) return;
    packet.acked = true;
    if (packet.ackEliciting) {
        const NetTime sample = std::max(now - packet.sentAt, NetTime{0});
        // EWMA with lightyear's weight 1/12 (crates/core/sync/src/ping/estimator.rs:4, RTT_EWMA_ALPHA), in
        // integer nanoseconds so every platform computes the same value.
        smoothedRtt_ = haveRtt_ ? smoothedRtt_ + (sample - smoothedRtt_) / 12 : sample;
        haveRtt_ = true;
    }
    for (const PieceRef& ref : packet.reliablePieces) {
        auto& channel = channels_[ref.channel];
        if (channel.outgoing.empty()) continue;
        const std::size_t offset = static_cast<std::uint16_t>(ref.id - channel.outgoing.front().id);
        if (offset >= channel.outgoing.size()) continue;
        auto& message = channel.outgoing[offset];
        auto& piece = message.pieces[ref.index];
        if (!piece.acked) {
            piece.acked = true;
            --message.open;
        }
    }
    packet.reliablePieces.clear();
    for (auto& channel : channels_) {
        if (channel.settings.kind != ChannelKind::OrderedReliable) continue;
        while (!channel.outgoing.empty() && channel.outgoing.front().open == 0) channel.outgoing.pop_front();
    }
}

void Connection::DeliverReliable(Channel& channel, const std::size_t index) {
    while (channel.inbox.size() < settings_.inboxCapacity) {
        auto& slot = channel.window[channel.nextReceiveId % channel.window.size()];
        if (!slot.used || slot.id != channel.nextReceiveId || slot.received != slot.pieces.size()) return;
        channel.inbox.push_back(Concatenate(slot.pieces));
        ++stats_.channels[index].messagesDelivered;
        stats_.channels[index].piecesDelivered += slot.pieces.size();
        slot = InMessage{};
        ++channel.nextReceiveId;
    }
}

void Connection::ExpireFragments(const NetTime now) {
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        auto& reassembly = channels_[i].reassembly;
        for (auto it = reassembly.begin(); it != reassembly.end();) {
            if (now - it->second.firstSeen >= settings_.fragmentTimeout) {
                DropPieces(i, DropReason::FragmentTimeout, it->second.received);
                it = reassembly.erase(it);
            } else {
                ++it;
            }
        }
    }
}

NetResult<void> Connection::Receive(const std::span<const std::byte> datagram, const NetTime now) {
    auto decoded = DecodePacketHeader(datagram);
    if (!decoded) {
        Drop(DropReason::DecodeFailed, 1);
        return Base::Err(std::move(decoded).error());
    }
    const PacketHeader header = *decoded;
    if (header.connectionId != settings_.connectionId) {
        Drop(DropReason::ConnectionMismatch, 1);
        return {};
    }
    std::vector<Entry> entries;
    const EntryLimits limits{channels_.size(), maxWholePayload_, fragmentPayload_,
                             (settings_.maxMessageBytes + fragmentPayload_ - 1) / fragmentPayload_};
    auto parsed = ParseEntries(datagram.subspan(kPacketHeaderSize), limits, entries);
    if (parsed && IsHandshake(header.kind)) {
        parsed = Base::Err(Malformed("a handshake packet is not the transport's", "kind=" + std::to_string(static_cast<int>(header.kind))));
    }
    if (parsed && header.kind != PacketKind::Data && !entries.empty()) {
        parsed = Base::Err(Malformed("only a Data packet carries entries", "entries=" + std::to_string(entries.size())));
    }
    if (!parsed) {
        Drop(DropReason::DecodeFailed, 1);
        return parsed;
    }
    ExpireFragments(now);

    // Refuse the whole datagram before anything in it is used, so a refused packet is never acknowledged.
    auto refuse = [&](const DropReason reason) {
        Drop(reason, 1);
        for (const Entry& entry : entries) DropPieces(entry.channel, reason, 1);
    };
    if (newestReceived_ && !SequenceNewer(header.packetId, *newestReceived_)) {
        const std::size_t behind = static_cast<std::uint16_t>(*newestReceived_ - header.packetId);
        if (behind >= kReceiveHistory) {
            refuse(DropReason::TooOld);
            return {};
        }
        if (WasReceived(header.packetId)) {
            refuse(DropReason::DuplicatePacket);
            return {};
        }
    }
    for (const Entry& entry : entries) {
        const auto& channel = channels_[entry.channel];
        if (channel.settings.kind != ChannelKind::OrderedReliable) continue;
        if (SequenceNewer(channel.nextReceiveId, entry.id)) continue; // already handed out: a duplicate below
        if (static_cast<std::uint16_t>(entry.id - channel.nextReceiveId) >= channel.window.size()) {
            refuse(DropReason::ReliableWindowFull);
            return {};
        }
    }

    const bool newest = !newestReceived_ || SequenceNewer(header.packetId, *newestReceived_);
    MarkReceived(header.packetId);
    ++stats_.datagramsAccepted;
    if (newest) remoteStamp_ = Stamp{header.tick, header.worldGeneration};
    if (header.kind == PacketKind::Data) ackOwed_ = true;
    if (header.kind == PacketKind::Disconnect) remoteClosed_ = true;
    Acknowledge(header.ack, now);
    for (std::uint16_t i = 0; i < 32; ++i) {
        if ((header.ackBits >> i) & 1u) Acknowledge(static_cast<std::uint16_t>(header.ack - 1 - i), now);
    }

    for (const Entry& entry : entries) {
        auto& channel = channels_[entry.channel];
        auto& stats = stats_.channels[entry.channel];
        const std::size_t pieceCount = entry.fragment ? entry.count : 1;
        const std::size_t pieceIndex = entry.fragment ? entry.index : 0;
        if (channel.settings.kind == ChannelKind::OrderedReliable) {
            if (SequenceNewer(channel.nextReceiveId, entry.id)) {
                DropPieces(entry.channel, DropReason::DuplicateMessage, 1);
                continue;
            }
            auto& slot = channel.window[entry.id % channel.window.size()];
            if (!slot.used) {
                slot.used = true;
                slot.id = entry.id;
                slot.fragmentCount = entry.fragment ? entry.count : 0;
                slot.pieces.assign(pieceCount, {});
                slot.have.assign(pieceCount, false);
                slot.firstSeen = now;
            } else if (slot.pieces.size() != pieceCount) {
                // The same message announced with another fragment count: the remote end broke the protocol.
                DropPieces(entry.channel, DropReason::DecodeFailed, 1);
                continue;
            }
            if (slot.have[pieceIndex]) {
                DropPieces(entry.channel, DropReason::DuplicateMessage, 1);
                continue;
            }
            slot.have[pieceIndex] = true;
            slot.pieces[pieceIndex].assign(entry.bytes.begin(), entry.bytes.end());
            ++slot.received;
            continue;
        }
        if (!entry.fragment) {
            if (channel.inbox.size() >= settings_.inboxCapacity) {
                DropPieces(entry.channel, DropReason::BufferFull, 1);
                continue;
            }
            channel.inbox.emplace_back(entry.bytes.begin(), entry.bytes.end());
            ++stats.messagesDelivered;
            ++stats.piecesDelivered;
            continue;
        }
        auto found = channel.reassembly.find(entry.id);
        if (found != channel.reassembly.end() && found->second.pieces.size() != pieceCount) {
            // A new message reusing a wrapped id while an old one is incomplete: the old one is given up.
            DropPieces(entry.channel, DropReason::FragmentTimeout, found->second.received);
            channel.reassembly.erase(found);
            found = channel.reassembly.end();
        }
        if (found == channel.reassembly.end()) {
            if (channel.reassembly.size() >= settings_.maxPendingFragmented) {
                DropPieces(entry.channel, DropReason::BufferFull, 1);
                continue;
            }
            InMessage fresh;
            fresh.used = true;
            fresh.id = entry.id;
            fresh.fragmentCount = entry.count;
            fresh.pieces.assign(pieceCount, {});
            fresh.have.assign(pieceCount, false);
            fresh.firstSeen = now;
            found = channel.reassembly.emplace(entry.id, std::move(fresh)).first;
        }
        auto& message = found->second;
        if (message.have[pieceIndex]) {
            DropPieces(entry.channel, DropReason::DuplicateMessage, 1);
            continue;
        }
        message.have[pieceIndex] = true;
        message.pieces[pieceIndex].assign(entry.bytes.begin(), entry.bytes.end());
        ++message.received;
        if (message.received < message.pieces.size()) continue;
        if (channel.inbox.size() >= settings_.inboxCapacity) {
            DropPieces(entry.channel, DropReason::BufferFull, message.received);
        } else {
            channel.inbox.push_back(Concatenate(message.pieces));
            ++stats.messagesDelivered;
            stats.piecesDelivered += message.pieces.size();
        }
        channel.reassembly.erase(found);
    }
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        if (channels_[i].settings.kind == ChannelKind::OrderedReliable) DeliverReliable(channels_[i], i);
    }
    return {};
}

std::vector<std::byte> Connection::StartPacket(const PacketKind kind, const Stamp stamp, const NetTime now,
                                               const bool ackEliciting) {
    PacketHeader header;
    header.kind = kind;
    header.connectionId = settings_.connectionId;
    header.worldGeneration = stamp.worldGeneration;
    header.tick = stamp.tick;
    header.packetId = nextPacketId_++;
    // Before anything was received the ack fields say "packet 0": the first packet id sent is 1, so id 0 is
    // reached only after a full wrap, when something has long been received.
    header.ack = newestReceived_.value_or(0);
    if (newestReceived_) {
        for (std::uint16_t i = 0; i < 32; ++i) {
            if (WasReceived(static_cast<std::uint16_t>(header.ack - 1 - i))) header.ackBits |= (1u << i);
        }
    }
    auto& record = sent_[header.packetId % kSentHistory];
    record = SentPacket{true, false, ackEliciting, header.packetId, now, {}};
    std::vector<std::byte> bytes;
    bytes.reserve(settings_.mtu);
    EncodePacketHeader(header, bytes);
    ackOwed_ = false;
    ++stats_.datagramsSent;
    return bytes;
}

std::vector<std::vector<std::byte>> Connection::Collect(const NetTime now, const Stamp stamp) {
    ExpireFragments(now);
    std::vector<std::vector<std::byte>> datagrams;
    if (closed_) return datagrams;
    if (closing_) {
        for (std::uint32_t i = 0; i < settings_.disconnectPackets; ++i) {
            datagrams.push_back(StartPacket(PacketKind::Disconnect, stamp, now, false));
        }
        closed_ = true;
        return datagrams;
    }

    // lightyear ReliableSettings: rtt_resend_factor 1.5, rtt_resend_min_delay as the lower bound.
    const NetTime resendDelay = std::max(smoothedRtt_ * 3 / 2, settings_.resendMinDelay);
    std::vector<std::byte> current;
    std::uint16_t currentId = 0;
    std::vector<PieceRef> currentRefs;
    auto flush = [&] {
        if (current.empty()) return;
        sent_[currentId % kSentHistory].reliablePieces = std::move(currentRefs);
        currentRefs = {};
        datagrams.push_back(std::move(current));
        current = {};
    };

    for (const std::size_t index : priorityOrder_) {
        auto& channel = channels_[index];
        const bool reliable = channel.settings.kind == ChannelKind::OrderedReliable;
        for (std::size_t m = 0; m < channel.outgoing.size(); ++m) {
            if (reliable && m >= settings_.reliableWindow) break;
            auto& message = channel.outgoing[m];
            for (std::size_t i = 0; i < message.pieces.size(); ++i) {
                auto& piece = message.pieces[i];
                const bool due = reliable ? (!piece.acked && (!piece.sent || now - piece.lastSent >= resendDelay))
                                          : !piece.sent;
                if (!due) continue;
                const bool fragment = message.fragmentCount != 0;
                const std::size_t begin = fragment ? i * fragmentPayload_ : 0;
                const std::size_t end =
                    fragment ? std::min(begin + fragmentPayload_, message.bytes.size()) : message.bytes.size();
                const std::size_t size = (fragment ? kFragmentEntryOverhead : kWholeEntryOverhead) + (end - begin);
                if (!current.empty() && current.size() + size > settings_.mtu) flush();
                if (current.empty()) {
                    currentId = nextPacketId_;
                    current = StartPacket(PacketKind::Data, stamp, now, true);
                }
                Detail::ByteWriter writer(current);
                writer.U8(static_cast<std::uint8_t>(index));
                writer.U8(fragment ? kFragmentFlag : 0);
                writer.U16(message.id);
                if (fragment) {
                    writer.U16(static_cast<std::uint16_t>(i));
                    writer.U16(message.fragmentCount);
                }
                writer.U16(static_cast<std::uint16_t>(end - begin));
                writer.Bytes(std::span<const std::byte>(message.bytes).subspan(begin, end - begin));
                ++stats_.channels[index].piecesSent;
                if (reliable) {
                    currentRefs.push_back(PieceRef{static_cast<std::uint8_t>(index), message.id,
                                                   static_cast<std::uint16_t>(i)});
                } else {
                    --message.open;
                }
                piece.sent = true;
                piece.lastSent = now;
            }
        }
        if (!reliable) {
            while (!channel.outgoing.empty() && channel.outgoing.front().open == 0) channel.outgoing.pop_front();
        }
    }
    flush();
    if (datagrams.empty() && ackOwed_) datagrams.push_back(StartPacket(PacketKind::AckOnly, stamp, now, false));
    return datagrams;
}

std::vector<std::byte> Connection::KeepAlive(const NetTime now, const Stamp stamp) {
    if (closing_ || closed_) return {};
    return StartPacket(PacketKind::AckOnly, stamp, now, false);
}

std::optional<std::vector<std::byte>> Connection::Pop(const std::size_t channelIndex) {
    GYO_ASSERT(channelIndex < channels_.size());
    auto& channel = channels_[channelIndex];
    if (channel.inbox.empty()) return std::nullopt;
    auto message = std::move(channel.inbox.front());
    channel.inbox.pop_front();
    if (channel.settings.kind == ChannelKind::OrderedReliable) DeliverReliable(channel, channelIndex);
    return message;
}

void Connection::Close() noexcept {
    if (!closed_) closing_ = true;
}

std::size_t Connection::PendingPieces() const noexcept {
    std::size_t pending = 0;
    for (const auto& channel : channels_) {
        for (const auto& message : channel.outgoing) pending += message.open;
        for (const auto& slot : channel.window) pending += slot.used ? slot.received : 0;
        for (const auto& [id, message] : channel.reassembly) pending += message.received;
    }
    return pending;
}

NetResult<DatagramSummary> Connection::Inspect(const std::span<const std::byte> datagram) {
    auto header = DecodePacketHeader(datagram);
    if (!header) return Base::Err(std::move(header).error());
    std::vector<Entry> entries;
    auto parsed = ParseEntries(datagram.subspan(kPacketHeaderSize), EntryLimits{}, entries);
    if (!parsed) return Base::Err(std::move(parsed).error());
    DatagramSummary summary{*header, {}};
    for (const Entry& entry : entries) {
        if (summary.piecesPerChannel.size() <= entry.channel) summary.piecesPerChannel.resize(entry.channel + 1u, 0);
        ++summary.piecesPerChannel[entry.channel];
    }
    return summary;
}

} // namespace GYO::Engine::Net
