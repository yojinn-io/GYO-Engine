"""Makes the doors' two clips (assets/object_fps_pvp/audio/door_open.wav, door_close.wav): synthesized here, the same
bytes every run (a fixed seed), 48 kHz stereo 16-bit as the game's other clips.

    python3 games/object_fps_pvp/sounds-src/door_sounds.py
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
OUT = Path(__file__).resolve().parents[3] / "assets" / "object_fps_pvp" / "audio"


def write(name, samples):
    peak = max(1e-9, max(abs(s) for s in samples))
    frames = b"".join(struct.pack("<hh", v, v) for v in (int(round(s / peak * 0.8 * 32767)) for s in samples))
    with wave.open(str(OUT / name), "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(frames)


def door_open(rng):
    # A creak: a rough tone sliding up, its pulses uneven, fading in and out; then the latch's tick.
    seconds = 0.55
    samples = []
    phase = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        frequency = 170.0 + 120.0 * (t / seconds) + 25.0 * math.sin(2 * math.pi * 7.0 * t)
        phase += frequency / RATE
        tone = (phase % 1.0) * 2.0 - 1.0
        envelope = math.sin(math.pi * min(1.0, t / seconds)) ** 1.5
        samples.append(0.7 * tone * envelope + 0.08 * rng.uniform(-1, 1) * envelope)
    tick = int(0.012 * RATE)
    for i in range(tick):
        samples[i] += rng.uniform(-1, 1) * (1.0 - i / tick)
    return samples


def door_close(rng):
    # A thud: a low tone dying away under a short burst of noise.
    seconds = 0.3
    samples = []
    for i in range(int(seconds * RATE)):
        t = i / RATE
        body = math.sin(2 * math.pi * 85.0 * t) * math.exp(-t * 18.0)
        knock = rng.uniform(-1, 1) * math.exp(-t * 90.0)
        samples.append(0.9 * body + 0.6 * knock)
    return samples


def main():
    rng = random.Random(20261128)
    write("door_open.wav", door_open(rng))
    write("door_close.wav", door_close(rng))


if __name__ == "__main__":
    main()
