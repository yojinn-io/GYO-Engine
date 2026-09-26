#pragma once

#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <numbers>
#include <optional>

namespace fps::pvp {

struct SnapshotPresentation final {
    PlayerState player;
    std::uint64_t lowerTick{}, upperTick{};
    double presentationTick{}, alpha{};
    double latestReceiveAgeSeconds{}, holdSeconds{}, totalHoldSeconds{};
    std::size_t historySize{};
    std::uint64_t holdCount{}, gapCount{}, historyEvictions{}, phaseReanchors{};
    bool holding{};
    std::uint64_t lowerResolvedCommand{}, upperResolvedCommand{};
    bool missingFutureSnapshot{};
};

// Product-local, receive-stamped state history. Input/network frequency never
// changes the fixed authority slope. This aligns the arrival timeline; it does
// not estimate remote UTC or remove the minimum one-way transport delay.
class SnapshotTimeline final {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t Capacity = 64;
    static constexpr double DelaySeconds = MovementTickSeconds;

    void Reset() noexcept { *this = SnapshotTimeline{}; }

    [[nodiscard]] bool Push(const WorldSnapshot& snapshot, Clock::time_point receivedAt) {
        if (!snapshot.tick || (!history_.empty() && snapshot.tick <= history_.back().snapshot.tick)) return false;
        if (!history_.empty() && receivedAt < history_.back().receivedAt) return false;
        for (std::size_t i = 0; i < snapshot.players.size(); ++i) {
            const auto& player = snapshot.players[i];
            if (!player.playerId || !player.movementEpoch || !std::isfinite(player.position.x) ||
                !std::isfinite(player.position.y) || !std::isfinite(player.position.z) ||
                !std::isfinite(player.yaw) || !std::isfinite(player.pitch)) return false;
            for (std::size_t j = 0; j < i; ++j)
                if (snapshot.players[j].playerId == player.playerId) return false;
            if (!history_.empty()) {
                if (const auto* old = Find(history_.back().snapshot, player.playerId);
                    old && player.movementEpoch < old->movementEpoch) return false;
            }
        }
        if (history_.empty()) { baseTick_ = snapshot.tick; firstReceive_ = receivedAt; }
        else if (snapshot.tick > history_.back().snapshot.tick + 1)
            gaps_ += snapshot.tick - history_.back().snapshot.tick - 1;
        const double offset = Seconds(receivedAt, firstReceive_) - Relative(snapshot.tick) * MovementTickSeconds;
        if (!offsets_.empty() && offset > phase_ + .1) {
            phaseCandidates_.push_back(offset);
            if (phaseCandidates_.size() == 3) {
                offsets_ = phaseCandidates_;
                phaseCandidates_.clear();
                ++phaseReanchors_;
            } else offsets_.push_back(offset);
        } else {
            phaseCandidates_.clear();
            offsets_.push_back(offset);
        }
        while (offsets_.size() > Capacity) offsets_.pop_front();
        phase_ = *std::min_element(offsets_.begin(), offsets_.end());
        history_.push_back({snapshot, receivedAt});
        if (history_.size() > Capacity) { history_.pop_front(); ++evictions_; }
        return true;
    }

    [[nodiscard]] std::optional<SnapshotPresentation> Sample(PlayerId id, Clock::time_point now) {
        if (history_.empty()) return std::nullopt;
        const auto* latest = Find(history_.back().snapshot, id);
        if (!latest) return std::nullopt; // Membership comes from the newest state.
        const double desired = (Seconds(now, firstReceive_) - phase_ - DelaySeconds) / MovementTickSeconds;
        cursor_ = std::max(cursor_, std::clamp(desired, Relative(history_.front().snapshot.tick),
            Relative(history_.back().snapshot.tick)));
        const bool missingFuture = desired > Relative(history_.back().snapshot.tick) + 1e-8;
        // A missing player or a new epoch ends that player's segment only.
        std::size_t first = history_.size() - 1;
        while (first > 0) {
            const auto* previous = Find(history_[first - 1].snapshot, id);
            if (!previous || previous->movementEpoch != latest->movementEpoch) break;
            --first;
        }
        // A stationary pose does not visibly stall when future data is absent.
        // Only the latest two states in this player's current epoch establish
        // motion; another player, a spawn or an epoch reset cannot imply it.
        const auto* prior = first + 1 < history_.size() ?
            Find(history_[history_.size() - 2].snapshot, id) : nullptr;
        const bool holding = missingFuture && prior && PoseChanged(*prior, *latest);
        const double playerCursor = std::clamp(cursor_, Relative(history_[first].snapshot.tick),
            Relative(history_.back().snapshot.tick));
        std::size_t before = first, after = first;
        for (std::size_t index = first; index < history_.size(); ++index) {
            if (Relative(history_[index].snapshot.tick) <= playerCursor) before = index;
            after = index;
            if (Relative(history_[index].snapshot.tick) >= playerCursor) break;
        }
        const auto& a = *Find(history_[before].snapshot, id);
        const auto& b = *Find(history_[after].snapshot, id);
        const auto aTick = history_[before].snapshot.tick, bTick = history_[after].snapshot.tick;
        const double alpha = aTick == bTick ? 0 : std::clamp(
            (playerCursor - Relative(aTick)) / static_cast<double>(bTick - aTick), 0.0, 1.0);
        const float fraction = static_cast<float>(alpha);
        auto player = a;
        player.position = {a.position.x + (b.position.x - a.position.x) * fraction,
                           a.position.y + (b.position.y - a.position.y) * fraction,
                           a.position.z + (b.position.z - a.position.z) * fraction};
        player.yaw = std::remainder(a.yaw + std::remainder(b.yaw - a.yaw,
            2 * std::numbers::pi_v<float>) * fraction, 2 * std::numbers::pi_v<float>);
        player.pitch = a.pitch + (b.pitch - a.pitch) * fraction;
        return SnapshotPresentation{player, aTick, bTick, static_cast<double>(baseTick_) + playerCursor,
            alpha, std::max(0.0, Seconds(now, history_.back().receivedAt)), holdSeconds_, totalHoldSeconds_,
            history_.size(), holdCount_, gaps_, evictions_, phaseReanchors_, holding,
            a.lastResolvedCommand, b.lastResolvedCommand, missingFuture};
    }
    // Commit only after the renderer successfully submits the selected pose.
    // A skipped selection must not create or end a visible hold episode.
    [[nodiscard]] bool CommitPresented(Clock::time_point now, const PlayerState* player, bool movingAtTail) {
        // Selecting successive latest states can keep missingFutureSnapshot
        // true while the visible pose advances normally. A visible hold needs
        // an unchanged pose across two actual submissions in the same epoch.
        const bool holding = movingAtTail && lastMovingAtTail_ && player && lastPresented_ &&
            player->playerId == lastPresented_->playerId &&
            player->movementEpoch == lastPresented_->movementEpoch &&
            !PoseChanged(*player, *lastPresented_) && lastSample_ && now >= *lastSample_;
        if (holding) {
            if (!holding_) { ++holdCount_; holdSeconds_ = 0; }
            const double elapsed = Seconds(now, *lastSample_);
            holdSeconds_ += elapsed;
            totalHoldSeconds_ += elapsed;
        }
        if (!holding) holdSeconds_ = 0;
        holding_ = holding;
        lastMovingAtTail_ = movingAtTail;
        lastPresented_ = player ? std::optional<PlayerState>(*player) : std::nullopt;
        lastSample_ = now;
        return holding;
    }
    [[nodiscard]] std::uint64_t HoldCount() const noexcept { return holdCount_; }
    [[nodiscard]] double HoldSeconds() const noexcept { return holdSeconds_; }
    [[nodiscard]] double TotalHoldSeconds() const noexcept { return totalHoldSeconds_; }
    [[nodiscard]] std::size_t Size() const noexcept { return history_.size(); }

private:
    struct Entry { WorldSnapshot snapshot; Clock::time_point receivedAt; };
    static double Seconds(Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double>(a - b).count();
    }
    static const PlayerState* Find(const WorldSnapshot& snapshot, PlayerId id) {
        const auto found = std::find_if(snapshot.players.begin(), snapshot.players.end(),
            [id](const auto& player) { return player.playerId == id; });
        return found == snapshot.players.end() ? nullptr : &*found;
    }
    static bool PoseChanged(const PlayerState& a, const PlayerState& b) {
        constexpr float epsilon = 1e-6F;
        return std::abs(a.position.x - b.position.x) > epsilon ||
            std::abs(a.position.y - b.position.y) > epsilon ||
            std::abs(a.position.z - b.position.z) > epsilon ||
            std::abs(std::remainder(a.yaw - b.yaw, 2 * std::numbers::pi_v<float>)) > epsilon ||
            std::abs(a.pitch - b.pitch) > epsilon;
    }
    double Relative(std::uint64_t tick) const { return static_cast<double>(tick - baseTick_); }
    std::deque<Entry> history_;
    std::deque<double> offsets_, phaseCandidates_;
    Clock::time_point firstReceive_{};
    std::optional<Clock::time_point> lastSample_;
    std::optional<PlayerState> lastPresented_;
    std::uint64_t baseTick_{}, gaps_{}, evictions_{}, phaseReanchors_{}, holdCount_{};
    double phase_{}, cursor_{}, holdSeconds_{}, totalHoldSeconds_{};
    bool holding_{}, lastMovingAtTail_{};
};
} // namespace fps::pvp
