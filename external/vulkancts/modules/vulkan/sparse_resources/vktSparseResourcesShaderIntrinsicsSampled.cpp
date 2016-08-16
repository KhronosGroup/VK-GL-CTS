/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 *//*
 * \file  vktSparseResourcesShaderIntrinsicsSampled.cpp
 * \brief Sparse Resources Shader Intrinsics for sampled images
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesShaderIntrinsicsSampled.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

void SparseShaderIntrinsicsCaseSampledBase::initPrograms (vk::SourceCollections& programCollection) const
{
	const deUint32		numLayers	= getNumLayers(m_imageType, m_imageSize);
	const std::string	coordString = getShaderImageCoordinates(m_imageType, "%local_texCoord_x", "%local_texCoord_xy", "%local_texCoord_xyz");

	// Create vertex shader
	std::ostringstream vs;

	vs	<< "#version 440\n"
		<< "layout(location = 0) in  highp vec2 vs_in_position;\n"
		<< "layout(location = 1) in  highp vec2 vs_in_texCoord;\n"
		<< "\n"
		<< "layout(location = 0) out highp vec3 vs_out_texCoord;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position		= vec4(vs_in_position, 0.0f, 1.0f);\n"
		<< "	vs_out_texCoord = vec3(vs_in_texCoord, 0.0f);\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	if (numLayers > 1u)
	{
		const deInt32 maxVertices = 3u * numLayers;

		// Create geometry shader
		std::ostringstream gs;

		gs << "#version 440\n"
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = " << static_cast<deInt32>(maxVertices) << ") out;\n"
			<< "\n"
			<< "in gl_PerVertex {\n"
			<< "	vec4  gl_Position;\n"
			<< "} gl_in[];\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4  gl_Position;\n"
			<< "};\n"
			<< "layout(location = 0) in  highp vec3 gs_in_texCoord[];\n"
			<< "\n"
			<< "layout(location = 0) out highp vec3 gs_out_texCoord;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    for (int layerNdx = 0; layerNdx < " << static_cast<deInt32>(numLayers) << "; ++layerNdx)\n"
			<< "    {\n"
			<< "		for (int vertexNdx = 0; vertexNdx < gl_in.length(); ++vertexNdx)\n"
			<< "		{\n"
			<< "			gl_Layer		= layerNdx;\n"
			<< "			gl_Position		= gl_in[vertexNdx].gl_Position;\n"
			<< "			gs_out_texCoord = vec3(gs_in_texCoord[vertexNdx].xy, float(layerNdx));\n"
			<< "			EmitVertex();\n"
			<< "		}\n"
			<< "		EndPrimitive();\n"
			<< "    }\n"
			<< "}\n";

		programCollection.glslSources.add("geometry_shader") << glu::GeometrySource(gs.str());
	}

	// Create fragment shader
	std::ostringstream fs;

	fs	<< "OpCapability Shader\n"
		<< "OpCapability SampledCubeArray\n"
		<< "OpCapability ImageCubeArray\n"
		<< "OpCapability SparseResidency\n"
		<< "OpCapability StorageImageExtendedFormats\n"

		<< "%ext_import = OpExtInstImport \"GLSL.std.450\"\n"
		<< "OpMemoryModel Logical GLSL450\n"
		<< "OpEntryPoint Fragment %func_main \"main\" %varying_texCoord %output_texel %output_residency\n"
		<< "OpExecutionMode %func_main OriginUpperLeft\n"
		<< "OpSource GLSL 440\n"

		<< "OpName %func_main \"main\"\n"

		<< "OpName %varying_texCoord \"varying_texCoord\"\n"

		<< "OpName %output_texel \"out_texel\"\n"
		<< "OpName %output_residency \"out_residency\"\n"

		<< "OpName %type_uniformblock_lod \"LodBlock\"\n"
		<< "OpMemberName %type_uniformblock_lod 0 \"lod\"\n"
		<< "OpName %uniformblock_lod_instance \"lodInstance\"\n"

		<< "OpName %uniformconst_image_sparse \"u_imageSparse\"\n"

		<< "OpDecorate %varying_texCoord Location 0\n"

		<< "OpDecorate %output_texel	 Location 0\n"
		<< "OpDecorate %output_residency Location 1\n"

		<< "OpDecorate		 %type_uniformblock_lod Block\n"
		<< "OpMemberDecorate %type_uniformblock_lod 0 Offset 0\n"

		<< "OpDecorate %uniformconst_image_sparse DescriptorSet 0\n"
		<< "OpDecorate %uniformconst_image_sparse Binding " << BINDING_IMAGE_SPARSE << "\n"

		<< "%type_void = OpTypeVoid\n"
		<< "%type_void_func = OpTypeFunction %type_void\n"

		<< "%type_bool							= OpTypeBool\n"
		<< "%type_int							= OpTypeInt 32 1\n"
		<< "%type_uint							= OpTypeInt 32 0\n"
		<< "%type_float							= OpTypeFloat 32\n"
		<< "%type_vec2							= OpTypeVector %type_float 2\n"
		<< "%type_vec3							= OpTypeVector %type_float 3\n"
		<< "%type_vec4							= OpTypeVector %type_float 4\n"
		<< "%type_uniformblock_lod				= OpTypeStruct %type_uint\n"
		<< "%type_img_comp						= " << getOpTypeImageComponent(m_format) << "\n"
		<< "%type_img_comp_vec4					= OpTypeVector %type_img_comp 4\n"
		<< "%type_struct_int_img_comp_vec4		= OpTypeStruct %type_int %type_img_comp_vec4\n"

		<< "%type_input_vec3					= OpTypePointer Input %type_vec3\n"
		<< "%type_input_float					= OpTypePointer Input %type_float\n"

		<< "%type_output_img_comp_vec4			= OpTypePointer Output %type_img_comp_vec4\n"
		<< "%type_output_uint					= OpTypePointer Output %type_uint\n"

		<< "%type_function_int					= OpTypePointer Function %type_int\n"
		<< "%type_function_img_comp				= OpTypePointer Function %type_img_comp\n"
		<< "%type_function_img_comp_vec4		= OpTypePointer Function %type_img_comp_vec4\n"
		<< "%type_function_int_img_comp_vec4	= OpTypePointer Function %type_struct_int_img_comp_vec4\n"

		<< "%type_pushconstant_uniformblock_lod			= OpTypePointer PushConstant %type_uniformblock_lod\n"
		<< "%type_pushconstant_uniformblock_member_lod  = OpTypePointer PushConstant %type_uint\n"

		<< "%type_image_sparse				= " << getOpTypeImageSparse(m_imageType, m_format, "%type_img_comp", true) << "\n"
		<< "%type_sampled_image_sparse		= OpTypeSampledImage %type_image_sparse\n"
		<< "%type_uniformconst_image_sparse = OpTypePointer UniformConstant %type_sampled_image_sparse\n"

		<< "%varying_texCoord			= OpVariable %type_input_vec3 Input\n"

		<< "%output_texel				= OpVariable %type_output_img_comp_vec4 Output\n"
		<< "%output_residency			= OpVariable %type_output_uint Output\n"

		<< "%uniformconst_image_sparse	= OpVariable %type_uniformconst_image_sparse UniformConstant\n"

		<< "%uniformblock_lod_instance  = OpVariable %type_pushconstant_uniformblock_lod PushConstant\n"

		// Declare constants
		<< "%constant_uint_0				= OpConstant %type_uint 0\n"
		<< "%constant_uint_1				= OpConstant %type_uint 1\n"
		<< "%constant_uint_2				= OpConstant %type_uint 2\n"
		<< "%constant_uint_3				= OpConstant %type_uint 3\n"
		<< "%constant_int_0					= OpConstant %type_int  0\n"
		<< "%constant_int_1					= OpConstant %type_int  1\n"
		<< "%constant_int_2					= OpConstant %type_int  2\n"
		<< "%constant_int_3					= OpConstant %type_int  3\n"
		<< "%constant_texel_resident		= OpConstant %type_uint " << MEMORY_BLOCK_BOUND_VALUE << "\n"
		<< "%constant_texel_not_resident	= OpConstant %type_uint " << MEMORY_BLOCK_NOT_BOUND_VALUE << "\n"

		// Call main function
		<< "%func_main		 = OpFunction %type_void None %type_void_func\n"
		<< "%label_func_main = OpLabel\n"

		<< "%local_image_sparse = OpLoad %type_sampled_image_sparse %uniformconst_image_sparse\n"

		<< "%local_texCoord_x = OpCompositeExtract %type_float %varying_texCoord 0\n"
		<< "%local_texCoord_y = OpCompositeExtract %type_float %varying_texCoord 1\n"
		<< "%local_texCoord_z = OpCompositeExtract %type_float %varying_texCoord 2\n"

		<< "%local_texCoord_xy	= OpCompositeConstruct %type_vec2 %local_texCoord_x %local_texCoord_y\n"
		<< "%local_texCoord_xyz = OpCompositeConstruct %type_vec3 %local_texCoord_x %local_texCoord_y %local_texCoord_z\n"

		<< "%access_uniformblock_member_uint_lod = OpAccessChain %type_pushconstant_uniformblock_member_lod %uniformblock_lod_instance %constant_int_0\n"
		<< "%local_uniformblock_member_uint_lod  = OpLoad %type_uint %access_uniformblock_member_uint_lod\n"
		<< "%local_uniformblock_member_float_lod = OpConvertUToF %type_float %local_uniformblock_member_uint_lod\n"

		<< sparseImageOpString("%local_sparse_op_result", "%type_struct_int_img_comp_vec4", "%local_image_sparse", coordString, "%local_uniformblock_member_float_lod") << "\n"

		// Load texel value
		<< "%local_img_comp_vec4 = OpCompositeExtract %type_img_comp_vec4 %local_sparse_op_result 1\n"

		<< "OpStore %output_texel %local_img_comp_vec4\n"

		// Load residency code
		<< "%local_residency_code = OpCompositeExtract %type_int %local_sparse_op_result 0\n"

		// Check if loaded texel is placed in resident memory
		<< "%local_texel_resident = OpImageSparseTexelsResident %type_bool %local_residency_code\n"
		<< "OpSelectionMerge %branch_texel_resident None\n"
		<< "OpBranchConditional %local_texel_resident %label_texel_resident %label_texel_not_resident\n"
		<< "%label_texel_resident = OpLabel\n"

		// Loaded texel is in resident memory
		<< "OpStore %output_residency %constant_texel_resident\n"

		<< "OpBranch %branch_texel_resident\n"
		<< "%label_texel_not_resident = OpLabel\n"

		// Loaded texel is not in resident memory
		<< "OpStore %output_residency %constant_texel_not_resident\n"

		<< "OpBranch %branch_texel_resident\n"
		<< "%branch_texel_resident = OpLabel\n"

		<< "OpReturn\n"
		<< "OpFunctionEnd\n";

	programCollection.spirvAsmSources.add("fragment_shader") << fs.str();
}

std::string	SparseCaseOpImageSparseSampleExplicitLod::sparseImageOpString (const std::string& resultVariable,
																		   const std::string& resultType,
																		   const std::string& image,
																		   const std::string& coord,
																		   const std::string& miplevel) const
{
	std::ostringstream	src;

	src << resultVariable << " = OpImageSparseSampleExplicitLod " << resultType << " " << image << " " << coord << " Lod " << miplevel << "\n";

	return src.str();
}

std::string	SparseCaseOpImageSparseSampleImplicitLod::sparseImageOpString  (const std::string& resultVariable,
																			const std::string& resultType,
																			const std::string& image,
																			const std::string& coord,
																			const std::string& miplevel) const
{
	DE_UNREF(miplevel);

	std::ostringstream	src;

	src << resultVariable << " = OpImageSparseSampleImplicitLod " << resultType << " " << image << " " << coord << "\n";

	return src.str();
}

std::string	SparseCaseOpImageSparseGather::sparseImageOpString (const std::string& resultVariable,
																const std::string& resultType,
																const std::string& image,
																const std::string& coord,
																const std::string& miplevel) const
{
	DE_UNREF(miplevel);

	std::ostringstream	src;

	src << "%local_sparse_gather_result_x = OpImageSparseGather " << resultType << " " << image << " " << coord << " %constant_int_0\n";
	src << "%local_sparse_gather_result_y = OpImageSparseGather " << resultType << " " << image << " " << coord << " %constant_int_1\n";
	src << "%local_sparse_gather_result_z = OpImageSparseGather " << resultType << " " << image << " " << coord << " %constant_int_2\n";
	src << "%local_sparse_gather_result_w = OpImageSparseGather " << resultType << " " << image << " " << coord << " %constant_int_3\n";

	src << "%local_gather_residency_code = OpCompositeExtract %type_int %local_sparse_gather_result_x 0\n";

	src << "%local_gather_texels_x = OpCompositeExtract %type_img_comp_vec4 %local_sparse_gather_result_x 1\n";
	src << "%local_gather_texels_y = OpCompositeExtract %type_img_comp_vec4 %local_sparse_gather_result_y 1\n";
	src << "%local_gather_texels_z = OpCompositeExtract %type_img_comp_vec4 %local_sparse_gather_result_z 1\n";
	src << "%local_gather_texels_w = OpCompositeExtract %type_img_comp_vec4 %local_sparse_gather_result_w 1\n";

	src << "%local_gather_primary_texel_x = OpCompositeExtract %type_img_comp %local_gather_texels_x 3\n";
	src << "%local_gather_primary_texel_y = OpCompositeExtract %type_img_comp %local_gather_texels_y 3\n";
	src << "%local_gather_primary_texel_z = OpCompositeExtract %type_img_comp %local_gather_texels_z 3\n";
	src << "%local_gather_primary_texel_w = OpCompositeExtract %type_img_comp %local_gather_texels_w 3\n";

	src << "%local_gather_primary_texel	= OpCompositeConstruct %type_img_comp_vec4 %local_gather_primary_texel_x %local_gather_primary_texel_y %local_gather_primary_texel_z %local_gather_primary_texel_w\n";
	src << resultVariable << " = OpCompositeConstruct " << resultType << " %local_gather_residency_code %local_gather_primary_texel\n";

	return src.str();
}

class SparseShaderIntrinsicsInstanceSampledBase : public SparseShaderIntrinsicsInstanceBase
{
public:
	SparseShaderIntrinsicsInstanceSampledBase	(Context&					context,
												 const SpirVFunction		function,
												 const ImageType			imageType,
												 const tcu::UVec3&			imageSize,
												 const tcu::TextureFormat&	format)
	: SparseShaderIntrinsicsInstanceBase(context, function, imageType, imageSize, format) {}

	VkImageUsageFlags		imageSparseUsageFlags	(void) const;
	VkImageUsageFlags		imageOutputUsageFlags	(void) const;

	VkQueueFlags			getQueueFlags			(void) const;

	void					recordCommands			(vk::Allocator&				allocator,
													 const VkCommandBuffer		commandBuffer,
													 const VkImageCreateInfo&	imageSparseInfo,
													 const VkImage				imageSparse,
													 const VkImage				imageTexels,
													 const VkImage				imageResidency);

	virtual VkImageSubresourceRange	sampledImageRangeToBind(const VkImageCreateInfo& imageSparseInfo, const deUint32 mipLevel) const = 0;

private:

	typedef de::SharedPtr< vk::Unique<vk::VkFramebuffer> > SharedVkFramebuffer;

	de::SharedPtr<Buffer>				vertexBuffer;
	std::vector<SharedVkFramebuffer>	framebuffers;
	Move<VkRenderPass>					renderPass;
	Move<VkSampler>						sampler;
};

VkImageUsageFlags SparseShaderIntrinsicsInstanceSampledBase::imageSparseUsageFlags (void) const
{
	return VK_IMAGE_USAGE_SAMPLED_BIT;
}

VkImageUsageFlags SparseShaderIntrinsicsInstanceSampledBase::imageOutputUsageFlags (void) const
{
	return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
}

VkQueueFlags SparseShaderIntrinsicsInstanceSampledBase::getQueueFlags (void) const
{
	return VK_QUEUE_GRAPHICS_BIT;
}

void SparseShaderIntrinsicsInstanceSampledBase::recordCommands (vk::Allocator&				allocator,
																const VkCommandBuffer		commandBuffer,
																const VkImageCreateInfo&	imageSparseInfo,
																const VkImage				imageSparse,
																const VkImage				imageTexels,
																const VkImage				imageResidency)
{
	const InstanceInterface&		 instance			= m_context.getInstanceInterface();
	const DeviceInterface&			 deviceInterface	= m_context.getDeviceInterface();
	const VkPhysicalDevice			 physicalDevice		= m_context.getPhysicalDevice();
	const VkPhysicalDeviceProperties deviceProperties	= getPhysicalDeviceProperties(instance, physicalDevice);

	if (imageSparseInfo.extent.width  > deviceProperties.limits.maxFramebufferWidth  ||
		imageSparseInfo.extent.height > deviceProperties.limits.maxFramebufferHeight ||
		imageSparseInfo.arrayLayers   > deviceProperties.limits.maxFramebufferLayers)
	{
		TCU_THROW(NotSupportedError, "Image size exceeds allowed framebuffer dimensions");
	}

	// Check if device supports image format for sampled images
	if (!checkImageFormatFeatureSupport(instance, physicalDevice, imageSparseInfo.format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
		TCU_THROW(NotSupportedError, "Device does not support image format for sampled images");

	// Check if device supports image format for color attachment
	if (!checkImageFormatFeatureSupport(instance, physicalDevice, imageSparseInfo.format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		TCU_THROW(NotSupportedError, "Device does not support image format for color attachment");

	// Make sure device supports VK_FORMAT_R32_UINT format for color attachment
	if (!checkImageFormatFeatureSupport(instance, physicalDevice, mapTextureFormat(m_residencyFormat), VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		TCU_THROW(TestError, "Device does not support VK_FORMAT_R32_UINT format for color attachment");

	// Create buffer storing vertex data
	std::vector<tcu::Vec2> vertexData;

	vertexData.push_back(tcu::Vec2(-1.0f,-1.0f));
	vertexData.push_back(tcu::Vec2( 0.0f, 0.0f));

	vertexData.push_back(tcu::Vec2(-1.0f, 1.0f));
	vertexData.push_back(tcu::Vec2( 0.0f, 1.0f));

	vertexData.push_back(tcu::Vec2( 1.0f,-1.0f));
	vertexData.push_back(tcu::Vec2( 1.0f, 0.0f));

	vertexData.push_back(tcu::Vec2( 1.0f, 1.0f));
	vertexData.push_back(tcu::Vec2( 1.0f, 1.0f));

	const VkFormat		vertexFormatPosition		= VK_FORMAT_R32G32_SFLOAT;
	const VkFormat		vertexFormatTexCoord		= VK_FORMAT_R32G32_SFLOAT;

	const deUint32		vertexSizePosition			= tcu::getPixelSize(mapVkFormat(vertexFormatPosition));
	const deUint32		vertexSizeTexCoord			= tcu::getPixelSize(mapVkFormat(vertexFormatTexCoord));

	const VkDeviceSize	vertexBufferStartOffset		= 0ull;
	const deUint32		vertexBufferOffsetPosition	= 0ull;
	const deUint32		vertexBufferOffsetTexCoord	= vertexSizePosition;

	const deUint32		vertexDataStride			= vertexSizePosition + vertexSizeTexCoord;
	const VkDeviceSize	vertexDataSizeInBytes		= sizeInBytes(vertexData);

	vertexBuffer = de::SharedPtr<Buffer>(new Buffer(deviceInterface, *m_logicalDevice, allocator, makeBufferCreateInfo(vertexDataSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation& vertexBufferAllocation = vertexBuffer->getAllocation();

	deMemcpy(vertexBufferAllocation.getHostPtr(), &vertexData[0], static_cast<std::size_t>(vertexDataSizeInBytes));
	flushMappedMemoryRange(deviceInterface, *m_logicalDevice, vertexBufferAllocation.getMemory(), vertexBufferAllocation.getOffset(), vertexDataSizeInBytes);

	// Create render pass
	const VkAttachmentDescription texelsAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
		imageSparseInfo.format,								// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription residencyAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
		mapTextureFormat(m_residencyFormat),				// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription colorAttachmentsDescription[] = { texelsAttachmentDescription, residencyAttachmentDescription };

	const VkAttachmentReference texelsAttachmentReference =
	{
		0u,													// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
	};

	const VkAttachmentReference residencyAttachmentReference =
	{
		1u,													// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
	};

	const VkAttachmentReference colorAttachmentsReference[] = { texelsAttachmentReference, residencyAttachmentReference };

	const VkAttachmentReference depthAttachmentReference =
	{
		VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
	};

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		2u,													// deUint32							colorAttachmentCount;
		colorAttachmentsReference,							// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		&depthAttachmentReference,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
		2u,													// deUint32							attachmentCount;
		colorAttachmentsDescription,						// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	renderPass = createRenderPass(deviceInterface, *m_logicalDevice, &renderPassInfo);

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descriptorLayerBuilder;

	descriptorLayerBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(descriptorLayerBuilder.build(deviceInterface, *m_logicalDevice));

	// Create descriptor pool
	DescriptorPoolBuilder descriptorPoolBuilder;

	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSparseInfo.mipLevels);

	descriptorPool = descriptorPoolBuilder.build(deviceInterface, *m_logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, imageSparseInfo.mipLevels);

	// Create sampler object
	const tcu::Sampler			samplerObject(tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL, tcu::Sampler::NEAREST_MIPMAP_NEAREST, tcu::Sampler::NEAREST);
	const VkSamplerCreateInfo	samplerCreateInfo = mapSampler(samplerObject, m_format);
	sampler = createSampler(deviceInterface, *m_logicalDevice, &samplerCreateInfo);

	// Create pipeline layout
	const VkPushConstantRange lodConstantRange =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
		0u,								// deUint32			offset;
		sizeof(deUint32),				// deUint32			size;
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout.get(),							// const VkDescriptorSetLayout*		pSetLayouts;
		1u,													// deUint32							pushConstantRangeCount;
		&lodConstantRange,									// const VkPushConstantRange*		pPushConstantRanges;
	};

	const Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(deviceInterface, *m_logicalDevice, &pipelineLayoutParams));

	// Create graphics pipeline
	const VkVertexInputBindingDescription vertexBinding =
	{
		0u,							// deUint32				binding;
		vertexDataStride,			// deUint32				stride;
		VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexAttributePosition =
	{
		0u,							// deUint32	location;
		0u,							// deUint32	binding;
		vertexFormatPosition,		// VkFormat	format;
		vertexBufferOffsetPosition,	// deUint32	offset;
	};

	const VkVertexInputAttributeDescription vertexAttributeTexCoord =
	{
		1u,							// deUint32	location;
		0u,							// deUint32	binding;
		vertexFormatTexCoord,		// VkFormat	format;
		vertexBufferOffsetTexCoord,	// deUint32	offset;
	};

	{
		GraphicsPipelineBuilder graphicPipelineBuilder;

		graphicPipelineBuilder.addVertexBinding(vertexBinding);
		graphicPipelineBuilder.addVertexAttribute(vertexAttributePosition);
		graphicPipelineBuilder.addVertexAttribute(vertexAttributeTexCoord);
		graphicPipelineBuilder.setPrimitiveTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		graphicPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
		graphicPipelineBuilder.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
		graphicPipelineBuilder.setAttachmentsCount(2u);
		graphicPipelineBuilder.setShader(deviceInterface, *m_logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get("vertex_shader"), DE_NULL);
		graphicPipelineBuilder.setShader(deviceInterface, *m_logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("fragment_shader"), DE_NULL);

		if (imageSparseInfo.arrayLayers > 1u)
		{
			requireFeatures(instance, physicalDevice, FEATURE_GEOMETRY_SHADER);
			graphicPipelineBuilder.setShader(deviceInterface, *m_logicalDevice, VK_SHADER_STAGE_GEOMETRY_BIT, m_context.getBinaryCollection().get("geometry_shader"), DE_NULL);
		}

		pipelines.push_back(makeVkSharedPtr(graphicPipelineBuilder.build(deviceInterface, *m_logicalDevice, *pipelineLayout, *renderPass)));
	}

	const VkPipeline graphicsPipeline = **pipelines[0];

	{
		const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

		VkImageMemoryBarrier imageShaderAccessBarriers[3];

		imageShaderAccessBarriers[0] = makeImageMemoryBarrier
		(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			imageSparse,
			fullImageSubresourceRange
		);

		imageShaderAccessBarriers[1] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			imageTexels,
			fullImageSubresourceRange
		);

		imageShaderAccessBarriers[2] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			imageResidency,
			fullImageSubresourceRange
		);

		deviceInterface.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 3u, imageShaderAccessBarriers);
	}

	imageSparseViews.resize(imageSparseInfo.mipLevels);
	imageTexelsViews.resize(imageSparseInfo.mipLevels);
	imageResidencyViews.resize(imageSparseInfo.mipLevels);
	framebuffers.resize(imageSparseInfo.mipLevels);
	descriptorSets.resize(imageSparseInfo.mipLevels);

	std::vector<VkClearValue> clearValues;
	clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
	clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));

	for (deUint32 mipLevelNdx = 0u; mipLevelNdx < imageSparseInfo.mipLevels; ++mipLevelNdx)
	{
		const vk::VkExtent3D mipLevelSize = mipLevelExtents(imageSparseInfo.extent, mipLevelNdx);

		const vk::VkRect2D renderArea =
		{
			makeOffset2D(0u, 0u),
			makeExtent2D(mipLevelSize.width, mipLevelSize.height),
		};

		const VkViewport viewport = makeViewport
		(
			0.0f, 0.0f,
			static_cast<float>(mipLevelSize.width), static_cast<float>(mipLevelSize.height),
			0.0f, 1.0f
		);

		const VkImageSubresourceRange mipLevelRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelNdx, 1u, 0u, imageSparseInfo.arrayLayers);

		// Create color attachments image views
		imageTexelsViews[mipLevelNdx] = makeVkSharedPtr(makeImageView(deviceInterface, *m_logicalDevice, imageTexels, mapImageViewType(m_imageType), imageSparseInfo.format, mipLevelRange));
		imageResidencyViews[mipLevelNdx] = makeVkSharedPtr(makeImageView(deviceInterface, *m_logicalDevice, imageResidency, mapImageViewType(m_imageType), mapTextureFormat(m_residencyFormat), mipLevelRange));

		const VkImageView attachmentsViews[] = { **imageTexelsViews[mipLevelNdx], **imageResidencyViews[mipLevelNdx] };

		// Create framebuffer
		const VkFramebufferCreateInfo framebufferInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType                             sType;
			DE_NULL,									// const void*                                 pNext;
			(VkFramebufferCreateFlags)0,				// VkFramebufferCreateFlags                    flags;
			*renderPass,								// VkRenderPass                                renderPass;
			2u,											// uint32_t                                    attachmentCount;
			attachmentsViews,							// const VkImageView*                          pAttachments;
			mipLevelSize.width,							// uint32_t                                    width;
			mipLevelSize.height,						// uint32_t                                    height;
			imageSparseInfo.arrayLayers,				// uint32_t                                    layers;
		};

		framebuffers[mipLevelNdx] = makeVkSharedPtr(createFramebuffer(deviceInterface, *m_logicalDevice, &framebufferInfo));

		// Create descriptor set
		descriptorSets[mipLevelNdx] = makeVkSharedPtr(makeDescriptorSet(deviceInterface, *m_logicalDevice, *descriptorPool, *descriptorSetLayout));
		const VkDescriptorSet descriptorSet = **descriptorSets[mipLevelNdx];

		// Update descriptor set
		const VkImageSubresourceRange sparseImageSubresourceRange = sampledImageRangeToBind(imageSparseInfo, mipLevelNdx);

		imageSparseViews[mipLevelNdx] = makeVkSharedPtr(makeImageView(deviceInterface, *m_logicalDevice, imageSparse, mapImageViewType(m_imageType), imageSparseInfo.format, sparseImageSubresourceRange));

		const VkDescriptorImageInfo imageSparseDescInfo = makeDescriptorImageInfo(*sampler, **imageSparseViews[mipLevelNdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		DescriptorSetUpdateBuilder descriptorUpdateBuilder;

		descriptorUpdateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(BINDING_IMAGE_SPARSE), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSparseDescInfo);
		descriptorUpdateBuilder.update(deviceInterface, *m_logicalDevice);

		// Begin render pass
		beginRenderPass(deviceInterface, commandBuffer, *renderPass, **framebuffers[mipLevelNdx], renderArea, clearValues);

		// Bind graphics pipeline
		deviceInterface.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind descriptor set
		deviceInterface.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

		// Bind vertex buffer
		deviceInterface.cmdBindVertexBuffers(commandBuffer, 0u, 1u, &vertexBuffer->get(), &vertexBufferStartOffset);

		// Bind Viewport
		deviceInterface.cmdSetViewport(commandBuffer, 0u, 1u, &viewport);

		// Bind Scissor Rectangle
		deviceInterface.cmdSetScissor(commandBuffer, 0u, 1u, &renderArea);

		// Update push constants
		deviceInterface.cmdPushConstants(commandBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(deUint32), &mipLevelNdx);

		// Draw full screen quad
		deviceInterface.cmdDraw(commandBuffer, 4u, 1u, 0u, 0u);

		// End render pass
		endRenderPass(deviceInterface, commandBuffer);
	}

	{
		const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

		VkImageMemoryBarrier imageOutputTransferSrcBarriers[2];

		imageOutputTransferSrcBarriers[0] = makeImageMemoryBarrier
		(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			imageTexels,
			fullImageSubresourceRange
		);

		imageOutputTransferSrcBarriers[1] = makeImageMemoryBarrier
		(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			imageResidency,
			fullImageSubresourceRange
		);

		deviceInterface.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u, imageOutputTransferSrcBarriers);
	}
}

class SparseShaderIntrinsicsInstanceSampledExplicit : public SparseShaderIntrinsicsInstanceSampledBase
{
public:
	SparseShaderIntrinsicsInstanceSampledExplicit	(Context&					context,
													 const SpirVFunction		function,
													 const ImageType			imageType,
													 const tcu::UVec3&			imageSize,
													 const tcu::TextureFormat&	format)
	: SparseShaderIntrinsicsInstanceSampledBase(context, function, imageType, imageSize, format) {}

	VkImageSubresourceRange	sampledImageRangeToBind(const VkImageCreateInfo& imageSparseInfo, const deUint32 mipLevel) const;
};

VkImageSubresourceRange SparseShaderIntrinsicsInstanceSampledExplicit::sampledImageRangeToBind (const VkImageCreateInfo& imageSparseInfo, const deUint32 mipLevel) const
{
	DE_UNREF(mipLevel);

	return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);
}

TestInstance* SparseShaderIntrinsicsCaseSampledExplicit::createInstance (Context& context) const
{
	return new SparseShaderIntrinsicsInstanceSampledExplicit(context, m_function, m_imageType, m_imageSize, m_format);
}

class SparseShaderIntrinsicsInstanceSampledImplicit : public SparseShaderIntrinsicsInstanceSampledBase
{
public:
	SparseShaderIntrinsicsInstanceSampledImplicit	(Context&					context,
													 const SpirVFunction		function,
													 const ImageType			imageType,
													 const tcu::UVec3&			imageSize,
													 const tcu::TextureFormat&	format)
	: SparseShaderIntrinsicsInstanceSampledBase(context, function, imageType, imageSize, format) {}

	VkImageSubresourceRange	sampledImageRangeToBind(const VkImageCreateInfo& imageSparseInfo, const deUint32 mipLevel) const;
};

VkImageSubresourceRange SparseShaderIntrinsicsInstanceSampledImplicit::sampledImageRangeToBind (const VkImageCreateInfo& imageSparseInfo, const deUint32 mipLevel) const
{
	return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, 0u, imageSparseInfo.arrayLayers);
}

TestInstance* SparseShaderIntrinsicsCaseSampledImplicit::createInstance (Context& context) const
{
	return new SparseShaderIntrinsicsInstanceSampledImplicit(context, m_function, m_imageType, m_imageSize, m_format);
}

} // sparse
} // vkt
