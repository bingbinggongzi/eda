# Smoke Checklist

Last updated: 2026-02-25

## Startup

- [ ] Build succeeds (`cmake --build build --config Debug`)
- [ ] App launches (`build/Debug/eda_ui_prototype.exe`)
- [ ] Main layout is visible (left tree, center canvas, right palette)

## Canvas Core

- [ ] Ctrl+wheel zoom works and status bar updates zoom percent
- [ ] Middle mouse pan works
- [ ] Rubber-band selection works
- [ ] Grid is visible

## Graph Editing

- [ ] Drag palette item to canvas creates a node
- [ ] Node has input/output ports
- [ ] Drag from one port to another creates an edge
- [ ] Moving a node updates connected edges

## Panels

- [ ] Selecting node updates property panel
- [ ] Editing `Name` in property panel updates node title
- [ ] Editing `X`/`Y` moves selected node
- [ ] Selecting node in scene highlights corresponding item in project tree
- [ ] Clicking node in project tree selects and centers it in scene

## Persistence

- [ ] Save graph to JSON
- [ ] Clear graph
- [ ] Open saved JSON
- [ ] Topology, node names, positions and edges are restored

## Undo/Redo

- [ ] Add node -> Undo removes it -> Redo restores it
- [ ] Connect edge -> Undo removes it -> Redo restores it
- [ ] Move node -> Undo reverts position -> Redo reapplies position
- [ ] Delete selected items -> Undo restores -> Redo deletes again

## Tests

- [ ] `ctest --test-dir build -C Debug --output-on-failure` passes
