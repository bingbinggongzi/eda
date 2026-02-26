# UI Snapshot Baselines

Golden images used by `eda_tests` snapshot checks:

- `main_window_default.png`
- `graph_view_drag_preview.png`

Update baselines locally:

```powershell
$env:EDA_UPDATE_SNAPSHOTS=1
ctest --test-dir build -C Debug --output-on-failure
Remove-Item Env:EDA_UPDATE_SNAPSHOTS
```

After updating, re-run tests without `EDA_UPDATE_SNAPSHOTS` and commit the changed `.png` files.
