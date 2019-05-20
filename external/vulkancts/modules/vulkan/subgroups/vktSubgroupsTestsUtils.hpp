#ifndef _VKTSUBGROUPSTESTSUTILS_HPP
#define _VKTSUBGROUPSTESTSUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups tests utility classes
 */ /*--------------------------------------------------------------------*/

#include "vkBuilderUtil.hpp"
#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "gluShaderUtil.hpp"

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"

#include <string>

namespace vkt
{
namespace subgroups
{
// A struct to represent input data to a shader
struct SSBOData
{
	SSBOData() :
		initializeType	(InitializeNone),
		layout			(LayoutStd140),
		format			(vk::VK_FORMAT_UNDEFINED),
		numElements		(0),
		isImage			(false),
		binding			(0u),
		stages			((vk::VkShaderStageFlagBits)0u)
	{}

	enum InputDataInitializeType
	{
		InitializeNone = 0,
		InitializeNonZero,
		InitializeZero,
	} initializeType;

	enum InputDataLayoutType
	{
		LayoutStd140 = 0,
		LayoutStd430,
		LayoutPacked
	} layout;

	vk::VkFormat				format;
	vk::VkDeviceSize			numElements;
	bool						isImage;
	deUint32					binding;
	vk::VkShaderStageFlagBits	stages;
};

std::string getSharedMemoryBallotHelper();

deUint32 getSubgroupSize(Context& context);

vk::VkDeviceSize maxSupportedSubgroupSize();

std::string getShaderStageName(vk::VkShaderStageFlags stage);

std::string getSubgroupFeatureName(vk::VkSubgroupFeatureFlagBits bit);

void addNoSubgroupShader (vk::SourceCollections& programCollection);

std::string getVertShaderForStage(vk::VkShaderStageFlags stage);//TODO

bool isSubgroupSupported(Context& context);

bool areSubgroupOperationsSupportedForStage(
	Context& context, vk::VkShaderStageFlags stage);

bool areSubgroupOperationsRequiredForStage(vk::VkShaderStageFlags stage);

bool isSubgroupFeatureSupportedForDevice(Context& context, vk::VkSubgroupFeatureFlagBits bit);

bool isFragmentSSBOSupportedForDevice(Context& context);

bool isVertexSSBOSupportedForDevice(Context& context);

bool isDoubleSupportedForDevice(Context& context);

bool isTessellationAndGeometryPointSizeSupported(Context& context);

bool isDoubleFormat(vk::VkFormat format);

std::string getFormatNameForGLSL(vk::VkFormat format);

void addGeometryShadersFromTemplate (const std::string& glslTemplate, const vk::ShaderBuildOptions& options, vk::GlslSourceCollection& collection);
void addGeometryShadersFromTemplate (const std::string& spirvTemplate, const vk::SpirVAsmBuildOptions& options, vk::SpirVAsmCollection& collection);

void setVertexShaderFrameBuffer (vk::SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection);

void setTesCtrlShaderFrameBuffer (vk::SourceCollections& programCollection);

void setTesEvalShaderFrameBuffer (vk::SourceCollections& programCollection);

bool check(std::vector<const void*> datas,
	deUint32 width, deUint32 ref);

bool checkCompute(std::vector<const void*> datas,
	const deUint32 numWorkgroups[3], const deUint32 localSize[3],
	deUint32 ref);

tcu::TestStatus makeTessellationEvaluationFrameBufferTest(Context& context, vk::VkFormat format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const vk::VkShaderStageFlags shaderStage = vk::VK_SHADER_STAGE_ALL_GRAPHICS);

tcu::TestStatus makeGeometryFrameBufferTest(Context& context, vk::VkFormat format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize));

tcu::TestStatus allStages(Context& context, vk::VkFormat format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const vk::VkShaderStageFlags shaderStage);

tcu::TestStatus makeVertexFrameBufferTest(Context& context, vk::VkFormat format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize));

tcu::TestStatus makeFragmentFrameBufferTest(Context& context, vk::VkFormat format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width,
									 deUint32 height, deUint32 subgroupSize));

tcu::TestStatus makeComputeTest(
	Context& context, vk::VkFormat format, SSBOData* inputs,
	deUint32 inputsCount,
	bool (*checkResult)(std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32 subgroupSize));
} // subgroups
} // vkt

#endif // _VKTSUBGROUPSTESTSUTILS_HPP
