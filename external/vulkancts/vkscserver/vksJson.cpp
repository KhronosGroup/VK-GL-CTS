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

#include "vksJson.hpp"

#define VULKAN_JSON_CTS
#ifdef __GNUC__
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-parameter"
	#pragma GCC diagnostic ignored "-Wunused-function"
	#pragma GCC diagnostic ignored "-Wunused-variable"
#endif // __GNUC__
#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#endif

#include "vulkan_json_parser.hpp"
#include "vulkan_json_data.hpp"

#ifdef __GNUC__
	#pragma GCC diagnostic pop
#endif // __GNUC__
#ifdef __clang__
	#pragma clang diagnostic pop
#endif

#include "vksStructsVKSC.hpp"

namespace vksc_server
{

namespace json
{

Context::Context()
{
	Json::CharReaderBuilder builder;
	builder.settings_["allowSpecialFloats"] = 1;
	reader.reset( builder.newCharReader() );
}

Context::~Context()
{
}

void runGarbageCollection()
{
	vk_json_parser::s_globalMem.clear();
}

void VkObjectToString(const vk::VkDeviceObjectReservationCreateInfo& in, string& out)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkDeviceObjectReservationCreateInfo(&in, "", false);
	out = vk_json::_string_stream.str();
}

void StringToVkObject(const string& in, vk::VkDeviceObjectReservationCreateInfo& out)
{
	Json::CharReaderBuilder				builder;
	builder.settings_["allowSpecialFloats"] = 1;
	std::unique_ptr<Json::CharReader>	jsonReader(builder.newCharReader());

	Json::Value jsonRoot;
	string errors;
	if (!jsonReader->parse(in.data(), in.data() + in.size(), &jsonRoot, &errors))
	{
		throw std::runtime_error("json parse error");
	}
	vk_json_parser::parse_VkDeviceObjectReservationCreateInfo("", jsonRoot, out);
}

string writeJSON_VkGraphicsPipelineCreateInfo(const VkGraphicsPipelineCreateInfo&	pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkGraphicsPipelineCreateInfo(pCreateInfo, "", 0);
	return vk_json::_string_stream.str();
}

string writeJSON_VkComputePipelineCreateInfo(const VkComputePipelineCreateInfo&	pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkComputePipelineCreateInfo(pCreateInfo, "", 0);
	return vk_json::_string_stream.str();
}

string writeJSON_VkRenderPassCreateInfo (const VkRenderPassCreateInfo& pCreateInfo)
{

	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkRenderPassCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

string writeJSON_VkRenderPassCreateInfo2 (const VkRenderPassCreateInfo2& pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkRenderPassCreateInfo2(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

string writeJSON_VkPipelineLayoutCreateInfo (const VkPipelineLayoutCreateInfo& pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkPipelineLayoutCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

string writeJSON_VkDescriptorSetLayoutCreateInfo (const VkDescriptorSetLayoutCreateInfo& pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkDescriptorSetLayoutCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

string writeJSON_VkSamplerCreateInfo(const VkSamplerCreateInfo& pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkSamplerCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

string writeJSON_VkDeviceObjectReservationCreateInfo (const VkDeviceObjectReservationCreateInfo& dmrCI)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkDeviceObjectReservationCreateInfo(&dmrCI, "", false);
	return vk_json::_string_stream.str();
}

string	writeJSON_VkPipelineOfflineCreateInfo(const vk::VkPipelineOfflineCreateInfo& piInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkPipelineOfflineCreateInfo(&piInfo, "", false);
	return vk_json::_string_stream.str();
}

string	writeJSON_GraphicsPipeline_vkpccjson (const std::string&															filePrefix,
											  deUint32																		pipelineIndex,
											  const vk::VkPipelineOfflineCreateInfo											id,
											  const VkGraphicsPipelineCreateInfo&											gpCI,
											  const vk::VkPhysicalDeviceFeatures2&											deviceFeatures2,
											  const std::vector<std::string>&												deviceExtensions,
											  const std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversionCreateInfo>&	samplerYcbcrConversions,
											  const std::map<VkSampler, VkSamplerCreateInfo>&								samplers,
											  const std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo>&		descriptorSetLayouts,
											  const std::map<VkRenderPass, VkRenderPassCreateInfo>&							renderPasses,
											  const std::map<VkRenderPass, VkRenderPassCreateInfo2>&						renderPasses2,
											  const std::map<VkPipelineLayout, VkPipelineLayoutCreateInfo>&					pipelineLayouts)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "{" << std::endl;
	vk_json::s_num_spaces += 4;

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"GraphicsPipelineState\" :" << std::endl;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "{" << std::endl;
	vk_json::s_num_spaces += 4;

	if (!renderPasses.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"Renderpass\" : " << std::endl;
		vk_json::print_VkRenderPassCreateInfo(begin(renderPasses)->second, "", true);
	}
	if (!renderPasses2.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"Renderpass2\" : " << std::endl;
		vk_json::print_VkRenderPassCreateInfo2(begin(renderPasses2)->second, "", true);
	}
	if (!samplerYcbcrConversions.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"YcbcrSamplers\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(samplerYcbcrConversions); it != end(samplerYcbcrConversions); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\""<< it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkSamplerYcbcrConversionCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			if((j + 1) < samplerYcbcrConversions.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	if (!samplers.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"ImmutableSamplers\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(samplers); it != end(samplers); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"" << it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkSamplerCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			if ((j + 1) < samplers.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	if (!descriptorSetLayouts.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"DescriptorSetLayouts\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(descriptorSetLayouts); it != end(descriptorSetLayouts); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"" << it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkDescriptorSetLayoutCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			if ((j + 1) < descriptorSetLayouts.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PipelineLayout\" : " << std::endl;
	vk_json::print_VkPipelineLayoutCreateInfo(begin(pipelineLayouts)->second, "", true);

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"GraphicsPipeline\" : " << std::endl;
	vk_json::print_VkGraphicsPipelineCreateInfo(gpCI, "", true);

	// shaders
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"ShaderFileNames\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		for( deUint32 j=0; j<gpCI.stageCount; ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			vk_json::print_VkShaderStageFlagBits(gpCI.pStages[j].stage, "stage", 1);

			std::stringstream shaderName;
			shaderName << filePrefix << "shader_" << pipelineIndex << "_" << gpCI.pStages[j].module.getInternal() << ".";

			switch (gpCI.pStages[j].stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:					shaderName << "vert";	break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		shaderName << "tesc";	break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	shaderName << "tese";	break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:					shaderName << "geom";	break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:					shaderName << "frag";	break;
			default:
				TCU_THROW(InternalError, "Unrecognized shader stage");
			}
			shaderName << ".spv";

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"filename\" : \"" << shaderName.str() << "\"" << std::endl;
			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			if ((j+1) >= gpCI.stageCount)
				vk_json::_string_stream << "}" << std::endl;
			else
				vk_json::_string_stream << "}," << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	// device features
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PhysicalDeviceFeatures\" : " << std::endl;
	vk_json::print_VkPhysicalDeviceFeatures2(deviceFeatures2, "", false);

	// close GraphicsPipelineState
	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "}," << std::endl;

	// device extensions
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"EnabledExtensions\" : " << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		for (unsigned int j = 0; j < deviceExtensions.size(); j++)
			vk_json::print_char(deviceExtensions[j].data(), "", (j + 1) != deviceExtensions.size());

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	// pipeline identifier
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PipelineUUID\" : " << std::endl;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "[" << std::endl;
	vk_json::s_num_spaces += 4;
	for (unsigned int j = 0; j < VK_UUID_SIZE; j++)
		vk_json::print_uint32_t((deUint32)id.pipelineIdentifier[j], "", (j + 1) != VK_UUID_SIZE);
	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "]" << std::endl;

	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "}" << std::endl;

	return vk_json::_string_stream.str();
}

string	writeJSON_ComputePipeline_vkpccjson (const std::string&																filePrefix,
											 deUint32																		pipelineIndex,
											 const vk::VkPipelineOfflineCreateInfo											id,
											 const VkComputePipelineCreateInfo&												cpCI,
											 const vk::VkPhysicalDeviceFeatures2&											deviceFeatures2,
											 const std::vector<std::string>&												deviceExtensions,
											 const std::map<VkSamplerYcbcrConversion, VkSamplerYcbcrConversionCreateInfo>&	samplerYcbcrConversions,
											 const std::map<VkSampler, VkSamplerCreateInfo>&								samplers,
											 const std::map<VkDescriptorSetLayout, VkDescriptorSetLayoutCreateInfo>&		descriptorSetLayouts,
											 const std::map<VkPipelineLayout, VkPipelineLayoutCreateInfo>&					pipelineLayouts)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "{" << std::endl;
	vk_json::s_num_spaces += 4;

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"ComputePipelineState\" :" << std::endl;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "{" << std::endl;
	vk_json::s_num_spaces += 4;

	if (!samplerYcbcrConversions.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"YcbcrSamplers\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(samplerYcbcrConversions); it != end(samplerYcbcrConversions); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"" << it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkSamplerYcbcrConversionCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			if ((j + 1) < samplerYcbcrConversions.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	if (!samplers.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"ImmutableSamplers\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(samplers); it != end(samplers); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"" << it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkSamplerCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			if ((j + 1) < samplers.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	if (!descriptorSetLayouts.empty())
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"DescriptorSetLayouts\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		size_t j = 0u;
		for (auto it = begin(descriptorSetLayouts); it != end(descriptorSetLayouts); ++it, ++j)
		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"" << it->first.getInternal() << "\":" << std::endl;

			vk_json::print_VkDescriptorSetLayoutCreateInfo(it->second, "", false);

			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			if ((j + 1) < descriptorSetLayouts.size())
				vk_json::_string_stream << "}," << std::endl;
			else
				vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PipelineLayout\" : " << std::endl;
	vk_json::print_VkPipelineLayoutCreateInfo(begin(pipelineLayouts)->second, "", true);

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"ComputePipeline\" : " << std::endl;
	vk_json::print_VkComputePipelineCreateInfo(cpCI, "", true);

	// shaders
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"ShaderFileNames\" :" << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		{
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "{" << std::endl;
			vk_json::s_num_spaces += 4;

			vk_json::print_VkShaderStageFlagBits(cpCI.stage.stage, "stage", 1);

			std::stringstream shaderName;
			shaderName << filePrefix << "shader_" << pipelineIndex << "_" << cpCI.stage.module.getInternal() << ".";

			switch (cpCI.stage.stage)
			{
			case VK_SHADER_STAGE_COMPUTE_BIT:					shaderName << "comp";	break;
			default:
				TCU_THROW(InternalError, "Unrecognized shader stage");
			}
			shaderName << ".spv";

			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"filename\" : \"" << shaderName.str() << "\"" << std::endl;
			vk_json::s_num_spaces -= 4;
			for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";

			vk_json::_string_stream << "}" << std::endl;
		}

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	// device features
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PhysicalDeviceFeatures\" : " << std::endl;
	vk_json::print_VkPhysicalDeviceFeatures2(deviceFeatures2, "", false);

	// close ComputePipelineState
	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "}," << std::endl;

	// device extensions
	{
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "\"EnabledExtensions\" : " << std::endl;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "[" << std::endl;
		vk_json::s_num_spaces += 4;

		for (unsigned int j = 0; j < deviceExtensions.size(); j++)
			vk_json::print_char(deviceExtensions[j].data(), "", (j + 1) != deviceExtensions.size());

		vk_json::s_num_spaces -= 4;
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "]," << std::endl;
	}

	// pipeline identifier
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "\"PipelineUUID\" : " << std::endl;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "[" << std::endl;
	vk_json::s_num_spaces += 4;
	for (unsigned int j = 0; j < VK_UUID_SIZE; j++)
		vk_json::print_uint32_t((deUint32)id.pipelineIdentifier[j], "", (j + 1) != VK_UUID_SIZE);
	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "]" << std::endl;

	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
	vk_json::_string_stream << "}" << std::endl;

	return vk_json::_string_stream.str();
}

string writeJSON_VkPhysicalDeviceFeatures2 (const vk::VkPhysicalDeviceFeatures2& features)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkPhysicalDeviceFeatures2(&features, "", false);
	return vk_json::_string_stream.str();
}

string	writeJSON_pNextChain (const void* pNext)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::dumpPNextChain(pNext);
	std::string result = vk_json::_string_stream.str();
	// remove "pNext" at the beggining of result and trailing comma
	return std::string(begin(result) + result.find_first_of('{'), begin(result) + result.find_last_of('}') + 1u);
}

string writeJSON_VkSamplerYcbcrConversionCreateInfo (const VkSamplerYcbcrConversionCreateInfo& pCreateInfo)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	vk_json::print_VkSamplerYcbcrConversionCreateInfo(&pCreateInfo, "", false);
	return vk_json::_string_stream.str();
}

static void print_VkShaderModuleCreateInfo (const VkShaderModuleCreateInfo* obj, const string& s, bool commaNeeded)
{
	DE_UNREF(s);

	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		vk_json::_string_stream << "{" << std::endl;
		vk_json::s_num_spaces += 4;

	vk_json::print_VkStructureType(obj->sType, "sType", 1);

	if (obj->pNext) {
		vk_json::dumpPNextChain(obj->pNext);
	}
	else {
		for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
			vk_json::_string_stream << "\"pNext\":" << "\"NULL\"" << "," << std::endl;
	}

	// VkShaderModuleCreateFlags is reserved for future use and must be 0.
	vk_json::print_uint32_t((deUint32)obj->flags, "flags", 1);
	vk_json::print_uint64_t((deUint64)obj->codeSize, "codeSize", 1);

	// pCode must be translated into base64, because JSON
	vk_json::print_void_data(obj->pCode, static_cast<int>(obj->codeSize), "pCode", 0);

	vk_json::s_num_spaces -= 4;
	for (int i = 0; i < vk_json::s_num_spaces; i++) vk_json::_string_stream << " ";
		if (commaNeeded)
			vk_json::_string_stream << "}," << std::endl;
		else
			vk_json::_string_stream << "}" << std::endl;
}

string writeJSON_VkShaderModuleCreateInfo (const VkShaderModuleCreateInfo& smCI)
{
	vk_json::_string_stream.str({});
	vk_json::_string_stream.clear();
	print_VkShaderModuleCreateInfo(&smCI, "", false);
	return vk_json::_string_stream.str();
}

void readJSON_VkGraphicsPipelineCreateInfo (Context& context,
											const string& graphicsPipelineCreateInfo,
											VkGraphicsPipelineCreateInfo&	gpCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(graphicsPipelineCreateInfo.c_str(), graphicsPipelineCreateInfo.c_str() + graphicsPipelineCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkGraphicsPipelineCreateInfo("", jsonRoot, gpCI);
}

void readJSON_VkComputePipelineCreateInfo (Context& context,
										   const string& computePipelineCreateInfo,
										   VkComputePipelineCreateInfo&	cpCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(computePipelineCreateInfo.c_str(), computePipelineCreateInfo.c_str() + computePipelineCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkComputePipelineCreateInfo("", jsonRoot, cpCI);
}

void readJSON_VkRenderPassCreateInfo (Context& context,
									  const string& renderPassCreateInfo,
									  VkRenderPassCreateInfo&	rpCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(renderPassCreateInfo.c_str(), renderPassCreateInfo.c_str() + renderPassCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkRenderPassCreateInfo("", jsonRoot, rpCI);
}

void readJSON_VkRenderPassCreateInfo2 (Context& context,
									   const string& renderPassCreateInfo,
									   VkRenderPassCreateInfo2&	rpCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(renderPassCreateInfo.c_str(), renderPassCreateInfo.c_str() + renderPassCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkRenderPassCreateInfo2("", jsonRoot, rpCI);
}

void readJSON_VkDescriptorSetLayoutCreateInfo (Context& context,
											   const string& descriptorSetLayoutCreateInfo,
											   VkDescriptorSetLayoutCreateInfo&	dsCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(descriptorSetLayoutCreateInfo.c_str(), descriptorSetLayoutCreateInfo.c_str() + descriptorSetLayoutCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkDescriptorSetLayoutCreateInfo("", jsonRoot, dsCI);
}

void readJSON_VkPipelineLayoutCreateInfo (Context& context,
										  const string& pipelineLayoutCreateInfo,
										  VkPipelineLayoutCreateInfo& plCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(pipelineLayoutCreateInfo.c_str(), pipelineLayoutCreateInfo.c_str() + pipelineLayoutCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkPipelineLayoutCreateInfo("", jsonRoot, plCI);
}

void readJSON_VkDeviceObjectReservationCreateInfo (Context& context,
												   const string& deviceMemoryReservation,
												   VkDeviceObjectReservationCreateInfo&	dmrCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful	= context.reader->parse(deviceMemoryReservation.c_str(), deviceMemoryReservation.c_str() + deviceMemoryReservation.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkDeviceObjectReservationCreateInfo("", jsonRoot, dmrCI);
}

void readJSON_VkPipelineOfflineCreateInfo (Context& context,
										   const string& pipelineIdentifierInfo,
										   vk::VkPipelineOfflineCreateInfo& piInfo)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful	= context.reader->parse(pipelineIdentifierInfo.c_str(), pipelineIdentifierInfo.c_str() + pipelineIdentifierInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkPipelineOfflineCreateInfo("", jsonRoot, piInfo);
}

void readJSON_VkSamplerCreateInfo (Context& context,
								   const string& samplerCreateInfo,
								   VkSamplerCreateInfo&	sCI)
{
	Json::Value						jsonRoot;
	string							errors;
	bool							parsingSuccessful = context.reader->parse(samplerCreateInfo.c_str(), samplerCreateInfo.c_str() + samplerCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	vk_json_parser::parse_VkSamplerCreateInfo("", jsonRoot, sCI);
}

void readJSON_VkSamplerYcbcrConversionCreateInfo (Context& context,
												  const std::string& samplerYcbcrConversionCreateInfo,
												  VkSamplerYcbcrConversionCreateInfo& sycCI)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = context.reader->parse(samplerYcbcrConversionCreateInfo.c_str(), samplerYcbcrConversionCreateInfo.c_str() + samplerYcbcrConversionCreateInfo.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkSamplerYcbcrConversionCreateInfo("", jsonRoot, sycCI);
}

void readJSON_VkPhysicalDeviceFeatures2 (Context&									context,
										 const std::string&							featuresJson,
										 vk::VkPhysicalDeviceFeatures2&				features)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = context.reader->parse(featuresJson.c_str(), featuresJson.c_str() + featuresJson.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	vk_json_parser::parse_VkPhysicalDeviceFeatures2("", jsonRoot, features);
}

void* readJSON_pNextChain (Context&				context,
						  const std::string&	chainJson)
{
	Json::Value						jsonRoot;
	std::string						errors;
	bool							parsingSuccessful = context.reader->parse(chainJson.c_str(), chainJson.c_str() + chainJson.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, (std::string("JSON parsing error: ") + errors).c_str());
	return vk_json_parser::parsePNextChain(jsonRoot);
}

static void parse_VkShaderModuleCreateInfo (const char*					s,
											Json::Value&				obj,
											VkShaderModuleCreateInfo&	o,
											std::vector<deUint8>&		spirvShader)
{
	DE_UNREF(s);

	vk_json_parser::parse_VkStructureType("sType", obj["sType"], (o.sType));

	o.pNext = (VkDeviceObjectReservationCreateInfo*)vk_json_parser::parsePNextChain(obj);

	vk_json_parser::parse_uint32_t("flags", obj["flags"], (o.flags));
	deUint64 codeSizeValue;
	vk_json_parser::parse_uint64_t("codeSize", obj["codeSize"], (codeSizeValue));
	o.codeSize = (deUintptr)codeSizeValue;

	// pCode is encoded using Base64.
	spirvShader = vk_json_parser::base64decode(obj["pCode"].asString());
	// Base64 always decodes a multiple of 3 bytes, so the size could mismatch the module
	// size by one or two bytes. resize spirvShader to match.
	spirvShader.resize(o.codeSize);
	o.pCode = (deUint32*)spirvShader.data();
}

void readJSON_VkShaderModuleCreateInfo (Context& context,
										const string&				shaderModuleCreate,
										VkShaderModuleCreateInfo&	smCI,
										std::vector<deUint8>&		spirvShader)
{
	Json::Value			jsonRoot;
	string				errors;
	bool				parsingSuccessful = context.reader->parse(shaderModuleCreate.c_str(), shaderModuleCreate.c_str() + shaderModuleCreate.size(), &jsonRoot, &errors);
	if (!parsingSuccessful)
		TCU_THROW(InternalError, ("JSON parsing error: " + errors).c_str());
	parse_VkShaderModuleCreateInfo("", jsonRoot, smCI, spirvShader);
}

} // namespace json

} // namespace vksc_server
