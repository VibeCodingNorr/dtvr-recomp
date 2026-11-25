# SPDX-License-Identifier: GPL-3.0-only
"""Local GUI path history; remembered input never bypasses installer validation."""

import json
import os
from pathlib import Path
import tempfile

from installer_checks import default_output
from installation_manifest import RUNTIMES


def state_path():
    base = Path(os.environ.get("XDG_STATE_HOME", ""))
    if not base.is_absolute():
        base = Path.home() / ".local/state"
    return base / "dtvr-recomp/installer.json"


def read_object(path, limit=65536):
    try:
        if path.stat().st_size > limit:
            return {}
        value = json.loads(path.read_text())
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError, UnicodeError):
        return {}


def recognized_installation(path):
    manifest = read_object(path / "manifest.json", 8 * 1024 * 1024)
    kind = manifest.get("format")
    return manifest.get("schema_version") == 2 and isinstance(kind, str) and kind in RUNTIMES


def load_paths():
    saved = read_object(state_path())
    result = {}
    if saved.get("schema_version") == 1:
        for key in ("source", "output", "soundfont"):
            value = saved.get(key)
            if isinstance(value, str) and len(value) <= 4096 and "\0" not in value and Path(value).is_absolute():
                result[key] = value
        result["source_kind"] = "directory" if saved.get("source_kind") == "directory" else "iso"
        result["music"] = saved.get("music") is not False
        result["dry"] = saved.get("dry") is True
    # Discover only documented default destinations, never scan for user media.
    default = default_output()
    candidates = [Path(result["output"])] if "output" in result else []
    candidates += [default, default.with_name("dtvr-recomb")]
    existing = next((path for path in candidates if recognized_installation(path)), None)
    target = existing or Path(result.get("output", str(default)))
    # A source selected for a different installation must not silently follow it.
    if "output" in result and str(target) != result["output"]:
        result.pop("source", None)
    result["output"] = str(target)
    result["update"] = existing is not None
    return result


def save_paths(source, output, source_kind, soundfont="", music=True, dry=False):
    value = {"schema_version": 1, "source": str(Path(source).expanduser().resolve()),
             "output": str(Path(output).expanduser().resolve()), "source_kind": source_kind,
             "soundfont": str(Path(soundfont).expanduser().resolve()) if soundfont else "",
             "music": music, "dry": dry}
    target = state_path()
    target.parent.mkdir(parents=True, exist_ok=True)
    # mkstemp creates private (0600) files; replace atomically to survive interruption.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=target.parent,
                                         prefix=".installer-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        temporary.replace(target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
