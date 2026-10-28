"""Exercise the actual desktop executable's headless path against binary plugins."""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

EXE = pathlib.Path(__file__).resolve().parents[1] / 'build-release/BowSpeedVREditor.exe'
if __name__ == '__main__' and len(sys.argv) > 1 and not sys.argv[1].startswith('-'):
    EXE = pathlib.Path(sys.argv.pop(1)).resolve()

def sub(tag, data):
    return tag.encode() + struct.pack('<H', len(data)) + data

def record(tag, fid, data, flags=0):
    return tag.encode() + struct.pack('<5I', len(data), flags, fid, 0, 44) + data

def fixture(root, unfamiliar=False):
    data = root / 'Data'
    data.mkdir()
    (root / 'SkyrimVR.exe').touch()  # target identity only; never executed
    bow = bytearray(100)
    bow[0] = 7
    struct.pack_into('<f', bow, 4, 1)
    projectile = bytearray(92)
    struct.pack_into('<H', projectile, 2, 64)
    struct.pack_into('<fff', projectile, 4, 1, 3600, 60000)
    struct.pack_into('<f', projectile, 52, 1)
    ammo = struct.pack('<IIfI', 0x3be11, 4, 8, 1)
    plugin = record('TES4', 0, b'')
    stats = struct.pack('<IfH', 0, 0, 7)
    plugin += record('WEAP', 0x3b562, sub('EDID', b'LongBow\0') + sub('DNAM', bow) + sub('DATA', stats))
    plugin += record('PROJ', 0x3be11, sub('DATA', projectile))
    plugin += record('AMMO', 0x1397d, sub('EDID', b'IronArrow\0') + sub('DATA', ammo))
    if unfamiliar:
        plugin += record('WEAP', 0x800, sub('EDID', b'UnfamiliarBow\0') + sub('DNAM', bow) + sub('DATA', stats))
        plugin += record('AMMO', 0x801, sub('EDID', b'UnfamiliarArrow\0') + sub('DATA', ammo))
    (data / 'Skyrim.esm').write_bytes(plugin)
    (root / 'plugins.txt').write_text('*Skyrim.esm\nDisabled.esp\n', encoding='utf-8')

class EditorCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert EXE.is_file(), f'Desktop executable has not been implemented: {EXE}'

    def test_active_report_and_runtime_consumable_zip(self):
        with tempfile.TemporaryDirectory(prefix='bowspeed-cli-') as tmp:
            root = pathlib.Path(tmp)
            game = root / 'game'
            game.mkdir()
            fixture(game)
            before = (game / 'Data/Skyrim.esm').read_bytes()
            report, export = root / 'scan.json', root / 'settings.zip'
            run = subprocess.run([str(EXE), '--game-dir', str(game), '--profile-dir', str(game), '--scan-report', str(report), '--export-zip', str(export)], capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr.decode(errors='replace'))
            scan = json.loads(report.read_text(encoding='utf-8'))
            self.assertTrue(scan['complete'])
            self.assertEqual(scan['pluginCount'], 1)
            self.assertEqual(len(scan['equipment']), 2)
            with zipfile.ZipFile(export) as archive:
                self.assertIsNone(archive.testzip())
                overlay = archive.read('SKSE/Plugins/BowSpeedVR.user.ini').decode()
                self.assertIn('skyrim.esm|0x03B562=', overlay)
            extracted = root / 'export.ini'
            extracted.write_text(overlay, encoding='utf-8')
            checked = subprocess.run([str(EXE), '--validate-overlay', str(extracted)], capture_output=True)
            self.assertEqual(checked.returncode, 0)
            self.assertEqual((game / 'Data/Skyrim.esm').read_bytes(), before)

    def test_missing_active_list_is_not_silently_accepted(self):
        with tempfile.TemporaryDirectory(prefix='bowspeed-cli-') as tmp:
            root = pathlib.Path(tmp)
            (root / 'SkyrimVR.exe').touch()
            run = subprocess.run([str(EXE), '--game-dir', str(root), '--profile-dir', str(root), '--scan-report', str(root / 'scan.json')], capture_output=True)
            self.assertNotEqual(run.returncode, 0)

    def test_unfamiliar_defaults_export_enabled_and_keep_inference_notes(self):
        with tempfile.TemporaryDirectory(prefix='bowspeed-inferred-') as tmp:
            root = pathlib.Path(tmp)
            game = root / 'game'
            game.mkdir()
            fixture(game, unfamiliar=True)
            args = [str(EXE), '--game-dir', str(game), '--profile-dir', str(game)]
            overlay_dir = root / 'settings'
            run = subprocess.run(args + ['--export-dir', str(overlay_dir)], capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr.decode(errors='replace'))
            overlay = overlay_dir / 'SKSE/Plugins/BowSpeedVR.user.ini'
            import configparser
            settings = configparser.ConfigParser()
            settings.read(overlay)
            self.assertAlmostEqual(settings.getfloat('CatalogBow', 'skyrim.esm|0x000800'), 1)
            self.assertAlmostEqual(float(settings.get('CatalogArrow', 'skyrim.esm|0x000801').split(',')[0]), 1)
            self.assertIn(';@inferred skyrim.esm|0x000800', overlay.read_text())
            # The real import/export path must preserve the explanation.
            run = subprocess.run(args + ['--import', str(overlay), '--export-dir', str(root / 'again')], capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr.decode(errors='replace'))
            again = (root / 'again/SKSE/Plugins/BowSpeedVR.user.ini').read_text()
            self.assertIn(';@inferred skyrim.esm|0x000800', again)

    def test_output_paths_cannot_overwrite_source_plugins(self):
        with tempfile.TemporaryDirectory(prefix='bowspeed-cli-') as tmp:
            root = pathlib.Path(tmp)
            game = root / 'game'
            game.mkdir()
            fixture(game)
            source = game / 'Data/Skyrim.esm'
            original = source.read_bytes()
            args = [str(EXE), '--game-dir', str(game), '--profile-dir', str(game)]
            report = subprocess.run(args + ['--scan-report', str(source)], capture_output=True)
            self.assertNotEqual(report.returncode, 0, 'Report path must not overwrite an ESM')
            self.assertEqual(source.read_bytes(), original)
            foreign = root / 'OtherMod.esp'
            foreign.write_bytes(b'preserve this plugin')
            exported = subprocess.run(args + ['--export-zip', str(foreign)], capture_output=True)
            self.assertNotEqual(exported.returncode, 0, 'ZIP export must require a ZIP destination')
            self.assertEqual(foreign.read_bytes(), b'preserve this plugin')

if __name__ == '__main__':
    unittest.main()
