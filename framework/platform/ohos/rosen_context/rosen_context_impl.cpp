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

#include "rosen_context_impl.h"

#include "ui/rs_surface_extractor.h"
#include "backend/rs_surface_ohos_gl.h"

using namespace OHOS;
using namespace Rosen;

RosenContextImpl::RosenContextImpl()
{
    InitEgl();
    InitProducer();
}

void RosenContextImpl::HiLog(const char *format, ...) {
}

void RosenContextImpl::ShowConfig(EGLConfig cfg)
{
    EGLint red, green, blue, alpha, depth, stencil, samples, sft, rt;

    eglGetConfigAttrib(eglDisplay_, cfg, EGL_RED_SIZE, &red);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_GREEN_SIZE, &green);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_BLUE_SIZE, &blue);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_ALPHA_SIZE, &alpha);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_DEPTH_SIZE, &depth);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_STENCIL_SIZE, &stencil);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_SAMPLES, &samples);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_SURFACE_TYPE, &sft);
    eglGetConfigAttrib(eglDisplay_, cfg, EGL_RENDERABLE_TYPE, &rt);

    printf("%8d%8d%8d%8d%8d%8d%8d%8d%8d\n", red, green, blue, alpha, depth, stencil, samples, sft, rt);
}

void RosenContextImpl::InitProducer()
{
    displayNode_ = RSDisplayNode::Create(RSDisplayNodeConfig());
    surfaceNode_ = RSSurfaceNode::Create(RSSurfaceNodeConfig());
    surfaceNode_->SetBounds(0, 0, 512, 512);
    displayNode_->AddChild(surfaceNode_, -1);

    std::shared_ptr<RSSurface> rsSurface = RSSurfaceExtractor::ExtractRSSurface(surfaceNode_);
    std::shared_ptr<RSSurfaceOhosGl> rsSurfaceOhosGl = std::static_pointer_cast<RSSurfaceOhosGl>(rsSurface);
    producer_ = rsSurfaceOhosGl->GetSurface();
}

bool RosenContextImpl::InitEgl()
{
    if(eglInited_) {
        return true;
    }
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY)
    {
        printf("Failed to create EGLDisplay gl errno : %x", eglGetError());
        return false;
    }

    EGLint major, minor;
    if (eglInitialize(eglDisplay_, &major, &minor) == EGL_FALSE)
    {
        printf("Failed to initialize EGLDisplay");
        return false;
    }

    glDepthMask(GL_TRUE);

    eglGetConfigs(eglDisplay_, NULL, 0, &configCount_);
    allConfigs_ = new EGLConfig[configCount_];
    eglGetConfigs(eglDisplay_, allConfigs_, configCount_, &configCount_);

    printf("config count : %d\n", configCount_);
    for (int i = 0; i < configCount_; i++)
    {
        ShowConfig(allConfigs_[i]);
    }
    eglInited_ = true;
    return true;
}

bool RosenContextImpl::SetConfig(int32_t w, int32_t h, RCI_GLES_VERSION ver, RCI_PIXEL_FORMAT pf, RCI_SURFACE_TYPE st, RCI_PROFILE tp, RCI_CONTEXT_FLAG flags)
{
    glesVersion_ = ver;
    typeProfile_ = tp;
    contextFlags_ = flags;
    surfaceType_ = st;
    width_ = w;
    height_ = h;
    pixelFormat_ = pf;

    EGLint eglApi;
    switch (typeProfile_)
    {
    case RCI_PROFILE::ES:
        eglApi = EGL_OPENGL_ES_API;
        break;
    case RCI_PROFILE::CORE:
        eglApi = EGL_OPENGL_API;
        break;
    case RCI_PROFILE::COMPATIBILITY:
        eglApi = EGL_OPENGL_API;
        break;
    default:
        return false;
    }
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        printf("Failed to bind OpenGL ES API");
        return false;
    }

    std::vector<EGLint> frameBufferAttribs;
    frameBufferAttribs.push_back(EGL_RENDERABLE_TYPE);
    switch (static_cast<int>(glesVersion_) / 10)
    {
    case 3:
        frameBufferAttribs.push_back(EGL_OPENGL_ES3_BIT);
        break;
    case 2:
        frameBufferAttribs.push_back(EGL_OPENGL_ES2_BIT);
        break;
    default:
        frameBufferAttribs.push_back(EGL_OPENGL_ES_BIT);
    }

    frameBufferAttribs.push_back(EGL_SURFACE_TYPE);
    switch (surfaceType_)
    {
    case RCI_SURFACE_TYPE::NONE:
        frameBufferAttribs.push_back(EGL_DONT_CARE);
        break;
    case RCI_SURFACE_TYPE::PBUFFER:
        frameBufferAttribs.push_back(EGL_PBUFFER_BIT);
        break;
    case RCI_SURFACE_TYPE::PIXMAP:
        frameBufferAttribs.push_back(EGL_PIXMAP_BIT);
        break;
    case RCI_SURFACE_TYPE::WINDOW:
        frameBufferAttribs.push_back(EGL_WINDOW_BIT);
        break;
    }

    if (pixelFormat_.redBits != -1)
    {
        frameBufferAttribs.push_back(EGL_RED_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.redBits);
    }
    if (pixelFormat_.greenBits != -1)
    {
        frameBufferAttribs.push_back(EGL_GREEN_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.greenBits);
    }
    if (pixelFormat_.blueBits != -1)
    {
        frameBufferAttribs.push_back(EGL_BLUE_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.blueBits);
    }
    if (pixelFormat_.alphaBits != -1)
    {
        frameBufferAttribs.push_back(EGL_ALPHA_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.alphaBits);
    }
    if (pixelFormat_.depthBits != -1)
    {
        frameBufferAttribs.push_back(EGL_DEPTH_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.depthBits);
    }
    if (pixelFormat_.stencilBits != -1)
    {
        frameBufferAttribs.push_back(EGL_STENCIL_SIZE);
        frameBufferAttribs.push_back(pixelFormat_.stencilBits);
    }
    if (pixelFormat_.numSamples != -1)
    {
        frameBufferAttribs.push_back(EGL_SAMPLES);
        frameBufferAttribs.push_back(pixelFormat_.numSamples);
    }
    frameBufferAttribs.push_back(EGL_NONE);

    unsigned int ret;
    EGLint count;
    ret = eglChooseConfig(eglDisplay_, &frameBufferAttribs[0], &config_, 1, &count);
    printf("ret=%d,count=%d\n", ret, count);
    if (!(ret && static_cast<unsigned int>(count) >= 1))
    {
        printf("Failed to eglChooseConfig\n");
        return false;
    }
    EGLint red, green, blue, alpha, depth, stencil, samples;
    eglGetConfigAttrib(eglDisplay_, config_, EGL_RED_SIZE, &red);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_GREEN_SIZE, &green);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_BLUE_SIZE, &blue);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_ALPHA_SIZE, &alpha);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_DEPTH_SIZE, &depth);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_STENCIL_SIZE, &stencil);
    eglGetConfigAttrib(eglDisplay_, config_, EGL_SAMPLES, &samples);
    ShowConfig(config_);
    if (pixelFormat_.redBits == -1)
    {
        pixelFormat_.redBits = red;
    }
    else if (pixelFormat_.redBits != red)
    {
        printf("Failed to eglChooseConfig redBits %d != %d\n", pixelFormat_.redBits, red);
        return false;
    }

    if (pixelFormat_.greenBits == -1)
    {
        pixelFormat_.greenBits = green;
    }
    else if (pixelFormat_.greenBits != green)
    {
        printf("Failed to eglChooseConfig redBits %d != %d\n", pixelFormat_.greenBits, green);
        return false;
    }

    if (pixelFormat_.blueBits != blue)
    {
        if (pixelFormat_.blueBits != -1)
            printf("Failed to eglChooseConfig blueBits %d != %d\n", pixelFormat_.blueBits, blue);
        pixelFormat_.blueBits = blue;
    }

    if (pixelFormat_.alphaBits != alpha)
    {
        if (pixelFormat_.alphaBits != -1)
            printf("Failed to eglChooseConfig alphaBits %d != %d\n", pixelFormat_.alphaBits, alpha);
        pixelFormat_.alphaBits = alpha;
    }

    if (pixelFormat_.depthBits != depth)
    {
        if (pixelFormat_.depthBits != -1)
            printf("Failed to eglChooseConfig depthBits %d != %d\n", pixelFormat_.depthBits, depth);
        pixelFormat_.depthBits = depth;
    }

    if (pixelFormat_.stencilBits != stencil)
    {
        if (pixelFormat_.stencilBits != -1)
            printf("Failed to eglChooseConfig stencilBits %d != %d\n", pixelFormat_.stencilBits, stencil);
        pixelFormat_.stencilBits = stencil;
    }

    if (pixelFormat_.numSamples != samples)
    {
        if (pixelFormat_.numSamples != -1)
            printf("Failed to eglChooseConfig numSamples %d != %d\n", pixelFormat_.numSamples, samples);
        pixelFormat_.numSamples = samples;
    }
    printf("config ok\n");
    return true;
}

bool RosenContextImpl::InitNativeWindow()
{
    if (nativeWindow_ == nullptr)
    {
        nativeWindow_ = CreateNativeWindowFromSurface(&producer_);
    }
    NativeWindowHandleOpt(nativeWindow_, SET_BUFFER_GEOMETRY, width_, height_);
    if (pixelFormat_.stencilBits != -1)
    {
        NativeWindowHandleOpt(nativeWindow_, SET_STRIDE, pixelFormat_.stencilBits);
    }
    if (pixelFormat_.redBits == 8 && pixelFormat_.greenBits == 8 && pixelFormat_.blueBits == 8 && pixelFormat_.alphaBits == 8)
    {
        NativeWindowHandleOpt(nativeWindow_, SET_FORMAT, GRAPHIC_PIXEL_FMT_RGBA_8888);
    }
    else if (pixelFormat_.redBits == 5 && pixelFormat_.greenBits == 6 && pixelFormat_.blueBits == 5 && pixelFormat_.alphaBits == 0)
    {
        NativeWindowHandleOpt(nativeWindow_, SET_FORMAT, GRAPHIC_PIXEL_FMT_RGB_565);
    }
    else if (pixelFormat_.redBits == 4 && pixelFormat_.greenBits == 4 && pixelFormat_.blueBits == 4 && pixelFormat_.alphaBits == 4)
    {
        NativeWindowHandleOpt(nativeWindow_, SET_FORMAT, GRAPHIC_PIXEL_FMT_RGBA_4444);
    }
    printf("native window ok\n");
    return true;
}

bool RosenContextImpl::InitEglSurface()
{
    if(eglSurface_ != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay_, eglSurface_);
        eglSurface_ = EGL_NO_SURFACE;
    }
    
    eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    std::vector<EGLint> surfaceAttribs;

    switch (surfaceType_)
    {
    case RCI_SURFACE_TYPE::NONE:
        break;
    case RCI_SURFACE_TYPE::WINDOW:
        // surfaceAttribs.push_back(EGL_GL_COLORSPACE_KHR);
        //TODO: EGL_GL_COLORSPACE_LINEAR_KHR, EGL_GL_COLORSPACE_SRGB_KHR
        // surfaceAttribs.push_back(EGL_GL_COLORSPACE_LINEAR_KHR);
        surfaceAttribs.push_back(EGL_NONE);

        eglSurface_ = eglCreateWindowSurface(eglDisplay_, config_, nativeWindow_, &surfaceAttribs[0]);
        if (eglSurface_ == EGL_NO_SURFACE)
        {
            printf("Failed to create eglsurface!!! %x\n", eglGetError());
            return false;
        }
        break;
    case RCI_SURFACE_TYPE::PBUFFER:
    case RCI_SURFACE_TYPE::PIXMAP:
        surfaceAttribs.push_back(EGL_WIDTH);
        surfaceAttribs.push_back(width_);
        surfaceAttribs.push_back(EGL_HEIGHT);
        surfaceAttribs.push_back(height_);
        surfaceAttribs.push_back(EGL_NONE);
        break;
    }
    printf("egl surface ok\n");
    return true;
}

bool RosenContextImpl::InitEglContext()
{
    if(eglContext_ != EGL_NO_CONTEXT) {
        eglDestroyContext(eglDisplay_, eglContext_);
        eglContext_ = EGL_NO_CONTEXT;
    }
    
    std::vector<EGLint> contextAttribs;
    contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
    contextAttribs.push_back(static_cast<int>(glesVersion_) / 10);
    contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
    contextAttribs.push_back(static_cast<int>(glesVersion_) % 10);

    switch (typeProfile_)
    {
    case RCI_PROFILE::ES:
        break;
    case RCI_PROFILE::CORE:
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
        break;
    case RCI_PROFILE::COMPATIBILITY:
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        contextAttribs.push_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
        break;
    }

    EGLint flags = 0;
    if ((static_cast<int>(contextFlags_) & static_cast<int>(RCI_CONTEXT_FLAG::DEBUG)) != 0)
        flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

    if ((static_cast<int>(contextFlags_) & static_cast<int>(RCI_CONTEXT_FLAG::ROBUST)) != 0)
        flags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;

    if ((static_cast<int>(contextFlags_) & static_cast<int>(RCI_CONTEXT_FLAG::FORWARD_COMPATIBLE)) != 0)
        flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

    contextAttribs.push_back(EGL_CONTEXT_FLAGS_KHR);
    contextAttribs.push_back(flags);

    contextAttribs.push_back(EGL_NONE);

    eglContext_ = eglCreateContext(eglDisplay_, config_, EGL_NO_CONTEXT, &contextAttribs[0]);
    if (eglContext_ == EGL_NO_CONTEXT)
    {
        printf("Failed to create egl context %x\n", eglGetError());
        return false;
    }
    printf("context ok\n");
    return true;
}

void RosenContextImpl::MakeCurrent()
{
    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_))
    {
        printf("eglMakeCurrent FAIL\n");
    }
}

void RosenContextImpl::SwapBuffer()
{
    eglSwapBuffers(eglDisplay_, eglSurface_);
    RSTransactionProxy::GetInstance()->FlushImplicitTransaction();
}

int32_t RosenContextImpl::GetAttrib(int32_t attrType)
{
    int32_t ret;
    eglGetConfigAttrib(eglDisplay_, config_, attrType, &ret);
    return ret;
}

uint64_t RosenContextImpl::CreateWindow(uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{
    static uint64_t windowId = 1;
    uint64_t wid = windowId++;

    // printf("impl create window %llu\n",wid);
    if(displayNode_==nullptr){
        displayNode_ = RSDisplayNode::Create(RSDisplayNodeConfig());
    }
    vulkanWindows_[wid].surfaceNode_ = RSSurfaceNode::Create(RSSurfaceNodeConfig());
    vulkanWindows_[wid].surfaceNode_->SetBounds(x,y,width,height);
    displayNode_->AddChild(vulkanWindows_[wid].surfaceNode_, -1);

    std::shared_ptr<RSSurface> rsSurface = RSSurfaceExtractor::ExtractRSSurface(vulkanWindows_[wid].surfaceNode_);
    std::shared_ptr<RSSurfaceOhosGl> rsSurfaceOhosGl = std::static_pointer_cast<RSSurfaceOhosGl>(rsSurface);
    vulkanWindows_[wid].producer_ = rsSurfaceOhosGl->GetSurface();

    vulkanWindows_[wid].nativeWindow_ = CreateNativeWindowFromSurface(&vulkanWindows_[wid].producer_);
    return wid;
}

void *RosenContextImpl::GetNativeWindow(uint64_t windowId)
{
    // printf("impl get native window %llu\n",windowId);
    return vulkanWindows_[windowId].nativeWindow_;
}

void RosenContextImpl::DestoryWindow(uint64_t windowId)
{
    // printf("impl destory window %llu\n",windowId);
    displayNode_->RemoveChild(vulkanWindows_[windowId].surfaceNode_);
    vulkanWindows_.erase(windowId);
}