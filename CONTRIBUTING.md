# Contributing

## Workflow

1. Create/update a short plan in `docs/IMPLEMENTATION_PLAN.md`.
2. Keep each commit focused on a single change set.
3. Build locally before committing.
4. Push and open a PR (or commit directly if working solo).

## Commit Style

Recommended commit prefix:

- `feat:` new feature
- `fix:` bug fix
- `refactor:` structural change without behavior change
- `docs:` documentation changes
- `chore:` build/tooling updates

## Local Validation

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Coding Notes

- Keep model and graphics items decoupled.
- Prefer command-based mutations for undo/redo compatibility.
- Add comments only for non-obvious logic.
