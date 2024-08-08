#ifndef _EGLUCALLLOGWRAPPER_HPP
#define _EGLUCALLLOGWRAPPER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Utilities
 * ------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief EGL call wrapper for logging.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"
#include "eglwDefs.hpp"

namespace eglw
{
class Library;
}

namespace eglu
{

class CallLogWrapper
{
public:
    CallLogWrapper(const eglw::Library &egl, tcu::TestLog &log);
    ~CallLogWrapper(void);

// EGL API is exposed as member functions
#include "egluCallLogWrapperApi.inl"

    void enableLogging(bool enable)
    {
        m_enableLog = enable;
    }

private:
    const eglw::Library &m_egl;
    tcu::TestLog &m_log;
    bool m_enableLog;
};

} // namespace eglu

#endif // _EGLUCALLLOGWRAPPER_HPP
