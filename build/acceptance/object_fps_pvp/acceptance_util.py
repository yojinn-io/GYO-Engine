"""Small helpers shared by the product's acceptance runners and analyzers."""

import hashlib
import math

# The client/runtime protocol version the acceptance analyzers expect. This is the only
# Python definition; the C++ probes have theirs in acceptance_protocol.hpp.
PROTOCOL_VERSION = 6

# How the analyzers' "100 ms stall" reads in practice. A Client frame catches up at most five
# fixed steps (LocalPlayerPrediction.hpp), so time beyond about 83 ms is dropped, and a dropped
# interval (dropped_seconds > 0) already counts as a stall. A frame of about 83 ms plus the rest of
# its work is therefore enough to trigger it; the player-facing thresholds are unchanged.
STALL_RULE = ("frame_seconds >= 100 ms or dropped_seconds > 0; a Client frame catches up at most five "
              "fixed steps, so a frame of about 83 ms already drops time and counts as a stall")


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def rank(values, quantile):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * quantile) - 1)] if ordered else None
