# Installer failure and cancellation tests

Run the reproducible suite without an original ISO or compiler toolchain:

```sh
python3 -m unittest discover -s tests -p 'test_installer*.py' -v
python3 -m unittest discover -s tests -p 'test_appimage*.py' -v
```

All media, build stubs, user files and output directories are generated in
temporary directories outside the checkout. The pipeline tests use a real
synthetic ISO9660 tree, actual importer/staging/manifest/publication code and real
subprocesses. Original-executable analysis and host-dependency preflight are
replaced for these pipeline tests; dedicated preflight tests cover them separately.
Controlled CMake/CTest/runtime stubs inject failures without compiling the engine.
These tests do not replace a real-ISO game build or cross-distribution testing.

| Scenario | Coverage / expected outcome |
| --- | --- |
| Not enough space before import | Simulated free-space report; no destination created |
| Disk full during import | Injected ENOSPC after extraction; staging removed, old installation unchanged |
| Missing tools, SDL2/TTF, Ogg libraries or fonts | Preflight rejection before writes |
| Invalid/truncated media | Real CLI and ISO reader reject it; no traceback or changed input |
| Spaces, Unicode and shell metacharacters | Real subprocess calls and installed launcher preserve literal paths |
| No write permission | Simulated access denial plus actual directory permissions when not root |
| Cancel during extraction | Real process-group SIGINT; staging removed, input/old files unchanged |
| Cancel while compiling | Real SIGINT reaches compiler and its child; neither keeps running |
| Compilation, install or runtime failure | Old installation and user files unchanged; new target absent |
| Final rename fails | Old installation restored; temporary output cleaned |
| Publication and restoration both fail | Old installation retained in backup; error gives its recovery path |
| Damaged soundtrack metadata | Rejected before a build, including duplicate/unsafe identifiers |
| Source ISO is inside update target | Rejected so publishing cannot move the original medium into a backup |
| Parallel HEAD changes during packaging | Export and embedded commit metadata use one pinned revision |

Successful pipeline checks also verify executable permissions, complete managed-file
hashes, the default game launcher and isolated smoke-test XDG directories. Update
tests preserve saves and local notes byte-for-byte and check external settings.
Existing tests cover retained Ogg packs, changed managed files and competing publishers.

Permission and disk-full injection are intentional: the suite never fills a real
disk, changes system permissions or requires root. Process tests have bounded
timeouts. SIGKILL and power failure do not offer cooperative cleanup guarantees;
recovery from a power loss between the publication renames remains manual.

## Graphical installer checks

Optional GUI checks exercise the custom folder chooser, system-dialog fallback,
dependency-command copying, update/version display and redacted export. Run with
a Python interpreter that has working Tk and a graphical display, or through Xvfb:

```sh
xvfb-run -a env DTVR_TEST_GUI=1 python3 -m unittest discover -s tests -p 'test_installer_ui.py' -v
```

These checks are skipped by default to avoid opening windows during ordinary
unit-test runs. Pure dependency, privacy and folder-listing tests run normally.
The design checks additionally exercise expanded music controls, disabled input
during installation, cancellation/return and the minimum window size. Check the
packaged runtime as well as source execution when changing fonts or Tcl/Tk paths.

## Checking an actual AppImage without FUSE mounting

Supply a previously built setup artifact and its adjacent SHA-256 sidecar:

```sh
python3 tools/check_appimage.py /path/to/DTVR-Recomb-Setup-0.0.1-x86_64.AppImage
```

The checker validates the checksum, runs the bundled version and CLI through
`--appimage-extract-and-run`, then checks invalid-media rejection. Each run uses
temporary XDG directories and deliberately invalid inherited Python configuration
to check that AppRun selects its bundled Python. No original game media, graphical
desktop, privileged mount or FUSE mount is needed for this check. The tool reports
JSON and returns a nonzero exit status on failure.

This verifies the extraction fallback even on a host that has FUSE. It does not
simulate every distribution or test graphical drivers. Use a graphical session
or Xvfb separately for GUI checks. The checker uses the current checkout's VERSION
as its expected project version.
