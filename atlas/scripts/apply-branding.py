"""Apply the scoped Atlas Studio display-name overlay. AGPL-3.0.

Run after verify_baseline.py and before CMake configuration. Internal DLL names,
settings keys, printer profiles, protocol identifiers and upstream credits remain
compatible with the tested development candidate.
"""
from pathlib import Path
import argparse

PATCHES = {
    'version.inc': [('set(SLIC3R_APP_NAME "BambuStudio")', 'set(SLIC3R_APP_NAME "Atlas Studio")')],
    'src/libslic3r/libslic3r.h': [
        ('#define SLIC3R_APP_FULL_NAME "Bambu Studio"', '#define SLIC3R_APP_FULL_NAME "Atlas Studio"'),
        ('#define GCODEVIEWER_APP_NAME "BambuStudio G-code Viewer"', '#define GCODEVIEWER_APP_NAME "Atlas Studio G-code Viewer"'),
    ],
    'src/CMakeLists.txt': [
        ('set_target_properties(BambuStudio_app_gui PROPERTIES OUTPUT_NAME "bambu-studio")',
         'set_target_properties(BambuStudio_app_gui PROPERTIES OUTPUT_NAME "atlas-studio")'),
    ],
    'src/platform/msw/BambuStudio.rc.in': [
        ('VALUE "CompanyName", "Bambu Research"', 'VALUE "CompanyName", "Atlas Studio Project"'),
        ('VALUE "OriginalFilename", "bambu-studio.exe"', 'VALUE "OriginalFilename", "atlas-studio.exe"'),
    ],
    'src/slic3r/GUI/MainFrame.cpp': [
        ('SetTitle(title + " - BambuStudio");', 'SetTitle(title + " - Atlas Studio");'),
    ],
    'src/slic3r/GUI/AboutDialog.cpp': [
        ('_L("Bambu Studio is licensed under ")', '_L("Atlas Studio is licensed under ")'),
        ('_L("Bambu Studio is based on PrusaSlicer by Prusa Research, which is from Slic3r by Alessandro Ranellucci and the RepRap community")',
         '_L("Atlas Studio is based on Bambu Studio, which is based on PrusaSlicer by Prusa Research, Slic3r by Alessandro Ranellucci and the RepRap community")'),
    ],
}

def apply(root, check=False):
    planned = {}
    for relative, replacements in PATCHES.items():
        path = root / relative
        original = path.read_bytes()
        text = original.decode('utf-8')
        for before, after in replacements:
            if text.count(before) == 1:
                text = text.replace(before, after, 1)
            elif before not in text and text.count(after) == 1:
                continue
            else:
                raise RuntimeError(f'Branding anchor changed: {relative}: {before}')
        planned[path] = text.encode('utf-8')
    if not check:
        for path, content in planned.items():
            if path.read_bytes() != content:
                path.write_bytes(content)
    print(f'PASS: Atlas Studio branding {"validated" if check else "applied"} in {len(planned)} files')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    apply(args.root, args.check)
