# Changelog

## 1.0.1

- Disable obfuscation to fix abnormal exported type names in the 1.0.0 obfuscated build.
- Remove unused resource files.

## 1.0.0

- Initial release.
- Capture in-app hilog output via `OH_LOG_SetCallback`.
- `setCapture` / `on` / `off` / `fetchLogs` / `clearLogs` / `getIgnoreTag`.
- Per-listener filtering by level, domain, tag, keyword.
- `ignoreTag` mechanism to prevent listener feedback loops.
- Ships `arm64-v8a` and `x86_64` native libraries.
