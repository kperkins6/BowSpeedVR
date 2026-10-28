import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

class EditorPackageTests(unittest.TestCase):
    def test_editor_and_runtime_are_separate_reproducible_packages(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent, prefix='editor-package-') as tmp:
            root = pathlib.Path(tmp)
            for name in ('a', 'b'):
                run = subprocess.run([sys.executable, str(ROOT / 'scripts/package-editor.py'), '--build-dir', str(ROOT / 'build-release'), '--output', str(root / name)], capture_output=True)
                self.assertEqual(run.returncode, 0, run.stderr.decode(errors='replace'))
            manifest = json.loads((root / 'a/manifest.json').read_text())
            self.assertEqual(len(manifest['archives']), 2)
            for name, metadata in manifest['archives'].items():
                content = (root / 'a' / name).read_bytes()
                self.assertEqual(content, (root / 'b' / name).read_bytes())
                self.assertEqual(hashlib.sha256(content).hexdigest(), metadata['sha256'])
                with zipfile.ZipFile(root / 'a' / name) as archive:
                    self.assertIsNone(archive.testzip())
                    self.assertFalse(any('..' in pathlib.PurePosixPath(p).parts for p in archive.namelist()))
                    self.assertNotIn('SKSE/Plugins/BowSpeedVR.user.ini', archive.namelist())
                    if name.endswith('-Editor.zip'):
                        self.assertIn('BowSpeedVREditor.exe', archive.namelist())
                        self.assertIn('presets/MGO.ini', archive.namelist())
                        self.assertNotIn('SKSE/Plugins/BowSpeedVR.dll', archive.namelist())
                    else:
                        self.assertIn('SKSE/Plugins/BowSpeedVR.dll', archive.namelist())
                        self.assertNotIn('presets/MGO.ini', archive.namelist())

if __name__ == '__main__':
    unittest.main()
