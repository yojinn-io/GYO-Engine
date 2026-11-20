#include "engine/world_logging/WriteFailure.hpp"

#include <string>
#include <utility>

#include "engine/orchestration/Product.hpp"

namespace GYO::Engine::WorldLogging {

Logging::FatalWriteReport WriteFailure(const Orchestration::WorldFailure& failure) noexcept {
    try {
        // Only the values gathered when the World broke; never the World or the Kernel.
        const std::string text = failure.Text();
        const std::string_view kernelCode = failure.kernelCode ? Kernel::ToString(*failure.kernelCode) : std::string_view();
        const Logging::Field fields[] = {{"kind", Orchestration::ToString(failure.kind)},
                                         {"entry", std::string_view(failure.entry)},
                                         {"schedule", std::string_view(failure.schedule)},
                                         {"sync_epoch", failure.syncEpoch},
                                         {"where", std::string_view(failure.where)},
                                         {"last_schedule", std::string_view(failure.lastSchedule)},
                                         {"kernel_code", kernelCode}};
        const std::size_t count = failure.kernelCode ? 7 : 6;
        return Logging::WriteFatalRecord(Logging::Record{.level = Logging::Level::Fatal,
                                                         .category = "world",
                                                         .text = text,
                                                         .product = failure.product,
                                                         .world = failure.world,
                                                         .fields = std::span<const Logging::Field>(fields, count)});
    } catch (...) {
        // No memory for the text: the record from the failure's names only.
        return Logging::WriteFatalRecord(Logging::Record{.level = Logging::Level::Fatal,
                                                         .category = "world",
                                                         .text = "a World broke (no memory to describe it)",
                                                         .product = failure.product,
                                                         .world = failure.world});
    }
}

Orchestration::Runner WithFailureLog(Orchestration::Runner runner) {
    return [runner = std::move(runner)](Orchestration::Product& product) -> Orchestration::RunnerExit {
        Orchestration::RunnerExit exit;
        try {
            exit = runner(product);
        } catch (...) {
            const Orchestration::WorldFailure* failure = product.Failure();
            if (failure == nullptr) throw;
            WriteFailure(*failure);
            return {Orchestration::RunnerExitCode::Failure, failure->Text()};
        }
        if (const Orchestration::WorldFailure* failure = product.Failure()) {
            WriteFailure(*failure);
            return {Orchestration::RunnerExitCode::Failure, exit.reason.empty() ? failure->Text() : exit.reason};
        }
        return exit;
    };
}

} // namespace GYO::Engine::WorldLogging
