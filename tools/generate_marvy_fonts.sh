#!/usr/bin/env bash
set -euo pipefail

# Rebuild the licensed, fixed-inventory UI fonts; no full CJK font enters Flash.
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

python3 - <<'PY'
import hashlib
from pathlib import Path

source = Path("assets/fonts/NotoSansSC-Regular.otf")
expected = "faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9"
if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
    raise SystemExit("NotoSansSC-Regular.otf does not match the documented source checksum")
PY

symbols="$(cat assets/fonts/marvy_characters.txt)"
for style in font pixel; do
    bpp=4
    if [[ "${style}" == pixel ]]; then bpp=1; fi
    for size in 16 20; do
        npx --yes lv_font_conv@1.5.3 \
            --font assets/fonts/NotoSansSC-Regular.otf \
            --range 0x20-0x7E --symbols "${symbols}" \
            --size "${size}" --bpp "${bpp}" --format lvgl --no-compress \
            --lv-font-name "marvy_${style}_${size}" --lv-include lvgl.h \
            --output "assets/fonts/marvy_${style}_${size}.c"
    done
done

python3 - <<'PY'
import runpy
from pathlib import Path

font_codepoints = runpy.run_path("tools/check_glyphs.py")["font_codepoints"]
required = set(range(0x20, 0x7f))
required.update(map(ord, Path("assets/fonts/marvy_characters.txt").read_text().strip()))
for style in ("font", "pixel"):
    for size in (16, 20):
        path = Path(f"assets/fonts/marvy_{style}_{size}.c")
        # Normalize the converter's trailing empty lines for clean Git diffs.
        text = path.read_text().rstrip("\n") + "\n"
        path.write_text(text)
        covered = font_codepoints(text, str(path))
        if covered is None or required - covered:
            raise SystemExit(f"Incomplete font coverage: {path}")
        if 0x9f98 in covered:
            raise SystemExit(f"Unexpected full-CJK coverage: {path}")
        print(f"{path}: {len(required)} requested glyphs verified; U+9F98 absent as expected")
PY

python3 tools/check_glyphs.py
