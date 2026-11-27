#!/usr/bin/env python3
"""Verify the supplied follow-QR resource without an on-device image decoder."""

import hashlib
import importlib.util
from pathlib import Path
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("generate_marvy_qr", ROOT / "tools/generate_marvy_qr.py")
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class MarvyQrTest(unittest.TestCase):
    def test_user_qr_attachment_is_retained_byte_for_byte(self):
        path = ROOT / "assets/images/marvy-follow-qr.jpg"
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), generator.SOURCE_SHA256)
        with Image.open(path) as image:
            self.assertEqual(image.size, (860, 860))

    def test_base_is_module_aligned_and_preserves_visible_source_modules(self):
        original = generator.image_source(ROOT)
        base, mark = generator.make_images(original)
        self.assertEqual(base.mode, "1")
        self.assertEqual(base.size, (180, 180))
        self.assertEqual(mark.mode, "RGB")
        self.assertEqual(mark.size, (44, 44))
        for row in range(37):
            for col in range(37):
                expected = max(original.getpixel((70 + col * 20, 70 + row * 20))) >= 128
                for dy in range(4):
                    for dx in range(4):
                        self.assertEqual(bool(base.getpixel((16 + col * 4 + dx,
                                                            16 + row * 4 + dy))), expected)

    def test_four_module_white_quiet_zone_survives_color_mark_compositing(self):
        preview = generator.preview_image(ROOT)
        self.assertEqual(preview.size, (180, 180))
        for y in range(180):
            for x in range(180):
                if x < 16 or x >= 164 or y < 16 or y >= 164:
                    self.assertEqual(preview.getpixel((x, y)), (255, 255, 255))
        # The color logo is retained rather than replaced by a monochrome mark.
        red = blue = False
        rgb = preview.crop((68, 68, 112, 112)).tobytes()
        for offset in range(0, len(rgb), 3):
            r, g, b = rgb[offset:offset + 3]
            red |= r > 180 and g < 100 and b < 100
            blue |= b > 150 and r < 100
        self.assertTrue(red and blue)

    def test_base_has_native_rgb565_rows_and_exact_black_white_boundaries(self):
        base, _ = generator.make_images(generator.image_source(ROOT))
        pixels = generator.encode_rgb565(base)
        self.assertEqual(len(pixels), 180 * 180 * 2)
        self.assertEqual(pixels[:180 * 16 * 2], bytes([255] * (180 * 16 * 2)))
        # First finder starts at (16,16), and its first black run is 7 modules.
        first_finder = 16 * 360 + 16 * 2
        self.assertEqual(pixels[first_finder:first_finder + 28 * 2], bytes(28 * 2))
        self.assertEqual(pixels[first_finder + 28 * 2:first_finder + 32 * 2], bytes([255] * 8))

    def test_rgb565_is_little_endian_and_opaque(self):
        image = Image.new("RGB", (4, 1))
        image.putdata(((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)))
        self.assertEqual(generator.encode_rgb565(image),
                         bytes((0, 0xf8, 0xe0, 7, 0x1f, 0, 0xff, 0xff)))

    def test_bad_source_geometry_is_rejected(self):
        for dimensions in ((180, 180), (860, 859)):
            with self.assertRaises(ValueError):
                generator.make_images(Image.new("RGB", dimensions))

    def test_generated_descriptors_match_the_retained_source(self):
        header, source = generator.build_assets(ROOT)
        self.assertEqual((ROOT / "main/marvy_qr.h").read_text(), header)
        self.assertEqual((ROOT / "main/marvy_qr.c").read_text(), source)
        self.assertIn("LV_COLOR_FORMAT_RGB565", source)
        self.assertNotIn("LV_COLOR_FORMAT_I1", source)
        self.assertIn(".stride = 360", source)
        self.assertIn(".stride = 88", source)
        base, mark = generator.make_images(generator.image_source(ROOT))
        self.assertEqual(len(generator.encode_rgb565(base)), 64800)
        self.assertEqual(len(generator.encode_rgb565(mark)), 3872)


if __name__ == "__main__":
    unittest.main()
