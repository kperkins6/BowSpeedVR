"""Build the standalone main download and configuration-only MGO FOMOD."""
import argparse
import configparser
import hashlib
import io
import json
import pathlib
import re
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
VERSION='1.1.0'
NPCS=[('0.8','Slower enemies — 0.8×'),('0.9','Slightly slower enemies — 0.9×'),('1.0','Default — 1.0×')]
PLAYERS=[('slow','Slower — 0.8×',.8,'Tuned'),('gentle','Slightly slower — 0.9×',.9,'Tuned'),
         ('fast','Slightly faster — 1.1×',1.1,'Tuned'),('faster','Faster — 1.2×',1.2,'Tuned'),
         ('vanilla','Vanilla calibrated — Long Bow + Iron',1,'Vanilla'),('normal','Default — 1.0×',1,'Tuned')]
WEIGHTS=[('on','Enable arrow inventory weight'),('off','Keep existing inventory behavior — Default')]

def standalone_ini(weight):
    cfg=configparser.ConfigParser();cfg.optionxform=str
    cfg.read(ROOT/'config/BowSpeedVR-standalone.ini',encoding='utf-8')
    cfg['Immersion']['ArrowInventoryWeight']='true' if weight=='on' else 'false'
    out=io.StringIO();cfg.write(out,space_around_delimiters=False)
    return f'; BowSpeedVR {VERSION} standalone preset. Restart after changes.\n'+out.getvalue()

def mgo_ini(npc,player,weight='off'):
    cfg=configparser.ConfigParser();cfg.optionxform=str
    cfg.read(ROOT/'config/BowSpeedVR-standalone.ini',encoding='utf-8')
    cfg['Compatibility']['MGOProfiles']='true'
    cfg['SpeedTuning'].update(NPCEnabled='true',NPCSpeedMult=npc,CompensateRicochet='true')
    cfg['ArrowPhysics']['Enabled']='true'
    cfg['Pairing'].update(ReferencePreset=player[3],PlayerSpeedMult=str(player[2]))
    cfg['Immersion']['ArrowInventoryWeight']='true' if weight=='on' else 'false'
    out=io.StringIO();cfg.write(out,space_around_delimiters=False)
    return f'; BowSpeedVR {VERSION} MGO preset. Restart the game after changes.\n'+out.getvalue()

def xml_write(root,path):
    ET.indent(root,space='  ')
    ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)

def add_weight_page(root):
    steps=root.find('installSteps')
    if steps is None: steps=ET.SubElement(root,'installSteps',order='Explicit')
    step=ET.SubElement(steps,'installStep',name='Immersion — arrow inventory weight')
    group=ET.SubElement(ET.SubElement(step,'optionalFileGroups',order='Explicit'),'group',name='Arrow inventory weight',type='SelectExactlyOne')
    plugins=ET.SubElement(group,'plugins',order='Explicit')
    for value,label in WEIGHTS:
        plugin=ET.SubElement(plugins,'plugin',name=label)
        ET.SubElement(plugin,'description').text=(
            'Give reviewed arrows inventory and carry weight using their physics mass × 0.10. '
            'Iron = 0.100, Steel ≈ 0.114, Glass ≈ 0.193, Daedric/Dragonbone ≈ 0.230, Bound ≈ 0.055 inventory units each. '
            'Per-arrow physics mass edits also change inventory weight. Applies to supported arrow items in all inventories; '
            'bolts and unknown items keep their existing behavior. This choice does not change flight or impact tuning. '
            'Avoid combining with another mod that independently adds arrow carry weight. Restart after changes.'
            if value=='on' else 'Keep existing inventory and carry-weight behavior. Default: OFF. Bow/arrow flight and impact still use the physics model.')
        ET.SubElement(ET.SubElement(plugin,'conditionFlags'),'flag',name='weight').text=value
        ET.SubElement(plugin,'files')
        ET.SubElement(ET.SubElement(plugin,'typeDescriptor'),'type',name='Recommended' if value=='off' else 'Optional')

def build_standalone(stage):
    stage=pathlib.Path(stage)
    if stage.resolve()!=stage.absolute(): raise ValueError('Staging must not traverse a symlink or junction')
    if stage.exists() and any(stage.iterdir()): raise FileExistsError('Refusing to replace nonempty staging directory')
    (stage/'fomod').mkdir(parents=True,exist_ok=True);(stage/'presets').mkdir()
    info=ET.Element('fomod')
    for key,value in [('Name','BowSpeedVR — Standalone'),('Author','BowSpeedVR contributors'),('Version',VERSION),
                      ('Description','Bow/arrow pairing physics with optional arrow inventory weight.')]:
        ET.SubElement(info,key).text=value
    xml_write(info,stage/'fomod/info.xml')
    root=ET.Element('config',{'xmlns:xsi':'http://www.w3.org/2001/XMLSchema-instance',
        'xsi:noNamespaceSchemaLocation':'http://qconsulting.ca/fo3/ModConfig5.0.xsd'})
    ET.SubElement(root,'moduleName',position='Left').text='BowSpeedVR — Standalone'
    required=ET.SubElement(root,'requiredInstallFiles')
    ET.SubElement(required,'file',source='SKSE/Plugins/BowSpeedVR.dll',destination='SKSE/Plugins/BowSpeedVR.dll',priority='0')
    ET.SubElement(required,'file',source='SKSE/Plugins/BowSpeedVR.ini',destination='SKSE/Plugins/BowSpeedVR.ini',priority='0')
    ET.SubElement(required,'folder',source='BowSpeedVR',destination='BowSpeedVR',priority='0')
    add_weight_page(root)
    patterns=ET.SubElement(ET.SubElement(root,'conditionalFileInstalls'),'patterns')
    for weight,_ in WEIGHTS:
        name=f'presets/weight-{weight}.ini';(stage/name).write_text(standalone_ini(weight),encoding='utf-8')
        pattern=ET.SubElement(patterns,'pattern')
        ET.SubElement(ET.SubElement(pattern,'dependencies',operator='And'),'flagDependency',flag='weight',value=weight)
        ET.SubElement(ET.SubElement(pattern,'files'),'file',source=name,destination='SKSE/Plugins/BowSpeedVR.ini',priority='100')
    xml_write(root,stage/'fomod/ModuleConfig.xml')
    return stage

def build_mgo(stage):
    stage=pathlib.Path(stage)
    if stage.resolve()!=stage.absolute(): raise ValueError('Staging must not traverse a symlink or junction')
    if stage.exists() and any(stage.iterdir()): raise FileExistsError('Refusing to replace nonempty staging directory')
    (stage/'fomod').mkdir(parents=True,exist_ok=True)
    (stage/'presets').mkdir();(stage/'payload').mkdir()
    normal=next(p for p in PLAYERS if p[0]=='normal')
    (stage/'payload/BowSpeedVR.ini').write_text(mgo_ini('1.0',normal),encoding='utf-8')
    info=ET.Element('fomod')
    for key,value in [('Name','BowSpeedVR — Mad God’s Overhaul patch'),('Author','BowSpeedVR contributors'),
                      ('Version',VERSION),('Description','Optional configuration for the standalone BowSpeedVR release.')]:
        ET.SubElement(info,key).text=value
    xml_write(info,stage/'fomod/info.xml')
    root=ET.Element('config',{'xmlns:xsi':'http://www.w3.org/2001/XMLSchema-instance',
        'xsi:noNamespaceSchemaLocation':'http://qconsulting.ca/fo3/ModConfig5.0.xsd'})
    ET.SubElement(root,'moduleName',position='Left').text='BowSpeedVR — MGO speed settings'
    required=ET.SubElement(root,'requiredInstallFiles')
    ET.SubElement(required,'file',source='payload/BowSpeedVR.ini',destination='SKSE/Plugins/BowSpeedVR.ini',priority='0')
    steps=ET.SubElement(root,'installSteps',order='Explicit')
    intro=ET.SubElement(steps,'installStep',name='Before you install')
    introgroup=ET.SubElement(ET.SubElement(intro,'optionalFileGroups',order='Explicit'),'group',name='Requirements',type='SelectAll')
    introplugin=ET.SubElement(ET.SubElement(introgroup,'plugins',order='Explicit'),'plugin',name='MGO configuration patch')
    ET.SubElement(introplugin,'description').text=(
        f'Install BowSpeedVR {VERSION} Standalone first. This patch installs only BowSpeedVR.ini. '
        'Let it overwrite the main configuration; keep one BowSpeedVR DLL enabled.\n\n'
        'NPC and player choices are independent and default to 1.0×. NPC controls affect ordinary actor-fired bow arrows; '
        'crossbows, spells, traps and reflected copies are excluded.\n\n'
        'NPC 1.0× means the installed bow/arrow speed before Ricochet’s global slowdown; it is not a vanilla trajectory preset. '
        'For slower enemy arrows with the default player model, select NPC 0.8× + player Default 1.0×. Ricochet.ini is read, never replaced. '
        'The immersion page chooses arrow inventory weight, default OFF. This complete patch configuration replaces the '
        'standalone installer’s weight choice, so select it again here. '
        'Restart after changes; keep Ricochet’s global speed fixed during a session. Health damage and draw timing are unchanged.')
    ET.SubElement(introplugin,'files')
    ET.SubElement(ET.SubElement(introplugin,'typeDescriptor'),'type',name='Required')
    for page,flag,choices in [('NPC bow-arrow speed','npc',NPCS),('Player bow-arrow speed','player',PLAYERS)]:
        step=ET.SubElement(steps,'installStep',name=page)
        group=ET.SubElement(ET.SubElement(step,'optionalFileGroups',order='Explicit'),'group',name=page,type='SelectExactlyOne')
        plugins=ET.SubElement(group,'plugins',order='Explicit')
        for choice in choices:
            value,label=choice[:2]
            plugin=ET.SubElement(plugins,'plugin',name=label)
            if flag=='npc':
                description=f'NPC arrows fly at {value}× the installed pre-Ricochet bow/arrow speed. Player speed is selected separately.'
            elif value=='vanilla':
                description='Set the pairing reference to 2,520 game units/s: the unmodified full-draw Long Bow + Iron Arrow launch speed. Keep relative bow/arrow differences. This calibrates speed, not gravity or health damage.'
            else:
                description=f'Player pairing speed is {choice[2]:g}× the 5,000 game-unit/s tuned reference. Preserve bow power, arrow mass and capacity differences. The speed ceiling and momentum also scale. NPC speed is independent.'
            ET.SubElement(plugin,'description').text=description
            ET.SubElement(ET.SubElement(plugin,'conditionFlags'),'flag',name=flag).text=value
            ET.SubElement(plugin,'files')
            ET.SubElement(ET.SubElement(plugin,'typeDescriptor'),'type',name='Recommended' if (flag=='npc' and value=='1.0') or (flag=='player' and value=='normal') else 'Optional')
    add_weight_page(root)
    patterns=ET.SubElement(ET.SubElement(root,'conditionalFileInstalls'),'patterns')
    for npc,_ in NPCS:
        for player in PLAYERS:
            for weight,_ in WEIGHTS:
                name=f'presets/npc-{npc}-player-{player[0]}-weight-{weight}.ini'
                (stage/name).write_text(mgo_ini(npc,player,weight),encoding='utf-8')
                pattern=ET.SubElement(patterns,'pattern')
                dependencies=ET.SubElement(pattern,'dependencies',operator='And')
                ET.SubElement(dependencies,'flagDependency',flag='npc',value=npc)
                ET.SubElement(dependencies,'flagDependency',flag='player',value=player[0])
                ET.SubElement(dependencies,'flagDependency',flag='weight',value=weight)
                ET.SubElement(ET.SubElement(pattern,'files'),'file',source=name,destination='SKSE/Plugins/BowSpeedVR.ini',priority='100')
    xml_write(root,stage/'fomod/ModuleConfig.xml')
    return stage

def zip_entries(path,entries):
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in sorted(entries):
            info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            archive.writestr(info,data)

def validate_public_dll(data):
    # Scan narrow strings and both possible UTF-16 alignments before packaging.
    strings=[data.decode('latin1')]+[data[start:].decode('utf-16-le',errors='ignore') for start in (0,1)]
    if any(re.search(r'[a-z]:[/\\](?:users|documents and settings)[/\\]|onedrive|referenced-chatgpt-conversation',s,re.IGNORECASE) for s in strings):
        raise ValueError('DLL contains a personal build path; rebuild with source path mapping')

def filtered_openvr_snapshot(data):
    entries=[]
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if archive.testzip() is not None: raise ValueError('OpenVR snapshot has corrupt entries')
        for item in archive.infolist():
            name=item.filename
            if item.is_dir(): continue
            parts=pathlib.PurePosixPath(name).parts
            if name.startswith('/') or '..' in parts: raise ValueError('Unsafe OpenVR snapshot path')
            if name in {'LICENSE','README.md','lib/win64/openvr_api.lib'} or name.startswith('headers/'):
                entries.append((name,archive.read(item)))
    names=[name for name,_ in entries]
    if len(names)!=len(set(names)): raise ValueError('Duplicate OpenVR snapshot path')
    if not {'LICENSE','headers/openvr.h','lib/win64/openvr_api.lib'}.issubset(names):
        raise ValueError('OpenVR snapshot is missing required SDK build inputs')
    out=io.BytesIO();zip_entries(out,entries)
    return out.getvalue()

def build_binary_archives(root,dll,output):
    root=pathlib.Path(root);output=pathlib.Path(output);output.mkdir(parents=True,exist_ok=True)
    dll_bytes=pathlib.Path(dll).read_bytes();validate_public_dll(dll_bytes)
    main=output/f'BowSpeedVR-{VERSION}-Standalone.zip'
    patch=output/f'BowSpeedVR-{VERSION}-MGO-FOMOD.zip'
    documents=['README.md','LICENSE','EXCEPTIONS.md','THIRD-PARTY.md','docs/INSTALLATION.md','docs/TESTING.md','docs/BUILDING.md','docs/vanilla-calibration.json']
    entries=[('SKSE/Plugins/BowSpeedVR.dll',dll_bytes),('SKSE/Plugins/BowSpeedVR.ini',(root/'config/BowSpeedVR-standalone.ini').read_bytes())]
    entries.extend(('BowSpeedVR/'+name,(root/name).read_bytes()) for name in documents)
    entries.extend(('BowSpeedVR/licenses/'+p.name,p.read_bytes()) for p in sorted((root/'licenses').iterdir()) if p.is_file())
    with tempfile.TemporaryDirectory(dir=output,prefix='release-') as temp:
        temp=pathlib.Path(temp);stage=build_mgo(temp/'mgo')
        main_stage=build_standalone(temp/'main')
        entries.extend((p.relative_to(main_stage).as_posix(),p.read_bytes()) for p in main_stage.rglob('*') if p.is_file())
        zip_entries(temp/main.name,entries)
        patch_entries=[(p.relative_to(stage).as_posix(),p.read_bytes()) for p in stage.rglob('*') if p.is_file()]
        patch_entries.append(('BowSpeedVR/MGO-INSTALLATION.md',(root/'docs/INSTALLATION.md').read_bytes()))
        zip_entries(temp/patch.name,patch_entries)
        (temp/main.name).replace(main);(temp/patch.name).replace(patch)
    return main,patch

def build_source_archive(root,output,dependencies):
    root=pathlib.Path(root);output=pathlib.Path(output)
    entries=[]
    for name in ['CMakeLists.txt','vcpkg.json','LICENSE','EXCEPTIONS.md','THIRD-PARTY.md','README.md',
                 'data/bow-records.csv','data/ammo-records.csv','data/matched-pair-design.json',
                 'docs/INSTALLATION.md','docs/TESTING.md','docs/BUILDING.md','docs/EDITOR.md','docs/vanilla-calibration.json','.clang-format']:
        entries.append(('BowSpeedVR/'+name,(root/name).read_bytes()))
    for folder in ['include','src','tests','scripts','config','licenses','dist','editor','presets']:
        for path in (root/folder).rglob('*'):
            if path.is_file() and '__pycache__' not in path.parts and path.suffix!='.pyc':
                entries.append(('BowSpeedVR/'+path.relative_to(root).as_posix(),path.read_bytes()))
    dependencies=pathlib.Path(dependencies)
    manifest=json.loads((dependencies/'manifest.json').read_text(encoding='utf-8'))
    if not manifest.get('sources'): raise ValueError('Matching dependency source is required')
    for name,digest in list(manifest['sources'].items()):
        if pathlib.PurePosixPath(name).name!=name or '\\' in name:
            raise ValueError('Dependency source names must be plain filenames')
        data=(dependencies/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest: raise ValueError(f'Dependency hash mismatch: {name}')
        if name=='OpenVR.zip':
            data=filtered_openvr_snapshot(data)
            manifest['sources'][name]=hashlib.sha256(data).hexdigest()
            manifest.setdefault('filters',{})['OpenVR']={
                'input_sha256':digest,
                'retained':['LICENSE','README.md','headers/*','lib/win64/openvr_api.lib'],
                'excluded':'Sample trees and runtime/debug binaries; required Windows x64 import library retained.'}
        entries.append(('dependencies/'+name,data))
    entries.append(('dependencies/manifest.json',(json.dumps(manifest,indent=2)+'\n').encode()))
    result=output/f'BowSpeedVR-{VERSION}-source.zip'
    zip_entries(result,entries)
    return result

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=pathlib.Path,required=True)
    parser.add_argument('--dll',type=pathlib.Path,default=ROOT/'build-release/BowSpeedVR.dll')
    parser.add_argument('--dependency-sources',type=pathlib.Path,required=True)
    args=parser.parse_args()
    paths=list(build_binary_archives(ROOT,args.dll,args.output))
    paths.append(build_source_archive(ROOT,args.output,args.dependency_sources))
    manifest={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths}
    (args.output/'archive-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':main()
