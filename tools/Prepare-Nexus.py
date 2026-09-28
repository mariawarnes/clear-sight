"""Prepare local Nexus deliverables; never upload or modify the game installation."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import uuid
import zipfile

root = Path(__file__).resolve().parents[1]
release = root / 'dist/ClearSight-0.3.13-Nexus'
release.mkdir(parents=True, exist_ok=True)
stage = root / 'build' / ('source-stage-' + uuid.uuid4().hex)
stage.mkdir()
excluded_suffixes = {'.exe', '.dll', '.lib', '.pdb', '.obj', '.o', '.a', '.so', '.dylib', '.zip', '.7z', '.rar', '.tar', '.gz', '.xz', '.bz2'}

def copy_source(src, dst):
    for item in src.rglob('*'):
        rel = item.relative_to(src)
        if any(part in {'.git', '__pycache__'} for part in rel.parts):
            continue
        if item.is_file() and item.suffix.lower() not in excluded_suffixes:
            target = dst / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)

copy_source(root / 'native', stage / 'native')
(stage / 'tools').mkdir()
shutil.copy2(root / 'tools/Build-ReleaseSource.ps1', stage / 'tools')
for name in ('COPYING', 'EXCEPTIONS.md'):
    shutil.copy2(root / name, stage)
for name in ('SOURCE-BUILD.md', 'LICENSING.md'):
    shutil.copy2(root / 'docs/release' / name, stage)
shutil.copy2(root / 'docs/NATIVE-VALIDATION.md', stage)
shutil.copy2(root / 'docs/NATIVE-INSTALL.md', stage)
(stage / 'README.md').write_text('''# Clear Sight 0.3.13 source\n\nAuthor: mlw05. Read SOURCE-BUILD.md for build instructions and LICENSING.md for terms.\nThis is the corresponding-source download, not the installable mod.\n''', encoding='utf-8')

lib = root / 'build/CommonLibSSE-NG'
assert subprocess.check_output(['git', '-C', str(lib), 'rev-parse', 'HEAD'], text=True).strip() == '70c1acd5261210982bd52f6d4468a082fe04d798'
assert not subprocess.check_output(['git', '-C', str(lib), 'status', '--porcelain'], text=True).strip(), 'CommonLib has unrecorded changes'
tracked = subprocess.check_output(['git', '-C', str(lib), 'ls-files', '-z']).decode().split('\0')
for rel in tracked:
    source = lib / rel
    if not rel or not source.is_file() or source.suffix.lower() in excluded_suffixes:
        continue
    dest = stage / 'vendor/CommonLibSSE-NG' / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
copy_source(lib / 'extern/openvr/headers', stage / 'vendor/CommonLibSSE-NG/extern/openvr/headers')
shutil.copy2(lib / 'extern/openvr/LICENSE', stage / 'vendor/CommonLibSSE-NG/extern/openvr/LICENSE')

dependency_sources = {
    'fmt': '12.2.0-d352ede310.clean',
    'spdlog': 'v1.17.0-8a1a388e84.clean',
    'directxtk': 'may2026-a68e548ea6.clean',
    'directxmath': 'jun2026-d9b23f80e1.clean',
    'rapidcsv': 'v9.07-e65237600d.clean',
}
for name, folder in dependency_sources.items():
    source = root / 'build/vcpkg/buildtrees' / name / 'src' / folder
    assert source.is_dir(), source
    copy_source(source, stage / 'vendor/dependencies' / name)
for name in (*dependency_sources, 'vcpkg-cmake', 'vcpkg-cmake-config'):
    copy_source(root / 'build/vcpkg/ports' / name, stage / 'vendor/vcpkg-ports' / name)
shutil.copy2(root / 'build/vcpkg/installed/vcpkg/status', stage / 'DEPENDENCIES.txt')

source_zip = release / 'ClearSight-0.3.13-Source.zip'
with zipfile.ZipFile(source_zip, 'w', compression=zipfile.ZIP_STORED) as z:
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            z.write(path, path.relative_to(stage).as_posix())

main_zip = release / 'ClearSight-0.3.13.zip'
shutil.copy2(root / 'build/ClearSight-High-Contrast-Accessibility-0.3.13.zip', main_zip)
for name in ('NEXUS-FIELDS.md', 'NEXUS-DESCRIPTION.txt', 'SOURCE-BUILD.md', 'LICENSING.md'):
    shutil.copy2(root / 'docs/release' / name, release)
(release / 'images').mkdir(exist_ok=True)
shutil.copy2(root / 'assets/marketing/ClearSight-Nexus-thumbnail.png', release / 'images')

def digest(data):
    return hashlib.sha256(data).hexdigest()

reports = []
for archive, reference in ((main_zip, root / 'build/clear-sight-package'), (source_zip, stage)):
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        entries = [i for i in z.infolist() if not i.is_dir()]
        names = [i.filename.replace('\\', '/') for i in entries]
        assert len(names) == len(set(names))
        for entry, name in zip(entries, names):
            assert not name.startswith('/') and '..' not in Path(name).parts
            assert digest(z.read(entry)) == digest((reference / name).read_bytes()), name
        if archive == source_zip:
            assert not any(Path(n).suffix.lower() in excluded_suffixes for n in names)
        else:
            assert set(n for n in names if not n.startswith('ClearSight-Documentation/')) == {'SKSE/Plugins/ClearSight.dll', 'SKSE/Plugins/ClearSight.ini'}
        (release / (archive.stem + '-CONTENTS.txt')).write_text('\n'.join(names) + '\n', encoding='utf-8')
        reports.append({'archive': archive.name, 'files': len(entries), 'bytes': archive.stat().st_size,
                        'sha256': digest(archive.read_bytes()), 'crc': 'passed', 'source_comparison': 'all files match staging'})

(release / 'SHA256SUMS.txt').write_text('\n'.join(f'{digest(p.read_bytes())}  {p.relative_to(release).as_posix()}' for p in [main_zip, source_zip, release / 'images/ClearSight-Nexus-thumbnail.png']) + '\n', encoding='utf-8')
(release / 'ARCHIVE-VERIFICATION.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
print(json.dumps(reports, indent=2))
print('Source staging: ' + str(stage))
