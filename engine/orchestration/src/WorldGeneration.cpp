#include "engine/orchestration/WorldGeneration.hpp"

namespace GYO::Engine::Orchestration {

namespace Detail {

bool IsGenerating(Kernel::Res<WorldGeneration> generation) {
    return generation->generating;
}

bool HasGenerated(Kernel::Res<WorldGeneration> generation) {
    return !generation->generating;
}

} // namespace Detail

Kernel::ConditionRef WhileGenerating() {
    return Kernel::Condition<&Detail::IsGenerating>("WhileGenerating");
}

Kernel::ConditionRef AfterGenerating() {
    return Kernel::Condition<&Detail::HasGenerated>("AfterGenerating");
}

} // namespace GYO::Engine::Orchestration
