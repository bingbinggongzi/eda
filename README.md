# EDA UI Prototype

Qt Widgets + `QGraphicsView` editor prototype for EDA-like workflow editing.

## Current Status

- UI shell completed (menu/toolbar/docks/tabbed canvas/palette)
- Canvas basics available (grid, zoom, pan, selection)
- Runtime deployment integrated with `windeployqt`
- Interactive graph editing available:
- Palette drag-drop node creation
- Port-to-port edge creation with live preview
- JSON save/load
- Undo/redo for add/move/connect/delete
- Component metadata-driven node creation (ports + defaults)
- Typed property editing (bool/int/double/string)
- Optional edge routing mode (`Manhattan` / `Avoid Nodes`)
- Dedicated move/rename/property undo commands with merge support
- Multi-document lifecycle (`New/Open/Close/Save As`) with dirty-state prompts
- Panel decomposition under `src/panels` (`ProjectTreePanel`, `PropertyPanel`, `PalettePanel`)
- Palette drag-drop visual feedback (drop preview box + guide lines)
- Lifecycle regression tests for `New/Open/Save As/Close` and unsaved prompt decisions
- UI snapshot regression tests with golden images and pixel-tolerance compare
- One-click auto layout (`Edit -> Auto Layout`) for selected nodes or full graph fallback
- Node transform actions (`Rotate +/-90`, z-order front/back/step)
- Group/Ungroup actions with JSON persistence (`Ctrl+G` / `Ctrl+Shift+G`)
- Auto layout v2 options: mode (`Layered` / `Grid`) and configurable spacing (`X` / `Y`)
- Routing v2 follow-up: dense parallel edges get bundle/spread offsets to reduce overlap
- Group v2: visual group frame/title for grouped editing readability
- Auto layout settings are persisted per document (`mode`, `X spacing`, `Y spacing`)
- Group v3: `Collapse Group` / `Expand Group` with undo and JSON persistence (`collapsedGroups`)
- Routing v3: configurable bundle policy (`Centered` / `Directional`) and bundle spacing
- Group v4: header toggle button (`+/-`) and refined collapse/expand shortcuts (`Ctrl+Alt+C` / `Ctrl+Alt+E`)

## Requirements

- Windows 10/11
- Visual Studio 2022 (MSVC toolchain)
- CMake 3.21+
- Qt 6 (tested with `6.8.3 msvc2022_64`)

## Quick Start

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\eda_ui_prototype.exe
```

or:

```powershell
.\scripts\build.ps1
.\scripts\run.ps1
```

## CLion Symbol Navigation (Qt Headers)

If "Go to Definition" on Qt symbols (for example `QGraphicsView`) does not jump to headers:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Then in CLion, reload CMake for your active profile.  
The project sets `CMAKE_NO_SYSTEM_FROM_IMPORTED=ON`, so Qt imported headers are indexed as normal includes (improves symbol navigation).

Optional (if Ninja is installed):

```powershell
cmake --preset ide-debug
cmake --build --preset ide-debug
```

## Run Tests

```powershell
cmake --build build --config Debug --target eda_tests
ctest --test-dir build -C Debug --output-on-failure
```

Stress run:

```powershell
.\scripts\stress.ps1
```

Update UI snapshot baselines:

```powershell
$env:EDA_UPDATE_SNAPSHOTS=1
ctest --test-dir build -C Debug --output-on-failure
Remove-Item Env:EDA_UPDATE_SNAPSHOTS
```

or:

```powershell
.\scripts\update_snapshots.ps1
```

## CI

GitHub Actions workflow (`.github/workflows/ci.yml`) runs build + tests on:

- push to `master`
- pull requests targeting `master`

If CMake cannot find Qt, set:

```powershell
$env:CMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64"
```

## Repository Layout

- `src/`: application source
- `docs/`: implementation and design docs
- `scripts/`: local helper scripts
- `examples/`: sample graph files

## Planning

Execution roadmap is tracked in:

- `docs/IMPLEMENTATION_PLAN.md`
