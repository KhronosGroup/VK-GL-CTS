import { CRdpInterface } from 'libentry.so'
import { LogUtil } from './LogUtil'
import EntryAbility from '../entryability/EntryAbility';

@Extend(Text)
function style(TextAlign: TextAlign) {
  .textAlign(TextAlign)
  .fontSize(12)
  .border({ width: 1 })
  .padding(10)
  .width('100%')
}

@Entry
@Component
struct MainWindow {
  private context:CRdpInterface|undefined=undefined;

  @State text: string = "KHR-GLES2.*"
  @State index: number = 0
  @State space: number = 8
  @State bEnable: boolean = true;
  @State msgText: string = 'Test ';

  aboutToAppear(): void {
  }

  build() {
    Flex({ direction: FlexDirection.Column, alignItems: ItemAlign.Center, justifyContent: FlexAlign.Start }) {
      Text("OpenHarmony VK-GL-CTS Runner")
        .fontSize(16)
        .fontWeight(500)
        .padding(20)

      XComponent({ id: 'mainWindow', type: 'texture', libraryname: 'entry' })
        .width(512)
        .height(512)
        .padding(20)
        .onLoad((context) => {
          this.context = context as CRdpInterface;
          if (this.context) {
            LogUtil.debug("XComponent loaded");
          }
        })
        .onDestroy(() => {
        })
        .focusable(true)
        .backgroundColor(Color.Transparent)

      Select([{ value: this.text },
        { value: 'KHR-GLES3.*' },
        { value: 'KHR-GLES31.*' },
        { value: 'KHR-GLES32.*' },
        { value: 'KHR-GLESEXT.*' },
        { value: 'KHR-NoContext.*' },
        { value: 'dEQP-GLES2.*'},
        { value: 'dEQP-GLES3.*'},
        { value: 'dEQP-GLES31.*'}])
        .width('80%')
        .selected(this.index)
        .value(this.text)
        .font({ size: 16, weight: 500 })
        .fontColor('#182431')
        .selectedOptionFont({ size: 16, weight: 400 })
        .optionFont({ size: 16, weight: 400 })
        .space(this.space)
        .onSelect((index:number, text?: string | undefined)=>{
          LogUtil.debug('Select Option: ' + index)
          this.index = index;
          if(text){
            this.text = text;
          }
        })

      Button("start runner")
        .enabled(this.bEnable)
        .height(50)
        .width('80%')
        .fontSize(16)
        .fontWeight(500)
        .onClick(()=>{
          if (this.context) {
            this.bEnable = false;
            this.msgText = 'Test ' + this.text + " running: \n";
            let startTime = new Date();
            this.context.testNapiThreadsafefunc(EntryAbility.resmgr, EntryAbility.filesDir, this.text,
              (data:string) => {
                let endTime = new Date();
                const diffMs = Math.abs(endTime.getTime() - startTime.getTime());
                
                const days = Math.floor(diffMs / (1000 * 60 * 60 * 24));
                const hours = Math.floor(diffMs / (1000 * 60 * 60));
                const minutes = Math.floor(diffMs / (1000 * 60));
                const seconds = Math.floor(diffMs / 1000);
                const diffHours = hours - days * 24;
                const diffMinutes = minutes - hours * 60;
                const diffSeconds = seconds - minutes * 60;
                
                LogUtil.debug("execute runner hours: D-" + days + " H-" + hours + 
                  " M-" + minutes + " S-" + seconds + data);

                LogUtil.debug("execute runner diffHours: D-" + days + " H-" + diffHours + 
                  " M-" + diffMinutes + " S-" + diffSeconds + data);

                this.msgText += "starttime: " + startTime.toLocaleString() + "\n" +
                 "endtime: " + endTime.toLocaleString() + "\n" +
                 "cost： " + days + "D-" + diffHours + 
                  "H-" + diffMinutes + "M-" + diffSeconds + "S\n";
                this.msgText += data;
                this.bEnable = true;
            });
          }
        })

      Text(this.msgText)
        .style(TextAlign.Start)
        .margin(20)
        .fontSize(16)
        .fontWeight(500)
    }
    .backgroundColor(Color.Transparent)
    .width('100%')
    .height('100%')
    .focusable(true)
  }
}