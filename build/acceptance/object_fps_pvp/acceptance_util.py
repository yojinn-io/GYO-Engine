"""Small helpers shared by the product's acceptance runners and analyzers."""

import hashlib
import math

# The client/runtime protocol version the acceptance analyzers expect. This is the only
# Python definition; the C++ probes have theirs in acceptance_protocol.hpp.
PROTOCOL_VERSION = 5


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def rank(values, quantile):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * quantile) - 1)] if ordered else None
