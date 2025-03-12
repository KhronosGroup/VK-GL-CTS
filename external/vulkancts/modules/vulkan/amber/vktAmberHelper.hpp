#ifndef _VKTAMBERHELPER_HPP
#define _VKTAMBERHELPER_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 *//*--------------------------------------------------------------------*/

#include "../../../../framework/delibs/debase/deDefs.h"
#include "amber/amber.h"

namespace vkt
{
namespace cts_amber
{

amber::EngineConfig *GetVulkanConfig(void *instance, void *physicalDevice, void *device, const void *features,
                                     const void *features2, const void *properties, const void *properties2,
                                     const std::vector<std::string> &instance_extensions,
                                     const std::vector<std::string> &device_extensions, uint32_t queueIdx, void *queue,
                                     void *getInstanceAddrProc);

} // namespace cts_amber
} // namespace vkt

#endif // _VKTAMBERHELPER_HPP
