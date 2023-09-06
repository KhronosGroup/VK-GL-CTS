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

#include "vksServices.hpp"
#include "vksJson.hpp"
#include "vksCacheBuilder.hpp"
#include "vksStore.hpp"

#include <map>
#include <mutex>
#include <fstream>
#include <iostream>

#include "deUniquePtr.hpp"
#include "vkPrograms.hpp"
#include "vkPlatform.hpp"
#include "vkDeviceUtil.hpp"
#include "vktTestCase.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestContext.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"

tcu::Platform* createPlatform (void);

struct VkscServer
{
	const vk::PlatformInterface&					vkp;
	vk::VkInstance									instance;
	const vk::InstanceInterface&					vki;
	vk::VkPhysicalDevice							physicalDevice;
	deUint32										queueIndex;
	const vk::VkPhysicalDeviceFeatures2&			enabledFeatures;
};

VkscServer* createServerVKSC(const std::string& logFile);
std::unique_ptr<VkscServer> vkscServer;

namespace vksc_server
{

using namespace json;

Store ServiceStore;

bool LoadPhysicalFile (const string& path, vector<u8>& content)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;

	content.assign((std::istreambuf_iterator<char>(file)),
					std::istreambuf_iterator<char>());

	return true;
}

bool StoreFile (const string& uniqueFilename, const vector<u8>& content)
{
	return ServiceStore.Set(uniqueFilename, content);
}

bool GetFile (const string& path, vector<u8>& content, bool removeAfter)
{
	return ServiceStore.Get(path, content, removeAfter) || LoadPhysicalFile(path, content);
}

bool AppendFile (const string& path, const vector<u8>& content, bool clear)
{
	auto mode = clear ? std::ios::binary : std::ios::binary | std::ios::app;

	std::ofstream file(path, mode);
	if (!file) return false;

	std::copy(content.begin(), content.end(), std::ostream_iterator<u8>{file});

	return true;
}

void CreateVulkanSCCache (const VulkanPipelineCacheInput& input, int caseFraction, vector<u8>& binary, const CmdLineParams& cmdLineParams, const std::string& logFile)
{
	if (!cmdLineParams.compilerPath.empty())
	{
		std::stringstream prefix;
		if (caseFraction >= 0)
			prefix << "sub_" << caseFraction << "_";
		else
			prefix << "";

		binary = vksc_server::buildOfflinePipelineCache(input,
														cmdLineParams.compilerPath,
														cmdLineParams.compilerDataDir,
														cmdLineParams.compilerArgs,
														cmdLineParams.compilerPipelineCacheFile,
														cmdLineParams.compilerLogFile,
														prefix.str());
	}
	else
	{
		if (vkscServer.get() == DE_NULL)
			vkscServer.reset(createServerVKSC(logFile));

		binary = buildPipelineCache(input,
									vkscServer->vkp,
									vkscServer->instance,
									vkscServer->vki,
									vkscServer->physicalDevice,
									vkscServer->queueIndex);
	}
}

bool CompileShader (const SourceVariant& source, const string& commandLine, vector<u8>& binary)
{
	glu::ShaderProgramInfo programInfo;
	vk::SpirVProgramInfo programInfoSpirv;
	tcu::CommandLine cmd(commandLine);

	std::unique_ptr<vk::ProgramBinary> programBinary;

	if (source.active == "glsl") programBinary.reset( vk::buildProgram(source.glsl, &programInfo, cmd) );
	else if (source.active == "hlsl") programBinary.reset( vk::buildProgram(source.hlsl, &programInfo, cmd) );
	else if (source.active == "spirv") programBinary.reset( vk::assembleProgram(source.spirv, &programInfoSpirv, cmd) );
	else return false;

	if (!programBinary || programBinary->getBinary() == nullptr)
	{
		return false;
	}

	if (programBinary->getFormat() != vk::PROGRAM_FORMAT_SPIRV)
	{
		throw std::runtime_error("CompileShader supports only PROGRAM_FORMAT_SPIRV binary output");
	}

	binary.assign(	programBinary->getBinary(),
					programBinary->getBinary() + programBinary->getSize()	);

	return true;
}

} // vksc_server

VkscServer* createServerVKSC(const std::string& logFile)
{
	tcu::CommandLine				cmdLine		{"--deqp-vk-device-id=0"};
	tcu::DirArchive					archive		{""};
	tcu::TestLog					log			{ logFile.c_str() }; log.supressLogging(true);
	tcu::Platform*					platform	{createPlatform()};
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
	vk::Library*					library		{platform->getVulkanPlatform().createLibrary(vk::Platform::LIBRARY_TYPE_VULKAN, DE_NULL)};
#else
	vk::Library*					library		{platform->getVulkanPlatform().createLibrary(DE_NULL)};
#endif
	tcu::TestContext*				tcx			= new tcu::TestContext{*platform, archive, log, cmdLine, nullptr};
	vk::ResourceInterface*			resource	= new vk::ResourceInterfaceStandard{*tcx};
	vk::BinaryCollection*			collection  = new vk::BinaryCollection{};
	vkt::Context*					context		= new vkt::Context(*tcx, library->getPlatformInterface(), *collection, de::SharedPtr<vk::ResourceInterface>{resource});

	VkscServer* result = new VkscServer
	{
		library->getPlatformInterface(),
		context->getInstance(),
		context->getInstanceInterface(),
		context->getPhysicalDevice(),
		context->getUniversalQueueFamilyIndex(),
		context->getDeviceFeatures2()
	};

	return result;
}
