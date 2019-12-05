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
 * \file  vktImageAstcDecodeModeTests.cpp
 * \brief Astc decode mode tests
 *//*--------------------------------------------------------------------*/

#include "vktImageAstcDecodeModeTests.hpp"
#include "vktImageLoadStoreUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"
#include <vector>

using namespace vk;
namespace vkt
{
namespace image
{
namespace
{
using std::string;
using std::vector;
using tcu::TestContext;
using tcu::TestStatus;
using tcu::UVec3;
using tcu::IVec3;
using tcu::CompressedTexFormat;
using tcu::CompressedTexture;
using de::MovePtr;
using de::SharedPtr;
using de::Random;

struct TestParameters
{
	ImageType			imageType;
	UVec3				imageSize;

	VkFormat			testedFormat;
	deBool				testedIsUnorm;
	VkFormat			testedDecodeMode;
	VkImageUsageFlags	testedImageUsage;

	VkFormat			resultFormat;
	VkImageUsageFlags	resultImageUsage;
};

class BasicComputeTestInstance : public TestInstance
{
public:
					BasicComputeTestInstance	(Context&					context,
												 const TestParameters&		parameters);

	TestStatus		iterate						(void);

protected:

	void			generateData				(deUint8*		toFill,
												 const size_t	size,
												 const VkFormat format,
												 const deUint32 layer,
												 const deUint32 level);

protected:

	const TestParameters	m_parameters;
};

BasicComputeTestInstance::BasicComputeTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance	(context)
	, m_parameters	(parameters)
{
}

TestStatus BasicComputeTestInstance::iterate (void)
{
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkImageType				imageType			= mapImageType(m_parameters.imageType);
	const VkExtent3D				extentCompressed	= makeExtent3D(getCompressedImageResolutionInBlocks(m_parameters.testedFormat, m_parameters.imageSize));
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkShaderModule>	shaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));

	const VkImageCreateInfo compressedImageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
		VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT,		// VkImageCreateFlags		flags;
		imageType,												// VkImageType				imageType;
		m_parameters.testedFormat,								// VkFormat					format;
		extentCompressed,										// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		1u,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,						// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};

	const VkImageCreateInfo resultImageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		0u,														// VkImageCreateFlags		flags;
		imageType,												// VkImageType				imageType;
		m_parameters.resultFormat,								// VkFormat					format;
		extentCompressed,										// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		1u,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,						// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};

	// create images
	Image							testedImage				(vk, device, allocator, compressedImageInfo, MemoryRequirement::Any);
	Image							referenceImage			(vk, device, allocator, compressedImageInfo, MemoryRequirement::Any);
	Image							resultImage				(vk, device, allocator, resultImageInfo, MemoryRequirement::Any);

	// create image views
	const VkImageViewType			imageViewType			(mapImageViewType(m_parameters.imageType));
	VkImageSubresourceRange			subresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	VkFormat						viewFormat				= m_parameters.testedIsUnorm ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_R32G32B32A32_SINT;

	VkImageViewASTCDecodeModeEXT decodeMode =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT,
		DE_NULL,
		m_parameters.testedDecodeMode
	};

	const VkImageViewCreateInfo imageViewParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
		&decodeMode,									// const void*				pNext;
		0u,												// VkImageViewCreateFlags	flags;
		testedImage.get(),								// VkImage					image;
		imageViewType,									// VkImageViewType			viewType;
		viewFormat,										// VkFormat					format;
		makeComponentMappingRGBA(),						// VkComponentMapping		components;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};

	Move<VkImageView>				testedView				= createImageView(vk, device, &imageViewParams);
	Move<VkImageView>				referenceView			= makeImageView(vk, device, referenceImage.get(), imageViewType, viewFormat, subresourceRange);
	Move<VkImageView>				resultView				= makeImageView(vk, device, resultImage.get(), imageViewType, m_parameters.resultFormat,
																makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, resultImageInfo.extent.depth, 0u, resultImageInfo.arrayLayers));

	Move<VkDescriptorSetLayout>		descriptorSetLayout		= DescriptorSetLayoutBuilder()
																.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
																.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
																.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
																.build(vk, device);
	Move<VkDescriptorPool>			descriptorPool			= DescriptorPoolBuilder()
																.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, compressedImageInfo.arrayLayers)
																.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, compressedImageInfo.arrayLayers)
																.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resultImageInfo.arrayLayers)
																.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, resultImageInfo.arrayLayers);

	Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline				(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkDeviceSize				bufferSizeCompresed		= getCompressedImageSizeInBytes(m_parameters.testedFormat, m_parameters.imageSize);
	const VkDeviceSize				bufferSizeUncompressed	= getImageSizeBytes(IVec3((int)extentCompressed.width, (int)extentCompressed.height, (int)extentCompressed.depth), m_parameters.resultFormat);
	VkBufferCreateInfo				compressedBufferCI		= makeBufferCreateInfo(bufferSizeCompresed, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VkBufferCreateInfo				uncompressedBufferCI	= makeBufferCreateInfo(bufferSizeUncompressed, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	Buffer							inBuffer				(vk, device, allocator, compressedBufferCI, MemoryRequirement::HostVisible);
	Buffer							resultBuffer			(vk, device, allocator, uncompressedBufferCI, MemoryRequirement::HostVisible);
	Move<VkSampler>					sampler;

	// generate data for compressed image and copy it to in buffer
	{
		vector<deUint8> generatedData;
		generatedData.resize(static_cast<size_t>(bufferSizeCompresed));
		generateData(generatedData.data(), generatedData.size(), m_parameters.testedFormat, 0u, 0u);

		const Allocation& alloc = inBuffer.getAllocation();
		deMemcpy(alloc.getHostPtr(), generatedData.data(), generatedData.size());
		flushAlloc(vk, device, alloc);
	}

	{
		const VkSamplerCreateInfo createInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,			//VkStructureType		sType;
			DE_NULL,										//const void*			pNext;
			0u,												//VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,								//VkFilter				magFilter;
			VK_FILTER_NEAREST,								//VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,					//VkSamplerMipmapMode	mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			//VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			//VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			//VkSamplerAddressMode	addressModeW;
			0.0f,											//float					mipLodBias;
			VK_FALSE,										//VkBool32				anisotropyEnable;
			1.0f,											//float					maxAnisotropy;
			VK_FALSE,										//VkBool32				compareEnable;
			VK_COMPARE_OP_EQUAL,							//VkCompareOp			compareOp;
			0.0f,											//float					minLod;
			1.0f,											//float					maxLod;
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,		//VkBorderColor			borderColor;
			VK_FALSE,										//VkBool32				unnormalizedCoordinates;
		};
		sampler = createSampler(vk, device, &createInfo);
	}

	VkDescriptorImageInfo descriptorImageInfos[] =
	{
		makeDescriptorImageInfo(*sampler,	*testedView,	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		makeDescriptorImageInfo(*sampler,	*referenceView,	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		makeDescriptorImageInfo(DE_NULL,	*resultView,	VK_IMAGE_LAYOUT_GENERAL),
	};
	DescriptorSetUpdateBuilder()
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[0])
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[1])
		.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfos[2])
		.update(vk, device);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		// copy input buffer to tested and reference images
		{
			Image* inImages[] = { &testedImage, &referenceImage };
			for (Image* image : inImages)
			{
				const VkImageMemoryBarrier preCopyImageBarrier = makeImageMemoryBarrier(
					0u, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					image->get(), subresourceRange);

				const VkBufferMemoryBarrier flushHostCopyBarrier = makeBufferMemoryBarrier(
					VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					inBuffer.get(), 0ull, bufferSizeCompresed);

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					(VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &flushHostCopyBarrier, 1u, &preCopyImageBarrier);

				const VkBufferImageCopy copyRegion =
				{
					0ull,																			//VkDeviceSize				bufferOffset;
					0u,																				//deUint32					bufferRowLength;
					0u,																				//deUint32					bufferImageHeight;
					makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),				//VkImageSubresourceLayers	imageSubresource;
					makeOffset3D(0, 0, 0),															//VkOffset3D				imageOffset;
					extentCompressed,																//VkExtent3D				imageExtent;
				};

				vk.cmdCopyBufferToImage(*cmdBuffer, inBuffer.get(), image->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
			}
		}

		// bind pipeline and descriptors
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		{
			const VkImageMemoryBarrier preShaderImageBarriers[] =
			{
				makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					testedImage.get(), subresourceRange),

				makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					referenceImage.get(), subresourceRange),

				makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
					resultImage.get(), subresourceRange)
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(preShaderImageBarriers), preShaderImageBarriers);
		}

		vk.cmdDispatch(*cmdBuffer, extentCompressed.width, extentCompressed.height, extentCompressed.depth);

		{
			const VkImageMemoryBarrier postShaderImageBarriers[] =
			{
				makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					resultImage.get(), subresourceRange)
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL,
				DE_LENGTH_OF_ARRAY(postShaderImageBarriers), postShaderImageBarriers);
		}

		const VkBufferImageCopy copyRegion =
		{
			0ull,																//	VkDeviceSize				bufferOffset;
			0u,																	//	deUint32					bufferRowLength;
			0u,																	//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),												//	VkOffset3D					imageOffset;
			resultImageInfo.extent,												//	VkExtent3D					imageExtent;
		};
		vk.cmdCopyImageToBuffer(*cmdBuffer, resultImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultBuffer.get(), 1u, &copyRegion);

		{
			const VkBufferMemoryBarrier postCopyBufferBarrier[] =
			{
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
					resultBuffer.get(), 0ull, bufferSizeUncompressed),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				(VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(postCopyBufferBarrier), postCopyBufferBarrier,
				0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	const Allocation& resultAlloc = resultBuffer.getAllocation();
	invalidateAlloc(vk, device, resultAlloc);

	// verification is done in shader - here we just check if one of pixels has wrong value
	const size_t	numBytes		= static_cast<size_t>(bufferSizeUncompressed);
	deUint8*		result		= static_cast<deUint8*>(resultAlloc.getHostPtr());
	for (size_t i = 0 ; i < numBytes ; i += 4)
	{
		// expected result should be around 128 (if reference is same as tested mode then we return 0.5)
		if ((result[i] < 100) || (result[i] > 150))
			return TestStatus::fail("Fail");
	}

	return TestStatus::pass("Pass");
}

void BasicComputeTestInstance::generateData (deUint8*		toFill,
											 const size_t	size,
											 const VkFormat format,
											 const deUint32 layer,
											 const deUint32 level)
{
	// Random data
	deUint32*	start32		= reinterpret_cast<deUint32*>(toFill);
	size_t		sizeToRnd32	= size / sizeof(deUint32);
	deUint32	seed		= (layer << 24) ^ (level << 16) ^ static_cast<deUint32>(format);
	Random		rnd			(seed);

	for (size_t i = 0; i < sizeToRnd32; i++)
		start32[i] = rnd.getUint32();
}

class AstcDecodeModeCase : public TestCase
{
public:
					AstcDecodeModeCase	(TestContext&			testCtx,
										 const std::string&		name,
										 const std::string&		desc,
										 const TestParameters&	parameters);
	virtual void	checkSupport		(Context&				context) const;
	void			initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*	createInstance		(Context&				context) const;

protected:

	const TestParameters m_parameters;
};

AstcDecodeModeCase::AstcDecodeModeCase (TestContext&			testCtx,
										const std::string&		name,
										const std::string&		desc,
										const TestParameters&	parameters)
	: TestCase		(testCtx, name, desc)
	, m_parameters	(parameters)
{
}

void AstcDecodeModeCase::checkSupport (Context& context) const
{
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	const InstanceInterface&	vk				= context.getInstanceInterface();

	context.requireDeviceFunctionality("VK_EXT_astc_decode_mode");
	if (!getPhysicalDeviceFeatures(vk, physicalDevice).textureCompressionASTC_LDR)
		TCU_THROW(NotSupportedError, "textureCompressionASTC_LDR not supported");

	VkImageFormatProperties imageFormatProperties;
	if (VK_ERROR_FORMAT_NOT_SUPPORTED == vk.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.testedFormat,
												  mapImageType(m_parameters.imageType), VK_IMAGE_TILING_OPTIMAL,
												  m_parameters.testedImageUsage, 0u, &imageFormatProperties))
		TCU_THROW(NotSupportedError, "Operation not supported with this image format");

	if (VK_ERROR_FORMAT_NOT_SUPPORTED == vk.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.resultFormat,
												  mapImageType(m_parameters.imageType), VK_IMAGE_TILING_OPTIMAL,
												  m_parameters.resultImageUsage, 0u, &imageFormatProperties))
		TCU_THROW(NotSupportedError, "Operation not supported with this image format");

	if ((m_parameters.testedDecodeMode == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32) &&
		!context.getASTCDecodeFeaturesEXT().decodeModeSharedExponent)
		TCU_THROW(NotSupportedError, "decodeModeSharedExponent not supported");

	VkFormatProperties properties;
	context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_parameters.resultFormat, &properties);
	if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		TCU_THROW(NotSupportedError, "Format storage feature not supported");
}

void AstcDecodeModeCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(m_parameters.imageSize.x() > 0);
	DE_ASSERT(m_parameters.imageSize.y() > 0);

	VkFormat					compatibileFormat	= m_parameters.testedIsUnorm ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_R32G32B32A32_SINT;
	tcu::TextureFormat			testedTextureFormat	= mapVkFormat(compatibileFormat);
	VkImageViewType				imageViewType		= mapImageViewType(m_parameters.imageType);
	string						samplerType			= getGlslSamplerType(testedTextureFormat, imageViewType);
	const string				formatQualifierStr	= getShaderImageFormatQualifier(mapVkFormat(m_parameters.resultFormat));
	const string				imageTypeStr		= getShaderImageType(mapVkFormat(m_parameters.resultFormat), m_parameters.imageType);

	std::ostringstream	src;
	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n\n"
		<< "layout (binding = 0) uniform " << samplerType << " compressed_tested;\n"
		<< "layout (binding = 1) uniform " << samplerType << " compressed_reference;\n"
		<< "layout (binding = 2, " << formatQualifierStr << ") writeonly uniform " << imageTypeStr << " result;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    const vec2 pixels_resolution = vec2(gl_NumWorkGroups.xy);\n"
		<< "    const vec2 cord = vec2(gl_GlobalInvocationID.xy) / vec2(pixels_resolution);\n"
		<< "    const ivec2 pos = ivec2(gl_GlobalInvocationID.xy); \n"
		<< "    vec4 tested = texture(compressed_tested, cord);\n"
		<< "    vec4 reference = texture(compressed_reference, cord);\n";

	// special case for e5b9g9r9 decode mode that was set on unorm astc formats
	// here negative values are clamped to zero and alpha is set to 1
	if (m_parameters.testedIsUnorm && (m_parameters.testedDecodeMode == VK_FORMAT_E5B9G9R9_UFLOAT_PACK32))
		src << "    reference = max(vec4(0,0,0,1), reference);\n"
			   "    float result_color = 0.5 * float(distance(tested, reference) < 0.01);\n";
	else
		src << "    float result_color = 0.5 * float(distance(tested, reference) < 0.01);\n";

	src << "    imageStore(result, pos, vec4(result_color));\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* AstcDecodeModeCase::createInstance (Context& context) const
{
	return new BasicComputeTestInstance(context, m_parameters);
}

} // anonymous ns

tcu::TestCaseGroup* createImageAstcDecodeModeTests (tcu::TestContext& testCtx)
{
	struct FormatData
	{
		VkFormat		format;
		std::string		name;
		deBool			isUnorm;
	};
	const FormatData astcFormats[] =
	{
		{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK,		"4x4_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK,		"4x4_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK,		"5x4_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK,		"5x4_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK,		"5x5_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK,		"5x5_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK,		"6x5_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK,		"6x5_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK,		"6x6_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK,		"6x6_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK,		"8x5_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK,		"8x5_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK,		"8x6_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK,		"8x6_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK,		"8x8_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK,		"8x8_srgb",		DE_FALSE },
		{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK,		"10x5_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK,		"10x5_srgb",	DE_FALSE },
		{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK,		"10x6_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK,		"10x6_srgb",	DE_FALSE },
		{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK,		"10x8_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK,		"10x8_srgb",	DE_FALSE },
		{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK,		"10x10_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK,		"10x10_srgb",	DE_FALSE },
		{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK,		"12x10_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK,		"12x10_srgb",	DE_FALSE },
		{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK,		"12x12_unorm",	DE_TRUE },
		{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK,		"12x12_srgb",	DE_FALSE },
	};

	struct DecodeModeData
	{
		VkFormat		mode;
		std::string		name;
	};
	const DecodeModeData decodeModes[] =
	{
		{ VK_FORMAT_R16G16B16A16_SFLOAT,	"r16g16b16a16_sfloat" },
		{ VK_FORMAT_R8G8B8A8_UNORM,			"r8g8b8a8_unorm" },
		{ VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,	"e5b9g9r9_ufloat_pack32" }
	};

	MovePtr<tcu::TestCaseGroup> astcDecodeModeTests(new tcu::TestCaseGroup(testCtx, "astc_decode_mode", "Intermediate decoding precision cases"));
	for (const FormatData& format : astcFormats)
	{
		for (const DecodeModeData& mode : decodeModes)
		{
			const TestParameters parameters =
			{
				IMAGE_TYPE_2D,
				UVec3(64u, 64u, 1u),
				format.format,
				format.isUnorm,
				mode.mode,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_USAGE_STORAGE_BIT
			};

			std::string name = format.name + "_to_" + mode.name;
			astcDecodeModeTests->addChild(new AstcDecodeModeCase(testCtx, name, "", parameters));
		}
	}

	return astcDecodeModeTests.release();
}

} // image
} // vkt
