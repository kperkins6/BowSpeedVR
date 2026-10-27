"""Exercise real ZIP/XML installation results, not source-text matching."""
import configparser
import hashlib
import io
import importlib.util
import itertools
import json
import pathlib
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release',ROOT/'scripts/release.py')
release=importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

def values(text):
    out=configparser.ConfigParser();out.read_string(text);return out

class ReleaseTests(unittest.TestCase):
    def test_installer_default_and_all_independent_choices(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            stage=pathlib.Path(folder)/'mgo'
            release.build_mgo(stage)
            root=ET.parse(stage/'fomod/ModuleConfig.xml').getroot()
            groups=root.findall('./installSteps/installStep/optionalFileGroups/group')
            choice_groups=[g for g in groups if g.get('type')=='SelectExactlyOne']
            self.assertEqual(len(choice_groups),3)
            options=[g.findall('./plugins/plugin') for g in choice_groups]
            self.assertEqual([len(o) for o in options],[3,6,2])
            defaults=[]
            for choices in options:
                recommended=[p for p in choices if p.find('./typeDescriptor/type').get('name')=='Recommended']
                self.assertEqual(len(recommended),1)
                self.assertIs(recommended[0],choices[-1])
                defaults.append(recommended[0])
            combinations=list(itertools.product(*options))
            self.assertEqual(len(combinations),36)
            for combination in combinations:
                flags={f.get('name'):f.text for plugin in combination for f in plugin.findall('./conditionFlags/flag')}
                install=[]
                for pattern in root.findall('./conditionalFileInstalls/patterns/pattern'):
                    conditions=pattern.findall('./dependencies/flagDependency')
                    if all(flags.get(c.get('flag'))==c.get('value') for c in conditions):
                        install.extend(pattern.findall('./files/file'))
                self.assertEqual(len(install),1)
                cfg=values((stage/install[0].get('source')).read_text())
                self.assertTrue(cfg.getboolean('Compatibility','MGOProfiles'))
                self.assertTrue(cfg.getboolean('SpeedTuning','NPCEnabled'))
                self.assertEqual(cfg.getfloat('SpeedTuning','NPCSpeedMult'),float(flags['npc']))
                expected={'slow':(.8,'Tuned'),'gentle':(.9,'Tuned'),'normal':(1,'Tuned'),'fast':(1.1,'Tuned'),'faster':(1.2,'Tuned'),'vanilla':(1,'Vanilla')}[flags['player']]
                self.assertEqual(cfg.getfloat('Pairing','PlayerSpeedMult'),expected[0])
                self.assertEqual(cfg.get('Pairing','ReferencePreset'),expected[1])
                self.assertEqual(cfg.getboolean('Immersion','ArrowInventoryWeight'),flags['weight']=='on')
                self.assertEqual(install[0].get('destination'),'SKSE/Plugins/BowSpeedVR.ini')
            flags={f.get('name'):f.text for p in defaults for f in p.findall('./conditionFlags/flag')}
            self.assertEqual(flags,{'npc':'1.0','player':'normal','weight':'off'})
            self.assertEqual(values((stage/'payload/BowSpeedVR.ini').read_text()).getfloat('SpeedTuning','NPCSpeedMult'),1)
            self.assertFalse(values((stage/'payload/BowSpeedVR.ini').read_text()).getboolean('Immersion','ArrowInventoryWeight'))
            for item in stage.rglob('*'):
                if item.is_file():
                    self.assertNotEqual(item.suffix.lower(),'.dll')
                    self.assertNotEqual(item.name,'RicochetFramework.ini')

    def test_standalone_installer_off_default_and_on_choice(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            stage=release.build_standalone(pathlib.Path(folder)/'main')
            root=ET.parse(stage/'fomod/ModuleConfig.xml').getroot()
            choices=root.findall('./installSteps/installStep/optionalFileGroups/group/plugins/plugin')
            self.assertEqual(len(choices),2)
            self.assertEqual(choices[-1].find('./typeDescriptor/type').get('name'),'Recommended')
            for choice in choices:
                flag=choice.find('./conditionFlags/flag')
                matching=[p for p in root.findall('./conditionalFileInstalls/patterns/pattern')
                          if p.find('./dependencies/flagDependency').get('value')==flag.text]
                self.assertEqual(len(matching),1)
                installed=matching[0].find('./files/file')
                cfg=values((stage/installed.get('source')).read_text())
                self.assertEqual(cfg.getboolean('Immersion','ArrowInventoryWeight'),flag.text=='on')
                self.assertFalse(cfg.getboolean('Compatibility','MGOProfiles'))
                self.assertFalse(cfg.getboolean('SpeedTuning','NPCEnabled'))
                self.assertEqual(installed.get('destination'),'SKSE/Plugins/BowSpeedVR.ini')
                self.assertEqual(installed.get('priority'),'100')

    def test_archives_install_only_own_files_and_are_reproducible(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            folder=pathlib.Path(folder)
            dll=folder/'BowSpeedVR.dll';dll.write_bytes(b'MZ-test-fixture-not-public')
            a=release.build_binary_archives(ROOT,dll,folder/'a')
            b=release.build_binary_archives(ROOT,dll,folder/'b')
            for left,right in zip(a,b):
                self.assertEqual(left.read_bytes(),right.read_bytes())
                with zipfile.ZipFile(left) as archive:
                    self.assertIsNone(archive.testzip())
                    for name in archive.namelist():
                        self.assertFalse(name.startswith('/') or '..' in pathlib.PurePosixPath(name).parts)
                        self.assertNotIn('RicochetFramework.ini',name)
                        self.assertNotIn('requiem-ammo-weight-help',name)
                        self.assertNotIn('backlog',name)
                        self.assertFalse(name.lower().endswith('.exe'))
            with zipfile.ZipFile(a[0]) as archive:
                self.assertEqual(archive.read('SKSE/Plugins/BowSpeedVR.dll'),dll.read_bytes())
                cfg=values(archive.read('SKSE/Plugins/BowSpeedVR.ini').decode())
                self.assertFalse(cfg.getboolean('SpeedTuning','NPCEnabled'))
                self.assertFalse(cfg.getboolean('Compatibility','MGOProfiles'))
                self.assertFalse(cfg.getboolean('ArrowPhysics','Enabled'))
                self.assertEqual(cfg.getfloat('General','PlayerSpeedMult'),1)
            with zipfile.ZipFile(a[1]) as archive:
                self.assertFalse(any(n.lower().endswith('.dll') for n in archive.namelist()))

    def test_rejects_nonempty_staging_and_never_deletes_other_files(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            stage=pathlib.Path(folder);guard=stage/'keep.txt';guard.write_text('preserve')
            with self.assertRaises(FileExistsError): release.build_mgo(stage)
            self.assertEqual(guard.read_text(),'preserve')

    def test_refuses_personal_paths_inside_binary_including_utf16(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            folder=pathlib.Path(folder);dll=folder/'BowSpeedVR.dll'
            for path in ['C:\\Users\\ExampleAccount\\build\\Logger.cpp',
                         'c:/users/ExampleAccount/build/IDDB.cpp']:
                for encoding in ['utf-8','utf-16-le']:
                    with self.subTest(path=path,encoding=encoding):
                        dll.write_bytes(b'MZ'+path.encode(encoding))
                        with self.assertRaisesRegex(ValueError,'personal build path'):
                            release.build_binary_archives(ROOT,dll,folder/'output')
            dll.write_bytes(b'MZCommonLibSSE-NG/src/SKSE/Logger.cpp')
            release.build_binary_archives(ROOT,dll,folder/'output')

    def test_source_sdk_keeps_build_inputs_and_excludes_sample_runtime(self):
        with tempfile.TemporaryDirectory(dir=ROOT.parent) as folder:
            folder=pathlib.Path(folder);deps=folder/'deps';deps.mkdir()
            source=deps/'OpenVR.zip'
            with zipfile.ZipFile(source,'w') as sdk:
                for name in ['LICENSE','README.md','headers/openvr.h','headers/openvr_capi.h',
                             'lib/win64/openvr_api.lib','samples/bin/win64/Qt5Core.dll',
                             'samples/bin/win64/example.exe','samples/bin/win64/example.pdb',
                             'bin/win64/openvr_api.dll']:
                    sdk.writestr(name,'fixture-'+name)
            manifest={'OpenVR':'60eb187801956ad277f1cae6680e3a410ee0873b',
                      'sources':{'OpenVR.zip':hashlib.sha256(source.read_bytes()).hexdigest()}}
            (deps/'manifest.json').write_text(json.dumps(manifest))
            output=release.build_source_archive(ROOT,folder,deps)
            with zipfile.ZipFile(output) as public:
                sdk_bytes=public.read('dependencies/OpenVR.zip')
                with zipfile.ZipFile(io.BytesIO(sdk_bytes)) as sdk:
                    self.assertEqual(set(sdk.namelist()),{'LICENSE','README.md','headers/openvr.h',
                                                        'headers/openvr_capi.h','lib/win64/openvr_api.lib'})
                actual=json.loads(public.read('dependencies/manifest.json'))
                self.assertEqual(actual['sources']['OpenVR.zip'],hashlib.sha256(sdk_bytes).hexdigest())
                self.assertIn('OpenVR',actual['filters'])
                self.assertEqual(actual['OpenVR'],manifest['OpenVR'])
            source.write_bytes(b'changed-after-provenance-recorded')
            with self.assertRaisesRegex(ValueError,'hash mismatch'):
                release.build_source_archive(ROOT,folder,deps)

if __name__=='__main__':unittest.main()
