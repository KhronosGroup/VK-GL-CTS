/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
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
*//*!
* \file
* \brief Vulkan Decriptor Indexing Tests
*//*--------------------------------------------------------------------*/

#include <algorithm>
#include <iostream>
#include <iterator>
#include <functional>
#include <sstream>
#include <utility>
#include <vector>

#include "vktDescriptorSetsIndexingTests.hpp"

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuSurface.hpp"
#include "tcuVectorUtil.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"

namespace vkt
{
namespace DescriptorIndexing
{
namespace
{
using namespace vk;
using tcu::UVec2;
using tcu::Vec4;
using tcu::TestStatus;
using tcu::PixelBufferAccess;
using tcu::Texture2D;

#define RESOLUTION_width	64
#define RESOLUTION_height	64
static const VkExtent3D RESOLUTION = { RESOLUTION_width, RESOLUTION_height, 1 };

#define MAX_DESCRIPTORS		4200
#define FUZZY_COMPARE		DE_FALSE
#define	CMP_THRESHOLD		0.02f

#define BINDING_Undefined				0
#define BINDING_UniformBuffer			1
#define BINDING_StorageBuffer			2
#define BINDING_UniformTexelBuffer		3
#define BINDING_StorageTexelBuffer		4
#define BINDING_Sampler					5
#define BINDING_SampledImage			6
#define BINDING_CombinedImageSampler	7
#define BINDING_UniformBufferDynamic	8
#define BINDING_StorageBufferDynamic	9
#define BINDING_InputAttachment			10
#define BINDING_StorageImage			11
#define BINDING_DescriptorEnumerator	12

static const VkExtent3D			smallImageExtent				= { 4, 4, 1 };
static const VkExtent3D			bigImageExtent					= { 32, 32, 1 };
static const VkDescriptorType	VK_DESCRIPTOR_TYPE_UNDEFINED = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;

template<deUint32 BindingNumber>
struct Binding
{
	static const deUint32	binding	= BindingNumber;
};

struct BindingUniformBuffer : Binding<BINDING_UniformBuffer>
{
	typedef struct
	{
		tcu::Vec4 c;
	} Data;
};

struct BindingStorageBuffer : Binding<BINDING_StorageBuffer>
{
	typedef struct
	{
		tcu::Vec4 cnew;
		tcu::Vec4 cold;
	} Data;
};

struct TestCaseParams
{
	VkDescriptorType	descriptorType;		// used only to distinguish test class instance
	VkShaderStageFlags	stageFlags;			// used only to build a proper program
	VkExtent3D			frameResolution;	// target frame buffer resolution
	bool				updateAfterBind;	// whether a test will use update after bind feature
	bool				calculateInLoop;	// perform calculation in a loop
	bool				usesMipMaps;		// this makes a sense and affects in image test cases only
	bool				minNonUniform;		// whether a test will use the minimum nonUniform decorations
	deBool				fuzzyComparison;	// if true then a test will use fuzzy comparison, otherwise float threshold
	float				thresholdValue;		// a threshold that will be used for both, float and fuzzy comparisons
};

struct TestParams
{
	VkShaderStageFlags	stageFlags;
	VkDescriptorType	descriptorType;
	deUint32			descriptorBinding;
	VkDescriptorType	additionalDescriptorType;
	deUint32			additionalDescriptorBinding;
	bool				copyBuffersToImages;
	bool				allowVertexStoring;
	VkExtent3D			frameResolution;
	bool				updateAfterBind;
	bool				calculateInLoop;
	bool				usesMipMaps;
	bool				minNonUniform;
	deBool				fuzzyComparison;
	float				thresholdValue;

	TestParams			(VkShaderStageFlags		stageFlags_,
						VkDescriptorType		descriptorType_,
						deUint32				descriptorBinding_,
						VkDescriptorType		additionalDescriptorType_,
						deUint32				additionalDescriptorBinding_,
						bool					copyBuffersToImages_,
						bool					allowVertexStoring_,
						const TestCaseParams&	caseParams)
		: stageFlags							(stageFlags_)
		, descriptorType						(descriptorType_)
		, descriptorBinding						(descriptorBinding_)
		, additionalDescriptorType				(additionalDescriptorType_)
		, additionalDescriptorBinding			(additionalDescriptorBinding_)
		, copyBuffersToImages					(copyBuffersToImages_)
		, allowVertexStoring					(allowVertexStoring_)
		, frameResolution						(caseParams.frameResolution)
		, updateAfterBind						(caseParams.updateAfterBind)
		, calculateInLoop						(caseParams.calculateInLoop)
		, usesMipMaps							(caseParams.usesMipMaps)
		, minNonUniform							(caseParams.minNonUniform)
		, fuzzyComparison						(caseParams.fuzzyComparison ? true : false)
		, thresholdValue						(caseParams.thresholdValue)
	{
	}
};

struct DescriptorEnumerator
{
	ut::BufferHandleAllocSp							buffer;
	ut::BufferViewSp								bufferView;
	VkDeviceSize									bufferSize;

	Move<VkDescriptorSetLayout>						descriptorSetLayout;
	Move<VkDescriptorPool>							descriptorPool;
	Move<VkDescriptorSet>							descriptorSet;

	void init(const vkt::Context& context, deUint32 vertexCount, deUint32 availableDescriptorCount);
	void update(const vkt::Context& context);
};

struct IterateCommonVariables
{
	// An amount of descriptors of a given type available on the platform
	deUint32										availableDescriptorCount;
	// An amount of valid descriptors that have connected a buffers to them
	deUint32										validDescriptorCount;
	// As the name suggests, sometimes it is used as invocationCount
	deUint32										vertexCount;
	VkRect2D										renderArea;
	VkDeviceSize									dataAlignment;
	deUint32										lowerBound;
	deUint32										upperBound;

	DescriptorEnumerator							descriptorEnumerator;

	ut::BufferHandleAllocSp							vertexAttributesBuffer;
	ut::BufferHandleAllocSp							descriptorsBuffer;
	std::vector<VkDescriptorBufferInfo>				descriptorsBufferInfos;
	std::vector<ut::BufferViewSp>					descriptorsBufferViews;
	std::vector<ut::ImageViewSp>					descriptorImageViews;
	std::vector<ut::SamplerSp>						descriptorSamplers;
	std::vector<ut::ImageHandleAllocSp>				descriptorsImages;
	ut::FrameBufferSp								frameBuffer;

	Move<VkDescriptorSetLayout>						descriptorSetLayout;
	Move<VkDescriptorPool>							descriptorPool;
	Move<VkDescriptorSet>							descriptorSet;
	Move<VkPipelineLayout>							pipelineLayout;
	Move<VkRenderPass>								renderPass;
	Move<VkPipeline>								pipeline;
	Move<VkCommandBuffer>							commandBuffer;
};

class CommonDescriptorInstance : public TestInstance
{
public:
								CommonDescriptorInstance		(Context&									context,
																const TestParams&							testParams);

	deUint32					computeAvailableDescriptorCount	(VkDescriptorType							descriptorType,
																 bool										reserveUniformTexelBuffer) const;

	Move<VkDescriptorSetLayout>	createDescriptorSetLayout		(bool										reserveUniformTexelBuffer,
																 deUint32&									descriptorCount) const;

	Move<VkDescriptorPool>		createDescriptorPool			(deUint32									descriptorCount) const;

	Move<VkDescriptorSet>		createDescriptorSet				(VkDescriptorPool							dsPool,
																 VkDescriptorSetLayout						dsLayout) const;

	struct attributes
	{
		typedef tcu::Vec4	vec4;
		typedef tcu::Vec2	vec2;
		typedef tcu::IVec4	ivec4;
		vec4			position;
		vec2			normalpos;
		ivec4			index;
		attributes& operator()(const vec4& pos)
		{
			position = pos;

			normalpos.x() = (pos.x() + 1.0f) / 2.0f;
			normalpos.y() = (pos.y() + 1.0f) / 2.0f;

			return *this;
		}
	};
	void						createVertexAttributeBuffer		(ut::BufferHandleAllocSp&					buffer,
																 deUint32									availableDescriptorCount) const;

	static std::string			substBinding					(deUint32									binding,
																 const char*								str,
																 deUint32									count = 0,
																 const char*								name = DE_NULL);

	static const char*			getVertexShaderProlog			(void);

	static const char*			getFragmentShaderProlog			(void);

	static const char*			getShaderEpilog					(void);

	static bool					performWritesInVertex			(VkDescriptorType							descriptorType);

	static bool					performWritesInVertex			(VkDescriptorType							descriptorType,
																 const Context&						        context);

	static std::string			getShaderAsm					(VkShaderStageFlagBits						shaderType,
																 const TestCaseParams&						testCaseParams,
																 bool										allowVertexStoring);

	static std::string			getShaderSource                 (VkShaderStageFlagBits						shaderType,
		                                                         const TestCaseParams&						testCaseParams,
		                                                         bool										allowVertexStoring);

	static std::string			getColorAccess					(VkDescriptorType							descriptorType,
																 const char*								indexVariableName,
																 bool										usesMipMaps);

	static std::string			getFragmentReturnSource			(const std::string&							colorAccess);

	static std::string			getFragmentLoopSource			(const std::string&							colorAccess1,
																 const std::string&							colorAccess2);

	virtual Move<VkRenderPass>	createRenderPass				(const IterateCommonVariables&				variables);

	struct push_constant
	{
		deInt32	lowerBound;
		deInt32	upperBound;
	};
	VkPushConstantRange			makePushConstantRange			(void) const;

	Move<VkPipelineLayout>		createPipelineLayout			(const std::vector<VkDescriptorSetLayout>&	descriptorSetLayouts) const;

	// Creates graphics or compute pipeline and appropriate shaders' modules according the testCaseParams.stageFlags
	// In the case of compute pipeline renderPass parameter is ignored.
	// Viewport will be created with a width and a height taken from testCaseParam.fragResolution.
	Move<VkPipeline>			createPipeline					(VkPipelineLayout							pipelineLayout,
																 VkRenderPass								renderPass);

	virtual void				createFramebuffer				(ut::FrameBufferSp&							frameBuffer,
																 VkRenderPass								renderPass,
																 const IterateCommonVariables&				variables);

	// Creates one big stagging buffer cutted out on chunks that can accomodate an element of elementSize size
	VkDeviceSize				createBuffers					(std::vector<VkDescriptorBufferInfo>&		bufferInfos,
																 ut::BufferHandleAllocSp&					buffer,
																 deUint32									elementCount,
																 deUint32									elementSize,
																 VkDeviceSize								alignment,
																 VkBufferUsageFlags							bufferUsage);

	// Creates and binds an imagesCount of images with given parameters.
	// Additionally creates stagging buffer for their data and PixelBufferAccess for particular images.
	VkDeviceSize				createImages					(std::vector<ut::ImageHandleAllocSp>&		images,
																 std::vector<VkDescriptorBufferInfo>&		bufferInfos,
																 ut::BufferHandleAllocSp&					buffer,
																 VkBufferUsageFlags							bufferUsage,
																 const VkExtent3D&							imageExtent,
																 VkFormat									imageFormat,
																 VkImageLayout								imageLayout,
																 deUint32									imageCount,
																 bool										withMipMaps = false);

	void						createBuffersViews				(std::vector<ut::BufferViewSp>&				views,
																 const std::vector<VkDescriptorBufferInfo>&	bufferInfos,
																 VkFormat									format);

	void						createImagesViews				(std::vector<ut::ImageViewSp>&				views,
																 const std::vector<ut::ImageHandleAllocSp>&	images,
																 VkFormat									format);

	virtual void				copyBuffersToImages				(IterateCommonVariables&					variables);

	virtual void				copyImagesToBuffers				(IterateCommonVariables&					variables);

	PixelBufferAccess			getPixelAccess					(deUint32									imageIndex,
																 const VkExtent3D&							imageExtent,
																 VkFormat									imageFormat,
																 const std::vector<VkDescriptorBufferInfo>&	bufferInfos,
																 const ut::BufferHandleAllocSp&				buffer,
																 deUint32									mipLevel = 0u) const;

	virtual void				createAndPopulateDescriptors	(IterateCommonVariables&					variables) = 0;

	virtual void				updateDescriptors				(IterateCommonVariables&					variables);

	virtual void				iterateCollectResults			(ut::UpdatablePixelBufferAccessPtr&			result,
																 const IterateCommonVariables&				variables,
																 bool										fromTest);

	void						iterateCommandSetup				(IterateCommonVariables&					variables);

	void						iterateCommandBegin				(IterateCommonVariables&					variables,
																bool										firstPass = true);

	void						iterateCommandEnd				(IterateCommonVariables&					variables,
																ut::UpdatablePixelBufferAccessPtr&	programResult,
																ut::UpdatablePixelBufferAccessPtr&	referenceResult,
																 bool										collectBeforeSubmit = true);

	bool						iterateVerifyResults			(IterateCommonVariables&					variables,
																	 ut::UpdatablePixelBufferAccessPtr	programResult,
																	 ut::UpdatablePixelBufferAccessPtr	referenceResult);

	Move<VkCommandBuffer>		createCmdBuffer					(void);

	void						commandBindPipeline				(VkCommandBuffer							commandBuffer,
																 VkPipeline									pipeline);

	void						commandBindVertexAttributes		(VkCommandBuffer							commandBuffer,
																 const ut::BufferHandleAllocSp&				vertexAttributesBuffer);

	void						commandBindDescriptorSets		(VkCommandBuffer							commandBuffer,
																 VkPipelineLayout							pipelineLayout,
																 VkDescriptorSet							descriptorSet,
																 deUint32									descriptorSetIndex);

	void						commandReadFrameBuffer			(ut::BufferHandleAllocSp&					content,
																 VkCommandBuffer							commandBuffer,
																 const ut::FrameBufferSp&					frameBuffer);
	ut::UpdatablePixelBufferAccessPtr
								commandReadFrameBuffer			(VkCommandBuffer							commandBuffer,
																 const ut::FrameBufferSp&					frameBuffer);

	Move<VkFence>				commandSubmit					(VkCommandBuffer							commandBuffer);

	virtual bool				verifyVertexWriteResults		(IterateCommonVariables&					variables);

protected:
	virtual tcu::TestStatus		iterate							(void);

protected:
	const VkDevice				m_vkd;
	const DeviceInterface&		m_vki;
	Allocator&					m_allocator;
	const VkQueue				m_queue;
	const deUint32				m_queueFamilyIndex;
	const Move<VkCommandPool>	m_commandPool;
	const VkFormat				m_colorFormat;
	const TestParams			m_testParams;
	static const tcu::Vec4		m_clearColor;
	const std::vector<float>	m_colorScheme;
	const deUint32				m_schemeSize;

private:

	Move<VkPipeline>			createGraphicsPipeline			(VkPipelineLayout							pipelineLayout,
																 VkRenderPass								renderPass);

	Move<VkPipeline>			createComputePipeline			(VkPipelineLayout							pipelineLayout);

	int							constructShaderModules			(void);

	static std::vector<float>	createColorScheme();

	Move<VkShaderModule>		m_vertexModule;
	Move<VkShaderModule>		m_fragmentModule;
	Move<VkShaderModule>		m_computeModule;
};
const tcu::Vec4 CommonDescriptorInstance::m_clearColor = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

void DescriptorEnumerator::init (const vkt::Context& context, deUint32 vertexCount, deUint32 availableDescriptorCount)
{
	const VkDevice					device = context.getDevice();
	const DeviceInterface&			deviceInterface = context.getDeviceInterface();

	const VkFormat					imageFormat = VK_FORMAT_R32G32B32A32_SINT;
	typedef ut::mapVkFormat2Type<imageFormat>::type pixelType;
	const VkDeviceSize				dataSize = vertexCount * sizeof(pixelType);
	const std::vector<deUint32>		primes = ut::generatePrimes(availableDescriptorCount);
	const deUint32					primeCount = static_cast<deUint32>(primes.size());

	std::vector<pixelType>	data(vertexCount);
	// e.g. 2,3,5,7,11,13,2,3,5,7,...
	for (deUint32 idx = 0; idx < vertexCount; ++idx)
	{
		data[idx].x() = static_cast<pixelType::Element>(primes[idx % primeCount]);
		data[idx].y() = static_cast<pixelType::Element>(idx);
	}

	bufferSize = ut::createBufferAndBind(buffer, context, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, dataSize);
	deMemcpy(buffer->alloc->getHostPtr(), data.data(), static_cast<size_t>(dataSize));

	const VkBufferViewCreateInfo bufferViewCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		0u,												// flags
		*(buffer.get()->buffer),						// buffer
		imageFormat,									// format
		0u,												// offset
		bufferSize,										// range
	};

	bufferView = ut::BufferViewSp(new Move<VkBufferView>(vk::createBufferView(deviceInterface, device, &bufferViewCreateInfo)));

	const VkDescriptorSetLayoutBinding	binding =
	{
		BINDING_DescriptorEnumerator,					// binding
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,		// descriptorType
		1u,												// descriptorCount
		VK_SHADER_STAGE_ALL,							// stageFlags
		DE_NULL,										// pImmutableSamplers
	};

	const VkDescriptorSetLayoutCreateInfo	layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,										// pNext
		0u,												// flags
		1u,												// bindingCount
		&binding,										// pBindings
	};

	descriptorSetLayout = vk::createDescriptorSetLayout(deviceInterface, device, &layoutCreateInfo);
	descriptorPool = DescriptorPoolBuilder().addType(binding.descriptorType)
		.build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const VkDescriptorSetAllocateInfo	dsAllocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// sType
		DE_NULL,										// pNext
		*descriptorPool,								// descriptorPool
		1u,												// descriptorSetCount
		&(*descriptorSetLayout)							// pSetLayouts
	};

	descriptorSet = vk::allocateDescriptorSet(deviceInterface, device, &dsAllocInfo);
}

void DescriptorEnumerator::update (const vkt::Context& context)
{
	const VkDescriptorBufferInfo bufferInfo =
	{
		*(buffer.get()->buffer),					// buffer
		0u,											// offset
		bufferSize,									// range
	};

	const VkWriteDescriptorSet writeInfo =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		DE_NULL,									// pNext
		*descriptorSet,								// dstSet
		BINDING_DescriptorEnumerator,				// dstBinding
		0u,											// dstArrayElement
		1u,											// descriptorCount
		VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,	// descriptorType
		DE_NULL,									// pImageInfo
		&bufferInfo,								// pBufferInfo
		&(**bufferView),							// pTexelBufferView
	};

	context.getDeviceInterface().updateDescriptorSets(context.getDevice(), 1u, &writeInfo, 0u, DE_NULL);
}

CommonDescriptorInstance::CommonDescriptorInstance					(Context&								context,
																	const TestParams&						testParams)
	: TestInstance		(context)
	, m_vkd				(context.getDevice())
	, m_vki				(context.getDeviceInterface())
	, m_allocator		(context.getDefaultAllocator())
	, m_queue			(context.getUniversalQueue())
	, m_queueFamilyIndex(context.getUniversalQueueFamilyIndex())
	, m_commandPool		(vk::createCommandPool(m_vki, m_vkd, (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT), m_queueFamilyIndex))
	, m_colorFormat		(VK_FORMAT_R32G32B32A32_SFLOAT)
	, m_testParams		(testParams)
	, m_colorScheme		(createColorScheme())
	, m_schemeSize		(static_cast<deUint32>(m_colorScheme.size()))
{
}

deUint32 CommonDescriptorInstance::computeAvailableDescriptorCount	(VkDescriptorType						descriptorType,
																	 bool									reserveUniformTexelBuffer) const
{
	DE_UNREF(descriptorType);
	const deUint32 vertexCount = m_testParams.frameResolution.width * m_testParams.frameResolution.height;
	const deUint32 availableDescriptorsOnDevice = ut::DeviceProperties(m_context).computeMaxPerStageDescriptorCount(m_testParams.descriptorType, m_testParams.updateAfterBind, reserveUniformTexelBuffer);
	return deMinu32(deMinu32(vertexCount, availableDescriptorsOnDevice), MAX_DESCRIPTORS);
}

Move<VkDescriptorSetLayout>	CommonDescriptorInstance::createDescriptorSetLayout (bool						reserveUniformTexelBuffer,
																				 deUint32&					descriptorCount) const
{
	descriptorCount = computeAvailableDescriptorCount(m_testParams.descriptorType, reserveUniformTexelBuffer);

	bool optional = (m_testParams.additionalDescriptorBinding != BINDING_Undefined) && (m_testParams.additionalDescriptorType != VK_DESCRIPTOR_TYPE_UNDEFINED);

	const VkDescriptorSetLayoutBinding	bindings[] =
	{
		{
			m_testParams.descriptorBinding,				// binding
			m_testParams.descriptorType,				// descriptorType
			descriptorCount,							// descriptorCount
			m_testParams.stageFlags,					// stageFlags
			DE_NULL,									// pImmutableSamplers
		},
		{
			m_testParams.additionalDescriptorBinding,	// binding
			m_testParams.additionalDescriptorType,		// descriptorType
			1,											// descriptorCount
			m_testParams.stageFlags,					// stageFlags
			DE_NULL,									// pImmutableSamplers
		}
	};

	const VkDescriptorBindingFlags	bindingFlagUpdateAfterBind =
		m_testParams.updateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0;

	const VkDescriptorBindingFlags bindingFlags[] =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | bindingFlagUpdateAfterBind,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | bindingFlagUpdateAfterBind
	};

	const VkDescriptorSetLayoutBindingFlagsCreateInfo	bindingCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		DE_NULL,
		optional ? 2u : 1u,	// bindingCount
		bindingFlags,		// pBindingFlags
	};

	const VkDescriptorSetLayoutCreateFlags	layoutCreateFlags =
		m_testParams.updateAfterBind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;

	const VkDescriptorSetLayoutCreateInfo	layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		&bindingCreateInfo,		// pNext
		layoutCreateFlags,		// flags
		optional ? 2u : 1u,		// bindingCount
		bindings,				// pBindings
	};

	return vk::createDescriptorSetLayout(m_vki, m_vkd, &layoutCreateInfo);
}

Move<VkDescriptorPool>	CommonDescriptorInstance::createDescriptorPool (deUint32							descriptorCount) const
{
	const VkDescriptorPoolCreateFlags pcf = m_testParams.updateAfterBind ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0;

	DescriptorPoolBuilder builder;

	builder.addType(m_testParams.descriptorType, descriptorCount);

	if (m_testParams.additionalDescriptorType != VK_DESCRIPTOR_TYPE_UNDEFINED && m_testParams.additionalDescriptorBinding != BINDING_Undefined)
	{
		builder.addType(m_testParams.additionalDescriptorType, 1);
	}

	return builder.build(m_vki, m_vkd, (VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | pcf), 1u);
}

Move<VkDescriptorSet> CommonDescriptorInstance::createDescriptorSet	(VkDescriptorPool						dsPool,
																	 VkDescriptorSetLayout					dsLayout) const
{
	const VkDescriptorSetAllocateInfo	dsAllocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// sType;
		DE_NULL,											// pNext;
		dsPool,												// descriptorPool;
		1u,													// descriptorSetCount
		&dsLayout											// pSetLayouts
	};

	return vk::allocateDescriptorSet(m_vki, m_vkd, &dsAllocInfo);
}

void CommonDescriptorInstance::createVertexAttributeBuffer			(ut::BufferHandleAllocSp&				buffer,
																	 deUint32								availableDescriptorCount) const
{
	float						xSize			= 0.0f;
	float						ySize			= 0.0f;

	const deUint32				invocationCount = m_testParams.frameResolution.width * m_testParams.frameResolution.height;
	const std::vector<Vec4>		vertices		= ut::createVertices(m_testParams.frameResolution.width, m_testParams.frameResolution.height, xSize, ySize);
	const std::vector<deUint32>	primes			= ut::generatePrimes(availableDescriptorCount);
	const deUint32				primeCount		= static_cast<deUint32>(primes.size());

	std::vector<attributes> data(vertices.size());
	std::transform(vertices.begin(), vertices.end(), data.begin(), attributes());

	for (deUint32 invIdx = 0; invIdx < invocationCount; ++invIdx)
	{
		// r: 2,3,5,7,11,13,2,3,5,7,...
		data[invIdx].index.x() = primes[invIdx % primeCount];

		// b: x index in texel coordinate
		data[invIdx].index.z() = invIdx % m_testParams.frameResolution.width;

		//a: y index in texel coordinate
		data[invIdx].index.w() = invIdx / m_testParams.frameResolution.width;
	}

	// g: 0,0,2,3,0,5,0,7,0,0,0,11,0,13,...
	for (deUint32 primeIdx = 0; primeIdx < primeCount; ++primeIdx)
	{
		const deUint32 prime = primes[primeIdx];
		DE_ASSERT(prime < invocationCount);
		data[prime].index.y() = prime;
	}

	const VkDeviceSize		dataSize = data.size() * sizeof(attributes);

	VkDeviceSize			deviceSize = ut::createBufferAndBind(buffer, m_context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, dataSize);

	deMemcpy(buffer->alloc->getHostPtr(), data.data(), static_cast<size_t>(deviceSize));

	vk::flushAlloc(m_vki, m_vkd, *buffer->alloc);
}

std::string CommonDescriptorInstance::substBinding					(deUint32								binding,
																	 const char*							str,
																	 deUint32								count,
																	 const char*							name)
{
	std::map<std::string, std::string> vars;
	vars["?"]	= de::toString(binding);
	vars["*"]	= (0 == count)		? ""		: de::toString(count);
	vars["VAR"]	= (DE_NULL == name)	? "data"	: name;
	return tcu::StringTemplate(str).specialize(vars);
}

const char* CommonDescriptorInstance::getVertexShaderProlog			(void)
{
	return
		"layout(location = 0) in  vec4  in_position;	\n"
		"layout(location = 1) in  vec2  in_normalpos;	\n"
		"layout(location = 2) in  ivec4 index;			\n"
		"layout(location = 0) out vec4  position;	\n"
		"layout(location = 1) out vec2  normalpos;	\n"
		"layout(location = 2) out int   vIndex;		\n"
		"layout(location = 3) out int   rIndex;		\n"
		"layout(location = 4) out int   gIndex;		\n"
		"layout(location = 5) out int   bIndex;		\n"
		"layout(location = 6) out int   aIndex;		\n"
		"void main()							\n"
		"{										\n"
		"    gl_PointSize = 0.2f;				\n"
		"    position = in_position;			\n"
		"    normalpos = in_normalpos;			\n"
		"    gl_Position = position;			\n"
		"    vIndex = gl_VertexIndex;			\n"
		"    rIndex = index.x;					\n"
		"    gIndex = index.y;					\n"
		"    bIndex = index.z;					\n"
		"    aIndex = index.w;					\n";
}

const char* CommonDescriptorInstance::getFragmentShaderProlog		(void)
{
	return
		"layout(location = 0) out vec4     FragColor;	\n"
		"layout(location = 0) in flat vec4 position;	\n"
		"layout(location = 1) in flat vec2 normalpos;	\n"
		"layout(location = 2) in flat int  vIndex;		\n"
		"layout(location = 3) in flat int  rIndex;		\n"
		"layout(location = 4) in flat int  gIndex;		\n"
		"layout(location = 5) in flat int  bIndex;		\n"
		"layout(location = 6) in flat int  aIndex;		\n"
		"void main()									\n"
		"{												\n";
}

const char* CommonDescriptorInstance::getShaderEpilog				(void)
{
	return "}											\n";
}

int	CommonDescriptorInstance::constructShaderModules				(void)
{
	int								result	= 0;
	tcu::TestLog&					log		= m_context.getTestContext().getLog();

	if (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		++result;
		const std::string name = ut::buildShaderName(VK_SHADER_STAGE_COMPUTE_BIT, m_testParams.descriptorType, m_testParams.updateAfterBind, m_testParams.calculateInLoop, m_testParams.minNonUniform, false);
		m_computeModule = vk::createShaderModule(m_vki, m_vkd, m_context.getBinaryCollection().get(name), (VkShaderModuleCreateFlags)0);
	}
	if (m_testParams.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		++result;
		const std::string name = ut::buildShaderName(VK_SHADER_STAGE_FRAGMENT_BIT, m_testParams.descriptorType, m_testParams.updateAfterBind, m_testParams.calculateInLoop, m_testParams.minNonUniform, m_testParams.allowVertexStoring);
		m_fragmentModule = vk::createShaderModule(m_vki, m_vkd, m_context.getBinaryCollection().get(name), (VkShaderModuleCreateFlags)0);
		log << tcu::TestLog::Message << "Finally used fragment shader: " << name << '\n' << tcu::TestLog::EndMessage;
	}
	if (m_testParams.stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
	{
		++result;
		const std::string name = ut::buildShaderName(VK_SHADER_STAGE_VERTEX_BIT, m_testParams.descriptorType, m_testParams.updateAfterBind, m_testParams.calculateInLoop, m_testParams.minNonUniform, m_testParams.allowVertexStoring);
		m_vertexModule = vk::createShaderModule(m_vki, m_vkd, m_context.getBinaryCollection().get(name), (VkShaderModuleCreateFlags)0);
		log << tcu::TestLog::Message << "Finally used vertex shader: " << name << '\n' << tcu::TestLog::EndMessage;
	}

	DE_ASSERT(result > 0);

	return result;
}

Move<VkRenderPass> CommonDescriptorInstance::createRenderPass		(const IterateCommonVariables&			variables)
{
	DE_UNREF(variables);
	if ((m_testParams.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) || (m_testParams.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT))
	{
		// Use VK_ATTACHMENT_LOAD_OP_LOAD to make the utility function select initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		return vk::makeRenderPass(m_vki, m_vkd, m_colorFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
	}
	return Move<VkRenderPass>();
}

VkPushConstantRange CommonDescriptorInstance::makePushConstantRange	(void) const
{
	const VkPushConstantRange pcr =
	{
		m_testParams.stageFlags,							// stageFlags
		0u,													// offset
		static_cast<deUint32>(sizeof(push_constant))		// size
	};
	return pcr;
}

Move<VkPipelineLayout> CommonDescriptorInstance::createPipelineLayout (const std::vector<VkDescriptorSetLayout>&	descriptorSetLayouts) const
{
	const VkPushConstantRange pcr = makePushConstantRange();

	const VkPipelineLayoutCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// sType
		DE_NULL,											// pNext
		(VkPipelineLayoutCreateFlags)0,						// flags
		static_cast<deUint32>(descriptorSetLayouts.size()),	// setLayoutCount
		descriptorSetLayouts.data(),						// pSetLayouts;
		m_testParams.calculateInLoop ? 1u : 0u,				// pushConstantRangeCount
		m_testParams.calculateInLoop ? &pcr : DE_NULL,		// pPushConstantRanges
	};

	return vk::createPipelineLayout(m_vki, m_vkd, &createInfo);
}

void CommonDescriptorInstance::createFramebuffer					(ut::FrameBufferSp&							frameBuffer,
																	 VkRenderPass								renderPass,
																	 const IterateCommonVariables&				variables)
{
	DE_UNREF(variables);
	ut::createFrameBuffer(frameBuffer, m_context, m_testParams.frameResolution, m_colorFormat, renderPass);
}

Move<VkPipeline> CommonDescriptorInstance::createPipeline			(VkPipelineLayout							pipelineLayout,
																	 VkRenderPass								renderPass)
{	DE_ASSERT(VK_SHADER_STAGE_ALL != m_testParams.stageFlags);

	constructShaderModules();

	return (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
		? createComputePipeline(pipelineLayout)
		: createGraphicsPipeline(pipelineLayout, renderPass);
}

Move<VkPipeline> CommonDescriptorInstance::createComputePipeline	(VkPipelineLayout							pipelineLayout)
{
	const VkPipelineShaderStageCreateInfo	shaderStaegCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,								// pNext
		(VkPipelineShaderStageCreateFlags)0,	// flags
		VK_SHADER_STAGE_COMPUTE_BIT,			// stage
		*m_computeModule,						// module
		"main",									// pName
		(VkSpecializationInfo*)DE_NULL			// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,								// pNext
		0u,										// flags
		shaderStaegCreateInfo,					// stage
		pipelineLayout,							// layout
		(VkPipeline)0,							// basePipelineHandle
		0u,										// basePipelineIndex
	};
	return vk::createComputePipeline(m_vki, m_vkd, (VkPipelineCache)0u, &pipelineCreateInfo);
}

Move<VkPipeline> CommonDescriptorInstance::createGraphicsPipeline	(VkPipelineLayout							pipelineLayout,
																	 VkRenderPass								renderPass)
{
	const VkVertexInputBindingDescription			bindingDescriptions[] =
	{
		{
			0u,													// binding
			sizeof(attributes),									// stride
			VK_VERTEX_INPUT_RATE_VERTEX,						// inputRate
		},
	};

	const VkVertexInputAttributeDescription			attributeDescriptions[] =
	{
		{
			0u,													// location
			0u,													// binding
			ut::mapType2vkFormat<attributes::vec4>::value,		// format
			0u													// offset
		},														// @in_position
		{
			1u,													// location
			0u,													// binding
			ut::mapType2vkFormat<attributes::vec2>::value,		// format
			static_cast<deUint32>(sizeof(attributes::vec4))		// offset
		},														// @normalpos
		{
			2u,													// location
			0u,													// binding
			ut::mapType2vkFormat<attributes::ivec4>::value,		// format
			static_cast<deUint32>(sizeof(attributes::vec2)
								+ sizeof(attributes::vec4))		// offset
		},														// @index
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0,	// flags
		DE_LENGTH_OF_ARRAY(bindingDescriptions),	// vertexBindingDescriptionCount
		bindingDescriptions,						// pVertexBindingDescriptions
		DE_LENGTH_OF_ARRAY(attributeDescriptions),	// vertexAttributeDescriptionCount
		attributeDescriptions						// pVertexAttributeDescriptions
	};

	const	VkDynamicState							dynamicStates[]				=
	{
		VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,  // sType
		DE_NULL,											   // pNext
		0u,													   // flags
		DE_LENGTH_OF_ARRAY(dynamicStates),					   // dynamicStateCount
		dynamicStates										   // pDynamicStates
	};

	const std::vector<VkViewport>	viewports	(1, makeViewport(m_testParams.frameResolution.width, m_testParams.frameResolution.height));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(0u, 0u));

	DE_ASSERT(m_vertexModule && m_fragmentModule);

	return vk::makeGraphicsPipeline(
		m_vki,											// vk
		m_vkd,											// device
		pipelineLayout,									// pipelineLayout
		*m_vertexModule,								// vertexShaderModule
		DE_NULL,										// tessellationControlModule
		DE_NULL,										// tessellationEvalModule
		DE_NULL,										// geometryShaderModule
		*m_fragmentModule,								// fragmentShaderModule
		renderPass,										// renderPass
		viewports,										// viewports
		scissors,										// scissors
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,				// topology
		0U,												// subpass
		0U,												// patchControlPoints
		&vertexInputStateCreateInfo,					// vertexInputStateCreateInfo
		nullptr,										// rasterizationStateCreateInfo
		nullptr,										// multisampleStateCreateInfo
		nullptr,										// depthStencilStateCreateInfo
		nullptr,										// colorBlendStateCreateInfo
		&dynamicStateCreateInfo);						// dynamicStateCreateInfo
}

VkDeviceSize CommonDescriptorInstance::createBuffers				(std::vector<VkDescriptorBufferInfo>&		bufferInfos,
																	 ut::BufferHandleAllocSp&					buffer,
																	 deUint32									elementCount,
																	 deUint32									elementSize,
																	 VkDeviceSize								alignment,
																	 VkBufferUsageFlags							bufferUsage)
{
	const VkDeviceSize	roundedSize = deAlign64(elementSize, alignment);
	VkDeviceSize		bufferSize	= ut::createBufferAndBind(buffer, m_context, bufferUsage, (roundedSize * elementCount));

	for (deUint32 elementIdx = 0; elementIdx < elementCount; ++elementIdx)
	{
		const VkDescriptorBufferInfo bufferInfo =
		{
			*buffer.get()->buffer,		//buffer;
			elementIdx * roundedSize,	//offset;
			elementSize,				// range;

		};
		bufferInfos.push_back(bufferInfo);
	}

	return bufferSize;
}

VkDeviceSize CommonDescriptorInstance::createImages					(std::vector<ut::ImageHandleAllocSp>&		images,
																	 std::vector<VkDescriptorBufferInfo>&		bufferInfos,
																	 ut::BufferHandleAllocSp&					buffer,
																	 VkBufferUsageFlags							bufferUsage,
																	 const VkExtent3D&							imageExtent,
																	 VkFormat									imageFormat,
																	 VkImageLayout								imageLayout,
																	 deUint32									imageCount,
																	 bool										withMipMaps)

{
	const deUint32		imageSize	= ut::computeImageSize(imageExtent, imageFormat, withMipMaps);

	const VkDeviceSize	bufferSize	= createBuffers(bufferInfos, buffer, imageCount, imageSize, sizeof(tcu::Vec4), bufferUsage);

	for (deUint32 imageIdx = 0; imageIdx < imageCount; ++imageIdx)
	{
		ut::ImageHandleAllocSp image;
		ut::createImageAndBind(image, m_context, imageFormat, imageExtent, imageLayout, withMipMaps);
		images.push_back(image);
	}

	return bufferSize;
}

void CommonDescriptorInstance::createBuffersViews					(std::vector<ut::BufferViewSp>&				views,
																	 const std::vector<VkDescriptorBufferInfo>&	bufferInfos,
																	 VkFormat									format)
{
	const deUint32 infoCount = static_cast<deUint32>(bufferInfos.size());
	for (deUint32 infoIdx = 0; infoIdx < infoCount; ++infoIdx)
	{
		const VkDescriptorBufferInfo&	bufferInfo = bufferInfos[infoIdx];
		const VkBufferViewCreateInfo	bufferViewInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			(VkBufferViewCreateFlags)0,					// flags
			bufferInfo.buffer,							// buffer
			format,										// format
			bufferInfo.offset,							// offset
			bufferInfo.range							// range;
		};
		views.push_back(ut::BufferViewSp(new Move<VkBufferView>(vk::createBufferView(m_vki, m_vkd, &bufferViewInfo))));
	}
}

void CommonDescriptorInstance::createImagesViews					(std::vector<ut::ImageViewSp>&				views,
																	 const std::vector<ut::ImageHandleAllocSp>&	images,
																	 VkFormat									format)
{
	const deUint32 imageCount = static_cast<deUint32>(images.size());
	for (deUint32 imageIdx = 0; imageIdx < imageCount; ++imageIdx)
	{
		const VkImageViewCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			(VkImageViewCreateFlags)0,					// flags
			*images[imageIdx]->image,					// image
			VK_IMAGE_VIEW_TYPE_2D,						// viewType
			format,										// format
			vk::makeComponentMappingRGBA(),				// components
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				// aspectMask
				(deUint32)0,							// baseMipLevel
				images[imageIdx]->levels,				// mipLevels
				(deUint32)0,							// baseArrayLayer
				(deUint32)1u,							// arraySize
			},
		};
		views.push_back(ut::ImageViewSp(new Move<VkImageView>(vk::createImageView(m_vki, m_vkd, &createInfo))));
	}
}

void CommonDescriptorInstance::copyBuffersToImages					(IterateCommonVariables&					variables)
{
	const deUint32 infoCount = static_cast<deUint32>(variables.descriptorsBufferInfos.size());
	DE_ASSERT(variables.descriptorsImages.size() == infoCount);
	const VkPipelineStageFlagBits dstStageMask = (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
		? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		: VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	for (deUint32 infoIdx = 0; infoIdx < infoCount; ++infoIdx)
	{
		ut::recordCopyBufferToImage(
			*variables.commandBuffer,						// commandBuffer
			m_vki,											// interface
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// srcStageMask
			dstStageMask,									// dstStageMask
			variables.descriptorsBufferInfos[infoIdx],		// bufferInfo
			*(variables.descriptorsImages[infoIdx]->image),	// image
			variables.descriptorsImages[infoIdx]->extent,	// imageExtent
			variables.descriptorsImages[infoIdx]->format,	// imageFormat
			VK_IMAGE_LAYOUT_UNDEFINED,						// oldImageLayout
			VK_IMAGE_LAYOUT_GENERAL,						// newImageLayout
			variables.descriptorsImages[infoIdx]->levels);	// mipLevelCount
	}
}

void CommonDescriptorInstance::copyImagesToBuffers					(IterateCommonVariables&					variables)
{
	const deUint32 infoCount = static_cast<deUint32>(variables.descriptorsBufferInfos.size());
	DE_ASSERT(variables.descriptorsImages.size() == infoCount);
	const VkPipelineStageFlagBits srcStageMask = (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
		? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		: VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	for (deUint32 infoIdx = 0; infoIdx < infoCount; ++infoIdx)
	{
		ut::recordCopyImageToBuffer(
			*variables.commandBuffer,						// commandBuffer
			m_vki,											// interface
			srcStageMask,									// srcStageMask
			VK_PIPELINE_STAGE_HOST_BIT,						// dstStageMask
			*(variables.descriptorsImages[infoIdx]->image),	// image
			variables.descriptorsImages[infoIdx]->extent,	// imageExtent
			variables.descriptorsImages[infoIdx]->format,	// imageFormat
			VK_IMAGE_LAYOUT_GENERAL,						// oldImageLayout
			VK_IMAGE_LAYOUT_GENERAL,						// newImageLayout
			variables.descriptorsBufferInfos[infoIdx]);		// bufferInfo
	}
}

PixelBufferAccess CommonDescriptorInstance::getPixelAccess			(deUint32									imageIndex,
																	 const VkExtent3D&							imageExtent,
																	 VkFormat									imageFormat,
																	 const std::vector<VkDescriptorBufferInfo>&	bufferInfos,
																	 const ut::BufferHandleAllocSp&				buffer,
																	 deUint32									mipLevel) const
{
	DE_ASSERT(bufferInfos[imageIndex].buffer == *buffer.get()->buffer);
	DE_ASSERT(ut::computeImageSize(imageExtent, imageFormat, true, (mipLevel ? ut::maxDeUint32 : 0)) <= bufferInfos[imageIndex].range);
	DE_ASSERT(imageExtent.width		>> mipLevel);
	DE_ASSERT(imageExtent.height	>> mipLevel);

	deUint32 mipOffset = 0;

	for (deUint32 level = 0; mipLevel && level < mipLevel; ++level)
	{
		mipOffset += ut::computeImageSize(imageExtent, imageFormat, true, level);
	}

	unsigned char* hostPtr	= static_cast<unsigned char*>(buffer->alloc->getHostPtr());
	unsigned char* data = hostPtr + bufferInfos[imageIndex].offset + mipOffset;
	return tcu::PixelBufferAccess(vk::mapVkFormat(imageFormat), (imageExtent.width >> mipLevel), (imageExtent.height >> mipLevel), imageExtent.depth, data);
}

void CommonDescriptorInstance::updateDescriptors					(IterateCommonVariables&					variables)
{
	const std::vector<deUint32>	primes = ut::generatePrimes(variables.availableDescriptorCount);
	const deUint32				primeCount = static_cast<deUint32>(primes.size());

	for (deUint32 primeIdx = 0; primeIdx < primeCount; ++primeIdx)
	{
		const VkDescriptorBufferInfo*	pBufferInfo			= DE_NULL;
		const VkDescriptorImageInfo*	pImageInfo			= DE_NULL;
		const VkBufferView*				pTexelBufferView	= DE_NULL;

		VkDescriptorImageInfo		imageInfo =
		{
			static_cast<VkSampler>(0),
			static_cast<VkImageView>(0),
			VK_IMAGE_LAYOUT_GENERAL
		};

		switch (m_testParams.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			{
				pBufferInfo = &variables.descriptorsBufferInfos[primeIdx];
				switch (m_testParams.descriptorType)
				{
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					pTexelBufferView = &(**variables.descriptorsBufferViews[primeIdx]);
					break;
				default:
					break;
				}
			}
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLER:
			imageInfo.sampler = **variables.descriptorSamplers[primeIdx];
			pImageInfo = &imageInfo;
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			imageInfo.imageView = **variables.descriptorImageViews[primeIdx];
			pImageInfo = &imageInfo;
			break;

		default:	break;
		}

		const VkWriteDescriptorSet writeInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// sType
			DE_NULL,										// pNext
			*variables.descriptorSet,						// descriptorSet
			m_testParams.descriptorBinding,					// descriptorBinding;
			primes[primeIdx],								// elementIndex
			1u,												// descriptorCount
			m_testParams.descriptorType,					// descriptorType
			pImageInfo,										// pImageInfo
			pBufferInfo,									// pBufferInfo
			pTexelBufferView								// pTexelBufferView
		};

		m_vki.updateDescriptorSets(m_vkd, 1u, &writeInfo, 0u, DE_NULL);
	}
}

void CommonDescriptorInstance::iterateCommandSetup					(IterateCommonVariables&					variables)
{
	variables.dataAlignment				= 0;

	variables.renderArea.offset.x		= 0;
	variables.renderArea.offset.y		= 0;
	variables.renderArea.extent.width	= m_testParams.frameResolution.width;
	variables.renderArea.extent.height	= m_testParams.frameResolution.height;

	variables.vertexCount				= m_testParams.frameResolution.width * m_testParams.frameResolution.height;

	variables.lowerBound				= 0;
	variables.upperBound				= variables.vertexCount;

	variables.descriptorSetLayout		= createDescriptorSetLayout(m_testParams.calculateInLoop, variables.availableDescriptorCount);
	variables.validDescriptorCount		= ut::computePrimeCount(variables.availableDescriptorCount);
	variables.descriptorPool			= createDescriptorPool(variables.availableDescriptorCount);
	variables.descriptorSet				= createDescriptorSet(*variables.descriptorPool, *variables.descriptorSetLayout);

	std::vector<VkDescriptorSetLayout>	descriptorSetLayouts;
	descriptorSetLayouts.push_back(*variables.descriptorSetLayout);
	if (m_testParams.calculateInLoop)
	{
		variables.descriptorEnumerator.init(m_context, variables.vertexCount, variables.availableDescriptorCount);
		descriptorSetLayouts.push_back(*variables.descriptorEnumerator.descriptorSetLayout);
	}

	variables.pipelineLayout			= createPipelineLayout(descriptorSetLayouts);

	createAndPopulateDescriptors		(variables);

	variables.renderPass				= createRenderPass(variables);
	variables.pipeline					= createPipeline(*variables.pipelineLayout, *variables.renderPass);

	variables.commandBuffer				= createCmdBuffer();

	if ((m_testParams.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) || (m_testParams.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT))
	{
		createVertexAttributeBuffer		(variables.vertexAttributesBuffer, variables.availableDescriptorCount);
		createFramebuffer				(variables.frameBuffer, *variables.renderPass, variables);
	}

	if (m_testParams.calculateInLoop)
	{
		variables.descriptorEnumerator.update(m_context);
	}

	if (!m_testParams.updateAfterBind)
	{
		updateDescriptors				(variables);
	}

}

void CommonDescriptorInstance::iterateCommandBegin					(IterateCommonVariables&					variables,	bool firstPass)
{
	vk::beginCommandBuffer				(m_vki, *variables.commandBuffer);

	// Clear color attachment, and transition it to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	if ((m_testParams.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) || (m_testParams.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT))
	{
		if (firstPass)
		{
			const VkImageMemoryBarrier preImageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
				DE_NULL,											// const void*			pNext
				0u,													// VkAccessFlags		srcAccessMask
				VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
				*variables.frameBuffer->image->image,				// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					VK_REMAINING_MIP_LEVELS,				// uint32_t				mipLevels,
					0u,										// uint32_t				baseArray
					VK_REMAINING_ARRAY_LAYERS,				// uint32_t				arraySize
				}
			};

			m_vki.cmdPipelineBarrier(*variables.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
									(VkDependencyFlags)0,
									0, (const VkMemoryBarrier*)DE_NULL,
									0, (const VkBufferMemoryBarrier*)DE_NULL,
									1, &preImageBarrier);

			const VkClearColorValue	clearColorValue		= makeClearValueColor(m_clearColor).color;

			m_vki.cmdClearColorImage(*variables.commandBuffer, *variables.frameBuffer->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &preImageBarrier.subresourceRange);

			const VkImageMemoryBarrier postImageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
				DE_NULL,											// const void*			pNext
				VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		srcAccessMask
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
				*variables.frameBuffer->image->image,				// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					VK_REMAINING_MIP_LEVELS,				// uint32_t				mipLevels,
					0u,										// uint32_t				baseArray
					VK_REMAINING_ARRAY_LAYERS,				// uint32_t				arraySize
				}
			};

			m_vki.cmdPipelineBarrier(*variables.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									(VkDependencyFlags)0,
									0, (const VkMemoryBarrier*)DE_NULL,
									0, (const VkBufferMemoryBarrier*)DE_NULL,
									1, &postImageBarrier);

		}
	}

	if (m_testParams.calculateInLoop)
	{
		deRandom rnd;
		deRandom_init(&rnd, static_cast<deUint32>(m_testParams.descriptorType));
		const deUint32 quarter = variables.vertexCount / 4;

		variables.lowerBound			= deRandom_getUint32(&rnd) % quarter;
		variables.upperBound			= (deRandom_getUint32(&rnd) % quarter) + (3 * quarter);

		const push_constant pc =
		{
			static_cast<deInt32>(variables.lowerBound),
			static_cast<deInt32>(variables.upperBound)
		};

		m_vki.cmdPushConstants(*variables.commandBuffer, *variables.pipelineLayout, m_testParams.stageFlags, 0u, static_cast<deUint32>(sizeof(pc)), &pc);
	}

	if ((m_testParams.stageFlags & VK_SHADER_STAGE_VERTEX_BIT) || (m_testParams.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT))
	{
		commandBindVertexAttributes		(*variables.commandBuffer, variables.vertexAttributesBuffer);
	}

	if (m_testParams.calculateInLoop)
	{
		commandBindDescriptorSets(*variables.commandBuffer, *variables.pipelineLayout, *variables.descriptorEnumerator.descriptorSet, 1);
	}

	if (!ut::isDynamicDescriptor(m_testParams.descriptorType))
	{
		commandBindDescriptorSets		(*variables.commandBuffer, *variables.pipelineLayout, *variables.descriptorSet, 0);
	}

	commandBindPipeline					(*variables.commandBuffer, *variables.pipeline);
}

tcu::TestStatus	CommonDescriptorInstance::iterate					(void)
{
	IterateCommonVariables	v;
	ut::UpdatablePixelBufferAccessPtr	programResult;
	ut::UpdatablePixelBufferAccessPtr	referenceResult;

	bool firstPass = true;

	iterateCommandSetup		(v);

	v.renderArea.extent.width	= m_testParams.frameResolution.width/4;
	v.renderArea.extent.height	= m_testParams.frameResolution.height/4;

	for (int x = 0; x < 4; x++)
		for (int y= 0; y < 4; y++)
		{
			iterateCommandBegin		(v, firstPass);

			if (true == firstPass && true == m_testParams.copyBuffersToImages)
			{
				copyBuffersToImages	(v);
			}

			firstPass = false;

			if (true == m_testParams.updateAfterBind)
			{
				updateDescriptors	(v);
			}

			v.renderArea.offset.x		= x * m_testParams.frameResolution.width/4;
			v.renderArea.offset.y		= y * m_testParams.frameResolution.height/4;

			vk::VkRect2D scissor = makeRect2D(v.renderArea.offset.x, v.renderArea.offset.y, v.renderArea.extent.width, v.renderArea.extent.height);
			m_vki.cmdSetScissor(*v.commandBuffer, 0u, 1u, &scissor);

			vk::beginRenderPass		(m_vki, *v.commandBuffer, *v.renderPass, *v.frameBuffer->buffer, v.renderArea, m_clearColor);
			m_vki.cmdDraw			(*v.commandBuffer, v.vertexCount, 1u, 0u, 0u);
			vk::endRenderPass		(m_vki, *v.commandBuffer);

			iterateCommandEnd(v, programResult, referenceResult);
			programResult->invalidate();
		}

	return ( iterateVerifyResults(v, programResult, referenceResult) ? tcu::TestStatus::pass : tcu::TestStatus::fail)("");
}

std::vector<float> CommonDescriptorInstance::createColorScheme		(void)
{
	std::vector<float> cs;
	int divider = 2;
	for (int i = 0; i < 10; ++i)
	{
		cs.push_back(1.0f / float(divider));
		divider *= 2;
	}
	return cs;
}

void CommonDescriptorInstance::iterateCommandEnd					(IterateCommonVariables&					variables,
																	 ut::UpdatablePixelBufferAccessPtr&	programResult,
																	 ut::UpdatablePixelBufferAccessPtr&	referenceResult,
																	 bool										collectBeforeSubmit)
{
	if (collectBeforeSubmit)
	{
		iterateCollectResults(programResult, variables, true);
		iterateCollectResults(referenceResult, variables, false);
	}

	VK_CHECK(m_vki.endCommandBuffer(*variables.commandBuffer));
	Move<VkFence> fence = commandSubmit(*variables.commandBuffer);
	m_vki.waitForFences(m_vkd, 1, &(*fence), DE_TRUE, ~0ull);

	if (false == collectBeforeSubmit)
	{
		iterateCollectResults(programResult, variables, true);
		iterateCollectResults(referenceResult, variables, false);
	}
}

bool CommonDescriptorInstance::iterateVerifyResults			(IterateCommonVariables&					variables,
																	 ut::UpdatablePixelBufferAccessPtr	programResult,
																	 ut::UpdatablePixelBufferAccessPtr	referenceResult)
{
	bool result = false;
	if (m_testParams.fuzzyComparison)
	{
		result = tcu::fuzzyCompare(m_context.getTestContext().getLog(),
			"Fuzzy Compare", "Comparison result", *referenceResult.get(), *programResult.get(), 0.02f, tcu::COMPARE_LOG_EVERYTHING);
	}
	else
	{
		result = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
			"Float Threshold Compare", "Comparison result", *referenceResult.get(), *programResult.get(), tcu::Vec4(0.02f, 0.02f, 0.02f, 0.02f), tcu::COMPARE_LOG_EVERYTHING);
	}

	if (m_testParams.allowVertexStoring)
	{
		result = verifyVertexWriteResults(variables);
	}

	return result;
}

void CommonDescriptorInstance::iterateCollectResults				(ut::UpdatablePixelBufferAccessPtr&			result,
																	 const IterateCommonVariables&				variables,
																	 bool										fromTest)
{
	if (fromTest)
	{
		result = commandReadFrameBuffer(*variables.commandBuffer, variables.frameBuffer);
	}
	else
	{
		result = ut::UpdatablePixelBufferAccessPtr(new ut::PixelBufferAccessAllocation(vk::mapVkFormat(m_colorFormat), m_testParams.frameResolution));

		for (deUint32 y = 0, pixelNum = 0; y < m_testParams.frameResolution.height; ++y)
		{
			for (deUint32 x = 0; x < m_testParams.frameResolution.width; ++x, ++pixelNum)
			{
				const float component = m_colorScheme[(pixelNum % variables.validDescriptorCount) % m_schemeSize];
				result->setPixel(tcu::Vec4(component, component, component, 1.0f), x, y);
			}
		}
	}
}

Move<VkCommandBuffer> CommonDescriptorInstance::createCmdBuffer		(void)
{
	return vk::allocateCommandBuffer(m_vki, m_vkd, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

Move<VkFence> CommonDescriptorInstance::commandSubmit				(VkCommandBuffer							cmd)
{
	Move<VkFence>	fence(vk::createFence(m_vki, m_vkd));

	const VkSubmitInfo	submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
		DE_NULL,											// pNext
		0u,													// waitSemaphoreCount
		static_cast<VkSemaphore*>(DE_NULL),					// pWaitSemaphores
		static_cast<const VkPipelineStageFlags*>(DE_NULL),	// pWaitDstStageMask
		1u,													// commandBufferCount
		&cmd,												// pCommandBuffers
		0u,													// signalSemaphoreCount
		static_cast<VkSemaphore*>(DE_NULL)					// pSignalSemaphores
	};

	VK_CHECK(m_vki.queueSubmit(m_queue, 1u, &submitInfo, *fence));

	return fence;
}

bool CommonDescriptorInstance::verifyVertexWriteResults(IterateCommonVariables&					variables)
{
	DE_UNREF(variables);
	return true;
}

void CommonDescriptorInstance::commandBindPipeline					(VkCommandBuffer							commandBuffer,
																	 VkPipeline									pipeline)
{
	const VkPipelineBindPoint pipelineBindingPoint = (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
	m_vki.cmdBindPipeline(commandBuffer, pipelineBindingPoint, pipeline);
}

void CommonDescriptorInstance::commandBindVertexAttributes			(VkCommandBuffer							commandBuffer,
																	 const ut::BufferHandleAllocSp&				vertexAttributesBuffer)
{
	const VkDeviceSize	offsets[] = { 0u };
	const VkBuffer		buffers[] = { *vertexAttributesBuffer->buffer };
	m_vki.cmdBindVertexBuffers(commandBuffer, 0u, 1u, buffers, offsets);
}

void CommonDescriptorInstance::commandBindDescriptorSets			(VkCommandBuffer							commandBuffer,
																	 VkPipelineLayout							pipelineLayout,
																	 VkDescriptorSet							descriptorSet,
																	 deUint32									descriptorSetIndex)
{
	const VkPipelineBindPoint pipelineBindingPoint = (m_testParams.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
	m_vki.cmdBindDescriptorSets(commandBuffer, pipelineBindingPoint, pipelineLayout, descriptorSetIndex, 1u, &descriptorSet, 0u, static_cast<deUint32*>(DE_NULL));
}

ut::UpdatablePixelBufferAccessPtr
CommonDescriptorInstance::commandReadFrameBuffer					(VkCommandBuffer							commandBuffer,
																	 const ut::FrameBufferSp&					frameBuffer)
{
	ut::BufferHandleAllocSp frameBufferContent;
	commandReadFrameBuffer(frameBufferContent, commandBuffer, frameBuffer);
	return ut::UpdatablePixelBufferAccessPtr(new ut::PixelBufferAccessBuffer(
		m_vkd, m_vki, vk::mapVkFormat(m_colorFormat), m_testParams.frameResolution,
		de::SharedPtr< Move<VkBuffer> >(new Move<VkBuffer>(frameBufferContent->buffer)),
		de::SharedPtr< de::MovePtr<Allocation> >(new de::MovePtr<Allocation>(frameBufferContent->alloc))));
}

void CommonDescriptorInstance::commandReadFrameBuffer				(ut::BufferHandleAllocSp&					content,
																	 VkCommandBuffer							commandBuffer,
																	 const ut::FrameBufferSp&					frameBuffer)
{
	Move<VkBuffer>			buffer;
	de::MovePtr<Allocation>	allocation;

	const VkDeviceSize bufferSize = ut::computeImageSize(frameBuffer->image);

	// create a buffer and an host allocation for it
	{
		const VkBufferCreateInfo bufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
			DE_NULL,									// pNext
			0u,											// flags
			bufferSize,									// size
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
			VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
			1u,											// queueFamilyIndexCoun
			&m_queueFamilyIndex							// pQueueFamilyIndices
		};

		buffer = vk::createBuffer(m_vki, m_vkd, &bufferCreateInfo);
		const VkMemoryRequirements	memRequirements(vk::getBufferMemoryRequirements(m_vki, m_vkd, *buffer));
		allocation = m_allocator.allocate(memRequirements, MemoryRequirement::HostVisible);

		VK_CHECK(m_vki.bindBufferMemory(m_vkd, *buffer, allocation->getMemory(), allocation->getOffset()));
	}

	const VkImage& image = *frameBuffer->image->image;

	VkImageSubresourceRange		subresourceRange =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
		0u,											// baseMipLevel
		1u,											// levelCount
		0u,											// baseArrayLayer
		1u											// layerCount
	};

	const VkImageMemoryBarrier	barrierBefore =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// sType;
		DE_NULL,									// pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// dstQueueFamilyIndex;
		image,										// image;
		subresourceRange							// subresourceRange;
	};

	const VkBufferImageCopy		copyRegion =
	{
		0u,											// bufferOffset
		frameBuffer->image->extent.width,				// bufferRowLength
		frameBuffer->image->extent.height,			// bufferImageHeight
		{											// VkImageSubresourceLayers
			VK_IMAGE_ASPECT_COLOR_BIT,				// aspect
			0u,										// mipLevel
			0u,										// baseArrayLayer
			1u,										// layerCount
		},
		{ 0, 0, 0 },								// imageOffset
		frameBuffer->image->extent					// imageExtent
	};

	const VkBufferMemoryBarrier	bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// sType;
		DE_NULL,									// pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// dstQueueFamilyIndex;
		*buffer,									// buffer;
		0u,											// offset;
		bufferSize									// size;
	};

	const VkImageMemoryBarrier	barrierAfter =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType;
		DE_NULL,										// pNext;
		VK_ACCESS_TRANSFER_READ_BIT,					// srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		image,											// image
		subresourceRange								// subresourceRange
	};


	m_vki.cmdPipelineBarrier(commandBuffer,												// commandBuffer
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,	// srcStageMask, dstStageMask
		(VkDependencyFlags)0,															// dependencyFlags
		0u, DE_NULL,																	// memoryBarrierCount, pMemoryBarriers
		0u, DE_NULL,																	// bufferBarrierCount, pBufferBarriers
		1u, &barrierBefore);																// imageBarrierCount, pImageBarriers

	m_vki.cmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);

	m_vki.cmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		(VkDependencyFlags)0,
		0u, DE_NULL,
		1u, &bufferBarrier,
		1u, &barrierAfter);

	content = ut::BufferHandleAllocSp(new ut::BufferHandleAlloc(buffer, allocation));
}

std::string CommonDescriptorInstance::getColorAccess				(VkDescriptorType							descriptorType,
																	 const char*								indexVariableName,
																	 bool										usesMipMaps)
{
	std::string text;
	std::map<std::string, std::string> vars;
	vars["INDEX"] = indexVariableName;

	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		text = "data[nonuniformEXT(${INDEX})].c";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		text = "data[nonuniformEXT(${INDEX})].cold";
		break;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		text = "subpassLoad(data[nonuniformEXT(${INDEX})]).rgba";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		text = "texelFetch(data[nonuniformEXT(${INDEX})], 0)";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		text = "imageLoad(data[nonuniformEXT(${INDEX})], 0)";
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		text = usesMipMaps
			? "textureLod(nonuniformEXT(sampler2D(tex[0], data[${INDEX}])), normalpos, 1)"
			: "texture(   nonuniformEXT(sampler2D(tex[0], data[${INDEX}])), normalpos   )";
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		text = usesMipMaps
			? "textureLod( nonuniformEXT(sampler2D(data[${INDEX}], samp[0])), vec2(0,0), textureQueryLevels(nonuniformEXT(sampler2D(data[${INDEX}], samp[0])))-1)"
			: "texture(    nonuniformEXT(sampler2D(data[${INDEX}], samp[0])), vec2(0,0)   )";
		break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		text = usesMipMaps
			? "textureLod( data[nonuniformEXT(${INDEX})], uvec2(0,0), textureQueryLevels(data[nonuniformEXT(${INDEX})])-1)"
			: "texture(    data[nonuniformEXT(${INDEX})], uvec2(0,0)   )";
		break;
	default:
		TCU_THROW(InternalError, "Not implemented descriptor type");
	}

	return tcu::StringTemplate(text).specialize(vars);
}

std::string CommonDescriptorInstance::getFragmentReturnSource		(const std::string&							colorAccess)
{
	return "  FragColor = " + colorAccess + ";\n";
}

std::string CommonDescriptorInstance::getFragmentLoopSource			(const std::string&							colorAccess1,
																	 const std::string&							colorAccess2)
{
	std::map < std::string, std::string > vars;
	vars["COLOR_ACCESS_1"] = colorAccess1;
	vars["COLOR_ACCESS_2"] = colorAccess2;

	const char* s =
		"  vec4 sumClr1 = vec4(0,0,0,0);		\n"
		"  vec4 sumClr2 = vec4(0,0,0,0);		\n"
		"  for (int i = pc.lowerBound; i < pc.upperBound; ++i)	\n"
		"  {\n"
		"    int loopIdx = texelFetch(iter, i).x;				\n"
		"    sumClr1 += ${COLOR_ACCESS_2} + ${COLOR_ACCESS_1};	\n"
		"    sumClr2 += ${COLOR_ACCESS_2};						\n"
		"  }\n"
		"  FragColor = vec4(((sumClr1 - sumClr2) / float(pc.upperBound - pc.lowerBound)).rgb, 1);	\n";

	return tcu::StringTemplate(s).specialize(vars);
}

bool CommonDescriptorInstance::performWritesInVertex				(VkDescriptorType							descriptorType)
{
	bool result = false;

	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		result = true;
		break;
	default:
		result = false;
		break;
	}

	return result;
}

bool CommonDescriptorInstance::performWritesInVertex				(VkDescriptorType							descriptorType,
																	const Context&								context)
{
	bool result = false;

	ut::DeviceProperties			dp		(context);
	const VkPhysicalDeviceFeatures&	feats	= dp.physicalDeviceFeatures();

	if (feats.vertexPipelineStoresAndAtomics != DE_FALSE)
	{
		result = CommonDescriptorInstance::performWritesInVertex(descriptorType);
	}

	return result;
}

std::string CommonDescriptorInstance::getShaderAsm					(VkShaderStageFlagBits						shaderType,
																	 const TestCaseParams&						testCaseParams,
																	 bool										allowVertexStoring)
{
	std::stringstream	s;
	switch (shaderType)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			switch (testCaseParams.descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability SampledBuffer\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Vertex %main \"main\" %_ %position %in_position %normalpos %in_normalpos %vIndex %gl_VertexIndex %rIndex %index %gIndex %bIndex %aIndex\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpSourceExtension \"GL_EXT_texture_buffer\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %gl_PerVertex \"gl_PerVertex\"\n";
					s << "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n";
					s << "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n";
					s << "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n";
					s << "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n";
					s << "               OpName %_ \"\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %in_position \"in_position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %in_normalpos \"in_normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gl_VertexIndex \"gl_VertexIndex\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %index \"index\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n";
					s << "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n";
					s << "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n";
					s << "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n";
					s << "               OpDecorate %gl_PerVertex Block\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %in_position Location 0\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %in_normalpos Location 1\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n";
					s << "               OpDecorate %rIndex Location 3\n";
					s << "               OpDecorate %index Location 2\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "       %uint = OpTypeInt 32 0\n";
					s << "     %uint_1 = OpConstant %uint 1\n";
					s << "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n";
					s << "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n";
					s << "%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n";
					s << "          %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "      %int_1 = OpConstant %int 1\n";
					s << "%float_0_200000003 = OpConstant %float 0.200000003\n";
					s << "%_ptr_Output_float = OpTypePointer Output %float\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "   %position = OpVariable %_ptr_Output_v4float Output\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "%in_position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Output_v2float = OpTypePointer Output %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Output_v2float Output\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "%in_normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Output_int = OpTypePointer Output %int\n";
					s << "     %vIndex = OpVariable %_ptr_Output_int Output\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %rIndex = OpVariable %_ptr_Output_int Output\n";
					s << "      %v4int = OpTypeVector %int 4\n";
					s << "%_ptr_Input_v4int = OpTypePointer Input %v4int\n";
					s << "      %index = OpVariable %_ptr_Input_v4int Input\n";
					s << "     %uint_0 = OpConstant %uint 0\n";
					s << "     %gIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %bIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_2 = OpConstant %uint 2\n";
					s << "     %aIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_3 = OpConstant %uint 3\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %18 = OpAccessChain %_ptr_Output_float %_ %int_1\n";
					s << "               OpStore %18 %float_0_200000003\n";
					s << "         %23 = OpLoad %v4float %in_position\n";
					s << "               OpStore %position %23\n";
					s << "         %29 = OpLoad %v2float %in_normalpos\n";
					s << "               OpStore %normalpos %29\n";
					s << "         %31 = OpLoad %v4float %position\n";
					s << "         %32 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n";
					s << "               OpStore %32 %31\n";
					s << "         %37 = OpLoad %int %gl_VertexIndex\n";
					s << "               OpStore %vIndex %37\n";
					s << "         %43 = OpAccessChain %_ptr_Input_int %index %uint_0\n";
					s << "         %44 = OpLoad %int %43\n";
					s << "               OpStore %rIndex %44\n";
					s << "         %46 = OpAccessChain %_ptr_Input_int %index %uint_1\n";
					s << "         %47 = OpLoad %int %46\n";
					s << "               OpStore %gIndex %47\n";
					s << "         %50 = OpAccessChain %_ptr_Input_int %index %uint_2\n";
					s << "         %51 = OpLoad %int %50\n";
					s << "               OpStore %bIndex %51\n";
					s << "         %54 = OpAccessChain %_ptr_Input_int %index %uint_3\n";
					s << "         %55 = OpLoad %int %54\n";
					s << "               OpStore %aIndex %55\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability ImageBuffer\n";
					if (allowVertexStoring)
					{
						s << "               OpCapability ShaderNonUniform\n";
						s << "               OpCapability RuntimeDescriptorArray\n";
						s << "               OpCapability StorageTexelBufferArrayNonUniformIndexing\n";
						s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					}
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Vertex %main \"main\" %_ %position %in_position %normalpos %in_normalpos %vIndex %gl_VertexIndex %rIndex %index %gIndex %bIndex %aIndex %data\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %gl_PerVertex \"gl_PerVertex\"\n";
					s << "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n";
					s << "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n";
					s << "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n";
					s << "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n";
					s << "               OpName %_ \"\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %in_position \"in_position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %in_normalpos \"in_normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gl_VertexIndex \"gl_VertexIndex\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %index \"index\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n";
					s << "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n";
					s << "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n";
					s << "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n";
					s << "               OpDecorate %gl_PerVertex Block\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %in_position Location 0\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %in_normalpos Location 1\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n";
					s << "               OpDecorate %rIndex Location 3\n";
					s << "               OpDecorate %index Location 2\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 4\n";
					if (allowVertexStoring)
					{
						// s << "               OpDecorate %66 NonUniform\n";
						// s << "               OpDecorate %68 NonUniform\n";
						s << "               OpDecorate %69 NonUniform\n";
						// s << "               OpDecorate %71 NonUniform\n";
						// s << "               OpDecorate %72 NonUniform\n";
						s << "               OpDecorate %73 NonUniform\n";
					}
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "       %uint = OpTypeInt 32 0\n";
					s << "     %uint_1 = OpConstant %uint 1\n";
					s << "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n";
					s << "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n";
					s << "%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n";
					s << "          %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "      %int_1 = OpConstant %int 1\n";
					s << "%float_0_200000003 = OpConstant %float 0.200000003\n";
					s << "%_ptr_Output_float = OpTypePointer Output %float\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "   %position = OpVariable %_ptr_Output_v4float Output\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "%in_position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Output_v2float = OpTypePointer Output %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Output_v2float Output\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "%in_normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Output_int = OpTypePointer Output %int\n";
					s << "     %vIndex = OpVariable %_ptr_Output_int Output\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %rIndex = OpVariable %_ptr_Output_int Output\n";
					s << "      %v4int = OpTypeVector %int 4\n";
					s << "%_ptr_Input_v4int = OpTypePointer Input %v4int\n";
					s << "      %index = OpVariable %_ptr_Input_v4int Input\n";
					s << "     %uint_0 = OpConstant %uint 0\n";
					s << "     %gIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %bIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_2 = OpConstant %uint 2\n";
					s << "     %aIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_3 = OpConstant %uint 3\n";
					if (allowVertexStoring)
					{
						s << "        %bool = OpTypeBool\n";
						s << "          %61 = OpTypeImage %float Buffer 0 0 0 2 Rgba32f\n";
						s << " %_runtimearr_61 = OpTypeRuntimeArray %61\n";
						s << " %_ptr_UniformConstant__runtimearr_61 = OpTypePointer UniformConstant %_runtimearr_61\n";
						s << "        %data = OpVariable %_ptr_UniformConstant__runtimearr_61 UniformConstant\n";
						s << " %_ptr_UniformConstant_61 = OpTypePointer UniformConstant %61\n";
					}
					else
					{
						s << "         %56 = OpTypeImage %float Buffer 0 0 0 2 Rgba32f\n";
						s << "%_arr_56_uint_1 = OpTypeArray %56 %uint_1\n";
						s << "%_ptr_UniformConstant__arr_56_uint_1 = OpTypePointer UniformConstant %_arr_56_uint_1\n";
						s << "       %data = OpVariable %_ptr_UniformConstant__arr_56_uint_1 UniformConstant\n";
					}
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %18 = OpAccessChain %_ptr_Output_float %_ %int_1\n";
					s << "               OpStore %18 %float_0_200000003\n";
					s << "         %23 = OpLoad %v4float %in_position\n";
					s << "               OpStore %position %23\n";
					s << "         %29 = OpLoad %v2float %in_normalpos\n";
					s << "               OpStore %normalpos %29\n";
					s << "         %31 = OpLoad %v4float %position\n";
					s << "         %32 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n";
					s << "               OpStore %32 %31\n";
					s << "         %37 = OpLoad %int %gl_VertexIndex\n";
					s << "               OpStore %vIndex %37\n";
					s << "         %43 = OpAccessChain %_ptr_Input_int %index %uint_0\n";
					s << "         %44 = OpLoad %int %43\n";
					s << "               OpStore %rIndex %44\n";
					s << "         %46 = OpAccessChain %_ptr_Input_int %index %uint_1\n";
					s << "         %47 = OpLoad %int %46\n";
					s << "               OpStore %gIndex %47\n";
					s << "         %50 = OpAccessChain %_ptr_Input_int %index %uint_2\n";
					s << "         %51 = OpLoad %int %50\n";
					s << "               OpStore %bIndex %51\n";
					s << "         %54 = OpAccessChain %_ptr_Input_int %index %uint_3\n";
					s << "         %55 = OpLoad %int %54\n";
					s << "               OpStore %aIndex %55\n";
					if (allowVertexStoring)
					{
						s << "          %56 = OpLoad %int %gIndex\n";
						s << "          %58 = OpINotEqual %bool %56 %int_0\n";
						s << "                OpSelectionMerge %60 None\n";
						s << "                OpBranchConditional %58 %59 %60\n";
						s << "          %59 = OpLabel\n";
						s << "          %65 = OpLoad %int %gIndex\n";
						s << "          %66 = OpCopyObject %int %65\n";
						s << "          %68 = OpAccessChain %_ptr_UniformConstant_61 %data %66\n";
						s << "          %69 = OpLoad %61 %68\n";
						s << "          %70 = OpLoad %int %rIndex\n";
						s << "          %71 = OpCopyObject %int %70\n";
						s << "          %72 = OpAccessChain %_ptr_UniformConstant_61 %data %71\n";
						s << "          %73 = OpLoad %61 %72\n";
						s << "          %74 = OpImageRead %v4float %73 %int_0\n";
						s << "                OpImageWrite %69 %int_1 %74\n";
						s << "                OpBranch %60\n";
						s << "          %60 = OpLabel\n";
					}
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					s << "               OpCapability Shader\n";
					if (allowVertexStoring)
					{
						s << "               OpCapability ShaderNonUniform\n";
						s << "               OpCapability RuntimeDescriptorArray\n";
						s << "               OpCapability StorageBufferArrayNonUniformIndexing\n";
						s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					}
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Vertex %main \"main\" %_ %position %in_position %normalpos %in_normalpos %vIndex %gl_VertexIndex %rIndex %index %gIndex %bIndex %aIndex %data\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %gl_PerVertex \"gl_PerVertex\"\n";
					s << "               OpMemberName %gl_PerVertex 0 \"gl_Position\"\n";
					s << "               OpMemberName %gl_PerVertex 1 \"gl_PointSize\"\n";
					s << "               OpMemberName %gl_PerVertex 2 \"gl_ClipDistance\"\n";
					s << "               OpMemberName %gl_PerVertex 3 \"gl_CullDistance\"\n";
					s << "               OpName %_ \"\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %in_position \"in_position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %in_normalpos \"in_normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gl_VertexIndex \"gl_VertexIndex\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %index \"index\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpName %Data \"Data\"\n";
					s << "               OpMemberName %Data 0 \"cnew\"\n";
					s << "               OpMemberName %Data 1 \"cold\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position\n";
					s << "               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize\n";
					s << "               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance\n";
					s << "               OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance\n";
					s << "               OpDecorate %gl_PerVertex Block\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %in_position Location 0\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %in_normalpos Location 1\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n";
					s << "               OpDecorate %rIndex Location 3\n";
					s << "               OpDecorate %index Location 2\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "               OpMemberDecorate %Data 0 Offset 0\n";
					s << "               OpMemberDecorate %Data 1 Offset 16\n";
					s << "               OpDecorate %Data Block\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 2\n";
					if (allowVertexStoring)
					{
						// s << "               OpDecorate %66 NonUniform\n";
						// s << "               OpDecorate %68 NonUniform\n";
						s << "               OpDecorate %70 NonUniform\n";
						// s << "               OpDecorate %71 NonUniform\n";
						s << "               OpDecorate %72 NonUniform\n";
					}
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "       %uint = OpTypeInt 32 0\n";
					s << "     %uint_1 = OpConstant %uint 1\n";
					s << "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n";
					s << "%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n";
					s << "%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex\n";
					s << "          %_ = OpVariable %_ptr_Output_gl_PerVertex Output\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "      %int_1 = OpConstant %int 1\n";
					s << "%float_0_200000003 = OpConstant %float 0.200000003\n";
					s << "%_ptr_Output_float = OpTypePointer Output %float\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "   %position = OpVariable %_ptr_Output_v4float Output\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "%in_position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Output_v2float = OpTypePointer Output %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Output_v2float Output\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "%in_normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Output_int = OpTypePointer Output %int\n";
					s << "     %vIndex = OpVariable %_ptr_Output_int Output\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %rIndex = OpVariable %_ptr_Output_int Output\n";
					s << "      %v4int = OpTypeVector %int 4\n";
					s << "%_ptr_Input_v4int = OpTypePointer Input %v4int\n";
					s << "      %index = OpVariable %_ptr_Input_v4int Input\n";
					s << "     %uint_0 = OpConstant %uint 0\n";
					s << "     %gIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %bIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_2 = OpConstant %uint 2\n";
					s << "     %aIndex = OpVariable %_ptr_Output_int Output\n";
					s << "     %uint_3 = OpConstant %uint 3\n";
					s << "       %Data = OpTypeStruct %v4float %v4float\n";
					if (allowVertexStoring)
					{
						s << "       %bool = OpTypeBool\n";
						s << "%_runtimearr_Data = OpTypeRuntimeArray %Data\n";
						s << "%_ptr_StorageBuffer__runtimearr_Data = OpTypePointer StorageBuffer %_runtimearr_Data\n";
						s << "       %data = OpVariable  %_ptr_StorageBuffer__runtimearr_Data StorageBuffer\n";
						s << "%_ptr_StorageBuffer_v4float = OpTypePointer StorageBuffer %v4float\n";
					}
					else
					{
						s << "%_arr_Data_uint_1 = OpTypeArray %Data %uint_1\n";
						s << "%_ptr_StorageBuffer__arr_Data_uint_1 = OpTypePointer StorageBuffer %_arr_Data_uint_1\n";
						s << "       %data = OpVariable %_ptr_StorageBuffer__arr_Data_uint_1 StorageBuffer\n";
					}
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %18 = OpAccessChain %_ptr_Output_float %_ %int_1\n";
					s << "               OpStore %18 %float_0_200000003\n";
					s << "         %23 = OpLoad %v4float %in_position\n";
					s << "               OpStore %position %23\n";
					s << "         %29 = OpLoad %v2float %in_normalpos\n";
					s << "               OpStore %normalpos %29\n";
					s << "         %31 = OpLoad %v4float %position\n";
					s << "         %32 = OpAccessChain %_ptr_Output_v4float %_ %int_0\n";
					s << "               OpStore %32 %31\n";
					s << "         %37 = OpLoad %int %gl_VertexIndex\n";
					s << "               OpStore %vIndex %37\n";
					s << "         %43 = OpAccessChain %_ptr_Input_int %index %uint_0\n";
					s << "         %44 = OpLoad %int %43\n";
					s << "               OpStore %rIndex %44\n";
					s << "         %46 = OpAccessChain %_ptr_Input_int %index %uint_1\n";
					s << "         %47 = OpLoad %int %46\n";
					s << "               OpStore %gIndex %47\n";
					s << "         %50 = OpAccessChain %_ptr_Input_int %index %uint_2\n";
					s << "         %51 = OpLoad %int %50\n";
					s << "               OpStore %bIndex %51\n";
					s << "         %54 = OpAccessChain %_ptr_Input_int %index %uint_3\n";
					s << "         %55 = OpLoad %int %54\n";
					s << "               OpStore %aIndex %55\n";
					if (allowVertexStoring)
					{
						s << "          %56 = OpLoad %int %gIndex\n";
						s << "          %58 = OpINotEqual %bool %56 %int_0\n";
						s << "                OpSelectionMerge %60 None\n";
						s << "                OpBranchConditional %58 %59 %60\n";
						s << "          %59 = OpLabel\n";
						s << "          %65 = OpLoad %int %gIndex\n";
						s << "          %66 = OpCopyObject %int %65\n";
						s << "          %67 = OpLoad %int %rIndex\n";
						s << "          %68 = OpCopyObject %int %67\n";
						s << "          %70 = OpAccessChain %_ptr_StorageBuffer_v4float %data %68 %int_1\n";
						s << "          %71 = OpLoad %v4float %70\n";
						s << "          %72 = OpAccessChain %_ptr_StorageBuffer_v4float %data %66 %int_0\n";
						s << "                OpStore %72 %71\n";
						s << "                OpBranch %60\n";
						s << "          %60 = OpLabel\n";
					}
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				default:
					TCU_THROW(InternalError, "Unexpected descriptor type");
			}
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			switch (testCaseParams.descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					s << "               OpCapability Shader\n";
					if (testCaseParams.usesMipMaps)
					{
						s << "               OpCapability ImageQuery\n";
					}
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability SampledImageArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Fragment %main \"main\" %FragColor %data %rIndex %position %normalpos %vIndex %gIndex %bIndex %aIndex\n";
					s << "               OpExecutionMode %main OriginUpperLeft\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpSourceExtension \"GL_EXT_texture_buffer\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %FragColor \"FragColor\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpDecorate %FragColor Location 0\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 7\n";
					s << "               OpDecorate %rIndex Flat\n";
					s << "               OpDecorate %rIndex Location 3\n";
					// s << "               OpDecorate %19 NonUniform\n";
					// s << "               OpDecorate %21 NonUniform\n";
					s << "               OpDecorate %22 NonUniform\n";
					if (testCaseParams.usesMipMaps)
					{
						// s << "               OpDecorate %27 NonUniform\n";
						// s << "               OpDecorate %28 NonUniform\n";
						// s << "               OpDecorate %29 NonUniform\n";
						s << "               OpDecorate %30 NonUniform\n";
					}
					s << "               OpDecorate %position Flat\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %normalpos Flat\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %vIndex Flat\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gIndex Flat\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Flat\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Flat\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "  %FragColor = OpVariable %_ptr_Output_v4float Output\n";
					s << "         %10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n";
					s << "         %11 = OpTypeSampledImage %10\n";
					s << "%_runtimearr_11 = OpTypeRuntimeArray %11\n";
					s << "%_ptr_UniformConstant__runtimearr_11 = OpTypePointer UniformConstant %_runtimearr_11\n";
					s << "       %data = OpVariable %_ptr_UniformConstant__runtimearr_11 UniformConstant\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "     %rIndex = OpVariable %_ptr_Input_int Input\n";
					s << "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "    %float_0 = OpConstant %float 0\n";
					s << "      %int_1 = OpConstant %int 1\n";
					s << "         %25 = OpConstantComposite %v2float %float_0 %float_0\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "   %position = OpVariable %_ptr_Input_v4float Input\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "     %vIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %gIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %bIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %aIndex = OpVariable %_ptr_Input_int Input\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %18 = OpLoad %int %rIndex\n";
					s << "         %19 = OpCopyObject %int %18\n";
					s << "         %21 = OpAccessChain %_ptr_UniformConstant_11 %data %19\n";
					s << "         %22 = OpLoad %11 %21\n";
					if (testCaseParams.usesMipMaps)
					{
						s << "          %26 = OpLoad %int %rIndex\n";
						s << "          %27 = OpCopyObject %int %26\n";
						s << "          %28 = OpAccessChain %_ptr_UniformConstant_11 %data %27\n";
						s << "          %29 = OpLoad %11 %28\n";
						s << "          %30 = OpImage %10 %29\n";
						s << "          %31 = OpImageQueryLevels %int %30\n";
						s << "          %33 = OpISub %int %31 %int_1\n";
						s << "          %34 = OpConvertSToF %float %33\n";
						s << "          %35 = OpImageSampleExplicitLod %v4float %22 %25 Lod %34\n";
						s << "                OpStore %FragColor %35\n";
					}
					else
					{
						s << "         %26 = OpImageSampleImplicitLod %v4float %22 %25\n";
						s << "               OpStore %FragColor %26\n";
					}
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability SampledBuffer\n";
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability UniformTexelBufferArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Fragment %main \"main\" %FragColor %data %rIndex %position %normalpos %vIndex %gIndex %bIndex %aIndex\n";
					s << "               OpExecutionMode %main OriginUpperLeft\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpSourceExtension \"GL_EXT_texture_buffer\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %FragColor \"FragColor\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpDecorate %FragColor Location 0\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 3\n";
					s << "               OpDecorate %rIndex Flat\n";
					s << "               OpDecorate %rIndex Location 3\n";
					// s << "               OpDecorate %19 NonUniform\n";
					// s << "               OpDecorate %21 NonUniform\n";
					// s << "               OpDecorate %22 NonUniform\n";
					s << "               OpDecorate %24 NonUniform\n";
					s << "               OpDecorate %position Flat\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %normalpos Flat\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %vIndex Flat\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gIndex Flat\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Flat\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Flat\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "  %FragColor = OpVariable %_ptr_Output_v4float Output\n";
					s << "         %10 = OpTypeImage %float Buffer 0 0 0 1 Unknown\n";
					s << "         %11 = OpTypeSampledImage %10\n";
					s << "%_runtimearr_11 = OpTypeRuntimeArray %11\n";
					s << "%_ptr_UniformConstant__runtimearr_11 = OpTypePointer UniformConstant %_runtimearr_11\n";
					s << "       %data = OpVariable %_ptr_UniformConstant__runtimearr_11 UniformConstant\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "     %rIndex = OpVariable %_ptr_Input_int Input\n";
					s << "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "   %position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "     %vIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %gIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %bIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %aIndex = OpVariable %_ptr_Input_int Input\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %18 = OpLoad %int %rIndex\n";
					s << "         %19 = OpCopyObject %int %18\n";
					s << "         %21 = OpAccessChain %_ptr_UniformConstant_11 %data %19\n";
					s << "         %22 = OpLoad %11 %21\n";
					s << "         %24 = OpImage %10 %22\n";
					s << "         %25 = OpImageFetch %v4float %24 %int_0\n";
					s << "               OpStore %FragColor %25\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability ImageBuffer\n";
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability StorageTexelBufferArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Fragment %main \"main\" %FragColor %data %rIndex %position %normalpos %vIndex %gIndex %bIndex %aIndex\n";
					s << "               OpExecutionMode %main OriginUpperLeft\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %FragColor \"FragColor\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpDecorate %FragColor Location 0\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 4\n";
					s << "               OpDecorate %rIndex Flat\n";
					s << "               OpDecorate %rIndex Location 3\n";
					// s << "               OpDecorate %18 NonUniform\n";
					// s << "               OpDecorate %20 NonUniform\n";
					s << "               OpDecorate %21 NonUniform\n";
					s << "               OpDecorate %position Flat\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %normalpos Flat\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %vIndex Flat\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gIndex Flat\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Flat\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Flat\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "  %FragColor = OpVariable %_ptr_Output_v4float Output\n";
					s << "         %10 = OpTypeImage %float Buffer 0 0 0 2 Rgba32f\n";
					s << "%_runtimearr_10 = OpTypeRuntimeArray %10\n";
					s << "%_ptr_UniformConstant__runtimearr_10 = OpTypePointer UniformConstant %_runtimearr_10\n";
					s << "       %data = OpVariable %_ptr_UniformConstant__runtimearr_10 UniformConstant\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "     %rIndex = OpVariable %_ptr_Input_int Input\n";
					s << "%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "   %position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "     %vIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %gIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %bIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %aIndex = OpVariable %_ptr_Input_int Input\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %17 = OpLoad %int %rIndex\n";
					s << "         %18 = OpCopyObject %int %17\n";
					s << "         %20 = OpAccessChain %_ptr_UniformConstant_10 %data %18\n";
					s << "         %21 = OpLoad %10 %20\n";
					s << "         %23 = OpImageRead %v4float %21 %int_0\n";
					s << "               OpStore %FragColor %23\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability StorageBufferArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Fragment %main \"main\" %FragColor %data %rIndex %position %normalpos %vIndex %gIndex %bIndex %aIndex\n";
					s << "               OpExecutionMode %main OriginUpperLeft\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %FragColor \"FragColor\"\n";
					s << "               OpName %Data \"Data\"\n";
					s << "               OpMemberName %Data 0 \"cnew\"\n";
					s << "               OpMemberName %Data 1 \"cold\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpDecorate %FragColor Location 0\n";
					s << "               OpMemberDecorate %Data 0 Offset 0\n";
					s << "               OpMemberDecorate %Data 1 Offset 16\n";
					s << "               OpDecorate %Data Block\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 2\n";
					s << "               OpDecorate %rIndex Flat\n";
					s << "               OpDecorate %rIndex Location 3\n";
					// s << "               OpDecorate %18 NonUniform\n";
					s << "               OpDecorate %21 NonUniform\n";
					// s << "               OpDecorate %22 NonUniform\n";
					s << "               OpDecorate %position Flat\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %normalpos Flat               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %vIndex Flat\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gIndex Flat\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Flat\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Flat\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "  %FragColor = OpVariable %_ptr_Output_v4float Output\n";
					s << "       %Data = OpTypeStruct %v4float %v4float\n";
					s << "%_runtimearr_Data = OpTypeRuntimeArray %Data\n";
					s << "%_ptr_StorageBuffer__runtimearr_Data = OpTypePointer StorageBuffer %_runtimearr_Data\n";
					s << "       %data = OpVariable %_ptr_StorageBuffer__runtimearr_Data StorageBuffer\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "     %rIndex = OpVariable %_ptr_Input_int Input\n";
					s << "      %int_1 = OpConstant %int 1\n";
					s << "%_ptr_StorageBuffer_v4float = OpTypePointer StorageBuffer %v4float\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "   %position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "     %vIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %gIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %bIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %aIndex = OpVariable %_ptr_Input_int Input\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %17 = OpLoad %int %rIndex\n";
					s << "         %18 = OpCopyObject %int %17\n";
					s << "         %21 = OpAccessChain %_ptr_StorageBuffer_v4float %data %18 %int_1\n";
					s << "         %22 = OpLoad %v4float %21\n";
					s << "               OpStore %FragColor %22\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					s << "               OpCapability Shader\n";
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability UniformBufferArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint Fragment %main \"main\" %FragColor %data %rIndex %position %normalpos %vIndex %gIndex %bIndex %aIndex\n";
					s << "               OpExecutionMode %main OriginUpperLeft\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %FragColor \"FragColor\"\n";
					s << "               OpName %Data \"Data\"\n";
					s << "               OpMemberName %Data 0 \"c\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpName %rIndex \"rIndex\"\n";
					s << "               OpName %position \"position\"\n";
					s << "               OpName %normalpos \"normalpos\"\n";
					s << "               OpName %vIndex \"vIndex\"\n";
					s << "               OpName %gIndex \"gIndex\"\n";
					s << "               OpName %bIndex \"bIndex\"\n";
					s << "               OpName %aIndex \"aIndex\"\n";
					s << "               OpDecorate %FragColor Location 0\n";
					s << "               OpMemberDecorate %Data 0 Offset 0\n";
					s << "               OpDecorate %Data Block\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 1\n";
					s << "               OpDecorate %rIndex Flat\n";
					s << "               OpDecorate %rIndex Location 3\n";
					// s << "               OpDecorate %18 NonUniform\n";
					s << "               OpDecorate %21 NonUniform\n";
					// s << "               OpDecorate %22 NonUniform\n";
					s << "               OpDecorate %position Flat\n";
					s << "               OpDecorate %position Location 0\n";
					s << "               OpDecorate %normalpos Flat\n";
					s << "               OpDecorate %normalpos Location 1\n";
					s << "               OpDecorate %vIndex Flat\n";
					s << "               OpDecorate %vIndex Location 2\n";
					s << "               OpDecorate %gIndex Flat\n";
					s << "               OpDecorate %gIndex Location 4\n";
					s << "               OpDecorate %bIndex Flat\n";
					s << "               OpDecorate %bIndex Location 5\n";
					s << "               OpDecorate %aIndex Flat\n";
					s << "               OpDecorate %aIndex Location 6\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "      %float = OpTypeFloat 32\n";
					s << "    %v4float = OpTypeVector %float 4\n";
					s << "%_ptr_Output_v4float = OpTypePointer Output %v4float\n";
					s << "  %FragColor = OpVariable %_ptr_Output_v4float Output\n";
					s << "       %Data = OpTypeStruct %v4float\n";
					s << "%_runtimearr_Data = OpTypeRuntimeArray %Data\n";
					s << "%_ptr_Uniform__runtimearr_Data = OpTypePointer Uniform %_runtimearr_Data\n";
					s << "       %data = OpVariable %_ptr_Uniform__runtimearr_Data Uniform\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "%_ptr_Input_int = OpTypePointer Input %int\n";
					s << "     %rIndex = OpVariable %_ptr_Input_int Input\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float\n";
					s << "%_ptr_Input_v4float = OpTypePointer Input %v4float\n";
					s << "   %position = OpVariable %_ptr_Input_v4float Input\n";
					s << "    %v2float = OpTypeVector %float 2\n";
					s << "%_ptr_Input_v2float = OpTypePointer Input %v2float\n";
					s << "  %normalpos = OpVariable %_ptr_Input_v2float Input\n";
					s << "     %vIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %gIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %bIndex = OpVariable %_ptr_Input_int Input\n";
					s << "     %aIndex = OpVariable %_ptr_Input_int Input\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "         %17 = OpLoad %int %rIndex\n";
					s << "         %18 = OpCopyObject %int %17\n";
					s << "         %21 = OpAccessChain %_ptr_Uniform_v4float %data %18 %int_0\n";
					s << "         %22 = OpLoad %v4float %21\n";
					s << "               OpStore %FragColor %22\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				default:
					TCU_THROW(InternalError, "Unexpected descriptor type");
			}
		    break;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			switch (testCaseParams.descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					s << "               OpCapability Shader\n";
					s << "               OpCapability ShaderNonUniform\n";
					s << "               OpCapability RuntimeDescriptorArray\n";
					s << "               OpCapability StorageImageArrayNonUniformIndexing\n";
					s << "               OpExtension \"SPV_EXT_descriptor_indexing\"\n";
					s << "          %1 = OpExtInstImport \"GLSL.std.450\"\n";
					s << "               OpMemoryModel Logical GLSL450\n";
					s << "               OpEntryPoint GLCompute %main \"main\" %idxs %gl_WorkGroupID %data\n";
					s << "               OpExecutionMode %main LocalSize 1 1 1\n";
					s << "               OpSource GLSL 450\n";
					s << "               OpSourceExtension \"GL_EXT_nonuniform_qualifier\"\n";
					s << "               OpName %main \"main\"\n";
					s << "               OpName %c \"c\"\n";
					s << "               OpName %idxs \"idxs\"\n";
					s << "               OpName %gl_WorkGroupID \"gl_WorkGroupID\"\n";
					s << "               OpName %data \"data\"\n";
					s << "               OpDecorate %idxs DescriptorSet 0\n";
					s << "               OpDecorate %idxs Binding 12\n";
					s << "               OpDecorate %gl_WorkGroupID BuiltIn WorkgroupId\n";
					s << "               OpDecorate %data DescriptorSet 0\n";
					s << "               OpDecorate %data Binding 11\n";
					// s << "               OpDecorate %36 NonUniform\n";
					// s << "               OpDecorate %37 NonUniform\n";
					s << "               OpDecorate %41 NonUniform\n";
					s << "               OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n";
					s << "       %void = OpTypeVoid\n";
					s << "          %3 = OpTypeFunction %void\n";
					s << "       %uint = OpTypeInt 32 0\n";
					s << "     %v4uint = OpTypeVector %uint 4\n";
					s << "%_ptr_Function_v4uint = OpTypePointer Function %v4uint\n";
					s << "         %10 = OpTypeImage %uint 2D 0 0 0 2 R32ui\n";
					s << "%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10\n";
					s << "       %idxs = OpVariable %_ptr_UniformConstant_10 UniformConstant\n";
					s << "     %v3uint = OpTypeVector %uint 3\n";
					s << "%_ptr_Input_v3uint = OpTypePointer Input %v3uint\n";
					s << "%gl_WorkGroupID = OpVariable %_ptr_Input_v3uint Input\n";
					s << "     %uint_0 = OpConstant %uint 0\n";
					s << "%_ptr_Input_uint = OpTypePointer Input %uint\n";
					s << "        %int = OpTypeInt 32 1\n";
					s << "     %uint_1 = OpConstant %uint 1\n";
					s << "      %v2int = OpTypeVector %int 2\n";
					s << "%_runtimearr_10 = OpTypeRuntimeArray %10\n";
					s << "%_ptr_UniformConstant__runtimearr_10 = OpTypePointer UniformConstant %_runtimearr_10\n";
					s << "       %data = OpVariable %_ptr_UniformConstant__runtimearr_10 UniformConstant\n";
					s << "%_ptr_Function_uint = OpTypePointer Function %uint\n";
					s << "      %int_0 = OpConstant %int 0\n";
					s << "         %39 = OpConstantComposite %v2int %int_0 %int_0\n";
					s << "%_ptr_Image_uint = OpTypePointer Image %uint\n";
					s << "%gl_WorkGroupSize = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1\n";
					s << "       %main = OpFunction %void None %3\n";
					s << "          %5 = OpLabel\n";
					s << "          %c = OpVariable %_ptr_Function_v4uint Function\n";
					s << "         %13 = OpLoad %10 %idxs\n";
					s << "         %19 = OpAccessChain %_ptr_Input_uint %gl_WorkGroupID %uint_0\n";
					s << "         %20 = OpLoad %uint %19\n";
					s << "         %22 = OpBitcast %int %20\n";
					s << "         %24 = OpAccessChain %_ptr_Input_uint %gl_WorkGroupID %uint_1\n";
					s << "         %25 = OpLoad %uint %24\n";
					s << "         %26 = OpBitcast %int %25\n";
					s << "         %28 = OpCompositeConstruct %v2int %22 %26\n";
					s << "         %29 = OpImageRead %v4uint %13 %28 ZeroExtend\n";
					s << "               OpStore %c %29\n";
					s << "         %34 = OpAccessChain %_ptr_Function_uint %c %uint_0\n";
					s << "         %35 = OpLoad %uint %34\n";
					s << "         %36 = OpCopyObject %uint %35\n";
					s << "         %37 = OpAccessChain %_ptr_UniformConstant_10 %data %36\n";
					s << "         %41 = OpImageTexelPointer %_ptr_Image_uint %37 %39 %uint_0\n";
					s << "         %42 = OpAtomicIAdd %uint %41 %uint_1 %uint_0 %uint_1\n";
					s << "               OpReturn\n";
					s << "               OpFunctionEnd\n";
					break;
				default:
					TCU_THROW(InternalError, "Unexpected descriptor type");
			}
			break;
		default:
			TCU_THROW(InternalError, "Unexpected stage");
	}

	return s.str();
}

std::string CommonDescriptorInstance::getShaderSource				(VkShaderStageFlagBits						shaderType,
																	 const TestCaseParams&						testCaseParams,
																	 bool										allowVertexStoring)
{
	std::stringstream	s;

	s << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << '\n';
	s << "#extension GL_EXT_nonuniform_qualifier : require	\n";

	if (testCaseParams.calculateInLoop)
	{
		s << "layout(push_constant)     uniform Block { int lowerBound, upperBound; } pc;\n";
		s << substBinding(BINDING_DescriptorEnumerator,
			"layout(set=1,binding=${?}) uniform isamplerBuffer iter;	\n");
	}

	switch (testCaseParams.descriptorType)
	{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			s << substBinding(BINDING_StorageBuffer,
				"layout(set=0,binding=${?}) buffer Data { vec4 cnew, cold; } data[]; \n");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			s << substBinding(BINDING_StorageBufferDynamic,
				"layout(set=0,binding=${?}) buffer Data { vec4 cnew, cold; } data[]; \n");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			s << substBinding(BINDING_UniformBuffer,
				"layout(set=0,binding=${?}) uniform Data { vec4 c; } data[]; \n");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			s << substBinding(BINDING_UniformBufferDynamic,
				"layout(set=0,binding=${?}) uniform Data { vec4 c; } data[]; \n");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			s << substBinding(BINDING_StorageTexelBuffer,
				"layout(set=0,binding=${?},rgba32f) uniform imageBuffer data[];\n");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			s << "#extension GL_EXT_texture_buffer : require	\n";
			s << substBinding(BINDING_UniformTexelBuffer,
				"layout(set=0,binding=${?}) uniform samplerBuffer data[];\n");
			break;
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			// Left for the consistent of code.
			// Header is set one swicth below
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			s << "#extension GL_EXT_texture_buffer : require	\n";
			s << substBinding(BINDING_SampledImage,
				"layout(set=0,binding=${?}) uniform texture2D ${VAR}[${*}];\n", 1, "tex");
			s << substBinding(BINDING_Sampler,
				"layout(set=0,binding=${?}) uniform sampler ${VAR}[${*}];\n");
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			s << "#extension GL_EXT_texture_buffer : require	\n";
			s << substBinding(BINDING_Sampler,
				"layout(set=0,binding=${?}) uniform sampler ${VAR}[${*}];\n", 1, "samp");
			s << substBinding(BINDING_SampledImage,
				"layout(set=0,binding=${?}) uniform texture2D ${VAR}[${*}];\n");
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			s << "#extension GL_EXT_texture_buffer : require	\n";
			s << substBinding(BINDING_CombinedImageSampler,
				"layout(set=0,binding=${?}) uniform sampler2D data[];\n");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			s << "layout(local_size_x=1,local_size_y=1,local_size_z=1) in;	\n";
			s << substBinding(BINDING_StorageImage + 1,
				"layout(r32ui,set=0,binding=${?}) uniform uimage2D idxs;	\n");
			s << substBinding(BINDING_StorageImage,
				"layout(r32ui,set=0,binding=${?}) uniform uimage2D data[];	\n");
			break;
		default:
			TCU_THROW(InternalError, "Not implemented descriptor type");
	}

	switch (shaderType)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:	s << getVertexShaderProlog();	break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			{
				if (testCaseParams.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				{
					s << substBinding(BINDING_InputAttachment,
						"layout(input_attachment_index=1,set=0,binding=${?}) uniform subpassInput data[];	\n");
				}
				s << getFragmentShaderProlog();
			}
			break;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			break;
		default:
			TCU_THROW(InternalError, "Not implemented shader stage");
	}

	switch (shaderType)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		{
			switch (testCaseParams.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				if (allowVertexStoring)
					s << "  if (gIndex != 0) data[nonuniformEXT(gIndex)].cnew = data[nonuniformEXT(rIndex)].cold;	\n";
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				if (allowVertexStoring)
					s << "  if (gIndex != 0) imageStore(data[nonuniformEXT(gIndex)], 1, imageLoad(data[nonuniformEXT(rIndex)], 0));	\n";
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				break;

			default:
				TCU_THROW(InternalError, "Not implemented descriptor type");
			}
		}
		break;

		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			switch (testCaseParams.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				{
					if (testCaseParams.calculateInLoop)
						s << getFragmentLoopSource(
							getColorAccess(testCaseParams.descriptorType, "rIndex", false),
							getColorAccess(testCaseParams.descriptorType, "loopIdx", false));
					else
						s << getFragmentReturnSource(getColorAccess(testCaseParams.descriptorType, "rIndex", false));
				}
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				if (testCaseParams.calculateInLoop)
					s << getFragmentLoopSource(
						getColorAccess(testCaseParams.descriptorType, "rIndex", testCaseParams.usesMipMaps),
						getColorAccess(testCaseParams.descriptorType, "loopIdx", testCaseParams.usesMipMaps));
				else
					s << getFragmentReturnSource(getColorAccess(testCaseParams.descriptorType, "rIndex", testCaseParams.usesMipMaps));
				break;
			default:	TCU_THROW(InternalError, "Not implemented descriptor type");
			}
		}
		break;

		case VK_SHADER_STAGE_COMPUTE_BIT: // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			s << "void main(void)\n{\n";
			if (testCaseParams.calculateInLoop)
				s << "  for (int i = pc.lowerBound; i < pc.upperBound; ++i)	\n"
					"    imageAtomicAdd(data[nonuniformEXT(texelFetch(iter, i).x)], ivec2(0, 0), 1);			\n";
			else
				s << "  uvec4 c = imageLoad(idxs, ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y));	\n"
					"  imageAtomicAdd( data[nonuniformEXT(c.r)], ivec2(0, 0), 1);								\n";
			break;

		default:	TCU_THROW(InternalError, "Not implemented shader stage");
	}

	s << getShaderEpilog();

	return s.str();
}

class StorageBufferInstance : virtual public CommonDescriptorInstance
{
public:
								StorageBufferInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
protected:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);

	virtual bool				verifyVertexWriteResults			(IterateCommonVariables&					variables);
};

StorageBufferInstance::StorageBufferInstance						(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			BINDING_StorageBuffer,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void StorageBufferInstance::createAndPopulateDescriptors			(IterateCommonVariables&					variables)
{
	BindingStorageBuffer::Data	data;

	bool						vertexStores = false;
	{
		ut::DeviceProperties dp(m_context);
		vertexStores = dp.physicalDeviceFeatures().vertexPipelineStoresAndAtomics != DE_FALSE;
	}
	const deUint32				alignment	= static_cast<deUint32>(ut::DeviceProperties(m_context).physicalDeviceProperties().limits.minStorageBufferOffsetAlignment);
	createBuffers(variables.descriptorsBufferInfos, variables.descriptorsBuffer, variables.validDescriptorCount, sizeof(data), alignment, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	unsigned char*				buffer		= static_cast<unsigned char*>(variables.descriptorsBuffer->alloc->getHostPtr());
	for (deUint32 infoIdx = 0; infoIdx < variables.validDescriptorCount; ++infoIdx)
	{
		const float				component	= m_colorScheme[infoIdx % m_schemeSize];
		const tcu::Vec4			color		(component, component, component, 1.0f);
		VkDescriptorBufferInfo& info		= variables.descriptorsBufferInfos[infoIdx];
		data.cnew							= vertexStores ? m_clearColor : color;
		data.cold							= color;

		deMemcpy(buffer + info.offset, &data, sizeof(data));
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);

	variables.dataAlignment = deAlign64(sizeof(data), alignment);
}

bool StorageBufferInstance::verifyVertexWriteResults				(IterateCommonVariables&					variables)
{
	const tcu::Vec4				threshold		(0.002f, 0.002f, 0.002f, 0.002f);
	const std::vector<deUint32>	primes			= ut::generatePrimes(variables.availableDescriptorCount);

	unsigned char*				buffer = static_cast<unsigned char*>(variables.descriptorsBuffer->alloc->getHostPtr());
	BindingStorageBuffer::Data	data;
	for (deUint32 primeIdx = 0; primeIdx < variables.validDescriptorCount; ++primeIdx)
	{
		const deUint32			prime		= primes[primeIdx];
		const float				component	= m_colorScheme[(prime % variables.validDescriptorCount) % m_schemeSize];
		const tcu::Vec4			referenceValue(component, component, component, 1.0f);

		VkDescriptorBufferInfo& info = variables.descriptorsBufferInfos[primeIdx];
		deMemcpy(&data, buffer + info.offset, sizeof(data));
		const tcu::Vec4			realValue = data.cnew;

		const tcu::Vec4			diff = tcu::absDiff(referenceValue, realValue);
		if (!tcu::boolAll(tcu::lessThanEqual(diff, threshold)))
			return false;
	}
	return true;
}

class UniformBufferInstance : virtual public CommonDescriptorInstance
{
public:
								UniformBufferInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
protected:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
};

UniformBufferInstance::UniformBufferInstance						(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			BINDING_UniformBuffer,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void UniformBufferInstance::createAndPopulateDescriptors			(IterateCommonVariables&					variables)
{
	BindingUniformBuffer::Data data;

	const deUint32				alignment	= static_cast<deUint32>(ut::DeviceProperties(m_context).physicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
	createBuffers(variables.descriptorsBufferInfos, variables.descriptorsBuffer, variables.validDescriptorCount, sizeof(data), alignment, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	unsigned char*				buffer		= static_cast<unsigned char*>(variables.descriptorsBuffer->alloc->getHostPtr());
	for (deUint32 infoIdx = 0; infoIdx < variables.validDescriptorCount; ++infoIdx)
	{
		const float				component	= m_colorScheme[infoIdx % m_schemeSize];
		VkDescriptorBufferInfo& info		= variables.descriptorsBufferInfos[infoIdx];
		data.c								= tcu::Vec4(component, component, component, 1.0f);
		deMemcpy(buffer + info.offset, &data, sizeof(data));
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);

	variables.dataAlignment = deAlign64(sizeof(data), alignment);
}

class StorageTexelInstance : public CommonDescriptorInstance
{
public:
								StorageTexelInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);

	virtual bool				verifyVertexWriteResults			(IterateCommonVariables&					variables);
};

StorageTexelInstance::StorageTexelInstance							(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			BINDING_StorageTexelBuffer,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void StorageTexelInstance::createAndPopulateDescriptors			(IterateCommonVariables&					variables)
{
	const VkExtent3D			imageExtent			= { 4, 4, 1 };
	const deUint32				imageSize			= ut::computeImageSize(imageExtent, m_colorFormat);

	createBuffers(variables.descriptorsBufferInfos, variables.descriptorsBuffer, variables.validDescriptorCount, imageSize, sizeof(tcu::Vec4), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	createBuffersViews(variables.descriptorsBufferViews, variables.descriptorsBufferInfos, m_colorFormat);

	for (deUint32 imageIdx = 0; imageIdx < variables.validDescriptorCount; ++imageIdx)
	{
		const float				component			= m_colorScheme[imageIdx % m_schemeSize];
		const PixelBufferAccess pa					= getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer);

		tcu::clear(pa, m_clearColor);
		pa.setPixel(tcu::Vec4(component, component, component, 1.0f), 0, 0);
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
}

bool StorageTexelInstance::verifyVertexWriteResults(IterateCommonVariables&					variables)
{
	const VkExtent3D			imageExtent		= { 4, 4, 1 };
	const tcu::Vec4				threshold		(0.002f, 0.002f, 0.002f, 0.002f);
	const std::vector<deUint32>	primes			= ut::generatePrimes(variables.availableDescriptorCount);

	for (deUint32 primeIdx = 0; primeIdx < variables.validDescriptorCount; ++primeIdx)
	{
		const deUint32			prime		= primes[primeIdx];
		const float				component	= m_colorScheme[( prime % variables.validDescriptorCount ) % m_schemeSize];
		const tcu::Vec4			referenceValue(component, component, component, 1.0f);

		const PixelBufferAccess pa			= getPixelAccess(primeIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer);
		const tcu::Vec4			realValue	= pa.getPixel(1, 0);

		const tcu::Vec4			diff		= tcu::absDiff(referenceValue, realValue);
		if (!tcu::boolAll(tcu::lessThanEqual(diff, threshold)))
			return false;
	}
	return true;
}

class UniformTexelInstance : public CommonDescriptorInstance
{
public:
								UniformTexelInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
};

UniformTexelInstance::UniformTexelInstance							(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
			BINDING_UniformTexelBuffer,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void UniformTexelInstance::createAndPopulateDescriptors				(IterateCommonVariables&					variables)
{
	const VkExtent3D			imageExtent	= { 4, 4, 1 };
	const deUint32				imageSize	= ut::computeImageSize(imageExtent, m_colorFormat);

	createBuffers(variables.descriptorsBufferInfos, variables.descriptorsBuffer, variables.validDescriptorCount, imageSize, sizeof(tcu::Vec4), VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
	createBuffersViews(variables.descriptorsBufferViews, variables.descriptorsBufferInfos, m_colorFormat);

	for (deUint32 imageIdx = 0; imageIdx < variables.validDescriptorCount; ++imageIdx)
	{
		const float				component	= m_colorScheme[imageIdx % m_schemeSize];
		const PixelBufferAccess	pa			= getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer);

		tcu::clear(pa, tcu::Vec4(component, component, component, 1.0f));
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
}

class DynamicBuffersInstance : virtual public CommonDescriptorInstance
{
public:
	DynamicBuffersInstance											(Context&									context,
																	 const TestParams&							testParams)
		: CommonDescriptorInstance(context, testParams) {}

protected:
	virtual tcu::TestStatus		iterate								(void);
	virtual void				updateDescriptors					(IterateCommonVariables&					variables);
};

void DynamicBuffersInstance::updateDescriptors						(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.dataAlignment);

	VkDescriptorBufferInfo	bufferInfo =
	{
		*variables.descriptorsBuffer.get()->buffer,
		0,	// always 0, it will be taken from pDynamicOffsets
		variables.dataAlignment
	};

	VkWriteDescriptorSet updateInfo =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// sType
		DE_NULL,										// pNext
		*variables.descriptorSet,						// descriptorSet
		m_testParams.descriptorBinding,					// descriptorBinding;
		0,	// to be set in below loop					// dstArrayElement
		1u,												// descriptorCount
		m_testParams.descriptorType,					// descriptorType
		DE_NULL,										// pImageInfo
		&bufferInfo,									// pBufferInfo
		DE_NULL											// pTexelBufferView
	};

	deUint32 descIdx = 0;
	const std::vector<deUint32> primes = ut::generatePrimes(variables.availableDescriptorCount);
	for (deUint32 validIdx = 0; validIdx < variables.validDescriptorCount; ++validIdx)
	{
		for (; descIdx < primes[validIdx]; ++descIdx)
		{
			updateInfo.dstArrayElement			= descIdx;
			m_vki.updateDescriptorSets	(m_vkd, 1u, &updateInfo, 0u, DE_NULL);
		}

		updateInfo.dstArrayElement				= primes[validIdx];
		m_vki.updateDescriptorSets		(m_vkd, 1u, &updateInfo, 0u, DE_NULL);

		++descIdx;
	}
	for (; descIdx < variables.availableDescriptorCount; ++descIdx)
	{
		updateInfo.dstArrayElement = descIdx;
		m_vki.updateDescriptorSets(m_vkd, 1u, &updateInfo, 0u, DE_NULL);
	}
}

tcu::TestStatus	DynamicBuffersInstance::iterate						(void)
{
	IterateCommonVariables	v;
	iterateCommandSetup		(v);

	ut::UpdatablePixelBufferAccessPtr	programResult;
	ut::UpdatablePixelBufferAccessPtr	referenceResult;
	bool firstPass = true;

	DE_ASSERT(v.dataAlignment);

	std::vector<deUint32> dynamicOffsets;

	deUint32 descIdx = 0;
	const std::vector<deUint32> primes = ut::generatePrimes(v.availableDescriptorCount);
	for (deUint32 validIdx = 0; validIdx < v.validDescriptorCount; ++validIdx)
	{
		for (; descIdx < primes[validIdx]; ++descIdx)
		{
			dynamicOffsets.push_back(0);
		}

		dynamicOffsets.push_back(static_cast<deUint32>(validIdx * v.dataAlignment));

		++descIdx;
	}
	for (; descIdx < v.availableDescriptorCount; ++descIdx)
	{
		dynamicOffsets.push_back(0);
	}

	// Unfortunatelly not lees and not more, only exactly
	DE_ASSERT(dynamicOffsets.size() == v.availableDescriptorCount);

	const VkDescriptorSet	descriptorSets[] = { *v.descriptorSet };

	v.renderArea.extent.width	= m_testParams.frameResolution.width/4;
	v.renderArea.extent.height	= m_testParams.frameResolution.height/4;

	for (int x = 0; x < 4; x++)
		for (int y= 0; y < 4; y++)
		{

			v.renderArea.offset.x		= x * m_testParams.frameResolution.width/4;
			v.renderArea.offset.y		= y * m_testParams.frameResolution.height/4;

			iterateCommandBegin		(v, firstPass);
			firstPass = false;

	m_vki.cmdBindDescriptorSets(
		*v.commandBuffer,						// commandBuffer
		VK_PIPELINE_BIND_POINT_GRAPHICS,		// pipelineBindPoint
		*v.pipelineLayout,						// layout
		0u,										// firstSet
		DE_LENGTH_OF_ARRAY(descriptorSets),		// descriptorSetCount
		descriptorSets,							// pDescriptorSets
		v.availableDescriptorCount,				// dynamicOffsetCount
		dynamicOffsets.data());					// pDynamicOffsets

			vk::VkRect2D scissor = makeRect2D(v.renderArea.offset.x, v.renderArea.offset.y, v.renderArea.extent.width, v.renderArea.extent.height);
	m_vki.cmdSetScissor(*v.commandBuffer, 0u, 1u, &scissor);

	vk::beginRenderPass	(m_vki, *v.commandBuffer, *v.renderPass, *v.frameBuffer->buffer, v.renderArea, m_clearColor);
			m_vki.cmdDraw			(*v.commandBuffer, v.vertexCount, 1u, 0u, 0u);
	vk::endRenderPass	(m_vki, *v.commandBuffer);

			iterateCommandEnd(v, programResult, referenceResult);
			programResult->invalidate();
		}

	return (iterateVerifyResults(v, programResult, referenceResult) ? tcu::TestStatus::pass : tcu::TestStatus::fail)("");
}

class DynamicStorageBufferInstance : public DynamicBuffersInstance, public StorageBufferInstance
{
public:
	DynamicStorageBufferInstance									(Context&					context,
																	 const TestCaseParams&		testCaseParams);
	tcu::TestStatus		iterate										(void);
	void				createAndPopulateDescriptors				(IterateCommonVariables&	variables);
	void				updateDescriptors							(IterateCommonVariables&	variables);
	bool				verifyVertexWriteResults					(IterateCommonVariables&	variables);
};

DynamicStorageBufferInstance::DynamicStorageBufferInstance			(Context&					context,
																	 const TestCaseParams&		testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
			BINDING_StorageBufferDynamic,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams)),
			DynamicBuffersInstance(context, m_testParams), StorageBufferInstance(context, testCaseParams)
{
}

tcu::TestStatus	DynamicStorageBufferInstance::iterate(void)
{
	return DynamicBuffersInstance::iterate();
}

void DynamicStorageBufferInstance::createAndPopulateDescriptors(IterateCommonVariables&			variables)
{
	StorageBufferInstance::createAndPopulateDescriptors(variables);
}

void DynamicStorageBufferInstance::updateDescriptors(IterateCommonVariables&					variables)
{
	DynamicBuffersInstance::updateDescriptors(variables);
}

bool DynamicStorageBufferInstance::verifyVertexWriteResults(IterateCommonVariables&				variables)
{
	return StorageBufferInstance::verifyVertexWriteResults(variables);
}

class DynamicUniformBufferInstance : public DynamicBuffersInstance, public UniformBufferInstance
{
public:
	DynamicUniformBufferInstance									(Context&					context,
																	 const TestCaseParams&		testCaseParams);
	tcu::TestStatus		iterate(void);
	void				createAndPopulateDescriptors(IterateCommonVariables&					variables);
	void				updateDescriptors(IterateCommonVariables&								variables);
};

DynamicUniformBufferInstance::DynamicUniformBufferInstance			(Context&					context,
																	 const TestCaseParams&		testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			BINDING_UniformBufferDynamic,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			false,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams)),
			DynamicBuffersInstance(context, m_testParams), UniformBufferInstance(context, testCaseParams)
{
}

tcu::TestStatus DynamicUniformBufferInstance::iterate(void)
{
	return DynamicBuffersInstance::iterate();
}

void DynamicUniformBufferInstance::createAndPopulateDescriptors(IterateCommonVariables&			variables)
{
	UniformBufferInstance::createAndPopulateDescriptors(variables);
}

void DynamicUniformBufferInstance::updateDescriptors(IterateCommonVariables&					variables)
{
	DynamicBuffersInstance::updateDescriptors(variables);
}

class InputAttachmentInstance : public CommonDescriptorInstance
{
public:
								InputAttachmentInstance				(Context&									context,
																	const TestCaseParams&						testCaseParams);
private:
	virtual Move<VkRenderPass>	createRenderPass					(const IterateCommonVariables&				variables);
	virtual void				createFramebuffer					(ut::FrameBufferSp&							frameBuffer,
																	 VkRenderPass								renderPass,
																	 const IterateCommonVariables&				variables);
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
};

InputAttachmentInstance::InputAttachmentInstance					(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			BINDING_InputAttachment,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			true,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void InputAttachmentInstance::createAndPopulateDescriptors			(IterateCommonVariables&					variables)
{
	createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, variables.descriptorsBuffer,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_testParams.frameResolution, m_colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, variables.validDescriptorCount);
	createImagesViews(variables.descriptorImageViews, variables.descriptorsImages, m_colorFormat);

	for (deUint32 descriptorIdx = 0; descriptorIdx < variables.validDescriptorCount; ++descriptorIdx)
	{
		const float						component	= m_colorScheme[descriptorIdx % m_schemeSize];
		const tcu::PixelBufferAccess	pa			= getPixelAccess(descriptorIdx, m_testParams.frameResolution, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer);
		tcu::clear(pa, tcu::Vec4(component, component, component, 1.0f));
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
}

Move<VkRenderPass> InputAttachmentInstance::createRenderPass		(const IterateCommonVariables&				variables)
{
	std::vector<VkAttachmentDescription>	attachmentDescriptions;
	std::vector<VkAttachmentReference>		inputAttachmentRefs;

	const VkAttachmentDescription	colorAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags;
		m_colorFormat,								// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					finalLayout;
	};
	const VkAttachmentReference		colorAttachmentRef =
	{
		0u,												// deUint32							attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		// VkImageLayout					layout;
	};
	attachmentDescriptions.push_back(colorAttachmentDescription);

	// build input atachments
	{
		const std::vector<deUint32>	primes = ut::generatePrimes(variables.availableDescriptorCount);
		const deUint32 inputCount = static_cast<deUint32>(variables.descriptorImageViews.size());
		for (deUint32 inputIdx = 0; inputIdx < inputCount; ++inputIdx)
		{
			// primes holds the indices of input attachments for shader binding 10 which has input_attachment_index=1
			deUint32 nextInputAttachmentIndex = primes[inputIdx] + 1;

			// Fill up the subpass description's input attachments with unused attachments forming gaps to the next referenced attachment
			for (deUint32 unusedIdx = static_cast<deUint32>(inputAttachmentRefs.size()); unusedIdx < nextInputAttachmentIndex; ++unusedIdx)
			{
				const VkAttachmentReference		inputAttachmentRef =
				{
					VK_ATTACHMENT_UNUSED,						// deUint32							attachment;
					VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout					layout;
				};

				inputAttachmentRefs.push_back(inputAttachmentRef);
			}

			const VkAttachmentDescription	inputAttachmentDescription =
			{
				VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,		// VkAttachmentDescriptionFlags		flags;
				variables.descriptorsImages[inputIdx]->format,	// VkFormat							format;
				VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_LOAD,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,					// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,				// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,				// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_GENERAL							// VkImageLayout					finalLayout;
			};

			const VkAttachmentReference		inputAttachmentRef =
			{
				inputIdx + 1,								// deUint32							attachment;
				VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout					layout;
			};

			inputAttachmentRefs.push_back(inputAttachmentRef);
			attachmentDescriptions.push_back(inputAttachmentDescription);
		}
	}

	const VkSubpassDescription		subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		static_cast<deUint32>(inputAttachmentRefs.size()),	// deUint32							inputAttachmentCount;
		inputAttachmentRefs.data(),							// const VkAttachmentReference*		pInputAttachments;
		1u,													// deUint32							colorAttachmentCount;
		&colorAttachmentRef,								// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo	renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments;
		1u,														// deUint32							subpassCount;
		&subpassDescription,									// const VkSubpassDescription*		pSubpasses;
		0u,														// deUint32							dependencyCount;
		DE_NULL													// const VkSubpassDependency*		pDependencies;
	};

	return vk::createRenderPass(m_vki, m_vkd, &renderPassInfo);
}

void InputAttachmentInstance::createFramebuffer						(ut::FrameBufferSp&							frameBuffer,
																	 VkRenderPass								renderPass,
																	 const IterateCommonVariables&				variables)
{
	std::vector<VkImageView>			inputAttachments;
	const deUint32 viewCount = static_cast<deUint32>(variables.descriptorImageViews.size());
	inputAttachments.resize(viewCount);
	for (deUint32 viewIdx = 0; viewIdx < viewCount; ++viewIdx)
	{
		inputAttachments[viewIdx] = **variables.descriptorImageViews[viewIdx];
	}
	ut::createFrameBuffer(frameBuffer, m_context, m_testParams.frameResolution, m_colorFormat, renderPass, viewCount, inputAttachments.data());
}

class SamplerInstance : public CommonDescriptorInstance
{
public:
								SamplerInstance						(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
	virtual void				updateDescriptors					(IterateCommonVariables&					variables);
};

SamplerInstance::SamplerInstance									(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_SAMPLER,
			BINDING_Sampler,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			BINDING_SampledImage,
			true,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void SamplerInstance::updateDescriptors								(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.descriptorsImages.size()		== 1);
	DE_ASSERT(variables.descriptorImageViews.size()		== 1);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== 1);
	DE_ASSERT(m_testParams.additionalDescriptorType		== VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	DE_ASSERT(variables.descriptorSamplers.size()		== variables.validDescriptorCount);

	// update an image
	{
		const VkDescriptorImageInfo imageInfo =
		{
			static_cast<VkSampler>(0),
			**variables.descriptorImageViews[0],
			VK_IMAGE_LAYOUT_GENERAL
		};

		const VkWriteDescriptorSet writeInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// sType
			DE_NULL,										// pNext
			*variables.descriptorSet,						// descriptorSet
			BINDING_SampledImage,							// descriptorBinding;
			0,												// elementIndex
			1u,												// descriptorCount
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				// descriptorType
			&imageInfo,										// pImageInfo
			DE_NULL,										// pBufferInfo
			DE_NULL											// pTexelBufferView
		};

		m_vki.updateDescriptorSets(m_vkd, 1u, &writeInfo, 0u, DE_NULL);
	}

	// update samplers
	CommonDescriptorInstance::updateDescriptors(variables);
}

void SamplerInstance::createAndPopulateDescriptors					(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.descriptorsImages.size()		== 0);
	DE_ASSERT(variables.descriptorImageViews.size()		== 0);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== 0);
	DE_ASSERT(variables.descriptorSamplers.size()		== 0);

	// create and populate an image
	{
		VkExtent3D imageExtent = m_testParams.frameResolution;
		if (m_testParams.usesMipMaps)
		{
			imageExtent.width *= 2;
			imageExtent.height *= 2;
		}

		createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, variables.descriptorsBuffer,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageExtent, m_colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, 1, m_testParams.usesMipMaps);
		createImagesViews(variables.descriptorImageViews, variables.descriptorsImages, m_colorFormat);

		PixelBufferAccess pa = getPixelAccess(0, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, m_testParams.usesMipMaps ? 1 : 0);

		for (deUint32 y = 0, pixelNum = 0; y < m_testParams.frameResolution.height; ++y)
		{
			for (deUint32 x = 0; x < m_testParams.frameResolution.width; ++x, ++pixelNum)
			{
				const float		component	= m_colorScheme[(pixelNum % variables.validDescriptorCount) % m_schemeSize];
				pa.setPixel(tcu::Vec4(component, component, component, 1.0f), x, y);
			}
		}

		vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
	}

	const tcu::Sampler sampler(
		tcu::Sampler::CLAMP_TO_BORDER,															// wrapS
		tcu::Sampler::CLAMP_TO_BORDER,															// wrapT
		tcu::Sampler::CLAMP_TO_BORDER,															// wrapR
		m_testParams.usesMipMaps ? tcu::Sampler::LINEAR_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// minFilter
		m_testParams.usesMipMaps ? tcu::Sampler::LINEAR_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// magFilter
		0.0f,																					// lodTreshold
		true);																					// normalizeCoords
	const VkSamplerCreateInfo createInfo = vk::mapSampler(sampler, vk::mapVkFormat(m_colorFormat));
	variables.descriptorSamplers.resize(variables.validDescriptorCount);

	for (deUint32 samplerIdx = 0; samplerIdx < variables.validDescriptorCount; ++samplerIdx)
	{
		variables.descriptorSamplers[samplerIdx] = ut::SamplerSp(new Move<VkSampler>(vk::createSampler(m_vki, m_vkd, &createInfo)));
	}
}

class SampledImageInstance : public CommonDescriptorInstance
{
public:
								SampledImageInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
	virtual void				updateDescriptors					(IterateCommonVariables&					variables);
};

SampledImageInstance::SampledImageInstance							(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			BINDING_SampledImage,
			VK_DESCRIPTOR_TYPE_SAMPLER,
			BINDING_Sampler,
			true,
			performWritesInVertex(testCaseParams.descriptorType, context),
			testCaseParams))
{
}

void SampledImageInstance::updateDescriptors						(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.descriptorSamplers.size()		== 1);
	DE_ASSERT(variables.descriptorsImages.size()		== variables.validDescriptorCount);
	DE_ASSERT(variables.descriptorImageViews.size()		== variables.validDescriptorCount);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== variables.validDescriptorCount);

	// update a sampler
	{
		const VkDescriptorImageInfo samplerInfo =
		{
			**variables.descriptorSamplers[0],
			static_cast<VkImageView>(0),
			static_cast<VkImageLayout>(0)
		};

		const VkWriteDescriptorSet writeInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// sType
			DE_NULL,										// pNext
			*variables.descriptorSet,						// descriptorSet
			BINDING_Sampler,								// descriptorBinding;
			0,												// elementIndex
			1u,												// descriptorCount
			VK_DESCRIPTOR_TYPE_SAMPLER,						// descriptorType
			&samplerInfo,									// pImageInfo
			DE_NULL,										// pBufferInfo
			DE_NULL											// pTexelBufferView
		};

		m_vki.updateDescriptorSets(m_vkd, 1u, &writeInfo, 0u, DE_NULL);
	}

	// update images
	CommonDescriptorInstance::updateDescriptors(variables);
}

void SampledImageInstance::createAndPopulateDescriptors				(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.descriptorSamplers.size()		== 0);
	DE_ASSERT(variables.descriptorsImages.size()		== 0);
	DE_ASSERT(variables.descriptorImageViews.size()		== 0);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== 0);

	// create an only one sampler for all images
	{
		const tcu::Sampler sampler(
			tcu::Sampler::CLAMP_TO_BORDER,																// wrapS
			tcu::Sampler::CLAMP_TO_BORDER,																// wrapT
			tcu::Sampler::CLAMP_TO_BORDER,																// wrapR
			m_testParams.usesMipMaps ? tcu::Sampler::NEAREST_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// minFilter
			m_testParams.usesMipMaps ? tcu::Sampler::NEAREST_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// magFilter
			0.0f,																						// lodTreshold
			true);																						// normalizeCoords
		const VkSamplerCreateInfo createInfo = vk::mapSampler(sampler, vk::mapVkFormat(m_colorFormat));
		variables.descriptorSamplers.push_back(ut::SamplerSp(new Move<VkSampler>(vk::createSampler(m_vki, m_vkd, &createInfo))));
	}

	const VkExtent3D&			imageExtent = m_testParams.usesMipMaps ? bigImageExtent : smallImageExtent;

	createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, variables.descriptorsBuffer,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageExtent, m_colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, variables.validDescriptorCount, m_testParams.usesMipMaps);
	createImagesViews(variables.descriptorImageViews, variables.descriptorsImages, m_colorFormat);

	PixelBufferAccess			pixelAccess;
	for (deUint32 imageIdx = 0; imageIdx < variables.validDescriptorCount; ++imageIdx)
	{
		const float				component	= m_colorScheme[imageIdx % m_schemeSize];

		if (m_testParams.usesMipMaps)
		{
			const deUint32 mipCount = ut::computeMipMapCount(imageExtent);
			DE_ASSERT(mipCount >= 2);
			for (deUint32 mipIdx = 0; mipIdx < mipCount; ++mipIdx)
			{
				pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, mipIdx);
				tcu::clear(pixelAccess, m_clearColor);
			}

			pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, mipCount-1);
			pixelAccess.setPixel(tcu::Vec4(component, component, component, 1.0f), 0, 0);
		}
		else
		{
			pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, 0);
			pixelAccess.setPixel(tcu::Vec4(component, component, component, 1.0f), 0, 0);
		}
	}
	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
}

class CombinedImageInstance : public CommonDescriptorInstance
{
public:
								CombinedImageInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
	virtual void				updateDescriptors					(IterateCommonVariables&					variables);
};

CombinedImageInstance::CombinedImageInstance						(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams(VK_SHADER_STAGE_ALL_GRAPHICS,
			testCaseParams.descriptorType,
			BINDING_CombinedImageSampler,
			VK_DESCRIPTOR_TYPE_UNDEFINED,
			BINDING_Undefined,
			true,
			performWritesInVertex(testCaseParams.descriptorType),
			testCaseParams))
{
}

void CombinedImageInstance::updateDescriptors						(IterateCommonVariables&					variables)
{
	const std::vector<deUint32>	primes = ut::generatePrimes(variables.availableDescriptorCount);
	const deUint32				primeCount = static_cast<deUint32>(primes.size());

	DE_ASSERT(variables.descriptorSamplers.size()		== 1);
	DE_ASSERT(variables.descriptorsImages.size()		== primeCount);
	DE_ASSERT(variables.descriptorImageViews.size()		== primeCount);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== primeCount);

	for (deUint32 primeIdx = 0; primeIdx < primeCount; ++primeIdx)
	{
		const VkDescriptorImageInfo imageInfo =
		{
			**variables.descriptorSamplers[0],
			**variables.descriptorImageViews[primeIdx],
			VK_IMAGE_LAYOUT_GENERAL
		};

		const VkWriteDescriptorSet writeInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// sType
			DE_NULL,										// pNext
			*variables.descriptorSet,						// descriptorSet
			BINDING_CombinedImageSampler,					// descriptorBinding;
			primes[primeIdx],								// elementIndex
			1u,												// descriptorCount
			m_testParams.descriptorType,					// descriptorType
			&imageInfo,										// pImageInfo
			DE_NULL,										// pBufferInfo
			DE_NULL											// pTexelBufferView
		};

		m_vki.updateDescriptorSets(m_vkd, 1u, &writeInfo, 0u, DE_NULL);
	}
}

void CombinedImageInstance::createAndPopulateDescriptors			(IterateCommonVariables&					variables)
{
	DE_ASSERT(variables.descriptorSamplers.size()		== 0);
	DE_ASSERT(variables.descriptorsImages.size()		== 0);
	DE_ASSERT(variables.descriptorImageViews.size()		== 0);
	DE_ASSERT(variables.descriptorsBufferInfos.size()	== 0);
	DE_ASSERT(variables.descriptorSamplers.size()		== 0);

	const tcu::Sampler sampler(
		tcu::Sampler::CLAMP_TO_BORDER,																// wrapS
		tcu::Sampler::CLAMP_TO_BORDER,																// wrapT
		tcu::Sampler::CLAMP_TO_BORDER,																// wrapR
		m_testParams.usesMipMaps ? tcu::Sampler::NEAREST_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// minFilter
		m_testParams.usesMipMaps ? tcu::Sampler::NEAREST_MIPMAP_NEAREST : tcu::Sampler::NEAREST,	// magFilter
		0.0f,																						// lodTreshold
		true);																						// normalizeCoords
	const VkSamplerCreateInfo	createInfo = vk::mapSampler(sampler, vk::mapVkFormat(m_colorFormat));
	variables.descriptorSamplers.push_back(ut::SamplerSp(new Move<VkSampler>(vk::createSampler(m_vki, m_vkd, &createInfo))));

	const VkExtent3D&			imageExtent = m_testParams.usesMipMaps ? bigImageExtent : smallImageExtent;
	createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, variables.descriptorsBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		imageExtent, m_colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, variables.validDescriptorCount, m_testParams.usesMipMaps);
	createImagesViews(variables.descriptorImageViews, variables.descriptorsImages, m_colorFormat);

	PixelBufferAccess			pixelAccess;
	for (deUint32 imageIdx = 0; imageIdx < variables.validDescriptorCount; ++imageIdx)
	{
		const float				component = m_colorScheme[imageIdx % m_schemeSize];

		if (m_testParams.usesMipMaps)
		{
			const deUint32	mipCount = ut::computeMipMapCount(imageExtent);
			DE_ASSERT(mipCount >= 2);
			for (deUint32 mipIdx = 0; mipIdx < mipCount; ++mipIdx)
			{
				pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, mipIdx);
				tcu::clear(pixelAccess, m_clearColor);
			}

			pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, mipCount-1);
			pixelAccess.setPixel(tcu::Vec4(component, component, component, 1.0f), 0, 0);
		}
		else
		{
			pixelAccess = getPixelAccess(imageIdx, imageExtent, m_colorFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer, 0);
			pixelAccess.setPixel(tcu::Vec4(component, component, component, 1.0f), 0, 0);
		}
	}

	vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
}

class StorageImageInstance : public CommonDescriptorInstance
{
public:
								StorageImageInstance				(Context&									context,
																	 const TestCaseParams&						testCaseParams);
private:
	virtual tcu::TestStatus		iterate								(void);
	virtual void				createAndPopulateDescriptors		(IterateCommonVariables&					variables);
	virtual void				updateDescriptors					(IterateCommonVariables&					variables);
	virtual void				iterateCollectResults				(ut::UpdatablePixelBufferAccessPtr&			result,
																	 const IterateCommonVariables&				variables,
																	 bool										fromTest);
	ut::BufferHandleAllocSp		m_buffer;
	const deUint32				m_fillColor;
	typedef deUint32			m_imageFormat_t;
};

StorageImageInstance::StorageImageInstance							(Context&									context,
																	 const TestCaseParams&						testCaseParams)
	: CommonDescriptorInstance(context,
		TestParams	(VK_SHADER_STAGE_COMPUTE_BIT,
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					BINDING_StorageImage,
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					(BINDING_StorageImage + 1),
					true,
					performWritesInVertex(testCaseParams.descriptorType, context),
					testCaseParams))
	, m_buffer		()
	, m_fillColor	(10)
{
}

void StorageImageInstance::updateDescriptors						(IterateCommonVariables&					variables)
{
	// update image at last index
	{
		VkDescriptorImageInfo		imageInfo =
		{
			static_cast<VkSampler>(0),
			**variables.descriptorImageViews[variables.validDescriptorCount],
			VK_IMAGE_LAYOUT_GENERAL
		};

		const VkWriteDescriptorSet writeInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// sType
			DE_NULL,									// pNext
			*variables.descriptorSet,					// descriptorSet
			m_testParams.additionalDescriptorBinding,	// descriptorBinding;
			0,											// elementIndex
			1u,											// descriptorCount
			m_testParams.additionalDescriptorType,		// descriptorType
			&imageInfo,									// pImageInfo
			DE_NULL,									// pBufferInfo
			DE_NULL										// pTexelBufferView
		};

		m_vki.updateDescriptorSets(m_vkd, 1u, &writeInfo, 0u, DE_NULL);
	}

	// update rest images
	CommonDescriptorInstance::updateDescriptors(variables);
}

void StorageImageInstance::createAndPopulateDescriptors				(IterateCommonVariables&					variables)
{
	const VkFormat				imageFormat = ut::mapType2vkFormat<m_imageFormat_t>::value;
	const VkBufferUsageFlags	bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	// create descriptor buffer, images and views
	{
		const VkExtent3D			imageExtent = { 4, 4, 1 };

		createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, variables.descriptorsBuffer,
			bufferUsage, imageExtent, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, variables.validDescriptorCount);

		for (deUint32 imageIdx = 0; imageIdx < variables.validDescriptorCount; ++imageIdx)
		{
			const PixelBufferAccess pa = getPixelAccess(imageIdx, imageExtent, imageFormat, variables.descriptorsBufferInfos, variables.descriptorsBuffer);
			tcu::clear(pa, tcu::UVec4(m_fillColor));
		}
		vk::flushAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
	}

	// create additional image that will be used as index container
	{
		createImages(variables.descriptorsImages, variables.descriptorsBufferInfos, m_buffer,
			bufferUsage, m_testParams.frameResolution, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, 1);

		// populate buffer
		const std::vector<deUint32>	primes = ut::generatePrimes(variables.availableDescriptorCount);
		const PixelBufferAccess pa = getPixelAccess(variables.validDescriptorCount, m_testParams.frameResolution, imageFormat, variables.descriptorsBufferInfos, m_buffer);
		for (deUint32 y = 0, pixel = 0; y < m_testParams.frameResolution.height; ++y)
		{
			for (deUint32 x = 0; x < m_testParams.frameResolution.width; ++x, ++pixel)
			{
				const deUint32 component = primes[pixel % variables.validDescriptorCount];
				pa.setPixel(tcu::UVec4(component), x, y);
			}
		}

		// save changes
		vk::flushAlloc(m_vki, m_vkd, *m_buffer->alloc);
	}

	// create views for all previously created images
	createImagesViews(variables.descriptorImageViews, variables.descriptorsImages, imageFormat);
}

tcu::TestStatus StorageImageInstance::iterate						(void)
{
	IterateCommonVariables	v;
	iterateCommandSetup		(v);
	iterateCommandBegin		(v);

	ut::UpdatablePixelBufferAccessPtr	programResult;
	ut::UpdatablePixelBufferAccessPtr	referenceResult;

	if (m_testParams.updateAfterBind)
	{
		updateDescriptors	(v);
	}

	copyBuffersToImages		(v);

	m_vki.cmdDispatch		(*v.commandBuffer,
							m_testParams.calculateInLoop ? 1 : v.renderArea.extent.width,
							m_testParams.calculateInLoop ? 1 : v.renderArea.extent.height,
							1);

	copyImagesToBuffers		(v);

	iterateCommandEnd(v, programResult, referenceResult, false);

	return ( iterateVerifyResults(v, programResult, referenceResult) ? tcu::TestStatus::pass : tcu::TestStatus::fail)("");
}

void StorageImageInstance::iterateCollectResults					(ut::UpdatablePixelBufferAccessPtr&			result,
																	 const IterateCommonVariables&				variables,
																	 bool										fromTest)
{
	result = ut::UpdatablePixelBufferAccessPtr(new ut::PixelBufferAccessAllocation(
		vk::mapVkFormat(ut::mapType2vkFormat<m_imageFormat_t>::value), m_testParams.frameResolution));
	const PixelBufferAccess& dst = *result.get();

	if (fromTest)
	{
		vk::invalidateAlloc(m_vki, m_vkd, *variables.descriptorsBuffer->alloc);
		for (deUint32 y = 0, pixelNum = 0; y < m_testParams.frameResolution.height; ++y)
		{
			for (deUint32 x = 0; x < m_testParams.frameResolution.width; ++x, ++pixelNum)
			{
				const deUint32 imageIdx = pixelNum % variables.validDescriptorCount;
				const PixelBufferAccess src = getPixelAccess(imageIdx,
					variables.descriptorsImages[imageIdx]->extent, variables.descriptorsImages[imageIdx]->format,
					variables.descriptorsBufferInfos, variables.descriptorsBuffer);
				dst.setPixel(tcu::Vector<m_imageFormat_t, 4>(src.getPixelT<m_imageFormat_t>(0, 0).x()), x, y);
			}
		}
	}
	else
	{
		std::vector<m_imageFormat_t> inc(variables.validDescriptorCount, m_fillColor);

		for (deUint32 invIdx = variables.lowerBound; invIdx < variables.upperBound; ++invIdx)
		{
			++inc[invIdx % variables.validDescriptorCount];
		}

		for (deUint32 invIdx = 0; invIdx < variables.vertexCount; ++invIdx)
		{
			const deUint32 row = invIdx / m_testParams.frameResolution.width;
			const deUint32 col = invIdx % m_testParams.frameResolution.width;
			const m_imageFormat_t color = inc[invIdx % variables.validDescriptorCount];
			dst.setPixel(tcu::Vector<m_imageFormat_t, 4>(color), col, row);
		}
	}
}

class DescriptorIndexingTestCase : public TestCase
{
	const TestCaseParams m_testCaseParams;
public:
	DescriptorIndexingTestCase (tcu::TestContext &context, const char *name, const char *description, const TestCaseParams& testCaseParams)
		: TestCase(context, name, description)
		, m_testCaseParams(testCaseParams)
	{
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const // override
	{
		switch (m_testCaseParams.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			return new StorageBufferInstance		(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			return new UniformBufferInstance		(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			return new StorageTexelInstance			(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			return new UniformTexelInstance			(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			return new DynamicStorageBufferInstance	(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			return new DynamicUniformBufferInstance	(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			return new InputAttachmentInstance		(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			return new SamplerInstance				(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return new SampledImageInstance			(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return new CombinedImageInstance		(context, m_testCaseParams);
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return new StorageImageInstance			(context, m_testCaseParams);
		default:
			TCU_THROW(InternalError, "Unknown Descriptor Type");
		}
		return DE_NULL;
	}

	virtual void checkSupport (vkt::Context& context) const
	{
		context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");

		const vk::VkPhysicalDeviceDescriptorIndexingFeaturesEXT& feats = context.getDescriptorIndexingFeatures();

		switch (m_testCaseParams.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			if (!(feats.shaderStorageBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over storage buffer descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingStorageBufferUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for storage buffer descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			if (!(feats.shaderUniformBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing for uniform buffer descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingUniformBufferUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for uniform buffer descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			if (!(feats.shaderStorageTexelBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing for storage texel buffer descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingStorageTexelBufferUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for storage texel buffer descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			if (!(feats.shaderUniformTexelBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing for uniform texel buffer descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingUniformTexelBufferUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for uniform texel buffer descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			if (!(feats.shaderStorageBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over storage buffer dynamic descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for storage buffer dynamic descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			if (!(feats.shaderUniformBufferArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over uniform buffer dynamic descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for uniform buffer dynamic descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			if (!(feats.shaderInputAttachmentArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over input attachment descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for input attachment descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			if (!(feats.shaderSampledImageArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over sampler descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingSampledImageUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for sampler descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			if (!(feats.shaderSampledImageArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over sampled image descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingSampledImageUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for sampled image descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			if (!(feats.shaderSampledImageArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over combined image sampler descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingSampledImageUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for combined image sampler descriptors is not supported.");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			if (!(feats.shaderStorageImageArrayNonUniformIndexing))
				TCU_THROW(NotSupportedError, "Non-uniform indexing over storage image descriptor arrays is not supported.");

			if (m_testCaseParams.updateAfterBind && !feats.descriptorBindingStorageImageUpdateAfterBind)
				TCU_THROW(NotSupportedError, "Update after bind for storage image descriptors is not supported.");
			break;
		default:
			DE_FATAL("Unknown Descriptor Type");
			break;
		}
	}

	void initAsmPrograms(SourceCollections&	programCollection) const
	{

		std::string(*genShaderSource)(VkShaderStageFlagBits, const TestCaseParams&, bool) = &CommonDescriptorInstance::getShaderAsm;

		deUint32 vulkan_version = VK_MAKE_VERSION(1, 2, 0);
		vk::SpirvVersion spirv_version = vk::SPIRV_VERSION_1_4;
		vk::SpirVAsmBuildOptions asm_options(vulkan_version, spirv_version);

		if (VK_SHADER_STAGE_VERTEX_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.spirvAsmSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false), &asm_options)
				<< (*genShaderSource)(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams, false);

			if (CommonDescriptorInstance::performWritesInVertex(m_testCaseParams.descriptorType))
			{
				programCollection.spirvAsmSources.add(
					ut::buildShaderName(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, true), &asm_options)
					<< (*genShaderSource)(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams, true);
			}
		}
		if (VK_SHADER_STAGE_FRAGMENT_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.spirvAsmSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false), &asm_options)
				<< (*genShaderSource)(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams, false);

			if (CommonDescriptorInstance::performWritesInVertex(m_testCaseParams.descriptorType))
			{
				programCollection.spirvAsmSources.add(
					ut::buildShaderName(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, true), &asm_options)
					<< (*genShaderSource)(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams, true);
			}
		}
		if (VK_SHADER_STAGE_COMPUTE_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.spirvAsmSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_COMPUTE_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false), &asm_options)
				<< (*genShaderSource)(VK_SHADER_STAGE_COMPUTE_BIT, m_testCaseParams, false);
		}
	}

	virtual void initPrograms (SourceCollections& programCollection) const
	{
		if (m_testCaseParams.minNonUniform) {
			initAsmPrograms(programCollection);
			return;
		}

		std::string(*genShaderSource)(VkShaderStageFlagBits, const TestCaseParams&, bool) = &CommonDescriptorInstance::getShaderSource;

		if (VK_SHADER_STAGE_VERTEX_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.glslSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false))
				<< glu::VertexSource((*genShaderSource)(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams, false));

			if (CommonDescriptorInstance::performWritesInVertex(m_testCaseParams.descriptorType))
			{
				programCollection.glslSources.add(
					ut::buildShaderName(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, true))
					<< glu::VertexSource((*genShaderSource)(VK_SHADER_STAGE_VERTEX_BIT, m_testCaseParams, true));
			}
		}
		if (VK_SHADER_STAGE_FRAGMENT_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.glslSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false))
				<< glu::FragmentSource((*genShaderSource)(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams, false));

			if (CommonDescriptorInstance::performWritesInVertex(m_testCaseParams.descriptorType))
			{
				programCollection.glslSources.add(
					ut::buildShaderName(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, true))
					<< glu::FragmentSource((*genShaderSource)(VK_SHADER_STAGE_FRAGMENT_BIT, m_testCaseParams, true));
			}
		}
		if (VK_SHADER_STAGE_COMPUTE_BIT & m_testCaseParams.stageFlags)
		{
			programCollection.glslSources.add(
				ut::buildShaderName(VK_SHADER_STAGE_COMPUTE_BIT, m_testCaseParams.descriptorType, m_testCaseParams.updateAfterBind, m_testCaseParams.calculateInLoop, m_testCaseParams.minNonUniform, false))
				<< glu::ComputeSource((*genShaderSource)(VK_SHADER_STAGE_COMPUTE_BIT, m_testCaseParams, false));
		}
	}
};

} // - unnamed namespace

void descriptorIndexingDescriptorSetsCreateTests (tcu::TestCaseGroup* group)
{
	struct TestCaseInfo
	{
		const char*		name;
		const char*		description;
		TestCaseParams	params;
	};

	tcu::TestContext&				context(group->getTestContext());

	TestCaseInfo casesAfterBindAndLoop[] =
	{
		{
			"storage_buffer", "Regular Storage Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"storage_texel_buffer", "Storage Texel Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"uniform_texel_buffer", "Uniform Texel Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind,
				false,	// calculateInLoop
				false,	// usesMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"storage_image", "Storage Image Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_SHADER_STAGE_COMPUTE_BIT,
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
	};

	for (int updateAfterBind = 0; updateAfterBind < 2; ++updateAfterBind)
	{
		for (int calculateInLoop = 0; calculateInLoop < 2; ++calculateInLoop)
		{
			for (deUint32 caseIdx = 0; caseIdx < DE_LENGTH_OF_ARRAY(casesAfterBindAndLoop); ++caseIdx)
			{
				TestCaseInfo&	info			(casesAfterBindAndLoop[caseIdx]);
				std::string		caseName		(info.name);
				std::string		caseDescription	(info.description);
				TestCaseParams	params			(info.params);

				caseName				+= (updateAfterBind	? "_after_bind"	: "");
				caseName				+= (calculateInLoop	? "_in_loop"	: "");

				caseDescription			+= (updateAfterBind	? " After Bind"	: "");
				caseDescription			+= (calculateInLoop ? " In Loop"	: "");

				params.updateAfterBind	= updateAfterBind	? true			: false;
				params.calculateInLoop	= calculateInLoop	? true			: false;

				group->addChild(new DescriptorIndexingTestCase(context, caseName.c_str(), caseDescription.c_str(), params));
			}
		}
	}

	TestCaseInfo casesAfterBindAndLoopAndLOD[] =
	{
		{
			"sampler", "Sampler Descriptors",
			{
				VK_DESCRIPTOR_TYPE_SAMPLER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"sampled_image", "Sampled Image Descriptors",
			{
				VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"combined_image_sampler", "Combined Image Sampler Descriptors",
			{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
	};

	for (int updateAfterBind = 0; updateAfterBind < 2; ++updateAfterBind)
	{
		for (int calculateInLoop = 0; calculateInLoop < 2; ++calculateInLoop)
		{
			for (int usesMipMaps = 0; usesMipMaps < 2; ++usesMipMaps)
			{
				for (deUint32 caseIdx = 0; caseIdx < DE_LENGTH_OF_ARRAY(casesAfterBindAndLoopAndLOD); ++caseIdx)
				{
					TestCaseInfo&	info			(casesAfterBindAndLoopAndLOD[caseIdx]);
					std::string		caseName		(info.name);
					std::string		caseDescription	(info.description);
					TestCaseParams	params			(info.params);

					caseName				+= (updateAfterBind	? "_after_bind"	: "");
					caseName				+= (calculateInLoop ? "_in_loop"	: "");
					caseName				+= (usesMipMaps		? "_with_lod"	: "");

					caseDescription			+= (updateAfterBind	? " After Bind"	: "");
					caseDescription			+= (calculateInLoop	? " In Loop"	: "");
					caseDescription			+= (usesMipMaps		? " Use LOD"	: "");

					params.updateAfterBind	= updateAfterBind	? true			: false;
					params.calculateInLoop	= calculateInLoop	? true			: false;
					params.usesMipMaps		= usesMipMaps		? true			: false;

					group->addChild(new DescriptorIndexingTestCase(context, caseName.c_str(), caseDescription.c_str(), params));
				}
			}
		}
	}

	TestCaseInfo casesNonAfterBindAndLoop[] =
	{
		{
			"uniform_buffer", "Regular Uniform Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"storage_buffer_dynamic", "Dynamic Storage Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"uniform_buffer_dynamic", "Dynamic Uniform Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"input_attachment", "Input Attachment Descriptors",
			{
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				false,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
	};

	for (int calculateInLoop = 0; calculateInLoop < 2; ++calculateInLoop)
	{
		for (deUint32 caseIdx = 0; caseIdx < DE_LENGTH_OF_ARRAY(casesNonAfterBindAndLoop); ++caseIdx)
		{
			TestCaseInfo&	info(casesNonAfterBindAndLoop[caseIdx]);
			std::string		caseName(info.name);
			std::string		caseDescription(info.description);
			TestCaseParams	params(info.params);

			caseName				+= (calculateInLoop	? "_in_loop"	: "");

			caseDescription			+= (calculateInLoop ? " In Loop"	: "");

			params.calculateInLoop	= calculateInLoop	? true			: false;

			group->addChild(new DescriptorIndexingTestCase(context, caseName.c_str(), caseDescription.c_str(), params));
		}
	}

	// SPIR-V Asm Tests
	// Tests that have the minimum necessary NonUniform decorations.
	// sampler and sampled_image GLSL already have minimum NonUniform decorations.

	TestCaseInfo casesMinNonUniform[] =
	{
		{
			"storage_buffer", "Regular Storage Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"storage_texel_buffer", "Storage Texel Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"uniform_texel_buffer", "Uniform Texel Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind,
				false,	// calculateInLoop
				false,	// usesMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"uniform_buffer", "Regular Uniform Buffer Descriptors",
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"combined_image_sampler", "Combined Image Sampler Descriptors",
			{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// usesMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"combined_image_sampler", "Combined Image Sampler Descriptors",
			{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				true,	// usesMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
		{
			"storage_image", "Storage Image Descriptors",
			{
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_SHADER_STAGE_COMPUTE_BIT,
				RESOLUTION,
				false,	// updateAfterBind
				false,	// calculateInLoop
				false,	// useMipMaps
				true,	// minNonUniform
				FUZZY_COMPARE, CMP_THRESHOLD
			}
		},
	};

	for (deUint32 caseIdx = 0; caseIdx < DE_LENGTH_OF_ARRAY(casesMinNonUniform); ++caseIdx)
	{
		TestCaseInfo&	info(casesMinNonUniform[caseIdx]);
		std::string		caseName(info.name);
		std::string		caseDescription(info.description);
		TestCaseParams	params(info.params);

		if (params.usesMipMaps) {
			caseName += "_with_lod";
		}
		caseName += "_minNonUniform";

		caseDescription += " With Minimum NonUniform Decorations";

		TestCase* tc = new DescriptorIndexingTestCase(context, caseName.c_str(), caseDescription.c_str(), params);
		group->addChild(tc);
		// group->addChild(new DescriptorIndexingTestCase(context, caseName.c_str(), caseDescription.c_str(), params));
	}
}

} // - DescriptorIndexing
} // - vkt
