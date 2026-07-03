import { LogEntry } from 'liblogcapture.so';

// Mock used for previewer / simulator when the native .so is unavailable.
const NativeMock: Record<string, Object> = {
  'setCapture': (_enable: boolean, _ignoreTag?: string) => {},
  'getIgnoreTag': () => '$$LOG_CAPTURE',
  'on': (_cb: (entry: LogEntry) => void, _options?: Object) => {},
  'off': (_cb?: (entry: LogEntry) => void) => {},
  'fetchLogs': (_clear?: boolean): Array<LogEntry> => [],
  'clearLogs': () => {},
};

export default NativeMock;
