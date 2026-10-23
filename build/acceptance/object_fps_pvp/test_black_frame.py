"""Unit tests for the record-only window-image classifier; no GUI is started."""
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

import black_frame as frame

FIXTURES = Path(__file__).resolve().parents[3] / "tests" / "object_fps_pvp" / "fixtures" / "black_frame"
SKY, FLOOR, PANEL, WEAPON, SKIN = (61, 73, 88), (110, 123, 132), (43, 51, 62), (70, 66, 60), (200, 150, 120)


def png_bytes(width, height, pixel, alpha=False, filters=(0,)):
    channels = 4 if alpha else 3
    rows, previous = [], bytes(width * channels)
    for y in range(height):
        line = bytes(value for x in range(width) for value in pixel(x, y) + ((255,) if alpha else ()))
        kind = filters[y % len(filters)]
        encoded = bytearray(line)
        for i in range(len(line)):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            predictor = (0, a, b, (a + b) >> 1, a if pa <= pb and pa <= pc else b if pb <= pc else c)[kind]
            encoded[i] = (line[i] - predictor) & 255
        rows.append(bytes([kind]) + bytes(encoded))
        previous = line

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    header = struct.pack(">IIBBBBB", width, height, 8, 6 if alpha else 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows))) + \
        chunk(b"IEND", b"")


def bmp_bytes(width, height, pixel, bits=24, top_down=False):
    step = bits // 8
    stride = (width * step + 3) & ~3
    body = bytearray()
    for row in range(height):
        y = row if top_down else height - 1 - row
        line = bytearray()
        for x in range(width):
            r, g, b = pixel(x, y)
            line += bytes((b, g, r)) + (b"\xff" if bits == 32 else b"")
        body += line + bytes(stride - len(line))
    compression = 3 if bits == 32 else 0
    masks = struct.pack("<III", 0x00FF0000, 0x0000FF00, 0x000000FF) if bits == 32 else b""
    offset = 54 + len(masks)
    header = struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height, 1, bits, compression,
                         len(body), 2835, 2835, 0, 0)
    return b"BM" + struct.pack("<IHHI", offset + len(body), 0, 0, offset) + header + masks + bytes(body)


def scene(background, hud=False, blocks=(), title=0, title_colour=(36, 36, 37), width=320):
    """A synthetic window: `title` rows of title bar above a 16:9 content area."""
    height = round(width * 9 / 16)
    left, upper, right, lower = frame.HUD_RECT

    def pixel(x, y):
        if y < title:
            return title_colour
        fx, fy = x / width, (y - title) / height
        for (x0, y0, x1, y1), colour in blocks:
            if x0 <= fx < x1 and y0 <= fy < y1:
                return colour
        if hud and left <= fx <= right and upper <= fy <= lower:
            return PANEL
        return background(fx, fy) if callable(background) else background
    return frame.Image(width, height + title, pixel)


def world_background(fx, fy):
    return SKY if fy < .45 else FLOOR


# The warmup frame: weapon low in the centre and one Idle character, nothing else.
WARMUP_BLOCKS = (((.40, .55, .65, 1.0), WEAPON), ((.47, .30, .53, .60), SKIN))


class ReaderTests(unittest.TestCase):
    @staticmethod
    def expected(x, y):
        # Pseudo-random bytes, so every filter meets its predictor ties.
        seed = (x * 7919 + y * 104729 + 12345) * 2654435761 % 2 ** 32
        return seed >> 24, (seed >> 16) & 255, (seed >> 8) & 255

    def check(self, image, width, height):
        self.assertEqual((image.width, image.height), (width, height))
        for y in range(height):
            for x in range(width):
                self.assertEqual(image.pixel(x, y), self.expected(x, y), (x, y))

    def decode(self, data):
        with tempfile.TemporaryDirectory(prefix="pvp-black-frame-") as directory:
            path = Path(directory) / "image"
            path.write_bytes(data)
            return frame.read_image(path)

    def test_png_every_filter_rgb_and_rgba(self):
        for alpha in (False, True):
            image = self.decode(png_bytes(32, 20, self.expected, alpha=alpha, filters=(0, 1, 2, 3, 4)))
            self.check(image, 32, 20)

    def test_png_paeth_prefers_left_over_upper_left_on_a_tie(self):
        # Second row, second pixel: left 80, up 110, upper-left 100 -> |p-a| == |p-c| == 10, so left wins.
        values = {(0, 0): 100, (1, 0): 110, (0, 1): 80, (1, 1): 90}
        image = self.decode(png_bytes(2, 2, lambda x, y: (values[x, y],) * 3, filters=(4,)))
        self.assertEqual([image.pixel(x, y)[0] for y in range(2) for x in range(2)], [100, 110, 80, 90])

    def test_bmp_bottom_up_24_bit_with_row_padding(self):
        self.check(self.decode(bmp_bytes(5, 4, self.expected)), 5, 4)

    def test_bmp_top_down_32_bit_bitfields(self):
        self.check(self.decode(bmp_bytes(5, 4, self.expected, bits=32, top_down=True)), 5, 4)

    def test_unsupported_input_is_an_error(self):
        with self.assertRaises(ValueError):
            self.decode(b"GIF89a")
        sixteen = bytearray(bmp_bytes(2, 2, self.expected))
        sixteen[28:30] = struct.pack("<H", 16)
        with self.assertRaises(ValueError):
            self.decode(bytes(sixteen))


class ClassificationTests(unittest.TestCase):
    def kind(self, image):
        return frame.measure(image)["class"]

    def test_world_needs_the_hud_and_a_scene_background(self):
        self.assertEqual(self.kind(scene(world_background, hud=True, blocks=WARMUP_BLOCKS)), "world")

    def test_a_large_panel_is_the_lobby_not_the_world(self):
        lobby = scene((23, 29, 40), blocks=(((.14, .13, .86, .92), PANEL),))
        self.assertEqual(self.kind(lobby), "lobby")

    def test_scene_without_hud_is_no_hud(self):
        self.assertEqual(self.kind(scene(world_background, blocks=WARMUP_BLOCKS)), "no_hud")

    def test_warmup_frame_is_a_black_background_without_hud(self):
        record = frame.measure(scene((0, 0, 0), blocks=WARMUP_BLOCKS))
        self.assertEqual(record["class"], "warmup_like")
        self.assertGreater(record["black_ratio"], frame.BACKGROUND_BLACK)
        self.assertLess(record["black_ratio"], frame.ALL_BLACK)

    def test_black_background_with_hud_is_not_the_warmup_frame(self):
        self.assertEqual(self.kind(scene((0, 0, 0), hud=True, blocks=WARMUP_BLOCKS)), "black_with_hud")

    def test_near_black_still_counts_as_black(self):
        self.assertEqual(self.kind(scene((8, 8, 8), blocks=WARMUP_BLOCKS)), "warmup_like")
        self.assertEqual(self.kind(scene((9, 0, 0), blocks=WARMUP_BLOCKS)), "no_hud")

    def test_nothing_but_black_and_a_blank_window(self):
        self.assertEqual(self.kind(scene((0, 0, 0))), "all_black")
        self.assertEqual(self.kind(scene((236, 236, 236))), "uniform")

    def test_title_bar_rows_are_not_content(self):
        # A black title bar as tall as a quarter of the content changes nothing.
        tall = scene(world_background, hud=True, title=45, title_colour=(0, 0, 0))
        record = frame.measure(tall)
        self.assertEqual(record["class"], "world")
        self.assertEqual(record["black_ratio"], 0)
        self.assertEqual(record["content_top"], 45)
        self.assertEqual(self.kind(scene((0, 0, 0), blocks=WARMUP_BLOCKS, title=45, title_colour=SKY)), "warmup_like")


class CorpusTests(unittest.TestCase):
    """Real images from 2026-10-23 (provenance in the fixture README)."""

    def test_screen_capture_of_the_world(self):
        record = frame.measure(frame.read_image(FIXTURES / "screen-world.png"))
        self.assertEqual(record["class"], "world")
        self.assertEqual(record["black_ratio"], 0)
        self.assertGreater(record["hud_panel_ratio"], .8)
        self.assertGreater(record["clear_ratio"], .2)

    def test_product_lobby_window(self):
        record = frame.measure(frame.read_image(FIXTURES / "window-lobby.png"))
        self.assertEqual(record["class"], "lobby")
        self.assertGreater(record["panel_ratio"], .4)
        self.assertLess(record["black_ratio"], .01)

    def test_scene_readback_has_no_hud_and_no_black(self):
        record = frame.measure(frame.read_image(FIXTURES / "readback-world.png"))
        self.assertEqual(record["class"], "no_hud")
        self.assertEqual(record["black_ratio"], 0)
        self.assertGreater(record["clear_ratio"], .2)


if __name__ == "__main__":
    unittest.main()
