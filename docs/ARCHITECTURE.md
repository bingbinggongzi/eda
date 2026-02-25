# Architecture Overview

## Layers

1. Model layer
- Graph document, nodes, ports, edges
- Serialization and version migration

2. Scene/View layer
- `QGraphicsScene` and `QGraphicsView`
- Node/port/edge visual items
- Selection, interaction and rendering

3. Command layer
- All mutating operations via command objects
- Undo/redo centralized in `QUndoStack`

## Core Principles

- View state must be derived from model state, not vice versa.
- Scene and model synchronization should use stable IDs.
- Keep interaction modes explicit (select/connect/pan/place).

## Proposed Source Tree Evolution

- `src/app/`: app startup and main window
- `src/model/`: document entities + serialization
- `src/scene/`: scene/view + interaction states
- `src/items/`: node/port/edge graphics items
- `src/commands/`: undoable editing commands
- `src/panels/`: project tree, palette, property inspector
