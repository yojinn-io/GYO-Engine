#pragma once

#include "engine/kernel/Declarations.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Plan (spec D.1): one per schedule, built once at Finish, never rebuilt
// (DI-KS 3, spec D-1, D-6). Serial order of System steps, sync points and the
// event trim. It depends only on registration order and declarations: not on
// hash order, addresses, threads or platform.

namespace GYO::Kernel {

// A SHA-256 over a versioned byte encoding (Fingerprint.cpp). The version is
// part of the hashed bytes and of the text form, so a change of encoding never
// passes for a change of content.
struct Fingerprint {
    std::uint32_t version = 0;
    std::string sha256;

    // "<version>:<64 hex digits>".
    [[nodiscard]] std::string Text() const;

    friend bool operator==(const Fingerprint&, const Fingerprint&) = default;
};

enum class StepKind : std::uint8_t {
    System,
    SyncPoint,
    EventTrim,
};

struct Step {
    StepKind kind;
    // System: its registrationSeq. Otherwise 0.
    std::uint32_t system = 0;
    // System name, or the sync point's name ("<stage>" for a stage end).
    std::string name;
};

// A declared before / after edge between two Systems of a schedule, after set
// expansion, with whether it carries data.
//   dataflow          the migration plan's definition (section 2.1, G1 predicate
//                     (ii)), the one G1 judges by: writes(u) ∩ (reads(v) ∪
//                     writes(v)) ≠ ∅ with writes = immediate writes (columns,
//                     resources, events, the allotter) and reads = immediate
//                     reads (run conditions and SetRead expansions included).
//   dataflowExtended  also counts u's deferred writes and v's With / Without
//                     (membership); reported beside it, not judged by (architect
//                     2026-11-13: whether to extend the definition is the plan's
//                     to decide).
struct DeclaredEdge {
    std::string from;
    std::string to;
    bool dataflow;
    bool dataflowExtended;
};

struct Plan {
    std::string schedule;
    Domain domain = Domain::None;
    std::vector<Step> steps;
    std::vector<DeclaredEdge> declaredEdges;
    Fingerprint fingerprint;

    [[nodiscard]] std::size_t NonDataflowEdges() const noexcept;
    [[nodiscard]] std::size_t NonDataflowEdgesExtended() const noexcept;
};

} // namespace GYO::Kernel
