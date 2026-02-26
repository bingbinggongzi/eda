# Changelog

All notable changes to this project should be documented in this file.

## [Unreleased]

### Added

- Initial Qt `QGraphicsView` UI prototype.
- Qt runtime deployment via `windeployqt`.
- Project planning document in `docs/IMPLEMENTATION_PLAN.md`.

### Changed

- Upgraded obstacle-avoid edge routing to weighted A* with turn penalties and start/end directional preferences.
- Added routing regression test (`obstacleRoutingDirectionalBias`) to guard against immediate backtracking and excessive bends.
- Added one-click auto layout action (`Edit -> Auto Layout`, `Ctrl+Shift+L`) with selected-node-first behavior.
- Added auto layout regression coverage for ordering, selection scope, and undo command registration.
- Added node transform actions (`Rotate +/-90`, z-order front/back/step) with undo and JSON persistence.
- Added `Group/Ungroup` actions with undo, scene rehydrate support, and grouped-selection behavior.
- Added auto layout v2 controls with explicit mode (`Layered` / `Grid`) and configurable spacing.
- Added routing v2 bundle/spread strategy so parallel edges between the same node pair do not fully overlap.
- Added group v2 visuals (frame/title) for grouped editing workflow readability.
- Persisted auto-layout settings (`mode`, `X spacing`, `Y spacing`) in JSON and wired dirty-state tracking.
- Added group v3 collapse/expand behavior with undo support and `collapsedGroups` persistence.
- Added routing v3 controls for bundle policy and spacing, including persistence and regression coverage.
- Added group v4 header toggle control (`+/-`) with click-driven collapse/expand and refined shortcuts.
- Added routing v4 global profile (`Balanced` / `Dense`) with adaptive high-density spread behavior and persistence.
