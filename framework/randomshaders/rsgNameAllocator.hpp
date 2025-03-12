#ifndef _RSGNAMEALLOCATOR_HPP
#define _RSGNAMEALLOCATOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Name Allocator.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"

#include <string>

namespace rsg
{

// \todo [2011-02-03 pyry] Name allocation should be done as a post-pass to better control symbol name
//                           randomization, re-use etc...
class NameAllocator
{
public:
    NameAllocator(void);
    ~NameAllocator(void);

    std::string allocate(void);

private:
    uint32_t m_nextName;
};

} // namespace rsg

#endif // _RSGNAMEALLOCATOR_HPP
