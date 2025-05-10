#ifndef _XSTESTPROCESS_HPP
#define _XSTESTPROCESS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief Test Process Abstraction.
 *//*--------------------------------------------------------------------*/

#include "xsDefs.hpp"

#include <stdexcept>

namespace xs
{

class TestProcessException : public std::runtime_error
{
public:
    TestProcessException(const std::string &message) : std::runtime_error(message)
    {
    }
};

class TestProcess
{
public:
    virtual ~TestProcess(void)
    {
    }

    virtual void start(const char *name, const char *params, const char *workingDir, const char *caseList) = 0;
    virtual void terminate(void)                                                                           = 0;
    virtual void cleanup(void)                                                                             = 0;

    virtual bool isRunning(void)        = 0;
    virtual int getExitCode(void) const = 0;

    virtual int readTestLog(uint8_t *dst, int numBytes) = 0;
    virtual int readInfoLog(uint8_t *dst, int numBytes) = 0;

protected:
    TestProcess(void)
    {
    }
};

} // namespace xs

#endif // _XSTESTPROCESS_HPP
