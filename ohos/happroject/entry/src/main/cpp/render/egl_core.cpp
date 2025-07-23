/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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


#include "plugin_common.h"
#include "plugin_render.h"
#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <GLES3/gl3.h>

EGLConfig getConfig(int version, EGLDisplay eglDisplay) {
    int attribList[] = {EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES2_BIT,
                        EGL_NONE};
    EGLConfig configs = NULL;
    int configsNum;
    if (!eglChooseConfig(eglDisplay, attribList, &configs, 1, &configsNum)) {
        LOGE("eglChooseConfig ERROR");
        return NULL;
    }
    return configs;
}

void EGLCore::SetXSize(int w, int h) {
    LOGD("EGLCore::SetXSize w = %{public}d, h = %{public}d.", w, h);
    width_ = w;
    height_ = h;
}
void EGLCore::GLContextInit(void *window, int w, int h) {
    static uint32_t createcount = 0;
    createcount++;
    CLOGD("EGLCore::GLContextInit %{public}d, w = %{public}d, h = %{public}d", createcount, w, h);
    width_ = w;
    height_ = h;

    mEglWindow = reinterpret_cast<EGLNativeWindowType>(window);

    // // 1. create sharedcontext
    // mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    // if (mEGLDisplay == EGL_NO_DISPLAY) {
    //     LOGE("EGLCore::unable to get EGL display.");
    //     return;
    // }
    // // CLOGD("EGLCore::mEGLDisplay = %{public}p", mEGLDisplay);

    // EGLint eglMajVers, eglMinVers;
    // if (!eglInitialize(mEGLDisplay, &eglMajVers, &eglMinVers)) {
    //     mEGLDisplay = EGL_NO_DISPLAY;
    //     LOGE("EGLCore::unable to initialize display");
    //     return;
    // }
    // // CLOGD("EGLCore::eglInitialize, version = %d.%d", eglMajVers, eglMinVers);

    // mEGLConfig = getConfig(3, mEGLDisplay);
    // if (mEGLConfig == nullptr) {
    //     LOGE("EGLCore::GLContextInit config ERROR");
    //     return;
    // }
    // // CLOGD("EGLCore::mEGLConfig = %{public}p", mEGLConfig);

    // // 2. Create EGL Surface from Native Window
    // EGLint winAttribs[] = {EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_LINEAR_KHR, EGL_NONE};
    // //     EGLint winAttribs[] = {EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE};
    // if (mEglWindow) {
    //     mEGLSurface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, mEglWindow, winAttribs);
    //     if (mEGLSurface == nullptr) {
    //         LOGE("EGLCore::eglCreateContext eglSurface is null");
    //         return;
    //     }
    // }
    // // CLOGD("EGLCore::mEGLSurface = %{public}p", mEGLSurface);

    // // 3. Create EGLContext from
    // int attrib3_list[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

    // mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, mSharedEGLContext, attrib3_list);
    // // CLOGD("EGLCore::mEGLContext = %{public}p", mEGLContext);
    // if (!eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
    //     LOGE("EGLCore::eglMakeCurrent error = %{public}d", eglGetError());
    // }
    // // CLOGD("EGLCore::eglMakeCurrent success");
    // appContext.eglSurface_ = mEGLSurface;
    // appContext.eglContext_ = mEGLContext;
    appContext.nativeWindow_ = mEglWindow;
    bInited = true;
}

std::string EGLCore::StartTest(const std::string &filesDir, const std::string &caseName) {
    CLOGE("do connect !!!");
    testDir = filesDir;
    testCase = caseName;

    std::string result = DoTest();
    
    return result;
}

void EGLCore::OnKeyEvent(uint32_t keyCode, uint32_t updown) {}
void EGLCore::OnTouch(int id, int x, int y, int type) {}

void EGLCore::OnWindowCommand(uint16_t command) {}
void *stdout_to_hilog(void *arg) { // 把vk-gl-cts的log打到hilog
    int *pipefd = (int *)arg;
    FILE *pipe_read = fdopen(pipefd[0], "r");
    if (!pipe_read) {
        perror("fdopen");
        return NULL;
    }

    char line[1024];
    while (true) {
        if (fgets(line, sizeof(line), pipe_read) != NULL) {
            CLOGE(" - %{public}s", line);
        } else {
            CLOGE("stdout_to_hilog exit");
            break;
        }
    }

    fclose(pipe_read);
    return nullptr;
}

void* test_thread(void *arg) { 
   EGLCore *pec = (EGLCore *)arg;
   pec->DoTest();
}

void EGLCore::UpdateScreen() {
    if (doTest) {
        doTest = false;
        DoTest();
    }
}
