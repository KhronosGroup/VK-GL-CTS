#ifndef _VKSJSON_HPP
#define _VKSJSON_HPP

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

#include "vkPrograms.hpp"

#include "vkDefs.hpp"
using namespace vk;

namespace Json
{
	class CharReader;
}

namespace vksc_server
{

struct VulkanPipelineStatistics;

namespace json
{

struct Context
{
	Context();
	~Context();
	std::unique_ptr<Json::CharReader> reader;
};

void	runGarbageCollection								();

string	writeJSON_VkGraphicsPipelineCreateInfo				(const vk::VkGraphicsPipelineCreateInfo&		pCreateInfo);
string	writeJSON_VkComputePipelineCreateInfo				(const vk::VkComputePipelineCreateInfo&			pCreateInfo);
string	writeJSON_VkRenderPassCreateInfo					(const vk::VkRenderPassCreateInfo&				pCreateInfo);
string	writeJSON_VkRenderPassCreateInfo2					(const vk::VkRenderPassCreateInfo2&				pCreateInfo);
string	writeJSON_VkPipelineLayoutCreateInfo				(const vk::VkPipelineLayoutCreateInfo&			pCreateInfo);
string	writeJSON_VkDescriptorSetLayoutCreateInfo			(const vk::VkDescriptorSetLayoutCreateInfo&		pCreateInfo);
string	writeJSON_VkSamplerCreateInfo						(const vk::VkSamplerCreateInfo&					pCreateInfo);
string	writeJSON_VkSamplerYcbcrConversionCreateInfo		(const vk::VkSamplerYcbcrConversionCreateInfo&	pCreateInfo);
string	writeJSON_VkShaderModuleCreateInfo					(const vk::VkShaderModuleCreateInfo&			smCI);
string	writeJSON_VkDeviceObjectReservationCreateInfo		(const vk::VkDeviceObjectReservationCreateInfo&	dmrCI);
string	writeJSON_VkPipelineOfflineCreateInfo				(const vk::VkPipelineOfflineCreateInfo&			piInfo);
string	writeJSON_GraphicsPipeline_vkpccjson				(const std::string&																		filePrefix,
															 deUint32																				pipelineIndex,
															 const vk::VkPipelineOfflineCreateInfo													id,
															 const vk::VkGraphicsPipelineCreateInfo&												gpCI,
															 const vk::VkPhysicalDeviceFeatures2&													deviceFeatures2,
															 const std::vector<std::string>&														deviceExtensions,
															 const std::map<vk::VkSamplerYcbcrConversion, vk::VkSamplerYcbcrConversionCreateInfo>&	samplerYcbcrConversions,
															 const std::map<vk::VkSampler, vk::VkSamplerCreateInfo>&								samplers,
															 const std::map<vk::VkDescriptorSetLayout, vk::VkDescriptorSetLayoutCreateInfo>&		descriptorSetLayouts,
															 const std::map<vk::VkRenderPass, vk::VkRenderPassCreateInfo>&							renderPasses,
															 const std::map<vk::VkRenderPass, vk::VkRenderPassCreateInfo2>&							renderPasses2,
															 const std::map<vk::VkPipelineLayout, vk::VkPipelineLayoutCreateInfo>&					pipelineLayouts);
string	writeJSON_ComputePipeline_vkpccjson					(const std::string&																		filePrefix,
															 deUint32																				pipelineIndex,
															 const vk::VkPipelineOfflineCreateInfo													id,
															 const vk::VkComputePipelineCreateInfo&													cpCI,
															 const vk::VkPhysicalDeviceFeatures2&													deviceFeatures2,
															 const std::vector<std::string>&														deviceExtensions,
															 const std::map<vk::VkSamplerYcbcrConversion, vk::VkSamplerYcbcrConversionCreateInfo>&	samplerYcbcrConversions,
															 const std::map<vk::VkSampler, vk::VkSamplerCreateInfo>&								samplers,
															 const std::map<vk::VkDescriptorSetLayout, vk::VkDescriptorSetLayoutCreateInfo>&		descriptorSetLayouts,
															 const std::map<vk::VkPipelineLayout, vk::VkPipelineLayoutCreateInfo>&					pipelineLayouts);
string	writeJSON_VkPhysicalDeviceFeatures2					(const vk::VkPhysicalDeviceFeatures2&			features);
string	writeJSON_pNextChain								(const void*			pNext);

void	readJSON_VkGraphicsPipelineCreateInfo				(Context&									context,
															 const string&								graphicsPipelineCreateInfo,
															 vk::VkGraphicsPipelineCreateInfo&			gpCI);
void	readJSON_VkComputePipelineCreateInfo				(Context&									context,
															 const string&								computePipelineCreateInfo,
															 vk::VkComputePipelineCreateInfo&			cpCI);
void	readJSON_VkRenderPassCreateInfo						(Context&									context,
															 const string&								renderPassCreateInfo,
															 vk::VkRenderPassCreateInfo&				rpCI);
void	readJSON_VkRenderPassCreateInfo2					(Context&									context,
															 const string&								renderPassCreateInfo,
															 vk::VkRenderPassCreateInfo2&				rpCI);
void	readJSON_VkDescriptorSetLayoutCreateInfo			(Context&									context,
															 const string&								descriptorSetLayoutCreateInfo,
															 vk::VkDescriptorSetLayoutCreateInfo&		dsCI);
void	readJSON_VkPipelineLayoutCreateInfo					(Context&									context,
															 const string&								pipelineLayoutCreateInfo,
															 vk::VkPipelineLayoutCreateInfo&			plCI);
void	readJSON_VkShaderModuleCreateInfo					(Context&									context,
															 const string&								shaderModuleCreate,
															 vk::VkShaderModuleCreateInfo&				smCI,
															 vector<deUint8>&							spirvShader);
void	readJSON_VkDeviceObjectReservationCreateInfo		(Context&									context,
															 const string&								deviceMemoryReservation,
															 vk::VkDeviceObjectReservationCreateInfo&	dmrCI);
void	readJSON_VkPipelineOfflineCreateInfo				(Context&									context,
															 const string&								pipelineIdentifierInfo,
															 vk::VkPipelineOfflineCreateInfo&			piInfo);
void	readJSON_VkSamplerCreateInfo						(Context&									context,
															 const string&								samplerCreateInfo,
															 vk::VkSamplerCreateInfo&					sCI);
void	readJSON_VkSamplerYcbcrConversionCreateInfo			(Context&									context,
															 const std::string&							samplerYcbcrConversionCreateInfo,
															 vk::VkSamplerYcbcrConversionCreateInfo&	sycCI);
void	readJSON_VkPhysicalDeviceFeatures2					(Context&									context,
															 const std::string&							featuresJson,
															 vk::VkPhysicalDeviceFeatures2&				features);
void*	readJSON_pNextChain									(Context&									context,
															 const std::string&							chainJson);

}

}

#endif // _VKSJSON_HPP
