import { Callback } from "@ohos.base";
import resourceManager from '@ohos.resourceManager';

export interface Callback<T> {
  (data: T): void;
}

export interface CRdpInterface {
  testNapiThreadsafefunc(resmgr: resourceManager.ResourceManager,
     filesDir:string, testCase: string, callback: Callback<string>): number;
  startTest(resmgr: resourceManager.ResourceManager,filesDir:string,testCase: string):void;
  registerCallback(callback:Callback<string>,obj:Object):void;
  updateScreen():void;
  keyEvent(wid:number,keycode:number,updown:number):void;
  windowCommand(wid:number,command:number):void;
}
