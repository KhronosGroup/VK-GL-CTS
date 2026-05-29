import { Callback } from "@ohos.base";
import resourceManager from '@ohos.resourceManager';

export interface Callback<T> {
  (data: T): void;
}

export interface CRdpInterface {
  testNapiThreadsafefunc(resmgr: resourceManager.ResourceManager,
     filesDir:string, testCase: string, callback: Callback<string>): number;
}
