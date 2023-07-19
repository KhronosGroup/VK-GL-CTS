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

#include <iostream>
#include <fstream>
#include <sstream>
#include <json/json.h>
#include "deCommandLine.hpp"
#include "deDirectoryIterator.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestContext.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "vkPlatform.hpp"
#include "vktTestCase.hpp"
#include "vksStructsVKSC.hpp"
#include "vksCacheBuilder.hpp"

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(CompilerDataPath,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(CompilerOutputFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(LogFile,			std::string);
DE_DECLARE_COMMAND_LINE_OPT(FilePrefix,			std::string);

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	parser << Option<CompilerDataPath>		("p", "path",		"Offline pipeline data directory",		"");
	parser << Option<CompilerOutputFile>	("o", "out",		"Output file with pipeline cache",		"");
	parser << Option<LogFile>				("l", "log",		"Log file",								"dummy.log");
	parser << Option<FilePrefix>			("x", "prefix",		"Prefix for input files",				"");
}

}

enum PipelineType
{
	PT_UNDEFINED_PIPELINE = 0,
	PT_GRAPHICS_PIPELINE,
	PT_COMPUTE_PIPELINE,
};

void importFilesForExternalCompiler (vksc_server::VulkanPipelineCacheInput&	input,
									 const std::string&						path,
									 const std::string&						filePrefix)
{
	vksc_server::json::Context context;

	for (de::DirectoryIterator iter(path); iter.hasItem(); iter.next())
	{
		const de::FilePath							filePath					= iter.getItem();
		if (filePath.getType() != de::FilePath::TYPE_FILE)
			continue;
		if (filePath.getFileExtension() != "json")
			continue;
		if (!filePrefix.empty() && filePath.getBaseName().find(filePrefix) != 0)
			continue;

		std::string									fileContents;
		{
			std::ifstream file(filePath.getPath());
			std::stringstream buffer;
			buffer << file.rdbuf();
			fileContents = buffer.str();
		}

		Json::Value									jsonRoot;
		std::string									errors;
		bool										parsingSuccessful			= context.reader->parse(fileContents.c_str(), fileContents.c_str() + fileContents.size(), &jsonRoot, &errors);
		if (!parsingSuccessful)
			TCU_THROW(InternalError, (std::string("JSON parsing error. File ") + filePath.getPath() + " Error : " + errors).c_str());

		// decide what pipeline type will be created later
		PipelineType pipelineType = PT_UNDEFINED_PIPELINE;
		if (jsonRoot.isMember("GraphicsPipelineState"))
			pipelineType = PT_GRAPHICS_PIPELINE;
		else if (jsonRoot.isMember("ComputePipelineState"))
			pipelineType = PT_COMPUTE_PIPELINE;
		if(pipelineType == PT_UNDEFINED_PIPELINE)
			TCU_THROW(InternalError, (std::string("JSON - unknown pipeline. File ") + filePath.getPath()).c_str());

		const Json::Value&							jsonGraphicsPipelineState	= jsonRoot["GraphicsPipelineState"];
		const Json::Value&							jsonComputePipelineState	= jsonRoot["ComputePipelineState"];
		const Json::Value&							jsonPipelineState			= (pipelineType == PT_GRAPHICS_PIPELINE ) ? jsonGraphicsPipelineState : jsonComputePipelineState;
		vksc_server::VulkanJsonPipelineDescription	pipelineDescription;

		{
			const Json::Value&	jsonSamplerYcbcrConversions		= jsonPipelineState["YcbcrSamplers"];
			if (!jsonSamplerYcbcrConversions.isNull())
			{
				for (Json::ArrayIndex i = 0; i < jsonSamplerYcbcrConversions.size(); ++i)
				{
					const Json::Value::Members	membersNames	= jsonSamplerYcbcrConversions[i].getMemberNames();
					const Json::Value&			value			= jsonSamplerYcbcrConversions[i][membersNames[0]];
					deUint64					index;
					std::istringstream(membersNames[0]) >> index;
					input.samplerYcbcrConversions[vk::VkSamplerYcbcrConversion(index)] =  std::string(fileContents.begin() + value.getOffsetStart(), fileContents.begin() + value.getOffsetLimit());
				}
			}

			const Json::Value&	jsonSamplers					= jsonPipelineState["ImmutableSamplers"];
			if (!jsonSamplers.isNull())
			{
				for (Json::ArrayIndex i = 0; i < jsonSamplers.size(); ++i)
				{
					const Json::Value::Members	membersNames	= jsonSamplers[i].getMemberNames();
					const Json::Value&			value			= jsonSamplers[i][membersNames[0]];
					deUint64					index;
					std::istringstream(membersNames[0]) >> index;
					input.samplers[vk::VkSampler(index)] = std::string(fileContents.begin() + value.getOffsetStart(), fileContents.begin() + value.getOffsetLimit());
				}
			}

			const Json::Value&	jsonDescriptorSetLayouts		= jsonPipelineState["DescriptorSetLayouts"];
			if (!jsonDescriptorSetLayouts.isNull())
			{
				for (Json::ArrayIndex i = 0; i < jsonDescriptorSetLayouts.size(); ++i)
				{
					const Json::Value::Members	membersNames	= jsonDescriptorSetLayouts[i].getMemberNames();
					const Json::Value&			value			= jsonDescriptorSetLayouts[i][membersNames[0]];
					deUint64					index;
					std::istringstream(membersNames[0]) >> index;
					input.descriptorSetLayouts[vk::VkDescriptorSetLayout(index)] = std::string(fileContents.begin() + value.getOffsetStart(), fileContents.begin() + value.getOffsetLimit());
				}
			}

			deUint64						pipelineLayoutHandle	= 0u;
			deUint64						renderPassHandle		= 0u;
			std::map<std::string, deUint64>	stages;

			const Json::Value&	jsonComputePipeline				= jsonPipelineState["ComputePipeline"];
			if (!jsonComputePipeline.isNull())
			{
				pipelineDescription.pipelineContents			= std::string(fileContents.begin() + jsonComputePipeline.getOffsetStart(), fileContents.begin() + jsonComputePipeline.getOffsetLimit());
				pipelineLayoutHandle							= jsonComputePipeline["layout"].asUInt64();

				const Json::Value&	jsonStage					= jsonComputePipeline["stage"];
				stages[jsonStage["stage"].asString()]			= jsonStage["module"].asUInt64();
			}

			const Json::Value&	jsonGraphicsPipeline			= jsonPipelineState["GraphicsPipeline"];
			if (!jsonGraphicsPipeline.isNull())
			{
				pipelineDescription.pipelineContents			= std::string(fileContents.begin() + jsonGraphicsPipeline.getOffsetStart(), fileContents.begin() + jsonGraphicsPipeline.getOffsetLimit());
				pipelineLayoutHandle							= jsonGraphicsPipeline["layout"].asUInt64();
				renderPassHandle								= jsonGraphicsPipeline["renderPass"].asUInt64();

				const Json::Value&	jsonStages = jsonGraphicsPipeline["pStages"];
				for (Json::ArrayIndex i = 0; i < jsonStages.size(); ++i)
					stages[jsonStages[i]["stage"].asString()] = jsonStages[i]["module"].asUInt64();
			}

			const Json::Value&	jsonPipelineLayout				= jsonPipelineState["PipelineLayout"];
			if (!jsonPipelineLayout.isNull() && pipelineLayoutHandle != 0u)
			{
				input.pipelineLayouts[vk::VkPipelineLayout(pipelineLayoutHandle)] = std::string(fileContents.begin() + jsonPipelineLayout.getOffsetStart(), fileContents.begin() + jsonPipelineLayout.getOffsetLimit());
			}

			const Json::Value&	jsonRenderPass					= jsonPipelineState["Renderpass"];
			if (!jsonRenderPass.isNull() && renderPassHandle != 0u)
			{
				input.renderPasses[vk::VkRenderPass(renderPassHandle)] = std::string(fileContents.begin() + jsonRenderPass.getOffsetStart(), fileContents.begin() + jsonRenderPass.getOffsetLimit());
			}

			const Json::Value&	jsonRenderPass2					= jsonPipelineState["Renderpass2"];
			if (!jsonRenderPass2.isNull() && renderPassHandle != 0u)
			{
				input.renderPasses[vk::VkRenderPass(renderPassHandle)] = std::string(fileContents.begin() + jsonRenderPass.getOffsetStart(), fileContents.begin() + jsonRenderPass.getOffsetLimit());
			}

			const Json::Value&	jsonShaderFileNames				= jsonPipelineState["ShaderFileNames"];
			if (!jsonShaderFileNames.isNull())
			{
				for (Json::ArrayIndex i = 0; i < jsonShaderFileNames.size(); ++i)
				{
					std::string				stageName	= jsonShaderFileNames[i]["stage"].asString();
					std::string				fileName	= jsonShaderFileNames[i]["filename"].asString();
					auto it = stages.find(stageName);
					if(it == end(stages))
						TCU_THROW(InternalError, (std::string("JSON - missing shader stage. File ") + filePath.getPath()).c_str());

					de::FilePath			shaderPath	(path);
					shaderPath.join(de::FilePath(fileName));
					std::ifstream			iFile		(shaderPath.getPath(), std::ios::in | std::ios::binary);
					if(!iFile)
						TCU_THROW(InternalError, (std::string("JSON - missing shader file ") + fileName + ". File " + filePath.getPath()).c_str());

					auto					fileBegin	= iFile.tellg();
					iFile.seekg(0, std::ios::end);
					auto					fileEnd		= iFile.tellg();
					iFile.seekg(0, std::ios::beg);
					std::size_t				fileSize	= static_cast<std::size_t>(fileEnd - fileBegin);
					std::vector<deUint8>	shaderData	(fileSize);

					iFile.read(reinterpret_cast<char*>(shaderData.data()), fileSize);
					if (iFile.fail())
						TCU_THROW(InternalError, (std::string("JSON - error reading shader file ") + fileName + ". File " + filePath.getPath()).c_str());

					vk::VkShaderModuleCreateInfo smCI
					{
						VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,		// VkStructureType				sType;
						DE_NULL,											// const void*					pNext;
						vk::VkShaderModuleCreateFlags(0u),					// VkShaderModuleCreateFlags	flags;
						fileSize,											// deUintptr					codeSize;
						reinterpret_cast<deUint32*>(shaderData.data())		// const deUint32*				pCode;
					};

					input.shaderModules[vk::VkShaderModule(it->second)] = vksc_server::json::writeJSON_VkShaderModuleCreateInfo(smCI);
				}
			}

			const Json::Value&	jsonPhysicalDeviceFeatures		= jsonPipelineState["PhysicalDeviceFeatures"];
			if (!jsonPhysicalDeviceFeatures.isNull())
			{
				pipelineDescription.deviceFeatures				= std::string(fileContents.begin() + jsonPhysicalDeviceFeatures.getOffsetStart(), fileContents.begin() + jsonPhysicalDeviceFeatures.getOffsetLimit());
			}
		}

		const Json::Value&	jsonEnabledExtensions				= jsonRoot["EnabledExtensions"];
		if (!jsonEnabledExtensions.isNull())
		{
			for (Json::ArrayIndex i = 0; i < jsonEnabledExtensions.size(); ++i)
				pipelineDescription.deviceExtensions.push_back(jsonEnabledExtensions[i].asString());
		}

		const Json::Value&	jsonPipelineUUID					= jsonRoot["PipelineUUID"];
		if (!jsonPipelineUUID.isNull())
		{
			pipelineDescription.id.sType			= VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO;
			pipelineDescription.id.pNext			= DE_NULL;
			for (Json::ArrayIndex i = 0; i < jsonPipelineUUID.size(); ++i)
				pipelineDescription.id.pipelineIdentifier[i] = deUint8(jsonPipelineUUID[i].asUInt());
			pipelineDescription.id.matchControl		= VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH;
			pipelineDescription.id.poolEntrySize	= 0u;
		}
		input.pipelines.push_back(pipelineDescription);
	}
}

tcu::Platform* createPlatform(void);

int main (int argc, char** argv)
{
	de::cmdline::CommandLine	cmdLine;

	// Parse command line.
	{
		de::cmdline::Parser	parser;
		opt::registerOptions(parser);

		if (!parser.parse(argc, argv, &cmdLine, std::cerr))
		{
			parser.help(std::cout);
			return EXIT_FAILURE;
		}
	}

	try
	{
		// load JSON files into VulkanPipelineCacheInput
		vksc_server::VulkanPipelineCacheInput			input;
		importFilesForExternalCompiler(input, cmdLine.getOption<opt::CompilerDataPath>(), cmdLine.getOption<opt::FilePrefix>());

		// create Vulkan instance
		tcu::CommandLine				cmdLineDummy	{"--deqp-vk-device-id=0"};
		tcu::DirArchive					archive			{""};
		tcu::TestLog					log				{ cmdLine.getOption<opt::LogFile>().c_str() }; log.supressLogging(true);
		de::SharedPtr<tcu::Platform>	platform		{createPlatform()};
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
		de::SharedPtr<vk::Library>		library			{platform->getVulkanPlatform().createLibrary(vk::Platform::LIBRARY_TYPE_VULKAN, DE_NULL)};
#else
		de::SharedPtr<vk::Library>		library			{platform->getVulkanPlatform().createLibrary(DE_NULL)};
#endif
		tcu::TestContext				tcx				{*platform, archive, log, cmdLineDummy, nullptr};
		vk::BinaryCollection			collection		{};
		vkt::Context					context			(tcx, library->getPlatformInterface(), collection, de::SharedPtr<vk::ResourceInterface>{new vk::ResourceInterfaceStandard{ tcx }});

		// create pipeline cache
		std::vector<deUint8>			binary			= vksc_server::buildPipelineCache(
															input,
															library->getPlatformInterface(),
															context.getInstance(),
															context.getInstanceInterface(),
															context.getPhysicalDevice(),
															context.getUniversalQueueFamilyIndex());

		// write pipeline cache to output file
		std::ofstream					oFile			(cmdLine.getOption<opt::CompilerOutputFile>().c_str(), std::ios::out | std::ios::binary);
		if (!oFile)
			TCU_THROW(InternalError, (std::string("Cannot create file : ") + cmdLine.getOption<opt::CompilerOutputFile>().c_str()));
		oFile.write(reinterpret_cast<char*>(binary.data()), binary.size());
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
