# EDA Editor Iteration Plan

Last updated: 2026-02-25

## 1. Goal

Build a Qt Widgets editor based on `QGraphicsView`, with UI and interaction style close to `o1.png`, while keeping a clean Model-View-Command architecture for long-term iteration.

## 2. Scope

MVP scope:
- Main window layout (menu, toolbar, left/right panels, central tabbed canvas)
- Canvas base interactions (grid, zoom, pan, rubber-band selection)
- Basic node/port/edge editing
- JSON persistence with schema version
- Undo/redo for core editing actions

Post-MVP:
- Auto layout
- Advanced obstacle-avoidance routing
- Multi-user collaboration

## 3. Technical Baseline

- C++17
- Qt 6 (Widgets)
- CMake + CMakePresets
- Architecture: `Model + Scene/View + Command`
- Data format: JSON with `schemaVersion`

## 4. Milestone Overview

| ID | Milestone | Est. Duration | Dependencies | Status |
|---|---|---:|---|---|
| M0 | Requirement freeze and interaction baseline | 0.5 day | None | Done |
| M1 | UI shell and project scaffold | 1 day | M0 | Done |
| M2 | Model and serialization | 1.5 days | M0 | Done |
| M3 | Canvas interaction hardening | 2 days | M1 | Done |
| M4 | Core graph items (Node/Port/Edge) | 2 days | M2, M3 | Done |
| M5 | Command system (Undo/Redo) | 2 days | M2, M4 | Done |
| M6 | Side panel integration | 2 days | M2, M4 | Done |
| M7 | Routing and visual polish | 2 days | M4, M6 | Done |
| M8 | Performance and stability | 1.5 days | M5, M7 | Done |
| M9 | Testing and delivery | 1 day | M8 | Done |

## 5. Detailed Breakdown and Acceptance

### M0 Requirement Freeze and Interaction Baseline

Tasks:
- [x] Freeze MVP in/out list
- [x] Freeze shortcut list and interaction modes
- [x] Freeze visual baseline (grid size, colors, fonts, line width)

Deliverables:
- Updated plan with locked scope

Acceptance:
- No frequent scope churn inside current sprint

### M1 UI Shell and Project Scaffold (Done)

Tasks:
- [x] `QMainWindow` with docks and tabbed central area
- [x] Menu and toolbar placeholders
- [x] Demo graph for visual validation
- [x] Build/run scripts and CMake presets

Deliverables:
- Runnable UI prototype

Acceptance:
- Layout regions match target structure

### M2 Model and Serialization

Tasks:
- [x] Implement `Document/Node/Port/Edge` data structures
- [x] Add stable ID generation and references
- [x] Implement JSON import/export with `schemaVersion`
- [x] Add migration entry point for future versions

Deliverables:
- `src/model/*` and `src/model/io/*`

Acceptance:
- Save/load roundtrip preserves topology and coordinates

### M3 Canvas Interaction Hardening

Tasks:
- [x] Grid, Ctrl+wheel zoom, middle-button pan, rubber-band select
- [x] Zoom limits and zoom status indicator
- [x] Grid snapping with toggle
- [x] Explicit interaction mode state machine (select/connect/place/pan)

Deliverables:
- Hardened `EditorView` and `EditorScene` interaction base

Acceptance:
- Stable interactions with predictable zoom anchor

### M4 Core Graph Items

Tasks:
- [x] Implement structured `NodeItem` layout (title + ports)
- [x] Implement `PortItem` direction and connection constraints
- [x] Implement data-driven `EdgeItem` rendering
- [x] Recompute only related edges on node move

Deliverables:
- `src/items/*`

Acceptance:
- Moving a node updates all related edges correctly

### M5 Command System (Undo/Redo)

Tasks:
- [x] Integrate `QUndoStack`
- [x] Add commands for add/delete/move node
- [x] Add commands for connect/disconnect edge
- [x] Add merge strategy for grouped actions

Deliverables:
- `src/commands/*`

Acceptance:
- Repeated undo/redo keeps consistent model and scene states

### M6 Side Panel Integration

Tasks:
- [x] Drag from palette to create real nodes
- [x] Property panel two-way binding
- [x] Project tree and scene selection sync
- [x] Preserve context on tab/document switch

Deliverables:
- `src/panels/*` + scene synchronization glue

Acceptance:
- Panels and canvas remain in sync across edits

### M7 Routing and Visual Polish

Tasks:
- [x] Manhattan edge routing
- [x] Hover/selection highlight
- [x] Live ghost edge while connecting
- [x] Alignment guides and port snap feedback

Deliverables:
- Routing and visual feedback modules

Acceptance:
- Graph readability is clearly improved in complex scenes

### M8 Performance and Stability

Tasks:
- [x] Dirty-region update strategy
- [x] Partial recompute for edge updates
- [x] Large-scene stress test (target: 1000 node scale)
- [x] Crash path hardening and error handling

Deliverables:
- Profiling notes and optimization changes

Acceptance:
- No obvious lag/regression on large test graph

### M9 Testing and Delivery

Tasks:
- [x] Unit tests for model layer
- [x] Regression tests for command layer
- [x] Manual smoke checklist
- [x] Example project and short user guide

Deliverables:
- `tests/*`, sample project, release notes

Acceptance:
- Core flow passes: create -> edit -> save -> reopen

## 6. Current Baseline (Completed)

- Qt environment installed and validated
- CMake fallback Qt discovery is in place
- `windeployqt` post-build deployment is integrated
- Initial UI prototype implemented
- Project scaffolding files added (`README`, `CONTRIBUTING`, `CHANGELOG`, presets, scripts)
- Interactive graph editing implemented (`Node/Port/Edge`, drag-drop create, connect preview, move updates)
- JSON save/load pipeline implemented and bound to menu actions
- Undo/redo stack integrated for add/move/connect/delete
- Test target `eda_tests` and smoke checklist added
- Multi-tab editor context switching implemented with per-tab scene/view state
- Performance notes and stress harness added (`docs/PERFORMANCE.md`, `scripts/stress.ps1`)
- CI workflow added (`.github/workflows/ci.yml`)
- Legacy schema migration tests and component metadata registry integrated
- Typed property editors added for bool/int/double/string properties
- Source tree aligned to `src/app|model|scene|items|commands|panels`
- Optional routing mode toggle added (`Manhattan` / `Avoid Nodes`)
- Dedicated move/rename/property command classes integrated with merge behavior
- Multi-document file lifecycle added (`New/Open/Close/Save As` + dirty prompts)
- Panel widgets decomposed into `src/panels` for project tree/property/palette ownership
- Palette drag-drop preview feedback added in canvas foreground
- File lifecycle regression tests added (`New/Open/Save As/Close` + dirty prompt branches)
- UI snapshot regression framework added (golden images + tolerance compare + diff artifacts)

## 7. Current Execution Queue (Next Up)

1. Routing v2: improve obstacle avoidance quality (fewer bends, stronger directional constraints)
2. Auto layout: provide one-click layered/grid arrangement for selected graph

## 8. Merge Quality Gates

- Local `cmake configure + build` passes
- Main flow remains runnable
- Plan status and actual implementation stay aligned
- Changes are isolated and reversible by commit

## 9. Update Rules

- This file is the single source of execution truth
- Update `Last updated` whenever plan changes
- Move milestone status to Done only after acceptance is met
- Reflect major scope changes explicitly in commit messages
