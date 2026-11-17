"""Create a plain USTAR .app for the 0.7.0 minitar installer; validate all members."""
from pathlib import Path
import hashlib
import json
import shutil
import tarfile
import configparser

root=Path(__file__).resolve().parents[1]
properties=configparser.ConfigParser(interpolation=None)
properties.optionxform=str
properties.read(root/'manifest.properties',encoding='utf-8')
assert properties['manifest']['version']=='0.1'
assert properties['target']['sdk']=='0.7.0'
assert properties['target']['platforms']=='esp32s3'
assert properties['app']['id']=='local.tvibe.dictionary'
version=properties['app']['versionName']
assert version=='0.2.0'
assert properties['app']['versionCode']=='14'
assert properties['app']['name']=='Vibe Dictionary'
stage=root/'release'/'local.tvibe.dictionary'
(stage/'elf').mkdir(parents=True,exist_ok=True)
(stage/'assets').mkdir(exist_ok=True)
elf=root/'build'/'VibeDictionary.app.elf'
assert elf.read_bytes()[:4]==b'\x7fELF'
assert elf.stat().st_mtime>=max(p.stat().st_mtime for p in (root/'main').glob('*') if p.suffix in ('.c','.h')), 'Rebuild ELF after source changes'
shutil.copyfile(elf,stage/'elf'/'esp32s3.elf')
for name in ['font18.bin','roots.pem','OFL-NotoSansSC.txt','LICENSE-certifi.txt','LICENSE-cJSON.txt','App-LICENSE.txt','App-NOTICE.txt']:
    shutil.copyfile(root/'assets'/name,stage/'assets'/name)
shutil.copyfile(root/'manifest.properties',stage/'manifest.properties')
package=stage.parent/f'VibeDictionary-{version}-esp32s3.app'
def public_metadata(member):
    member.uid = member.gid = 0
    member.uname = member.gname = ''
    return member

with tarfile.open(package,'w',format=tarfile.USTAR_FORMAT) as archive:
    for name in ['elf','assets']:
        archive.add(stage/name,arcname=name,recursive=True,filter=public_metadata)
    archive.add(stage/'manifest.properties',arcname='manifest.properties',filter=public_metadata)
with tarfile.open(package) as archive:
    for member in archive.getmembers():
        assert not member.name.startswith('/') and '..' not in Path(member.name).parts
        assert member.isfile() or member.isdir()
files=[]
for path in sorted(stage.rglob('*')):
    if path.is_file():
        files.append({'relative':path.relative_to(stage).as_posix(),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
report={'package':str(package),'sha256':hashlib.sha256(package.read_bytes()).hexdigest(),'files':files,'hardware_verified':False}
(stage.parent/'package-manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
assert package.is_file()
print(json.dumps({'package':str(package),'bytes':package.stat().st_size,'files':len(files)}))
