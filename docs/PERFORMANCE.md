# Performance Notes

Last updated: 2026-02-25

## Current Runtime Strategies

- `QGraphicsView::SmartViewportUpdate` enabled.
- Edge updates are local: moving a node updates only edges connected to that node's ports.
- Grid rendering is lightweight line drawing in background pass.
- Scene serialization/import keeps stable IDs, reducing remapping overhead.

## Stress Harness

Automated stress coverage is in:

- `tests/test_suite.cpp::stressLargeGraphBuild`

The test builds:

- 1000 nodes
- 999 edges

And verifies topology integrity after construction.

## Run

```powershell
cmake --build build --config Debug --target eda_tests
ctest --test-dir build -C Debug --output-on-failure
```

## Next Optimizations

- Introduce item indexing strategy tuning for huge scenes.
- Add incremental rerouting for complex edge graphs.
- Add perf counters in status bar (frame time, item count).
