import hilog from '@ohos.hilog';

export class LogUtil {
  public static readonly DOMAIN: number = 0xFF00;
  public static readonly TAG: string = "vkglcts";

  public static debug(message: string, ...args: Object[]) {
    hilog.debug(LogUtil.DOMAIN, LogUtil.TAG, message, args)
  }

  public static info(message: string, ...args: Object[]) {
    hilog.info(LogUtil.DOMAIN, LogUtil.TAG, message, args)
  }

  public static log(message: string, ...args: Object[]) {
    hilog.debug(LogUtil.DOMAIN, LogUtil.TAG, message, args)
  }

  public static warn(message: string, ...args: Object[]) {
    hilog.warn(LogUtil.DOMAIN, LogUtil.TAG, message, args)
  }

  public static error(message: string, ...args: Object[]) {
    hilog.error(LogUtil.DOMAIN, LogUtil.TAG, message, args)
  }
}