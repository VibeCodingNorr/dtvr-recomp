# SPDX-License-Identifier: GPL-3.0-only
"""Installer edition profiles: table, language check, incomplete folders, no cross-edition updates."""

import hashlib
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
import editions
from editions import ENGLISH_RETAIL, GERMAN_RETAIL
from installation_manifest import verify_installation, write_manifest
from installer_directory import inspect_directory
from project_version import read_version

EXE = b"synthetic retail executable"
EXE_HASH = hashlib.sha256(EXE).hexdigest()


def runtime_profiles():
    """(id, hash, letter, required files, overview) of every profile in src/edition.cpp."""
    source = (REPO / "src/edition.cpp").read_text()
    pattern = re.compile(r'\{Edition::\w+, "([a-z-]+)", .*?"([0-9a-f]{64})", \'([A-Z])\',\s*'
                         r'\{[0-9, ]+\},.*?\{([^}]*)\},\s*"([^"]+)"', re.S)
    return [(m[1], m[2], m[3].lower(), set(re.findall(r'"([^"]+)"', m[4])), m[5])
            for m in pattern.finditer(source)]


class ProfileTableTests(unittest.TestCase):
    def test_mirrors_the_runtime_profiles(self):
        runtime = runtime_profiles()
        self.assertEqual([p[0] for p in runtime], [p["id"] for p in editions.PROFILES])
        for (edition, sha, letter, required, overview), profile in zip(runtime, editions.PROFILES):
            with self.subTest(edition=edition):
                self.assertEqual((sha, letter, overview),
                                 (profile["executable_sha256"], profile["language"], profile["overview"]))
                # The installer checks at least what the runtime refuses to start without.
                self.assertLessEqual(required, set(profile["required"]))
                self.assertIn(f"dat/{letter}/sync.dat", profile["required"])

    def test_lookup_and_unsupported_builds(self):
        self.assertIs(editions.profile_for_executable(GERMAN_RETAIL["executable_sha256"].upper()), GERMAN_RETAIL)
        self.assertIs(editions.profile_for_executable(ENGLISH_RETAIL["executable_sha256"]), ENGLISH_RETAIL)
        # The official English demo (27-MAY-95) is no profile; it is refused as a demo.
        demo = "c8b7dab6574047c6e3a3d4e600dd3f2ed23e90bd8a10de8a1bb891688d0624d9"
        for sha in (demo, None, ""):
            self.assertIsNone(editions.profile_for_executable(sha))
        with self.assertRaisesRegex(ValueError, "^Demoversion von Dr. Drago's Madcap Chase .* nicht unterstützt"):
            editions.require_profile(demo.upper(), "directory")
        for sha in (None, "", GERMAN_RETAIL["executable_sha256"], "f" * 64):
            self.assertIsNone(editions.demo_message(sha))
        with self.assertRaisesRegex(ValueError, "Unsupported original executable; this directory"):
            editions.require_profile("f" * 64, "directory")
        self.assertIn("26260860553361b356a8884b290c659ab6cedf2e29645fbcf2f8f0c511551fc7",
                      ENGLISH_RETAIL["known_isos"])

    def test_language_letter(self):
        self.assertEqual(editions.language_from_bytes(b"E\r\n"), "e")
        for data in (b"", b"\r\n", b"1"):
            with self.assertRaisesRegex(ValueError, "language letter"):
                editions.language_from_bytes(data)
        editions.require_language(ENGLISH_RETAIL, "e")
        with self.assertRaisesRegex(ValueError, "names language D"):
            editions.require_language(ENGLISH_RETAIL, "d")

    def test_installed_edition_and_cross_edition_updates(self):
        english = {"executable_sha256": ENGLISH_RETAIL["executable_sha256"], "edition": "en-us-retail"}
        self.assertIs(editions.installed_profile(english), ENGLISH_RETAIL)
        # Installations of earlier installers: German hash, no edition field.
        self.assertIs(editions.installed_profile({"executable_sha256": GERMAN_RETAIL["executable_sha256"]}),
                      GERMAN_RETAIL)
        self.assertIs(editions.installed_profile({}), GERMAN_RETAIL)
        for manifest, message in [({"edition": "fr-retail"}, "unknown edition"),
                                  ({**english, "edition": "de-retail"}, "disagree"),
                                  ({"executable_sha256": "0" * 64}, "unsupported")]:
            with self.assertRaisesRegex(ValueError, message):
                editions.installed_profile(manifest)
        editions.require_same_edition(english, ENGLISH_RETAIL)
        with self.assertRaisesRegex(ValueError, "new directory instead of --update"):
            editions.require_same_edition(english, GERMAN_RETAIL)
        with self.assertRaisesRegex(ValueError, "new directory instead of --update"):
            editions.require_same_edition({}, ENGLISH_RETAIL)


class EnglishDirectoryTests(unittest.TestCase):
    """The English profile's real file list with a synthetic executable."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.program = Path(temporary.name) / "PROGRAM"
        manager = patch("editions.PROFILES", ({**ENGLISH_RETAIL, "executable_sha256": EXE_HASH},))
        manager.start()
        self.addCleanup(manager.stop)
        for name in ENGLISH_RETAIL["required"]:
            path = self.program / name.upper()
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"E\r\n" if name == "dat/lan.inf" else b"synthetic")
        (self.program / "RALLY.EXE").write_bytes(EXE)

    def test_complete_folder_is_english(self):
        self.assertEqual(inspect_directory(self.program)["edition"], "en-us-retail")

    def test_installed_hard_disk_folder_without_butt_is_rejected(self):
        # Like the installed HDD/BLUEBYTE/DRAGO folder: EXE and data, but no butt/.
        for path in sorted((self.program / "BUTT").rglob("*"), reverse=True):
            path.rmdir() if path.is_dir() else path.unlink()
        with self.assertRaisesRegex(ValueError, "program/butt/1/map.bmp.*--iso"):
            inspect_directory(self.program)

    def test_wrong_language_letter_is_rejected(self):
        (self.program / "DAT/LAN.INF").write_bytes(b"D\r\n")
        with self.assertRaisesRegex(ValueError, "names language D"):
            inspect_directory(self.program)


class ManifestTests(unittest.TestCase):
    def test_english_installation_needs_its_overview(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("play", "bin/dtvr-recomp", "data/program/butt/1/map.bmp"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("x")
                path.chmod(0o755)
            metadata = {"project_version": read_version(), "edition": "en-us-retail",
                        "executable_sha256": ENGLISH_RETAIL["executable_sha256"]}
            write_manifest(root, metadata)
            with self.assertRaisesRegex(ValueError, "missing required runtime entries"):
                verify_installation(root)
            (root / "data/program/butt/6").mkdir()
            (root / "data/program/butt/6/map.bmp").write_text("x")
            write_manifest(root, metadata)
            self.assertEqual(verify_installation(root)["edition"], "en-us-retail")


if __name__ == "__main__":
    unittest.main()
