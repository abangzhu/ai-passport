#!/usr/bin/env python3
"""Rebuild Marvy's licensed game PCM from checksum-pinned Freesound sources."""
import argparse
import array
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

BASE = Path(__file__).resolve().parent
TARGET_PEAK = 27852  # 85% of signed 16-bit full scale; mixer reserves more headroom.
SOURCES = [
    ("bgm", "fs_749008_adventure.mp3", "marvy_bgm.pcm"),
    ("ordinary_hit", "fs_162805_8-bit-coin1-1-wav.mp3", "marvy_hit.pcm"),
    ("marvy_hit", "fs_431329_8-Bit-Powerup.mp3", "marvy_bonus.pcm"),
    ("level_clear", "fs_609336_Completed-wav.mp3", "marvy_clear.pcm"),
    ("level_fail", "fs_450616_8-Bit-Error.mp3", "marvy_fail.pcm"),
    ("combo_3", "fs_361264_8-bit-Happy-Ding.mp3", "marvy_combo3.pcm"),
    ("combo_5", "fs_609335_LevelUp-wav.mp3", "marvy_combo5.pcm"),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffmpeg", help="Explicit ffmpeg executable; otherwise resolve it on PATH")
    args = parser.parse_args()
    ffmpeg = args.ffmpeg or shutil.which("ffmpeg")
    if not ffmpeg:
        raise SystemExit("ffmpeg is required; install it or pass --ffmpeg /path/to/ffmpeg")
    version = subprocess.run([ffmpeg, "-version"], check=True, capture_output=True,
                             text=True).stdout.splitlines()[0]
    manifest_path = BASE / "marvy_audio.source.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["conversion_tool"] = version
    for role, source_name, target_name in SOURCES:
        record = manifest["assets"][role]
        source = BASE / source_name
        if hashlib.sha256(source.read_bytes()).hexdigest() != record["source_sha256"]:
            raise SystemExit(f"Source checksum mismatch: {source_name}")
        with tempfile.TemporaryDirectory(prefix="marvy-pcm-") as directory:
            raw = Path(directory) / "decoded.pcm"
            subprocess.run([
                ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(source),
                "-ac", "1", "-ar", "16000", "-c:a", "pcm_s16le", "-f", "s16le", str(raw),
            ], check=True)
            pcm = array.array("h")
            pcm.frombytes(raw.read_bytes())
        if sys.byteorder != "little":
            pcm.byteswap()
        if not pcm:
            raise SystemExit(f"Empty PCM: {source_name}")
        record["decoded_duration_before_edit"] = len(pcm) / 16000
        processing = record.get("processing", {})
        trim_end = processing.get("trim_end_seconds")
        if trim_end is not None:
            keep_samples = round(float(trim_end) * 16000)
            if not 0 < keep_samples <= len(pcm):
                raise SystemExit(f"Invalid trim end: {source_name}")
            pcm = pcm[:keep_samples]
        fade_samples = round(float(processing.get("fade_out_ms", 0)) * 16)
        if fade_samples:
            if not 0 < fade_samples <= len(pcm):
                raise SystemExit(f"Invalid fade duration: {source_name}")
            for offset in range(fade_samples):
                at = len(pcm) - fade_samples + offset
                pcm[at] = round(pcm[at] * (fade_samples - offset - 1) / fade_samples)
        peak = max(abs(value) for value in pcm)
        if peak == 0:
            raise SystemExit(f"Silent PCM: {source_name}")
        normalized = array.array("h", (round(value * TARGET_PEAK / peak) for value in pcm))
        record["decoded_peak_before_normalization"] = peak
        record["decoded_rms_before_normalization"] = (
            sum(value * value for value in pcm) / len(pcm)
        ) ** 0.5
        record["pcm"] = {
            "file": target_name, "format": "signed 16-bit little-endian mono PCM",
            "sample_rate": 16000, "samples": len(normalized),
            "duration_seconds": len(normalized) / 16000,
            "bytes": len(normalized) * 2, "peak": max(abs(v) for v in normalized),
        }
        energy = sum(value * value for value in normalized)
        cumulative = 0
        for position, value in enumerate(normalized):
            cumulative += value * value
            if cumulative >= energy * 0.95:
                record["energy_95_percent_by_seconds"] = (position + 1) / 16000
                break
        record["normalized_rms"] = round((energy / len(normalized)) ** 0.5, 3)
        record["normalized_rms_100ms_windows"] = [
            round((sum(value * value for value in window) / len(window)) ** 0.5, 1)
            for start in range(0, len(normalized), 1600)
            if (window := normalized[start:start + 1600])
        ]
        if sys.byteorder != "little":
            normalized.byteswap()
        target = BASE / target_name
        target.write_bytes(normalized.tobytes())
        record["pcm_sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
        print(f"{target_name}: {len(pcm) / 16000:.6f}s, {len(pcm) * 2} bytes, normalized peak {TARGET_PEAK}")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
