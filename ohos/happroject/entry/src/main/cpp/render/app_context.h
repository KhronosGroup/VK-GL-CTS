#ifndef CRDP_APP_CONTEXT_H
#define CRDP_APP_CONTEXT_H
#include "ohos_context_i.h"
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace OHOS {
    class AppContext : public OHOS::OhosContextI {
    public:
        AppContext() {}
        bool InitEgl();
        void HiLog(const char *format, ...) override;
        bool SetConfig(int32_t w, int32_t h, RCI_GLES_VERSION ver, RCI_PIXEL_FORMAT pf, RCI_SURFACE_TYPE st,
                       RCI_PROFILE tp, RCI_CONTEXT_FLAG flags) override;
        bool InitNativeWindow() override;
        bool InitEglSurface() override;
        bool InitEglContext() override;

        void MakeCurrent() override;
        void SwapBuffer() override;

        int32_t GetAttrib(int32_t attrType) override;

        uint64_t CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        void *GetNativeWindow(uint64_t windowId) override;
        void DestoryWindow(uint64_t windowId) override;
    
        EGLDisplay eglDisplay_;
        EGLSurface eglSurface_ = EGL_NO_SURFACE;
        EGLContext eglContext_ = EGL_NO_CONTEXT;
        EGLNativeWindowType nativeWindow_ = 0;

    private:
        void ShowConfig(EGLConfig cfg);
        EGLConfig *allConfigs_;
        EGLint configCount_;
        EGLConfig config_;

        RCI_GLES_VERSION glesVersion_;
        RCI_PROFILE typeProfile_;
        RCI_CONTEXT_FLAG contextFlags_;
        RCI_SURFACE_TYPE surfaceType_;
        int32_t width_;
        int32_t height_;
        RCI_PIXEL_FORMAT pixelFormat_;
        bool eglInited_ = false;
    };
} // namespace OHOS

#endif // CRDP_APP_CONTEXT_H
