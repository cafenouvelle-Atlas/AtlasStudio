# Atlas AI Creator — milestone 1

Scope: extend the existing wxWidgets application. No AI runtime, subscription,
model download, cloud generation, voice recognition or automatic printing.

## Use

Open the **Atlas AI Creator** tab in editor mode.
Enter a description and click **Search existing models in browser**. The default
opens MakerWorld and STL Buddy, or select one provider. Only this explicit search
sends the description to the selected websites. Search requires internet; local
shape creation and slicing do not. Results are reviewed in the user's browser:
Atlas does not scrape/rank results, decide that a match exists, automatically
purchase/download files, or import external print profiles. Review the author's
license and import downloaded STL using the existing Prepare import command.

For local generation choose Box, Cylinder or Sphere and dimensions in millimetres.
Click **Create and open in Prepare**. The existing 3D renderer, selection tools,
STL export, slicing and G-code preview remain the workflow. Undo removes the new
object. The module uses bounded tessellation; bed dimensions still apply.
The search text does not generate arbitrary geometry in this milestone.

## Integration

`src/slic3r/GUI/AtlasAICreator`: native panel, UI-independent request validation,
primitive geometry adapter and fixed-destination URL builder. Appends the page by
window identity, so optional existing tab indices are not shifted. Plater bridge
uses existing model loading, plate placement, selection and undo infrastructure.
Existing source changes are recorded by exact LF-normalized hashes in
`atlas/source-changes.json`; baseline verification continues to reject all other
protected upstream changes. Existing source and profile licenses are retained.

## Validation

Locally passed: 48 dimension-validation cases, URL escaping/destination/length
checks, protected-source verification, whitespace and PowerShell parser/checksum
checks. Both search URL formats were verified in the sites' browser interfaces.

Windows native tests are required before delivering a runnable candidate:
`atlas_ai_creator_request_tests`, `atlas_ai_creator_mesh_tests` and existing
`atlas_centauri_profile_tests`. Mesh tests check dimensions, bounded topology,
closed/oppositely wound edges, volume, binary STL round-trip, profile loading,
real slicing and G-code export for all three primitives. Packaging requires these
tests present and passing. No native Windows pass claimed until CI evidence.
Existing deferred suites and legacy regressions remain disclosed.

## Manual Windows 11 acceptance

1. Start the previous baseline and record existing project/profile behavior.
2. Start the candidate. Check Home, Prepare, Preview, Device, Project and optional
   tabs; existing Bambu/ELEGOO profiles, import/export and saved project still work.
3. Create Box 20x30x40 mm, Cylinder diameter20 height40, Sphere diameter20.
   Inspect scale/placement; export STL, reimport, slice, preview and export G-code.
4. Add an object to an existing project; undo/redo, save/reopen, and confirm the
   original objects remain unchanged. Repeat on a second plate.
5. Reject zero, negative, NaN, excessive dimensions and invalid text. Round shape
   inactive fields must not affect validation. Check viewer-only and active-job
   guards. Check dark mode and 100/150/200% scaling.
6. Search `phone stand` on both sites and each site individually. Check spaces,
   ampersands, Unicode and empty input; no background browser opens while typing.
7. Disconnect internet: local shapes, STL export and slicing remain usable.
8. Record total process RAM/responsiveness on Windows11 with 8GB RAM and integrated
   graphics. This milestone adds no model weights or second 3D viewport.

No physical print is part of acceptance. Automated mesh tests do not validate
GUI interactions; manual Windows checks are still required.
