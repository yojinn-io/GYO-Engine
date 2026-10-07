#pragma once

// The room capacity the product acceptance probes expect. This is the only C++
// definition in acceptance; the Python analyzers have theirs in
// acceptance_capacity.py. The product keeps its own constants (pv6 contract §1).
inline constexpr unsigned AcceptanceMaxPlayers = 2;
