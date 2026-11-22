# SPDX-License-Identifier: GPL-3.0-only
"""Explicit local updates, preserving user files and a rollback installation."""

from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import tempfile

from installation_manifest import verify_installation
from project_version import read_version, version_tuple


def update_info(root):
    verify_installation(root)
    manifest = json.loads((root / "manifest.json").read_text())
    previous = manifest.get("project_version")
    if previous is None or version_tuple(previous) > version_tuple(read_version()):
        raise ValueError("Update requires a recognized project version; downgrades are not supported")
    # Reject links even among user files; never copy content from outside the installation.
    for path in root.rglob("*"):
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise ValueError(f"Unsupported linked or special installation entry: {path.relative_to(root)}")
    return manifest


def preserve_files(root, stage, previous, new_packs):
    """Copy mutable extras and reusable Ogg packs immediately before publication."""
    if update_info(root) != previous:
        raise ValueError("Installation changed during build; retry the update")
    excluded = []
    packs = list(new_packs)
    replaced = {p["id"] for p in new_packs}
    old_packs = previous.get("soundtrack_packs", [])
    # Use validated file paths, never directory strings supplied by pack metadata.
    for pack in old_packs:
        if not isinstance(pack, dict) or not isinstance(pack.get("id"), str):
            raise ValueError("Invalid installed soundtrack catalog")
        pack_id = pack["id"]
        if not pack_id or Path(pack_id).name != pack_id or pack_id in (".", ".."):
            raise ValueError("Invalid installed soundtrack identifier")
        if pack_id not in replaced:
            packs.append({**pack, "directory": f"data/music/packs/{pack_id}"})
    reused = {p["id"] for p in packs} - replaced
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        name = relative.as_posix()
        if name == "manifest.json":
            continue
        user_file = name not in previous["files"] or relative.parts[0] == "saves"
        old_pack = (len(relative.parts) >= 5 and relative.parts[:3] == ("data", "music", "packs")
                    and relative.parts[3] in reused)
        if path.is_dir() or not (user_file or old_pack):
            continue
        target = stage / relative
        if target.exists():
            raise ValueError(f"Local file conflicts with the new installation: {name}")
        target.parent.mkdir(parents=True, exist_ok=True)
        before = path.stat()
        shutil.copy2(path, target)
        after = path.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError("Local files changed during update; close the game and retry")
        if user_file and not old_pack:
            excluded.append(name)
    return packs, excluded


@contextmanager
def publication_lock(output):
    lock = output.parent / f".{output.name}.install-lock"
    descriptor = os.open(lock, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError("Another installer is completing an installation at this destination") from error
        # Deliver cancellation only after the short rename/rollback sequence.
        previous_mask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
        try:
            yield
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, previous_mask)
    finally:
        os.close(descriptor)


def publish(stage, output, previous=None):
    with publication_lock(output):
        if previous is None:
            if output.exists() or output.is_symlink():
                raise ValueError(f"Target appeared during installation: {output}")
            stage.rename(output)
            return None
        if update_info(output) != previous:
            raise ValueError("Installation changed before publication; retry the update")
        backup = Path(tempfile.mkdtemp(prefix=f"{output.name}.backup-", dir=output.parent))
        saved = backup / "installation"
        try:
            output.rename(saved)
            try:
                stage.rename(output)
            except BaseException:
                saved.rename(output)
                raise
        except BaseException:
            if not saved.exists():
                backup.rmdir()
            raise
        return saved
