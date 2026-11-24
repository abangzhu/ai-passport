#!/usr/bin/env python3
"""Exercise the RGB565A8 logo pipeline without ESP-IDF or LVGL."""

import hashlib
import importlib.util
from pathlib import Path
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("generate_marvy_logo", ROOT / "tools/generate_marvy_logo.py")
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class MarvyLogoTest(unittest.TestCase):
    def test_rgb565_is_little_endian_and_alpha_is_a_separate_plane(self):
        image = Image.new("RGBA", (3, 1))
        image.putdata([(255, 0, 0, 255), (0, 255, 0, 128), (0, 0, 255, 0)])
        self.assertEqual(generator.encode_rgb565a8(image),
                         bytes([0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00, 255, 128, 0]))

    def test_rgb565_quantization_uses_all_channel_bits(self):
        image = Image.new("RGBA", (1, 1), (255, 255, 255, 17))
        self.assertEqual(generator.encode_rgb565a8(image), bytes([255, 255, 17]))

    def test_source_attachment_is_preserved_exactly(self):
        source = ROOT / "assets/images/marvy-logo.png"
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), generator.SOURCE_SHA256)
        with Image.open(source) as image:
            self.assertEqual(image.mode, "RGBA")
            self.assertEqual(image.size, (600, 600))
            self.assertEqual(image.getpixel((0, 0))[3], 0)

    def test_both_sizes_preserve_transparency_and_aspect(self):
        with Image.open(ROOT / "assets/images/marvy-logo.png") as image:
            for size in (112, 20):
                scaled = generator.resize_logo(image, size)
                self.assertEqual(scaled.mode, "RGBA")
                self.assertEqual(scaled.size, (size, size))
                self.assertEqual(scaled.getpixel((0, 0))[3], 0)
                self.assertEqual(scaled.getchannel("A").getextrema(), (0, 255))
                self.assertEqual(len(generator.encode_rgb565a8(scaled)), size * size * 3)
                bounds = scaled.getchannel("A").getbbox()
                self.assertIsNotNone(bounds)
                self.assertGreater(bounds[2] - bounds[0], size * 0.85)
                self.assertGreater(bounds[3] - bounds[1], size * 0.75)

    def test_invalid_target_size_is_rejected(self):
        image = Image.new("RGBA", (2, 2))
        for size in (0, -1):
            with self.assertRaises(ValueError):
                generator.resize_logo(image, size)

    def test_non_square_source_is_letterboxed_without_distortion(self):
        image = Image.new("RGBA", (4, 2), (0, 255, 0, 255))
        scaled = generator.resize_logo(image, 20)
        self.assertEqual(scaled.getchannel("A").getbbox(), (0, 5, 20, 15))
        self.assertEqual(scaled.getpixel((10, 10)), (0, 255, 0, 255))

    def test_checked_in_c_assets_match_the_source(self):
        header, source = generator.build_assets(ROOT)
        self.assertEqual((ROOT / "main/marvy_logo.h").read_text(), header)
        self.assertEqual((ROOT / "main/marvy_logo.c").read_text(), source)
        self.assertIn("LV_COLOR_FORMAT_RGB565A8", source)
        self.assertIn(".magic = LV_IMAGE_HEADER_MAGIC", source)
        self.assertIn(".stride = 224", source)
        self.assertIn(".stride = 40", source)


if __name__ == "__main__":
    unittest.main()
