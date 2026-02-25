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

## Run Tests

```powershell
cmake --build build --config Debug --target eda_tests
ctest --test-dir build -C Debug --output-on-failure
```

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
