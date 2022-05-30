#ifndef _VKSCACHEBUILDER_HPP
#define _VKSCACHEBUILDER_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *-------------------------------------------------------------------------*/

#include "vksCommon.hpp"
#include "vksStructsVKSC.hpp"

#include "vkPrograms.hpp"

namespace vksc_server
{

void							exportFilesForExternalCompiler	(const VulkanPipelineCacheInput&	input,
																 const std::string&					path,
																 const std::string&					filePrefix);
vector<u8>						buildOfflinePipelineCache		(const VulkanPipelineCacheInput&	input,
																const std::string&					pipelineCompilerPath,
																const std::string&					pipelineCompilerDataDir,
																const std::string&					pipelineCompilerArgs,
																const std::string&					pipelineCompilerOutputFile,
																const std::string&					pipelineCompilerLogFile,
																const std::string&					pipelineCompilerFilePrefix);
vector<u8>						buildPipelineCache				(const VulkanPipelineCacheInput&	input,
																 const vk::PlatformInterface&		vkp,
																 vk::VkInstance						instance,
																 const vk::InstanceInterface&		vki,
																 vk::VkPhysicalDevice				physicalDevice,
																 deUint32							queueIndex);
std::vector<VulkanPipelineSize>	extractSizesFromPipelineCache	(const VulkanPipelineCacheInput&	input,
																 const vector<u8>&					pipelineCache,
																 deUint32							pipelineDefaultSize,
																 bool								recyclePipelineMemory);

}

#endif // _VKSCACHEBUILDER_HPP
