/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
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
 * \brief Test frag shader side effects are not removed by optimizations.
 *//*--------------------------------------------------------------------*/

#include "vktRasterizationFragShaderSideEffectsTests.hpp"
#include "vktTestCase.hpp"

#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

namespace vkt
{
namespace rasterization
{

namespace
{

enum class CaseType
{
	KILL,
	DEMOTE,
	TERMINATE_INVOCATION,
	SAMPLE_MASK_BEFORE,
	SAMPLE_MASK_AFTER,
	ALPHA_COVERAGE_BEFORE,
	ALPHA_COVERAGE_AFTER,
	DEPTH_BOUNDS,
	STENCIL_NEVER,
	DEPTH_NEVER,
};

constexpr deUint32 kFramebufferWidth	= 32u;
constexpr deUint32 kFramebufferHeight	= 32u;
constexpr deUint32 kTotalPixels			= kFramebufferWidth * kFramebufferHeight;

constexpr vk::VkFormatFeatureFlags	kNeededColorFeatures	= (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
constexpr vk::VkFormat				kColorFormat			= vk::VK_FORMAT_R8G8B8A8_UNORM;
constexpr vk::VkFormatFeatureFlags	kNeededDSFeatures		= vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
// VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT must be supported for one of these two, according to the spec.
const vk::VkFormat					kDepthStencilFormats[]	= { vk::VK_FORMAT_D32_SFLOAT_S8_UINT, vk::VK_FORMAT_D24_UNORM_S8_UINT };

struct DepthBoundsParameters
{
	float minDepthBounds;
	float maxDepthBounds;
	float depthValue;
};

struct TestParams
{
	CaseType							caseType;
	tcu::Vec4							clearColor;
	tcu::Vec4							drawColor;
	bool								colorAtEnd;
	tcu::Maybe<DepthBoundsParameters>	depthBoundsParams;

	TestParams (CaseType type, const tcu::Vec4& clearColor_, const tcu::Vec4& drawColor_, bool colorAtEnd_, const tcu::Maybe<DepthBoundsParameters>& depthBoundsParams_)
		: caseType			(type)
		, clearColor		(clearColor_)
		, drawColor			(drawColor_)
		, colorAtEnd		(colorAtEnd_)
		, depthBoundsParams	(depthBoundsParams_)
	{
		if (caseType == CaseType::DEPTH_BOUNDS)
			DE_ASSERT(static_cast<bool>(depthBoundsParams));
	}
};

bool expectClearColor (CaseType caseType)
{
	return (caseType != CaseType::ALPHA_COVERAGE_BEFORE && caseType != CaseType::ALPHA_COVERAGE_AFTER);
}

bool needsDepthStencilAttachment (CaseType caseType)
{
	return (caseType == CaseType::DEPTH_BOUNDS || caseType == CaseType::DEPTH_NEVER || caseType == CaseType::STENCIL_NEVER);
}

vk::VkBool32 makeVkBool32 (bool value)
{
	return (value ? VK_TRUE : VK_FALSE);
}

class FragSideEffectsTestCase : public vkt::TestCase
{
public:
							FragSideEffectsTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~FragSideEffectsTestCase	(void) {}

	virtual void			checkSupport				(Context& context) const;
	virtual void			initPrograms				(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;

private:
	TestParams				m_params;
};

class FragSideEffectsInstance : public vkt::TestInstance
{
public:
								FragSideEffectsInstance		(Context& context, const TestParams& params);
	virtual						~FragSideEffectsInstance	(void) {}

	virtual tcu::TestStatus		iterate						(void);

private:
	TestParams					m_params;
};

FragSideEffectsTestCase::FragSideEffectsTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void FragSideEffectsTestCase::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	if (m_params.caseType == CaseType::DEPTH_BOUNDS)
	{
		const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);
		if (!features.depthBounds)
			TCU_THROW(NotSupportedError, "Depth bounds test not supported");
	}
	else if (m_params.caseType == CaseType::DEMOTE)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_demote_to_helper_invocation");
	}
	else if (m_params.caseType == CaseType::TERMINATE_INVOCATION)
	{
		context.requireDeviceFunctionality("VK_KHR_shader_terminate_invocation");
	}

	const auto colorFormatProperties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kColorFormat);
	if ((colorFormatProperties.optimalTilingFeatures & kNeededColorFeatures) != kNeededColorFeatures)
		TCU_THROW(NotSupportedError, "Color format lacks required features");
}

void FragSideEffectsTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream headers;
	std::ostringstream before;
	std::ostringstream after;

	std::ostringstream vert;
	std::ostringstream frag;

	// Depth should be 0 by default unless provided by the depth bounds parameters.
	const float	meshDepth	= (m_params.depthBoundsParams ? m_params.depthBoundsParams.get().depthValue : 0.0f);
	const auto&	drawColor	= m_params.drawColor;

	vert
		<< "#version 450\n"
		<< "\n"
		<< "layout (location=0) in vec2 inPos;\n"
		<< "\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4(inPos, " << meshDepth << ", 1.0);\n"
		<< "}\n"
		;

	// Prepare output color statement to be used before or after SSBO write.
	std::ostringstream colorStatement;
	if (m_params.caseType == CaseType::ALPHA_COVERAGE_BEFORE || m_params.caseType == CaseType::ALPHA_COVERAGE_AFTER)
	{
		// In the alpha coverage cases the alpha color value is supposed to be 0.
		DE_ASSERT(m_params.drawColor.w() == 0.0f);

		// Leave out the alpha component for these cases.
		colorStatement << "    outColor.rgb = vec3(" << drawColor.x() << ", " << drawColor.y() << ", " << drawColor.z() << ");\n";
	}
	else
	{
		colorStatement << "    outColor = vec4(" << drawColor.x() << ", " << drawColor.y() << ", " << drawColor.z() << ", " << drawColor.w() << ");\n";
	}

	switch (m_params.caseType)
	{
	case CaseType::KILL:
		after	<< "    discard;\n";
		break;
	case CaseType::DEMOTE:
		headers	<< "#extension GL_EXT_demote_to_helper_invocation : enable\n";
		after	<< "    demote;\n";
		break;
	case CaseType::TERMINATE_INVOCATION:
		headers	<< "#extension GL_EXT_terminate_invocation : enable\n";
		after	<< "    terminateInvocation;\n";
		break;
	case CaseType::SAMPLE_MASK_BEFORE:
		before	<< "    gl_SampleMask[0] = 0;\n";
		break;
	case CaseType::SAMPLE_MASK_AFTER:
		after	<< "    gl_SampleMask[0] = 0;\n";
		break;
	case CaseType::ALPHA_COVERAGE_BEFORE:
		before	<< "    outColor.a = float(" << drawColor.w() << ");\n";
		break;
	case CaseType::ALPHA_COVERAGE_AFTER:
		after	<< "    outColor.a = float(" << drawColor.w() << ");\n";
		break;
	case CaseType::DEPTH_BOUNDS:
	case CaseType::STENCIL_NEVER:
	case CaseType::DEPTH_NEVER:
		break;
	default:
		DE_ASSERT(false); break;
	}

	frag
		<< "#version 450\n"
		<< "layout(set=0, binding=0, std430) buffer OutputBuffer {\n"
		<< "    int val[" << kTotalPixels << "];\n"
		<< "} outBuffer;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< headers.str()
		<< "\n"
		<< "void main() {\n"
		<< "    const ivec2 fragCoord = ivec2(gl_FragCoord);\n"
		<< "    const int bufferIndex = (fragCoord.y * " << kFramebufferWidth << ") + fragCoord.x;\n"
		<< (m_params.colorAtEnd ? "" : colorStatement.str())
		<< before.str()
		<< "    outBuffer.val[bufferIndex] = 1;\n"
		<< after.str()
		<< (m_params.colorAtEnd ? colorStatement.str() : "")
		<< "}\n"
		;

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance* FragSideEffectsTestCase::createInstance (Context& context) const
{
	return new FragSideEffectsInstance(context, m_params);
}

FragSideEffectsInstance::FragSideEffectsInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

tcu::TestStatus FragSideEffectsInstance::iterate (void)
{
	const auto&	vki				= m_context.getInstanceInterface();
	const auto	physicalDevice	= m_context.getPhysicalDevice();
	const auto&	vkd				= m_context.getDeviceInterface();
	const auto	device			= m_context.getDevice();
	auto&		alloc			= m_context.getDefaultAllocator();
	const auto	queue			= m_context.getUniversalQueue();
	const auto	queueIndex		= m_context.getUniversalQueueFamilyIndex();

	// Color and depth/stencil images.

	const vk::VkImageCreateInfo colorCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											//	VkStructureType			sType;
		nullptr,																			//	const void*				pNext;
		0u,																					//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,																//	VkImageType				imageType;
		kColorFormat,																		//	VkFormat				format;
		vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u),						//	VkExtent3D				extent;
		1u,																					//	deUint32				mipLevels;
		1u,																					//	deUint32				arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,															//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,														//	VkImageTiling			tiling;
		(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT),	//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,														//	VkSharingMode			sharingMode;
		0u,																					//	deUint32				queueFamilyIndexCount;
		nullptr,																			//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,														//	VkImageLayout			initialLayout;
	};
	vk::ImageWithMemory colorImage(vkd, device, alloc, colorCreateInfo, vk::MemoryRequirement::Any);

	std::unique_ptr<vk::ImageWithMemory>	depthStencilImage;
	vk::VkFormat							depthStencilFormat = vk::VK_FORMAT_UNDEFINED;

	if (needsDepthStencilAttachment(m_params.caseType))
	{
		// Find available image format first.
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(kDepthStencilFormats); ++i)
		{
			const auto dsFormatProperties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kDepthStencilFormats[i]);
			if ((dsFormatProperties.optimalTilingFeatures & kNeededDSFeatures) == kNeededDSFeatures)
			{
				depthStencilFormat = kDepthStencilFormats[i];
				break;
			}
		}

		if (depthStencilFormat == vk::VK_FORMAT_UNDEFINED)
			TCU_FAIL("No suitable depth/stencil format found");

		const vk::VkImageCreateInfo depthStencilCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											//	VkStructureType			sType;
			nullptr,																			//	const void*				pNext;
			0u,																					//	VkImageCreateFlags		flags;
			vk::VK_IMAGE_TYPE_2D,																//	VkImageType				imageType;
			depthStencilFormat,																	//	VkFormat				format;
			vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u),						//	VkExtent3D				extent;
			1u,																					//	deUint32				mipLevels;
			1u,																					//	deUint32				arrayLayers;
			vk::VK_SAMPLE_COUNT_1_BIT,															//	VkSampleCountFlagBits	samples;
			vk::VK_IMAGE_TILING_OPTIMAL,														//	VkImageTiling			tiling;
			vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,									//	VkImageUsageFlags		usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,														//	VkSharingMode			sharingMode;
			0u,																					//	deUint32				queueFamilyIndexCount;
			nullptr,																			//	const deUint32*			pQueueFamilyIndices;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,														//	VkImageLayout			initialLayout;
		};

		depthStencilImage.reset(new vk::ImageWithMemory(vkd, device, alloc, depthStencilCreateInfo, vk::MemoryRequirement::Any));
	}

	// Image views.
	const auto colorSubresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorImageView			= vk::makeImageView(vkd, device, colorImage.get(), vk::VK_IMAGE_VIEW_TYPE_2D, kColorFormat, colorSubresourceRange);

	vk::Move<vk::VkImageView> depthStencilImageView;
	if (depthStencilImage)
	{
		const auto depthStencilSubresourceRange = vk::makeImageSubresourceRange((vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), 0u, 1u, 0u, 1u);
		depthStencilImageView = vk::makeImageView(vkd, device, depthStencilImage.get()->get(), vk::VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, depthStencilSubresourceRange);
	}

	// Color image buffer.
	const auto tcuFormat			= vk::mapVkFormat(kColorFormat);
	const auto colorImageBufferSize	= static_cast<vk::VkDeviceSize>(kTotalPixels * tcuFormat.getPixelSize());
	const auto colorImageBufferInfo	= vk::makeBufferCreateInfo(colorImageBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vk::BufferWithMemory colorImageBuffer(vkd, device, alloc, colorImageBufferInfo, vk::MemoryRequirement::HostVisible);

	// Vertex buffer.
	const std::vector<tcu::Vec2> fullScreenQuad =
	{
		tcu::Vec2(-1.0f,  1.0f),
		tcu::Vec2( 1.0f,  1.0f),
		tcu::Vec2( 1.0f, -1.0f),
		tcu::Vec2(-1.0f,  1.0f),
		tcu::Vec2( 1.0f, -1.0f),
		tcu::Vec2(-1.0f, -1.0f),
	};

	const auto				vertexBufferSize	= static_cast<vk::VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
	const auto				vertexBufferInfo	= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const vk::VkDeviceSize	vertexBufferOffset	= 0ull;
	vk::BufferWithMemory	vertexBuffer		(vkd, device, alloc, vertexBufferInfo, vk::MemoryRequirement::HostVisible);
	const auto&				vertexBufferAlloc	= vertexBuffer.getAllocation();

	deMemcpy(vertexBufferAlloc.getHostPtr(), fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
	vk::flushAlloc(vkd, device, vertexBufferAlloc);

	// Storage buffer.
	const auto				storageBufferSize	= static_cast<vk::VkDeviceSize>(kTotalPixels * sizeof(deInt32));
	const auto				storageBufferInfo	= vk::makeBufferCreateInfo(storageBufferSize, (vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
	vk::BufferWithMemory	storageBuffer		(vkd, device, alloc, storageBufferInfo, vk::MemoryRequirement::HostVisible);
	const auto&				storageBufferAlloc	= storageBuffer.getAllocation();

	deMemset(storageBufferAlloc.getHostPtr(), 0, static_cast<size_t>(storageBufferSize));
	vk::flushAlloc(vkd, device, storageBufferAlloc);

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = vk::makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// Descriptor pool.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set.
	vk::DescriptorSetUpdateBuilder	updateBuilder;
	const auto						descriptorBufferInfo = vk::makeDescriptorBufferInfo(storageBuffer.get(), 0u, storageBufferSize);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
	updateBuilder.update(vkd, device);

	// Render pass.
	const auto renderPass = vk::makeRenderPass(vkd, device, kColorFormat, depthStencilFormat);

	// Framebuffer.
	std::vector<vk::VkImageView> imageViews(1u, colorImageView.get());
	if (depthStencilImage)
		imageViews.push_back(depthStencilImageView.get());

	const auto framebuffer = vk::makeFramebuffer(vkd, device, renderPass.get(), static_cast<deUint32>(imageViews.size()), imageViews.data(), kFramebufferWidth, kFramebufferHeight);

	// Shader modules.
	const auto vertModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	// Vertex input state.
	const auto vertexBinding	= vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(sizeof(tcu::Vec2)), vk::VK_VERTEX_INPUT_RATE_VERTEX);
	const auto vertexAttributes	= vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u);

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttributes,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	// Input assembly state.
	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//	VkPrimitiveTopology						topology;
		VK_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	// Viewport state.
	const auto viewport	= vk::makeViewport(kFramebufferWidth, kFramebufferHeight);
	const auto scissor	= vk::makeRect2D(kFramebufferWidth, kFramebufferHeight);

	const vk::VkPipelineViewportStateCreateInfo viewportInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineViewportStateCreateFlags	flags;
		1u,															//	deUint32							viewportCount;
		&viewport,													//	const VkViewport*					pViewports;
		1u,															//	deUint32							scissorCount;
		&scissor,													//	const VkRect2D*						pScissors;
	};

	// Rasterization state.
	const vk::VkPipelineRasterizationStateCreateInfo rasterizationInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		vk::VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		vk::VK_CULL_MODE_NONE,											//	VkCullModeFlags							cullMode;
		vk::VK_FRONT_FACE_COUNTER_CLOCKWISE,							//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	// Multisample state.
	const bool										alphaToCoverageEnable	= (m_params.caseType == CaseType::ALPHA_COVERAGE_BEFORE || m_params.caseType == CaseType::ALPHA_COVERAGE_AFTER);
	const vk::VkPipelineMultisampleStateCreateInfo	multisampleInfo			=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		vk::VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														//	VkBool32								sampleShadingEnable;
		0.0f,															//	float									minSampleShading;
		nullptr,														//	const VkSampleMask*						pSampleMask;
		makeVkBool32(alphaToCoverageEnable),							//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,														//	VkBool32								alphaToOneEnable;
	};

	// Depth/stencil state.
	const auto enableDepthBounds		= makeVkBool32(m_params.caseType == CaseType::DEPTH_BOUNDS);
	const auto enableDepthStencilTest	= static_cast<bool>(depthStencilImage);

	const auto depthCompareOp			= ((m_params.caseType == CaseType::DEPTH_NEVER) ? vk::VK_COMPARE_OP_NEVER : vk::VK_COMPARE_OP_ALWAYS);
	const auto stencilCompareOp			= ((m_params.caseType == CaseType::STENCIL_NEVER) ? vk::VK_COMPARE_OP_NEVER : vk::VK_COMPARE_OP_ALWAYS);
	const auto stencilOpState			= vk::makeStencilOpState(vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, stencilCompareOp, 0xFFu, 0xFFu, 0u);

	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,					//	VkStructureType							sType;
		nullptr,																		//	const void*								pNext;
		0u,																				//	VkPipelineDepthStencilStateCreateFlags	flags;
		enableDepthStencilTest,															//	VkBool32								depthTestEnable;
		enableDepthStencilTest,															//	VkBool32								depthWriteEnable;
		depthCompareOp,																	//	VkCompareOp								depthCompareOp;
		enableDepthBounds,																//	VkBool32								depthBoundsTestEnable;
		enableDepthStencilTest,															//	VkBool32								stencilTestEnable;
		stencilOpState,																	//	VkStencilOpState						front;
		stencilOpState,																	//	VkStencilOpState						back;
		(enableDepthBounds ? m_params.depthBoundsParams.get().minDepthBounds : 0.0f),	//	float									minDepthBounds;
		(enableDepthBounds ? m_params.depthBoundsParams.get().maxDepthBounds : 1.0f),	//	float									maxDepthBounds;
	};

	// Color blend state.
	const vk::VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,						// VkBool32                 blendEnable
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor
		vk::VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor
		vk::VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp
		vk::VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags    colorWriteMask
		| vk::VK_COLOR_COMPONENT_G_BIT
		| vk::VK_COLOR_COMPONENT_B_BIT
		| vk::VK_COLOR_COMPONENT_A_BIT
	};

	const vk::VkPipelineColorBlendStateCreateInfo colorBlendInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,														//	VkBool32									logicOpEnable;
		vk::VK_LOGIC_OP_NO_OP,											//	VkLogicOp									logicOp;
		1u,																//	deUint32									attachmentCount;
		&colorBlendAttachmentState,										//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ .0f, .0f, .0f, .0f },											//	float										blendConstants[4];
	};

	// Graphics pipeline.
	const auto graphicsPipeline = vk::makeGraphicsPipeline(
		vkd, device, pipelineLayout.get(),
		vertModule.get(), DE_NULL, DE_NULL, DE_NULL, fragModule.get(),
		renderPass.get(), 0u,
		&vertexInputInfo,
		&inputAssemblyInfo,
		nullptr,
		&viewportInfo,
		&rasterizationInfo,
		&multisampleInfo,
		&depthStencilInfo,
		&colorBlendInfo);

	// Command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Draw full-screen quad.
	std::vector<vk::VkClearValue> clearValues;
	clearValues.push_back(vk::makeClearValueColor(m_params.clearColor));
	clearValues.push_back(vk::makeClearValueDepthStencil(1.0f, 0u));

	vk::beginCommandBuffer(vkd, cmdBuffer);
	vk::beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), vk::makeRect2D(kFramebufferWidth, kFramebufferHeight), static_cast<deUint32>(clearValues.size()), clearValues.data());
	vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(fullScreenQuad.size()), 1u, 0u, 0u);
	vk::endRenderPass(vkd, cmdBuffer);

	// Image and buffer barriers.

	// Storage buffer frag-write to host-read barrier.
	const auto storageBufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, storageBuffer.get(), 0u, VK_WHOLE_SIZE);

	// Color image frag-write to transfer-read barrier.
	const auto colorImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSubresourceRange);

	// Color buffer transfer-write to host-read barrier.
	const auto colorBufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, colorImageBuffer.get(), 0u, VK_WHOLE_SIZE);

	vk::cmdPipelineBufferMemoryBarrier(vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, &storageBufferBarrier);
	vk::cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, &colorImageBarrier);
	const auto copyRegion = vk::makeBufferImageCopy(vk::makeExtent3D(kFramebufferWidth, kFramebufferHeight, 1u), vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImageBuffer.get(), 1u, &copyRegion);
	vk::cmdPipelineBufferMemoryBarrier(vkd, cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, &colorBufferBarrier);

	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check output.
	{
		// Check SSBO contents.
		vk::invalidateAlloc(vkd, device, storageBufferAlloc);
		const auto bufferElements = reinterpret_cast<const deInt32*>(storageBufferAlloc.getHostPtr());

		for (deUint32 i = 0; i < kTotalPixels; ++i)
		{
			if (bufferElements[i] != 1)
			{
				std::ostringstream msg;
				msg << "Unexpected value in storage buffer element " << i;
				return tcu::TestStatus::fail("Fail: " + msg.str());
			}
		}
	}

	{
		// Check color attachment.
		std::vector<tcu::Vec4> expectedColors(1u, m_params.clearColor);
		if (!expectClearColor(m_params.caseType))
			expectedColors.push_back(m_params.drawColor);

		const auto& colorImageBufferAlloc = colorImageBuffer.getAllocation();
		vk::invalidateAlloc(vkd, device, colorImageBufferAlloc);

		const auto iWidth	= static_cast<int>(kFramebufferWidth);
		const auto iHeight	= static_cast<int>(kFramebufferHeight);

		tcu::ConstPixelBufferAccess colorPixels		(tcuFormat, iWidth, iHeight, 1, colorImageBufferAlloc.getHostPtr());
		std::vector<deUint8>		errorMaskBuffer	(kTotalPixels * tcuFormat.getPixelSize(), 0u);
		tcu::PixelBufferAccess		errorMask		(tcuFormat, iWidth, iHeight, 1, errorMaskBuffer.data());
		const tcu::Vec4				green			(0.0f, 1.0f, 0.0f, 1.0f);
		const tcu::Vec4				red				(1.0f, 0.0f, 0.0f, 1.0f);
		bool						allPixOk		= true;

		for (int i = 0; i < iWidth; ++i)
		for (int j = 0; j < iHeight; ++j)
		{
			const auto pixel = colorPixels.getPixel(i, j);
			const bool pixOk = std::any_of(begin(expectedColors), end(expectedColors), [&pixel](const tcu::Vec4& expected) -> bool { return (pixel == expected); });
			errorMask.setPixel((pixOk ? green : red), i, j);
			if (!pixOk)
				allPixOk = false;
		}

		if (!allPixOk)
		{
			auto& testLog = m_context.getTestContext().getLog();
			testLog << tcu::TestLog::Image("ColorBuffer", "Result color buffer", colorPixels);
			testLog << tcu::TestLog::Image("ErrorMask", "Error mask with errors marked in red", errorMask);
			return tcu::TestStatus::fail("Fail: color buffer with unexpected values; check logged images");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createFragSideEffectsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> fragSideEffectsGroup(new tcu::TestCaseGroup(testCtx, "frag_side_effects", "Test fragment shader side effects are not removed by optimizations"));

	const tcu::Vec4		kDefaultClearColor			(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4		kDefaultDrawColor			(0.0f, 0.0f, 1.0f, 1.0f);
	const auto			kDefaultDepthBoundsParams	= tcu::nothing<DepthBoundsParameters>();

	static const struct
	{
		bool		colorAtEnd;
		std::string	name;
		std::string	desc;
	} kColorOrders[] =
	{
		{ false,	"color_at_beginning",	"Fragment shader output assignment at the beginning of the shader"	},
		{ true,		"color_at_end",			"Fragment shader output assignment at the end of the shader"		},
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(kColorOrders); ++i)
	{
		de::MovePtr<tcu::TestCaseGroup> colorOrderGroup(new tcu::TestCaseGroup(testCtx, kColorOrders[i].name.c_str(), kColorOrders[i].desc.c_str()));
		const bool colorAtEnd = kColorOrders[i].colorAtEnd;

		{
			TestParams params(CaseType::KILL, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "kill", "OpKill after SSBO write", params));
		}
		{
			TestParams params(CaseType::DEMOTE, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "demote", "OpDemoteToHelperInvocation after SSBO write", params));
		}
		{
			TestParams params(CaseType::TERMINATE_INVOCATION, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "terminate_invocation", "OpTerminateInvocation after SSBO write", params));
		}
		{
			TestParams params(CaseType::SAMPLE_MASK_BEFORE, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "sample_mask_before", "Set sample mask to zero before SSBO write", params));
		}
		{
			TestParams params(CaseType::SAMPLE_MASK_AFTER, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "sample_mask_after", "Set sample mask to zero after SSBO write", params));
		}
		{
			TestParams params(CaseType::STENCIL_NEVER, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "stencil_never", "SSBO write with stencil test never passes", params));
		}
		{
			TestParams params(CaseType::DEPTH_NEVER, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, kDefaultDepthBoundsParams);
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "depth_never", "SSBO write with depth test never passes", params));
		}
		{
			const tcu::Vec4	drawColor(kDefaultDrawColor.x(), kDefaultDrawColor.y(), kDefaultDrawColor.z(), 0.0f);
			{
				TestParams params(CaseType::ALPHA_COVERAGE_BEFORE, kDefaultClearColor, drawColor, colorAtEnd, kDefaultDepthBoundsParams);
				colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "alpha_coverage_before", "Enable alpha coverage and draw with alpha zero before SSBO write", params));
			}
			{
				TestParams params(CaseType::ALPHA_COVERAGE_AFTER, kDefaultClearColor, drawColor, colorAtEnd, kDefaultDepthBoundsParams);
				colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "alpha_coverage_after", "Enable alpha coverage and draw with alpha zero after SSBO write", params));
			}
		}
		{
			DepthBoundsParameters depthBoundsParams = {0.25f, 0.5f, 0.75f}; // min, max, draw depth.
			TestParams params(CaseType::DEPTH_BOUNDS, kDefaultClearColor, kDefaultDrawColor, colorAtEnd, tcu::just(depthBoundsParams));
			colorOrderGroup->addChild(new FragSideEffectsTestCase(testCtx, "depth_bounds", "SSBO write with depth bounds test failing", params));
		}

		fragSideEffectsGroup->addChild(colorOrderGroup.release());
	}

	return fragSideEffectsGroup.release();
}

} // rasterization
} // vkt
