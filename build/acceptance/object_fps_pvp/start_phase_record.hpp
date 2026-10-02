// Included only by product acceptance probes and the product's recorder test,
// after their standard (<algorithm>, <array>, <optional>, <vector>), JSON and
// product headers; it includes nothing itself.
// One bounded record per (player, movement epoch, life) of phase tracking:
// the Host's movement slack samples and connection-quality failures in the
// probe's own snapshot, and the Client prediction's tracking state, decisions
// and corrections. Every frame is folded in: tracking keeps correcting for the
// whole epoch, and a stall reseed restarts it (Acquiring) inside the same
// record. Observe() only compares and copies inside a timed loop; JSON is
// built after measurement. A product build without these fields records
// "supported": false instead of guessing.
#pragma once
class StartPhaseRecorder {
public:
    static constexpr std::size_t Capacity = 64;
    // State changes stored per record; later changes are still counted.
    static constexpr std::size_t StateChangeCapacity = 64;
    StartPhaseRecorder() { records_.reserve(Capacity); }
    // playerId names the local player whose prediction `client` observes (the
    // observation carries no id); the Host state is keyed by its own playerId.
    // Player id 0 is never a joined player: an active Client observed while the
    // caller knows no player id cannot be attributed and is only counted.
    template<class Host, class Client>
    void Observe(std::uint64_t frame, std::int64_t steadyNs, std::uint64_t playerId, const Host* host, const Client& client) {
        if constexpr (requires { host->playerId; host->movementEpoch; host->lifeGeneration; host->movementSlackSequence;
                                 host->movementSlackMicros; host->connectionQualityFailures;
                                 client.active; client.movementEpoch; client.lifeGeneration; client.phaseTracking;
                                 client.phaseErrorSeconds; client.phaseCorrectionSeconds; client.phaseCorrections;
                                 client.phaseLateCorrections; }) {
            supported_ = true;
            if (host)
                if (auto* record = Find(host->playerId, host->movementEpoch, host->lifeGeneration, frame, steadyNs)) {
                    if (host->movementSlackSequence && host->movementSlackMicros &&
                        *host->movementSlackSequence != record->lastSampleSequence) {
                        const auto micros = *host->movementSlackMicros;
                        record->lastSampleSequence = *host->movementSlackSequence;
                        ++record->hostSamples;
                        if (micros < 0) ++record->hostLateSamples;
                        record->hostSlackMin = record->hostSamples == 1 ? micros : (std::min)(record->hostSlackMin, micros);
                        record->hostSlackMax = record->hostSamples == 1 ? micros : (std::max)(record->hostSlackMax, micros);
                    }
                    record->qualityFailuresMax = (std::max)(record->qualityFailuresMax,
                        static_cast<std::uint32_t>(host->connectionQualityFailures));
                }
            if (client.active) {
                if (playerId == 0) ++unattributedClientFrames_;
                else if (auto* record = Find(playerId, client.movementEpoch, client.lifeGeneration, frame, steadyNs))
                    ObserveClient(*record, frame, steadyNs, StateOf(client.phaseTracking), client.phaseErrorSeconds,
                        client.phaseCorrectionSeconds, client.phaseCorrections, client.phaseLateCorrections);
            }
        } else supported_ = false;
    }
    // Statuses: tracking (the Client decided at least once in this epoch and
    // life), acquiring (Host samples arrived but no decision was observed),
    // host_samples_absent (no Host slack sample was seen in the probe's snapshots).
    nlohmann::json Json() const {
        auto epochs = nlohmann::json::array();
        const auto optional = [](const auto& value) { return value ? nlohmann::json(*value) : nlohmann::json(nullptr); };
        const auto when = [](bool set, auto value) { return set ? nlohmann::json(value) : nlohmann::json(nullptr); };
        for (const auto& r : records_) {
            const bool decided = r.firstError.has_value(), observed = r.state != State::None;
            const auto stored = (std::min)(r.changeCount, StateChangeCapacity);
            auto changes = nlohmann::json::array();
            for (std::size_t index = 0; index < stored; ++index)
                changes.push_back(nlohmann::json{{"state", StateName(r.changes[index].state)},
                    {"frame", r.changes[index].frame}, {"steady_ns", r.changes[index].steadyNs}});
            epochs.push_back(nlohmann::json{{"player_id", r.player}, {"movement_epoch", r.epoch}, {"life_generation", r.life},
                {"status", Status(r)}, {"first_observed_frame", r.firstFrame}, {"first_observed_steady_ns", r.firstNs},
                {"host_samples", r.hostSamples}, {"host_late_samples", r.hostLateSamples},
                {"host_slack_min_micros", when(r.hostSamples != 0, r.hostSlackMin)},
                {"host_slack_max_micros", when(r.hostSamples != 0, r.hostSlackMax)},
                {"connection_quality_failures_max", r.qualityFailuresMax},
                {"first_decision_frame", when(decided, r.firstDecisionFrame)},
                {"first_decision_steady_ns", when(decided, r.firstDecisionNs)},
                {"first_error_seconds", optional(r.firstError)},
                {"corrections", r.corrections}, {"late_corrections", r.lateCorrections},
                {"last_error_seconds", optional(r.lastError)}, {"last_correction_seconds", optional(r.lastCorrection)},
                {"last_state", observed ? nlohmann::json(StateName(r.state)) : nlohmann::json(nullptr)},
                {"last_frame", when(observed, r.lastFrame)}, {"last_steady_ns", when(observed, r.lastNs)},
                {"state_changes", std::move(changes)}, {"state_changes_dropped", r.changeCount - stored}});
        }
        return {{"supported", optional(supported_)}, {"record_key", "player_id, movement_epoch, life_generation"},
            {"record_capacity", Capacity}, {"dropped_observations", dropped_},
            {"unattributed_client_frames", unattributedClientFrames_}, {"state_change_capacity", StateChangeCapacity},
            {"status_scope", "tracking: the Client decided its phase at least once in this epoch and life; acquiring: "
                             "Host samples arrived but no decision was observed; host_samples_absent: no Host slack "
                             "sample in the probe's snapshots. state_changes holds acquiring/settling/tracking: a "
                             "stall reseed returns to acquiring, a correction is settling until its slew ends. "
                             "corrections and late_corrections are the Client's counters at the last observed frame"},
            {"timestamps", "frame-start std::chrono::steady_clock of the observing frame: the first frame for first_*, "
                           "the changing frame for state_changes, the last frame for last_*"},
            {"epochs", std::move(epochs)}};
    }
private:
    enum class State : std::uint8_t { None, Acquiring, Settling, Tracking };
    struct Change {
        State state{};
        std::uint64_t frame{};
        std::int64_t steadyNs{};
    };
    struct Record {
        std::uint64_t player{}, epoch{}, life{}, firstFrame{};
        std::int64_t firstNs{};
        std::uint64_t hostSamples{}, hostLateSamples{}, lastSampleSequence{};
        std::int32_t hostSlackMin{}, hostSlackMax{};
        std::uint32_t qualityFailuresMax{};
        std::optional<double> firstError, lastError, lastCorrection;
        std::uint64_t firstDecisionFrame{}; std::int64_t firstDecisionNs{};
        std::uint32_t corrections{}, lateCorrections{};
        State state{State::None};
        std::uint64_t lastFrame{}; std::int64_t lastNs{};
        std::array<Change, StateChangeCapacity> changes{};
        std::size_t changeCount{};
    };
    template<class Value>
    static State StateOf(Value value) {
        using Tracking = std::remove_cvref_t<Value>;
        if (value == Tracking::Settling) return State::Settling;
        if (value == Tracking::Tracking) return State::Tracking;
        return State::Acquiring;
    }
    void ObserveClient(Record& r, std::uint64_t frame, std::int64_t steadyNs, State state, std::optional<double> error,
                       std::optional<double> correction, std::uint32_t corrections, std::uint32_t lateCorrections) {
        if (error && !r.firstError) { r.firstError = error; r.firstDecisionFrame = frame; r.firstDecisionNs = steadyNs; }
        if (error) r.lastError = error;
        if (correction) r.lastCorrection = correction;
        r.corrections = corrections;
        r.lateCorrections = lateCorrections;
        if (state != r.state) {
            if (r.changeCount < StateChangeCapacity) r.changes[r.changeCount] = Change{state, frame, steadyNs};
            ++r.changeCount;
            r.state = state;
        }
        r.lastFrame = frame; r.lastNs = steadyNs;
    }
    static const char* StateName(State state) {
        switch (state) {
        case State::Acquiring: return "acquiring";
        case State::Settling: return "settling";
        case State::Tracking: return "tracking";
        case State::None: break;
        }
        return "unobserved";
    }
    static const char* Status(const Record& r) {
        if (r.firstError) return "tracking";
        return r.hostSamples ? "acquiring" : "host_samples_absent";
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
    std::uint64_t dropped_{}, unattributedClientFrames_{};
    std::optional<bool> supported_;
};
