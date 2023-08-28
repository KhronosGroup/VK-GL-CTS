/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Vertex Input Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineVertexInputTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuImageCompare.hpp"
#include "deFloat16.h"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <map>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

constexpr int kMaxComponents = 4;

bool isSupportedVertexFormat (Context& context, VkFormat format)
{
	if (isVertexFormatDouble(format) && !context.getDeviceFeatures().shaderFloat64)
		return false;

	VkFormatProperties  formatProps;
	deMemset(&formatProps, 0, sizeof(VkFormatProperties));
	context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), format, &formatProps);

	return (formatProps.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0u;
}

float getRepresentableDifferenceUnorm (VkFormat format)
{
	DE_ASSERT(isVertexFormatUnorm(format) || isVertexFormatSRGB(format));

	return 1.0f / float((1 << (getVertexFormatComponentSize(format) * 8)) - 1);
}

float getRepresentableDifferenceUnormPacked(VkFormat format, deUint32 componentNdx)
{
	DE_ASSERT((isVertexFormatUnorm(format) || isVertexFormatSRGB(format)) && isVertexFormatPacked(format));

	return 1.0f / float((1 << (getPackedVertexFormatComponentWidth(format, componentNdx))) - 1);
}

float getRepresentableDifferenceSnorm (VkFormat format)
{
	DE_ASSERT(isVertexFormatSnorm(format));

	return 1.0f / float((1 << (getVertexFormatComponentSize(format) * 8 - 1)) - 1);
}

float getRepresentableDifferenceSnormPacked(VkFormat format, deUint32 componentNdx)
{
	DE_ASSERT(isVertexFormatSnorm(format) && isVertexFormatPacked(format));

	return 1.0f / float((1 << (getPackedVertexFormatComponentWidth(format, componentNdx) - 1)) - 1);
}

deUint32 getNextMultipleOffset (deUint32 divisor, deUint32 value)
{
	if (value % divisor == 0)
		return 0;
	else
		return divisor - (value % divisor);
}

class VertexInputTest : public vkt::TestCase
{
public:
	enum GlslType
	{
		GLSL_TYPE_INT,
		GLSL_TYPE_IVEC2,
		GLSL_TYPE_IVEC3,
		GLSL_TYPE_IVEC4,

		GLSL_TYPE_UINT,
		GLSL_TYPE_UVEC2,
		GLSL_TYPE_UVEC3,
		GLSL_TYPE_UVEC4,

		GLSL_TYPE_FLOAT,
		GLSL_TYPE_VEC2,
		GLSL_TYPE_VEC3,
		GLSL_TYPE_VEC4,

		GLSL_TYPE_F16,
		GLSL_TYPE_F16VEC2,
		GLSL_TYPE_F16VEC3,
		GLSL_TYPE_F16VEC4,

		GLSL_TYPE_MAT2,
		GLSL_TYPE_MAT3,
		GLSL_TYPE_MAT4,

		GLSL_TYPE_DOUBLE,
		GLSL_TYPE_DVEC2,
		GLSL_TYPE_DVEC3,
		GLSL_TYPE_DVEC4,
		GLSL_TYPE_DMAT2,
		GLSL_TYPE_DMAT3,
		GLSL_TYPE_DMAT4,

		GLSL_TYPE_COUNT
	};

	enum GlslBasicType
	{
		GLSL_BASIC_TYPE_INT,
		GLSL_BASIC_TYPE_UINT,
		GLSL_BASIC_TYPE_FLOAT,
		GLSL_BASIC_TYPE_DOUBLE,
		GLSL_BASIC_TYPE_FLOAT16,
	};

	enum BindingMapping
	{
		BINDING_MAPPING_ONE_TO_ONE,		//!< Vertex input bindings will not contain data for more than one attribute.
		BINDING_MAPPING_ONE_TO_MANY		//!< Vertex input bindings can contain data for more than one attribute.
	};

	enum AttributeLayout
	{
		ATTRIBUTE_LAYOUT_INTERLEAVED,	//!< Attribute data is bundled together as if in a structure: [pos 0][color 0][pos 1][color 1]...
		ATTRIBUTE_LAYOUT_SEQUENTIAL		//!< Data for each attribute is laid out separately: [pos 0][pos 1]...[color 0][color 1]...
										//   Sequential only makes a difference if ONE_TO_MANY mapping is used (more than one attribute in a binding).
	};

	enum LayoutSkip
	{
		LAYOUT_SKIP_ENABLED,	//!< Skip one location slot after each attribute
		LAYOUT_SKIP_DISABLED	//!< Consume locations sequentially
	};

	enum LayoutOrder
	{
		LAYOUT_ORDER_IN_ORDER,		//!< Assign locations in order
		LAYOUT_ORDER_OUT_OF_ORDER	//!< Assign locations out of order
	};

	struct AttributeInfo
	{
		GlslType				glslType;
		VkFormat				vkType;
		VkVertexInputRate		inputRate;
	};

	struct GlslTypeDescription
	{
		const char*		name;
		int				vertexInputComponentCount;
		int				vertexInputCount;
		GlslBasicType	basicType;
	};

	static const GlslTypeDescription		s_glslTypeDescriptions[GLSL_TYPE_COUNT];

											VertexInputTest				(tcu::TestContext&					testContext,
																		 const std::string&					name,
																		 const std::string&					description,
																		 const PipelineConstructionType		pipelineConstructionType,
																		 const std::vector<AttributeInfo>&	attributeInfos,
																		 BindingMapping						bindingMapping,
																		 AttributeLayout					attributeLayout,
																		 LayoutSkip							layoutSkip = LAYOUT_SKIP_DISABLED,
																		 LayoutOrder						layoutOrder = LAYOUT_ORDER_IN_ORDER,
																		 const bool							testMissingComponents = false);

	virtual									~VertexInputTest			(void) {}
	virtual void							initPrograms				(SourceCollections& programCollection) const;
	virtual void							checkSupport				(Context& context) const;
	virtual TestInstance*					createInstance				(Context& context) const;
	static bool								isCompatibleType			(VkFormat format, GlslType glslType);

private:
	AttributeInfo							getAttributeInfo			(size_t attributeNdx) const;
	size_t									getNumAttributes			(void) const;
	std::string								getGlslExtensions			(void) const;
	std::string								getGlslInputDeclarations	(void) const;
	std::string								getGlslVertexCheck			(void) const;
	std::string								getGlslAttributeConditions	(const AttributeInfo& attributeInfo, const std::string attributeIndex) const;
	static tcu::Vec4						getFormatThreshold			(VkFormat format);

	const PipelineConstructionType			m_pipelineConstructionType;
	const std::vector<AttributeInfo>		m_attributeInfos;
	const BindingMapping					m_bindingMapping;
	const AttributeLayout					m_attributeLayout;
	const LayoutSkip						m_layoutSkip;
	mutable std::vector<deUint32>			m_locations;
	const bool								m_queryMaxAttributes;
	bool									m_usesDoubleType;
	bool									m_usesFloat16Type;
	mutable size_t							m_maxAttributes;
	const bool								m_testMissingComponents;
};

class VertexInputInstance : public vkt::TestInstance
{
public:
	struct VertexInputAttributeDescription
	{
		VertexInputTest::GlslType			glslType;
		int									vertexInputIndex;
		VkVertexInputAttributeDescription	vkDescription;
	};

	typedef	std::vector<VertexInputAttributeDescription>	AttributeDescriptionList;

											VertexInputInstance			(Context&												context,
																		 const PipelineConstructionType							pipelineConstructionType,
																		 const AttributeDescriptionList&						attributeDescriptions,
																		 const std::vector<VkVertexInputBindingDescription>&	bindingDescriptions,
																		 const std::vector<VkDeviceSize>&						bindingOffsets);

	virtual									~VertexInputInstance		(void);
	virtual tcu::TestStatus					iterate						(void);


	static void								writeVertexInputData		(deUint8* destPtr, const VkVertexInputBindingDescription& bindingDescription, const VkDeviceSize bindingOffset, const AttributeDescriptionList& attributes);
	static void								writeVertexInputValue		(deUint8* destPtr, const VertexInputAttributeDescription& attributes, int indexId);

private:
	tcu::TestStatus							verifyImage					(void);

private:
	std::vector<VkBuffer>					m_vertexBuffers;
	std::vector<Allocation*>				m_vertexBufferAllocs;

	const tcu::UVec2						m_renderSize;
	const VkFormat							m_colorFormat;

	Move<VkImage>							m_colorImage;
	de::MovePtr<Allocation>					m_colorImageAlloc;
	Move<VkImage>							m_depthImage;
	Move<VkImageView>						m_colorAttachmentView;
	RenderPassWrapper						m_renderPass;

	ShaderWrapper							m_vertexShaderModule;
	ShaderWrapper							m_fragmentShaderModule;

	PipelineLayoutWrapper					m_pipelineLayout;
	GraphicsPipelineWrapper					m_graphicsPipeline;

	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBuffer;
};

const VertexInputTest::GlslTypeDescription VertexInputTest::s_glslTypeDescriptions[GLSL_TYPE_COUNT] =
{
	{ "int",		1, 1, GLSL_BASIC_TYPE_INT },
	{ "ivec2",		2, 1, GLSL_BASIC_TYPE_INT },
	{ "ivec3",		3, 1, GLSL_BASIC_TYPE_INT },
	{ "ivec4",		4, 1, GLSL_BASIC_TYPE_INT },

	{ "uint",		1, 1, GLSL_BASIC_TYPE_UINT },
	{ "uvec2",		2, 1, GLSL_BASIC_TYPE_UINT },
	{ "uvec3",		3, 1, GLSL_BASIC_TYPE_UINT },
	{ "uvec4",		4, 1, GLSL_BASIC_TYPE_UINT },

	{ "float",		1, 1, GLSL_BASIC_TYPE_FLOAT },
	{ "vec2",		2, 1, GLSL_BASIC_TYPE_FLOAT },
	{ "vec3",		3, 1, GLSL_BASIC_TYPE_FLOAT },
	{ "vec4",		4, 1, GLSL_BASIC_TYPE_FLOAT },

	{ "float16_t",	1, 1, GLSL_BASIC_TYPE_FLOAT16 },
	{ "f16vec2",	2, 1, GLSL_BASIC_TYPE_FLOAT16 },
	{ "f16vec3",	3, 1, GLSL_BASIC_TYPE_FLOAT16 },
	{ "f16vec4",	4, 1, GLSL_BASIC_TYPE_FLOAT16 },

	{ "mat2",		2, 2, GLSL_BASIC_TYPE_FLOAT },
	{ "mat3",		3, 3, GLSL_BASIC_TYPE_FLOAT },
	{ "mat4",		4, 4, GLSL_BASIC_TYPE_FLOAT },

	{ "double",		1, 1, GLSL_BASIC_TYPE_DOUBLE },
	{ "dvec2",		2, 1, GLSL_BASIC_TYPE_DOUBLE },
	{ "dvec3",		3, 1, GLSL_BASIC_TYPE_DOUBLE },
	{ "dvec4",		4, 1, GLSL_BASIC_TYPE_DOUBLE },
	{ "dmat2",		2, 2, GLSL_BASIC_TYPE_DOUBLE },
	{ "dmat3",		3, 3, GLSL_BASIC_TYPE_DOUBLE },
	{ "dmat4",		4, 4, GLSL_BASIC_TYPE_DOUBLE }
};

const char* expandGlslNameToFullComponents (const char* name)
{
	static const std::map<std::string, std::string> nameMap
	{
		std::make_pair("int",			"ivec4"),
		std::make_pair("ivec2",			"ivec4"),
		std::make_pair("ivec3",			"ivec4"),
		std::make_pair("ivec4",			"ivec4"),
		std::make_pair("uint",			"uvec4"),
		std::make_pair("uvec2",			"uvec4"),
		std::make_pair("uvec3",			"uvec4"),
		std::make_pair("uvec4",			"uvec4"),
		std::make_pair("float",			"vec4"),
		std::make_pair("vec2",			"vec4"),
		std::make_pair("vec3",			"vec4"),
		std::make_pair("vec4",			"vec4"),
		std::make_pair("float16_t",		"f16vec4"),
		std::make_pair("f16vec2",		"f16vec4"),
		std::make_pair("f16vec3",		"f16vec4"),
		std::make_pair("f16vec4",		"f16vec4"),
		std::make_pair("mat2",			"mat2x4"),
		std::make_pair("mat3",			"mat3x4"),
		std::make_pair("mat4",			"mat4"),
#if 0
		// 64-bit types don't have default values, so they cannot be used in missing component tests.
		// In addition, they may be expanded from one location to using more than one, which creates vertex input mismatches.
		std::make_pair("double",		"dvec4"),
		std::make_pair("dvec2",			"dvec4"),
		std::make_pair("dvec3",			"dvec4"),
		std::make_pair("dvec4",			"dvec4"),
		std::make_pair("dmat2",			"dmat2x4"),
		std::make_pair("dmat3",			"dmat3x4"),
		std::make_pair("dmat4",			"dmat4"),
#endif
	};

	const auto pos = nameMap.find(name);
	if (pos == nameMap.end())
		return nullptr;
	return pos->second.c_str();
}

deUint32 getAttributeBinding (const VertexInputTest::BindingMapping bindingMapping, const VkVertexInputRate firstInputRate, const VkVertexInputRate inputRate, const deUint32 attributeNdx)
{
	if (bindingMapping == VertexInputTest::BINDING_MAPPING_ONE_TO_ONE)
	{
		// Each attribute uses a unique binding
		return attributeNdx;
	}
	else // bindingMapping == BINDING_MAPPING_ONE_TO_MANY
	{
		// Alternate between two bindings
		return deUint32(firstInputRate + inputRate) % 2u;
	}
}

//! Number of locations used up by an attribute.
deUint32 getConsumedLocations (const VertexInputTest::AttributeInfo& attributeInfo)
{
	// double formats with more than 2 components will take 2 locations
	const VertexInputTest::GlslType type = attributeInfo.glslType;
	if ((type == VertexInputTest::GLSL_TYPE_DMAT2 || type == VertexInputTest::GLSL_TYPE_DMAT3 || type == VertexInputTest::GLSL_TYPE_DMAT4) &&
		(attributeInfo.vkType == VK_FORMAT_R64G64B64_SFLOAT || attributeInfo.vkType == VK_FORMAT_R64G64B64A64_SFLOAT))
	{
		return 2u;
	}
	else
		return 1u;
}

VertexInputTest::VertexInputTest (tcu::TestContext&						testContext,
								  const std::string&					name,
								  const std::string&					description,
								  const PipelineConstructionType		pipelineConstructionType,
								  const std::vector<AttributeInfo>&		attributeInfos,
								  BindingMapping						bindingMapping,
								  AttributeLayout						attributeLayout,
								  LayoutSkip							layoutSkip,
								  LayoutOrder							layoutOrder,
								  const bool							testMissingComponents)
	: vkt::TestCase					(testContext, name, description)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_attributeInfos				(attributeInfos)
	, m_bindingMapping				(bindingMapping)
	, m_attributeLayout				(attributeLayout)
	, m_layoutSkip					(layoutSkip)
	, m_queryMaxAttributes			(attributeInfos.size() == 0)
	, m_usesDoubleType				(false)
	, m_usesFloat16Type				(false)
	, m_maxAttributes				(16)
	, m_testMissingComponents		(testMissingComponents)
{
	DE_ASSERT(m_attributeLayout == ATTRIBUTE_LAYOUT_INTERLEAVED || m_bindingMapping == BINDING_MAPPING_ONE_TO_MANY);

	for (size_t attributeNdx = 0; attributeNdx < m_attributeInfos.size(); attributeNdx++)
	{
		const auto& basicType = s_glslTypeDescriptions[m_attributeInfos[attributeNdx].glslType].basicType;

		if (basicType == GLSL_BASIC_TYPE_DOUBLE)
			m_usesDoubleType = true;
		else if (basicType == GLSL_BASIC_TYPE_FLOAT16)
			m_usesFloat16Type = true;
	}

	// Determine number of location slots required for each attribute
	deUint32				attributeLocation		= 0;
	std::vector<deUint32>	locationSlotsNeeded;
	const size_t			numAttributes			= getNumAttributes();

	for (size_t attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
	{
		const AttributeInfo&		attributeInfo			= getAttributeInfo(attributeNdx);
		const GlslTypeDescription&	glslTypeDescription		= s_glslTypeDescriptions[attributeInfo.glslType];
		const deUint32				prevAttributeLocation	= attributeLocation;

		attributeLocation += glslTypeDescription.vertexInputCount * getConsumedLocations(attributeInfo);

		if (m_layoutSkip == LAYOUT_SKIP_ENABLED)
			attributeLocation++;

		locationSlotsNeeded.push_back(attributeLocation - prevAttributeLocation);
	}

	if (layoutOrder == LAYOUT_ORDER_IN_ORDER)
	{
		deUint32 loc = 0;

		// Assign locations in order
		for (size_t attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
		{
			m_locations.push_back(loc);
			loc += locationSlotsNeeded[attributeNdx];
		}
	}
	else
	{
		// Assign locations out of order
		std::vector<deUint32>	indices;
		std::vector<deUint32>	slots;
		deUint32				slot	= 0;

		// Mix the location slots: first all even and then all odd attributes.
		for (deUint32 attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
			if (attributeNdx % 2 == 0)
				indices.push_back(attributeNdx);
		for (deUint32 attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
			if (attributeNdx % 2 != 0)
				indices.push_back(attributeNdx);

		for (size_t i = 0; i < indices.size(); i++)
		{
			slots.push_back(slot);
			slot += locationSlotsNeeded[indices[i]];
		}

		for (size_t attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
		{
			deUint32 slotIdx = 0;
			for (deUint32 i = 0; i < (deUint32)indices.size(); i++)
				if (attributeNdx == indices[i])
					slotIdx = i;
			m_locations.push_back(slots[slotIdx]);
		}
	}
}

VertexInputTest::AttributeInfo VertexInputTest::getAttributeInfo (size_t attributeNdx) const
{
	if (m_queryMaxAttributes)
	{
		AttributeInfo attributeInfo =
		{
			GLSL_TYPE_VEC4,
			VK_FORMAT_R8G8B8A8_SNORM,
			(attributeNdx % 2 == 0) ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE
		};

		return attributeInfo;
	}
	else
	{
		return m_attributeInfos.at(attributeNdx);
	}
}

size_t VertexInputTest::getNumAttributes (void) const
{
	if (m_queryMaxAttributes)
		return m_maxAttributes;
	else
		return m_attributeInfos.size();
}

void VertexInputTest::checkSupport (Context& context) const
{
	const deUint32 maxAttributes = context.getDeviceProperties().limits.maxVertexInputAttributes;

	if (m_attributeInfos.size() > maxAttributes)
		TCU_THROW(NotSupportedError, "Unsupported number of vertex input attributes, maxVertexInputAttributes: " + de::toString(maxAttributes));

	if (m_usesFloat16Type)
	{
		const auto& sf16i8Features = context.getShaderFloat16Int8Features();
		if (!sf16i8Features.shaderFloat16)
			TCU_THROW(NotSupportedError, "shaderFloat16 not supported");

		const auto& storage16Features = context.get16BitStorageFeatures();
		if (!storage16Features.storageInputOutput16)
			TCU_THROW(NotSupportedError, "storageInputOutput16 not supported");
	}

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

TestInstance* VertexInputTest::createInstance (Context& context) const
{
	typedef VertexInputInstance::VertexInputAttributeDescription VertexInputAttributeDescription;

	// Check upfront for maximum number of vertex input attributes
	{
		const InstanceInterface&		vki				= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice		= context.getPhysicalDevice();
		const VkPhysicalDeviceLimits	limits			= getPhysicalDeviceProperties(vki, physDevice).limits;

		const deUint32					maxAttributes	= limits.maxVertexInputAttributes;

		// Use VkPhysicalDeviceLimits::maxVertexInputAttributes
		if (m_queryMaxAttributes)
		{
			m_maxAttributes = maxAttributes;
			m_locations.clear();
			for (deUint32 i = 0; i < maxAttributes; i++)
				m_locations.push_back(i);
		}
	}

	// Create enough binding descriptions with random offsets
	std::vector<VkVertexInputBindingDescription>	bindingDescriptions;
	std::vector<VkDeviceSize>						bindingOffsets;
	const size_t									numAttributes		= getNumAttributes();
	const size_t									numBindings			= (m_bindingMapping == BINDING_MAPPING_ONE_TO_ONE) ? numAttributes : ((numAttributes > 1) ? 2 : 1);
	const VkVertexInputRate							firstInputrate		= getAttributeInfo(0).inputRate;

	for (size_t bindingNdx = 0; bindingNdx < numBindings; ++bindingNdx)
	{
		// Bindings alternate between STEP_RATE_VERTEX and STEP_RATE_INSTANCE
		const VkVertexInputRate						inputRate			= ((firstInputrate + bindingNdx) % 2 == 0) ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;

		// Stride will be updated when creating the attribute descriptions
		const VkVertexInputBindingDescription		bindingDescription	=
		{
			static_cast<deUint32>(bindingNdx),		// deUint32				binding;
			0u,										// deUint32				stride;
			inputRate								// VkVertexInputRate	inputRate;
		};

		bindingDescriptions.push_back(bindingDescription);
		bindingOffsets.push_back(4 * bindingNdx);
	}

	std::vector<VertexInputAttributeDescription>	attributeDescriptions;
	std::vector<deUint32>							attributeOffsets		(bindingDescriptions.size(), 0);
	std::vector<deUint32>							attributeMaxSizes		(bindingDescriptions.size(), 0);	// max component or vector size, depending on which layout we are using
	std::vector<uint32_t>							attributeMaxCompSizes	(bindingDescriptions.size(), 0u);	// max component size for each binding.
	std::vector<uint32_t>							bindingSeqStrides		(bindingDescriptions.size(), 0u);	// strides for bindings in sequential layout mode

	// To place the attributes sequentially we need to know the largest attribute and use its size in stride and offset calculations.
	if (m_attributeLayout == ATTRIBUTE_LAYOUT_SEQUENTIAL)
	{
		for (size_t attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
		{
			const AttributeInfo&	attributeInfo			= getAttributeInfo(attributeNdx);
			const deUint32			attributeBinding		= getAttributeBinding(m_bindingMapping, firstInputrate, attributeInfo.inputRate, static_cast<deUint32>(attributeNdx));
			const deUint32			inputSize				= getVertexFormatSize(attributeInfo.vkType);
			const auto				componentSize			= getVertexFormatComponentSize(attributeInfo.vkType);
			const auto				maxSize					= de::max(attributeMaxSizes[attributeBinding], inputSize);
			const auto				maxComponentSize		= de::max(attributeMaxCompSizes[attributeBinding], componentSize);

			attributeMaxSizes[attributeBinding]				= maxSize;
			attributeMaxCompSizes[attributeBinding]			= maxComponentSize;
		}

		// Round up the maximum size so the components are always aligned.
		for (size_t bindingIdx = 0u; bindingIdx < bindingSeqStrides.size(); ++bindingIdx)
			bindingSeqStrides[bindingIdx] = de::roundUp(attributeMaxSizes[bindingIdx], attributeMaxCompSizes[bindingIdx]);
	}

	// Create attribute descriptions, assign them to bindings and update stride.
	for (size_t attributeNdx = 0; attributeNdx < numAttributes; ++attributeNdx)
	{
		const AttributeInfo&		attributeInfo			= getAttributeInfo(attributeNdx);
		const GlslTypeDescription&	glslTypeDescription		= s_glslTypeDescriptions[attributeInfo.glslType];
		const deUint32				inputSize				= getVertexFormatSize(attributeInfo.vkType);
		const deUint32				attributeBinding		= getAttributeBinding(m_bindingMapping, firstInputrate, attributeInfo.inputRate, static_cast<deUint32>(attributeNdx));
		const deUint32				vertexCount				= (attributeInfo.inputRate == VK_VERTEX_INPUT_RATE_VERTEX) ? (4 * 2) : 2;

		VertexInputAttributeDescription attributeDescription =
		{
			attributeInfo.glslType,							// GlslType		glslType;
			0,												// int			vertexInputIndex;
			{
				0u,											// uint32_t    location;
				attributeBinding,							// uint32_t    binding;
				attributeInfo.vkType,						// VkFormat    format;
				0u,											// uint32_t    offset;
			},
		};

		// Matrix types add each column as a separate attribute.
		for (int descNdx = 0; descNdx < glslTypeDescription.vertexInputCount; ++descNdx)
		{
			attributeDescription.vertexInputIndex		= descNdx;
			attributeDescription.vkDescription.location	= m_locations[attributeNdx] + getConsumedLocations(attributeInfo) * descNdx;

			if (m_attributeLayout == ATTRIBUTE_LAYOUT_INTERLEAVED)
			{
				const deUint32	offsetToComponentAlignment		 = getNextMultipleOffset(inputSize,
																						 (deUint32)bindingOffsets[attributeBinding] + attributeOffsets[attributeBinding]);

				attributeOffsets[attributeBinding]				+= offsetToComponentAlignment;

				attributeDescription.vkDescription.offset		 = attributeOffsets[attributeBinding];
				attributeDescriptions.push_back(attributeDescription);

				bindingDescriptions[attributeBinding].stride	+= offsetToComponentAlignment + inputSize;
				attributeOffsets[attributeBinding]				+= inputSize;
				attributeMaxSizes[attributeBinding]				 = de::max(attributeMaxSizes[attributeBinding], inputSize);
			}
			else // m_attributeLayout == ATTRIBUTE_LAYOUT_SEQUENTIAL
			{
				attributeDescription.vkDescription.offset		 = attributeOffsets[attributeBinding];
				attributeDescriptions.push_back(attributeDescription);

				attributeOffsets[attributeBinding]				+= vertexCount * bindingSeqStrides[attributeBinding];
			}
		}

		if (m_attributeLayout == ATTRIBUTE_LAYOUT_SEQUENTIAL)
			bindingDescriptions[attributeBinding].stride = bindingSeqStrides[attributeBinding];
	}

	if (m_attributeLayout == ATTRIBUTE_LAYOUT_INTERLEAVED)
	{
		// Make sure the stride results in aligned access
		for (size_t bindingNdx = 0; bindingNdx < bindingDescriptions.size(); ++bindingNdx)
		{
			auto& stride = bindingDescriptions[bindingNdx].stride; // note: by reference to modify it below.

			if (attributeMaxSizes[bindingNdx] > 0)
				stride += getNextMultipleOffset(attributeMaxSizes[bindingNdx], stride);
		}
	}

	// Check upfront for maximum number of vertex input bindings
	{
		const InstanceInterface&		vki				= context.getInstanceInterface();
		const VkPhysicalDevice			physDevice		= context.getPhysicalDevice();
		const VkPhysicalDeviceLimits	limits			= getPhysicalDeviceProperties(vki, physDevice).limits;

		const deUint32					maxBindings		= limits.maxVertexInputBindings;

		if (bindingDescriptions.size() > maxBindings)
		{
			const std::string notSupportedStr = "Unsupported number of vertex input bindings, maxVertexInputBindings: " + de::toString(maxBindings);
			TCU_THROW(NotSupportedError, notSupportedStr.c_str());
		}
	}

	// Portability requires stride to be multiply of minVertexInputBindingStrideAlignment
#ifndef CTS_USES_VULKANSC
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
	{
		deUint32 minStrideAlignment = context.getPortabilitySubsetProperties().minVertexInputBindingStrideAlignment;
		for (size_t bindingNdx = 0; bindingNdx < bindingDescriptions.size(); ++bindingNdx)
		{
			if ((bindingDescriptions[bindingNdx].stride % minStrideAlignment) != 0)
				TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: stride is not multiply of minVertexInputBindingStrideAlignment");
		}
	}
#endif // CTS_USES_VULKANSC

	return new VertexInputInstance(context, m_pipelineConstructionType, attributeDescriptions, bindingDescriptions, bindingOffsets);
}

void VertexInputTest::initPrograms (SourceCollections& programCollection) const
{
	std::ostringstream vertexSrc;

	vertexSrc << "#version 460\n"
			  << getGlslExtensions()
			  << "layout(constant_id = 0) const int numAttributes = " << m_maxAttributes << ";\n"
			  << getGlslInputDeclarations()
			  << "layout(location = 0) out highp vec4 vtxColor;\n"
			  << "out gl_PerVertex {\n"
			  << "  vec4 gl_Position;\n"
			  << "};\n";

	vertexSrc << "void main (void)\n"
			  << "{\n"
			  << getGlslVertexCheck()
			  << "}\n";

	programCollection.glslSources.add("attribute_test_vert") << glu::VertexSource(vertexSrc.str());

	programCollection.glslSources.add("attribute_test_frag") << glu::FragmentSource(
		"#version 460\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n");
}

std::string VertexInputTest::getGlslExtensions (void) const
{
	std::string	extensions;

	if (m_usesFloat16Type)
		extensions += "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n";

	return extensions;
}

std::string VertexInputTest::getGlslInputDeclarations (void) const
{
	std::ostringstream	glslInputs;

	if (m_queryMaxAttributes)
	{
		// Don't use the first input binding to leave room for VertexIndex and InstanceIndex, which count towards the
		// total number of inputs attributes. Leave the first binding so that the largest location number are still used.
		const GlslTypeDescription& glslTypeDesc = s_glslTypeDescriptions[GLSL_TYPE_VEC4];
		glslInputs << "layout(location = 1) in " << glslTypeDesc.name << " attr[numAttributes-1];\n";
	}
	else
	{
		for (size_t attributeNdx = 0; attributeNdx < m_attributeInfos.size(); attributeNdx++)
		{
			const char* declType = nullptr;
			if (m_testMissingComponents)
			{
				const auto& glslType = m_attributeInfos[attributeNdx].glslType;
				const auto& typeInfo = s_glslTypeDescriptions[glslType];

				DE_ASSERT(typeInfo.vertexInputComponentCount < kMaxComponents);
				DE_ASSERT(typeInfo.basicType != GLSL_BASIC_TYPE_DOUBLE);

				// Find the equivalent type with 4 components.
				declType = expandGlslNameToFullComponents(typeInfo.name);
			}
			else
				declType = s_glslTypeDescriptions[m_attributeInfos[attributeNdx].glslType].name;

			glslInputs << "layout(location = " << m_locations[attributeNdx] << ") in " << declType << " attr" << attributeNdx << ";\n";
		}
	}

	return glslInputs.str();
}

std::string VertexInputTest::getGlslVertexCheck (void) const
{
	std::ostringstream	glslCode;
	std::string			inputCountStr;

	glslCode << "	int okCount = 0;\n";

	if (m_queryMaxAttributes)
	{
		DE_ASSERT(!m_testMissingComponents);

		// numAttributes will be replaced later by a specialisation constant, so this loop and
		// the multiplication by numAttributes, below, must happen in the shader itself.
		const AttributeInfo attributeInfo = getAttributeInfo(0);

		glslCode << "	for (int checkNdx = 1; checkNdx < numAttributes; checkNdx++)\n"
				 <<	"	{\n"
				 << "		uint index = (checkNdx % 2 == 0) ? gl_VertexIndex : gl_InstanceIndex;\n";

		// Because our location is offset by 1 relative to the API definitions, checkNdx-1 here.
		glslCode << getGlslAttributeConditions(attributeInfo, "checkNdx-1")
				 << "	}\n";

		const int vertexInputCount		= VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].vertexInputCount;
		int totalInputComponentCount	= vertexInputCount * VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].vertexInputComponentCount;

		// Don't count components from location 0 which was skipped.
		inputCountStr = std::to_string(totalInputComponentCount) + " * (numAttributes-1)";
	}
	else
	{
		// Generate 1 check per attribute and work out the number of components at compile time.
		int totalInputComponentCount = 0;
		for (size_t attributeNdx = 0; attributeNdx < m_attributeInfos.size(); attributeNdx++)
		{
			glslCode << getGlslAttributeConditions(m_attributeInfos[attributeNdx], de::toString(attributeNdx));

			const int vertexInputCount	= VertexInputTest::s_glslTypeDescriptions[m_attributeInfos[attributeNdx].glslType].vertexInputCount;
			const int vertexCompCount	= VertexInputTest::s_glslTypeDescriptions[m_attributeInfos[attributeNdx].glslType].vertexInputComponentCount;
			totalInputComponentCount	+= vertexInputCount * ((!m_testMissingComponents) ? vertexCompCount : kMaxComponents - vertexCompCount);
		}

		inputCountStr = std::to_string(totalInputComponentCount);
	}

	glslCode <<
		"	if (okCount == " << inputCountStr << ")\n"
		"	{\n"
		"		if (gl_InstanceIndex == 0)\n"
		"			vtxColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
		"		else\n"
		"			vtxColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"	}\n"
		"	else\n"
		"	{\n"
		"		vtxColor = vec4(okCount / float(" << inputCountStr << "), 0.0f, 0.0f, 1.0);\n" <<
		"	}\n\n"
		"	if (gl_InstanceIndex == 0)\n"
		"	{\n"
		"		if (gl_VertexIndex == 0) gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 1) gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 2) gl_Position = vec4(-1.0, 1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 3) gl_Position = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"		else gl_Position = vec4(0.0);\n"
		"	}\n"
		"	else\n"
		"	{\n"
		"		if (gl_VertexIndex == 0) gl_Position = vec4(0.0, -1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 1) gl_Position = vec4(1.0, -1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 2) gl_Position = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"		else if (gl_VertexIndex == 3) gl_Position = vec4(1.0, 1.0, 0.0, 1.0);\n"
		"		else gl_Position = vec4(0.0);\n"
		"	}\n";

	return glslCode.str();
}

std::string VertexInputTest::getGlslAttributeConditions (const AttributeInfo& attributeInfo, const std::string attributeIndex) const
{
	std::ostringstream	glslCode;
	std::ostringstream	attributeVar;
	const int			componentCount		= VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].vertexInputComponentCount;
	const int			vertexInputCount	= VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].vertexInputCount;
	const deUint32		totalComponentCount	= componentCount * vertexInputCount;
	const tcu::Vec4		threshold			= getFormatThreshold(attributeInfo.vkType);
	const std::string	indexStr			= m_queryMaxAttributes ? "[" + attributeIndex + "]" : attributeIndex;
	const std::string	indentStr			= m_queryMaxAttributes ? "\t\t" : "\t";
	deUint32			componentIndex		= 0;
	deUint32			orderNdx;
	std::string			indexId;

	const deUint32		BGROrder[]			= { 2, 1, 0, 3 };
	const deUint32		ABGROrder[]			= { 3, 2, 1, 0 };
	const deUint32		ARGBOrder[]			= { 1, 2, 3, 0 };

	if (m_queryMaxAttributes)
		indexId	= "index";
	else
		indexId	= (attributeInfo.inputRate == VK_VERTEX_INPUT_RATE_VERTEX) ? "gl_VertexIndex" : "gl_InstanceIndex";

	attributeVar << "attr" << indexStr;

	glslCode << std::fixed;

	for (int columnNdx = 0; columnNdx< vertexInputCount; columnNdx++)
	{
		for (int rowNdx = 0; rowNdx < kMaxComponents; rowNdx++)
		{
			if (isVertexFormatComponentOrderABGR(attributeInfo.vkType))
				orderNdx = ABGROrder[rowNdx];
			else if (isVertexFormatComponentOrderARGB(attributeInfo.vkType))
				orderNdx = ARGBOrder[rowNdx];
			else
				orderNdx = BGROrder[rowNdx];

			std::string accessStr;
			{
				// Build string representing the access to the attribute component
				std::ostringstream accessStream;
				accessStream << attributeVar.str();

				if (vertexInputCount == 1)
				{
					if (componentCount > 1 || m_testMissingComponents)
						accessStream << "[" << rowNdx << "]";
				}
				else
				{
					accessStream << "[" << columnNdx << "][" << rowNdx << "]";
				}

				accessStr = accessStream.str();
			}

			if (rowNdx < componentCount && !m_testMissingComponents)
			{
				if (isVertexFormatSint(attributeInfo.vkType))
				{
					if (isVertexFormatPacked(attributeInfo.vkType))
					{
						const deInt32 maxIntValue = (1 << (getPackedVertexFormatComponentWidth(attributeInfo.vkType, orderNdx) - 1)) - 1;
						const deInt32 minIntValue = -maxIntValue;

						glslCode << indentStr << "if (" << accessStr << " == clamp(-(" << totalComponentCount << " * " << indexId << " + " << componentIndex << "), " << minIntValue << ", " << maxIntValue << "))\n";
					}
					else
						glslCode << indentStr << "if (" << accessStr << " == -(" << totalComponentCount << " * " << indexId << " + " << componentIndex << "))\n";
				}
				else if (isVertexFormatUint(attributeInfo.vkType))
				{
					if (isVertexFormatPacked(attributeInfo.vkType))
					{
						const deUint32 maxUintValue = (1 << getPackedVertexFormatComponentWidth(attributeInfo.vkType, orderNdx)) - 1;

						glslCode << indentStr << "if (" << accessStr << " == clamp(uint(" << totalComponentCount << " * " << indexId << " + " << componentIndex << "), 0, " << maxUintValue << "))\n";
					}
					else
						glslCode << indentStr << "if (" << accessStr << " == uint(" << totalComponentCount << " * " << indexId << " + " << componentIndex << "))\n";
				}
				else if (isVertexFormatSfloat(attributeInfo.vkType))
				{
					const auto& basicType = VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].basicType;

					if (basicType == VertexInputTest::GLSL_BASIC_TYPE_DOUBLE)
					{
						glslCode << indentStr << "if (abs(" << accessStr << " + double(0.01 * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < double(" << threshold[rowNdx] << "))\n";
					}
					else if (basicType == VertexInputTest::GLSL_BASIC_TYPE_FLOAT16)
					{
						glslCode << indentStr << "if (abs(" << accessStr << " + float16_t(0.01HF * (" << totalComponentCount << ".0HF * float16_t(" << indexId << ") + " << componentIndex << ".0HF))) < float16_t(" << threshold[rowNdx] << "HF))\n";
					}
					else
					{
						glslCode << indentStr << "if (abs(" << accessStr << " + (0.01 * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < " << threshold[rowNdx] << ")\n";
					}
				}
				else if (isVertexFormatSscaled(attributeInfo.vkType))
				{
					if (isVertexFormatPacked(attributeInfo.vkType))
					{
						const float maxScaledValue = float((1 << (getPackedVertexFormatComponentWidth(attributeInfo.vkType, orderNdx) - 1)) - 1);
						const float minScaledValue = -maxScaledValue - 1.0f;

						glslCode << indentStr << "if (abs(" << accessStr << " + clamp(" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0, " << minScaledValue << ", " << maxScaledValue << ")) < " << threshold[orderNdx] << ")\n";
					}
					else
						glslCode << indentStr << "if (abs(" << accessStr << " + (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0)) < " << threshold[rowNdx] << ")\n";
				}
				else if (isVertexFormatUscaled(attributeInfo.vkType))
				{
					if (isVertexFormatPacked(attributeInfo.vkType))
					{
						const float maxScaledValue = float((1 << getPackedVertexFormatComponentWidth(attributeInfo.vkType, orderNdx)) - 1);

						glslCode << indentStr << "if (abs(" << accessStr << " - clamp(" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0, 0, " << maxScaledValue << ")) < " << threshold[orderNdx] << ")\n";
					}
					else
						glslCode << indentStr << "if (abs(" << accessStr << " - (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0)) < " << threshold[rowNdx] << ")\n";
				}
				else if (isVertexFormatSnorm(attributeInfo.vkType))
				{
					const float representableDiff = isVertexFormatPacked(attributeInfo.vkType) ? getRepresentableDifferenceSnormPacked(attributeInfo.vkType, orderNdx) : getRepresentableDifferenceSnorm(attributeInfo.vkType);

					if(isVertexFormatPacked(attributeInfo.vkType))
						glslCode << indentStr << "if (abs(" << accessStr << " - clamp((-1.0 + " << representableDiff << " * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0)), -1.0, 1.0)) < " << threshold[orderNdx] << ")\n";
					else
						glslCode << indentStr << "if (abs(" << accessStr << " - (-1.0 + " << representableDiff << " * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < " << threshold[rowNdx] << ")\n";
				}
				else if (isVertexFormatUnorm(attributeInfo.vkType) || isVertexFormatSRGB(attributeInfo.vkType))
				{
					const float representableDiff = isVertexFormatPacked(attributeInfo.vkType) ? getRepresentableDifferenceUnormPacked(attributeInfo.vkType, orderNdx) : getRepresentableDifferenceUnorm(attributeInfo.vkType);

					if (isVertexFormatPacked(attributeInfo.vkType))
						glslCode << indentStr << "if (abs(" << accessStr << " - " << "clamp((" << representableDiff << " * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0)), 0.0, 1.0)) < " << threshold[orderNdx] << ")\n";
					else
						glslCode << indentStr << "if (abs(" << accessStr << " - " << "(" << representableDiff << " * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < " << threshold[rowNdx] << ")\n";
				}
				else if (isVertexFormatUfloat(attributeInfo.vkType))
				{
					const auto& basicType = VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].basicType;

					if (basicType == VertexInputTest::GLSL_BASIC_TYPE_DOUBLE)
					{
						glslCode << indentStr << "if (abs(" << accessStr << " - double(0.01 * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < double(" << threshold[rowNdx] << "))\n";
					}
					else if (basicType == VertexInputTest::GLSL_BASIC_TYPE_FLOAT16)
					{
						glslCode << indentStr << "if (abs(" << accessStr << " - float16_t(0.01HF * (" << totalComponentCount << ".0HF * float16_t(" << indexId << ") + " << componentIndex << ".0HF))) < float16_t(" << threshold[rowNdx] << "HF))\n";
					}
					else
					{
						glslCode << indentStr << "if (abs(" << accessStr << " - (0.01 * (" << totalComponentCount << ".0 * float(" << indexId << ") + " << componentIndex << ".0))) < (" << threshold[rowNdx] << "))\n";
					}
				}
				else
				{
					DE_ASSERT(false);
				}

				glslCode << indentStr << "\tokCount++;\n\n";

				componentIndex++;
			}
			else if (rowNdx >= componentCount && m_testMissingComponents)
			{
				const auto	expectedValue	= ((rowNdx == (kMaxComponents - 1)) ? 1u : 0u); // Color components are expanded with zeros and alpha with one.
				const auto&	basicType		= VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].basicType;
				std::string	glslType;

				switch (basicType)
				{
				case GLSL_BASIC_TYPE_INT:		glslType = "int";		break;
				case GLSL_BASIC_TYPE_UINT:		glslType = "uint";		break;
				case GLSL_BASIC_TYPE_FLOAT:		glslType = "float";		break;
				case GLSL_BASIC_TYPE_DOUBLE:	glslType = "double";	break;
				case GLSL_BASIC_TYPE_FLOAT16:	glslType = "float16_t";	break;
				default:
					DE_ASSERT(false); break;
				}

				glslCode << indentStr << "if (" << accessStr << " == " << glslType << "(" << expectedValue << "))\n";
				glslCode << indentStr << "\tokCount++;\n\n";
			}
		}

	}
	return glslCode.str();
}

tcu::Vec4 VertexInputTest::getFormatThreshold (VkFormat format)
{
	using tcu::Vec4;

	switch (format)
	{
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return Vec4(0.00001f);

		default:
			break;
	}

	if (isVertexFormatSnorm(format))
	{
		return (isVertexFormatPacked(format) ? Vec4(1.5f * getRepresentableDifferenceSnormPacked(format, 0),
													1.5f * getRepresentableDifferenceSnormPacked(format, 1),
													1.5f * getRepresentableDifferenceSnormPacked(format, 2),
													1.5f * getRepresentableDifferenceSnormPacked(format, 3))
													: Vec4(1.5f * getRepresentableDifferenceSnorm(format)));
	}
	else if (isVertexFormatUnorm(format))
	{
		return (isVertexFormatPacked(format) ? Vec4(1.5f * getRepresentableDifferenceUnormPacked(format, 0),
													1.5f * getRepresentableDifferenceUnormPacked(format, 1),
													1.5f * getRepresentableDifferenceUnormPacked(format, 2),
													1.5f * getRepresentableDifferenceUnormPacked(format, 3))
													: Vec4(1.5f * getRepresentableDifferenceUnorm(format)));
	} else if (isVertexFormatUfloat(format))
	{
		return Vec4(0.008f);
	}

	return Vec4(0.001f);
}

VertexInputInstance::VertexInputInstance (Context&												context,
										  const PipelineConstructionType						pipelineConstructionType,
										  const AttributeDescriptionList&						attributeDescriptions,
										  const std::vector<VkVertexInputBindingDescription>&	bindingDescriptions,
										  const std::vector<VkDeviceSize>&						bindingOffsets)
	: vkt::TestInstance			(context)
	, m_renderSize				(16, 16)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_graphicsPipeline		(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), pipelineConstructionType)
{
	DE_ASSERT(bindingDescriptions.size() == bindingOffsets.size());

	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Check upfront for unsupported features
	for (size_t attributeNdx = 0; attributeNdx < attributeDescriptions.size(); attributeNdx++)
	{
		const VkVertexInputAttributeDescription& attributeDescription = attributeDescriptions[attributeNdx].vkDescription;
		if (!isSupportedVertexFormat(context, attributeDescription.format))
		{
			throw tcu::NotSupportedError(std::string("Unsupported format for vertex input: ") + getFormatName(attributeDescription.format));
		}
	}

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },  // VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = RenderPassWrapper(pipelineConstructionType, vk, vkDevice, m_colorFormat);

	// Create framebuffer
	{
		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			1u,													// deUint32					attachmentCount;
			&m_colorAttachmentView.get(),						// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32					width;
			(deUint32)m_renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *m_colorImage);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("attribute_test_vert"), 0);
	m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("attribute_test_frag"), 0);

	// Create specialization constant
	deUint32 specializationData = static_cast<deUint32>(attributeDescriptions.size());

	const VkSpecializationMapEntry specializationMapEntry =
	{
		0,														// uint32_t							constantID
		0,														// uint32_t							offset
		sizeof(specializationData),								// uint32_t							size
	};
	const VkSpecializationInfo specializationInfo =
	{
		1,														// uint32_t							mapEntryCount
		&specializationMapEntry,								// const void*						pMapEntries
		sizeof(specializationData),								// size_t							dataSize
		&specializationData										// const void*						pData
	};

	// Create pipeline
	{
		// Create vertex attribute array and check if their VK formats are supported
		std::vector<VkVertexInputAttributeDescription> vkAttributeDescriptions;
		for (size_t attributeNdx = 0; attributeNdx < attributeDescriptions.size(); attributeNdx++)
		{
			const VkVertexInputAttributeDescription& attributeDescription = attributeDescriptions[attributeNdx].vkDescription;
			vkAttributeDescriptions.push_back(attributeDescription);
		}

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			(deUint32)bindingDescriptions.size(),							// deUint32									vertexBindingDescriptionCount;
			bindingDescriptions.data(),										// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			(deUint32)vkAttributeDescriptions.size(),						// deUint32									vertexAttributeDescriptionCount;
			vkAttributeDescriptions.data()									// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>	viewport	{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>		scissor		{ makeRect2D(m_renderSize) };

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,																		// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ONE,														// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_ONE,														// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp				alphaBlendOp;
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |						// VkColorComponentFlags	colorWriteMask;
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
		};

		m_graphicsPipeline.setDefaultRasterizationState()
						  .setDefaultDepthStencilState()
						  .setDefaultMultisampleState()
						  .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewport,
										scissor,
										m_pipelineLayout,
										*m_renderPass,
										0u,
										m_vertexShaderModule,
										DE_NULL,
										ShaderWrapper(),
										ShaderWrapper(),
										ShaderWrapper(),
										&specializationInfo)
						  .setupFragmentShaderState(m_pipelineLayout,
										*m_renderPass,
										0u,
										m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
						  .setMonolithicPipelineLayout(m_pipelineLayout)
						  .buildPipeline();
	}

	// Create vertex buffer
	{
		// calculate buffer size
		// 32 is maximal attribute size (4*sizeof(double)),
		// 8 maximal vertex count used in writeVertexInputData
		VkDeviceSize bufferSize = 32 * 8 * attributeDescriptions.size();

		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		// Upload data for each vertex input binding
		for (deUint32 bindingNdx = 0; bindingNdx < bindingDescriptions.size(); bindingNdx++)
		{
			Move<VkBuffer>			vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
			de::MovePtr<Allocation>	vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);

			VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset()));

			writeVertexInputData((deUint8*)vertexBufferAlloc->getHostPtr(), bindingDescriptions[bindingNdx], bindingOffsets[bindingNdx], attributeDescriptions);
			flushAlloc(vk, vkDevice, *vertexBufferAlloc);

			m_vertexBuffers.push_back(vertexBuffer.disown());
			m_vertexBufferAllocs.push_back(vertexBufferAlloc.release());
		}
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		const VkImageMemoryBarrier attachmentLayoutBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			*m_colorImage,									// VkImage					image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, 1u, &attachmentLayoutBarrier);

		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);

		m_graphicsPipeline.bind(*m_cmdBuffer);

		std::vector<VkBuffer> vertexBuffers;
		for (size_t bufferNdx = 0; bufferNdx < m_vertexBuffers.size(); bufferNdx++)
			vertexBuffers.push_back(m_vertexBuffers[bufferNdx]);

		if (vertexBuffers.size() <= 1)
		{
			// One vertex buffer
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, (deUint32)vertexBuffers.size(), vertexBuffers.data(), bindingOffsets.data());
		}
		else
		{
			// Smoke-test vkCmdBindVertexBuffers(..., startBinding, ... )

			const deUint32 firstHalfLength = (deUint32)vertexBuffers.size() / 2;
			const deUint32 secondHalfLength = firstHalfLength + (deUint32)(vertexBuffers.size() % 2);

			// Bind first half of vertex buffers
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, firstHalfLength, vertexBuffers.data(), bindingOffsets.data());

			// Bind second half of vertex buffers
			vk.cmdBindVertexBuffers(*m_cmdBuffer, firstHalfLength, secondHalfLength,
									vertexBuffers.data() + firstHalfLength,
									bindingOffsets.data() + firstHalfLength);
		}

		vk.cmdDraw(*m_cmdBuffer, 4, 2, 0, 0);

		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

VertexInputInstance::~VertexInputInstance (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();

	for (size_t bufferNdx = 0; bufferNdx < m_vertexBuffers.size(); bufferNdx++)
		vk.destroyBuffer(vkDevice, m_vertexBuffers[bufferNdx], DE_NULL);

	for (size_t allocNdx = 0; allocNdx < m_vertexBufferAllocs.size(); allocNdx++)
		delete m_vertexBufferAllocs[allocNdx];
}

void VertexInputInstance::writeVertexInputData(deUint8* destPtr, const VkVertexInputBindingDescription& bindingDescription, const VkDeviceSize bindingOffset, const AttributeDescriptionList& attributes)
{
	const deUint32 vertexCount = (bindingDescription.inputRate == VK_VERTEX_INPUT_RATE_VERTEX) ? (4 * 2) : 2;

	deUint8* destOffsetPtr = ((deUint8 *)destPtr) + bindingOffset;
	for (deUint32 vertexNdx = 0; vertexNdx < vertexCount; vertexNdx++)
	{
		for (size_t attributeNdx = 0; attributeNdx < attributes.size(); attributeNdx++)
		{
			const VertexInputAttributeDescription& attribDesc = attributes[attributeNdx];

			// Only write vertex input data to bindings referenced by attribute descriptions
			if (attribDesc.vkDescription.binding == bindingDescription.binding)
			{
				writeVertexInputValue(destOffsetPtr + attribDesc.vkDescription.offset, attribDesc, vertexNdx);
			}
		}
		destOffsetPtr += bindingDescription.stride;
	}
}

void writeVertexInputValueSint (deUint8* destPtr, VkFormat format, int componentNdx, deInt32 value)
{
	const deUint32	componentSize	= getVertexFormatComponentSize(format);
	deUint8*		destFormatPtr	= ((deUint8*)destPtr) + componentSize * componentNdx;

	switch (componentSize)
	{
		case 1:
			*((deInt8*)destFormatPtr) = (deInt8)value;
			break;

		case 2:
			*((deInt16*)destFormatPtr) = (deInt16)value;
			break;

		case 4:
			*((deInt32*)destFormatPtr) = (deInt32)value;
			break;

		default:
			DE_ASSERT(false);
	}
}

void writeVertexInputValueIntPacked(deUint8* destPtr, deUint32& packedFormat, deUint32& componentOffset, VkFormat format, deUint32 componentNdx, deUint32 value)
{
	const deUint32	componentWidth	= getPackedVertexFormatComponentWidth(format, componentNdx);
	const deUint32	componentCount	= getVertexFormatComponentCount(format);
	const deUint32	usedBits		= ~(deUint32)0 >> ((getVertexFormatSize(format) * 8) - componentWidth);

	componentOffset -= componentWidth;
	packedFormat |= (((deUint32)value & usedBits) << componentOffset);

	if (componentNdx == componentCount - 1)
		*((deUint32*)destPtr) = (deUint32)packedFormat;
}

void writeVertexInputValueUint (deUint8* destPtr, VkFormat format, int componentNdx, deUint32 value)
{
	const deUint32	componentSize	= getVertexFormatComponentSize(format);
	deUint8*		destFormatPtr	= ((deUint8*)destPtr) + componentSize * componentNdx;

	switch (componentSize)
	{
		case 1:
			*((deUint8 *)destFormatPtr) = (deUint8)value;
			break;

		case 2:
			*((deUint16 *)destFormatPtr) = (deUint16)value;
			break;

		case 4:
			*((deUint32 *)destFormatPtr) = (deUint32)value;
			break;

		default:
			DE_ASSERT(false);
	}
}

void writeVertexInputValueSfloat (deUint8* destPtr, VkFormat format, int componentNdx, float value)
{
	const deUint32	componentSize	= getVertexFormatComponentSize(format);
	deUint8*		destFormatPtr	= ((deUint8*)destPtr) + componentSize * componentNdx;

	switch (componentSize)
	{
		case 2:
		{
			deFloat16 f16 = deFloat32To16(value);
			deMemcpy(destFormatPtr, &f16, sizeof(f16));
			break;
		}

		case 4:
			deMemcpy(destFormatPtr, &value, sizeof(value));
			break;

		default:
			DE_ASSERT(false);
	}
}

void writeVertexInputValueUfloat (deUint8* destPtr, deUint32& packedFormat, deUint32& componentOffset, VkFormat format, deUint32 componentNdx, float value)
{
	deFloat16		f16				= deFloat32To16(value);

	const deUint32	componentWidth	= getPackedVertexFormatComponentWidth(format, componentNdx);
	const deUint32	componentCount	= getVertexFormatComponentCount(format);
	const deUint32	usedBits		= ~(deUint32)0 >> ((getVertexFormatSize(format) * 8) - componentWidth);
	// The ufloat 10 or 11 has no sign bit, but the same exponent bits than float16.
	// The sign bit will be removed by the mask. Therefore we pick one more mantissa bit.
	deUint32		valueUFloat		= f16 >> (16 - componentWidth - 1);

	// TODO: VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 not supported.
	DE_ASSERT(format == VK_FORMAT_B10G11R11_UFLOAT_PACK32);

	componentOffset -= componentWidth;
	packedFormat |= (valueUFloat & usedBits) << componentOffset;

	if (componentNdx == componentCount - 1)
		*((deUint32*)destPtr) = (deUint32)packedFormat;
}


void VertexInputInstance::writeVertexInputValue (deUint8* destPtr, const VertexInputAttributeDescription& attribute, int indexId)
{
	const int		vertexInputCount	= VertexInputTest::s_glslTypeDescriptions[attribute.glslType].vertexInputCount;
	const int		componentCount		= VertexInputTest::s_glslTypeDescriptions[attribute.glslType].vertexInputComponentCount;
	const deUint32	totalComponentCount	= componentCount * vertexInputCount;
	const deUint32	vertexInputIndex	= indexId * totalComponentCount + attribute.vertexInputIndex * componentCount;
	const bool		hasBGROrder			= isVertexFormatComponentOrderBGR(attribute.vkDescription.format);
	const bool		hasABGROrder		= isVertexFormatComponentOrderABGR(attribute.vkDescription.format);
	const bool		hasARGBOrder		= isVertexFormatComponentOrderARGB(attribute.vkDescription.format);
	deUint32		componentOffset		= getVertexFormatSize(attribute.vkDescription.format) * 8;
	deUint32		packedFormat32		= 0;
	deUint32		swizzledNdx;

	const deUint32	BGRSwizzle[]		= { 2, 1, 0, 3 };
	const deUint32	ABGRSwizzle[]		= { 3, 2, 1, 0 };
	const deUint32	ARGBSwizzle[]		= { 3, 0, 1, 2 };

	for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
	{
		if (hasABGROrder)
			swizzledNdx = ABGRSwizzle[componentNdx];
		else if (hasARGBOrder)
			swizzledNdx = ARGBSwizzle[componentNdx];
		else if (hasBGROrder)
			swizzledNdx = BGRSwizzle[componentNdx];
		else
			swizzledNdx = componentNdx;

		const deInt32	maxIntValue		= isVertexFormatPacked(attribute.vkDescription.format) ? (1 << (getPackedVertexFormatComponentWidth(attribute.vkDescription.format, componentNdx) - 1)) - 1 : (1 << (getVertexFormatComponentSize(attribute.vkDescription.format) * 8 - 1)) - 1;
		const deUint32	maxUintValue	= isVertexFormatPacked(attribute.vkDescription.format) ? ((1 << getPackedVertexFormatComponentWidth(attribute.vkDescription.format, componentNdx)) - 1) : (1 << (getVertexFormatComponentSize(attribute.vkDescription.format) * 8 )) - 1;
		const deInt32	minIntValue		= -maxIntValue;
		const deUint32	minUintValue	= 0;

		switch (attribute.glslType)
		{
			case VertexInputTest::GLSL_TYPE_INT:
			case VertexInputTest::GLSL_TYPE_IVEC2:
			case VertexInputTest::GLSL_TYPE_IVEC3:
			case VertexInputTest::GLSL_TYPE_IVEC4:
			{
				if (isVertexFormatPacked(attribute.vkDescription.format))
					writeVertexInputValueIntPacked(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, deClamp32(-(deInt32)(vertexInputIndex + swizzledNdx), minIntValue, maxIntValue));
				else
					writeVertexInputValueSint(destPtr, attribute.vkDescription.format, componentNdx, -(deInt32)(vertexInputIndex + swizzledNdx));

				break;
			}
			case VertexInputTest::GLSL_TYPE_UINT:
			case VertexInputTest::GLSL_TYPE_UVEC2:
			case VertexInputTest::GLSL_TYPE_UVEC3:
			case VertexInputTest::GLSL_TYPE_UVEC4:
			{
				if (isVertexFormatPacked(attribute.vkDescription.format))
					writeVertexInputValueIntPacked(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, deClamp32(vertexInputIndex + swizzledNdx, minUintValue, maxUintValue));
				else
					writeVertexInputValueUint(destPtr, attribute.vkDescription.format, componentNdx, vertexInputIndex + swizzledNdx);

				break;
			}
			case VertexInputTest::GLSL_TYPE_FLOAT:
			case VertexInputTest::GLSL_TYPE_VEC2:
			case VertexInputTest::GLSL_TYPE_VEC3:
			case VertexInputTest::GLSL_TYPE_VEC4:
			case VertexInputTest::GLSL_TYPE_MAT2:
			case VertexInputTest::GLSL_TYPE_MAT3:
			case VertexInputTest::GLSL_TYPE_MAT4:
			case VertexInputTest::GLSL_TYPE_F16:
			case VertexInputTest::GLSL_TYPE_F16VEC2:
			case VertexInputTest::GLSL_TYPE_F16VEC3:
			case VertexInputTest::GLSL_TYPE_F16VEC4:
			{
				if (isVertexFormatSfloat(attribute.vkDescription.format))
				{
					writeVertexInputValueSfloat(destPtr, attribute.vkDescription.format, componentNdx, -(0.01f * (float)(vertexInputIndex + swizzledNdx)));
				} else if (isVertexFormatUfloat(attribute.vkDescription.format))
				{
					writeVertexInputValueUfloat(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, 0.01f * (float)(vertexInputIndex + swizzledNdx));
				}
				else if (isVertexFormatSscaled(attribute.vkDescription.format))
				{
					if (isVertexFormatPacked(attribute.vkDescription.format))
						writeVertexInputValueIntPacked(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, deClamp32(-(deInt32)(vertexInputIndex + swizzledNdx), minIntValue, maxIntValue));
					else
						writeVertexInputValueSint(destPtr, attribute.vkDescription.format, componentNdx, -(deInt32)(vertexInputIndex + swizzledNdx));
				}
				else if (isVertexFormatUscaled(attribute.vkDescription.format) || isVertexFormatUnorm(attribute.vkDescription.format) || isVertexFormatSRGB(attribute.vkDescription.format))
				{
					if (isVertexFormatPacked(attribute.vkDescription.format))
						writeVertexInputValueIntPacked(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, deClamp32(vertexInputIndex + swizzledNdx, minUintValue, maxUintValue));
					else
						writeVertexInputValueUint(destPtr, attribute.vkDescription.format, componentNdx, vertexInputIndex + swizzledNdx);
				}
				else if (isVertexFormatSnorm(attribute.vkDescription.format))
				{
					if (isVertexFormatPacked(attribute.vkDescription.format))
						writeVertexInputValueIntPacked(destPtr, packedFormat32, componentOffset, attribute.vkDescription.format, componentNdx, deClamp32(minIntValue + (vertexInputIndex + swizzledNdx), minIntValue, maxIntValue));
					else
						writeVertexInputValueSint(destPtr, attribute.vkDescription.format, componentNdx, minIntValue + (vertexInputIndex + swizzledNdx));
				}
				else
					DE_ASSERT(false);
				break;
			}
			case VertexInputTest::GLSL_TYPE_DOUBLE:
			case VertexInputTest::GLSL_TYPE_DVEC2:
			case VertexInputTest::GLSL_TYPE_DVEC3:
			case VertexInputTest::GLSL_TYPE_DVEC4:
			case VertexInputTest::GLSL_TYPE_DMAT2:
			case VertexInputTest::GLSL_TYPE_DMAT3:
			case VertexInputTest::GLSL_TYPE_DMAT4:
				*(reinterpret_cast<double *>(destPtr) + componentNdx) = -0.01 * (vertexInputIndex + swizzledNdx);

				break;

			default:
				DE_ASSERT(false);
		}
	}
}

tcu::TestStatus VertexInputInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

bool VertexInputTest::isCompatibleType (VkFormat format, GlslType glslType)
{
	const GlslTypeDescription glslTypeDesc = s_glslTypeDescriptions[glslType];

	if ((deUint32)s_glslTypeDescriptions[glslType].vertexInputComponentCount == getVertexFormatComponentCount(format))
	{
		switch (glslTypeDesc.basicType)
		{
			case GLSL_BASIC_TYPE_INT:
				return isVertexFormatSint(format);

			case GLSL_BASIC_TYPE_UINT:
				return isVertexFormatUint(format);

			case GLSL_BASIC_TYPE_FLOAT:
				return (isVertexFormatPacked(format) ? (getVertexFormatSize(format) <= 4) : getVertexFormatComponentSize(format) <= 4) && (isVertexFormatSfloat(format) ||
					isVertexFormatSnorm(format) || isVertexFormatUnorm(format) || isVertexFormatSscaled(format) || isVertexFormatUscaled(format) || isVertexFormatSRGB(format) ||
					isVertexFormatUfloat(format));

			case GLSL_BASIC_TYPE_DOUBLE:
				return isVertexFormatSfloat(format) && getVertexFormatComponentSize(format) == 8;

			case GLSL_BASIC_TYPE_FLOAT16:
				return ((isVertexFormatSfloat(format)/* || isVertexFormatSnorm(format) || isVertexFormatUnorm(format)*/) && getVertexFormatComponentSize(format) == 2);

			default:
				DE_ASSERT(false);
				return false;
		}
	}
	else
		return false;
}

tcu::TestStatus VertexInputInstance::verifyImage (void)
{
	bool							compareOk			= false;
	const tcu::TextureFormat		tcuColorFormat		= mapVkFormat(m_colorFormat);
	tcu::TextureLevel				reference			(tcuColorFormat, m_renderSize.x(), m_renderSize.y());
	const tcu::PixelBufferAccess	refRedSubregion		(tcu::getSubregion(reference.getAccess(),
																		   deRoundFloatToInt32((float)m_renderSize.x() * 0.0f),
																		   deRoundFloatToInt32((float)m_renderSize.y() * 0.0f),
																		   deRoundFloatToInt32((float)m_renderSize.x() * 0.5f),
																		   deRoundFloatToInt32((float)m_renderSize.y() * 1.0f)));
	const tcu::PixelBufferAccess	refBlueSubregion	(tcu::getSubregion(reference.getAccess(),
																		   deRoundFloatToInt32((float)m_renderSize.x() * 0.5f),
																		   deRoundFloatToInt32((float)m_renderSize.y() * 0.0f),
																		   deRoundFloatToInt32((float)m_renderSize.x() * 0.5f),
																		   deRoundFloatToInt32((float)m_renderSize.y() * 1.0f)));

	// Create reference image
	tcu::clear(reference.getAccess(), defaultClearColor(tcuColorFormat));
	tcu::clear(refRedSubregion, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	tcu::clear(refBlueSubregion, tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

	// Compare result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  reference.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

std::string getAttributeInfoCaseName (const VertexInputTest::AttributeInfo& attributeInfo)
{
	std::ostringstream	caseName;
	const std::string	formatName	= getFormatName(attributeInfo.vkType);

	caseName << "as_" << de::toLower(formatName.substr(10)) << "_rate_";

	if (attributeInfo.inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
		caseName <<  "vertex";
	else
		caseName <<  "instance";

	return caseName.str();
}

std::string getAttributeInfoDescription (const VertexInputTest::AttributeInfo& attributeInfo)
{
	std::ostringstream caseDesc;

	caseDesc << std::string(VertexInputTest::s_glslTypeDescriptions[attributeInfo.glslType].name) << " from type " << getFormatName(attributeInfo.vkType) <<  " with ";

	if (attributeInfo.inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
		caseDesc <<  "vertex input rate ";
	else
		caseDesc <<  "instance input rate ";

	return caseDesc.str();
}

std::string getAttributeInfosDescription (const std::vector<VertexInputTest::AttributeInfo>& attributeInfos)
{
	std::ostringstream caseDesc;

	caseDesc << "Uses vertex attributes:\n";

	for (size_t attributeNdx = 0; attributeNdx < attributeInfos.size(); attributeNdx++)
		caseDesc << "\t- " << getAttributeInfoDescription (attributeInfos[attributeNdx]) << "\n";

	return caseDesc.str();
}

struct CompatibleFormats
{
	VertexInputTest::GlslType	glslType;
	std::vector<VkFormat>		compatibleVkFormats;
};

void createSingleAttributeCases (tcu::TestCaseGroup* singleAttributeTests, PipelineConstructionType pipelineConstructionType, VertexInputTest::GlslType glslType)
{
	const VkFormat vertexFormats[] =
	{
		// Required, unpacked
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,

		// Scaled formats
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,

		// SRGB formats
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_SRGB,

		// Double formats
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_SFLOAT,

		// Packed formats
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32
	};

	for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); formatNdx++)
	{
		if (VertexInputTest::isCompatibleType(vertexFormats[formatNdx], glslType))
		{
			{
				// Create test case for RATE_VERTEX
				VertexInputTest::AttributeInfo attributeInfo;
				attributeInfo.vkType = vertexFormats[formatNdx];
				attributeInfo.glslType = glslType;
				attributeInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

				singleAttributeTests->addChild(new VertexInputTest(singleAttributeTests->getTestContext(),
																getAttributeInfoCaseName(attributeInfo),
																getAttributeInfoDescription(attributeInfo),
																pipelineConstructionType,
																std::vector<VertexInputTest::AttributeInfo>(1, attributeInfo),
																VertexInputTest::BINDING_MAPPING_ONE_TO_ONE,
																VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED));

				// Create test case for RATE_INSTANCE
				attributeInfo.inputRate	= VK_VERTEX_INPUT_RATE_INSTANCE;

				singleAttributeTests->addChild(new VertexInputTest(singleAttributeTests->getTestContext(),
																getAttributeInfoCaseName(attributeInfo),
																getAttributeInfoDescription(attributeInfo),
																pipelineConstructionType,
																std::vector<VertexInputTest::AttributeInfo>(1, attributeInfo),
																VertexInputTest::BINDING_MAPPING_ONE_TO_ONE,
																VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED));
			}

			// Test accessing missing components to verify "Converstion to RGBA" is correctly applied.
			const auto& typeInfo = VertexInputTest::s_glslTypeDescriptions[glslType];
			if (typeInfo.vertexInputComponentCount < kMaxComponents && typeInfo.basicType != VertexInputTest::GLSL_BASIC_TYPE_DOUBLE)
			{
				// Create test case for RATE_VERTEX
				VertexInputTest::AttributeInfo attributeInfo;
				attributeInfo.vkType = vertexFormats[formatNdx];
				attributeInfo.glslType = glslType;
				attributeInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				const auto nameSuffix = "_missing_components";
				const auto descSuffix = " using missing components";

				singleAttributeTests->addChild(new VertexInputTest(singleAttributeTests->getTestContext(),
																getAttributeInfoCaseName(attributeInfo) + nameSuffix,
																getAttributeInfoDescription(attributeInfo) + descSuffix,
																pipelineConstructionType,
																std::vector<VertexInputTest::AttributeInfo>(1, attributeInfo),
																VertexInputTest::BINDING_MAPPING_ONE_TO_ONE,
																VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED,
																VertexInputTest::LAYOUT_SKIP_DISABLED,
																VertexInputTest::LAYOUT_ORDER_IN_ORDER,
																true));

				// Create test case for RATE_INSTANCE
				attributeInfo.inputRate	= VK_VERTEX_INPUT_RATE_INSTANCE;

				singleAttributeTests->addChild(new VertexInputTest(singleAttributeTests->getTestContext(),
																getAttributeInfoCaseName(attributeInfo) + nameSuffix,
																getAttributeInfoDescription(attributeInfo) + descSuffix,
																pipelineConstructionType,
																std::vector<VertexInputTest::AttributeInfo>(1, attributeInfo),
																VertexInputTest::BINDING_MAPPING_ONE_TO_ONE,
																VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED,
																VertexInputTest::LAYOUT_SKIP_DISABLED,
																VertexInputTest::LAYOUT_ORDER_IN_ORDER,
																true));
			}
		}
	}
}

void createSingleAttributeTests (tcu::TestCaseGroup* singleAttributeTests, PipelineConstructionType pipelineConstructionType)
{
	for (int glslTypeNdx = 0; glslTypeNdx < VertexInputTest::GLSL_TYPE_COUNT; glslTypeNdx++)
	{
		VertexInputTest::GlslType glslType = (VertexInputTest::GlslType)glslTypeNdx;
		addTestGroup(singleAttributeTests, VertexInputTest::s_glslTypeDescriptions[glslType].name, "", createSingleAttributeCases, pipelineConstructionType, glslType);
	}
}

// Create all unique GlslType combinations recursively
void createMultipleAttributeCases (PipelineConstructionType pipelineConstructionType, deUint32 depth, deUint32 firstNdx, CompatibleFormats* compatibleFormats, de::Random& randomFunc, tcu::TestCaseGroup& testGroup, VertexInputTest::BindingMapping bindingMapping, VertexInputTest::AttributeLayout attributeLayout, VertexInputTest::LayoutSkip layoutSkip, VertexInputTest::LayoutOrder layoutOrder, const std::vector<VertexInputTest::AttributeInfo>& attributeInfos = std::vector<VertexInputTest::AttributeInfo>(0))
{
	tcu::TestContext& testCtx = testGroup.getTestContext();

	// Exclude double values, which are not included in vertexFormats
	for (deUint32 currentNdx = firstNdx; currentNdx < VertexInputTest::GLSL_TYPE_DOUBLE - depth; currentNdx++)
	{
		std::vector <VertexInputTest::AttributeInfo> newAttributeInfos = attributeInfos;

		{
			VertexInputTest::AttributeInfo attributeInfo;
			attributeInfo.glslType	= (VertexInputTest::GlslType)currentNdx;
			attributeInfo.inputRate	= (depth % 2 == 0) ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
			attributeInfo.vkType	= VK_FORMAT_UNDEFINED;

			newAttributeInfos.push_back(attributeInfo);
		}

		// Add test case
		if (depth == 0)
		{
			// Select a random compatible format for each attribute
			for (size_t i = 0; i < newAttributeInfos.size(); i++)
			{
				const std::vector<VkFormat>& formats = compatibleFormats[newAttributeInfos[i].glslType].compatibleVkFormats;
				newAttributeInfos[i].vkType = formats[randomFunc.getUint32() % formats.size()];
			}

			const std::string caseName = VertexInputTest::s_glslTypeDescriptions[currentNdx].name;
			const std::string caseDesc = getAttributeInfosDescription(newAttributeInfos);

			testGroup.addChild(new VertexInputTest(testCtx, caseName, caseDesc, pipelineConstructionType, newAttributeInfos, bindingMapping, attributeLayout, layoutSkip, layoutOrder));
		}
		// Add test group
		else
		{
			const std::string				name			= VertexInputTest::s_glslTypeDescriptions[currentNdx].name;
			de::MovePtr<tcu::TestCaseGroup>	newTestGroup	(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

			createMultipleAttributeCases(pipelineConstructionType, depth - 1u, currentNdx + 1u, compatibleFormats, randomFunc, *newTestGroup, bindingMapping, attributeLayout, layoutSkip, layoutOrder, newAttributeInfos);
			testGroup.addChild(newTestGroup.release());
		}
	}
}

void createMultipleAttributeTests (tcu::TestCaseGroup* multipleAttributeTests, PipelineConstructionType pipelineConstructionType)
{
	// Required vertex formats, unpacked
	const VkFormat vertexFormats[] =
	{
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};

	const VertexInputTest::LayoutSkip layoutSkips[] =
	{
		VertexInputTest::LAYOUT_SKIP_DISABLED,
		VertexInputTest::LAYOUT_SKIP_ENABLED
	};

	const VertexInputTest::LayoutOrder layoutOrders[] =
	{
		VertexInputTest::LAYOUT_ORDER_IN_ORDER,
		VertexInputTest::LAYOUT_ORDER_OUT_OF_ORDER
	};

	// Find compatible VK formats for each GLSL vertex type
	CompatibleFormats compatibleFormats[VertexInputTest::GLSL_TYPE_COUNT];
	{
		for (int glslTypeNdx = 0; glslTypeNdx < VertexInputTest::GLSL_TYPE_COUNT; glslTypeNdx++)
		{
			for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); formatNdx++)
			{
				if (VertexInputTest::isCompatibleType(vertexFormats[formatNdx], (VertexInputTest::GlslType)glslTypeNdx))
					compatibleFormats[glslTypeNdx].compatibleVkFormats.push_back(vertexFormats[formatNdx]);
			}
		}
	}

	de::Random						randomFunc(102030);
	tcu::TestContext&				testCtx = multipleAttributeTests->getTestContext();

	for (deUint32 layoutSkipNdx = 0; layoutSkipNdx < DE_LENGTH_OF_ARRAY(layoutSkips); layoutSkipNdx++)
	for (deUint32 layoutOrderNdx = 0; layoutOrderNdx < DE_LENGTH_OF_ARRAY(layoutOrders); layoutOrderNdx++)
	{
		const VertexInputTest::LayoutSkip	layoutSkip	= layoutSkips[layoutSkipNdx];
		const VertexInputTest::LayoutOrder	layoutOrder	= layoutOrders[layoutOrderNdx];
		de::MovePtr<tcu::TestCaseGroup> oneToOneAttributeTests(new tcu::TestCaseGroup(testCtx, "attributes", ""));
		de::MovePtr<tcu::TestCaseGroup> oneToManyAttributeTests(new tcu::TestCaseGroup(testCtx, "attributes", ""));
		de::MovePtr<tcu::TestCaseGroup> oneToManySequentialAttributeTests(new tcu::TestCaseGroup(testCtx, "attributes_sequential", ""));

		if (layoutSkip == VertexInputTest::LAYOUT_SKIP_ENABLED && layoutOrder == VertexInputTest::LAYOUT_ORDER_OUT_OF_ORDER)
			continue;

		createMultipleAttributeCases(pipelineConstructionType, 2u, 0u, compatibleFormats, randomFunc, *oneToOneAttributeTests,			VertexInputTest::BINDING_MAPPING_ONE_TO_ONE,	VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED, layoutSkip, layoutOrder);
		createMultipleAttributeCases(pipelineConstructionType, 2u, 0u, compatibleFormats, randomFunc, *oneToManyAttributeTests,			VertexInputTest::BINDING_MAPPING_ONE_TO_MANY,	VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED, layoutSkip, layoutOrder);
		createMultipleAttributeCases(pipelineConstructionType, 2u, 0u, compatibleFormats, randomFunc, *oneToManySequentialAttributeTests,	VertexInputTest::BINDING_MAPPING_ONE_TO_MANY,	VertexInputTest::ATTRIBUTE_LAYOUT_SEQUENTIAL, layoutSkip, layoutOrder);

		if (layoutSkip == VertexInputTest::LAYOUT_SKIP_ENABLED)
		{
			de::MovePtr<tcu::TestCaseGroup> layoutSkipTests(new tcu::TestCaseGroup(testCtx, "layout_skip", "Skip one layout after each attribute"));

			de::MovePtr<tcu::TestCaseGroup> bindingOneToOneTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_one", "Each attribute uses a unique binding"));
			bindingOneToOneTests->addChild(oneToOneAttributeTests.release());
			layoutSkipTests->addChild(bindingOneToOneTests.release());

			de::MovePtr<tcu::TestCaseGroup> bindingOneToManyTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_many", "Attributes share the same binding"));
			bindingOneToManyTests->addChild(oneToManyAttributeTests.release());
			bindingOneToManyTests->addChild(oneToManySequentialAttributeTests.release());
			layoutSkipTests->addChild(bindingOneToManyTests.release());
			multipleAttributeTests->addChild(layoutSkipTests.release());
		}
		else if (layoutOrder == VertexInputTest::LAYOUT_ORDER_OUT_OF_ORDER)
		{
			de::MovePtr<tcu::TestCaseGroup> layoutOutOfOrderTests(new tcu::TestCaseGroup(testCtx, "out_of_order", "Layout slots out of order"));

			de::MovePtr<tcu::TestCaseGroup> bindingOneToOneTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_one", "Each attribute uses a unique binding"));
			bindingOneToOneTests->addChild(oneToOneAttributeTests.release());
			layoutOutOfOrderTests->addChild(bindingOneToOneTests.release());

			de::MovePtr<tcu::TestCaseGroup> bindingOneToManyTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_many", "Attributes share the same binding"));
			bindingOneToManyTests->addChild(oneToManyAttributeTests.release());
			bindingOneToManyTests->addChild(oneToManySequentialAttributeTests.release());
			layoutOutOfOrderTests->addChild(bindingOneToManyTests.release());
			multipleAttributeTests->addChild(layoutOutOfOrderTests.release());
		}
		else
		{
			de::MovePtr<tcu::TestCaseGroup> bindingOneToOneTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_one", "Each attribute uses a unique binding"));
			bindingOneToOneTests->addChild(oneToOneAttributeTests.release());
			multipleAttributeTests->addChild(bindingOneToOneTests.release());

			de::MovePtr<tcu::TestCaseGroup> bindingOneToManyTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_many", "Attributes share the same binding"));
			bindingOneToManyTests->addChild(oneToManyAttributeTests.release());
			bindingOneToManyTests->addChild(oneToManySequentialAttributeTests.release());
			multipleAttributeTests->addChild(bindingOneToManyTests.release());
		}
	}
}

void createMaxAttributeTests (tcu::TestCaseGroup* maxAttributeTests, PipelineConstructionType pipelineConstructionType)
{
	// Required vertex formats, unpacked
	const VkFormat					vertexFormats[]		=
	{
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};

	// VkPhysicalDeviceLimits::maxVertexInputAttributes is used when attributeCount is 0
	const deUint32					attributeCount[]	= { 16, 32, 64, 128, 0 };
	tcu::TestContext&				testCtx				(maxAttributeTests->getTestContext());
	de::Random						randomFunc			(132030);

	// Find compatible VK formats for each GLSL vertex type
	CompatibleFormats compatibleFormats[VertexInputTest::GLSL_TYPE_COUNT];
	{
		for (int glslTypeNdx = 0; glslTypeNdx < VertexInputTest::GLSL_TYPE_COUNT; glslTypeNdx++)
		{
			for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); formatNdx++)
			{
				if (VertexInputTest::isCompatibleType(vertexFormats[formatNdx], (VertexInputTest::GlslType)glslTypeNdx))
					compatibleFormats[glslTypeNdx].compatibleVkFormats.push_back(vertexFormats[formatNdx]);
			}
		}
	}

	for (deUint32 attributeCountNdx = 0; attributeCountNdx < DE_LENGTH_OF_ARRAY(attributeCount); attributeCountNdx++)
	{
		const std::string							groupName = (attributeCount[attributeCountNdx] == 0 ? "query_max" : de::toString(attributeCount[attributeCountNdx])) + "_attributes";
		const std::string							groupDesc = de::toString(attributeCount[attributeCountNdx]) + " vertex input attributes";

		de::MovePtr<tcu::TestCaseGroup>				numAttributeTests(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));
		de::MovePtr<tcu::TestCaseGroup>				bindingOneToOneTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_one", "Each attribute uses a unique binding"));
		de::MovePtr<tcu::TestCaseGroup>				bindingOneToManyTests(new tcu::TestCaseGroup(testCtx, "binding_one_to_many", "Attributes share the same binding"));

		std::vector<VertexInputTest::AttributeInfo>	attributeInfos(attributeCount[attributeCountNdx]);

		for (deUint32 attributeNdx = 0; attributeNdx < attributeCount[attributeCountNdx]; attributeNdx++)
		{
			// Use random glslTypes, each consuming one attribute location
			const VertexInputTest::GlslType	glslType	= (VertexInputTest::GlslType)(randomFunc.getUint32() % VertexInputTest::GLSL_TYPE_MAT2);
			const std::vector<VkFormat>&	formats		= compatibleFormats[glslType].compatibleVkFormats;
			const VkFormat					format		= formats[randomFunc.getUint32() % formats.size()];

			attributeInfos[attributeNdx].glslType		= glslType;
			attributeInfos[attributeNdx].inputRate		= ((attributeCountNdx + attributeNdx) % 2 == 0) ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
			attributeInfos[attributeNdx].vkType			= format;
		}

		bindingOneToOneTests->addChild(new VertexInputTest(testCtx, "interleaved", "Interleaved attribute layout", pipelineConstructionType, attributeInfos, VertexInputTest::BINDING_MAPPING_ONE_TO_ONE, VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED));
		bindingOneToManyTests->addChild(new VertexInputTest(testCtx, "interleaved", "Interleaved attribute layout", pipelineConstructionType, attributeInfos, VertexInputTest::BINDING_MAPPING_ONE_TO_MANY, VertexInputTest::ATTRIBUTE_LAYOUT_INTERLEAVED));
		bindingOneToManyTests->addChild(new VertexInputTest(testCtx, "sequential", "Sequential attribute layout", pipelineConstructionType, attributeInfos, VertexInputTest::BINDING_MAPPING_ONE_TO_MANY, VertexInputTest::ATTRIBUTE_LAYOUT_SEQUENTIAL));

		numAttributeTests->addChild(bindingOneToOneTests.release());
		numAttributeTests->addChild(bindingOneToManyTests.release());
		maxAttributeTests->addChild(numAttributeTests.release());
	}
}

} // anonymous

void createVertexInputTests (tcu::TestCaseGroup* vertexInputTests, PipelineConstructionType pipelineConstructionType)
{
	addTestGroup(vertexInputTests, "single_attribute", "Uses one attribute", createSingleAttributeTests, pipelineConstructionType);
	addTestGroup(vertexInputTests, "multiple_attributes", "Uses more than one attribute", createMultipleAttributeTests, pipelineConstructionType);
	addTestGroup(vertexInputTests, "max_attributes", "Implementations can use as many vertex input attributes as they advertise", createMaxAttributeTests, pipelineConstructionType);
}

} // pipeline
} // vkt
