#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inspect user-supplied MIDI timing and optionally compare local Ogg segments.

Research only: scores are similarity measures, not original track IDs or proof of
an original hardware model. No note sequences or original audio are exported.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess


def read_midi(data):
    def integer(fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(data):
            raise ValueError("Truncated MIDI integer")
        return struct.unpack_from(fmt, data, at)

    if data[:4] != b"MThd":
        raise ValueError("Missing MIDI header")
    length, fmt, count, division = integer(">IHHH", 4)
    if length != 6 or fmt not in (0, 1) or not 0 < count <= 256 or not 0 < division < 0x8000:
        raise ValueError("Only standard format 0/1 PPQN MIDI is supported")
    at = 14
    events, ends = [], []
    for _ in range(count):
        if data[at:at + 4] != b"MTrk":
            raise ValueError("Missing MIDI track")
        size = integer(">I", at + 4)[0]
        at += 8
        end = at + size
        if end > len(data):
            raise ValueError("Truncated MIDI track")
        tick, running = 0, None

        def take(n):
            nonlocal at
            if n < 0 or at + n > end:
                raise ValueError("Truncated MIDI event")
            result = data[at:at + n]
            at += n
            return result

        def vlq():
            value = 0
            for _ in range(4):
                byte = take(1)[0]
                value = (value << 7) | (byte & 127)
                if byte < 128:
                    return value
            raise ValueError("Invalid MIDI variable-length integer")

        while at < end:
            tick += vlq()
            if at >= end:
                raise ValueError("Missing MIDI event status")
            status = data[at]
            if status >= 128:
                at += 1
            elif running is not None:
                status = running
            else:
                raise ValueError("MIDI running status without channel status")
            if status == 255:
                kind = take(1)[0]
                payload = take(vlq())
                if kind == 81:
                    if len(payload) != 3 or not int.from_bytes(payload, "big"):
                        raise ValueError("Invalid MIDI tempo")
                    events.append((tick, "tempo", int.from_bytes(payload, "big")))
            elif status in (240, 247):
                take(vlq())
                running = None
            elif 128 <= status < 240:
                running = status
                kind, channel = status >> 4, status & 15
                payload = take(1 if kind in (12, 13) else 2)
                if any(byte >= 128 for byte in payload):
                    raise ValueError("Invalid MIDI channel data")
                if kind == 9 and payload[1]:
                    events.append((tick, "note", channel, payload[0], payload[1]))
                elif kind == 8 or (kind == 9 and payload[1] == 0):
                    events.append((tick, "off", channel, payload[0]))
            else:
                raise ValueError("Unsupported MIDI status")
            if len(events) > 500000:
                raise ValueError("MIDI event limit exceeded")
        ends.append(tick)
    if at != len(data):
        raise ValueError("Unexpected bytes after MIDI tracks")
    tempo, last, seconds = 500000, 0, 0.0
    timed = []
    for event in sorted(events, key=lambda e: (e[0], 0 if e[1] == "tempo" else 1)):
        seconds += (event[0] - last) * tempo / (division * 1000000)
        last = event[0]
        if event[1] == "tempo":
            tempo = event[2]
        else:
            timed.append((seconds, *event[1:]))
    duration = seconds + (max(ends) - last) * tempo / (division * 1000000)
    return {"duration_seconds": duration, "ppqn": division, "tracks": count,
            "events": timed, "sha256": hashlib.sha256(data).hexdigest()}


def compare_audio(midis, directory):
    import numpy as np  # Optional research dependency, not a native runtime dependency.
    if not shutil.which("ffmpeg"):
        raise ValueError("Audio comparison requires FFmpeg")
    rate, size, hop, step = 11025, 4096, 2756, 0.25
    frequencies = np.fft.rfftfreq(size, 1 / rate)
    valid = (frequencies > 65) & (frequencies < 4500)
    notes = np.zeros(len(frequencies))
    notes[valid] = 69 + 12 * np.log2(frequencies[valid] / 440)
    nearest = np.rint(notes).astype(int)
    valid &= abs(notes - nearest) < 0.35
    weights = np.zeros((len(frequencies), 12))
    weights[np.where(valid)[0], nearest[valid] % 12] = 1

    def normalize(matrix):
        matrix = np.sqrt(matrix + 1e-9)
        matrix -= matrix.mean(axis=1, keepdims=True)
        return matrix / (np.linalg.norm(matrix, axis=1, keepdims=True) + 1e-9)

    midi_chroma = []
    for name, midi in midis:
        if not 0 <= midi["duration_seconds"] <= 3600:
            raise ValueError("Comparison supports MIDI durations up to one hour")
        matrix = np.zeros((int(midi["duration_seconds"] / step) + 2, 12))
        active, last = {}, 0

        def fill(time):
            nonlocal last
            end = int(time / step)
            for (channel, note), velocity in active.items():
                if channel == 9:
                    continue
                for harmonic in range(1, 6):
                    pitch = round(note + 12 * np.log2(harmonic)) % 12
                    matrix[last:end, pitch] += velocity / (harmonic * harmonic)
            last = end

        for time, kind, channel, note, *velocity in midi["events"]:
            fill(time)
            if kind == "note":
                active[channel, note] = velocity[0]
            else:
                active.pop((channel, note), None)
        fill(midi["duration_seconds"])
        midi_chroma.append((name, normalize(matrix)))
    result = []
    paths = sorted(directory.glob("segment-*.ogg"))
    if not paths or len(paths) > 99:
        raise ValueError("Expected 1–99 recovered Ogg segments")
    for path in paths:
        decoded = subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-i", str(path),
                                  "-t", "600", "-ac", "1", "-ar", str(rate), "-f", "f32le", "-"],
                                 check=True, capture_output=True).stdout
        audio = np.frombuffer(decoded, dtype="<f4")
        if len(audio) < size:
            raise ValueError("Audio segment is too short")
        frames = np.lib.stride_tricks.sliding_window_view(audio, size)[::hop]
        spectrum = abs(np.fft.rfft(frames * np.hanning(size), axis=1))
        chroma = normalize(spectrum @ weights)
        times = (np.arange(len(chroma)) * hop + size / 2) / rate
        scores = []
        for name, matrix in midi_chroma:
            best = {"midi": name, "similarity": -1.0}
            for scale in np.arange(0.65, 1.36, 0.01):
                for offset in np.arange(-3, 5.01, 0.25):
                    index = np.rint((times - offset) / scale / step).astype(int)
                    keep = (index >= 0) & (index < len(matrix))
                    if keep.mean() < 0.8:
                        continue
                    score = float(np.sum(chroma[keep] * matrix[index[keep]], axis=1).mean())
                    if score > best["similarity"]:
                        best = {"midi": name, "similarity": score,
                                "time_scale": round(float(scale), 3), "offset_seconds": float(offset)}
            scores.append(best)
        scores.sort(key=lambda e: e["similarity"], reverse=True)
        result.append({"audio": path.name, "best_candidates": scores[:5]})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi-dir", type=Path, required=True)
    parser.add_argument("--audio-dir", type=Path, help="Compare Ogg segments; needs NumPy and FFmpeg")
    args = parser.parse_args()
    try:
        paths = sorted(p for p in args.midi_dir.iterdir() if p.is_file())
        if not paths or len(paths) > 100:
            raise ValueError("Expected 1–100 original MIDI files")
        midis = []
        for path in paths:
            if path.stat().st_size > 8 * 1024 * 1024:
                raise ValueError("MIDI size limit exceeded")
            midis.append((path.name, read_midi(path.read_bytes())))
        report = {"method_version": 1,
                  "midi": [{"file": name, **{k: v for k, v in midi.items() if k != "events"}}
                           for name, midi in midis]}
        if args.audio_dir:
            report["audio_comparison_limit_seconds"] = 600
            report["matches"] = compare_audio(midis, args.audio_dir)
    except (OSError, ValueError, ImportError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Music inspection failed: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
