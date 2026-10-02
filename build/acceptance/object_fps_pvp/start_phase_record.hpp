// Included only by product acceptance probes and the product's recorder test,
// after their standard (<algorithm>, <array>, <optional>, <vector>), JSON and
// product headers; it includes nothing itself.
// One bounded record per (player, movement epoch, life) of A1 start-phase
// alignment: the Host-reported wait in the probe's own snapshot and the Client
// prediction's wait/shift/skip reason. The Client's first decision keeps the
// frame and steady-clock time it became set; every later frame is folded into
// the record too, because an armed shift is withdrawn while frames run below
// 60 FPS and returns with them, a decision taken below the cut may be armed
// later, and a stall reseed may cancel a pending or decided start phase.
// Observe() only compares and copies inside a timed loop; JSON is built after
// measurement. A product build without these fields records "supported":
// false, one without the skip reason "skip_reason_supported": false, and one
// whose reason cannot name a reseed cancellation "cancel_reason_supported":
// false, instead of guessing.
#pragma once
class StartPhaseRecorder {
public:
    static constexpr std::size_t Capacity = 64;
    // State changes stored per record; later changes are still counted.
    static constexpr std::size_t StateChangeCapacity = 32;
    // The prediction takes a representative wait only once eight positive
    // frame intervals describe the frame rate. The first frame after its
    // Reset() adds none, and a frame's Reconcile runs before its Advance, so an
    // undecided Client seen active for at most this many consecutive frames is
    // still collecting intervals. A Reset and reseed inside one frame is not
    // visible here; without a reported cancellation such an epoch reads
    // not_armed_or_invalidated.
    static constexpr std::uint64_t PendingFrameAllowance = 8 + 2;
    StartPhaseRecorder() { records_.reserve(Capacity); }
    // playerId names the local player whose prediction `client` observes (the
    // observation carries no id); the Host state is keyed by its own playerId.
    // Player id 0 is never a joined player: an active Client observed while the
    // caller knows no player id cannot be attributed and is only counted.
    template<class Host, class Client>
    void Observe(std::uint64_t frame, std::int64_t steadyNs, std::uint64_t playerId, const Host* host, const Client& client) {
        if constexpr (requires { host->playerId; host->movementEpoch; host->lifeGeneration; host->epochStartWaitMicros;
                                 client.active; client.movementEpoch; client.lifeGeneration;
                                 client.epochStartWaitSeconds; client.startPhaseShiftSeconds; }) {
            supported_ = true;
            skipReasonSupported_ = requires { client.startPhaseSkip; };
            cancelReasonSupported_ = CancelReasonOf(client);
            if (host)
                if (auto* record = Find(host->playerId, host->movementEpoch, host->lifeGeneration, frame, steadyNs);
                    record && host->epochStartWaitMicros) {
                    if (!record->hostWaitMicros) { record->hostWaitMicros = *host->epochStartWaitMicros; record->hostFrame = frame; record->hostNs = steadyNs; }
                    else if (*record->hostWaitMicros != *host->epochStartWaitMicros) ++record->hostConflicts;
                }
            // Consecutive frames this player's Client was active: the recorder's
            // view of how long the prediction has been collecting frame intervals.
            // Unattributed frames still belong to the run of the id that follows.
            activeFrames_ = !client.active ? 0 : playerId == activePlayer_ || activePlayer_ == 0 ? activeFrames_ + 1 : 1;
            activePlayer_ = playerId;
            if (client.active) {
                if (playerId == 0) ++unattributedClientFrames_;
                else if (auto* record = Find(playerId, client.movementEpoch, client.lifeGeneration, frame, steadyNs))
                    ObserveClient(*record, frame, steadyNs, client.epochStartWaitSeconds, client.startPhaseShiftSeconds, SkipOf(client));
            }
        } else supported_ = false;
    }
    // Statuses: shift_armed (armed, never withdrawn while observed);
    // shift_withdrawn_below_cut (armed, then withdrawn for withdrawn_frames);
    // shift_armed_after_below_cut_decision (decided below the cut, armed
    // later); skipped_frame_rate_below_tick (decided below the cut, never
    // armed); skipped_host_late; cancelled_by_reseed (the product reported that
    // a stall reseed cancelled the pending or decided start phase; final for
    // its epoch and life); rejected_or_skipped_unknown (kept the phase with no
    // recognised reason); pending_frame_window (Host wait present while the
    // Client was still collecting frame intervals); not_armed_or_invalidated
    // (Host wait present, otherwise never taken); host_wait_absent (no Host
    // wait was seen).
    nlohmann::json Json() const {
        auto epochs = nlohmann::json::array();
        const auto optional = [](const auto& value) { return value ? nlohmann::json(*value) : nlohmann::json(nullptr); };
        const auto when = [](bool set, auto value) { return set ? nlohmann::json(value) : nlohmann::json(nullptr); };
        for (const auto& r : records_) {
            const bool decided = r.clientWaitSeconds.has_value(), observed = r.state != State::None;
            const auto stored = (std::min)(r.changeCount, StateChangeCapacity);
            auto changes = nlohmann::json::array();
            for (std::size_t index = 0; index < stored; ++index)
                changes.push_back(nlohmann::json{{"state", StateName(r.changes[index].state)},
                    {"frame", r.changes[index].frame}, {"steady_ns", r.changes[index].steadyNs}});
            nlohmann::json item{{"player_id", r.player}, {"movement_epoch", r.epoch}, {"life_generation", r.life},
                {"status", Status(r)}, {"first_observed_frame", r.firstFrame}, {"first_observed_steady_ns", r.firstNs},
                {"host_wait_micros", optional(r.hostWaitMicros)},
                {"host_wait_first_frame", when(r.hostWaitMicros.has_value(), r.hostFrame)},
                {"host_wait_first_steady_ns", when(r.hostWaitMicros.has_value(), r.hostNs)},
                {"host_wait_conflicting_frames", r.hostConflicts},
                {"client_wait_seconds", optional(r.clientWaitSeconds)}, {"client_shift_seconds", optional(r.clientShiftSeconds)},
                {"client_skip_reason", SkipName(r.clientSkip)}, {"client_first_frame", when(decided, r.clientFrame)},
                {"client_first_steady_ns", when(decided, r.clientNs)}, {"client_conflicting_frames", r.clientConflicts},
                {"client_armed_shift_seconds", optional(r.armedShift)},
                {"client_first_armed_frame", when(r.armedShift.has_value(), r.armedFrame)},
                {"client_first_armed_steady_ns", when(r.armedShift.has_value(), r.armedNs)},
                {"client_cancelled_frames", r.cancelledFrames},
                {"client_cancelled_first_frame", when(r.cancelledFrames != 0, r.cancelledFrame)},
                {"client_cancelled_first_steady_ns", when(r.cancelledFrames != 0, r.cancelledNs)},
                {"last_client_state", observed ? nlohmann::json(StateName(r.state)) : nlohmann::json(nullptr)},
                {"last_client_frame", when(observed, r.lastFrame)}, {"last_client_steady_ns", when(observed, r.lastNs)},
                {"last_client_wait_seconds", optional(r.lastWait)}, {"last_client_shift_seconds", optional(r.lastShift)},
                {"last_client_skip_reason", SkipName(r.lastSkip)},
                {"withdrawn_frames", r.withdrawnFrames},
                {"withdrawn_first_frame", when(r.withdrawnFrames != 0, r.withdrawnFrame)},
                {"withdrawn_first_steady_ns", when(r.withdrawnFrames != 0, r.withdrawnNs)},
                {"withdrawals", r.withdrawals}, {"restorations", r.restorations},
                {"host_wait_undecided_frames", r.undecidedWithHostWait},
                {"client_active_frames_at_last_undecided", when(r.undecidedWithHostWait != 0, r.activeFramesAtUndecided)},
                {"client_state_changes", std::move(changes)}, {"client_state_changes_dropped", r.changeCount - stored}};
            epochs.push_back(std::move(item));
        }
        return {{"supported", optional(supported_)}, {"skip_reason_supported", optional(skipReasonSupported_)},
            {"cancel_reason_supported", optional(cancelReasonSupported_)},
            {"record_key", "player_id, movement_epoch, life_generation"},
            {"record_capacity", Capacity}, {"dropped_observations", dropped_},
            {"unattributed_client_frames", unattributedClientFrames_},
            {"state_change_capacity", StateChangeCapacity}, {"pending_frame_allowance", PendingFrameAllowance},
            {"status_scope", "client_* fields are the Client's first decision and last_client_* its last observed frame. "
                             "shift_armed: armed and never withdrawn while observed; shift_withdrawn_below_cut: armed, then "
                             "withdrawn below 60 FPS (shift unset, skip frame_rate_below_tick) for withdrawn_frames frames, "
                             "with withdrawals/restorations and last_client_state telling whether it returned; "
                             "pending_frame_window: Host wait present while the Client, active for at most "
                             "pending_frame_allowance frames, was still collecting frame intervals; cancelled_by_reseed: "
                             "the product reported (skip cancelled_by_reseed) that a stall reseed cancelled the pending or "
                             "decided start phase, from client_cancelled_first_frame. With cancel_reason_supported false "
                             "a reseed cancellation is not visible here. Whether a slew completed is not observable "
                             "from this record. unattributed_client_frames: active Client frames observed with player id 0"},
            {"timestamps", "frame-start std::chrono::steady_clock of the observing frame: the first frame for first_* and "
                           "client_first_*, the changing frame for client_state_changes, the last frame for last_client_*"},
            {"epochs", std::move(epochs)}};
    }
private:
    enum class Skip : std::uint8_t { None, HostLate, FrameRateBelowTick, CancelledByReseed, Unrecognised };
    // Withdrawn is below the cut after the record saw an armed shift; BelowCut
    // is below the cut before any. Cancelled is final for its epoch and life.
    enum class State : std::uint8_t { None, Undecided, Armed, Withdrawn, BelowCut, HostLate, Cancelled, Unknown };
    struct Change {
        State state{};
        std::uint64_t frame{};
        std::int64_t steadyNs{};
    };
    struct Record {
        std::uint64_t player{}, epoch{}, life{}, firstFrame{};
        std::int64_t firstNs{};
        std::optional<std::uint32_t> hostWaitMicros;
        std::uint64_t hostFrame{}; std::int64_t hostNs{}; std::uint64_t hostConflicts{};
        // The Client's first decision.
        std::optional<double> clientWaitSeconds, clientShiftSeconds;
        Skip clientSkip{Skip::None};
        State decision{State::None};
        std::uint64_t clientFrame{}; std::int64_t clientNs{}; std::uint64_t clientConflicts{};
        // The first armed shift; a decision below the cut may reach it only later.
        std::optional<double> armedShift;
        std::uint64_t armedFrame{}; std::int64_t armedNs{};
        // The first frame the product reported a reseed cancellation.
        std::uint64_t cancelledFrames{}, cancelledFrame{}; std::int64_t cancelledNs{};
        // The last observed frame.
        State state{State::None};
        std::optional<double> lastWait, lastShift;
        Skip lastSkip{Skip::None};
        std::uint64_t lastFrame{}; std::int64_t lastNs{};
        std::uint64_t withdrawnFrames{}, withdrawnFrame{}; std::int64_t withdrawnNs{};
        std::uint64_t withdrawals{}, restorations{};
        std::uint64_t undecidedWithHostWait{}, activeFramesAtUndecided{};
        std::array<Change, StateChangeCapacity> changes{};
        std::size_t changeCount{};
    };
    // The reason enum is named only through the observation, and each
    // enumerator only once it exists, so this compiles against products with
    // and without LocalMovementObservation::startPhaseSkip or
    // StartPhaseSkip::CancelledByReseed.
    template<class Client>
    static Skip SkipOf(const Client& client) {
        if constexpr (requires { static_cast<bool>(client.startPhaseSkip); *client.startPhaseSkip; }) {
            if (!client.startPhaseSkip) return Skip::None;
            using Reason = std::remove_cvref_t<decltype(*client.startPhaseSkip)>;
            const auto reason = *client.startPhaseSkip;
            if constexpr (requires { Reason::HostLate; }) {
                if (reason == Reason::HostLate) return Skip::HostLate;
            }
            if constexpr (requires { Reason::FrameRateBelowTick; }) {
                if (reason == Reason::FrameRateBelowTick) return Skip::FrameRateBelowTick;
            }
            if constexpr (requires { Reason::CancelledByReseed; }) {
                if (reason == Reason::CancelledByReseed) return Skip::CancelledByReseed;
            }
            return Skip::Unrecognised;
        } else return Skip::None;
    }
    template<class Client>
    static bool CancelReasonOf(const Client& client) {
        if constexpr (requires { *client.startPhaseSkip; }) {
            using Reason = std::remove_cvref_t<decltype(*client.startPhaseSkip)>;
            return requires { Reason::CancelledByReseed; };
        } else return false;
    }
    void ObserveClient(Record& r, std::uint64_t frame, std::int64_t steadyNs, std::optional<double> wait,
                       std::optional<double> shift, Skip skip) {
        const State state = skip == Skip::CancelledByReseed ? State::Cancelled : !wait ? State::Undecided :
            shift ? State::Armed : skip == Skip::HostLate ? State::HostLate :
            skip == Skip::FrameRateBelowTick ? (r.armedShift ? State::Withdrawn : State::BelowCut) : State::Unknown;
        // A cancellation may keep the decided wait or carry none.
        const bool waitKept = state == State::Cancelled ? !wait || !r.clientWaitSeconds || *wait == *r.clientWaitSeconds
                                                        : wait && r.clientWaitSeconds && *wait == *r.clientWaitSeconds;
        if (r.cancelledFrames) {
            // Final for its epoch and life: any other state contradicts it.
            if (state != State::Cancelled || !waitKept) ++r.clientConflicts;
        } else if (r.clientWaitSeconds) {
            // A representative wait may alternate between armed and below the
            // cut, and any decision may be cancelled; anything else, a changed
            // wait or a changed armed shift contradicts the first decision.
            const bool representative = r.decision == State::Armed || r.decision == State::BelowCut;
            const bool reachable = state == State::Cancelled ||
                (representative ? state == State::Armed || state == State::Withdrawn || state == State::BelowCut
                                : state == r.decision);
            if (!waitKept || !reachable || (shift && r.armedShift && *shift != *r.armedShift))
                ++r.clientConflicts;
        } else if (wait && state != State::Cancelled) {
            r.clientWaitSeconds = wait; r.clientShiftSeconds = shift; r.clientSkip = skip; r.decision = state;
            r.clientFrame = frame; r.clientNs = steadyNs;
        }
        if (state == State::Armed && !r.armedShift) { r.armedShift = shift; r.armedFrame = frame; r.armedNs = steadyNs; }
        if (state == State::Withdrawn && r.withdrawnFrames++ == 0) { r.withdrawnFrame = frame; r.withdrawnNs = steadyNs; }
        if (state == State::Cancelled && r.cancelledFrames++ == 0) { r.cancelledFrame = frame; r.cancelledNs = steadyNs; }
        if (state == State::Undecided && r.hostWaitMicros) { ++r.undecidedWithHostWait; r.activeFramesAtUndecided = activeFrames_; }
        if (state != r.state) {
            if (r.state == State::Armed && state == State::Withdrawn) ++r.withdrawals;
            if ((r.state == State::Withdrawn || r.state == State::BelowCut) && state == State::Armed) ++r.restorations;
            if (r.changeCount < StateChangeCapacity) r.changes[r.changeCount] = Change{state, frame, steadyNs};
            ++r.changeCount;
            r.state = state;
        }
        r.lastWait = wait; r.lastShift = shift; r.lastSkip = skip; r.lastFrame = frame; r.lastNs = steadyNs;
    }
    static nlohmann::json SkipName(Skip skip) {
        switch (skip) {
        case Skip::HostLate: return "host_late";
        case Skip::FrameRateBelowTick: return "frame_rate_below_tick";
        case Skip::CancelledByReseed: return "cancelled_by_reseed";
        case Skip::Unrecognised: return "unrecognised";
        case Skip::None: break;
        }
        return nullptr;
    }
    static const char* StateName(State state) {
        switch (state) {
        case State::Undecided: return "undecided";
        case State::Armed: return "shift_armed";
        case State::Withdrawn: return "withdrawn_below_cut";
        case State::BelowCut: return "skipped_below_cut";
        case State::HostLate: return "skipped_host_late";
        case State::Cancelled: return "cancelled_by_reseed";
        case State::Unknown: return "skipped_unknown";
        case State::None: break;
        }
        return "unobserved";
    }
    static const char* Status(const Record& r) {
        if (r.cancelledFrames) return "cancelled_by_reseed";
        if (!r.clientWaitSeconds) {
            if (!r.hostWaitMicros) return "host_wait_absent";
            return r.undecidedWithHostWait && r.activeFramesAtUndecided <= PendingFrameAllowance ? "pending_frame_window"
                                                                                                  : "not_armed_or_invalidated";
        }
        switch (r.decision) {
        case State::HostLate: return "skipped_host_late";
        case State::Armed: case State::BelowCut:
            if (r.withdrawnFrames) return "shift_withdrawn_below_cut";
            if (r.armedShift) return r.decision == State::Armed ? "shift_armed" : "shift_armed_after_below_cut_decision";
            return "skipped_frame_rate_below_tick";
        default: return "rejected_or_skipped_unknown";
        }
    }
    Record* Find(std::uint64_t player, std::uint64_t epoch, std::uint64_t life, std::uint64_t frame, std::int64_t steadyNs) {
        for (auto record = records_.rbegin(); record != records_.rend(); ++record)
            if (record->player == player && record->epoch == epoch && record->life == life) return &*record;
        if (records_.size() == Capacity) { ++dropped_; return nullptr; } // Reserved: no allocation in the loop.
        Record record; record.player = player; record.epoch = epoch; record.life = life; record.firstFrame = frame; record.firstNs = steadyNs;
        records_.push_back(record);
        return &records_.back();
    }
    std::vector<Record> records_;
    std::uint64_t dropped_{}, activeFrames_{}, activePlayer_{}, unattributedClientFrames_{};
    std::optional<bool> supported_, skipReasonSupported_, cancelReasonSupported_;
};
