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
