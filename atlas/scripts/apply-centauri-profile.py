"""Install the scoped official-source Centauri Carbon profile overlay. AGPL-3.0."""
from pathlib import Path
import argparse, base64, json


def apply(root, check=False):
    bundle_path = Path(__file__).with_name('centauri-carbon-bundle.json')
    bundle = json.loads(bundle_path.read_text())
    planned = {}
    for relative, data in bundle['files'].items():
        path = Path(relative)
        if path.is_absolute() or '..' in path.parts or path.parts[:2] != ('resources', 'profiles'):
            raise RuntimeError(f'Invalid profile overlay path: {relative}')
        content = (json.dumps(data['json'], indent=4) + '\n').encode() if 'json' in data else base64.b64decode(data['base64'], validate=True)
        planned[root / path] = content
    manifest_path = root / 'resources/profiles/Elegoo.json'
    manifest = json.loads(manifest_path.read_text())
    groups = {'machine_model': 'machine_model_list', 'machine': 'machine_list',
              'process': 'process_list', 'filament': 'filament_list'}
    for relative, data in bundle['files'].items():
        if 'json' not in data:
            continue
        preset = data['json']
        key = groups[preset['type']]
        manifest[key] = [item for item in manifest[key] if item['name'] != preset['name']]
        manifest[key].insert(0, {'name': preset['name'], 'sub_path': str(Path(relative).relative_to('resources/profiles/Elegoo')).replace('\\', '/')})
    manifest['version'] = '02.00.00.03'
    planned[manifest_path] = (json.dumps(manifest, indent=4) + '\n').encode()
    setup = """Atlas Studio - ELEGOO Centauri Carbon (original single-colour model)
Open the Printer dropdown -> Select/Remove printers(system presets) -> Elegoo.
Centauri Carbon is the first ELEGOO model; enable its 0.4mm variant.
Select ATLAS - ELEGOO Centauri Carbon 0.4 nozzle, then the matching ATLAS
0.20mm Standard process and ATLAS Elegoo PLA filament.
Initial scope: 0.4mm nozzle, local slicing and G-code export. No ELEGOO
network/MMS connection integration. Physical printer validation is pending.
Source: https://github.com/elegooofficial/ElegooSlicer
Original single-colour machine commands: 1abe50f97cc0e826e5e988106963b8dc4d159c79
Current official geometry/process/filament/assets: 2d507e39a96ab9562b35d06b23ef5df8fe613297
AGPL-3.0; exact adaptations and omitted Orca-only fields are recorded in
atlas/scripts/centauri-carbon-bundle.json within the supplied source archive.
"""
    planned[root / 'resources/profiles/Elegoo/ATLAS-CENTAURI-SETUP.txt'] = setup.encode()

    # Every default/compatibility link must resolve to a supplied concrete preset.
    presets = {d['json']['name']: d['json'] for d in bundle['files'].values() if 'json' in d}
    machine = presets['ATLAS - ELEGOO Centauri Carbon 0.4 nozzle']
    assert machine['inherits'] == '' and machine['nozzle_diameter'] == ['0.4']
    assert machine['printable_height'] == '256' and '256x256' in machine['printable_area']
    assert machine['default_print_profile'] in presets
    assert all(name in presets for name in machine['default_filament_profile'])
    for preset in presets.values():
        for name in preset.get('compatible_printers', []):
            assert name in presets
    if not check:
        for path, content in planned.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists() or path.read_bytes() != content:
                path.write_bytes(content)
    print(f'PASS: Centauri Carbon overlay {"validated" if check else "applied"}; {len(planned)} files')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    apply(args.root, args.check)
