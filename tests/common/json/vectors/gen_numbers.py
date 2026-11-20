"""Regenerate the number vectors of the JSON library survey and prove they are the same set.

Usage: python3 -I gen_numbers.py <out_dir>

The number part of the survey's generator (v8-arch-json-library-survey, PR #216:
build/evidence/json-library-survey/probe/gen.py in the main checkout, seed
20261113), unchanged in what it draws and in what order, so the same Python
produces the same files. Reference values come from Python float()/int()
(correctly rounded), never from a C++ library. Each file's SHA-256 must equal
the survey's; a mismatch (another Python drawing other numbers, for example)
fails with exit status 1 instead of silently testing a different set
(decision v8-arch-json-fast-float: the survey's vectors on every L1 platform;
docs/architecture/target/v2/json-module-declaration.zh-Hant.md, section 8.2).
"""
import hashlib
import json
import math
import os
import random
import struct
import sys
from decimal import Decimal, getcontext

SEED = 20261113
EXPECTED = {
    "num_fixed.txt": "e195ee51906d61bbf25702b2a9bebf9540215e98153fbc829fad9e33aa1c0b7c",
    "num_fixed_ref.txt": "c99b1b87292f6f91c9c9809ebe6ca28ae90d66ba269b5e3fb2c1d4b7015afcc7",
    "num_gen.txt": "8b0b9525d4267579e6d938d3328ba823614cdd4b5c3ac6a104bd15617dd11af1",
    "num_gen_ref.txt": "5a0baf2d81a392f7da9f8d07488b6325f8f7de5f803c9d2dc9f41153888226a0",
    "num_int.txt": "e8b7a2d6637248763cbc6dd5f3208faf540a36839d6f3322120ff08b56fb9631",
    "num_int_ref.txt": "7c86ca1448c111d96f21b8c15f500d084556efabfe9a5e2fff5d196b04a0504f",
}

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
rng = random.Random(SEED)
getcontext().prec = 2000


def bits(x):
    return "d:%016x" % struct.unpack("<Q", struct.pack("<d", x))[0]


def ref_double(s):
    v = float(s)
    if math.isinf(v):
        return "E:range"
    if v == 0.0 and any(c in "123456789" for c in s.split("e")[0].split("E")[0]):
        return "E:underflow"
    return bits(v)


def w(name, text):
    with open(os.path.join(out, name), "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def rand_finite():
    while True:
        x = struct.unpack("<d", struct.pack("<Q", rng.getrandbits(64)))[0]
        if math.isfinite(x):
            return x


mid_01 = (Decimal(0.1) + Decimal(math.nextafter(0.1, 1))) / 2
fixed = [
    "0.1", "0.7", "20.1", "1e23", "8.98846567431158e307", "2.2250738585072011e-308",
    "2.2250738585072014e-308", "4.9406564584124654e-324", "5e-324", "2.4703282292062328e-324",
    "1.7976931348623157e308", "1.7976931348623158e308", "9007199254740993", "9007199254740993.0",
    "-0", "-0.0", "0e0", "1E+2", "1e-2", "123456789012345678901234567890",
    format(mid_01, "e"), "1.00000000000000011102230246251565404236316680908203125",
    "1." + "0" * 799 + "1", "0." + "0" * 300 + "1", "7.2057594037927933e16", "3.0517578125e-5",
    "1e400", "-1e400", "1e-400", "2.4703282292062327e-324",
    "9223372036854775807", "-9223372036854775808", "9223372036854775808", "-9223372036854775809",
    "18446744073709551615", "18446744073709551616", "1.5", "-1.5", "0.30000000000000004", "1e-7",
]
w("num_fixed.txt", "\n".join(fixed) + "\n")
w("num_fixed_ref.txt", "\n".join(ref_double(s) for s in fixed) + "\n")

gen = []
for _ in range(50000):
    gen.append(repr(rand_finite()))
for _ in range(50000):
    gen.append("%.17e" % rand_finite())
while len(gen) < 150000:
    n = rng.randint(1, 40)
    digits = str(rng.randint(1, 9)) + "".join(str(rng.randint(0, 9)) for _ in range(n - 1))
    e = rng.randint(-330, 308)
    s = ("-" if rng.random() < 0.5 else "") + digits[0] + ("." + digits[1:] if n > 1 else "") + "e" + str(e)
    v = float(s)
    if math.isfinite(v) and v != 0.0:
        gen.append(s)
while len(gen) < 200000:
    x = abs(rand_finite())
    y = math.nextafter(x, math.inf)
    if not math.isfinite(y):
        continue
    m = (Decimal(x) + Decimal(y)) / 2
    gen.append(("-" if rng.random() < 0.5 else "") + format(m, "e"))
for s in gen:
    json.loads(s)
w("num_gen.txt", "\n".join(gen) + "\n")
w("num_gen_ref.txt", "\n".join(ref_double(s) for s in gen) + "\n")

ints = ["0", "-0", "1", "-1", "9223372036854775807", "-9223372036854775808", "9223372036854775808",
        "-9223372036854775809", "18446744073709551615", "18446744073709551616"]
ints += [str(rng.randint(-2**63, 2**63 - 1)) for _ in range(5000)]
ints += [str(rng.randint(2**63, 2**64 - 1)) for _ in range(5000)]


def ref_int(s):
    v = int(s)
    if -2**63 <= v < 2**63:
        return "i:%d" % v
    if 0 <= v < 2**64:
        return "u:%d" % v
    return "E:range"


w("num_int.txt", "\n".join(ints) + "\n")
w("num_int_ref.txt", "\n".join(ref_int(s) for s in ints) + "\n")

failed = False
for name, expected in EXPECTED.items():
    with open(os.path.join(out, name), "rb") as f:
        actual = hashlib.sha256(f.read()).hexdigest()
    if actual != expected:
        print(f"{name}: SHA-256 {actual}, expected {expected} (not the survey's vectors)")
        failed = True
print(f"Python {sys.version.split()[0]}: " + ("MISMATCH" if failed else "the survey's vectors (6 files, SHA-256 match)"))
sys.exit(1 if failed else 0)
