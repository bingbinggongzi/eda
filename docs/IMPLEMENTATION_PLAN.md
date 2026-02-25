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
| M0 | Requirement freeze and interaction baseline | 0.5 day | None | Planned |
| M1 | UI shell and project scaffold | 1 day | M0 | Done |
| M2 | Model and serialization | 1.5 days | M0 | Planned |
| M3 | Canvas interaction hardening | 2 days | M1 | In progress |
| M4 | Core graph items (Node/Port/Edge) | 2 days | M2, M3 | Planned |
| M5 | Command system (Undo/Redo) | 2 days | M2, M4 | Planned |
| M6 | Side panel integration | 2 days | M2, M4 | In progress |
| M7 | Routing and visual polish | 2 days | M4, M6 | Planned |
| M8 | Performance and stability | 1.5 days | M5, M7 | Planned |
| M9 | Testing and delivery | 1 day | M8 | Planned |

## 5. Detailed Breakdown and Acceptance

### M0 Requirement Freeze and Interaction Baseline

Tasks:
- [ ] Freeze MVP in/out list
- [ ] Freeze shortcut list and interaction modes
- [ ] Freeze visual baseline (grid size, colors, fonts, line width)

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
- [ ] Implement `Document/Node/Port/Edge` data structures
- [ ] Add stable ID generation and references
- [ ] Implement JSON import/export with `schemaVersion`
- [ ] Add migration entry point for future versions

Deliverables:
- `src/model/*` and `src/model/io/*`

Acceptance:
- Save/load roundtrip preserves topology and coordinates

### M3 Canvas Interaction Hardening

Tasks:
- [x] Grid, Ctrl+wheel zoom, middle-button pan, rubber-band select
- [ ] Zoom limits and zoom status indicator
- [ ] Grid snapping with toggle
- [ ] Explicit interaction mode state machine (select/connect/place/pan)

Deliverables:
- Hardened `EditorView` and `EditorScene` interaction base

Acceptance:
- Stable interactions with predictable zoom anchor

### M4 Core Graph Items

Tasks:
- [ ] Implement structured `NodeItem` layout (title + ports)
- [ ] Implement `PortItem` direction and connection constraints
- [ ] Implement data-driven `EdgeItem` rendering
- [ ] Recompute only related edges on node move

Deliverables:
- `src/items/*`

Acceptance:
- Moving a node updates all related edges correctly

### M5 Command System (Undo/Redo)

Tasks:
- [ ] Integrate `QUndoStack`
- [ ] Add commands for add/delete/move node
- [ ] Add commands for connect/disconnect edge
- [ ] Add merge strategy for grouped actions

Deliverables:
- `src/commands/*`

Acceptance:
- Repeated undo/redo keeps consistent model and scene states

### M6 Side Panel Integration

Tasks:
- [ ] Drag from palette to create real nodes
- [ ] Property panel two-way binding
- [ ] Project tree and scene selection sync
- [ ] Preserve context on tab/document switch

Deliverables:
- `src/panels/*` + scene synchronization glue

Acceptance:
- Panels and canvas remain in sync across edits

### M7 Routing and Visual Polish

Tasks:
- [ ] Manhattan edge routing
- [ ] Hover/selection highlight
- [ ] Live ghost edge while connecting
- [ ] Alignment guides and port snap feedback

Deliverables:
- Routing and visual feedback modules

Acceptance:
- Graph readability is clearly improved in complex scenes

### M8 Performance and Stability

Tasks:
- [ ] Dirty-region update strategy
- [ ] Partial recompute for edge updates
- [ ] Large-scene stress test (target: 1000 node scale)
- [ ] Crash path hardening and error handling

Deliverables:
- Profiling notes and optimization changes

Acceptance:
- No obvious lag/regression on large test graph

### M9 Testing and Delivery

Tasks:
- [ ] Unit tests for model layer
- [ ] Regression tests for command layer
- [ ] Manual smoke checklist
- [ ] Example project and short user guide

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

## 7. Current Execution Queue (Next Up)

1. Palette drag-drop creates real nodes (M6)
2. Data-driven `EdgeItem` and node-move link updates (M4)
3. Port-level connect interaction with live preview (M7)
4. Minimal model layer and ID mapping pipeline (M2)
5. First undo commands: add/move/connect (M5)

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
