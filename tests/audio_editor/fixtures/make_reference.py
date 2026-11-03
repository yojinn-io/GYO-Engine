#!/usr/bin/env python3
"""Writes the resampling reference of audio_editor's conversion test.

The audio editor takes over the procedure of
docs/architecture/plans/audio/scripts/convert_wav.py (AGENTS.md section 5). Its
acceptance (docs/tools/plans/v8/assets.md, v8-tool-audio-editor-acceptance)
compares the editor's resampling with the script's resample() on synthetic
WAV files. This program writes those files and the script's result:

  reference/<case>.wav       the input
  reference/resample.json    per case: source rate, channels, and the script's
                             resampled planes (before dither), as repr() floats

Run from the repository root; the output is committed. Rerun only when the
script's resampling or the cases change.
  python3 tests/audio_editor/fixtures/make_reference.py
"""
import json
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "docs/architecture/plans/audio/scripts"))
import convert_wav  # noqa: E402

OUT = Path(__file__).resolve().parent / "reference"

# name, rate, channels, format tag, bits, frames
CASES = [
    ("mono_44100_pcm16", 44100, 1, 1, 16, 400),
    ("stereo_22050_pcm24", 22050, 2, 1, 24, 300),
    ("mono_96000_float32", 96000, 1, 3, 32, 500),
    ("stereo_48000_pcm8", 48000, 2, 1, 8, 100),
    ("mono_32000_pcm32", 32000, 1, 1, 32, 200),
]


def signal(frame, channel, rate):
    t = frame / rate
    return 0.55 * math.sin(2 * math.pi * (440 + 110 * channel) * t) + 0.3 * math.sin(2 * math.pi * 5000 * t + channel)


def encode(value, tag, bits):
    if tag == 3:
        return struct.pack("<f", value)
    if bits == 8:
        return bytes([max(0, min(255, int(round(value * 127)) + 128))])
    if bits == 16:
        return struct.pack("<h", int(round(value * 32767)))
    if bits == 24:
        return struct.pack("<i", int(round(value * 8388607)))[:3]
    return struct.pack("<i", int(round(value * 2147483647)))


def write_input(path, rate, channels, tag, bits, frames):
    data = b"".join(encode(signal(f, c, rate), tag, bits) for f in range(frames) for c in range(channels))
    block = channels * bits // 8
    fmt = struct.pack("<HHIIHH", tag, channels, rate, rate * block, block, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    if len(data) & 1:
        body += b"\0"
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def main():
    OUT.mkdir(exist_ok=True)
    reference = {}
    for name, rate, channels, tag, bits, frames in CASES:
        path = OUT / f"{name}.wav"
        write_input(path, rate, channels, tag, bits, frames)
        source_rate, planes, _ = convert_wav.read_wav(path)
        reference[name] = {
            "source_rate": source_rate,
            "channels": len(planes),
            "planes": [[float(repr(v)) for v in convert_wav.resample(plane, source_rate)] for plane in planes],
        }
    text = json.dumps(reference, indent=1)
    (OUT / "resample.json").write_text(text + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
