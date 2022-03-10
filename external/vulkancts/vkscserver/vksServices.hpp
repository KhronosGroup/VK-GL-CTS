#ifndef _VKSSERVICES_HPP
#define _VKSSERVICES_HPP

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

namespace vksc_server
{

bool StoreFile				(const string&						uniqueFilename,
							 const vector<u8>&					content);
bool GetFile				(const string&						path,
							 vector<u8>&						content,
							 bool								removeAfter);
bool AppendFile				(const string&						path,
							 const vector<u8>&					content,
							 bool								clear);
void CreateVulkanSCCache	(const VulkanPipelineCacheInput&	input,
							 int								caseFraction,
							 vector<u8>&						binary,
							 const CmdLineParams&				cmdLineParams,
							 const std::string&					logFile);
bool CompileShader			(const SourceVariant&				source,
							 const string&						commandLine,
							 vector<u8>&						binary	);

}

#endif // _VKSSERVICES_HPP
