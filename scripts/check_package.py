"""Verify actual minitar extraction against the package hash manifest."""
from pathlib import Path
import hashlib,json,sys
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'release/package-manifest.json').read_text(encoding='utf-8'))
folder=Path(sys.argv[1])
for row in manifest['files']:
    relative=Path(row['relative'])
    assert not relative.is_absolute() and '..' not in relative.parts
    data=(folder/relative).read_bytes()
    assert len(data)==row['bytes']
    assert hashlib.sha256(data).hexdigest()==row['sha256'],relative
print('All 9 extracted SHA256 values match the package manifest')
