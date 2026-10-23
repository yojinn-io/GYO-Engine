"""Record-only classification of GUI window images for N-gui-black-map-intermittent.

Never a verdict: the classes describe what a window showed (the Initialize warmup frame,
the world, the lobby, or something else) so the black-screen observation can be counted.
Reads PNG (8-bit RGB/RGBA, non-interlaced) and uncompressed BMP with the standard library.
"""
import argparse
import json
import struct
import zlib
from pathlib import Path


class Image:
    """Top-down RGB pixels; `pixel(x, y)` decodes one sample on demand."""

    def __init__(self, width, height, pixel):
        self.width, self.height, self.pixel = width, height, pixel


def _png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    offset, header, compressed = 8, None, []
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset:offset + 8])
        body = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.append(body)
        elif kind == b"IEND":
            break
    if header is None:
        raise ValueError("PNG without IHDR")
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace:
        raise ValueError(f"unsupported PNG depth={depth} color={color} interlace={interlace}")
    channels = 3 if color == 2 else 4
    stride = width * channels
    raw = zlib.decompress(b"".join(compressed))
    if len(raw) < (stride + 1) * height:
        raise ValueError("truncated PNG data")
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        if kind == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = previous[i]
                c = previous[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        elif kind != 0:
            raise ValueError(f"unknown PNG filter {kind}")
        previous = line
        rows.append(bytes(line))
    return Image(width, height, lambda x, y: tuple(rows[y][channels * x:channels * x + 3]))


def _bmp(data):
    if data[:2] != b"BM":
        raise ValueError("not a BMP file")
    pixels = struct.unpack("<I", data[10:14])[0]
    _, width, height, _, bits, compression = struct.unpack("<IiiHHI", data[14:34])
    if bits not in (24, 32) or compression not in (0, 3):
        raise ValueError(f"unsupported BMP bits={bits} compression={compression}")
    # BI_BITFIELDS: the masks follow a 40-byte header, or sit inside a larger one.
    masks = (0x00FF0000, 0x0000FF00, 0x000000FF)
    if compression == 3:
        masks = struct.unpack("<III", data[54:66])
    shifts = [(mask & -mask).bit_length() - 1 for mask in masks]
    if any(mask >> shift != 255 for mask, shift in zip(masks, shifts)):
        raise ValueError("unsupported BMP channel masks")
    step = bits // 8
    stride = (width * step + 3) & ~3
    top_down = height < 0
    height = abs(height)
    if len(data) < pixels + stride * height:
        raise ValueError("truncated BMP data")

    def pixel(x, y):
        at = pixels + (y if top_down else height - 1 - y) * stride + x * step
        value = int.from_bytes(data[at:at + step], "little")
        return tuple((value >> shift) & 255 for shift in shifts)
    return Image(width, height, pixel)


def read_image(path):
    data = Path(path).read_bytes()
    return _png(data) if data[:4] == b"\x89PNG" else _bmp(data)


# Declared before any observation run (AGENTS.md §14.1); calibrated on the 2026-10-23 corpus:
# screen sky (61,73,88), readback sky (56,72,89), screen HUD panel (43,51,62).
CONTENT_ASPECT = 9 / 16           # the probes' 1280x720 window; rows above it are the title bar
GRID = (160, 90)                  # sampled columns x rows of the content
BLACK_MAX = 8                     # a channel maximum at or below this is black
CLEAR_COLOR, CLEAR_TOLERANCE = (58, 72, 88), 12
HUD_PANEL, HUD_TOLERANCE = (43, 51, 62), 16
HUD_RECT = (16 / 1280, 16 / 720, 696 / 1280, 130 / 720)   # PvpApplication::Render's top-left panel
HUD_PRESENT = 0.4                 # fraction of HUD-rect samples with the panel colour
LOBBY_PANEL = 0.25                # fraction of all samples with the panel colour: the lobby's large panel
                                  # (0.47 on the product lobby; 0.08 with the in-world HUD; added in O2)
BACKGROUND_BLACK = 0.5            # black fraction: the scene background is black
ALL_BLACK = 0.97                  # nothing but black: not even the weapon or a character
UNIFORM = 0.9                     # one non-black colour (5-bit bins) fills the window
CLASSES = ("all_black", "uniform", "warmup_like", "black_with_hud", "lobby", "world", "no_hud")


def _near(colour, reference, tolerance):
    return all(abs(a - b) <= tolerance for a, b in zip(colour, reference))


def measure(image):
    """Sample the content area and classify it; the result is a record, never a verdict."""
    width = image.width
    content = min(image.height, round(width * CONTENT_ASPECT))
    top = image.height - content
    columns, rows = GRID
    black = clear = panel = hud_samples = hud_panel = 0
    bins = {}
    total = columns * rows
    left, upper, right, lower = HUD_RECT
    for row in range(rows):
        fy = (row + .5) / rows
        y = top + min(content - 1, int(fy * content))
        for column in range(columns):
            fx = (column + .5) / columns
            colour = image.pixel(min(width - 1, int(fx * width)), y)
            if max(colour) <= BLACK_MAX:
                black += 1
            else:
                key = tuple(value >> 3 for value in colour)
                bins[key] = bins.get(key, 0) + 1
            if _near(colour, CLEAR_COLOR, CLEAR_TOLERANCE):
                clear += 1
            near_panel = _near(colour, HUD_PANEL, HUD_TOLERANCE)
            panel += near_panel
            if left <= fx <= right and upper <= fy <= lower:
                hud_samples += 1
                hud_panel += near_panel
    black_ratio, clear_ratio, panel_ratio = black / total, clear / total, panel / total
    dominant = max(bins.values(), default=0) / total
    hud_ratio = hud_panel / hud_samples if hud_samples else 0.0
    hud = hud_ratio >= HUD_PRESENT
    if black_ratio >= ALL_BLACK:
        kind = "all_black"
    elif dominant >= UNIFORM:
        kind = "uniform"
    elif black_ratio >= BACKGROUND_BLACK:
        kind = "black_with_hud" if hud else "warmup_like"
    elif panel_ratio >= LOBBY_PANEL:
        kind = "lobby"
    else:
        kind = "world" if hud else "no_hud"
    return {"class": kind, "black_ratio": round(black_ratio, 4), "clear_ratio": round(clear_ratio, 4),
            "hud_panel_ratio": round(hud_ratio, 4), "panel_ratio": round(panel_ratio, 4),
            "dominant_ratio": round(dominant, 4),
            "width": width, "height": image.height, "content_top": top}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("images", nargs="+", type=Path)
    arguments = parser.parse_args()
    for path in arguments.images:
        try:
            record = {"file": str(path), **measure(read_image(path))}
        except (OSError, ValueError) as error:
            record = {"file": str(path), "error": str(error)}
        print(json.dumps(record, sort_keys=True))


if __name__ == "__main__":
    main()
