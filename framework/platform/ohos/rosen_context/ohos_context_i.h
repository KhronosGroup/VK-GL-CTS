/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #ifndef _ROSEN_CONTEXT_H_
 #define _ROSEN_CONTEXT_H_
 
 #include <stdint.h>
 
 namespace OHOS {
 
     enum class RCI_GLES_VERSION { V20 = 20, V30 = 30, V31 = 31, V32 = 32 };
 
     enum class RCI_PROFILE { ES = 0, CORE, COMPATIBILITY };
 
     enum class RCI_CONTEXT_FLAG {
         NONE = 0,
         ROBUST =(1<<0) ,             //!< Robust context
         DEBUG  =(1<<1 ),              //!!< Debug context
         FORWARD_COMPATIBLE =(1<<2) , //!< Forward-compatible context
     };
 
     enum class RCI_SURFACE_TYPE { NONE = 0, WINDOW, PIXMAP, PBUFFER };
 
     struct RCI_PIXEL_FORMAT {
         int32_t redBits;
         int32_t greenBits;
         int32_t blueBits;
         int32_t alphaBits;
         int32_t depthBits;
         int32_t stencilBits;
         int32_t numSamples;
     };
 
     class OhosContextI {
     public:
         virtual void HiLog(const char *format, ...) = 0;
         static void SetInstance(void *instance);
         static OhosContextI &GetInstance();
 
         virtual bool SetConfig(int32_t w, int32_t h, RCI_GLES_VERSION ver, RCI_PIXEL_FORMAT pf, RCI_SURFACE_TYPE st,
                                RCI_PROFILE tp, RCI_CONTEXT_FLAG flags) = 0;
         virtual bool InitNativeWindow() = 0;
         virtual bool InitEglSurface() = 0;
         virtual bool InitEglContext() = 0;
 
         virtual void MakeCurrent() = 0;
         virtual void SwapBuffer() = 0;
 
         virtual int32_t GetAttrib(int32_t attrType) = 0;
 
         virtual uint64_t CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
         virtual void *GetNativeWindow(uint64_t windowId) = 0;
         virtual void DestoryWindow(uint64_t windowId) = 0;
 
     private:
     };
 
 } // namespace OHOS
 

 #endif