import { hilog } from '@kit.PerformanceAnalysisKit';
import { setCapture, on, off, fetchLogs, clearLogs, getIgnoreTag, LogEntry } from 'log-capture';

const DEMO_DOMAIN = 0xF100;
const DEMO_TAG = 'LogCaptureDemo';

function levelName(level: number): string {
  switch (level) {
    case 3: return 'D';
    case 4: return 'I';
    case 5: return 'W';
    case 6: return 'E';
    case 7: return 'F';
    default: return String(level);
  }
}

@Entry
@Component
struct Index {
  @State capturing: boolean = true;
  @State listening: boolean = false;
  @State logs: LogEntry[] = [];
  @State liveCount: number = 0;
  private seq: number = 0;

  // 保存引用，供 off 精确移除
  private listener = (entry: LogEntry) => {
    this.liveCount++;
    // 实时把新日志追加到列表（新数组触发刷新）
    this.logs = this.logs.concat(entry);
  };

  aboutToDisappear(): void {
    off(this.listener);
  }

  private emit(level: 'info' | 'warn' | 'error') {
    this.seq++;
    const msg = `demo ${level} #${this.seq} @${Date.now()}`;
    if (level === 'info') {
      hilog.info(DEMO_DOMAIN, DEMO_TAG, '%{public}s', msg);
    } else if (level === 'warn') {
      hilog.warn(DEMO_DOMAIN, DEMO_TAG, '%{public}s', msg);
    } else {
      hilog.error(DEMO_DOMAIN, DEMO_TAG, '%{public}s', msg);
    }
  }

  build() {
    Column({ space: 8 }) {
      Text('log-capture demo')
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
      Text(`ignoreTag: ${getIgnoreTag()}`)
        .fontSize(12)
        .fontColor('#888888')

      // 捕获开关 + 监听开关
      Row({ space: 10 }) {
        Text(`capture: ${this.capturing ? 'ON' : 'OFF'}`).fontSize(13)
        Toggle({ type: ToggleType.Switch, isOn: this.capturing })
          .onChange((on_) => {
            this.capturing = on_;
            setCapture(on_);
          })
        Text(`listen: ${this.listening ? 'ON' : 'OFF'} (${this.liveCount})`).fontSize(13)
        Toggle({ type: ToggleType.Switch, isOn: this.listening })
          .onChange((on_) => {
            this.listening = on_;
            if (on_) {
              // 只监听本 demo 的 domain，过滤系统噪声
              on(this.listener, { domain: DEMO_DOMAIN });
            } else {
              off(this.listener);
            }
          })
      }

      // 打日志按钮
      Row({ space: 8 }) {
        Button('info').onClick(() => this.emit('info'))
        Button('warn').onClick(() => this.emit('warn'))
        Button('error').onClick(() => this.emit('error'))
      }

      // 缓冲区操作
      Row({ space: 8 }) {
        Button('fetchLogs')
          .onClick(() => {
            // 拉取本 demo domain 的缓冲日志
            this.logs = fetchLogs().filter((e) => e.domain === DEMO_DOMAIN);
          })
        Button('clear')
          .onClick(() => {
            clearLogs();
            this.logs = [];
            this.liveCount = 0;
          })
      }

      Text(`shown: ${this.logs.length}`).fontSize(12).fontColor('#888888')

      List({ space: 2 }) {
        ForEach(this.logs, (item: LogEntry, index: number) => {
          ListItem() {
            Row({ space: 6 }) {
              Text(levelName(item.level))
                .fontSize(11)
                .fontColor(Color.White)
                .backgroundColor(item.level >= 6 ? '#CC0000' : (item.level === 5 ? '#E69138' : '#3366CC'))
                .borderRadius(3)
                .padding({ left: 5, right: 5, top: 1, bottom: 1 })
              Text(item.msg)
                .fontSize(12)
                .layoutWeight(1)
                .maxLines(2)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            .width('100%')
            .padding({ top: 3, bottom: 3 })
          }
        }, (item: LogEntry, index: number) => `${index}-${item.msg}`)
      }
      .layoutWeight(1)
      .width('100%')
      .border({ width: { top: 0.5 }, color: '#DDDDDD' })
    }
    .width('100%')
    .height('100%')
    .padding(12)
    .alignItems(HorizontalAlign.Start)
  }
}
