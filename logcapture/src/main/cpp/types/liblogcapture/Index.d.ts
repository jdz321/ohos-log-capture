export interface LogEntry {
  type: number;
  level: number;
  domain: number;
  tag: string;
  msg: string;
}

export interface LogFilterOptions {
  /** hilog LogLevel: 3=DEBUG 4=INFO 5=WARN 6=ERROR 7=FATAL */
  level?: number;
  domain?: number;
  /** exact tag match */
  tag?: string;
  /** substring match on msg */
  keyword?: string;
}

export type LogListener = (entry: LogEntry) => void;

export const setCapture: (enable: boolean, ignoreTag?: string) => void;
export const getIgnoreTag: () => string;
export const on: (cb: LogListener, options?: LogFilterOptions) => void;
export const off: (cb?: LogListener) => void;
export const fetchLogs: (clear?: boolean) => Array<LogEntry>;
export const clearLogs: () => void;
