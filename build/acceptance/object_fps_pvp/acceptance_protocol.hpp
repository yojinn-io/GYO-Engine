#pragma once

// The client/runtime protocol version the product acceptance probes speak and
// record. This is the only C++ definition in acceptance; the Python analyzers
// have theirs in acceptance_util.py. The product keeps its own constants.
inline constexpr unsigned AcceptanceProtocolVersion = 6;
