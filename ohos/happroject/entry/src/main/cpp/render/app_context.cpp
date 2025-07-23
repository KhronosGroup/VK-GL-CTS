#include <stdio.h>

#include "app_context.h"
#include "cc_common.h"

namespace OHOS {
    void AppContext::HiLog(const char *format, ...) {
        char buffer[1024 * 10];
        va_list args;

//         va_start(args, format);
//         vsprintf(buffer, format, args);
//         va_end(args);
//        CLOGE("%{public}s", buffer);
    }
    bool AppContext::InitEgl() {
        if (eglInited_) {
            return true;
        }
        eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglDisplay_ == EGL_NO_DISPLAY) {
            printf("Failed to create EGLDisplay gl errno : %x", eglGetError());
            return false;
        }

        EGLint major, minor;
        if (eglInitialize(eglDisplay_, &major, &minor) == EGL_FALSE) {
            printf("Failed to initialize EGLDisplay");
            return false;
        }

        glDepthMask(GL_TRUE);

        eglGetConfigs(eglDisplay_, NULL, 0, &configCount_);
        allConfigs_ = new EGLConfig[configCount_];
        eglGetConfigs(eglDisplay_, allConfigs_, configCount_, &configCount_);

        printf("config count : %d\n", configCount_);
        for (int i = 0; i < configCount_; i++) {
            ShowConfig(allConfigs_[i]);
        }
        eglInited_ = true;
        return true;
    }
    void AppContext::ShowConfig(EGLConfig cfg) {
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

        HiLog("%8d%8d%8d%8d%8d%8d%8d%8d%8d\n", red, green, blue, alpha, depth, stencil, samples, sft, rt);
    }
    bool AppContext::SetConfig(int32_t w, int32_t h, RCI_GLES_VERSION ver, RCI_PIXEL_FORMAT pf, RCI_SURFACE_TYPE st,
                               RCI_PROFILE tp, RCI_CONTEXT_FLAG flags) {
        if (!eglInited_) {
            eglInited_ = InitEgl();
        }
        HiLog("w:%d,h:%d,ver:%d,pf.redBits:%d,st:%d,tp:%d,flags:%d\n", w, h, ver, pf.redBits, st, tp, flags);
        glesVersion_ = ver;
        typeProfile_ = tp;
        contextFlags_ = flags;
        surfaceType_ = st;
        width_ = w;
        height_ = h;
        pixelFormat_ = pf;

        EGLint eglApi;
        switch (typeProfile_) {
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
        if (eglBindAPI(eglApi) == EGL_FALSE) {
            HiLog("Failed to bind OpenGL ES API");
            return false;
        }

        std::vector<EGLint> frameBufferAttribs;
        frameBufferAttribs.push_back(EGL_SURFACE_TYPE);
        switch (surfaceType_) {
        case RCI_SURFACE_TYPE::NONE:
            HiLog("EGL_SURFACE_TYPE:EGL_NONE");
            frameBufferAttribs.push_back(EGL_DONT_CARE);
            break;
        case RCI_SURFACE_TYPE::PBUFFER:
            HiLog("EGL_SURFACE_TYPE:EGL_PBUFFER");
            frameBufferAttribs.push_back(EGL_PBUFFER_BIT);
            break;
        case RCI_SURFACE_TYPE::PIXMAP:
            HiLog("EGL_SURFACE_TYPE:EGL_PIXMAP");
            frameBufferAttribs.push_back(EGL_PIXMAP_BIT);
            break;
        case RCI_SURFACE_TYPE::WINDOW:
            HiLog("EGL_SURFACE_TYPE:EGL_WINDOW ok");
            frameBufferAttribs.push_back(EGL_WINDOW_BIT);
            break;
        default:
            HiLog("EGL_SURFACE_TYPE:unknown");
            frameBufferAttribs.push_back(EGL_WINDOW_BIT);
            break;
        }

        if (pixelFormat_.redBits != -1) {
            frameBufferAttribs.push_back(EGL_RED_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.redBits);
            HiLog("EGL_RED_SIZE:%d", pixelFormat_.redBits);
        }
        if (pixelFormat_.greenBits != -1) {
            frameBufferAttribs.push_back(EGL_GREEN_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.greenBits);
            HiLog("EGL_GREEN_SIZE:%d", pixelFormat_.greenBits);
        }
        if (pixelFormat_.blueBits != -1) {
            frameBufferAttribs.push_back(EGL_BLUE_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.blueBits);
            HiLog("EGL_BLUE_SIZE:%d", pixelFormat_.blueBits);
        }
        if (pixelFormat_.alphaBits != -1) {
            frameBufferAttribs.push_back(EGL_ALPHA_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.alphaBits);
            HiLog("EGL_ALPHA_SIZE:%d", pixelFormat_.alphaBits);
        }
        if (pixelFormat_.depthBits != -1) {
            frameBufferAttribs.push_back(EGL_DEPTH_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.depthBits);
            HiLog("EGL_DEPTH_SIZE:%d", pixelFormat_.depthBits);
        }
        if (pixelFormat_.stencilBits != -1) {
            frameBufferAttribs.push_back(EGL_STENCIL_SIZE);
            frameBufferAttribs.push_back(pixelFormat_.stencilBits);
            HiLog("EGL_STENCIL_SIZE:%d", pixelFormat_.stencilBits);
        }
        if (pixelFormat_.numSamples != -1) {
            frameBufferAttribs.push_back(EGL_SAMPLES);
            frameBufferAttribs.push_back(pixelFormat_.numSamples);
            HiLog("EGL_SAMPLES:%d", pixelFormat_.numSamples);
        }
        frameBufferAttribs.push_back(EGL_RENDERABLE_TYPE);
        switch (static_cast<int>(glesVersion_) / 10) {
        case 3:
            HiLog("GLES3.0");
            frameBufferAttribs.push_back(EGL_OPENGL_ES3_BIT);
            break;
        case 2:
            HiLog("GLES2.0 ok");
            frameBufferAttribs.push_back(EGL_OPENGL_ES2_BIT);
            break;
        default:
            HiLog("GLES1.0");
            frameBufferAttribs.push_back(EGL_OPENGL_ES_BIT);
        }
        frameBufferAttribs.push_back(EGL_NONE);

        unsigned int ret;
        EGLint count;
        ret = eglChooseConfig(eglDisplay_, &frameBufferAttribs[0], &config_, 1, &count);
        HiLog("ret=%d,count=%d\n", ret, count);
        if (!(ret && static_cast<unsigned int>(count) >= 1)) {
            HiLog("Failed to eglChooseConfig\n");
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
        if (pixelFormat_.redBits == -1) {
            pixelFormat_.redBits = red;
        } else if (pixelFormat_.redBits != red) {
            HiLog("Failed to eglChooseConfig redBits %d != %d\n", pixelFormat_.redBits, red);
            return false;
        }

        if (pixelFormat_.greenBits == -1) {
            pixelFormat_.greenBits = green;
        } else if (pixelFormat_.greenBits != green) {
            HiLog("Failed to eglChooseConfig redBits %d != %d\n", pixelFormat_.greenBits, green);
            return false;
        }

        if (pixelFormat_.blueBits != blue) {
            if (pixelFormat_.blueBits != -1)
                HiLog("Failed to eglChooseConfig blueBits %d != %d\n", pixelFormat_.blueBits, blue);
            pixelFormat_.blueBits = blue;
        }

        if (pixelFormat_.alphaBits != alpha) {
            if (pixelFormat_.alphaBits != -1)
                HiLog("Failed to eglChooseConfig alphaBits %d != %d\n", pixelFormat_.alphaBits, alpha);
            pixelFormat_.alphaBits = alpha;
        }

        if (pixelFormat_.depthBits != depth) {
            if (pixelFormat_.depthBits != -1)
                HiLog("Failed to eglChooseConfig depthBits %d != %d\n", pixelFormat_.depthBits, depth);
            pixelFormat_.depthBits = depth;
        }

        if (pixelFormat_.stencilBits != stencil) {
            if (pixelFormat_.stencilBits != -1)
                HiLog("Failed to eglChooseConfig stencilBits %d != %d\n", pixelFormat_.stencilBits, stencil);
            pixelFormat_.stencilBits = stencil;
        }

        if (pixelFormat_.numSamples != samples) {
            if (pixelFormat_.numSamples != -1)
                HiLog("Failed to eglChooseConfig numSamples %d != %d\n", pixelFormat_.numSamples, samples);
            pixelFormat_.numSamples = samples;
        }
        HiLog("config ok\n");
        return true;
    }
    bool AppContext::InitNativeWindow() {
        HiLog("InitNativeWindow");
        return true;
    }
    bool AppContext::InitEglSurface() {
        if (eglSurface_ != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay_, eglSurface_);
            eglSurface_ = EGL_NO_SURFACE;
        }

        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        std::vector<EGLint> surfaceAttribs;

        switch (surfaceType_) {
        case RCI_SURFACE_TYPE::NONE:
            break;
        case RCI_SURFACE_TYPE::WINDOW:
            // surfaceAttribs.push_back(EGL_GL_COLORSPACE_KHR);
            // TODO: EGL_GL_COLORSPACE_LINEAR_KHR, EGL_GL_COLORSPACE_SRGB_KHR
            // surfaceAttribs.push_back(EGL_GL_COLORSPACE_LINEAR_KHR);
            surfaceAttribs.push_back(EGL_NONE);

            eglSurface_ = eglCreateWindowSurface(eglDisplay_, config_, nativeWindow_, &surfaceAttribs[0]);
            if (eglSurface_ == EGL_NO_SURFACE) {
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
    bool AppContext::InitEglContext() {
        if (eglContext_ != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay_, eglContext_);
            eglContext_ = EGL_NO_CONTEXT;
        }

        std::vector<EGLint> contextAttribs;
        contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
        contextAttribs.push_back(static_cast<int>(glesVersion_) / 10);
        contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
        contextAttribs.push_back(static_cast<int>(glesVersion_) % 10);

        switch (typeProfile_) {
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
        if (eglContext_ == EGL_NO_CONTEXT) {
            printf("Failed to create egl context %x\n", eglGetError());
            return false;
        }
        printf("context ok\n");
        return true;
    }
    void AppContext::MakeCurrent() {
        HiLog("MakeCurrent");
        if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
            HiLog("eglMakeCurrent FAIL\n");
        }
    }
    void AppContext::SwapBuffer() {
        HiLog("SwapBuffer");
        eglSwapBuffers(eglDisplay_, eglSurface_);
    }
    int32_t AppContext::GetAttrib(int32_t attrType) {
        int32_t ret;
        eglGetConfigAttrib(eglDisplay_, config_, attrType, &ret);
        HiLog("attrType:%d,value:%d\n", attrType, ret);
        return ret;
    }
    uint64_t AppContext::CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        HiLog("x:%d,y:%d,width:%d,height:%d\n", x, y, width, height);
        return 0;
    }
    void *AppContext::GetNativeWindow(uint64_t windowId) {
        HiLog("windowId:%lu\n", windowId);
        return nullptr;
    }
    void AppContext::DestoryWindow(uint64_t windowId) { HiLog("windowId:%lu\n", windowId); }
} // namespace OHOS

// int main(int argc, char **argv) {
//     OHOS::AppContext appContext;
//     OHOS::OhosContextI::SetInstance((void *)&appContext);
//     main1(argc, argv);
//     return 0;
// }