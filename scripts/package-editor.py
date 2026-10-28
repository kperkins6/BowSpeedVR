"""Package an offline desktop editor separately from its matching SKSEVR runtime."""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = re.search(r'project\(BowSpeedVR VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)

def check_pe(path):
    data = path.read_bytes()
    if data[:2] != b'MZ' or len(data) < 64:
        raise ValueError(f'Not a PE binary: {path.name}')
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    if offset + 24 > len(data) or data[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', data, offset+4)[0] != 0x8664:
        raise ValueError(f'Not a Windows x64 binary: {path.name}')
    for needle in (b'C:\\Users\\', 'C:\\Users\\'.encode('utf-16le')):
        if needle.lower() in data.lower():
            raise ValueError(f'Personal build path in {path.name}')
    return data

def write_zip(path, entries):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, content in sorted(entries.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content)

def build_archives(build_dir, output):
    build_dir, output = pathlib.Path(build_dir).resolve(), pathlib.Path(output).resolve()
    editor, runtime = build_dir / 'BowSpeedVREditor.exe', build_dir / 'BowSpeedVR.dll'
    editor_bytes, runtime_bytes = check_pe(editor), check_pe(runtime)
    version = subprocess.run([str(editor), '--version'], check=True, capture_output=True).stdout.decode().strip()
    if version != f'BowSpeedVREditor {VERSION}':
        raise ValueError('Editor version does not match source version')
    version = subprocess.run([str(editor), '--runtime-version', str(runtime)], check=True, capture_output=True).stdout.decode().strip()
    if version != VERSION:
        raise ValueError('Runtime version does not match source version')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output, prefix='editor-stage-') as staging:
        preset = pathlib.Path(staging) / 'MGO.ini'
        subprocess.run([str(editor), '--write-mgo-preset', str(preset)], check=True)
        subprocess.run([str(editor), '--validate-overlay', str(preset)], check=True, capture_output=True)
        editor_files = {
            'BowSpeedVREditor.exe': editor_bytes,
            'presets/MGO.ini': preset.read_bytes(),
            'EDITOR.md': (ROOT / 'docs/EDITOR.md').read_bytes(),
            'LICENSE': (ROOT / 'LICENSE').read_bytes(),
            'THIRD-PARTY.md': (ROOT / 'THIRD-PARTY.md').read_bytes(),
            'licenses/puff.txt': (ROOT / 'editor/vendor/puff.h').read_bytes(),
        }
        runtime_files = {
            'SKSE/Plugins/BowSpeedVR.dll': runtime_bytes,
            'SKSE/Plugins/BowSpeedVR.ini': (ROOT / 'config/BowSpeedVR-standalone.ini').read_bytes(),
            'BowSpeedVR/EDITOR.md': (ROOT / 'docs/EDITOR.md').read_bytes(),
            'BowSpeedVR/INSTALLATION.md': (ROOT / 'docs/INSTALLATION.md').read_bytes(),
            'BowSpeedVR/LICENSE': (ROOT / 'LICENSE').read_bytes(),
            'BowSpeedVR/EXCEPTIONS.md': (ROOT / 'EXCEPTIONS.md').read_bytes(),
            'BowSpeedVR/THIRD-PARTY.md': (ROOT / 'THIRD-PARTY.md').read_bytes(),
        }
        for notice in (ROOT / 'licenses').rglob('*'):
            if notice.is_file():
                runtime_files['BowSpeedVR/licenses/' + notice.relative_to(ROOT / 'licenses').as_posix()] = notice.read_bytes()
        archives = {}
        for kind, entries in (('Editor', editor_files), ('Runtime', runtime_files)):
            name = f'BowSpeedVR-{VERSION}-{kind}.zip'
            write_zip(pathlib.Path(staging) / name, entries)
            data = (pathlib.Path(staging) / name).read_bytes()
            (output / name).write_bytes(data)
            archives[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        manifest = {'version': VERSION, 'archives': archives, 'binaries': {
            'BowSpeedVREditor.exe': hashlib.sha256(editor_bytes).hexdigest(),
            'BowSpeedVR.dll': hashlib.sha256(runtime_bytes).hexdigest()}}
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        return manifest

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=pathlib.Path, default=ROOT / 'build-release')
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build_archives(args.build_dir, args.output), indent=2))

if __name__ == '__main__':
    main()
