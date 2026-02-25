# EDA Editor Implementation Plan

Last updated: 2026-02-25

## 1. Goal

Build a Qt Widgets editor based on `QGraphicsView`, with UI and interaction style close to `o1.png`.

## 2. Scope

In scope (MVP):
- Main window layout close to target screenshot.
- Graph canvas with grid, zoom, pan, selection.
- Node/port/edge editing basics.
- JSON save/load.
- Undo/redo for core editing actions.

Out of scope (post-MVP):
- Full auto-layout.
- Complex obstacle-avoidance routing.
- Multi-user collaboration.

## 3. Technical Baseline

- Language: C++17
- UI framework: Qt 6 (Widgets, Graphics View Framework)
- Build system: CMake
- Core architecture: `Model + Scene/View + Command`
- Persistence: JSON with `schemaVersion`

## 4. Architecture Rules

- Persist only Model objects; never persist `QGraphicsItem` directly.
- Every node/port/edge must have a stable unique ID.
- Scene and Model synchronize by ID mapping.
- Any data-mutating action must go through command objects for undo/redo support.

## 5. Milestones

| ID | Milestone | Deliverables | Acceptance Criteria | Status |
|---|---|---|---|---|
| M0 | Requirement freeze | MVP boundary, shortcuts, visual baseline | No ambiguous scope for current sprint | Planned |
| M1 | UI shell and project skeleton | MainWindow + Dock layout + tabbed canvas | Layout structure matches target zones | Done |
| M2 | Model and serialization | Document/Node/Port/Edge + JSON IO | Save-load roundtrip keeps topology and coordinates | Planned |
| M3 | Canvas base interactions | Grid, zoom, pan, rubber-band select | Smooth operation, predictable zoom anchor | In progress |
| M4 | Core graph items | NodeItem/PortItem/EdgeItem | Node movement updates related edges correctly | Planned |
| M5 | Command system | `QUndoStack` + add/delete/move/connect commands | Ctrl+Z/Ctrl+Y works without state corruption | Planned |
| M6 | Side panel integration | Palette drag-drop, project tree, property editor | UI panels and canvas reflect same data | In progress |
| M7 | Routing and visuals | Manhattan edge, highlight, snapping guides | Readability and editability significantly improved | Planned |
| M8 | Performance and stability | Scene optimization, redraw optimization | Large graph remains responsive | Planned |
| M9 | Tests and delivery | Unit tests, smoke checklist, sample project | Critical flow passes regression checks | Planned |

## 6. Current Baseline (already completed)

- Qt environment installed and validated.
- CMake configured to auto-discover Qt under `C:/Qt`.
- `windeployqt` post-build deployment enabled.
- Initial UI prototype implemented:
- Top menu and toolbar.
- Left project tree + property panel.
- Center tabbed graph area.
- Right categorized component palette.
- Demo graph nodes and edges for visual preview.
- Project iteration scaffolding added:
- `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`
- `.editorconfig`, `.gitattributes`, expanded `.gitignore`
- `CMakePresets.json`
- `scripts/build.ps1` and `scripts/run.ps1`

## 7. Short-Term Execution Queue

1. Implement palette drag-drop to create real nodes on canvas.
2. Convert demo edges into data-driven edge items.
3. Add port-level connection interaction and live preview edge.
4. Introduce Model layer and ID mapping.
5. Wire core operations into undo/redo commands.

## 8. Update Rules

- Keep this file as the single source of truth for execution status.
- Update `Last updated` date on every meaningful planning change.
- Move milestone status only when acceptance criteria are met.
- Record major scope changes in commit messages.
