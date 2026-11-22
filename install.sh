#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Pick an installed Python with a working Tk runtime; no system packages are changed.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for interpreter in python3 python3.14 python3.13 python3.12 python3.11 python3.10; do
    if command -v "$interpreter" >/dev/null 2>&1 &&
       "$interpreter" -c 'import tkinter' >/dev/null 2>&1; then
        exec "$interpreter" "$source_dir/tools/installer_gui.py" "$@"
    fi
done
printf '%s\n' 'Der grafische Installer benötigt Python mit Tk-Unterstützung (python-tk / python3-tk).' >&2
printf '%s\n' 'CLI-Alternative: python3 tools/install.py --help' >&2
exit 1
