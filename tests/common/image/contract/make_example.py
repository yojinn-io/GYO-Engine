"""Writes gradient_rgba_16x8.png, the image Data Contract's example from a real
encoder (zlib's compressed blocks, every filter type by row), for
tests/common/image/PngLoaderTests.cpp. Pixel (x, y) is
(16x, 32y, 255 - 16x, 128 + 8y). Run once; the file is checked in.

    python3 -I make_example.py <output directory>
"""

import struct
import sys
import zlib
from pathlib import Path

WIDTH, HEIGHT, BPP = 16, 8, 4


def chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def main() -> None:
    rows = [bytes(v for x in range(WIDTH) for v in (16 * x, 32 * y, 255 - 16 * x, 128 + 8 * y)) for y in range(HEIGHT)]
    raw = bytearray()
    for y, row in enumerate(rows):
        kind = y % 5
        up = rows[y - 1] if y else bytes(len(row))
        raw.append(kind)
        for i, value in enumerate(row):
            a = row[i - BPP] if i >= BPP else 0
            b = up[i]
            c = up[i - BPP] if i >= BPP else 0
            predicted = [0, a, b, (a + b) // 2, paeth(a, b, c)][kind]
            raw.append((value - predicted) & 0xFF)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    Path(sys.argv[1], "gradient_rgba_16x8.png").write_bytes(png)


if __name__ == "__main__":
    main()
