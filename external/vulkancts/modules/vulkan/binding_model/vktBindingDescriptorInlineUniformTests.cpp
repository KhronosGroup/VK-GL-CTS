/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for inline uniform blocks
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorInlineUniformTests.hpp"
#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkDefs.hpp"
#include "vkQueryUtil.hpp"
#include "vkPrograms.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuDefs.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"

#include "deDefs.h"
#include "deSTLUtil.hpp"
#include "deSharedPtr.hpp"

#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <vector>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;

#ifndef CTS_USES_VULKANSC

const tcu::IVec2 renderSize(1, 16);

enum
{
	UPD_STATUS_NONE = 0,
	UPD_STATUS_WRITTEN,
	UPD_STATUS_COPIED
};

using UintVector = std::vector<uint32_t>;

const uint32_t inlineUniformBlockMinimumSize = 4;

class InlineUniformBlockDescriptor
{
	public:
		InlineUniformBlockDescriptor(uint32_t set, uint32_t binding, uint32_t size, uint32_t& id);
		virtual ~InlineUniformBlockDescriptor(void) {}

		uint32_t getSet(void) const { return m_set; }
		uint32_t getBinding(void) const { return m_binding; }
		uint32_t getSize(void) const { return m_size; }
		uint32_t getData(uint32_t at) const { return m_dataToWrite[at]; }
		uint32_t* getDataReference(uint32_t at) { return &m_dataToWrite[at]; }

		uint32_t getStatus(uint32_t at) { return m_updateStatus[at]; }
		void changeStatus(uint32_t offset, uint32_t size, uint32_t status);

		void setVerificationData(uint32_t at, uint32_t data)
		{
			m_verificationData[at] = data;
		}
		uint32_t getVerificationData(uint32_t at) { return m_verificationData[at]; }
	private:
		const uint32_t m_set;
		const uint32_t m_binding;
		const uint32_t m_size; // must be multiple of 4 in input
		UintVector m_dataToWrite;
		UintVector m_updateStatus;
		UintVector m_verificationData;
};

InlineUniformBlockDescriptor::InlineUniformBlockDescriptor(uint32_t set, uint32_t binding, uint32_t size, uint32_t& id)
    : m_set(set)
    , m_binding(binding)
    , m_size(size)
{
	uint32_t arraySize = m_size/inlineUniformBlockMinimumSize;
	m_dataToWrite.resize(arraySize);
	m_verificationData.resize(arraySize);
	m_updateStatus.resize(arraySize);
	for (uint32_t k = 0; k < arraySize; k++)
	{
		m_dataToWrite[k] = m_verificationData[k] = id++;
		m_updateStatus[k] = UPD_STATUS_NONE;
	}
}

void InlineUniformBlockDescriptor::changeStatus(uint32_t offset, uint32_t size, uint32_t status)
{
	const uint32_t updIdx = offset/inlineUniformBlockMinimumSize;
	const uint32_t updSize = size/inlineUniformBlockMinimumSize;

	for (uint32_t i = updIdx; i < updSize; i++)
		m_updateStatus[i] = status;
}

using IubPtr = de::SharedPtr<InlineUniformBlockDescriptor>;

class InlineUniformBlockWrite
{
	public:
		InlineUniformBlockWrite(IubPtr descriptor, uint32_t writeOffset, uint32_t writeSize)
			: m_descriptor(descriptor)
			, m_writeSize(writeSize)
			{
				m_writeLocation = { writeOffset, writeOffset/inlineUniformBlockMinimumSize };
			}
		virtual ~InlineUniformBlockWrite(void) {}

		VkWriteDescriptorSetInlineUniformBlockEXT getInlineUniformBlockWrite() const;
		uint32_t getDestSet(void) const { return m_descriptor->getSet(); }
		uint32_t getDestBinding(void) const { return m_descriptor->getBinding(); }
		uint32_t getDestOffset(void) const { return m_writeLocation.writeOffset; }
		uint32_t getWriteSize(void) const { return m_writeSize; }
	private:
		IubPtr m_descriptor;
		struct
		{
			uint32_t writeOffset; // must be multiple of 4 in input
			uint32_t writeIndex;
		} m_writeLocation;

		const uint32_t m_writeSize; // must be multiple of 4 in input
};

VkWriteDescriptorSetInlineUniformBlockEXT InlineUniformBlockWrite::getInlineUniformBlockWrite() const
{
	const VkWriteDescriptorSetInlineUniformBlockEXT iubWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT,	// VkStructureType    sType;
		nullptr,															// const void*        pNext;
		m_writeSize,														// uint32_t           dataSize;
		m_descriptor->getDataReference(m_writeLocation.writeIndex)			// const void*        pData;
	};
	return iubWrite;
}

using IubWritePtr = de::SharedPtr<InlineUniformBlockWrite>;

class InlineUniformBlockCopy
{
	public:
		InlineUniformBlockCopy(IubPtr srcDesc, IubPtr destDesc, uint32_t srcOffset, uint32_t destOffset, uint32_t copySize)
			: m_srcDescriptor(srcDesc)
			, m_destDescriptor(destDesc)
			, m_copySize(copySize)
			{
				m_srcLocation = { srcOffset, srcOffset/inlineUniformBlockMinimumSize };
				m_destLocation = { destOffset, destOffset/inlineUniformBlockMinimumSize };
			}

		uint32_t getSrcSet(void) const { return m_srcDescriptor->getSet(); }
		uint32_t getSrcBinding(void) const { return m_srcDescriptor->getBinding(); }
		uint32_t getSrcOffset(void) const { return m_srcLocation.srcOffset; }
		uint32_t getDestSet(void) { return m_destDescriptor->getSet(); }
		uint32_t getDestBinding(void) const { return m_destDescriptor->getBinding(); }
		uint32_t getDestOffset(void) const { return m_destLocation.destOffset; }
		uint32_t getCopySize(void) const { return m_copySize; }

	private:
		IubPtr m_srcDescriptor;
		IubPtr m_destDescriptor;
		struct
		{
			uint32_t srcOffset; // must be multiple of 4 in input
			uint32_t srcIndex;
		} m_srcLocation;

		struct
		{
			uint32_t destOffset; // must be multiple of 4 in input
			uint32_t destIndex;
		} m_destLocation;

		const uint32_t m_copySize; // must be multiple of 4 in input
};

using IubCopyPtr = de::SharedPtr<InlineUniformBlockCopy>;

using Bindings = std::vector<IubPtr>;
using Sets = std::map<uint32_t, Bindings>;
using SetBinding = std::pair<uint32_t, Bindings>;

class DescriptorOps
{
	public:
		DescriptorOps(){}
		virtual ~DescriptorOps(void){}
		IubPtr addDescriptor(uint32_t set, uint32_t binding, uint32_t size, uint32_t& id);
		void updateVerificationData(IubPtr fromDesc, IubPtr toDesc, uint32_t srcOffset, uint32_t destOffset, uint32_t size);
		void writeDescriptor(IubPtr desc, uint32_t offset, uint32_t size);
		void copyDescriptor(IubPtr fromDesc, IubPtr toDesc, uint32_t srcOffset, uint32_t destOffset, uint32_t size);

		uint32_t getNumWriteOps() const { return de::sizeU32(m_writes); }
		const IubWritePtr getWriteOp(uint32_t at) const { return m_writes[at]; }
		uint32_t getNumCopyOps() const { return de::sizeU32(m_copies); }
		const IubCopyPtr getCopyOp(uint32_t at) const { return m_copies[at]; }
		uint32_t getNumDescriptors() const { return de::sizeU32(m_allDescriptors); }
		const IubPtr getDescriptor(uint32_t at) const { return m_allDescriptors[at]; }
		uint32_t getNumDescriptorSets() const { return de::sizeU32(m_Sets); }
		const Sets& getDescriptorSets(void) const { return m_Sets; }
	private:
		std::vector<IubWritePtr> m_writes;
		std::vector<IubCopyPtr> m_copies;
		std::vector<IubPtr> m_allDescriptors;
		Sets m_Sets;
};

using OpsPtr = de::SharedPtr<DescriptorOps>;

IubPtr DescriptorOps::addDescriptor(uint32_t set, uint32_t binding, uint32_t size, uint32_t& id)
{
	IubPtr newIub = IubPtr(new InlineUniformBlockDescriptor(set, binding, size, id));
	m_allDescriptors.push_back(newIub);

	uint32_t key = newIub->getSet();
	// assuming binding id will be unique
	Sets::iterator descsIt = m_Sets.find(key);
	if(descsIt != m_Sets.end())
	{
		Bindings& descs = descsIt->second;
		descs.push_back(newIub);
	}
	else
	{
		Bindings newDescs;
		newDescs.push_back(newIub);
		m_Sets.insert(SetBinding(key, newDescs));
	}

	return newIub;
}

void DescriptorOps::updateVerificationData(IubPtr fromDesc, IubPtr toDesc, uint32_t srcOffset, uint32_t destOffset, uint32_t size)
{
	uint32_t srcIndex = srcOffset/inlineUniformBlockMinimumSize;
	uint32_t destIndex = destOffset/inlineUniformBlockMinimumSize;
	for (uint32_t k = 0, s = srcIndex, d = destIndex; k < size; k += inlineUniformBlockMinimumSize, s++, d++)
		toDesc->setVerificationData(d, fromDesc->getData(s));// TODO: chain writes?
}

void DescriptorOps::writeDescriptor(IubPtr desc, uint32_t offset, uint32_t size)
{
	m_writes.push_back(IubWritePtr(new InlineUniformBlockWrite(desc, offset, size)));
	desc->changeStatus(offset, size, UPD_STATUS_WRITTEN);
}

void DescriptorOps::copyDescriptor(IubPtr fromDesc, IubPtr toDesc, uint32_t srcOffset, uint32_t destOffset, uint32_t size)
{
	updateVerificationData(fromDesc, toDesc, srcOffset, destOffset, size);

	// those descriptors that have to be copied have to be written first
	writeDescriptor(fromDesc, srcOffset, size);
	writeDescriptor(toDesc, destOffset, size);

	m_copies.push_back(IubCopyPtr(new InlineUniformBlockCopy(fromDesc, toDesc, srcOffset, destOffset, size)));
	toDesc->changeStatus(destOffset, size, UPD_STATUS_COPIED);
}

class DescriptorInlineUniformTestInstance: public TestInstance
{
	public:
		DescriptorInlineUniformTestInstance(Context& context, OpsPtr ops);
		virtual ~DescriptorInlineUniformTestInstance(void);
		tcu::TestStatus iterate(void);
		bool verifyResultImage (const tcu::ConstPixelBufferAccess& pba);
	private:
		const VkFormat m_colorFormat;
		OpsPtr m_ops;
};

DescriptorInlineUniformTestInstance::DescriptorInlineUniformTestInstance(Context& context, OpsPtr ops)
    : vkt::TestInstance(context)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_ops(ops)
{
}

DescriptorInlineUniformTestInstance::~DescriptorInlineUniformTestInstance(void)
{
}

VkImageCreateInfo makeColorImageCreateInfo (const VkFormat format, const deUint32 width, const deUint32 height)
{
	const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo	imageInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType			sType;
		nullptr,								//  const void*				pNext;
		(VkImageCreateFlags)0,					//  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType				imageType;
		format,									//  VkFormat				format;
		makeExtent3D(width, height, 1),			//  VkExtent3D				extent;
		1u,										//  deUint32				mipLevels;
		1u,										//  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling			tiling;
		usage,									//  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode			sharingMode;
		0u,										//  deUint32				queueFamilyIndexCount;
		nullptr,								//  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout			initialLayout;
	};

	return imageInfo;
}

VkImageViewCreateInfo makeImageViewCreateInfo (VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
	const VkComponentMapping		components			=
	{
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A,
	};
	const VkImageSubresourceRange	subresourceRange	=
	{
		aspectMask,	//  VkImageAspectFlags	aspectMask;
		0,			//  deUint32			baseMipLevel;
		1,			//  deUint32			levelCount;
		0,			//  deUint32			baseArrayLayer;
		1,			//  deUint32			layerCount;
	};
	const VkImageViewCreateInfo		result				=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//  VkStructureType			sType;
		nullptr,									//  const void*				pNext;
		0u,											//  VkImageViewCreateFlags	flags;
		image,										//  VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						//  VkImageViewType			viewType;
		format,										//  VkFormat				format;
		components,									//  VkComponentMapping		components;
		subresourceRange,							//  VkImageSubresourceRange	subresourceRange;
	};

	return result;
}

bool DescriptorInlineUniformTestInstance::verifyResultImage (const tcu::ConstPixelBufferAccess& resultAccess)
{
	bool testOk = true;
	tcu::TestLog& log = m_context.getTestContext().getLog();

	const tcu::TextureFormat tcuFormat = vk::mapVkFormat(m_colorFormat);
	const tcu::Vec4 correctColor (0.0f, 1.0f, 0.0f, 1.0f);
	tcu::TextureLevel referenceLevel (tcuFormat, renderSize.x(), renderSize.y());
	auto referenceAccess = referenceLevel.getAccess();

	for (int y = 0; y < renderSize.y(); y++)
		for (int x = 0; x < renderSize.x(); x++)
			referenceAccess.setPixel(correctColor, x, y);

	const tcu::Vec4 threshold (0.0f, 0.0f, 0.0f, 0.0f);
	if (!tcu::floatThresholdCompare(log, "Result", "Reference", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		testOk = false;

	return testOk;
}

tcu::TestStatus DescriptorInlineUniformTestInstance::iterate(void)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	const VkDevice device = m_context.getDevice();
	const uint32_t queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator& allocator = m_context.getDefaultAllocator();
	const VkQueue queue = m_context.getUniversalQueue();
	bool testOk = true;
	const uint32_t numDescs = m_ops->getNumDescriptors();

	// Create descriptor pool
	Move<VkDescriptorPool> descPool;
	{
		uint32_t totalIubSize = 0;
		for (uint32_t i = 0; i < numDescs; i++)
			totalIubSize += m_ops->getDescriptor(i)->getSize();

		VkDescriptorPoolInlineUniformBlockCreateInfoEXT iubPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
			nullptr,
			numDescs
		};

		descPool = DescriptorPoolBuilder()
		    .addType(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, totalIubSize)
		    .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, m_ops->getNumDescriptorSets(), &iubPoolCreateInfo);
	}

	// Create descriptor set layouts for each set and allocate the set
	std::vector<Move<VkDescriptorSetLayout>> descSetLayouts;
	std::vector<Move<VkDescriptorSet>> descSetPtrs;
	std::vector<VkDescriptorSet> descSets;
	{
		const uint32_t numSets = m_ops->getNumDescriptorSets();
		descSetLayouts.resize(numSets);
		descSetPtrs.resize(numSets);
		descSets.resize(numSets);

		const Sets& descSetBindingMap = m_ops->getDescriptorSets();
		for (const auto& it: descSetBindingMap)
		{
			uint32_t setId = it.first;
			Bindings bindings = it.second;


			// create layout of each binding within the set
			DescriptorSetLayoutBuilder descSetLayoutBuilder;

			for (uint32_t idx = 0; idx < bindings.size(); idx++)
				descSetLayoutBuilder.addIndexedBinding(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, bindings[idx]->getSize(), VK_SHADER_STAGE_FRAGMENT_BIT, bindings[idx]->getBinding(), nullptr);

			// create layout of the descriptor set
			descSetLayouts[setId] = descSetLayoutBuilder.build(vk, device);

			// allocate the descriptor set
			descSetPtrs[setId] = makeDescriptorSet(vk, device, *descPool, descSetLayouts[setId].get());
			descSets[setId] = descSetPtrs[setId].get();
		}
	}

	// Descriptor update
	{
		DescriptorSetUpdateBuilder descSetUpdateBuilder;
		std::vector<VkWriteDescriptorSetInlineUniformBlockEXT> iubWrites;

		const uint32_t numWrites = m_ops->getNumWriteOps();
		iubWrites.resize(numWrites);
		for (uint32_t idx = 0; idx < numWrites; idx++)
		{
			const IubWritePtr writeOp = m_ops->getWriteOp(idx);
			iubWrites[idx] = writeOp->getInlineUniformBlockWrite();
			const VkDescriptorSet& destSet = descSets[writeOp->getDestSet()];
			descSetUpdateBuilder.write(destSet,
			                           writeOp->getDestBinding(),
									   writeOp->getDestOffset(),
									   writeOp->getWriteSize(),
									   VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
									   nullptr, nullptr, nullptr,
									   &iubWrites[idx]);
		}

		const uint32_t numCopies = m_ops->getNumCopyOps();
		for (uint32_t idx = 0; idx < numCopies; idx++)
		{
			const IubCopyPtr copyOp = m_ops->getCopyOp(idx);
			const VkDescriptorSet& srcSet = descSets[copyOp->getSrcSet()];
			const VkDescriptorSet& destSet = descSets[copyOp->getDestSet()];
			descSetUpdateBuilder.copy(srcSet,
			                          copyOp->getSrcBinding(),
									  copyOp->getSrcOffset(),
									  destSet,
									  copyOp->getDestBinding(),
									  copyOp->getDestOffset(),
									  copyOp->getCopySize());
		}

		descSetUpdateBuilder.update(vk, device);
	}

	// create pipeline and bind desc layout
	std::vector<VkDescriptorSetLayout> descSetLayoutHandles;
	descSetLayoutHandles.reserve(descSetLayouts.size());
	std::transform(begin(descSetLayouts), end(descSetLayouts), std::back_inserter(descSetLayoutHandles),
		[](const Move<VkDescriptorSetLayout>& layout) { return layout.get(); });
	const auto pipelineLayout = makePipelineLayout(vk, device, de::sizeU32(descSetLayoutHandles), de::dataOrNull(descSetLayoutHandles));

	// create image and image view that will hold rendered frame
	const VkImageCreateInfo colorImageCreateInfo = makeColorImageCreateInfo(m_colorFormat, renderSize.x(), renderSize.y());
	de::MovePtr<ImageWithMemory> colorImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
	const VkImageViewCreateInfo colorImageViewCreateInfo = makeImageViewCreateInfo(**colorImage, m_colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
	const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);

	// create renderpass and framebuffer
	Move<VkRenderPass> renderPass = makeRenderPass(vk, device, m_colorFormat);
	Move<VkFramebuffer> framebuffer = makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());

	// create output buffer for verification
	const VkDeviceSize outputBufferDataSize = static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
	const VkBufferCreateInfo outputBufferCreateInfo = makeBufferCreateInfo(outputBufferDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const BufferWithMemory outputBuffer (vk, device, allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);

	// Create graphics pipeline
	Move<VkPipeline> pipeline;
	{
		const Unique<VkShaderModule> vertexShaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule> fragmentShaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = initVulkanStructure();

		const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
		const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

		pipeline = makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
										device,									// const VkDevice                                device
										*pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
										*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
										VK_NULL_HANDLE,							// const VkShaderModule                          tessellationControlShaderModule
										VK_NULL_HANDLE,							// const VkShaderModule                          tessellationEvalShaderModule
										VK_NULL_HANDLE,							// const VkShaderModule                          geometryShaderModule
										*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
										*renderPass,							// const VkRenderPass                            renderPass
										viewports,								// const std::vector<VkViewport>&                viewports
										scissors,								// const std::vector<VkRect2D>&                  scissors
										VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
										0u,										// const deUint32                                subpass
										0u,										// const deUint32                                patchControlPoints
										&vertexInputStateParams);				// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
	}

	// Run verification shader
	{
		Move<VkCommandPool> cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
		Move<VkCommandBuffer> commandBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(vk, *commandBuffer);

		const VkRect2D renderArea = makeRect2D(renderSize);
		const tcu::Vec4 clearColor  (1.0f, 0.0f, 0.0f, 1.0f);
		beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, clearColor);

		vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, de::sizeU32(descSets), de::dataOrNull(descSets), 0u, nullptr);

		vk.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
		endRenderPass(vk, *commandBuffer);
		copyImageToBuffer(vk, *commandBuffer, **colorImage, *outputBuffer, renderSize);

		endCommandBuffer(vk, *commandBuffer);
		submitCommandsAndWait(vk, device, queue, *commandBuffer);
	}

	{
		invalidateAlloc(vk, device, outputBuffer.getAllocation());

		tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(m_colorFormat), renderSize.x(), renderSize.y(), 1, outputBuffer.getAllocation().getHostPtr());
		testOk = verifyResultImage(resultBufferAccess);
	}
	return (testOk == true ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

class DescriptorInlineUniformTestCase: public TestCase
{
	public:
		DescriptorInlineUniformTestCase(tcu::TestContext& context, const char* name, OpsPtr ops);
		virtual ~DescriptorInlineUniformTestCase(void);
		virtual	void initPrograms(vk::SourceCollections& programCollection) const;
		virtual TestInstance* createInstance(Context& context) const;
		void checkSupport(Context& context) const;
	private:
		OpsPtr m_ops;

};

DescriptorInlineUniformTestCase::DescriptorInlineUniformTestCase(tcu::TestContext& context, const char* name, OpsPtr ops)
: vkt::TestCase(context, name)
, m_ops(ops)
{
}

DescriptorInlineUniformTestCase::~DescriptorInlineUniformTestCase(void)
{
}

TestInstance* DescriptorInlineUniformTestCase::createInstance(Context& context) const
{
	TestInstance* instance = new DescriptorInlineUniformTestInstance(context, m_ops);
	return instance;
}

void DescriptorInlineUniformTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
}

void DescriptorInlineUniformTestCase::initPrograms(vk::SourceCollections& programCollection) const
{
	std::ostringstream vert;
	{
		vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "void main()\n"
			<< "{\n"
			<< "    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			<< "                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			<< "}\n";
	}
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	std::ostringstream frag;
	{
		frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
		frag << "#extension GL_EXT_debug_printf : enable\n";

		for (uint32_t k = 0; k < m_ops->getNumDescriptors(); k++)
		{
			const IubPtr iub = m_ops->getDescriptor(k);
			frag << "layout(set=" << iub->getSet() << ", binding=" << iub->getBinding() << ") uniform Iub" << k <<"\n";
			frag << "{\n";
			for (uint32_t m = 1, n = 0; n < iub->getSize(); n += inlineUniformBlockMinimumSize, m++)
			{
				frag << "    int data" << m << ";\n";
			}
			frag << "} iub" << k << ";\n";
		}

		frag << "layout (location = 0) out vec4 outColor;\n"
			 << "void main()\n"
			 << "{\n"
			 << "    int result = 1;\n";

		for (uint32_t k = 0; k <  m_ops->getNumDescriptors(); k++)
		{
			const IubPtr iub = m_ops->getDescriptor(k);
			for (uint32_t m = 1, n = 0; n < iub->getSize(); n += inlineUniformBlockMinimumSize, m++)
			{
				if (iub->getStatus(m-1) != UPD_STATUS_NONE)
				{
					frag << "    if(iub" << k << ".data" << m << " != " << iub->getVerificationData(m-1) << ")"; frag << " result = 0;\n";
					//frag << "	 debugPrintfEXT(\"data is %i, result is %i\\n\", " << "iub" << k << ".data" << m << ", result);\n";
				}
			}
		}

		frag << "    if (result == 1)\n"
			 << "        outColor = vec4(0, 1, 0, 1);\n"
			 << "    else\n"
			 << "        outColor = vec4(1, 0, 1, 0);\n"
			 << "}\n";
	}
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void createInlineUniformWriteTests(tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup>& group)
{
	uint32_t uniqueID = 1;
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr desc1 =  ops->addDescriptor(0u, 0u, 4u, uniqueID);
		ops->writeDescriptor(desc1, 0u, 4u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "write_size_4", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr desc1 =  ops->addDescriptor(0u, 0u, 8u, uniqueID);
		ops->writeDescriptor(desc1, 0u, 8u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "write_size_8", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr desc1 =  ops->addDescriptor(0u, 0u, 16u, uniqueID);
		ops->writeDescriptor(desc1, 0u, 16u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "write_size_16", ops));
	}

	{
		OpsPtr ops(new DescriptorOps());

		IubPtr desc1 =  ops->addDescriptor(0u, 0u, 16u, uniqueID);
		ops->writeDescriptor(desc1, 4u, 8u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "write_offset_nonzero", ops));
	}

}

void createInlineUniformCopyTests(tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup>& group)
{
	uint32_t uniqueID = 1;
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr fromDesc1 =  ops->addDescriptor(0u, 0u, 4u, uniqueID);
		IubPtr toDesc1 =  ops->addDescriptor(0u, 1u, 4u, uniqueID);
		ops->copyDescriptor(fromDesc1, toDesc1, 0u, 0u, 4u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "copy_size_4", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr fromDesc1 =  ops->addDescriptor(0u, 0u, 8u, uniqueID);
		IubPtr toDesc1 =  ops->addDescriptor(0u, 1u, 8u, uniqueID);
		ops->copyDescriptor(fromDesc1, toDesc1, 0u, 0u, 8u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "copy_size_8", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr fromDesc1 =  ops->addDescriptor(0u, 0u, 16u, uniqueID);
		IubPtr toDesc1 =  ops->addDescriptor(0u, 1u, 16u, uniqueID);
		ops->copyDescriptor(fromDesc1, toDesc1, 0u, 0u, 16u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "copy_size_16", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr fromDesc1 =  ops->addDescriptor(0u, 0u, 16u, uniqueID);
		IubPtr toDesc1 =  ops->addDescriptor(0u, 1u, 16u, uniqueID);
		ops->copyDescriptor(fromDesc1, toDesc1, 0u, 4u, 8u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "copy_at_offset_nonzero", ops));
	}
	{
		OpsPtr ops(new DescriptorOps());

		IubPtr fromDesc1 =  ops->addDescriptor(0u, 0u, 16u, uniqueID);
		IubPtr toDesc1 =  ops->addDescriptor(0u, 1u, 16u, uniqueID);
		ops->copyDescriptor(fromDesc1, toDesc1, 4u, 0u, 8u);
		group->addChild(new DescriptorInlineUniformTestCase(testCtx, "copy_from_offset_nonzero", ops));
	}
}

}

tcu::TestCaseGroup*	createDescriptorInlineUniformTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> iubGroup(new tcu::TestCaseGroup(testCtx, "inline_uniform_blocks"));
	createInlineUniformWriteTests(testCtx, iubGroup);
	createInlineUniformCopyTests(testCtx, iubGroup);
	return iubGroup.release();
}
#endif

}	// BindingModel
}	// vkt
