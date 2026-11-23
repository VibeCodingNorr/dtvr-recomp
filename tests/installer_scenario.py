# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic media and controlled external build tools for installer fault tests."""

import errno
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from unittest.mock import patch

from test_iso9660 import both16, both32, record

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))


def disc_fixture():
    files = {"program/rally.exe": b"synthetic executable",
             "program/butt/1/map.bmp": b"synthetic bitmap",
             "program/map/0/part0000.sbb": b"synthetic tiles",
             "program/sbs/d.sbs": b"synthetic sprites"}
    directories = {"", *(str(parent) for name in files for parent in Path(name).parents if str(parent) != ".")}
    blocks = {name: index + 20 for index, name in enumerate(sorted(directories))}
    contents = {name: (index + 20 + len(blocks), data) for index, (name, data) in enumerate(files.items())}
    total = 20 + len(blocks) + len(contents)
    image = bytearray(total * 2048)
    pvd = 16 * 2048
    image[pvd:pvd + 7] = b"\x01CD001\x01"
    image[pvd + 80:pvd + 88] = both32(total)
    image[pvd + 128:pvd + 132] = both16(2048)
    image[pvd + 156:pvd + 190] = record(b"\x00", blocks[""], 2048, True)
    for directory, block in blocks.items():
        entries = bytearray()
        for name, child in blocks.items():
            if name and str(Path(name).parent).replace(".", "") == directory:
                entries += record(Path(name).name.upper().encode(), child, 2048, True)
        for name, (child, data) in contents.items():
            if str(Path(name).parent) == directory:
                entries += record((Path(name).name.upper() + ";1").encode(), child, len(data))
        image[block * 2048:block * 2048 + len(entries)] = entries
    for block, data in contents.values():
        image[block * 2048:block * 2048 + len(data)] = data
    return bytes(image)


def ready(stage):
    print(f"TEST_READY:{stage}", flush=True)
    while True:
        time.sleep(.1)


def fake_build_tool():
    args = sys.argv[1:]
    mode = os.environ["DTVR_TEST_MODE"]
    if "--build" in args:
        if mode == "build-cancel":
            child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(120)"])
            print(f"TEST_CHILD:{child.pid}", flush=True)
            ready("build")
        if mode == "build-fail":
            raise SystemExit(3)
    if "--install" in args:
        if mode == "install-fail":
            raise SystemExit(4)
        stage = Path(args[args.index("--prefix") + 1])
        binary = stage / "bin/dtvr-recomp"
        binary.parent.mkdir(parents=True)
        binary.write_text("#!" + sys.executable + "\n"
                          "import json, os, sys\n"
                          "print('TEST_RUNTIME:' + json.dumps(sys.argv[1:]), flush=True)\n"
                          "assert os.environ.get('XDG_CONFIG_HOME') != os.environ.get('DTVR_TEST_CONFIG') or '--smoke-test' not in sys.argv\n"
                          "raise SystemExit(9 if os.environ.get('DTVR_TEST_MODE') == 'smoke-fail' else 0)\n")
        binary.chmod(0o755)
    raise SystemExit(0)


def run_scenario(iso, output, mode, update):
    import install
    from installer_checks import digest

    synthetic_hash = hashlib.sha256(b"synthetic executable").hexdigest()
    # Only the original-executable analysis and host tool preflight are replaced.
    # ISO reading/extraction, staging, subprocesses, verification and publication are real.
    install.KNOWN_EXE = synthetic_hash
    install.inspect_game = lambda data: {"executable": {"sha256": synthetic_hash}}
    install.preflight = lambda *args: {"iso": str(iso), "output": str(output), "iso_sha256": digest(iso)}
    extract = install.IsoImage.extract_program
    rename = Path.rename

    def extraction(image, data):
        count = extract(image, data)
        if mode == "import-space":
            raise OSError(errno.ENOSPC, "simulated disk full during extraction")
        if mode == "import-cancel":
            ready("import")
        return count

    def publication(path, destination):
        if mode == "publish-fail" and path.name == "installation" and ".dtvr-install-" in str(path.parent):
            raise PermissionError("simulated final rename failure")
        return rename(path, destination)

    with patch.object(install.IsoImage, "extract_program", extraction), patch.object(Path, "rename", publication):
        # These fault fixtures deliberately contain no MIDI data or instrument bank.
        sys.argv = ["install.py", "--iso", str(iso), "--output", str(output), "--jobs", "1", "--no-music",
                    *(["--update"] if update else [])]
        install.main()


if __name__ == "__main__":
    if Path(sys.argv[0]).name in ("cmake", "ctest"):
        fake_build_tool()
    else:
        run_scenario(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4] == "update")
