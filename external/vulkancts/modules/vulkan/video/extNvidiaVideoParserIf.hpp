#ifndef _EXTNVIDIAVIDEOPARSERIF_HPP
#define _EXTNVIDIAVIDEOPARSERIF_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Interface glue to the NVIDIA Vulkan Video samples.
 *//*--------------------------------------------------------------------*/
/*
 * Copyright 2021 NVIDIA Corporation.
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vkDefs.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "VkCodecUtils/VulkanVideoReferenceCountedPool.h"
#include "VkVideoCore/VkVideoRefCountBase.h"
#include "vktBitstreamBufferImpl.hpp"
#include "vkvideo_parser/VulkanVideoParser.h"
#include "vkvideo_parser/VulkanVideoParserIf.h"
#include "vkvideo_parser/VulkanVideoParserParams.h"

#include <iostream>

namespace vkt
{
namespace video
{

// TODO: Eventually convert all the debug logging from the NVIDIA samples code to TCU printing functions.
#define DEBUGLOG(X)

} // video
} // vkt

#endif // _EXTNVIDIAVIDEOPARSERIF_HPP
