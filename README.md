# log-capture

Capture and inspect your app's own [hilog](https://developer.huawei.com/consumer/en/doc/harmonyos-references/hilog) output at runtime, from inside the app. Built on the NDK `OH_LOG_SetCallback` API, wrapped as a NAPI module and shipped as a HAR.

This repository is a DevEco Studio project:

- **`logcapture/`** — the publishable HAR (the actual library). See **[logcapture/README.md](logcapture/README.md)** for installation, the full API, and usage notes.
- **`entry/`** — a demo app that exercises the library on a device/emulator.

## Install

```bash
ohpm install log-capture
```

Full documentation: **[logcapture/README.md](logcapture/README.md)**

## License

Apache-2.0
