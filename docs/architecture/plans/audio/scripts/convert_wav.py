"""Converts WAV files to the clip format of GYO::Audio: 48 kHz, PCM16, the source's channels (1 or 2).

The Script stage of audio import (AGENTS.md section 5: manual, script, tool, platform): the assets
editor is to take this over (v8-tool-assets-editor). Python standard library only, so it runs the
same on Windows, Linux and macOS, and the output is reproducible: the same input gives the same
bytes.

  - Input: RIFF/WAVE, integer PCM of 8, 16, 24 or 32 bits or 32-bit float (format 1, 3, or 0xFFFE
    with one of those), 1 or 2 channels, any sample rate.
  - Resampling: windowed-sinc interpolation (Blackman window, 32 zero crossings each side, cutoff at
    0.95 of the lower Nyquist frequency), evaluated at each output sample's exact source position.
  - Quantizing to 16 bits: triangular dither of one least significant bit from a fixed seed, then
    saturation; the number of saturated samples is reported.

Usage (from the repository root):
  python3 docs/architecture/plans/audio/scripts/convert_wav.py <input.wav> <output.wav> [...pairs]
Exit status: 0 when every file converted, 1 when one could not be read, 2 on a usage error.
"""
import math
import random
import struct
import sys
from pathlib import Path

TARGET_RATE = 48000
ZERO_CROSSINGS = 32
CUTOFF = 0.95


def read_wav(path):
    data = Path(path).read_bytes()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")
    fmt = None
    frames = None
    offset = 12
    while offset + 8 <= len(data):
        chunk, size = data[offset:offset + 4], struct.unpack_from("<I", data, offset + 4)[0]
        body = data[offset + 8:offset + 8 + size]
        if chunk == b"fmt ":
            tag, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", body)
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack_from("<H", body, 24)[0]
            fmt = (tag, channels, rate, bits)
        elif chunk == b"data":
            frames = body
        offset += 8 + size + (size & 1)
    if fmt is None or frames is None:
        raise ValueError("missing fmt or data chunk")
    tag, channels, rate, bits = fmt
    if channels not in (1, 2):
        raise ValueError(f"{channels} channels; a clip has 1 or 2")
    width = bits // 8
    count = len(frames) // width
    if tag == 3 and bits == 32:
        samples = list(struct.unpack_from(f"<{count}f", frames))
    elif tag == 1 and bits == 8:
        samples = [(b - 128) / 128.0 for b in frames[:count]]
    elif tag == 1 and bits == 16:
        samples = [s / 32768.0 for s in struct.unpack_from(f"<{count}h", frames)]
    elif tag == 1 and bits == 24:
        samples = []
        for i in range(count):
            b = frames[i * 3:i * 3 + 3]
            value = b[0] | (b[1] << 8) | (b[2] << 16)
            samples.append((value - (1 << 24) if value & 0x800000 else value) / 8388608.0)
    elif tag == 1 and bits == 32:
        samples = [s / 2147483648.0 for s in struct.unpack_from(f"<{count}i", frames)]
    else:
        raise ValueError(f"unsupported sample format (tag {tag}, {bits} bits)")
    count -= count % channels
    planes = [samples[c:count:channels] for c in range(channels)]
    return rate, planes, (tag, bits)


def resample(plane, source_rate):
    if source_rate == TARGET_RATE:
        return list(plane)
    ratio = TARGET_RATE / source_rate
    # Cut off below the lower of the two Nyquist frequencies, in source samples.
    cutoff = CUTOFF * min(1.0, ratio)
    half = ZERO_CROSSINGS / cutoff
    n_out = int(math.floor(len(plane) * ratio))
    out = []
    for j in range(n_out):
        x = j / ratio  # this output sample's position in source samples
        first = max(0, int(math.ceil(x - half)))
        last = min(len(plane) - 1, int(math.floor(x + half)))
        acc = 0.0
        for i in range(first, last + 1):
            t = i - x
            arg = cutoff * t
            sinc = 1.0 if arg == 0.0 else math.sin(math.pi * arg) / (math.pi * arg)
            w = 0.42 + 0.5 * math.cos(math.pi * t / half) + 0.08 * math.cos(2 * math.pi * t / half)
            acc += plane[i] * cutoff * sinc * w
        out.append(acc)
    return out


def quantize(planes, seed):
    rng = random.Random(seed)
    clipped = 0
    interleaved = []
    for frame in zip(*planes):
        for value in frame:
            dithered = value * 32767.0 + (rng.random() - rng.random())
            sample = int(round(dithered))
            if sample > 32767 or sample < -32768:
                clipped += 1
                sample = max(-32768, min(32767, sample))
            interleaved.append(sample)
    return interleaved, clipped


def write_wav(path, channels, samples):
    data = struct.pack(f"<{len(samples)}h", *samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, channels, TARGET_RATE, TARGET_RATE * channels * 2,
                                    channels * 2, 16)
    header += b"data" + struct.pack("<I", len(data))
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(header + data)


def convert(source, target):
    rate, planes, (tag, bits) = read_wav(source)
    converted = [resample(plane, rate) for plane in planes]
    # A fixed seed per file name keeps the output reproducible.
    samples, clipped = quantize(converted, Path(target).name)
    write_wav(target, len(planes), samples)
    peak = max((abs(s) for s in samples), default=0) / 32768.0
    print(f"{source} -> {target}: {rate} Hz {bits}-bit (tag {tag}) {len(planes)} ch -> {TARGET_RATE} Hz 16-bit, "
          f"{len(samples) // len(planes)} frames ({len(samples) / len(planes) / TARGET_RATE:.3f} s), "
          f"peak {peak:.3f}, saturated {clipped}")


def main(argv):
    if len(argv) < 2 or len(argv) % 2 != 0:
        print(__doc__, file=sys.stderr)
        return 2
    status = 0
    for source, target in zip(argv[0::2], argv[1::2]):
        try:
            convert(source, target)
        except (OSError, ValueError, struct.error) as error:
            print(f"{source}: {error}", file=sys.stderr)
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
