# Versions and setup AppImage

The project starts at **0.0.1**. `VERSION` is the single source for CMake,
`dtvr-recomb --version`, the installer window, `tools/install.py --version`,
installation manifests and setup filenames. Increase it for every release;
ordinary development commits do not increase it. Use MAJOR.MINOR.PATCH and tags
such as `v0.0.1`. Fix releases normally increase PATCH, compatible features MINOR,
and incompatible changes MAJOR. A release version must exceed every earlier one.

The setup filename is `DTVR-Recomb-Setup-0.0.1-x86_64.AppImage` for the initial
x86_64 version. Packaging is implemented; no public release is published yet.
The AppImage contains our committed source, branding, GPL license and a supplied
relocatable Python/Tk runtime with its third-party license files. No ISO, original
assets, rendered Ogg music or SoundFont is bundled. Installer sources come from
Git HEAD, excluding ignored media, build directories and uncommitted changes.

Users launch the setup, select their own ISO and a new installation destination,
and click **Installieren und kompilieren**. Compilation and verification happen
automatically. Python/Tk is included; CMake, a C++17 compiler, pkg-config and SDL2
development files must be available on the host. Audio conversion also needs
FFmpeg/ffprobe; SoundFont rendering needs FluidSynth. Preflight identifies missing
dependencies. The package does not install system packages or require root.
The resulting game still needs the host SDL2 runtime.

```sh
chmod +x DTVR-Recomb-Setup-0.0.1-x86_64.AppImage
./DTVR-Recomb-Setup-0.0.1-x86_64.AppImage
./DTVR-Recomb-Setup-0.0.1-x86_64.AppImage --version
./DTVR-Recomb-Setup-0.0.1-x86_64.AppImage --cli --help
```

On hosts without usable FUSE, the AppImage runtime's `--appimage-extract-and-run`
option can launch it via extraction. A graphical desktop is required for the GUI;
`--cli` supports headless use.

## Building a package

Commit the intended source first. On the target architecture, supply a complete
relocatable Python distribution with working Tk, the official
[appimagetool](https://github.com/AppImage/appimagetool) and its matching
[type-2 runtime](https://github.com/AppImage/type2-runtime). Verify downloaded
checksums against their release metadata. Extract appimagetool to use its AppRun
without requiring FUSE. The builder never downloads dependencies implicitly.
Preserve all runtime third-party licenses and check redistribution terms.

```sh
python3 tools/build_appimage.py \
  --python-runtime /path/to/portable-python \
  --appimagetool /path/to/extracted-appimagetool/AppRun \
  --runtime /path/to/runtime-x86_64 \
  --output /path/to/new/package-directory
```

The builder validates the source policy, checks Python/Tk relocation, rejects
external runtime symlinks, and produces the versioned AppImage and SHA-256 file
outside the checkout. Embedded `build-info.json` records the commit, project
version, architecture and supplied tool/runtime hashes. Build on the intended
architecture; packaging is not cross compilation. Other distributions and older
glibc versions need validation before claiming support.

## Release checklist

1. Update `VERSION` and the documented current version/filename; commit the source.
2. Validate the intended tag and the highest previous release version:
   `python3 tools/project_version.py --release-tag v0.0.1 --previous 0.0.0`.
3. Run source policy, Python tests, CMake/CTest and real-ISO installation checks.
4. Build the AppImage, verify its checksum/version, open its GUI and exercise an
   installation. Test on supported target distributions before publication.
5. Publish the matching tag and release with the AppImage and checksum when
   release publication is explicitly requested.

The CI validates `VERSION` and checks new version tags against existing tags.
Automatic updates, save migration and release publication automation remain future work.
