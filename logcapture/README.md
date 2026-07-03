# log-capture

Capture and inspect your app's own [hilog](https://developer.huawei.com/consumer/en/doc/harmonyos-references/hilog) output at runtime, from inside the app. Useful for building an in-app log panel, a shake-to-report diagnostics screen, or collecting logs for bug reports without a USB connection.

Built on the NDK `OH_LOG_SetCallback` API, wrapped as a NAPI module and shipped as a HAR.

## Features

- Register listeners that receive each log line as it is emitted
- Filter delivered logs by level, domain, tag, or keyword
- A rolling in-memory buffer you can read on demand (`fetchLogs`)
- A configurable `ignoreTag` that breaks the feedback loop when your listener itself logs

## Requirements

- HarmonyOS, stage model
- Minimum compatible SDK: API 12 (5.0.2). `OH_LOG_SetCallback` is available from API 11+.

## Install

```bash
ohpm install log-capture
```

## Quick start

```typescript
import { setCapture, on, off, fetchLogs, LogEntry } from 'log-capture';

// Enable capture once, early in app startup (e.g. AbilityStage.onCreate).
setCapture(true);

// Receive each new log line. Filter to your app's domain to skip system noise.
const listener = (entry: LogEntry) => {
  // Never log with a normal tag here, or it re-enters capture. Use the
  // ignoreTag, or push to your UI/store instead.
  // pushToPanel(entry)
};
on(listener, { domain: 0xFF00 });

// Read whatever is buffered right now (e.g. when opening a log screen).
const buffered = fetchLogs();

// Stop listening.
off(listener);
```

## API

### `setCapture(enable: boolean, ignoreTag?: string): void`

Turns the underlying hilog callback on or off. `ignoreTag` defaults to `'$$LOG_CAPTURE'`; any log whose tag equals it is dropped entirely (not buffered, not delivered).

### `getIgnoreTag(): string`

Returns the current `ignoreTag`. Log with this tag inside a listener to avoid re-capturing your own output.

### `on(cb: (entry: LogEntry) => void, options?: LogFilterOptions): void`

Adds a listener. Call multiple times to add several. `options` filters what this listener receives:

| field | type | match |
| --- | --- | --- |
| `level` | number | exact hilog level (3=DEBUG 4=INFO 5=WARN 6=ERROR 7=FATAL) |
| `domain` | number | exact domain |
| `tag` | string | exact tag |
| `keyword` | string | substring of `msg` |

### `off(cb?: (entry: LogEntry) => void): void`

Removes the given listener by reference. With no argument, removes all listeners.

### `fetchLogs(clear?: boolean): LogEntry[]`

Returns the buffered logs. Pass `true` to clear the buffer after reading.

### `clearLogs(): void`

Empties the buffer.

### `LogEntry`

```typescript
interface LogEntry {
  type: number;
  level: number;   // 3=DEBUG 4=INFO 5=WARN 6=ERROR 7=FATAL
  domain: number;
  tag: string;
  msg: string;
}
```

## Important notes

- **Process-global singleton.** Capture uses `OH_LOG_SetCallback`, which has a single system-wide callback slot. Treat this as a singleton per process: all listeners share one buffer, and enabling/disabling affects the whole process.
- **Avoid feedback loops.** A listener that logs with a normal tag will have that log captured and delivered again. Use the `ignoreTag` (see `getIgnoreTag`) for any logging inside a listener.
- **Buffer size** is capped in native code (500 entries) and drops the oldest first.
- **Obfuscation.** This package ships consumer obfuscation rules that keep its own export names, so it works even when your app enables `-enable-export-obfuscation`. You do not need to add keep rules yourself.

## License

Apache-2.0
