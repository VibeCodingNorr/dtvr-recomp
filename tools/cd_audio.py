# SPDX-License-Identifier: GPL-3.0-only
"""Convert recovered CD-DA regions to Ogg Vorbis; never guess original track IDs."""

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


# Observed non-zero regions after the Mode 1 track in this exact image.
# Silence separators are NOT a recovered disc TOC. These are provisional segments.
KNOWN_ISO = "a94cb115097c22b539563dac1954e965a5ee5f812070d70beeb39906a4197fee"
PROFILE_VERSION = 1
KNOWN_REGIONS = [(38156, 51570), (51721, 66087), (66238, 80980),
                 (81131, 92876), (93027, 106147), (106298, 114479)]


def audio_layout(iso_hash, image_size):
    if iso_hash != KNOWN_ISO:
        return None
    if image_size != 114479 * 2352:
        raise ValueError("Known CD audio profile has an inconsistent image size")
    return {"profile_version": PROFILE_VERSION,
            "boundary_source": "observed-nonzero-regions-not-original-toc",
            "original_track_numbers_known": False,
            "pcm_format": "s16be", "sample_rate": 44100, "channels": 2,
            "sector_bytes": 2352,
            "regions": [{"name": f"segment-{i:02}", "start_sector": start,
                         "end_sector_exclusive": end}
                        for i, (start, end) in enumerate(KNOWN_REGIONS, 1)]}


def validate_layout(layout, image_size):
    if layout["pcm_format"] not in ("s16be", "s16le"):
        raise ValueError("Unsupported CD PCM format")
    if (layout["sample_rate"], layout["channels"], layout["sector_bytes"]) != (44100, 2, 2352):
        raise ValueError("Unsupported CD audio sample layout")
    if image_size % 2352:
        raise ValueError("Truncated CD audio image")
    previous = 0
    names = set()
    if not layout["regions"] or len(layout["regions"]) > 99:
        raise ValueError("Invalid audio region count")
    for region in layout["regions"]:
        name = region["name"]
        if not isinstance(name, str) or not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in name):
            raise ValueError("Unsafe audio segment name")
        if name in names:
            raise ValueError("Duplicate audio segment name")
        names.add(name)
        start, end = region["start_sector"], region["end_sector_exclusive"]
        if type(start) is not int or type(end) is not int or not previous <= start < end <= image_size // 2352:
            raise ValueError("Overlapping or out-of-bounds CD audio region")
        previous = end


def convert_audio(iso, target, layout):
    """Write a new audio directory and return provenance for the installer manifest."""
    validate_layout(layout, iso.stat().st_size)
    if target.exists() or target.is_symlink():
        raise ValueError(f"Audio destination already exists: {target}")
    for tool in ("ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            raise ValueError(f"CD audio conversion requires {tool} (FFmpeg with libvorbis)")
    version = subprocess.run(["ffmpeg", "-version"], check=True, capture_output=True,
                             text=True).stdout.splitlines()[0]
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-audio-", dir=target.parent) as temporary:
        stage = Path(temporary) / "audio"
        stage.mkdir()
        converted = []
        with iso.open("rb") as source:
            for region in layout["regions"]:
                start, end = region["start_sector"], region["end_sector_exclusive"]
                source.seek(start * 2352)
                remaining = (end - start) * 2352
                pcm = Path(temporary) / "source.pcm"
                h = hashlib.sha256()
                with pcm.open("wb") as stream:
                    while remaining:
                        data = source.read(min(remaining, 1024 * 1024))
                        if not data:
                            raise ValueError("Truncated CD audio region")
                        stream.write(data)
                        h.update(data)
                        remaining -= len(data)
                output = stage / (region["name"] + ".ogg")
                print(f"Converting CD audio: {output.name}", flush=True)
                subprocess.run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-n",
                                "-f", layout["pcm_format"], "-ar", "44100", "-ac", "2", "-i", str(pcm),
                                "-map", "0:a:0", "-c:a", "libvorbis", "-q:a", "6",
                                "-metadata", f"title=Recovered CD audio {region['name']}",
                                str(output)], check=True)
                probe = json.loads(subprocess.run(
                    ["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(output)],
                    check=True, capture_output=True, text=True).stdout)
                streams = probe["streams"]
                duration = (end - start) / 75
                if (len(streams) != 1 or streams[0]["codec_name"] != "vorbis"
                        or streams[0]["channels"] != 2 or streams[0]["sample_rate"] != "44100"
                        or abs(float(streams[0]["duration"]) - duration) > 0.05):
                    raise ValueError(f"Converted audio verification failed: {output.name}")
                converted.append({**region, "file": output.name, "source_pcm_sha256": h.hexdigest(),
                                  "duration_seconds": duration})
        result = {**layout, "encoder": version, "codec": "vorbis", "quality": 6,
                  "regions": converted, "playback_mapping": "unverified"}
        (stage / "audio.json").write_text(json.dumps(result, indent=2) + "\n")
        if target.exists() or target.is_symlink():
            raise ValueError(f"Audio destination appeared during conversion: {target}")
        stage.rename(target)
    return result
