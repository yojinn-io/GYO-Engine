#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "engine/net/NetFailure.hpp"
#include "engine/net/PacketHeader.hpp"

// One connection's transport (declaration D-2): a pure state machine. Its input is a received datagram with
// the current time, its output the datagrams to send now, and what arrived is handed out per channel. It
// touches no socket and reads no clock; whoever drives it owns the Link and the time (D-9: wall time only in
// the IO layer, never in decision data).
//
// Acknowledgement follows Gaffer On Games ("Reliable Ordered Messages") as lightyear does: packet ids only
// increase; every header acknowledges the newest packet id received and the 32 before it; a packet that is
// acknowledged acknowledges the reliable message pieces it carried. Resending is per message piece, not per
// packet: a reliable piece not acknowledged after max(1.5 x RTT, resendMinDelay) goes into the next packet
// (lightyear crates/transport/transport/src/channel/builder.rs:625-638, ReliableSettings).
//
// Every datagram or message piece the Connection drops is counted under one DropReason (D-12), so a test can
// match what was sent against what was handed out and what was dropped.

namespace GYO::Engine::Net {

// Time since an epoch the caller picks; only differences are used.
using NetTime = std::chrono::nanoseconds;

// lightyear crates/io/link/src/mtu.rs:5 (DEFAULT_MTU): fits the common 1280-byte IPv6 minimum with room for
// IP and UDP headers.
inline constexpr std::size_t kDefaultMtu = 1200;

enum class ChannelKind : std::uint8_t {
    // Each message at most once, in any order; a fragmented message is handed out only when every piece
    // arrived, and dropped when they do not arrive within fragmentTimeout.
    Unreliable = 1,
    // Each message exactly once, in the order sent.
    OrderedReliable = 2,
};

struct ChannelSettings final {
    ChannelKind kind = ChannelKind::Unreliable;
    // Pieces of a channel with a higher priority go into a packet before those of a lower one; equal
    // priorities keep the channels' order.
    std::int32_t priority = 0;
};

struct ConnectionSettings final {
    // Written into every header; a received datagram with another id is dropped (ConnectionMismatch).
    std::uint32_t connectionId = 0;
    // Both ends of a connection list the same channels in the same order (at most 255).
    std::vector<ChannelSettings> channels;
    std::size_t mtu = kDefaultMtu;
    // The lower bound of the resend delay. lightyear's default is zero (ReliableSettings::default).
    NetTime resendMinDelay{0};
    // An unreliable fragmented message still incomplete after this long is dropped (lightyear
    // crates/transport/transport/src/channel/receive.rs:53, DISCARD_AFTER = 3000 ms).
    NetTime fragmentTimeout = std::chrono::milliseconds(3000);
    // Reliable messages in flight per channel (sender) and accepted ahead of the next one to hand out
    // (receiver). A packet carrying a reliable piece beyond it is refused unacknowledged (ReliableWindowFull).
    std::uint16_t reliableWindow = 256;
    // Messages per channel waiting to be taken with Pop. A full unreliable inbox drops the message
    // (BufferFull); a full reliable inbox holds the next messages back instead.
    std::size_t inboxCapacity = 1024;
    // Messages per channel not yet sent (unreliable) or not yet acknowledged (reliable).
    std::size_t sendQueueCapacity = 4096;
    // The largest message Send takes (lightyear fragments/receive.rs DEFAULT_MAX_FRAGMENTED_MESSAGE_SIZE).
    std::size_t maxMessageBytes = 16u * 1024u * 1024u;
    // Unreliable fragmented messages being reassembled per channel.
    std::size_t maxPendingFragmented = 64;
    // Disconnect packets sent by Close (lightyear crates/connection/netcode/src/client.rs:57,
    // num_disconnect_packets = 10).
    std::uint32_t disconnectPackets = 10;
};

// Why a datagram or a message piece was dropped. The unit is in each comment: a datagram, a message piece
// (an unfragmented message is one piece, a fragmented one has one per fragment), or a message.
enum class DropReason : std::uint8_t {
    // datagram: the header or the payload cannot be read (Receive also returns the failure).
    DecodeFailed = 0,
    // datagram: another connection's id.
    ConnectionMismatch,
    // datagram: a packet id already received.
    DuplicatePacket,
    // datagram: older than the receive history, so a duplicate cannot be told apart.
    TooOld,
    // datagram: it carries a reliable piece beyond the receive window; refused and not acknowledged, so the
    // sender resends.
    ReliableWindowFull,
    // piece: a reliable piece already received.
    DuplicateMessage,
    // piece: a piece of an unreliable fragmented message that did not complete in time.
    FragmentTimeout,
    // piece: an unreliable message with its inbox full, or a fragment with no reassembly room.
    BufferFull,
    // message: Send refused it (send queue full, or larger than maxMessageBytes).
    SendRejected,
    // datagram (the connection layer): a data packet from an endpoint with no connection.
    UnknownSource,
    // datagram (the connection layer): a handshake packet that does not fit the state (a challenge to a
    // server, an Accepted after the connection was made, a request smaller than a challenge).
    Unexpected,
};
inline constexpr std::size_t kDropReasonCount = 11;

[[nodiscard]] const char* ToString(DropReason reason) noexcept;

struct ChannelStats final {
    // Messages Send took, and messages handed out to Pop.
    std::uint64_t messagesSent = 0;
    std::uint64_t messagesDelivered = 0;
    // Pieces written into datagrams (resends included), pieces of the messages handed out.
    std::uint64_t piecesSent = 0;
    std::uint64_t piecesDelivered = 0;
    // Pieces of this channel in each dropped datagram or dropped on their own, by reason.
    std::array<std::uint64_t, kDropReasonCount> piecesDropped{};
};

struct ConnectionStats final {
    std::uint64_t datagramsSent = 0;
    std::uint64_t datagramsAccepted = 0;
    // Each reason in its own unit (DropReason).
    std::array<std::uint64_t, kDropReasonCount> drops{};
    std::vector<ChannelStats> channels;
};

// The tick and World generation of the newest packet accepted.
struct Stamp final {
    std::uint32_t tick = 0;
    std::uint32_t worldGeneration = 0;

    friend constexpr bool operator==(const Stamp&, const Stamp&) noexcept = default;
};

// What one datagram carries, read without a Connection (diagnostics and tests).
struct DatagramSummary final {
    PacketHeader header;
    // Message pieces per channel index.
    std::vector<std::uint32_t> piecesPerChannel;
};

class Connection final {
public:
    // A Programmer Error when the settings cannot work: more than 255 channels, an mtu too small for a header
    // and one piece, a zero window, or a maxMessageBytes that needs more than 65 535 fragments.
    explicit Connection(ConnectionSettings settings);

    // Queues a message. MessageRejected when the channel's send queue is full or the message is larger than
    // maxMessageBytes (counted as SendRejected). The channel index is a Programmer Error when out of range.
    [[nodiscard]] NetResult<void> Send(std::size_t channel, std::span<const std::byte> message);

    // Takes one datagram from the remote end. A datagram that cannot be read is dropped as DecodeFailed and
    // its failure returned; any other drop is counted and Ok.
    [[nodiscard]] NetResult<void> Receive(std::span<const std::byte> datagram, NetTime now);

    // The datagrams to send now, stamped with stamp: data packets for every piece due, else one AckOnly
    // packet when an acknowledgement is owed. After Close, the Disconnect packets once, then nothing.
    [[nodiscard]] std::vector<std::vector<std::byte>> Collect(NetTime now, Stamp stamp);

    // The next message handed out on channel, or nullopt.
    [[nodiscard]] std::optional<std::vector<std::byte>> Pop(std::size_t channel);

    // One AckOnly packet now, stamped with stamp: the keepalive of an idle connection (the connection layer
    // decides when). Empty after Close.
    [[nodiscard]] std::vector<std::byte> KeepAlive(NetTime now, Stamp stamp);

    // Sends the Disconnect packets on the next Collect; nothing is sent after them.
    void Close() noexcept;
    [[nodiscard]] bool RemoteClosed() const noexcept { return remoteClosed_; }

    // The tick and World generation of the newest packet accepted, or nullopt before the first.
    [[nodiscard]] std::optional<Stamp> RemoteStamp() const noexcept { return remoteStamp_; }

    // The smoothed round-trip time; zero before the first sample.
    [[nodiscard]] NetTime SmoothedRtt() const noexcept { return smoothedRtt_; }

    [[nodiscard]] const ConnectionStats& Stats() const noexcept { return stats_; }
    [[nodiscard]] const ConnectionSettings& Settings() const noexcept { return settings_; }

    // Message pieces queued or in flight (sender) and waiting in buffers (receiver), across channels: zero
    // when everything sent was acknowledged and everything received was handed out or dropped.
    [[nodiscard]] std::size_t PendingPieces() const noexcept;

    // Reads one datagram's header and entries without any state.
    [[nodiscard]] static NetResult<DatagramSummary> Inspect(std::span<const std::byte> datagram);

private:
    struct OutPiece final {
        bool sent = false;
        bool acked = false;
        NetTime lastSent{0};
    };
    struct OutMessage final {
        std::uint16_t id = 0;
        std::uint16_t fragmentCount = 0; // 0: unfragmented
        std::vector<std::byte> bytes;
        std::vector<OutPiece> pieces;
        std::size_t open = 0; // pieces not yet sent (unreliable) or not yet acknowledged (reliable)
    };
    struct InMessage final {
        bool used = false;
        std::uint16_t id = 0;
        std::uint16_t fragmentCount = 0;
        std::vector<std::vector<std::byte>> pieces;
        std::vector<bool> have;
        std::size_t received = 0;
        NetTime firstSeen{0};
    };
    struct Channel final {
        ChannelSettings settings;
        std::deque<OutMessage> outgoing;
        std::uint16_t nextSendId = 0;
        // OrderedReliable receive: the next id to hand out, and a ring of reliableWindow slots.
        std::uint16_t nextReceiveId = 0;
        std::vector<InMessage> window;
        // Unreliable receive: fragmented messages being reassembled, by id.
        std::map<std::uint16_t, InMessage> reassembly;
        std::deque<std::vector<std::byte>> inbox;
    };
    struct PieceRef final {
        std::uint8_t channel = 0;
        std::uint16_t id = 0;
        std::uint16_t index = 0;
    };
    struct SentPacket final {
        bool used = false;
        bool acked = false;
        bool ackEliciting = false;
        std::uint16_t id = 0;
        NetTime sentAt{0};
        std::vector<PieceRef> reliablePieces;
    };
    struct ReceivedSlot final {
        bool used = false;
        std::uint16_t id = 0;
    };

    [[nodiscard]] bool WasReceived(std::uint16_t id) const noexcept;
    void MarkReceived(std::uint16_t id) noexcept;
    void Acknowledge(std::uint16_t id, NetTime now);
    void DeliverReliable(Channel& channel, std::size_t index);
    void ExpireFragments(NetTime now);
    void Drop(DropReason reason, std::uint64_t units) noexcept;
    void DropPieces(std::size_t channel, DropReason reason, std::uint64_t pieces) noexcept;
    [[nodiscard]] std::vector<std::byte> StartPacket(PacketKind kind, Stamp stamp, NetTime now, bool ackEliciting);

    ConnectionSettings settings_;
    std::size_t maxWholePayload_ = 0;
    std::size_t fragmentPayload_ = 0;
    std::vector<Channel> channels_;
    std::vector<std::size_t> priorityOrder_;
    ConnectionStats stats_;

    std::uint16_t nextPacketId_ = 1;
    std::vector<SentPacket> sent_;
    std::vector<ReceivedSlot> received_;
    std::optional<std::uint16_t> newestReceived_;
    bool ackOwed_ = false;
    NetTime smoothedRtt_{0};
    bool haveRtt_ = false;
    std::optional<Stamp> remoteStamp_;
    bool closing_ = false;
    bool closed_ = false;
    bool remoteClosed_ = false;
};

} // namespace GYO::Engine::Net
