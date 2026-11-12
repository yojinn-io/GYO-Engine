#pragma once

#include <cstddef>
#include <cstdint>

// Page guard (v8-arch-kernel-spike-pageguard): a third access-check layer
// that sees the write itself, not its net effect at the System boundary.
//
// With the World option pageGuard on, every Exclusive column lives in whole
// pages of its own. During a Run, a (c) System gets write access only to the
// columns it declared as written; every other Exclusive column is read-only.
// A write to a read-only column faults; the process-wide fault handler
// records it in the running System's context (Detail::RunContext), lifts the
// column's protection and lets the write run again, so the System continues
// to its end and the Run returns the violation as Err (World.hpp, System.hpp).
//
// This header is the platform layer only (no OS headers here): page size,
// page allocation, protection changes, the handler, and the table of guarded
// ranges the handler searches. The World decides what to guard and when.
// Implementation: src/PageGuard.cpp (POSIX sigaction for SIGSEGV and SIGBUS,
// Windows AddVectoredExceptionHandler).

namespace KernelSpike::PageGuard {

// The OS page size, queried at run time (not assumed to be 4 KiB).
[[nodiscard]] std::size_t PageSize() noexcept;
[[nodiscard]] std::size_t RoundUpToPages(std::size_t bytes) noexcept;

// bytes (a non-zero multiple of PageSize()) of page-aligned, readable and
// writable memory that shares no page with anything else. Throws
// std::bad_alloc when the OS refuses, like operator new.
[[nodiscard]] std::byte* AllocatePages(std::size_t bytes);
void FreePages(std::byte* pages, std::size_t bytes) noexcept;

// Read-only (writable false) or read-write. False when the OS call fails.
[[nodiscard]] bool SetWritable(std::byte* pages, std::size_t bytes, bool writable) noexcept;

// Installs the process-wide fault handler; only the first call does anything.
// One handler per process: tools that install their own SIGSEGV/SIGBUS
// handler (sanitizers, crash reporters, a test framework's fatal-signal
// report) cannot run alongside it.
void InstallFaultHandler();

// The guarded-range table every World shares. Updated under a lock; the
// handler reads it without one (see src/PageGuard.cpp for the simplification).
void AddRange(const void* world, std::uint32_t column, std::byte* pages, std::size_t bytes);
void RemoveRange(const std::byte* pages) noexcept;

// Diagnostics: faults on guarded columns the handler resolved, and faults on
// guarded columns with no System running on the faulting thread (each of
// those aborts the process right after it is counted).
[[nodiscard]] std::uint64_t HandledFaults() noexcept;
[[nodiscard]] std::uint64_t FaultsWithoutSystem() noexcept;

} // namespace KernelSpike::PageGuard
