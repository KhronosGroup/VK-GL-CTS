/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Image size Tests
 *//*--------------------------------------------------------------------*/

#include "vktImageSizeTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <strstream>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

//! Get a texture based on image type and suggested size.
Texture getTexture (const ImageType imageType, const tcu::IVec3& size)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return Texture(imageType, tcu::IVec3(size.x(), 1, 1), 1);

		case IMAGE_TYPE_1D_ARRAY:
			return Texture(imageType, tcu::IVec3(size.x(), 1, 1), size.y());

		case IMAGE_TYPE_2D:
			return Texture(imageType, tcu::IVec3(size.x(), size.y(), 1), 1);

		case IMAGE_TYPE_2D_ARRAY:
			return Texture(imageType, tcu::IVec3(size.x(), size.y(), 1), size.z());

		case IMAGE_TYPE_CUBE:
			return Texture(imageType, tcu::IVec3(size.x(), size.x(), 1), 6);

		case IMAGE_TYPE_CUBE_ARRAY:
			return Texture(imageType, tcu::IVec3(size.x(), size.x(), 1), 2*6);

		case IMAGE_TYPE_3D:
			return Texture(imageType, size, 1);
	}

	DE_FATAL("Internal error");
	return Texture(IMAGE_TYPE_LAST, tcu::IVec3(), 0);
}

inline VkImageCreateInfo makeImageCreateInfo (const Texture& texture, const VkFormat format)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,								// VkStructureType			sType;
		DE_NULL,															// const void*				pNext;
		(isCube(texture) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u),		// VkImageCreateFlags		flags;
		mapImageType(texture.type()),										// VkImageType				imageType;
		format,																// VkFormat					format;
		makeExtent3D(texture.layerSize()),									// VkExtent3D				extent;
		1u,																	// deUint32					mipLevels;
		texture.numLayers(),												// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,											// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,											// VkSharingMode			sharingMode;
		0u,																	// deUint32					queueFamilyIndexCount;
		DE_NULL,															// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout			initialLayout;
	};
	return imageParams;
}

//! Interpret the memory as IVec3
inline tcu::IVec3 readIVec3 (const void* const data)
{
	const int* const p = reinterpret_cast<const int* const>(data);
	return tcu::IVec3(p[0], p[1], p[2]);
}

tcu::IVec3 getExpectedImageSizeResult (const Texture& texture)
{
	// GLSL imageSize() function returns:
	// z = 0 for cubes
	// z = N for cube arrays, where N is the number of cubes
	// y or z = L where L is the number of layers for other array types (e.g. 1D array, 2D array)
	// z = D where D is the depth of 3d image

	const tcu::IVec3 size = texture.size();
	const int numCubeFaces = 6;

	switch (texture.type())
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return tcu::IVec3(size.x(), 0, 0);

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_CUBE:
			return tcu::IVec3(size.x(), size.y(), 0);

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
			return size;

		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::IVec3(size.x(), size.y(), size.z() / numCubeFaces);
	}

	DE_FATAL("Internal error");
	return tcu::IVec3();
}

class SizeTest : public TestCase
{
public:
	enum TestFlags
	{
		FLAG_READONLY_IMAGE		= 1u << 0,
		FLAG_WRITEONLY_IMAGE	= 1u << 1,
	};

						SizeTest			(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const Texture&		texture,
											 const VkFormat		format,
											 const deUint32		flags = 0);

	void				initPrograms		(SourceCollections& programCollection) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const Texture		m_texture;
	const VkFormat		m_format;
	const bool			m_useReadonly;
	const bool			m_useWriteonly;
};

SizeTest::SizeTest (tcu::TestContext&		testCtx,
					const std::string&		name,
					const std::string&		description,
					const Texture&			texture,
					const VkFormat			format,
					const deUint32			flags)
	: TestCase			(testCtx, name, description)
	, m_texture			(texture)
	, m_format			(format)
	, m_useReadonly		((flags & FLAG_READONLY_IMAGE) != 0)
	, m_useWriteonly	((flags & FLAG_WRITEONLY_IMAGE) != 0)
{
	// We expect at least one flag to be set.
	DE_ASSERT(m_useReadonly || m_useWriteonly);
}

void SizeTest::initPrograms (SourceCollections& programCollection) const
{
	const std::string formatQualifierStr = getShaderImageFormatQualifier(mapVkFormat(m_format));
	const std::string imageTypeStr = getShaderImageType(mapVkFormat(m_format), m_texture.type());
	const int dimension = m_texture.dimension();

	std::ostringstream accessQualifier;
	if (m_useReadonly)
		accessQualifier << " readonly";
	if (m_useWriteonly)
		accessQualifier << " writeonly";

	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(glu::GLSLVersion::GLSL_VERSION_440) << "\n"
		<< "\n"
		<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout (binding = 0, " << formatQualifierStr << ")" << accessQualifier.str() << " uniform highp " << imageTypeStr << " u_image;\n"
		<< "layout (binding = 1) writeonly buffer Output {\n"
		<< "    ivec3 size;\n"
		<< "} sb_out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< (dimension == 1 ?
			"    sb_out.size = ivec3(imageSize(u_image), 0, 0);\n"
			: dimension == 2 || m_texture.type() == IMAGE_TYPE_CUBE ?		// cubes return ivec2
			"    sb_out.size = ivec3(imageSize(u_image), 0);\n"
			: dimension == 3 ?												// cube arrays return ivec3
			"    sb_out.size = imageSize(u_image);\n"
			: "")
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
}

//! Build a case name, e.g. "readonly_writeonly_32x32"
std::string getCaseName (const Texture& texture, const deUint32 flags)
{
	std::ostringstream str;
	str << ((flags & SizeTest::TestFlags::FLAG_READONLY_IMAGE) != 0 ? "readonly_" : "")
		<< ((flags & SizeTest::TestFlags::FLAG_WRITEONLY_IMAGE) != 0 ? "writeonly_" : "");

	const int numComponents = texture.dimension();
	for (int i = 0; i < numComponents; ++i)
		str << (i == 0 ? "" : "x") << texture.size()[i];

	return str.str();
}

//! Base test instance for image and buffer tests
class SizeTestInstance : public TestInstance
{
public:
									SizeTestInstance			(Context&				context,
																 const Texture&			texture,
																 const VkFormat			format);

	tcu::TestStatus                 iterate						(void);

	virtual							~SizeTestInstance			(void) {}

protected:
	virtual VkDescriptorSetLayout	prepareDescriptors			(void) = 0;
	virtual VkDescriptorSet         getDescriptorSet			(void) const = 0;
	virtual void					commandBeforeCompute		(const VkCommandBuffer	cmdBuffer) = 0;

	const Texture					m_texture;
	const VkFormat					m_format;
	const VkDeviceSize				m_resultBufferSizeBytes;
	de::MovePtr<Buffer>				m_resultBuffer;				//!< Shader writes the output here.
};

SizeTestInstance::SizeTestInstance (Context& context, const Texture& texture, const VkFormat format)
	: TestInstance				(context)
	, m_texture					(texture)
	, m_format					(format)
	, m_resultBufferSizeBytes	(3 * sizeof(deUint32))	// ivec3 in shader
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// Create an SSBO for shader output.

	m_resultBuffer = de::MovePtr<Buffer>(new Buffer(
		vk, device, allocator,
		makeBufferCreateInfo(m_resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		MemoryRequirement::HostVisible));
}

tcu::TestStatus SizeTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	// Create memory barriers.

	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		m_resultBuffer->get(), 0ull, m_resultBufferSizeBytes);

	const void* const barriersAfter[] = { &shaderWriteBarrier };

	// Create the pipeline.

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));

	const VkDescriptorSetLayout descriptorSetLayout = prepareDescriptors();
	const VkDescriptorSet descriptorSet = getDescriptorSet();

	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

	commandBeforeCompute(*cmdBuffer);
	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, DE_FALSE, DE_LENGTH_OF_ARRAY(barriersAfter), barriersAfter);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Compare the result.

	const Allocation& bufferAlloc = m_resultBuffer->getAllocation();
	invalidateMappedMemoryRange(vk, device, bufferAlloc.getMemory(), bufferAlloc.getOffset(), m_resultBufferSizeBytes);

	const tcu::IVec3 resultSize = readIVec3(bufferAlloc.getHostPtr());
	const tcu::IVec3 expectedSize = getExpectedImageSizeResult(m_texture);

	if (resultSize != expectedSize)
		return tcu::TestStatus::fail("Incorrect imageSize(): expected " + de::toString(expectedSize) + " but got " + de::toString(resultSize));
	else
		return tcu::TestStatus::pass("Passed");
}

class ImageSizeTestInstance : public SizeTestInstance
{
public:
									ImageSizeTestInstance		(Context&				context,
																 const Texture&			texture,
																 const VkFormat			format);

protected:
	VkDescriptorSetLayout			prepareDescriptors			(void);
	void							commandBeforeCompute		(const VkCommandBuffer	cmdBuffer);

	VkDescriptorSet                 getDescriptorSet			(void) const { return *m_descriptorSet; }

	de::MovePtr<Image>				m_image;
	Move<VkImageView>				m_imageView;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
};

ImageSizeTestInstance::ImageSizeTestInstance (Context& context, const Texture& texture, const VkFormat format)
	: SizeTestInstance	(context, texture, format)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// Create an image. Its data be uninitialized, as we're not reading from it.

	m_image = de::MovePtr<Image>(new Image(vk, device, allocator, makeImageCreateInfo(m_texture, m_format), MemoryRequirement::Any));

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_texture.numLayers());
	m_imageView = makeImageView(vk, device, m_image->get(), mapImageViewType(m_texture.type()), m_format, subresourceRange);
}

VkDescriptorSetLayout ImageSizeTestInstance::prepareDescriptors (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device);

	m_descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

	const VkDescriptorImageInfo descriptorImageInfo = makeDescriptorImageInfo(DE_NULL, *m_imageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo descriptorBufferInfo = makeDescriptorBufferInfo(m_resultBuffer->get(), 0ull, m_resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
		.update(vk, device);

	return *m_descriptorSetLayout;
}

void ImageSizeTestInstance::commandBeforeCompute (const VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_texture.numLayers());
	const VkImageMemoryBarrier barrierSetImageLayout = makeImageMemoryBarrier(
		0u, 0u,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		m_image->get(), subresourceRange);

	const void* const barriers[] = { &barrierSetImageLayout };

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DE_FALSE, DE_LENGTH_OF_ARRAY(barriers), barriers);
}

class BufferSizeTestInstance : public SizeTestInstance
{
public:
									BufferSizeTestInstance		(Context&				context,
																 const Texture&			texture,
																 const VkFormat			format);

protected:
	VkDescriptorSetLayout			prepareDescriptors			(void);

	void							commandBeforeCompute		(const VkCommandBuffer	cmdBuffer) {}
	VkDescriptorSet					getDescriptorSet			(void) const { return *m_descriptorSet; }

	de::MovePtr<Buffer>				m_imageBuffer;
	Move<VkBufferView>				m_bufferView;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
};

BufferSizeTestInstance::BufferSizeTestInstance (Context& context, const Texture& texture, const VkFormat format)
	: SizeTestInstance	(context, texture, format)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// Create a texel storage buffer. Its data be uninitialized, as we're not reading from it.

	const VkDeviceSize imageSizeBytes = getImageSizeBytes(m_texture.size(), m_format);
	m_imageBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
		makeBufferCreateInfo(imageSizeBytes, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT), MemoryRequirement::Any));

	m_bufferView = makeBufferView(vk, device, m_imageBuffer->get(), m_format, 0ull, imageSizeBytes);
}

VkDescriptorSetLayout BufferSizeTestInstance::prepareDescriptors (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	m_descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device);

	m_descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

	const VkDescriptorBufferInfo descriptorBufferInfo = makeDescriptorBufferInfo(m_resultBuffer->get(), 0ull, m_resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, &m_bufferView.get())
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
		.update(vk, device);

	return *m_descriptorSetLayout;
}

TestInstance* SizeTest::createInstance (Context& context) const
{
	if (m_texture.type() == IMAGE_TYPE_BUFFER)
		return new BufferSizeTestInstance(context, m_texture, m_format);
	else
		return new ImageSizeTestInstance(context, m_texture, m_format);
}

static const ImageType s_imageTypes[] =
{
	IMAGE_TYPE_1D,
	IMAGE_TYPE_1D_ARRAY,
	IMAGE_TYPE_2D,
	IMAGE_TYPE_2D_ARRAY,
	IMAGE_TYPE_3D,
	IMAGE_TYPE_CUBE,
	IMAGE_TYPE_CUBE_ARRAY,
	IMAGE_TYPE_BUFFER,
};

//! Base sizes used to generate actual image/buffer sizes in the test.
static const tcu::IVec3 s_baseImageSizes[] =
{
	tcu::IVec3(32, 32, 32),
	tcu::IVec3(12, 34, 56),
	tcu::IVec3(1,   1,  1),
	tcu::IVec3(7,   1,  1),
};

static const deUint32 s_flags[] =
{
	SizeTest::TestFlags::FLAG_READONLY_IMAGE,
	SizeTest::TestFlags::FLAG_WRITEONLY_IMAGE,
	SizeTest::TestFlags::FLAG_READONLY_IMAGE | SizeTest::TestFlags::FLAG_WRITEONLY_IMAGE,
};

} // anonymous ns

tcu::TestCaseGroup* createImageSizeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_size", "imageSize() cases"));

	const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

	for (int imageTypeNdx = 0; imageTypeNdx < DE_LENGTH_OF_ARRAY(s_imageTypes); ++imageTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> imageGroup(new tcu::TestCaseGroup(testCtx, getImageTypeName(s_imageTypes[imageTypeNdx]).c_str(), ""));

		for (int flagNdx = 0; flagNdx < DE_LENGTH_OF_ARRAY(s_flags); ++flagNdx)
		for (int imageSizeNdx = 0; imageSizeNdx < DE_LENGTH_OF_ARRAY(s_baseImageSizes); ++imageSizeNdx)
		{
			const Texture texture = getTexture(s_imageTypes[imageTypeNdx], s_baseImageSizes[imageSizeNdx]);
			imageGroup->addChild(new SizeTest(testCtx, getCaseName(texture, s_flags[flagNdx]), "", texture, format, s_flags[flagNdx]));
		}

		testGroup->addChild(imageGroup.release());
	}
	return testGroup.release();
}

} // image
} // vkt
