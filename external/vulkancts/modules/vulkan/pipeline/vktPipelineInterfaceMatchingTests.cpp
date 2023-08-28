/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \file vktPipelineInterfaceMatchingTests.cpp
 * \brief Interface matching tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineInterfaceMatchingTests.hpp"
#include "vktPipelineImageUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuTestCase.hpp"
#include "tcuStringTemplate.hpp"

#include <set>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using namespace de;
using namespace tcu;

namespace
{

enum class TestType
{
	VECTOR_LENGTH			= 0,
	DECORATION_MISMATCH,
};

enum class VecType
{
	VEC2 = 0,
	VEC3,
	VEC4,
	IVEC2,
	IVEC3,
	IVEC4,
	UVEC2,
	UVEC3,
	UVEC4,
};

enum class DecorationType
{
	NONE = 0,
	FLAT,
	NO_PERSPECTIVE,
	COMPONENT0
};

enum class PipelineType
{
	// all combinations with vert and frag
	VERT_OUT_FRAG_IN = 0,

	// all combinations with vert, tesc, tese and frag
	VERT_OUT_TESC_IN_TESE_FRAG,
	VERT_TESC_TESE_OUT_FRAG_IN,
	VERT_TESC_OUT_TESE_IN_FRAG,

	// all combinations with vert, geom and frag
	VERT_OUT_GEOM_IN_FRAG,
	VERT_GEOM_OUT_FRAG_IN,

	// all combinations with vert, tesc, tese, geom and frag
	VERT_OUT_TESC_IN_TESE_GEOM_FRAG,		// this won't add coverage as it is similar to VERT_OUT_TESC_IN_TESE_FRAG
	//VERT_TESC_OUT_TESE_IN_GEOM_FRAG,		// this won't add coverage as it is similar to VERT_TESC_OUT_TESE_IN_FRAG
	VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
	VERT_TESC_TESE_GEOM_OUT_FRAG_IN,
};

enum class DefinitionType
{
	LOOSE_VARIABLE = 0,
	MEMBER_OF_BLOCK,
	MEMBER_OF_STRUCTURE,
	MEMBER_OF_ARRAY_OF_STRUCTURES,
	MEMBER_OF_STRUCTURE_IN_BLOCK,
	MEMBER_OF_ARRAY_OF_STRUCTURES_IN_BLOCK,
};

struct TestParams
{
	PipelineConstructionType	pipelineConstructionType;
	TestType					testType;

	VecType						outVecType;
	VecType						inVecType;

	DecorationType				outDeclDecoration;
	DecorationType				inDeclDecoration;

	PipelineType				pipelineType;
	DefinitionType				definitionType;
};

typedef de::SharedPtr<TestParams> TestParamsSp;

// helper function that check if specified pipeline is in set of pipelines
bool isPipelineOneOf(PipelineType pipelineType, std::set<PipelineType> pipelines)
{
	return !!pipelines.count(pipelineType);
}

class InterfaceMatchingTestInstance : public vkt::TestInstance
{
public:
						InterfaceMatchingTestInstance	(Context&			context,
														 const TestParamsSp	params);
	virtual				~InterfaceMatchingTestInstance	(void) = default;

	tcu::TestStatus		iterate(void) override;

private:
	TestParamsSp				m_params;
	SimpleAllocator				m_alloc;

	Move<VkBuffer>				m_vertexBuffer;
	de::MovePtr<Allocation>		m_vertexBufferAlloc;
	Move<VkBuffer>				m_resultBuffer;
	de::MovePtr<Allocation>		m_resultBufferAlloc;

	Move<VkImage>				m_colorImage;
	de::MovePtr<Allocation>		m_colorImageAlloc;
	Move<VkImageView>			m_colorAttachmentView;
	RenderPassWrapper			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;

	ShaderWrapper				m_vertShaderModule;
	ShaderWrapper				m_tescShaderModule;
	ShaderWrapper				m_teseShaderModule;
	ShaderWrapper				m_geomShaderModule;
	ShaderWrapper				m_fragShaderModule;

	PipelineLayoutWrapper		m_pipelineLayout;
	GraphicsPipelineWrapper		m_graphicsPipeline;

	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBuffer;
};

InterfaceMatchingTestInstance::InterfaceMatchingTestInstance(Context& context, const TestParamsSp params)
	: vkt::TestInstance(context)
	, m_params(params)
	, m_alloc(context.getDeviceInterface(), context.getDevice(),
			  getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
	, m_graphicsPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(), context.getDevice(), context.getDeviceExtensions(), params->pipelineConstructionType)
{
}

tcu::TestStatus InterfaceMatchingTestInstance::iterate(void)
{
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	const VkQueue				queue					= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkComponentMapping	componentMappingRGBA	= makeComponentMappingRGBA();
	VkImageSubresourceRange		subresourceRange		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	const VkFormat				colorFormat				(VK_FORMAT_R8G8B8A8_UNORM);
	const tcu::UVec2			renderSize				(16, 16);
	const tcu::TextureFormat	textureFormat			= mapVkFormat(colorFormat);
	const VkDeviceSize			pixelDataSize			= renderSize.x() * renderSize.y() * textureFormat.getPixelSize();
	const VkDeviceSize			vertexBufferOffset		= 0;

	// create color image that is used as a color attachment
	{
		const VkImageCreateInfo colorImageParams
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			colorFormat,																// VkFormat					format;
			{ renderSize.x(), renderSize.y(), 1u },										// VkExtent3D				extent;
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

		m_colorImage = createImage(vk, device, &colorImageParams);

		// allocate and bind color image memory
		m_colorImageAlloc = m_alloc.allocate(getImageMemoryRequirements(vk, device, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageViewCreateFlags	flags;
			*m_colorImage,																// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,														// VkImageViewType			viewType;
			colorFormat,																// VkFormat					format;
			componentMappingRGBA,														// VkComponentMapping		components;
			subresourceRange															// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, device, &colorAttachmentViewParams);
	}

	// create render pass
	m_renderPass = RenderPassWrapper(m_params->pipelineConstructionType, vk, device, colorFormat);

	// create framebuffer
	{
		const VkFramebufferCreateInfo framebufferParams
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkFramebufferCreateFlags	flags;
			*m_renderPass,																// VkRenderPass				renderPass;
			1u,																			// deUint32					attachmentCount;
			&m_colorAttachmentView.get(),												// const VkImageView*		pAttachments;
			(deUint32)renderSize.x(),													// deUint32					width;
			(deUint32)renderSize.y(),													// deUint32					height;
			1u																			// deUint32					layers;
		};

		m_renderPass.createFramebuffer(vk, device, &framebufferParams, *m_colorImage);
	}

	// create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,								// VkStructureType					sType;
			DE_NULL,																	// const void*						pNext;
			0u,																			// VkPipelineLayoutCreateFlags		flags;
			0u,																			// deUint32							setLayoutCount;
			DE_NULL,																	// const VkDescriptorSetLayout*		pSetLayouts;
			0u,																			// deUint32							pushConstantRangeCount;
			DE_NULL																		// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_params->pipelineConstructionType, vk, device, &pipelineLayoutParams);
	}

	// create pipeline
	bool useTess = isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,
		PipelineType::VERT_TESC_OUT_TESE_IN_FRAG,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN });

	m_vertShaderModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	m_fragShaderModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"), 0);
	if (useTess)
	{
		m_tescShaderModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tesc"), 0);
		m_teseShaderModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("tese"), 0);
	}

	if (isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_GEOM_OUT_FRAG_IN,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN }))
	{
		m_geomShaderModule = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("geom"), 0);
	}

	const std::vector<VkViewport>	viewports	{ makeViewport(renderSize) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(renderSize) };

	m_graphicsPipeline.setDefaultTopology(useTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
					  .setDefaultRasterizationState()
					  .setDefaultDepthStencilState()
					  .setDefaultMultisampleState()
					  .setDefaultColorBlendState()
					  .setupVertexInputState()
					  .setupPreRasterizationShaderState(viewports,
														scissors,
														m_pipelineLayout,
														*m_renderPass,
														0u,
														m_vertShaderModule,
														DE_NULL,
														m_tescShaderModule,
														m_teseShaderModule,
														m_geomShaderModule)
					  .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, m_fragShaderModule)
					  .setupFragmentOutputState(*m_renderPass)
					  .setMonolithicPipelineLayout(m_pipelineLayout)
					  .buildPipeline();

	// create vertex buffer
	{
		std::vector<float> vertices
		{
			 1.0f, -1.0f, 0.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 1.0f,
		};
		const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexBuffer		= createBuffer(vk, device, &bufferCreateInfo);
		m_vertexBufferAlloc = m_alloc.allocate(getBufferMemoryRequirements(vk, device, *m_vertexBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
		flushAlloc(vk, device, *m_vertexBufferAlloc);
	}

	// create buffer to which we will grab rendered result
	{
		const VkBufferCreateInfo	bufferCreateInfo	= makeBufferCreateInfo(pixelDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		m_resultBuffer		= createBuffer(vk, device, &bufferCreateInfo);
		m_resultBufferAlloc = m_alloc.allocate(getBufferMemoryRequirements(vk, device, *m_resultBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *m_resultBuffer, m_resultBufferAlloc->getMemory(), m_resultBufferAlloc->getOffset()));
	}

	// create command pool and command buffer
	m_cmdPool	= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// record command buffer
	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// change image layout so we can use it as color attachment
	const VkImageMemoryBarrier attachmentLayoutBarrier = makeImageMemoryBarrier(
		VK_ACCESS_NONE_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		*m_colorImage, subresourceRange);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &attachmentLayoutBarrier);

	// render single triangle
	m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(renderSize), Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	m_graphicsPipeline.bind(*m_cmdBuffer);
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &*m_vertexBuffer, &vertexBufferOffset);
	vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

	m_renderPass.end(vk, *m_cmdBuffer);

	copyImageToBuffer(vk, *m_cmdBuffer, *m_colorImage, *m_resultBuffer, tcu::IVec2(renderSize.x(), renderSize.y()));

	endCommandBuffer(vk, *m_cmdBuffer);

	// submit commands
	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	// read buffer data
	invalidateAlloc(vk, device, *m_resultBufferAlloc);

	// validate result - verification is done in glsl, just checking
	// two texels, if test passed then r channel should be set to 255
	const unsigned char* bufferPtr = static_cast<unsigned char*>(m_resultBufferAlloc->getHostPtr());
	if ((bufferPtr[0] > 254) && (bufferPtr[renderSize.x()*4+8] > 254))
		return TestStatus::pass("Pass");

	const tcu::ConstPixelBufferAccess resultAccess(textureFormat, tcu::IVec3((int)renderSize.x(), (int)renderSize.y(), 1u), bufferPtr);
	TestLog& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::ImageSet("Result of rendering", "")
		<< TestLog::Image("Result", "", resultAccess)
		<< tcu::TestLog::EndImageSet;

	return TestStatus::fail("Fail");
}

class InterfaceMatchingTestCase : public vkt::TestCase
{
public:
					InterfaceMatchingTestCase	(tcu::TestContext&	testContext,
												 TestParamsSp		params);
	virtual			~InterfaceMatchingTestCase	(void) = default;

	void			initPrograms				(SourceCollections& sourceCollections) const override;
	void			checkSupport				(Context& context) const override;
	TestInstance*	createInstance				(Context& context) const override;

protected:

	enum class ComponentType
	{
		FLOAT = 0,
		INT,
		UINT
	};

	struct VecData
	{
		std::string		glslType;
		ComponentType	componentType;
		deUint32		componentsCount;
		std::string		components[4];
	};

	struct DecorationData
	{
		std::string		namePart;
		std::string		glslDecoration;
		std::string		glslComponent;
	};

	// helper structure used during construction of in/out declaration
	struct PipelineData
	{
		bool outDeclArray;
		bool inFlatDecoration;		// needed for frag in
		bool inDeclArray;
	};

	typedef std::map<std::string, std::string> SpecializationMap;

	std::string				genOutAssignment			(const std::string& variableName, const VecData& outVecData) const;
	std::string				genInVerification			(const std::string& variableName, const VecData& outVecData, const VecData& inVecData) const;

	const VecData&			getVecData					(VecType vecType) const;
	const DecorationData&	getDecorationData			(DecorationType decorationType) const;

	const PipelineData&		getPipelineData				(PipelineType pipelineType) const;
	std::string				generateName				(const TestParams& testParams) const;

private:

	const TestParamsSp m_params;
};

InterfaceMatchingTestCase::InterfaceMatchingTestCase(tcu::TestContext&	testContext,
													 TestParamsSp		params)
	: vkt::TestCase	(testContext, generateName(*params), "")
	, m_params		(params)
{
}

void InterfaceMatchingTestCase::initPrograms(SourceCollections& sourceCollections) const
{
	GlslSourceCollection&	glslSources				= sourceCollections.glslSources;
	const VecData&			outVecData				= getVecData(m_params->outVecType);
	const VecData&			inVecData				= getVecData(m_params->inVecType);
	const DecorationData&	outDecorationData		= getDecorationData(m_params->outDeclDecoration);
	const DecorationData&	inDecorationData		= getDecorationData(m_params->inDeclDecoration);
	const PipelineData&		pipelineData			= getPipelineData(m_params->pipelineType);

	// deterimine if decoration or array is needed for in/out declarations
	const std::string	outDeclArray			= pipelineData.outDeclArray ? "[]" : "";
	const std::string	inDeclArray				= pipelineData.inDeclArray  ? "[]" : "";
	const std::string	variableToAssignArray	= pipelineData.outDeclArray ? "[gl_InvocationID]" : "";
	const std::string	variableToVerifyArray	= pipelineData.inDeclArray  ? "[0]" : "";

	std::string		outDecoration	= "";
	std::string		inDecoration	= pipelineData.inFlatDecoration ? "flat " : "";
	std::string		outComponent	= outDecorationData.glslComponent;
	std::string		inComponent		= inDecorationData.glslComponent;
	if (m_params->testType == TestType::DECORATION_MISMATCH)
	{
		outDecoration	= outDecorationData.glslDecoration;
		inDecoration	= inDecorationData.glslDecoration;
	}

	std::string outDeclaration;
	std::string inDeclaration;
	std::string variableToAssignName;
	std::string variableToVerifyName;

	// generate in/out declarations
	switch (m_params->definitionType)
	{
	case DefinitionType::LOOSE_VARIABLE:
		outDeclaration			= "layout(location = 0" + outDecorationData.glslComponent + ") out " + outDecoration + outVecData.glslType + " looseVariable" + outDeclArray + ";\n";
		inDeclaration			= "layout(location = 0" +  inDecorationData.glslComponent + ") in "  +  inDecoration +  inVecData.glslType + " looseVariable" + inDeclArray  + ";\n";
		variableToAssignName	= "looseVariable" + variableToAssignArray;
		variableToVerifyName	= "looseVariable" + variableToVerifyArray;
		break;

	case DefinitionType::MEMBER_OF_BLOCK:
		outDeclaration		   += "layout(location = 0) out block {\n"
								  "  vec2 dummy;\n"
								  "layout(location = 1" + outDecorationData.glslComponent + ") " +
								  outDecoration + outVecData.glslType + " variableInBlock;\n"
								  "} testBlock" + outDeclArray + ";\n";
		inDeclaration		   += "in block {\n"
								  "layout(location = 0) vec2 dummy;\n"
								  "layout(location = 1" + inDecorationData.glslComponent + ") " +
								  inDecoration + inVecData.glslType + " variableInBlock;\n"
								  "} testBlock" + inDeclArray + ";\n";
		variableToAssignName	= "testBlock" + variableToAssignArray + ".variableInBlock";
		variableToVerifyName	= "testBlock" + variableToVerifyArray + ".variableInBlock";
		break;

	case DefinitionType::MEMBER_OF_STRUCTURE:
		outDeclaration		   += "layout(location = 0) out " + outDecoration + "struct {\n"
								  "  vec2 dummy;\n"
								  "  " + outVecData.glslType + " variableInStruct;\n"
								  "} testStruct" + outDeclArray + ";\n";
		inDeclaration		   += "layout(location = 0) in " + inDecoration + "struct {\n"
								  "  vec2 dummy;\n"
								  "  " + inVecData.glslType + " variableInStruct;\n"
								  "} testStruct" + inDeclArray + ";\n";
		variableToAssignName	= "testStruct" + variableToAssignArray + ".variableInStruct";
		variableToVerifyName	= "testStruct" + variableToVerifyArray + ".variableInStruct";
		break;

	case DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES:
		outDeclaration		   += "layout(location = 0) out " + outDecoration + "struct {\n"
								  "  float dummy;\n"
								  "  " + outVecData.glslType + " variableInStruct;\n"
								  "} testStructArray" + outDeclArray + "[3];\n";
		inDeclaration		   += "layout(location = 0) in " + inDecoration + "struct {\n"
								  "  float dummy;\n"
								  "  " + inVecData.glslType + " variableInStruct;\n"
								  "} testStructArray" + inDeclArray + "[3];\n";
		// just verify last item from array
		variableToAssignName	= "testStructArray" + variableToAssignArray + "[2].variableInStruct";
		variableToVerifyName	= "testStructArray" + variableToVerifyArray + "[2].variableInStruct";
		break;

	case DefinitionType::MEMBER_OF_STRUCTURE_IN_BLOCK:
		outDeclaration		   += "struct TestStruct {\n"
								  "  vec2 dummy;\n"
								  "  " + outVecData.glslType + " variableInStruct;\n"
								  "};\n"
								  "layout(location = 0) out block {\n"
								  "  vec2 dummy;\n"
								  "  " + outDecoration + "TestStruct structInBlock;\n"
								  "} testBlock" + outDeclArray + ";\n";
		inDeclaration		   += "struct TestStruct {\n"
								  "  vec2 dummy;\n"
								  "  " + inVecData.glslType + " variableInStruct;\n"
								  "};\n"
								  "layout(location = 0) in block {\n"
								  "  vec2 dummy;\n"
								  "  " + inDecoration + "TestStruct structInBlock;\n"
								  "} testBlock" + inDeclArray + ";\n";
		variableToAssignName	= "testBlock" + variableToAssignArray  + ".structInBlock.variableInStruct";
		variableToVerifyName	= "testBlock" + variableToVerifyArray  + ".structInBlock.variableInStruct";
		break;

	case DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES_IN_BLOCK:
		outDeclaration		   += "struct TestStruct {\n"
								  "  vec4 dummy;\n"
								  "  " + outVecData.glslType + " variableInStruct;\n"
								  "};\n"
								  "layout(location = 0) out block {\n"
								  "  " + outDecoration + "TestStruct structArrayInBlock[3];\n"
								  "} testBlock" + outDeclArray + ";\n";
		inDeclaration		   += "struct TestStruct {\n"
								  "  vec4 dummy;\n"
								  "  " + inVecData.glslType + " variableInStruct;\n"
								  "};"
								  "layout(location = 0) in block {\n"
								  "  " + inDecoration + "TestStruct structArrayInBlock[3];\n"
								  "} testBlock" + inDeclArray + ";\n";
		// just verify second item from array
		variableToAssignName	= "testBlock" + variableToAssignArray  + ".structArrayInBlock[1].variableInStruct";
		variableToVerifyName	= "testBlock" + variableToVerifyArray  + ".structArrayInBlock[1].variableInStruct";
		break;

	default:
		DE_ASSERT(DE_FALSE);
	}

	std::string outValueAssignment	= genOutAssignment (variableToAssignName, outVecData);
	std::string inValueVerification	= genInVerification(variableToVerifyName, outVecData, inVecData);

	// create specialization map and grab references to both
	// values so we dont have to index into it in every case
	SpecializationMap specializationMap
	{
		{ "DECLARATIONS",	"" },
		{ "OPERATIONS",		"" },
	};
	std::string& declarations	= specializationMap["DECLARATIONS"];
	std::string& operations		= specializationMap["OPERATIONS"];

	// define vertex shader source
	if (isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_FRAG_IN,
		PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,
		PipelineType::VERT_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG }))
	{
		declarations	= outDeclaration;
		operations		= outValueAssignment;
	}
	// else passthrough source

	tcu::StringTemplate vertTemplate(
		"#version 450\n"
		"layout(location = 0) in vec4 inPosition;\n"
		"${DECLARATIONS}"
		"void main(void)\n"
		"{\n"
		"  gl_Position = inPosition;\n"
		"${OPERATIONS}"
		"}\n");
	glslSources.add("vert") << glu::VertexSource(vertTemplate.specialize(specializationMap));

	// define tesselation control shader source
	bool tescNeeded = DE_FALSE;
	switch (m_params->pipelineType)
	{
	case PipelineType::VERT_TESC_OUT_TESE_IN_FRAG:
		declarations	= outDeclaration;
		operations		= outValueAssignment;
		tescNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_OUT_TESC_IN_TESE_FRAG:
	case PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG:
		declarations	= inDeclaration +
						  "layout(location = 0) out float outResult[];\n";
		operations		= "  float result;\n" +
						  inValueVerification +
						  "  outResult[gl_InvocationID] = result;\n";
		tescNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_TESC_TESE_OUT_FRAG_IN:
	case PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG:
	case PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN:
		// passthrough sources
		tescNeeded = DE_TRUE;
		break;

	default:
		break;
	}

	std::string tescSource = tescNeeded ?
		StringTemplate(
			"#version 450\n"
			"#extension GL_EXT_tessellation_shader : require\n\n"
			"layout(vertices = 1) out;\n\n"
			"${DECLARATIONS}"
			"void main(void)\n"
			"{\n"
			"  gl_TessLevelInner[0] = 1.0;\n"
			"  gl_TessLevelOuter[0] = 1.0;\n"
			"  gl_TessLevelOuter[1] = 1.0;\n"
			"  gl_TessLevelOuter[2] = 1.0;\n"
			"${OPERATIONS}"
			"}\n").specialize(specializationMap)
		: "";

	// define tesselation evaluation shader source
	bool teseNeeded = DE_FALSE;
	switch (m_params->pipelineType)
	{
	case PipelineType::VERT_TESC_TESE_OUT_FRAG_IN:
	case PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG:
		declarations	= outDeclaration;
		operations		= outValueAssignment;
		teseNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_TESC_OUT_TESE_IN_FRAG:
		declarations	= inDeclaration +
						  "layout(location = 0) out float outResult;\n";
		operations		= "  float result;\n" +
						  inValueVerification +
						  "  outResult = result;\n";
		teseNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_OUT_TESC_IN_TESE_FRAG:
	case PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG:
		declarations	= "layout(location = 0) in float inResult[];\n"
						  "layout(location = 0) out float outResult;\n";
		operations		= "  outResult = inResult[0];\n";
		teseNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN:
		// passthrough sources
		teseNeeded = DE_TRUE;
		break;

	default:
		break;
	}

	std::string teseSource = teseNeeded ?
		StringTemplate(
			"#version 450\n"
			"#extension GL_EXT_tessellation_shader : require\n\n"
			"layout(triangles) in;\n"
			"${DECLARATIONS}"
			"void main(void)\n"
			"{\n"
			"  gl_Position = vec4(gl_TessCoord.xy * 2.0 - 1.0, 0.0, 1.0);\n"
			"${OPERATIONS}"
			"}\n").specialize(specializationMap)
		: "";

	DE_ASSERT(tescSource.empty() == teseSource.empty());
	if (!tescSource.empty())
	{
		glslSources.add("tesc") << glu::TessellationControlSource(tescSource);
		glslSources.add("tese") << glu::TessellationEvaluationSource(teseSource);
	}

	// define geometry shader source
	bool geomNeeded = DE_FALSE;
	switch (m_params->pipelineType)
	{
	case PipelineType::VERT_GEOM_OUT_FRAG_IN:
	case PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN:
		declarations	= outDeclaration;
		operations		= outValueAssignment;
		geomNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_OUT_GEOM_IN_FRAG:
	case PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG:
		declarations	= inDeclaration +
						  "layout(location = 0) out float result;\n";
		operations		= inValueVerification;
		geomNeeded		= DE_TRUE;
		break;

	case PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG:
		declarations	= "layout(location = 0) in float inResult[];\n"
						  "layout(location = 0) out float outResult;\n";
		operations		= "  outResult = inResult[0];\n";
		geomNeeded		= DE_TRUE;
		break;

	default:
		break;
	}

	if (geomNeeded)
	{
		tcu::StringTemplate geomTemplate(
			"#version 450\n"
			"#extension GL_EXT_geometry_shader : require\n"
			"layout(triangles) in;\n"
			"layout(triangle_strip, max_vertices=3) out;\n"
			"${DECLARATIONS}"
			"void main(void)\n"
			"{\n"
			"${OPERATIONS}"
			"  gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);\n"
			"  EmitVertex();\n"
			"${OPERATIONS}"
			"  gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
			"  EmitVertex();\n"
			"${OPERATIONS}"
			"  gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n");
		glslSources.add("geom") << glu::GeometrySource(geomTemplate.specialize(specializationMap));
	}

	// define fragment shader source
	if (isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_FRAG_IN,
		PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,
		PipelineType::VERT_GEOM_OUT_FRAG_IN,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN }))
	{
		declarations	= inDeclaration;
		operations		= "  float result = 0.0;\n" +
						  inValueVerification;
	}
	else // passthrough source
	{
		declarations	= "layout(location = 0) in flat float result;\n";
		operations		= "";
	}

	tcu::StringTemplate fragTemplate(
		"#version 450\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"${DECLARATIONS}"
		"void main(void)\n"
		"{\n"
		"${OPERATIONS}"
		"  fragColor = vec4(result);\n"
		"}\n");
	glslSources.add("frag") << glu::FragmentSource(fragTemplate.specialize(specializationMap));
}

std::string InterfaceMatchingTestCase::genOutAssignment(const std::string& variableName, const VecData&	outVecData) const
{
	// generate value assignment to out variable;
	// for vec2/looseVariable this will generate:
	//   "looseVariable = vec2(-2.0, 3.0);"

	// define separators to avoid if statements in loop
	std::string					outSeparator(", ");
	std::string					endSeparator("");
	std::vector<std::string*>	outSeparators(4, &outSeparator);
	outSeparators[outVecData.componentsCount - 1] = &endSeparator;

	// generate value assignment
	std::string outValueAssignment = std::string("  ") + variableName + " = " + outVecData.glslType + "(";
	for (deUint32 i = 0; i < outVecData.componentsCount; ++i)
		outValueAssignment += outVecData.components[i] + *outSeparators[i];

	return outValueAssignment + ");\n";
}

std::string InterfaceMatchingTestCase::genInVerification(const std::string& variableName, const VecData& outVecData, const VecData& inVecData) const
{
	// generate value verification;
	// note that input has same or less components then output;
	// for vec2/looseVariable this will generate:
	//   "result = float(abs(looseVariable.x - -2.0) < eps) *"
	//            "float(abs(looseVariable.y - 3.0) < eps);\n"

	static const std::string componentNames[] = { "x", "y", "z", "w" };

	// define separators to avoid if statements in loop
	std::string		inSeparator		(" *\n\t\t   ");
	std::string		endSeparator	("");
	std::string*	inSeparators[]	{ &inSeparator, &inSeparator, &inSeparator, &endSeparator };

	inSeparators[inVecData.componentsCount - 1] = &endSeparator;

	std::string			inValueVerification("  result = ");
	tcu::StringTemplate verificationTemplate(
		inVecData.componentType == ComponentType::FLOAT ?
		"float(abs(" + variableName + ".${COMPONENT} - ${VALUE}) < 0.001)" :
		"float(" + variableName + ".${COMPONENT} == ${VALUE})");

	// verify each component using formula for float or int
	for (deUint32 i = 0; i < inVecData.componentsCount; ++i)
	{
		inValueVerification += verificationTemplate.specialize({
			{ "COMPONENT",	componentNames[i] },
			{ "VALUE",		outVecData.components[i] }
		});
		inValueVerification += *inSeparators[i];
	}

	return inValueVerification + ";\n";
}

void InterfaceMatchingTestCase::checkSupport(Context& context) const
{
	if (m_params->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params->pipelineConstructionType);

		// if graphicsPipelineLibraryIndependentInterpolationDecoration is VK_FALSE then interface mismatch
		// tests involving the Flat or NoPerspective qualifiers should be skipped for pipeline library tests
#ifndef CTS_USES_VULKANSC
		if (!context.getGraphicsPipelineLibraryPropertiesEXT().graphicsPipelineLibraryIndependentInterpolationDecoration)
		{
			if ((m_params->inDeclDecoration == DecorationType::FLAT) ||
				(m_params->inDeclDecoration == DecorationType::NO_PERSPECTIVE) ||
				(m_params->outDeclDecoration == DecorationType::FLAT) ||
				(m_params->outDeclDecoration == DecorationType::NO_PERSPECTIVE))
				TCU_THROW(NotSupportedError, "graphicsPipelineLibraryIndependentInterpolationDecoration is not supported");
		}
#endif // CTS_USES_VULKANSC
	}

	// when outputs from earlier stage are matched with smaller
	// inputs in future stage request VK_KHR_maintenance4
	if ((m_params->testType == TestType::VECTOR_LENGTH) &&
		(m_params->outVecType != m_params->inVecType))
	{
		context.requireDeviceFunctionality("VK_KHR_maintenance4");
	}

	const InstanceInterface&		vki				= context.getInstanceInterface();
	const VkPhysicalDevice			physicalDevice	= context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures	features		= getPhysicalDeviceFeatures(vki, physicalDevice);

	if (isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,
		PipelineType::VERT_TESC_OUT_TESE_IN_FRAG,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN }))
		if (!features.tessellationShader)
			TCU_THROW(NotSupportedError, "Tessellation shader not supported");

	if (isPipelineOneOf(m_params->pipelineType, {
		PipelineType::VERT_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_GEOM_OUT_FRAG_IN,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN }))
		if (!features.geometryShader)
			TCU_THROW(NotSupportedError, "Geometry shader not supported");
}

TestInstance* InterfaceMatchingTestCase::createInstance(Context& context) const
{
	return new InterfaceMatchingTestInstance(context, m_params);
}

const InterfaceMatchingTestCase::VecData& InterfaceMatchingTestCase::getVecData(VecType vecType) const
{
	static const std::map<VecType, VecData> vecDataMap
	{
		{ VecType::VEC2,	{ "vec2",  ComponentType::FLOAT, 2, { "-2.0",  "3.0", "",    ""    } } },
		{ VecType::VEC3,	{ "vec3",  ComponentType::FLOAT, 3, { "-3.0",  "2.0", "5.0", ""    } } },
		{ VecType::VEC4,	{ "vec4",  ComponentType::FLOAT, 4, { "-4.0", "-9.0", "3.0", "7.0" } } },
		{ VecType::IVEC2,	{ "ivec2", ComponentType::INT,   2, { "-4",    "8",   "",    ""    } } },
		{ VecType::IVEC3,	{ "ivec3", ComponentType::INT,   3, { "-5",    "10",  "15",  ""    } } },
		{ VecType::IVEC4,	{ "ivec4", ComponentType::INT,   4, { "-16",   "12",  "20",  "80"  } } },
		{ VecType::UVEC2,	{ "uvec2", ComponentType::UINT,  2, { "2",     "8",   "",    ""    } } },
		{ VecType::UVEC3,	{ "uvec3", ComponentType::UINT,  3, { "3",     "9",   "27",  ""    } } },
		{ VecType::UVEC4,	{ "uvec4", ComponentType::UINT,  4, { "4",     "16",  "64",  "256" } } },
	};

	DE_ASSERT(vecDataMap.find(vecType) != vecDataMap.end());
	return vecDataMap.at(vecType);
}

const InterfaceMatchingTestCase::DecorationData& InterfaceMatchingTestCase::getDecorationData(DecorationType decorationType) const
{
	static const std::map<DecorationType, DecorationData> decorationDataMap
	{
		{ DecorationType::NONE,				{ "none",			"",					""					} },
		{ DecorationType::FLAT,				{ "flat",			"flat ",			""					} },
		{ DecorationType::NO_PERSPECTIVE,	{ "noperspective",	"noperspective ",	""					} },
		{ DecorationType::COMPONENT0,		{ "component0",		"",					", component = 0 "	} },
	};

	DE_ASSERT(decorationDataMap.find(decorationType) != decorationDataMap.end());
	return decorationDataMap.at(decorationType);
}

const InterfaceMatchingTestCase::PipelineData& InterfaceMatchingTestCase::getPipelineData(PipelineType pipelineType) const
{
	// pipelineDataMap is used to simplify generation of declarations in glsl
	// it represent fallowing rules:
	// * for case where tesc outputs variable it must be declarred as an array
	// * when frag input variable is verified we need to use flat interpolation
	// * all stages except for frag need input to be array (note: we do not use input in vert)

	static const std::map<PipelineType, PipelineData> pipelineDataMap
	{
		//													  outArr inFlat inArr
		{ PipelineType::VERT_OUT_FRAG_IN,					{ 0,     1,     0 } },
		{ PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,			{ 0,     0,     1 } },
		{ PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,			{ 0,     1,     0 } },
		{ PipelineType::VERT_TESC_OUT_TESE_IN_FRAG,			{ 1,     0,     1 } },
		{ PipelineType::VERT_OUT_GEOM_IN_FRAG,				{ 0,     0,     1 } },
		{ PipelineType::VERT_GEOM_OUT_FRAG_IN,				{ 0,     1,     0 } },
		{ PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,	{ 0,     0,     1 } },
		{ PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,	{ 0,     0,     1 } },
		{ PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN,	{ 0,     1,     0 } },
	};

	DE_ASSERT(pipelineDataMap.find(pipelineType) != pipelineDataMap.end());
	return pipelineDataMap.at(pipelineType);
}

std::string InterfaceMatchingTestCase::generateName(const TestParams& testParams) const
{
	static const std::map<PipelineType, std::string> pipelineTypeMap
	{
		{ PipelineType::VERT_OUT_FRAG_IN,							"vert_out_frag_in" },
		{ PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,					"vert_out_tesc_in_tese_frag" },
		{ PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,					"vert_tesc_tese_out_frag_in" },
		{ PipelineType::VERT_TESC_OUT_TESE_IN_FRAG,					"vert_tesc_out_tese_in_frag" },
		{ PipelineType::VERT_OUT_GEOM_IN_FRAG,						"vert_out_geom_in_frag" },
		{ PipelineType::VERT_GEOM_OUT_FRAG_IN,						"vert_geom_out_frag_in" },
		{ PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,			"vert_out_tesc_in_tese_geom_frag" },
		{ PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,			"vert_tesc_tese_out_geom_in_frag" },
		{ PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN,			"vert_tesc_tese_geom_out_frag_in" },
	};

	static const std::map <DefinitionType, std::string> definitionTypeMap
	{
		{ DefinitionType::LOOSE_VARIABLE,							"loose_variable" },
		{ DefinitionType::MEMBER_OF_BLOCK,							"member_of_block" },
		{ DefinitionType::MEMBER_OF_STRUCTURE,						"member_of_structure" },
		{ DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES,			"member_of_array_of_structures" },
		{ DefinitionType::MEMBER_OF_STRUCTURE_IN_BLOCK,				"member_of_structure_in_block" },
		{ DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES_IN_BLOCK,	"member_of_array_of_structures_in_block" },
	};

	DE_ASSERT(pipelineTypeMap.find(testParams.pipelineType) != pipelineTypeMap.end());
	DE_ASSERT(definitionTypeMap.find(testParams.definitionType) != definitionTypeMap.end());

	std::string caseName;

	if (testParams.testType == TestType::VECTOR_LENGTH)
		caseName = "out_" + getVecData(testParams.outVecType).glslType +
				   "_in_" + getVecData(testParams.inVecType).glslType;
	else
		caseName = "out_" + getDecorationData(testParams.outDeclDecoration).namePart +
				   "_in_" + getDecorationData(testParams.inDeclDecoration).namePart;

	return caseName + "_" +
		   definitionTypeMap.at(testParams.definitionType) + "_" +
		   pipelineTypeMap.at(testParams.pipelineType);
};

} // anonymous

tcu::TestCaseGroup* createInterfaceMatchingTests(tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	VecType vecTypeList[3][3]
	{
		{ VecType::VEC4,	VecType::VEC3,		VecType::VEC2 },	// float
		{ VecType::IVEC4,	VecType::IVEC3,		VecType::IVEC2 },	// int
		{ VecType::UVEC4,	VecType::UVEC3,		VecType::UVEC2 },	// uint
	};

	PipelineType pipelineTypeList[]
	{
		PipelineType::VERT_OUT_FRAG_IN,
		PipelineType::VERT_OUT_TESC_IN_TESE_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_FRAG_IN,
		PipelineType::VERT_TESC_OUT_TESE_IN_FRAG,
		PipelineType::VERT_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_GEOM_OUT_FRAG_IN,
		PipelineType::VERT_OUT_TESC_IN_TESE_GEOM_FRAG,
		PipelineType::VERT_TESC_TESE_OUT_GEOM_IN_FRAG,
		PipelineType::VERT_TESC_TESE_GEOM_OUT_FRAG_IN,
	};

	DefinitionType definitionsTypeList[]
	{
		DefinitionType::LOOSE_VARIABLE,
		DefinitionType::MEMBER_OF_BLOCK,
		DefinitionType::MEMBER_OF_STRUCTURE,
		DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES,
		DefinitionType::MEMBER_OF_STRUCTURE_IN_BLOCK,
		DefinitionType::MEMBER_OF_ARRAY_OF_STRUCTURES_IN_BLOCK,
	};

	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "interface_matching", ""));

	de::MovePtr<tcu::TestCaseGroup> vectorMatching(new tcu::TestCaseGroup(testCtx, "vector_length", "Tests vector matching"));
	for (PipelineType pipelineType : pipelineTypeList)
		for (DefinitionType defType : definitionsTypeList)
		{
			// iterate over vector type - float, int or uint
			for (deUint32 vecDataFormat = 0; vecDataFormat < 3; ++vecDataFormat)
			{
				// iterate over all out/in lenght combinations
				const VecType* vecType = vecTypeList[vecDataFormat];
				for (deUint32 outVecSizeIndex = 0; outVecSizeIndex < 3; ++outVecSizeIndex)
				{
					VecType outVecType = vecType[outVecSizeIndex];
					for (deUint32 inVecSizeIndex = 0; inVecSizeIndex < 3; ++inVecSizeIndex)
					{
						VecType inVecType = vecType[inVecSizeIndex];
						if (outVecType < inVecType)
							continue;

						auto testParams = new TestParams
						{
							pipelineConstructionType,
							TestType::VECTOR_LENGTH,
							outVecType,
							inVecType,
							DecorationType::NONE,
							DecorationType::NONE,
							pipelineType,
							defType
						};

						vectorMatching->addChild(new InterfaceMatchingTestCase(testCtx, TestParamsSp(testParams)));
					}
				}
			}
		}
	testGroup->addChild(vectorMatching.release());

	std::vector<std::pair<DecorationType, DecorationType> > decorationPairs
	{
		{ DecorationType::NONE,				DecorationType::NO_PERSPECTIVE },
		{ DecorationType::NONE,				DecorationType::FLAT },
		{ DecorationType::FLAT,				DecorationType::NO_PERSPECTIVE },
		{ DecorationType::FLAT,				DecorationType::NONE },
		{ DecorationType::NO_PERSPECTIVE,	DecorationType::FLAT },
		{ DecorationType::NO_PERSPECTIVE,	DecorationType::NONE },
		{ DecorationType::COMPONENT0,		DecorationType::NONE },
		{ DecorationType::NONE,				DecorationType::COMPONENT0 },
	};

	de::MovePtr<tcu::TestCaseGroup> decorationMismatching(new tcu::TestCaseGroup(testCtx, "decoration_mismatch", "Decoration mismatch tests"));
	for (PipelineType stageType : pipelineTypeList)
		for (DefinitionType defType : definitionsTypeList)
			for (const auto& decoration : decorationPairs)
			{
				// tests component = 0 only for loose variables or member of block
				if (((decoration.first == DecorationType::COMPONENT0) ||
					 (decoration.second == DecorationType::COMPONENT0)) &&
					((defType != DefinitionType::LOOSE_VARIABLE) &&
					 (defType != DefinitionType::MEMBER_OF_BLOCK)))
					continue;

				auto testParams = new TestParams
				{
					pipelineConstructionType,
					TestType::DECORATION_MISMATCH,
					VecType::VEC4,
					VecType::VEC4,
					decoration.first,
					decoration.second,
					stageType,
					defType
				};
				decorationMismatching->addChild(new InterfaceMatchingTestCase(testCtx, TestParamsSp(testParams)));
			}

	testGroup->addChild(decorationMismatching.release());
	return testGroup.release();
}

} // pipeline
} // vkt
