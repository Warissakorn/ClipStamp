# Repository Guidelines

## Project Structure & Module Organization

- `src/`: C++20 code in namespace `vt`, covering domain models, persistence, media inspection, rendering, export jobs, and Qt Widgets UI.
- `tests/`: Qt Test suites named `*_test.cpp`.
- `assets/`: embedded fonts and SVG icons; retain accompanying `licenses/` notices.
- `tools/windows/`: build, packaging, installer, and verification scripts.
- `docs/ARCHITECTURE.md`: timing and job design. Generated dependencies and outputs belong in ignored `.tools/`, `build/`, and `dist/` directories.

## Build, Test, and Development Commands

Run PowerShell commands from the repository root:

- `.\tools\windows\build.ps1 -Setup -Package`: fetch dependencies, build, test, and generate the portable application.
- `.\tools\windows\build.ps1`: rebuild and test using existing dependencies.
- `ctest --test-dir build --output-on-failure`: rerun suites; Qt and MinGW DLL directories must be on PATH. The build script configures them.
- `.\Launch-VideoTime.cmd`: launch locally; it prefers `dist/`, so refresh with `-Package` before reviewing changes.
- `.\tools\windows\build.ps1 -Installer`: build, test, package, and generate Windows Setup.
- `.\tools\windows\test-installer.ps1`: verify isolated installation, reinstall, startup, and removal.

## Coding Style & Naming Conventions

Use four-space indentation and same-line opening braces. Name types in PascalCase, functions and variables in camelCase, and private members with trailing underscores. Keep source filenames lowercase. Use UTF-8 and `QStringLiteral` for UI literals. No formatter or linter is configured; follow surrounding style and keep compiler warnings clean.

## Testing Guidelines

Use descriptive camelCase Qt Test slots, such as `calendarCrossesMidnight`. Add regression cases for changed timing, persistence, or export behavior. No numerical coverage threshold is configured. Release validation requires FFmpeg and ffprobe, with no skipped media tests. For UI changes, inspect native Windows screenshots at 1366×768 and verify Thai text and Arabic digits 0–9.

## Architecture & Data Safety

Read `docs/ARCHITECTURE.md` before changing timing or export pipelines. Keep Preview and export on the shared timestamp renderer. Preserve imported Date modified start times, original videos, and user projects. Invoke tools through `QProcess` argument lists. Update notices when changing bundled dependencies.

## Commit & Pull Request Guidelines

This checkout has no Git metadata, so historical commit conventions cannot be verified. Use concise, imperative subjects, such as `Reduce input heights`. Keep changes focused. PRs should describe behavior, link relevant issues, list validation results, and include screenshots for UI changes. Exclude generated SDKs, binaries, and test artifacts.
