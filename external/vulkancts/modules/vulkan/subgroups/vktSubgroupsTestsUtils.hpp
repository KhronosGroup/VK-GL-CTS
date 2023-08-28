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
#include "vkRayTracingUtil.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "gluShaderUtil.hpp"

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace subgroups
{
typedef bool (*CheckResult)(const void* internalData, std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize);
typedef bool (*CheckResultFragment)(const void* internalData, std::vector<const void*> datas, deUint32 width, deUint32 height, deUint32 subgroupSize);
typedef bool (*CheckResultCompute)(const void* internalData, std::vector<const void*> datas, const deUint32 numWorkgroups[3], const deUint32 localSize[3], deUint32 subgroupSize);

// A struct to represent input data to a shader
struct SSBOData
{
	enum InputDataInitializeType
	{
		InitializeNone = 0,
		InitializeNonZero,
		InitializeZero,
	};
	enum InputDataLayoutType
	{
		LayoutStd140 = 0,
		LayoutStd430,
		LayoutPacked
	};

	enum BindingType
	{
		BindingSSBO,
		BindingImage,
		BindingUBO,
	};

	SSBOData() :
		initializeType	(InitializeNone),
		layout			(LayoutStd140),
		format			(vk::VK_FORMAT_UNDEFINED),
		numElements		(0),
		bindingType		(BindingSSBO),
		binding			(0u),
		stages			((vk::VkShaderStageFlags)0u)
	{}

	SSBOData (InputDataInitializeType	initializeType_,
			  InputDataLayoutType		layout_,
			  vk::VkFormat				format_,
			  vk::VkDeviceSize			numElements_,
			  BindingType				bindingType_	= BindingSSBO,
			  deUint32					binding_		= 0u,
			  vk::VkShaderStageFlags	stages_			= static_cast<vk::VkShaderStageFlags>(0u))
		: initializeType	(initializeType_)
		, layout			(layout_)
		, format			(format_)
		, numElements		(numElements_)
		, bindingType		(bindingType_)
		, binding			(binding_)
		, stages			(stages_)
	{
		if (bindingType == BindingUBO)
			DE_ASSERT(layout == LayoutStd140);
	}

	bool isImage () const
	{
		return (bindingType == BindingImage);
	}

	bool isUBO () const
	{
		return (bindingType == BindingUBO);
	}

	InputDataInitializeType		initializeType;
	InputDataLayoutType			layout;
	vk::VkFormat				format;
	vk::VkDeviceSize			numElements;
	BindingType					bindingType;
	deUint32					binding;
	vk::VkShaderStageFlags		stages;
};

deUint32 getStagesCount (vk::VkShaderStageFlags shaderStages);

std::string getSharedMemoryBallotHelper ();

std::string getSharedMemoryBallotHelperARB ();

deUint32 getSubgroupSize (Context& context);

deUint32 maxSupportedSubgroupSize ();

std::string getShaderStageName (vk::VkShaderStageFlags stage);

std::string getSubgroupFeatureName (vk::VkSubgroupFeatureFlagBits bit);

void addNoSubgroupShader (vk::SourceCollections& programCollection);

void initStdFrameBufferPrograms (vk::SourceCollections&				programCollection,
								 const vk::ShaderBuildOptions&		buildOptions,
								 vk::VkShaderStageFlags				shaderStage,
								 vk::VkFormat						format,
								 bool								gsPointSize,
								 const std::string&					extHeader,
								 const std::string&					testSrc,
								 const std::string&					helperStr,
								 const std::vector<std::string>&	declarations	= std::vector<std::string>());

void initStdPrograms (vk::SourceCollections&			programCollection,
					  const vk::ShaderBuildOptions&		buildOptions,
					  vk::VkShaderStageFlags			shaderStage,
					  vk::VkFormat						format,
					  bool								gsPointSize,
					  const std::string&				extHeader,
					  const std::string&				testSrc,
					  const std::string&				helperStr,
					  const std::vector<std::string>&	declarations			= std::vector<std::string>(),
					  const bool						avoidHelperInvocations	= false,
					  const std::string&				tempRes					= "  uint tempRes;\n");

bool isSubgroupSupported (Context& context);

bool areSubgroupOperationsSupportedForStage (Context& context, vk::VkShaderStageFlags stage);

bool isSubgroupFeatureSupportedForDevice (Context& context, vk::VkSubgroupFeatureFlagBits bit);

bool areQuadOperationsSupportedForStages (Context& context, const vk::VkShaderStageFlags stages);

bool isFragmentSSBOSupportedForDevice (Context& context);

bool isVertexSSBOSupportedForDevice (Context& context);

bool isFormatSupportedForDevice (Context& context, vk::VkFormat format);

bool isInt64SupportedForDevice (Context& context);

bool isTessellationAndGeometryPointSizeSupported (Context& context);

bool is16BitUBOStorageSupported (Context& context);

bool is8BitUBOStorageSupported (Context& context);

bool isSubgroupBroadcastDynamicIdSupported (Context& context);

std::string getFormatNameForGLSL (vk::VkFormat format);

std::string getAdditionalExtensionForFormat (vk::VkFormat format);

const std::vector<vk::VkFormat> getAllFormats ();

bool isFormatSigned (vk::VkFormat format);
bool isFormatUnsigned (vk::VkFormat format);
bool isFormatFloat (vk::VkFormat format);
bool isFormatBool (vk::VkFormat format);
bool isFormat8bitTy (vk::VkFormat format);
bool isFormat16BitTy (vk::VkFormat format);

void addGeometryShadersFromTemplate (const std::string& glslTemplate, const vk::ShaderBuildOptions& options, vk::GlslSourceCollection& collection);
void addGeometryShadersFromTemplate (const std::string& spirvTemplate, const vk::SpirVAsmBuildOptions& options, vk::SpirVAsmCollection& collection);

void setVertexShaderFrameBuffer (vk::SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection);

void setTesCtrlShaderFrameBuffer (vk::SourceCollections& programCollection);

void setTesEvalShaderFrameBuffer (vk::SourceCollections& programCollection);

bool check (std::vector<const void*> datas, deUint32 width, deUint32 ref);

bool checkComputeOrMesh (std::vector<const void*>	datas,
						 const deUint32				numWorkgroups[3],
						 const deUint32				localSize[3],
						 deUint32					ref);

tcu::TestStatus makeTessellationEvaluationFrameBufferTest (Context&						context,
														   vk::VkFormat					format,
														   const SSBOData*				extraData,
														   deUint32						extraDataCount,
														   const void*					internalData,
														   CheckResult					checkResult,
														   const vk::VkShaderStageFlags	shaderStage = vk::VK_SHADER_STAGE_ALL_GRAPHICS);

tcu::TestStatus makeGeometryFrameBufferTest (Context&			context,
											 vk::VkFormat		format,
											 const SSBOData*	extraData,
											 deUint32			extraDataCount,
											 const void*		internalData,
											 CheckResult		checkResult);

// Allows using verification functions with or without the optional last boolean argument.
// If using a function that does not need the last argument, it will not be passed down to it.
class VerificationFunctor
{
public:
	using NoLastArgVariant	= bool(*)(const void*, std::vector<const void*>, deUint32, deUint32);
	using AllArgsVariant	= bool(*)(const void*, std::vector<const void*>, deUint32, deUint32, bool);

	VerificationFunctor (NoLastArgVariant func)
		: m_noLastArgFunc{func}, m_allArgsFunc{nullptr}
		{}

	VerificationFunctor (AllArgsVariant func)
		: m_noLastArgFunc{nullptr}, m_allArgsFunc{func}
		{}

	bool operator() (const void* extraData, std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize, bool multipleCallsPossible) const
	{
		if (m_allArgsFunc)
			return m_allArgsFunc(extraData, datas, width, subgroupSize, multipleCallsPossible);
		return m_noLastArgFunc(extraData, datas, width, subgroupSize);
	}

private:
	NoLastArgVariant	m_noLastArgFunc;
	AllArgsVariant		m_allArgsFunc;
};

vk::VkShaderStageFlags getPossibleGraphicsSubgroupStages (Context& context, const vk::VkShaderStageFlags testedStages);

tcu::TestStatus allStages (Context&						context,
						   vk::VkFormat					format,
						   const SSBOData*				extraData,
						   deUint32						extraDataCount,
						   const void*					internalData,
						   const VerificationFunctor&	checkResult,
						   const vk::VkShaderStageFlags	shaderStage);

tcu::TestStatus makeVertexFrameBufferTest (Context&			context,
										   vk::VkFormat		format,
										   const SSBOData*	extraData,
										   deUint32			extraDataCount,
										   const void*		internalData,
										   CheckResult		checkResult);

tcu::TestStatus makeFragmentFrameBufferTest (Context&				context,
											 vk::VkFormat			format,
											 const SSBOData*		extraData,
											 deUint32				extraDataCount,
											 const void*			internalData,
											 CheckResultFragment	checkResult);

tcu::TestStatus makeComputeTest (Context&				context,
								 vk::VkFormat			format,
								 const SSBOData*		inputs,
								 deUint32				inputsCount,
								 const void*			internalData,
								 CheckResultCompute		checkResult,
								 deUint32				requiredSubgroupSize = 0u,
								 const deUint32			pipelineShaderStageCreateFlags = 0u);

tcu::TestStatus makeMeshTest (Context&				context,
							  vk::VkFormat			format,
							  const SSBOData*		inputs,
							  deUint32				inputsCount,
							  const void*			internalData,
							  CheckResultCompute	checkResult,
							  deUint32				requiredSubgroupSize = 0u,
							  const deUint32		pipelineShaderStageCreateFlags = 0u);

/* Functions needed for VK_EXT_subgroup_size_control tests */
tcu::TestStatus makeTessellationEvaluationFrameBufferTestRequiredSubgroupSize (Context&							context,
																			   vk::VkFormat						format,
																			   const SSBOData*					extraData,
																			   deUint32							extraDataCount,
																			   const void*						internalData,
																			   CheckResult						checkResult,
																			   const vk::VkShaderStageFlags		shaderStage = vk::VK_SHADER_STAGE_ALL_GRAPHICS,
																			   const deUint32					tessShaderStageCreateFlags = 0u,
																			   const deUint32					requiredSubgroupSize = 0u);

tcu::TestStatus makeGeometryFrameBufferTestRequiredSubgroupSize (Context&			context,
																 vk::VkFormat		format,
																 const SSBOData*	extraData,
																 deUint32			extraDataCount,
																 const void*		internalData,
																 CheckResult		checkResult,
																 const deUint32		geometryShaderStageCreateFlags = 0u,
																 const deUint32		requiredSubgroupSize = 0u);

tcu::TestStatus allStagesRequiredSubgroupSize (Context&						context,
											   vk::VkFormat					format,
											   const SSBOData*				extraDatas,
											   deUint32						extraDatasCount,
											   const void*					internalData,
											   const VerificationFunctor&	checkResult,
											   const vk::VkShaderStageFlags	shaderStageTested,
											   const deUint32				vertexShaderStageCreateFlags,
											   const deUint32				tessellationControlShaderStageCreateFlags,
											   const deUint32				tessellationEvalShaderStageCreateFlags,
											   const deUint32				geometryShaderStageCreateFlags,
											   const deUint32				fragmentShaderStageCreateFlags,
											   const deUint32				requiredSubgroupSize[5]);

tcu::TestStatus makeVertexFrameBufferTestRequiredSubgroupSize (Context&				context,
															   vk::VkFormat			format,
															   const SSBOData*		extraData,
															   deUint32				extraDataCount,
															   const void*			internalData,
															   CheckResult			checkResult,
															   const deUint32		vertexShaderStageCreateFlags = 0u,
															   const deUint32		requiredSubgroupSize = 0u);

tcu::TestStatus makeFragmentFrameBufferTestRequiredSubgroupSize (Context&				context,
																 vk::VkFormat			format,
																 const SSBOData*		extraData,
																 deUint32				extraDataCount,
																 const void*			internalData,
																 CheckResultFragment	checkResult,
																 const deUint32			fragmentShaderStageCreateFlags = 0u,
																 const deUint32			requiredSubgroupSize = 0u);

tcu::TestStatus makeComputeTestRequiredSubgroupSize (Context&				context,
													 vk::VkFormat			format,
													 const SSBOData*		inputs,
													 deUint32				inputsCount,
													 const void*			internalData,
													 CheckResultCompute		checkResult,
													 const deUint32			pipelineShaderStageCreateFlags,
													 const deUint32			numWorkgroups[3],
													 const deBool			isRequiredSubgroupSize,
													 const deUint32			subgroupSize,
													 const deUint32			localSizesToTest[][3],
													 const deUint32			localSizesToTestCount);

tcu::TestStatus makeMeshTestRequiredSubgroupSize	(Context&				context,
													 vk::VkFormat			format,
													 const SSBOData*		inputs,
													 deUint32				inputsCount,
													 const void*			internalData,
													 CheckResultCompute		checkResult,
													 const deUint32			pipelineShaderStageCreateFlags,
													 const deUint32			numWorkgroups[3],
													 const deBool			isRequiredSubgroupSize,
													 const deUint32			subgroupSize,
													 const deUint32			localSizesToTest[][3],
													 const deUint32			localSizesToTestCount);

void supportedCheckShader (Context& context, const vk::VkShaderStageFlags shaderStage);

const std::vector<vk::VkFormat> getAllRayTracingFormats();

void addRayTracingNoSubgroupShader (vk::SourceCollections& programCollection);

vk::VkShaderStageFlags getPossibleRayTracingSubgroupStages (Context& context, const vk::VkShaderStageFlags testedStages);

tcu::TestStatus allRayTracingStages (Context&						context,
									 vk::VkFormat					format,
									 const SSBOData*				extraData,
									 deUint32						extraDataCount,
									 const void*					internalData,
									 const VerificationFunctor&		checkResult,
									 const vk::VkShaderStageFlags	shaderStage);

tcu::TestStatus allRayTracingStagesRequiredSubgroupSize (Context&						context,
														 vk::VkFormat					format,
														 const SSBOData*				extraDatas,
														 deUint32						extraDatasCount,
														 const void*					internalData,
														 const VerificationFunctor&		checkResult,
														 const vk::VkShaderStageFlags	shaderStageTested,
														 const deUint32					shaderStageCreateFlags[6],
														 const deUint32					requiredSubgroupSize[6]);
} // subgroups
} // vkt

#endif // _VKTSUBGROUPSTESTSUTILS_HPP
