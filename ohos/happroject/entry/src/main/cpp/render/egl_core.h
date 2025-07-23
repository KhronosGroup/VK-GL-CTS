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

#ifndef _GL_CORE_
#define _GL_CORE_

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "cc_common.h"
#include <string>
#include "app_context.h"
#include "ohos_context_i.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
void *stdout_to_hilog(void *arg);
void *test_thread(void *arg);
class EGLCore {
public:
    EGLCore(std::string &id) : id_(id) {
        OHOS::OhosContextI::SetInstance(&appContext);
        CLOGE("ccnto set app context instance finish");
        if (pipe(pipefd_stdout) == -1) {
            CLOGE("create pipe failed");
            return;
        } else {
            dup2(pipefd_stdout[1], STDOUT_FILENO);
            close(pipefd_stdout[1]);
            //                 fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        }
        if (pipe(pipefd_stderr) == -1) {
            CLOGE("create pipe failed");
            return;
        } else {
            dup2(pipefd_stderr[1], STDERR_FILENO);
            close(pipefd_stderr[1]);
            //                 fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        }
        pthread_t tid;
        pthread_create(&tid, NULL, stdout_to_hilog, pipefd_stdout);
        pthread_create(&tid, NULL, stdout_to_hilog, pipefd_stderr);
        CLOGE("pipefd_stdout ok");
    }
    std::string DoTest() {
        CLOGE("start do test");
        // std::string logFile = "--deqp-log-filename=/data/storage/el2/base/haps/entry/files/TestResults.qpa";
        std::string logFile = "--deqp-log-filename=" + testDir + "/TestResults.qpa";
        //         std::string glCase = "-n=KHR-GLES2.texture_3d.filtering.formats.rgba8_nearest_mipmap_linear";
        std::string glCase = "-n=" + testCase;
        std::string archiveDir = "--deqp-archive-dir=" + testDir;
        char *argv_[10];
        int p = 0;
        argv_[p++] = strdup("./glcts_app_mock");
        argv_[p++] = strdup(logFile.c_str());
        argv_[p++] = strdup(glCase.c_str());
        argv_[p++] = strdup(archiveDir.c_str());
        argv_[p] = NULL;

        TestRunStatus_t ret = main1(p, argv_);
        CLOGE("Test run totals:");
        CLOGE("  passed: %{public}d/%{public}d", ret.numPassed, ret.numExecuted);
        CLOGE("  failed: %{public}d/%{public}d", ret.numFailed, ret.numExecuted);
        CLOGE("  not support: %{public}d/%{public}d", ret.numNotSupported, ret.numExecuted);
        CLOGE("  warning: %{public}d/%{public}d", ret.numWarnings, ret.numExecuted);
        CLOGE("end do test");
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "Test run totals:\n"
                                         "  passed: %d/%d\n"
                                         "  failed: %d/%d\n"
                                         "  not support: %d/%d\n"
                                         "  warning: %d/%d\n"
                                         "end do test\n",
                                         ret.numPassed, ret.numExecuted, ret.numFailed, ret.numExecuted,
                                         ret.numNotSupported, ret.numExecuted, ret.numWarnings, ret.numExecuted);
        return std::string(buffer);
    }
    ~EGLCore() {
        CLOGE("!!!   EGLCore deinit");
        if (mEGLDisplay != EGL_NO_DISPLAY) {
            if (!eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
                CLOGE("EGLCore::eglMakeCurrent error = %{public}d", eglGetError());
                return;
            }
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (mEGLSurface != EGL_NO_SURFACE) {
                eglDestroySurface(mEGLDisplay, mEGLSurface);
                mEGLSurface = EGL_NO_SURFACE;
            }
            if (mEGLContext != EGL_NO_CONTEXT) {
                eglDestroyContext(mEGLDisplay, mEGLContext);
                mEGLContext = EGL_NO_CONTEXT;
            }
        }
    }
    void GLContextInit(void *window, int w, int h);
    std::string StartTest(const std::string &filesDir, const std::string &caseName);
    // void DrawTriangle();
    void UpdateScreen();
    void OnKeyEvent(uint32_t keyCode, uint32_t updown);
    void OnTouch(int id, int x, int y, int type);
    void OnWindowCommand(uint16_t command);
    void SetXSize(int w, int h);

    //     void FreshUpdateResult();

public:
    std::string id_;
    int width_;
    int height_;

private:
    EGLNativeWindowType mEglWindow;
    EGLDisplay mEGLDisplay = EGL_NO_DISPLAY;
    EGLConfig mEGLConfig = nullptr;
    EGLContext mEGLContext = EGL_NO_CONTEXT;
    EGLContext mSharedEGLContext = EGL_NO_CONTEXT;
    EGLSurface mEGLSurface = EGL_NO_SURFACE;

    bool bInited = false;

    OHOS::AppContext appContext;
    bool doTest = false;
    int pipefd_stdout[2];
    int pipefd_stderr[2];
    std::string testDir;
    std::string testCase;
};

#endif // _GL_CORE_
