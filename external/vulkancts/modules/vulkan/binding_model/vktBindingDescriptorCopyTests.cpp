/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google Inc.
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
 * \brief Tests for descriptor copying
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorCopyTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkBarrierUtil.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;
using tcu::Vec2;
using tcu::Vec4;

enum PipelineType
{
    PIPELINE_TYPE_COMPUTE  = 0,
    PIPELINE_TYPE_GRAPHICS = 1
};

struct DescriptorCopy
{
    uint32_t srcSet;
    uint32_t srcBinding;
    uint32_t srcArrayElement;
    uint32_t dstSet;
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
};

struct DescriptorData
{
    vector<uint32_t> data; // The actual data. One element per dynamic offset.
    bool written;          // Is the data written in descriptor update
    bool copiedInto;       // Is the data being overwritten by a copy operation
};

typedef de::SharedPtr<ImageWithMemory> ImageWithMemorySp;
typedef de::SharedPtr<Unique<VkImageView>> VkImageViewSp;
typedef de::SharedPtr<Unique<VkBufferView>> VkBufferViewSp;
typedef de::SharedPtr<Unique<VkSampler>> VkSamplerSp;
typedef de::SharedPtr<Unique<VkDescriptorSetLayout>> VkDescriptorSetLayoutSp;

const tcu::IVec2 renderSize(64, 64);

// Base class for descriptors
class Descriptor
{
public:
    Descriptor(VkDescriptorType descriptorType, uint32_t arraySize = 1u, uint32_t writeStart = 0u,
               uint32_t elementsToWrite = 1u, uint32_t numDynamicAreas = 1u);
    virtual ~Descriptor(void);
    VkDescriptorType getType(void) const
    {
        return m_descriptorType;
    }
    uint32_t getArraySize(void) const
    {
        return m_arraySize;
    }
    virtual VkWriteDescriptorSet getDescriptorWrite(void)          = 0;
    virtual string getShaderDeclaration(void) const                = 0;
    virtual void init(Context &context, PipelineType pipelineType) = 0;
    virtual void copyValue(const Descriptor &src, uint32_t srcElement, uint32_t dstElement, uint32_t numElements);
    virtual void invalidate(Context &context)
    {
        DE_UNREF(context);
    }
    virtual vector<uint32_t> getData(void)
    {
        DE_FATAL("Unexpected");
        return vector<uint32_t>();
    }
    uint32_t getId(void) const
    {
        return m_id;
    }
    virtual string getShaderVerifyCode(void) const = 0;
    string getArrayString(uint32_t index) const;
    uint32_t getFirstWrittenElement(void) const;
    uint32_t getNumWrittenElements(void) const;
    uint32_t getReferenceData(uint32_t arrayIdx, uint32_t dynamicAreaIdx = 0) const
    {
        return m_data[arrayIdx].data[dynamicAreaIdx];
    }
    virtual bool isDynamic(void) const
    {
        return false;
    }
    virtual void setDynamicAreas(vector<uint32_t> dynamicAreas)
    {
        DE_UNREF(dynamicAreas);
    }
    virtual vector<VkImageViewSp> getImageViews(void) const
    {
        return vector<VkImageViewSp>();
    }
    virtual vector<VkAttachmentReference> getAttachmentReferences(void) const
    {
        return vector<VkAttachmentReference>();
    }

    static uint32_t s_nextId;

protected:
    VkDescriptorType m_descriptorType;
    uint32_t m_arraySize;
    uint32_t m_id;
    vector<DescriptorData> m_data;
    uint32_t m_numDynamicAreas;
};

typedef de::SharedPtr<Descriptor> DescriptorSp;

// Base class for all buffer based descriptors
class BufferDescriptor : public Descriptor
{
public:
    BufferDescriptor(VkDescriptorType type, uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                     uint32_t numDynamicAreas = 1u);
    virtual ~BufferDescriptor(void);
    void init(Context &context, PipelineType pipelineType);

    VkWriteDescriptorSet getDescriptorWrite(void);
    virtual string getShaderDeclaration(void) const = 0;
    void invalidate(Context &context);
    vector<uint32_t> getData(void);
    virtual string getShaderVerifyCode(void) const             = 0;
    virtual VkBufferUsageFlags getBufferUsageFlags(void) const = 0;
    virtual bool usesBufferView(void)
    {
        return false;
    }

private:
    vector<VkDescriptorBufferInfo> m_descriptorBufferInfos;
    de::MovePtr<BufferWithMemory> m_buffer;
    uint32_t m_bufferSize;
    vector<VkBufferViewSp> m_bufferViews;
    vector<VkBufferView> m_bufferViewHandles;
};

#ifndef CTS_USES_VULKANSC
// Inline uniform block descriptor.
class InlineUniformBlockDescriptor : public Descriptor
{
public:
    InlineUniformBlockDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                 uint32_t numDynamicAreas = 1u);
    virtual ~InlineUniformBlockDescriptor(void);
    void init(Context &context, PipelineType pipelineType);

    VkWriteDescriptorSet getDescriptorWrite(void);
    virtual string getShaderDeclaration(void) const;
    virtual string getShaderVerifyCode(void) const;
    virtual bool usesBufferView(void)
    {
        return false;
    }
    uint32_t getElementSizeInBytes(void) const
    {
        return static_cast<uint32_t>(sizeof(decltype(m_blockData)::value_type));
    }
    uint32_t getSizeInBytes(void) const
    {
        return m_blockElements * getElementSizeInBytes();
    }

private:
    // Inline uniform blocks cannot form arrays, so we will reuse the array size to create a data array inside the uniform block as
    // an array of integers. However, with std140, each of those ints will be padded to 16 bytes in the shader. The struct below
    // allows memory to match between the host and the shader.
    struct PaddedUint
    {
        PaddedUint() : value(0)
        {
            deMemset(padding, 0, sizeof(padding));
        }
        PaddedUint(uint32_t value_) : value(value_)
        {
            deMemset(padding, 0, sizeof(padding));
        }
        PaddedUint &operator=(uint32_t value_)
        {
            value = value_;
            return *this;
        }

        uint32_t value;
        uint32_t padding[3];
    };

    vector<PaddedUint> m_blockData;
    VkWriteDescriptorSetInlineUniformBlockEXT m_inlineWrite;
    uint32_t m_blockElements;
    uint32_t m_writeStart;
    uint32_t m_elementsToWrite;
    uint32_t m_writeStartByteOffset;
    uint32_t m_bytesToWrite;
};
#endif

class UniformBufferDescriptor : public BufferDescriptor
{
public:
    UniformBufferDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                            uint32_t numDynamicAreas = 1u);
    virtual ~UniformBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

private:
};

class DynamicUniformBufferDescriptor : public BufferDescriptor
{
public:
    DynamicUniformBufferDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                   uint32_t numDynamicAreas);
    virtual ~DynamicUniformBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    virtual void setDynamicAreas(vector<uint32_t> dynamicAreas)
    {
        m_dynamicAreas = dynamicAreas;
    }
    virtual bool isDynamic(void) const
    {
        return true;
    }

private:
    vector<uint32_t> m_dynamicAreas;
};

class StorageBufferDescriptor : public BufferDescriptor
{
public:
    StorageBufferDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                            uint32_t numDynamicAreas = 1u);
    virtual ~StorageBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

private:
};

class DynamicStorageBufferDescriptor : public BufferDescriptor
{
public:
    DynamicStorageBufferDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                   uint32_t numDynamicAreas);
    virtual ~DynamicStorageBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    virtual void setDynamicAreas(vector<uint32_t> dynamicAreas)
    {
        m_dynamicAreas = dynamicAreas;
    }
    virtual bool isDynamic(void) const
    {
        return true;
    }

private:
    vector<uint32_t> m_dynamicAreas;
};

class UniformTexelBufferDescriptor : public BufferDescriptor
{
public:
    UniformTexelBufferDescriptor(uint32_t arraySize = 1, uint32_t writeStart = 0, uint32_t elementsToWrite = 1,
                                 uint32_t numDynamicAreas = 1);
    virtual ~UniformTexelBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }
    bool usesBufferView(void)
    {
        return true;
    }

private:
};

class StorageTexelBufferDescriptor : public BufferDescriptor
{
public:
    StorageTexelBufferDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                                 uint32_t numDynamicAreas = 1u);
    virtual ~StorageTexelBufferDescriptor(void);

    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkBufferUsageFlags getBufferUsageFlags(void) const
    {
        return VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }
    bool usesBufferView(void)
    {
        return true;
    }

private:
};

// Base class for all image based descriptors
class ImageDescriptor : public Descriptor
{
public:
    ImageDescriptor(VkDescriptorType type, uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                    uint32_t numDynamicAreas);
    virtual ~ImageDescriptor(void);
    void init(Context &context, PipelineType pipelineType);

    VkWriteDescriptorSet getDescriptorWrite(void);
    virtual VkImageUsageFlags getImageUsageFlags(void) const = 0;
    virtual string getShaderDeclaration(void) const          = 0;
    virtual string getShaderVerifyCode(void) const           = 0;
    virtual VkAccessFlags getAccessFlags(void) const
    {
        return VK_ACCESS_SHADER_READ_BIT;
    }
    virtual VkImageLayout getImageLayout(void) const
    {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

protected:
    vector<VkImageViewSp> m_imageViews;

private:
    vector<ImageWithMemorySp> m_images;
    vector<VkDescriptorImageInfo> m_descriptorImageInfos;
    Move<VkSampler> m_sampler;
};

class InputAttachmentDescriptor : public ImageDescriptor
{
public:
    InputAttachmentDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                              uint32_t numDynamicAreas = 1u);
    virtual ~InputAttachmentDescriptor(void);

    VkImageUsageFlags getImageUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    vector<VkImageViewSp> getImageViews(void) const
    {
        return m_imageViews;
    }
    void copyValue(const Descriptor &src, uint32_t srcElement, uint32_t dstElement, uint32_t numElements);
    VkAccessFlags getAccessFlags(void) const
    {
        return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }
    vector<VkAttachmentReference> getAttachmentReferences(void) const;
    static uint32_t s_nextAttachmentIndex;

private:
    vector<uint32_t> m_attachmentIndices;
    uint32_t m_originalAttachmentIndex;
};

class CombinedImageSamplerDescriptor : public ImageDescriptor
{
public:
    CombinedImageSamplerDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                                   uint32_t numDynamicAreas = 1u);
    virtual ~CombinedImageSamplerDescriptor(void);

    VkImageUsageFlags getImageUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;

private:
};

class SamplerDescriptor;

class SampledImageDescriptor : public ImageDescriptor
{
public:
    SampledImageDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                           uint32_t numDynamicAreas = 1u);
    virtual ~SampledImageDescriptor(void);

    VkImageUsageFlags getImageUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    void addSampler(SamplerDescriptor *sampler, uint32_t count = 1u)
    {
        for (uint32_t i = 0; i < count; i++)
            m_samplers.push_back(sampler);
    }

private:
    vector<SamplerDescriptor *> m_samplers;
};

class SamplerDescriptor : public Descriptor
{
public:
    SamplerDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                      uint32_t numDynamicAreas = 1u);
    virtual ~SamplerDescriptor(void);
    void init(Context &context, PipelineType pipelineType);

    void addImage(SampledImageDescriptor *image, uint32_t count = 1u)
    {
        for (uint32_t i = 0; i < count; i++)
            m_images.push_back(image);
    }
    VkWriteDescriptorSet getDescriptorWrite(void);
    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;

private:
    vector<VkSamplerSp> m_samplers;
    vector<VkDescriptorImageInfo> m_descriptorImageInfos;
    vector<SampledImageDescriptor *> m_images;
};

class StorageImageDescriptor : public ImageDescriptor
{
public:
    StorageImageDescriptor(uint32_t arraySize = 1u, uint32_t writeStart = 0u, uint32_t elementsToWrite = 1u,
                           uint32_t numDynamicAreas = 1u);
    virtual ~StorageImageDescriptor(void);

    VkImageUsageFlags getImageUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    string getShaderDeclaration(void) const;
    string getShaderVerifyCode(void) const;
    VkImageLayout getImageLayout(void) const
    {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

private:
};

class DescriptorSet
{
public:
    DescriptorSet(void);
    ~DescriptorSet(void);
    void addBinding(DescriptorSp descriptor);
    const vector<DescriptorSp> getBindings(void) const
    {
        return m_bindings;
    }

private:
    vector<DescriptorSp> m_bindings;
};

typedef de::SharedPtr<DescriptorSet> DescriptorSetSp;

// Class that handles descriptor sets and descriptors bound to those sets. Keeps track of copy operations.
class DescriptorCommands
{
public:
    DescriptorCommands(PipelineType pipelineType, bool useUpdateAfterBind);
    ~DescriptorCommands(void);
    void addDescriptor(DescriptorSp descriptor, uint32_t descriptorSet);
    void copyDescriptor(uint32_t srcSet, uint32_t srcBinding, uint32_t srcArrayElement, uint32_t dstSet,
                        uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount);
    void copyDescriptor(uint32_t srcSet, uint32_t srcBinding, uint32_t dstSet, uint32_t dstBinding)
    {
        copyDescriptor(srcSet, srcBinding, 0u, dstSet, dstBinding, 0u, 1u);
    }
    string getShaderDeclarations(void) const;
    string getDescriptorVerifications(void) const;
    void addResultBuffer(void);
    uint32_t getResultBufferId(void) const
    {
        return m_resultBuffer->getId();
    }
    void setDynamicAreas(vector<uint32_t> areas);
    bool hasDynamicAreas(void) const;
    PipelineType getPipelineType(void) const
    {
        return m_pipelineType;
    }

    void checkSupport(Context &context) const;
    tcu::TestStatus run(Context &context);

protected:
    void updateDescriptorSets(Context &context, const vector<VkDescriptorSet> &descriptorSets);

private:
    PipelineType m_pipelineType;
    bool m_useUpdateAfterBind;
    vector<DescriptorSetSp> m_descriptorSets;
    vector<DescriptorCopy> m_descriptorCopies;
    vector<DescriptorSp> m_descriptors;
    map<VkDescriptorType, uint32_t> m_descriptorCounts;
    DescriptorSp m_resultBuffer;
    vector<uint32_t> m_dynamicAreas;
};

typedef de::SharedPtr<DescriptorCommands> DescriptorCommandsSp;

class DescriptorCopyTestInstance : public TestInstance
{
public:
    DescriptorCopyTestInstance(Context &context, DescriptorCommandsSp commands);
    ~DescriptorCopyTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    DescriptorCommandsSp m_commands;
};

class DescriptorCopyTestCase : public TestCase
{
public:
    DescriptorCopyTestCase(tcu::TestContext &context, const char *name, DescriptorCommandsSp commands);
    virtual ~DescriptorCopyTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    void checkSupport(Context &context) const;

private:
    mutable DescriptorCommandsSp m_commands;
};

uint32_t Descriptor::s_nextId                             = 0xabc; // Random starting point for ID counter
uint32_t InputAttachmentDescriptor::s_nextAttachmentIndex = 0;

Descriptor::Descriptor(VkDescriptorType descriptorType, uint32_t arraySize, uint32_t writeStart,
                       uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : m_descriptorType(descriptorType)
    , m_arraySize(arraySize)
    , m_id(s_nextId++)
    , m_numDynamicAreas(numDynamicAreas)
{
    for (uint32_t arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
    {
        const bool written = arrayIdx >= writeStart && arrayIdx < writeStart + elementsToWrite;
        vector<uint32_t> data;

        for (uint32_t dynamicAreaIdx = 0; dynamicAreaIdx < m_numDynamicAreas; dynamicAreaIdx++)
            data.push_back(m_id + arrayIdx * m_numDynamicAreas + dynamicAreaIdx);

        const DescriptorData descriptorData = {
            data,    // vector<uint32_t>    data
            written, // bool                written
            false    // bool                copiedInto
        };

        m_data.push_back(descriptorData);
    }
}

Descriptor::~Descriptor(void)
{
}

// Copy refrence data from another descriptor
void Descriptor::copyValue(const Descriptor &src, uint32_t srcElement, uint32_t dstElement, uint32_t numElements)
{
    for (uint32_t elementIdx = 0; elementIdx < numElements; elementIdx++)
    {
        DE_ASSERT(src.m_data[elementIdx + srcElement].written);

        for (uint32_t dynamicAreaIdx = 0; dynamicAreaIdx < de::min(m_numDynamicAreas, src.m_numDynamicAreas);
             dynamicAreaIdx++)
            m_data[elementIdx + dstElement].data[dynamicAreaIdx] =
                src.m_data[elementIdx + srcElement].data[dynamicAreaIdx];

        m_data[elementIdx + dstElement].copiedInto = true;
    }
}

string Descriptor::getArrayString(uint32_t index) const
{
    return m_arraySize > 1 ? (string("[") + de::toString(index) + "]") : "";
}

// Returns the first element to be written in descriptor update
uint32_t Descriptor::getFirstWrittenElement(void) const
{
    for (uint32_t i = 0; i < (uint32_t)m_data.size(); i++)
        if (m_data[i].written)
            return i;

    return 0;
}

// Returns the number of array elements to be written for a descriptor array
uint32_t Descriptor::getNumWrittenElements(void) const
{
    uint32_t numElements = 0;

    for (uint32_t i = 0; i < (uint32_t)m_data.size(); i++)
        if (m_data[i].written)
            numElements++;

    return numElements;
}

BufferDescriptor::BufferDescriptor(VkDescriptorType type, uint32_t arraySize, uint32_t writeStart,
                                   uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : Descriptor(type, arraySize, writeStart, elementsToWrite, numDynamicAreas)
    , m_bufferSize(256u * arraySize * numDynamicAreas)
{
}

BufferDescriptor::~BufferDescriptor(void)
{
}

void BufferDescriptor::init(Context &context, PipelineType pipelineType)
{
    DE_UNREF(pipelineType);

    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    Allocator &allocator      = context.getDefaultAllocator();

    // Create buffer
    {
        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
            nullptr,                              // const void*            pNext
            0u,                                   // VkBufferCreateFlags    flags
            m_bufferSize,                         // VkDeviceSize            size
            getBufferUsageFlags(),                // VkBufferUsageFlags    usage
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode
            0u,                                   // uint32_t                queueFamilyIndexCount
            nullptr                               // const uint32_t*        pQueueFamilyIndices
        };

        m_buffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
    }

    // Create descriptor buffer infos
    {
        for (uint32_t arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
        {
            const VkDescriptorBufferInfo bufferInfo = {
                m_buffer->get(),                     // VkBuffer        buffer
                256u * m_numDynamicAreas * arrayIdx, // VkDeviceSize    offset
                isDynamic() ? 256u : 4u              // VkDeviceSize    range
            };

            m_descriptorBufferInfos.push_back(bufferInfo);
        }
    }

    // Create buffer views
    if (usesBufferView())
    {
        for (uint32_t viewIdx = 0; viewIdx < m_arraySize; viewIdx++)
        {
            const VkBufferViewCreateInfo bufferViewCreateInfo = {
                VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, // VkStructureType            sType
                nullptr,                                   // const void*                pNext
                0u,                                        // VkBufferViewCreateFlags    flags
                m_buffer->get(),                           // VkBuffer                    buffer
                VK_FORMAT_R32_SFLOAT,                      // VkFormat                    format
                256u * viewIdx,                            // VkDeviceSize                offset
                4u                                         // VkDeviceSize                range
            };

            m_bufferViews.push_back(
                VkBufferViewSp(new Unique<VkBufferView>(createBufferView(vk, device, &bufferViewCreateInfo))));
            m_bufferViewHandles.push_back(**m_bufferViews[viewIdx]);
        }
    }

    // Initialize buffer memory
    {
        uint32_t *hostPtr = (uint32_t *)m_buffer->getAllocation().getHostPtr();

        for (uint32_t arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
        {
            for (uint32_t dynamicAreaIdx = 0; dynamicAreaIdx < m_numDynamicAreas; dynamicAreaIdx++)
            {
                union BufferValue
                {
                    uint32_t uintValue;
                    float floatValue;
                } bufferValue;

                bufferValue.uintValue = m_id + (arrayIdx * m_numDynamicAreas) + dynamicAreaIdx;

                if (usesBufferView())
                    bufferValue.floatValue = (float)bufferValue.uintValue;

                hostPtr[(256 / 4) * (m_numDynamicAreas * arrayIdx + dynamicAreaIdx)] = bufferValue.uintValue;
            }
        }

        flushAlloc(vk, device, m_buffer->getAllocation());
    }
}

VkWriteDescriptorSet BufferDescriptor::getDescriptorWrite(void)
{
    const uint32_t firstElement = getFirstWrittenElement();

    // Set and binding will be overwritten later
    const VkWriteDescriptorSet descriptorWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
        nullptr,                                // const void*                        pNext
        VK_NULL_HANDLE,                         // VkDescriptorSet                    dstSet
        0u,                                     // uint32_t                            dstBinding
        firstElement,                           // uint32_t                            dstArrayElement
        getNumWrittenElements(),                // uint32_t                            descriptorCount
        getType(),                              // VkDescriptorType                    descriptorType
        nullptr,                                // const VkDescriptorImageInfo        pImageInfo
        usesBufferView() ? nullptr :
                           &m_descriptorBufferInfos[firstElement], // const VkDescriptorBufferInfo*    pBufferInfo
        usesBufferView() ? &m_bufferViewHandles[firstElement] :
                           nullptr // const VkBufferView*                pTexelBufferView
    };

    return descriptorWrite;
}

void BufferDescriptor::invalidate(Context &context)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();

    invalidateAlloc(vk, device, m_buffer->getAllocation());
}

// Returns the buffer data as a vector
vector<uint32_t> BufferDescriptor::getData(void)
{
    vector<uint32_t> data;
    int32_t *hostPtr = (int32_t *)m_buffer->getAllocation().getHostPtr();

    for (uint32_t i = 0; i < m_arraySize; i++)
        data.push_back(hostPtr[i]);

    return data;
}

#ifndef CTS_USES_VULKANSC
// Inline Uniform Block descriptor. These are similar to uniform buffers, but they can't form arrays for spec reasons.
// The array size is reused, instead, as the size of a data array inside the uniform block.
InlineUniformBlockDescriptor::InlineUniformBlockDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                           uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : Descriptor(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, arraySize, writeStart, elementsToWrite, 1u)
    , m_blockElements(arraySize)
    , m_writeStart(writeStart)
    , m_elementsToWrite(elementsToWrite)
    , m_writeStartByteOffset(m_writeStart * getElementSizeInBytes())
    , m_bytesToWrite(m_elementsToWrite * getElementSizeInBytes())
{
    DE_UNREF(numDynamicAreas);
}

InlineUniformBlockDescriptor::~InlineUniformBlockDescriptor(void)
{
}

void InlineUniformBlockDescriptor::init(Context &context, PipelineType pipelineType)
{
    DE_UNREF(context);
    DE_UNREF(pipelineType);

    // Initialize host memory.
    m_blockData.resize(m_blockElements);
    for (uint32_t i = 0; i < m_blockElements; ++i)
        m_blockData[i] = m_id + i;

    // Initialize descriptor write extension structure.
    m_inlineWrite.sType    = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
    m_inlineWrite.pNext    = nullptr;
    m_inlineWrite.dataSize = m_bytesToWrite;
    m_inlineWrite.pData    = &m_blockData[m_writeStart];
}

VkWriteDescriptorSet InlineUniformBlockDescriptor::getDescriptorWrite(void)
{
    // Set and binding will be overwritten later
    const VkWriteDescriptorSet descriptorWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
        &m_inlineWrite,                         // const void*                        pNext
        VK_NULL_HANDLE,                         // VkDescriptorSet                    dstSet
        0u,                                     // uint32_t                            dstBinding
        m_writeStartByteOffset,                 // uint32_t                            dstArrayElement
        m_bytesToWrite,                         // uint32_t                            descriptorCount
        getType(),                              // VkDescriptorType                    descriptorType
        nullptr,                                // const VkDescriptorImageInfo        pImageInfo
        nullptr,                                // const VkDescriptorBufferInfo*    pBufferInfo
        nullptr                                 // const VkBufferView*                pTexelBufferView
    };

    return descriptorWrite;
}

string InlineUniformBlockDescriptor::getShaderDeclaration(void) const
{
    const string idStr = de::toString(m_id);
    return string(") uniform InlineUniformBlock" + idStr +
                  "\n"
                  "{\n"
                  "    int data" +
                  getArrayString(m_arraySize) +
                  ";\n"
                  "} inlineUniformBlock" +
                  idStr + ";\n");
}

string InlineUniformBlockDescriptor::getShaderVerifyCode(void) const
{
    const string idStr = de::toString(m_id);
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
        {
            ret += string("if (inlineUniformBlock") + idStr + ".data" + getArrayString(i) +
                   " != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
        }
    }

    return ret;
}
#endif

UniformBufferDescriptor::UniformBufferDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                                 uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

UniformBufferDescriptor::~UniformBufferDescriptor(void)
{
}

string UniformBufferDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform UniformBuffer" + de::toString(m_id) +
                  "\n"
                  "{\n"
                  "    int data;\n"
                  "} uniformBuffer" +
                  de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string UniformBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (uniformBuffer") + de::toString(m_id) + getArrayString(i) +
                   ".data != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

DynamicUniformBufferDescriptor::DynamicUniformBufferDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                               uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, arraySize, writeStart, elementsToWrite,
                       numDynamicAreas)
{
}

DynamicUniformBufferDescriptor::~DynamicUniformBufferDescriptor(void)
{
}

string DynamicUniformBufferDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform UniformBuffer" + de::toString(m_id) +
                  "\n"
                  "{\n"
                  "    int data;\n"
                  "} dynamicUniformBuffer" +
                  de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string DynamicUniformBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
    {
        if (m_data[arrayIdx].written || m_data[arrayIdx].copiedInto)
            ret += string("if (dynamicUniformBuffer") + de::toString(m_id) + getArrayString(arrayIdx) +
                   ".data != " + de::toString(m_data[arrayIdx].data[m_dynamicAreas[arrayIdx]]) + ") result = 0;\n";
    }

    return ret;
}

StorageBufferDescriptor::StorageBufferDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                                 uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

StorageBufferDescriptor::~StorageBufferDescriptor(void)
{
}

string StorageBufferDescriptor::getShaderDeclaration(void) const
{
    return string(") buffer StorageBuffer" + de::toString(m_id) +
                  "\n"
                  "{\n"
                  "    int data;\n"
                  "} storageBuffer" +
                  de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string StorageBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (storageBuffer") + de::toString(m_id) + getArrayString(i) +
                   ".data != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

DynamicStorageBufferDescriptor::DynamicStorageBufferDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                               uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, arraySize, writeStart, elementsToWrite,
                       numDynamicAreas)
{
}

DynamicStorageBufferDescriptor::~DynamicStorageBufferDescriptor(void)
{
}

string DynamicStorageBufferDescriptor::getShaderDeclaration(void) const
{
    return string(") buffer StorageBuffer" + de::toString(m_id) +
                  "\n"
                  "{\n"
                  "    int data;\n"
                  "} dynamicStorageBuffer" +
                  de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string DynamicStorageBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
    {
        if (m_data[arrayIdx].written || m_data[arrayIdx].copiedInto)
            ret += string("if (dynamicStorageBuffer") + de::toString(m_id) + getArrayString(arrayIdx) +
                   ".data != " + de::toString(m_data[arrayIdx].data[m_dynamicAreas[arrayIdx]]) + ") result = 0;\n";
    }

    return ret;
}

UniformTexelBufferDescriptor::UniformTexelBufferDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                           uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

UniformTexelBufferDescriptor::~UniformTexelBufferDescriptor(void)
{
}

string UniformTexelBufferDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform textureBuffer uniformTexelBuffer" + de::toString(m_id) + getArrayString(m_arraySize) +
                  ";\n");
}

string UniformTexelBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (texelFetch(uniformTexelBuffer") + de::toString(m_id) + getArrayString(i) +
                   ", 0).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

StorageTexelBufferDescriptor::StorageTexelBufferDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                           uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

StorageTexelBufferDescriptor::~StorageTexelBufferDescriptor(void)
{
}

string StorageTexelBufferDescriptor::getShaderDeclaration(void) const
{
    return string(", r32f) uniform imageBuffer storageTexelBuffer" + de::toString(m_id) + getArrayString(m_arraySize) +
                  ";\n");
}

string StorageTexelBufferDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (imageLoad(storageTexelBuffer") + de::toString(m_id) + getArrayString(i) +
                   ", 0).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

ImageDescriptor::ImageDescriptor(VkDescriptorType type, uint32_t arraySize, uint32_t writeStart,
                                 uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : Descriptor(type, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

ImageDescriptor::~ImageDescriptor(void)
{
}

void ImageDescriptor::init(Context &context, PipelineType pipelineType)
{
    const DeviceInterface &vk                 = context.getDeviceInterface();
    const VkDevice device                     = context.getDevice();
    Allocator &allocator                      = context.getDefaultAllocator();
    const VkQueue queue                       = context.getUniversalQueue();
    uint32_t queueFamilyIndex                 = context.getUniversalQueueFamilyIndex();
    const VkFormat format                     = VK_FORMAT_R32_SFLOAT;
    const VkComponentMapping componentMapping = makeComponentMappingRGBA();

    const VkImageSubresourceRange subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
        0u,                        // uint32_t                baseMipLevel
        1u,                        // uint32_t                levelCount
        0u,                        // uint32_t                baseArrayLayer
        1u,                        // uint32_t                layerCount
    };

    // Create sampler
    {
        const tcu::Sampler sampler =
            tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                         tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE, 0,
                         tcu::Vec4(0.0f), true);
        const tcu::TextureFormat texFormat      = mapVkFormat(format);
        const VkSamplerCreateInfo samplerParams = mapSampler(sampler, texFormat);

        m_sampler = createSampler(vk, device, &samplerParams);
    }

    // Create images
    for (uint32_t imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
    {
        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                     // VkStructureType            stype
            nullptr,                                                 // const void*                pNext
            0u,                                                      // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                                        // VkImageType                imageType
            format,                                                  // VkFormat                    format
            {(uint32_t)renderSize.x(), (uint32_t)renderSize.y(), 1}, // VkExtent3D                extent
            1u,                                                      // uint32_t                    mipLevels
            1u,                                                      // uint32_t                    arrayLayers
            VK_SAMPLE_COUNT_1_BIT,                                   // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,                                 // VkImageTiling            tiling
            getImageUsageFlags(),                                    // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,                               // VkSharingMode            sharingMode
            1u,                        // uint32_t                    queueFamilyIndexCount
            &queueFamilyIndex,         // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout            initialLayout
        };

        m_images.push_back(
            ImageWithMemorySp(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any)));
    }

    // Create image views
    for (uint32_t imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
    {
        const VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType            sType
            nullptr,                                  // const void*                pNext
            0u,                                       // VkImageViewCreateFlags    flags
            **m_images[imageIdx],                     // VkImage                    image
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType            viewType
            format,                                   // VkFormat                    format
            componentMapping,                         // VkComponentMapping        components
            subresourceRange                          // VkImageSubresourceRange    subresourceRange
        };

        m_imageViews.push_back(
            VkImageViewSp(new Unique<VkImageView>(createImageView(vk, device, &imageViewCreateInfo))));
    }

    // Create descriptor image infos
    {
        for (uint32_t i = 0; i < m_arraySize; i++)
        {
            const VkDescriptorImageInfo imageInfo = {
                *m_sampler,        // VkSampler        sampler
                **m_imageViews[i], // VkImageView        imageView
                getImageLayout()   // VkImageLayout    imageLayout
            };

            m_descriptorImageInfos.push_back(imageInfo);
        }
    }

    // Clear images to reference value
    for (uint32_t imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
    {
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        const float clearValue        = (float)(m_id + imageIdx);
        const VkClearValue clearColor = makeClearValueColorF32(clearValue, clearValue, clearValue, clearValue);

        const VkImageMemoryBarrier preImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
            nullptr,                                // const void*                pNext
            0u,                                     // VkAccessFlags            srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags            dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout            oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout            newLayout
            queueFamilyIndex,                       // uint32_t                    srcQueueFamilyIndex
            queueFamilyIndex,                       // uint32_t                    dstQueueFamilyIndex
            **m_images[imageIdx],                   // VkImage                    image
            subresourceRange                        // VkImageSubresourceRange    subresourceRange
        };

        const VkImageMemoryBarrier postImageBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
            nullptr,                                // const void*                pNext
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags            srcAccessMask
            getAccessFlags(),                       // VkAccessFlags            dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout            oldLayout
            getImageLayout(),                       // VkImageLayout            newLayout
            queueFamilyIndex,                       // uint32_t                    srcQueueFamilyIndex
            queueFamilyIndex,                       // uint32_t                    dstQueueFamilyIndex
            **m_images[imageIdx],                   // VkImage                    image
            subresourceRange                        // VkImageSubresourceRange    subresourceRange
        };

        beginCommandBuffer(vk, *cmdBuffer);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u, &preImageBarrier);
        vk.cmdClearColorImage(*cmdBuffer, **m_images[imageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color,
                              1, &subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              pipelineType == PIPELINE_TYPE_COMPUTE ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                                                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              (VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u, &postImageBarrier);
        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }
}

VkWriteDescriptorSet ImageDescriptor::getDescriptorWrite(void)
{
    const uint32_t firstElement = getFirstWrittenElement();

    // Set and binding will be overwritten later
    const VkWriteDescriptorSet descriptorWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
        nullptr,                                // const void*                        pNext
        VK_NULL_HANDLE,                         // VkDescriptorSet                    dstSet
        0u,                                     // uint32_t                            dstBinding
        firstElement,                           // uint32_t                            dstArrayElement
        getNumWrittenElements(),                // uint32_t                            descriptorCount
        getType(),                              // VkDescriptorType                    descriptorType
        &m_descriptorImageInfos[firstElement],  // const VkDescriptorImageInfo        pImageInfo
        nullptr,                                // const VkDescriptorBufferInfo*    pBufferInfo
        nullptr                                 // const VkBufferView*                pTexelBufferView
    };

    return descriptorWrite;
}

InputAttachmentDescriptor::InputAttachmentDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                                     uint32_t numDynamicAreas)
    : ImageDescriptor(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, arraySize, writeStart, elementsToWrite, 1u)
    , m_originalAttachmentIndex(s_nextAttachmentIndex)
{
    DE_UNREF(numDynamicAreas);

    for (uint32_t i = 0; i < m_arraySize; i++)
        m_attachmentIndices.push_back(s_nextAttachmentIndex++);
}

InputAttachmentDescriptor::~InputAttachmentDescriptor(void)
{
}

string InputAttachmentDescriptor::getShaderDeclaration(void) const
{
    return string(", input_attachment_index=" + de::toString(m_originalAttachmentIndex) +
                  ") uniform subpassInput inputAttachment" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string InputAttachmentDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (subpassLoad(inputAttachment") + de::toString(m_id) + getArrayString(i) +
                   ").x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

void InputAttachmentDescriptor::copyValue(const Descriptor &src, uint32_t srcElement, uint32_t dstElement,
                                          uint32_t numElements)
{
    Descriptor::copyValue(src, srcElement, dstElement, numElements);

    for (uint32_t elementIdx = 0; elementIdx < numElements; elementIdx++)
    {
        m_attachmentIndices[elementIdx + dstElement] =
            reinterpret_cast<const InputAttachmentDescriptor &>(src).m_attachmentIndices[elementIdx + srcElement];
    }
}

vector<VkAttachmentReference> InputAttachmentDescriptor::getAttachmentReferences(void) const
{
    vector<VkAttachmentReference> references;
    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        const VkAttachmentReference attachmentReference = {
            // The first attachment is the color buffer, thus +1
            m_attachmentIndices[i] + 1, // uint32_t            attachment
            getImageLayout()            // VkImageLayout    layout
        };

        references.push_back(attachmentReference);
    }

    return references;
}

CombinedImageSamplerDescriptor::CombinedImageSamplerDescriptor(uint32_t arraySize, uint32_t writeStart,
                                                               uint32_t elementsToWrite, uint32_t numDynamicAreas)
    : ImageDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

CombinedImageSamplerDescriptor::~CombinedImageSamplerDescriptor(void)
{
}

string CombinedImageSamplerDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform sampler2D texSampler" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string CombinedImageSamplerDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (texture(texSampler") + de::toString(m_id) + getArrayString(i) +
                   ", vec2(0)).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

SampledImageDescriptor::SampledImageDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                               uint32_t numDynamicAreas)
    : ImageDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

SampledImageDescriptor::~SampledImageDescriptor(void)
{
}

string SampledImageDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform texture2D sampledImage" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string SampledImageDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if ((m_data[i].written || m_data[i].copiedInto) && m_samplers.size() > i)
        {
            ret += string("if (texture(sampler2D(sampledImage") + de::toString(m_id) + getArrayString(i) + ", sampler" +
                   de::toString(m_samplers[i]->getId()) + "), vec2(0)).x != " + de::toString(m_data[i].data[0]) +
                   ") result = 0;\n";
        }
    }

    return ret;
}

StorageImageDescriptor::StorageImageDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                               uint32_t numDynamicAreas)
    : ImageDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

StorageImageDescriptor::~StorageImageDescriptor(void)
{
}

string StorageImageDescriptor::getShaderDeclaration(void) const
{
    return string(", r32f) readonly uniform image2D image" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string StorageImageDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if (m_data[i].written || m_data[i].copiedInto)
            ret += string("if (imageLoad(image") + de::toString(m_id) + getArrayString(i) +
                   ", ivec2(0)).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
    }

    return ret;
}

SamplerDescriptor::SamplerDescriptor(uint32_t arraySize, uint32_t writeStart, uint32_t elementsToWrite,
                                     uint32_t numDynamicAreas)
    : Descriptor(VK_DESCRIPTOR_TYPE_SAMPLER, arraySize, writeStart, elementsToWrite, 1u)
{
    DE_UNREF(numDynamicAreas);
}

SamplerDescriptor::~SamplerDescriptor(void)
{
}

void SamplerDescriptor::init(Context &context, PipelineType pipelineType)
{
    DE_UNREF(pipelineType);

    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    const VkFormat format     = VK_FORMAT_R32_SFLOAT;

    // Create samplers
    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        const float borderValue = (float)((m_id + i) % 2);
        const tcu::Sampler sampler =
            tcu::Sampler(tcu::Sampler::CLAMP_TO_BORDER, tcu::Sampler::CLAMP_TO_BORDER, tcu::Sampler::CLAMP_TO_BORDER,
                         tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE, 0,
                         Vec4(borderValue), true);
        const tcu::TextureFormat texFormat      = mapVkFormat(format);
        const VkSamplerCreateInfo samplerParams = mapSampler(sampler, texFormat);

        m_samplers.push_back(VkSamplerSp(new Unique<VkSampler>(createSampler(vk, device, &samplerParams))));
    }

    // Create descriptor image infos
    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        const VkDescriptorImageInfo imageInfo = {
            **m_samplers[i],          // VkSampler        sampler
            VK_NULL_HANDLE,           // VkImageView        imageView
            VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout    imageLayout
        };

        m_descriptorImageInfos.push_back(imageInfo);
    }
}

VkWriteDescriptorSet SamplerDescriptor::getDescriptorWrite(void)
{
    const uint32_t firstElement = getFirstWrittenElement();

    // Set and binding will be overwritten later
    const VkWriteDescriptorSet descriptorWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                    sType
        nullptr,                                // const void*                        pNext
        VK_NULL_HANDLE,                         // VkDescriptorSet                    dstSet
        0u,                                     // uint32_t                            dstBinding
        firstElement,                           // uint32_t                            dstArrayElement
        getNumWrittenElements(),                // uint32_t                            descriptorCount
        getType(),                              // VkDescriptorType                    descriptorType
        &m_descriptorImageInfos[firstElement],  // const VkDescriptorImageInfo        pImageInfo
        nullptr,                                // const VkDescriptorBufferInfo*    pBufferInfo
        nullptr                                 // const VkBufferView*                pTexelBufferView
    };

    return descriptorWrite;
}

string SamplerDescriptor::getShaderDeclaration(void) const
{
    return string(") uniform sampler sampler" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string SamplerDescriptor::getShaderVerifyCode(void) const
{
    string ret;

    for (uint32_t i = 0; i < m_arraySize; i++)
    {
        if ((m_data[i].written || m_data[i].copiedInto) && m_images.size() > i)
        {
            // Sample from (-1, -1) to get border color.
            ret += string("if (texture(sampler2D(sampledImage") + de::toString(m_images[i]->getId()) + ", sampler" +
                   de::toString(m_id) + getArrayString(i) + "), vec2(-1)).x != " + de::toString(m_data[i].data[0] % 2) +
                   ") result = 0;\n";
        }
    }

    return ret;
}

DescriptorSet::DescriptorSet(void)
{
}

DescriptorSet::~DescriptorSet(void)
{
}

void DescriptorSet::addBinding(DescriptorSp descriptor)
{
    m_bindings.push_back(descriptor);
}

DescriptorCommands::DescriptorCommands(PipelineType pipelineType, bool useUpdateAfterBind)
    : m_pipelineType(pipelineType)
    , m_useUpdateAfterBind(useUpdateAfterBind)
{
    // Reset counters
    Descriptor::s_nextId                             = 0xabc;
    InputAttachmentDescriptor::s_nextAttachmentIndex = 0;
}

DescriptorCommands::~DescriptorCommands(void)
{
}

void DescriptorCommands::addDescriptor(DescriptorSp descriptor, uint32_t descriptorSet)
{
    const VkDescriptorType type = descriptor->getType();

    // Create descriptor set objects until one with the given index exists
    while (m_descriptorSets.size() <= descriptorSet)
        m_descriptorSets.push_back(DescriptorSetSp(new DescriptorSet()));

    m_descriptorSets[descriptorSet]->addBinding(descriptor);

    // Keep track of how many descriptors of each type is needed. Inline uniform blocks cannot form arrays. We reuse the array size
    // as size of the data array for them, within a single descriptor.

#ifndef CTS_USES_VULKANSC
    const uint32_t count = ((type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 1u : descriptor->getArraySize());
#else
    const uint32_t count = descriptor->getArraySize();
#endif

    if (m_descriptorCounts.find(type) != m_descriptorCounts.end())
        m_descriptorCounts[type] += count;
    else
        m_descriptorCounts[type] = count;

    // Keep descriptors also in a flat list for easier iteration
    m_descriptors.push_back(descriptor);
}

void DescriptorCommands::copyDescriptor(uint32_t srcSet, uint32_t srcBinding, uint32_t srcArrayElement, uint32_t dstSet,
                                        uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount)
{
    // For inline uniform blocks, (src|dst)ArrayElement are data array indices and descriptorCount is the number of integers to copy.
    DescriptorCopy descriptorCopy = {srcSet,     srcBinding,      srcArrayElement, dstSet,
                                     dstBinding, dstArrayElement, descriptorCount};

#ifndef CTS_USES_VULKANSC
    if (m_descriptorSets[srcSet]->getBindings()[srcBinding]->getType() == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
        // For inline uniform blocks, these members of VkCopyDescriptorSet are offsets and sizes in bytes.
        const InlineUniformBlockDescriptor *iub =
            static_cast<InlineUniformBlockDescriptor *>(m_descriptorSets[srcSet]->getBindings()[srcBinding].get());
        const uint32_t elementSize = iub->getElementSizeInBytes();

        descriptorCopy.srcArrayElement *= elementSize;
        descriptorCopy.dstArrayElement *= elementSize;
        descriptorCopy.descriptorCount *= elementSize;
    }
#endif

    m_descriptorCopies.push_back(descriptorCopy);
    m_descriptorSets[descriptorCopy.dstSet]->getBindings()[descriptorCopy.dstBinding]->copyValue(
        *m_descriptorSets[descriptorCopy.srcSet]->getBindings()[descriptorCopy.srcBinding], srcArrayElement,
        dstArrayElement, descriptorCount);
}

// Generates shader source code for declarations of all descriptors
string DescriptorCommands::getShaderDeclarations(void) const
{
    string ret;

    for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
    {
        const vector<DescriptorSp> bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

        for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
        {
            ret += "layout (set=" + de::toString(descriptorSetIdx) + ", binding=" + de::toString(bindingIdx) +
                   bindings[bindingIdx]->getShaderDeclaration();
        }
    }

    return ret;
}

// Generates shader source code for verification of all descriptor data
string DescriptorCommands::getDescriptorVerifications(void) const
{
    string ret;

    for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
    {
        const vector<DescriptorSp> bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

        for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
        {
            if (m_pipelineType == PIPELINE_TYPE_COMPUTE && descriptorSetIdx == 0 && bindingIdx == bindings.size() - 1)
                continue; // Skip the result buffer which is always the last descriptor of set 0
            ret += bindings[bindingIdx]->getShaderVerifyCode();
        }
    }

    return ret;
}

void DescriptorCommands::addResultBuffer(void)
{
    // Add result buffer if using compute pipeline
    if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
    {
        m_resultBuffer = DescriptorSp(new StorageBufferDescriptor());
        addDescriptor(m_resultBuffer, 0u);
    }
}

// Sets the list of dynamic areas selected for each dynamic descriptor when running the verification shader
void DescriptorCommands::setDynamicAreas(vector<uint32_t> areas)
{
    m_dynamicAreas   = areas;
    uint32_t areaIdx = 0;

    for (vector<DescriptorSp>::iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
    {
        if ((*desc)->isDynamic())
        {
            vector<uint32_t> dynamicAreas;

            for (uint32_t elementIdx = 0; elementIdx < (*desc)->getArraySize(); elementIdx++)
                dynamicAreas.push_back(areas[areaIdx++]);

            (*desc)->setDynamicAreas(dynamicAreas);
        }
    }
}

bool DescriptorCommands::hasDynamicAreas(void) const
{
    for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
        if ((*desc)->isDynamic())
            return true;

    return false;
}

void DescriptorCommands::checkSupport(Context &context) const
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const VkPhysicalDeviceLimits limits   = getPhysicalDeviceProperties(vki, physicalDevice).limits;

    if (limits.maxBoundDescriptorSets <= m_descriptorSets.size())
        TCU_THROW(NotSupportedError, "Maximum bound descriptor sets limit exceeded.");

    if (m_useUpdateAfterBind)
        context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");

#ifndef CTS_USES_VULKANSC
    uint32_t numTotalIUBs = 0;

    // Check if inline uniform blocks are supported.
    VkPhysicalDeviceInlineUniformBlockFeaturesEXT iubFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT, nullptr, VK_FALSE, VK_FALSE};
    VkPhysicalDeviceInlineUniformBlockPropertiesEXT iubProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT, nullptr, 0u, 0u, 0u, 0u, 0u};

    if (context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
    {
        VkPhysicalDeviceFeatures2 features2 = initVulkanStructure(&iubFeatures);
        vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

        VkPhysicalDeviceProperties2 properties2 = initVulkanStructure(&iubProperties);
        vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
    }
#endif

    // Check physical device limits of per stage and per desriptor set descriptor count
    {
        uint32_t numPerStageSamplers         = 0;
        uint32_t numPerStageUniformBuffers   = 0;
        uint32_t numPerStageStorageBuffers   = 0;
        uint32_t numPerStageSampledImages    = 0;
        uint32_t numPerStageStorageImages    = 0;
        uint32_t numPerStageInputAttachments = 0;
        uint32_t numPerStageTotalResources   = 0;

        bool usesUniformBuffer      = false;
        bool usesSampledImage       = false;
        bool usesStorageImage       = false;
        bool usesStorageBuffer      = false;
        bool usesUniformTexelBuffer = false;
        bool usesStorageTexelBuffer = false;

        for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
        {
            uint32_t numSamplers              = 0;
            uint32_t numUniformBuffers        = 0;
            uint32_t numUniformBuffersDynamic = 0;
            uint32_t numStorageBuffers        = 0;
            uint32_t numStorageBuffersDynamic = 0;
            uint32_t numSampledImages         = 0;
            uint32_t numStorageImages         = 0;
            uint32_t numInputAttachments      = 0;
            uint32_t numTotalResources =
                m_pipelineType == PIPELINE_TYPE_GRAPHICS ? 1u : 0u; // Color buffer counts as a resource.

            const vector<DescriptorSp> &bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

            for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
            {
                const uint32_t arraySize = bindings[bindingIdx]->getArraySize();

#ifndef CTS_USES_VULKANSC
                // Inline uniform blocks cannot form arrays. The array size is the size of the data array in the descriptor.
                if (bindings[bindingIdx]->getType() == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                {
                    const InlineUniformBlockDescriptor *iub =
                        static_cast<InlineUniformBlockDescriptor *>(bindings[bindingIdx].get());
                    const uint32_t bytes = iub->getSizeInBytes();

                    // Check inline uniform block size.
                    if (bytes > iubProperties.maxInlineUniformBlockSize)
                    {
                        std::ostringstream msg;
                        msg << "Maximum size for an inline uniform block exceeded by binding " << bindingIdx
                            << " from set " << descriptorSetIdx;
                        TCU_THROW(NotSupportedError, msg.str().c_str());
                    }

                    ++numTotalResources;
                }
                else
#endif
                {
                    numTotalResources += arraySize;
                }

                switch (bindings[bindingIdx]->getType())
                {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    numUniformBuffers += arraySize;
                    usesUniformBuffer = true;
                    break;

                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    numUniformBuffers += arraySize;
                    numUniformBuffersDynamic += arraySize;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    numStorageBuffers += arraySize;
                    usesStorageBuffer = true;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    numStorageBuffers += arraySize;
                    numStorageBuffersDynamic += arraySize;
                    break;

                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    numSamplers += arraySize;
                    numSampledImages += arraySize;
                    usesSampledImage = true;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    numStorageImages += arraySize;
                    usesStorageImage = true;
                    break;

                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    numStorageImages += arraySize;
                    usesStorageTexelBuffer = true;
                    break;

                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    numInputAttachments += arraySize;
                    break;

                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    numSampledImages += arraySize;
                    usesSampledImage = true;
                    break;

                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                    numSampledImages += arraySize;
                    usesUniformTexelBuffer = true;
                    break;

                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    numSamplers += arraySize;
                    usesSampledImage = true;
                    break;

#ifndef CTS_USES_VULKANSC
                case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
                    ++numTotalIUBs;
                    break;
#endif
                default:
                    DE_FATAL("Unexpected descriptor type");
                    break;
                }
            }

            if (numSamplers > limits.maxDescriptorSetSamplers)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set sampler limit exceeded.");

            if (numUniformBuffers > limits.maxDescriptorSetUniformBuffers)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set uniform buffer limit exceeded.");

            if (numUniformBuffersDynamic > limits.maxDescriptorSetUniformBuffersDynamic)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set uniform buffer dynamic limit exceeded.");

            if (numStorageBuffers > limits.maxDescriptorSetStorageBuffers)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set storage buffer limit exceeded.");

            if (numStorageBuffersDynamic > limits.maxDescriptorSetStorageBuffersDynamic)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set storage buffer dynamic limit exceeded.");

            if (numSampledImages > limits.maxDescriptorSetSampledImages)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set sampled image limit exceeded.");

            if (numStorageImages > limits.maxDescriptorSetStorageImages)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set storage image limit exceeded.");

            if (numInputAttachments > limits.maxDescriptorSetInputAttachments)
                TCU_THROW(NotSupportedError, "Maximum per descriptor set input attachment limit exceeded.");

            numPerStageSamplers += numSamplers;
            numPerStageUniformBuffers += numUniformBuffers;
            numPerStageStorageBuffers += numStorageBuffers;
            numPerStageSampledImages += numSampledImages;
            numPerStageStorageImages += numStorageImages;
            numPerStageInputAttachments += numInputAttachments;
            numPerStageTotalResources += numTotalResources;
        }

        if (numPerStageTotalResources > limits.maxPerStageResources)
            TCU_THROW(NotSupportedError, "Maximum per stage total resource limit exceeded.");

        if (numPerStageSamplers > limits.maxPerStageDescriptorSamplers)
            TCU_THROW(NotSupportedError, "Maximum per stage sampler limit exceeded.");

        if (numPerStageUniformBuffers > limits.maxPerStageDescriptorUniformBuffers)
            TCU_THROW(NotSupportedError, "Maximum per stage uniform buffer limit exceeded.");

        if (numPerStageStorageBuffers > limits.maxPerStageDescriptorStorageBuffers)
            TCU_THROW(NotSupportedError, "Maximum per stage storage buffer limit exceeded.");

        if (numPerStageSampledImages > limits.maxPerStageDescriptorSampledImages)
            TCU_THROW(NotSupportedError, "Maximum per stage sampled image limit exceeded.");

        if (numPerStageStorageImages > limits.maxPerStageDescriptorStorageImages)
            TCU_THROW(NotSupportedError, "Maximum per stage storage image limit exceeded.");

        if (numPerStageInputAttachments > limits.maxPerStageDescriptorInputAttachments)
            TCU_THROW(NotSupportedError, "Maximum per stage input attachment limit exceeded.");

#ifndef CTS_USES_VULKANSC
        if (numTotalIUBs > iubProperties.maxDescriptorSetInlineUniformBlocks ||
            numTotalIUBs > iubProperties.maxPerStageDescriptorInlineUniformBlocks)
        {
            TCU_THROW(NotSupportedError, "Number of per stage inline uniform blocks exceeds limits.");
        }
#endif
        if (m_useUpdateAfterBind)
        {
            if (usesUniformBuffer &&
                !context.getDescriptorIndexingFeatures().descriptorBindingUniformBufferUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingUniformBufferUpdateAfterBind not supported.");

            if (usesSampledImage &&
                !context.getDescriptorIndexingFeatures().descriptorBindingSampledImageUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingSampledImageUpdateAfterBind not supported.");

            if (usesStorageImage &&
                !context.getDescriptorIndexingFeatures().descriptorBindingStorageImageUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingStorageImageUpdateAfterBind not supported.");

            if (usesStorageBuffer &&
                !context.getDescriptorIndexingFeatures().descriptorBindingStorageBufferUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingStorageBufferUpdateAfterBind not supported.");

            if (usesUniformTexelBuffer &&
                !context.getDescriptorIndexingFeatures().descriptorBindingUniformTexelBufferUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingUniformTexelBufferUpdateAfterBind not supported.");

            if (usesStorageTexelBuffer &&
                !context.getDescriptorIndexingFeatures().descriptorBindingStorageTexelBufferUpdateAfterBind)
                TCU_THROW(NotSupportedError, "descriptorBindingStorageTexelBufferUpdateAfterBind not supported.");
        }
    }
}

tcu::TestStatus DescriptorCommands::run(Context &context)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = context.getDefaultAllocator();
    tcu::TestLog &log               = context.getTestContext().getLog();
    const Unique<VkCommandPool> commandPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const VkShaderStageFlags shaderStage =
        m_pipelineType == PIPELINE_TYPE_COMPUTE ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkFormat resultFormat = VK_FORMAT_R8G8B8A8_UNORM;
    de::MovePtr<ImageWithMemory> resultImage;
    de::MovePtr<BufferWithMemory> resultImageBuffer;
    Move<VkImageView> resultImageView;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;
    Move<VkDescriptorPool> descriptorPool;
    vector<VkDescriptorSetLayoutSp> descriptorSetLayouts;
    vector<VkDescriptorSet> descriptorSets;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkPipeline> pipeline;
    vector<VkAttachmentReference> inputAttachments;
    vector<VkAttachmentDescription> attachmentDescriptions;
    vector<VkImageView> imageViews;

    // Initialize all descriptors
    for (vector<DescriptorSp>::iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
        (*desc)->init(context, m_pipelineType);

    uint32_t numTotalIUBs                                           = 0;
    uint32_t iubTotalBytes                                          = 0;
    VkDescriptorPoolCreateFlags poolCreateFlags                     = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags = 0u;
    VkDescriptorBindingFlags bindingFlag                            = 0u;
    if (m_useUpdateAfterBind)
    {
        poolCreateFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descriptorSetLayoutCreateFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        bindingFlag |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

#ifndef CTS_USES_VULKANSC
    for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
    {
        const vector<DescriptorSp> &bindings = m_descriptorSets[descriptorSetIdx]->getBindings();
        for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
        {
            // Inline uniform blocks cannot form arrays. The array size is the size of the data array in the descriptor.
            if (bindings[bindingIdx]->getType() == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
            {
                const InlineUniformBlockDescriptor *iub =
                    static_cast<InlineUniformBlockDescriptor *>(bindings[bindingIdx].get());
                iubTotalBytes += iub->getSizeInBytes();

                ++numTotalIUBs;
            }
        }
    }
#else
    DE_UNREF(numTotalIUBs);
    DE_UNREF(iubTotalBytes);
#endif // CTS_USES_VULKANSC

    // Create descriptor pool
    {
        vector<VkDescriptorPoolSize> poolSizes;

        for (map<VkDescriptorType, uint32_t>::iterator i = m_descriptorCounts.begin(); i != m_descriptorCounts.end();
             i++)
        {
            VkDescriptorPoolSize poolSize = {
                i->first, // VkDescriptorType    type
                i->second // uint32_t            descriptorCount
            };

#ifndef CTS_USES_VULKANSC
            // Inline uniform blocks have a special meaning for descriptorCount.
            if (poolSize.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                poolSize.descriptorCount = iubTotalBytes;
#endif

            poolSizes.push_back(poolSize);
        }

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // VkStructureType                sType
            nullptr,                                       // const void*                    pNext
            poolCreateFlags,                               // VkDescriptorPoolCreateFlags    flags
            (uint32_t)m_descriptorSets.size(),             // uint32_t                        maxSets
            (uint32_t)poolSizes.size(),                    // uint32_t                        poolSizeCount
            poolSizes.data(),                              // const VkDescriptorPoolSize*    pPoolSizes
        };

        // Include information about inline uniform blocks if needed.
#ifndef CTS_USES_VULKANSC
        VkDescriptorPoolInlineUniformBlockCreateInfoEXT iubPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT, nullptr, numTotalIUBs};
        if (numTotalIUBs > 0)
            descriptorPoolCreateInfo.pNext = &iubPoolCreateInfo;
#endif

        descriptorPool = createDescriptorPool(vk, device, &descriptorPoolCreateInfo);
    }

    // Create descriptor set layouts. One for each descriptor set used in this test.
    {
        for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
        {
            vector<VkDescriptorSetLayoutBinding> layoutBindings;
            const vector<DescriptorSp> &bindings = m_descriptorSets[descriptorSetIdx]->getBindings();
            const vector<VkDescriptorBindingFlags> bindingsFlags(bindings.size(), bindingFlag);

            for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
            {
                VkDescriptorSetLayoutBinding layoutBinding = {
                    (uint32_t)bindingIdx,                 // uint32_t                binding
                    bindings[bindingIdx]->getType(),      // VkDescriptorType        descriptorType
                    bindings[bindingIdx]->getArraySize(), // uint32_t                descriptorCount
                    shaderStage,                          // VkShaderStageFlags    stageFlags
                    nullptr                               // const VkSampler*        pImmutableSamplers
                };

#ifndef CTS_USES_VULKANSC
                // Inline uniform blocks have a special meaning for descriptorCount.
                if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
                {
                    const InlineUniformBlockDescriptor *iub =
                        static_cast<InlineUniformBlockDescriptor *>(bindings[bindingIdx].get());
                    layoutBinding.descriptorCount = iub->getSizeInBytes();
                }
#endif
                layoutBindings.push_back(layoutBinding);
            }

            const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                           // const void* pNext;
                (uint32_t)layoutBindings.size(),                                   // uint32_t bindingCount;
                layoutBindings.empty() ? nullptr :
                                         bindingsFlags.data(), // const VkDescriptorBindingFlags* pBindingFlags;
            };

            const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
                m_useUpdateAfterBind ? &bindingFlagsInfo : nullptr,  // const void*                            pNext
                descriptorSetLayoutCreateFlags,                      // VkDescriptorSetLayoutCreateFlags        flags
                (uint32_t)layoutBindings.size(), // uint32_t                                bindingCount
                layoutBindings.data()            // const VkDescriptorSetLayoutBinding*    pBindings
            };

            descriptorSetLayouts.push_back(VkDescriptorSetLayoutSp(new Unique<VkDescriptorSetLayout>(
                createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo, nullptr))));
        }
    }

    // Create descriptor sets
    {
        for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
        {
            const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,  // VkStructureType                sType
                nullptr,                                         // const void*                    pNext
                *descriptorPool,                                 // VkDescriptorPool                descriptorPool
                1u,                                              // uint32_t                        descriptorSetCount
                &(descriptorSetLayouts[descriptorSetIdx]->get()) // const VkDescriptorSetLayout*    pSetLayouts
            };

            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            VK_CHECK(vk.allocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
            descriptorSets.push_back(descriptorSet);
        }
    }

    // Update descriptor sets when this should be done before bind
    if (!m_useUpdateAfterBind)
        updateDescriptorSets(context, descriptorSets);

    // Create pipeline layout
    {
        vector<VkDescriptorSetLayout> descriptorSetLayoutHandles;

        for (size_t i = 0; i < descriptorSetLayouts.size(); i++)
            descriptorSetLayoutHandles.push_back(descriptorSetLayouts[i]->get());

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                sType
            nullptr,                                       // const void*                    pNext
            0u,                                            // VkPipelineLayoutCreateFlags    flags
            (uint32_t)descriptorSetLayoutHandles.size(),   // uint32_t                        setLayoutCount
            descriptorSetLayoutHandles.data(),             // const VkDescriptorSetLayout*    pSetLayouts
            0u,                                            // uint32_t                        pushConstantRangeCount
            nullptr                                        // const VkPushConstantRange*    pPushConstantRanges
        };

        pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    }

    if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
    {
        // Create compute pipeline
        {
            const Unique<VkShaderModule> shaderModule(
                createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0u));
            const VkPipelineShaderStageCreateInfo shaderStageInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                    sType
                nullptr,                                             // const void*                        pNext
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags    flags
                VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits            stage
                *shaderModule,                                       // VkShaderModule                    module
                "main",                                              // const char*                        pName
                nullptr // const VkSpecializationInfo*        pSpecializationInfo
            };

            const VkComputePipelineCreateInfo pipelineInfo = {
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                    sType
                nullptr,                                        // const void*                        pNext
                (VkPipelineCreateFlags)0,                       // VkPipelineCreateFlags            flags
                shaderStageInfo,                                // VkPipelineShaderStageCreateInfo    stage
                *pipelineLayout,                                // VkPipelineLayout                    layout
                VK_NULL_HANDLE,                                 // VkPipeline                        basePipelineHandle
                0                                               // int32_t                            basePipelineIndex
            };

            pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo);
        }
    }
    else
    {
        // Create result image
        {
            const VkImageCreateInfo imageCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                     // VkStructureType            stype
                nullptr,                                                 // const void*                pNext
                0u,                                                      // VkImageCreateFlags        flags
                VK_IMAGE_TYPE_2D,                                        // VkImageType                imageType
                resultFormat,                                            // VkFormat                    format
                {(uint32_t)renderSize.x(), (uint32_t)renderSize.y(), 1}, // VkExtent3D                extent
                1u,                                                      // uint32_t                    mipLevels
                1u,                                                      // uint32_t                    arrayLayers
                VK_SAMPLE_COUNT_1_BIT,                                   // VkSampleCountFlagBits    samples
                VK_IMAGE_TILING_OPTIMAL,                                 // VkImageTiling            tiling
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
                VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode            sharingMode
                1u,                        // uint32_t                    queueFamilyIndexCount
                &queueFamilyIndex,         // const uint32_t*            pQueueFamilyIndices
                VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout            initialLayout
            };

            resultImage = de::MovePtr<ImageWithMemory>(
                new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
        }

        // Create result image view
        {
            const VkComponentMapping componentMapping = makeComponentMappingRGBA();

            const VkImageSubresourceRange subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                0u,                        // uint32_t                baseMipLevel
                1u,                        // uint32_t                levelCount
                0u,                        // uint32_t                baseArrayLayer
                1u,                        // uint32_t                layerCount
            };

            const VkImageViewCreateInfo imageViewCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType            sType
                nullptr,                                  // const void*                pNext
                0u,                                       // VkImageViewCreateFlags    flags
                **resultImage,                            // VkImage                    image
                VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType            viewType
                resultFormat,                             // VkFormat                    format
                componentMapping,                         // VkComponentMapping        components
                subresourceRange                          // VkImageSubresourceRange    subresourceRange
            };

            resultImageView = createImageView(vk, device, &imageViewCreateInfo);
        }

        // Create result buffer
        {
            const VkDeviceSize bufferSize =
                renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(resultFormat));
            const VkBufferCreateInfo bufferCreateInfo = {
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
                nullptr,                              // const void*            pNext
                0u,                                   // VkBufferCreateFlags    flags
                bufferSize,                           // VkDeviceSize            size
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags    usage
                VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode
                0u,                                   // uint32_t                queueFamilyIndexCount
                nullptr                               // const uint32_t*        pQueueFamilyIndices
            };

            resultImageBuffer = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
        }

        // Create render pass
        {
            const VkAttachmentReference colorAttachmentRef = {
                0u,                                      // uint32_t            attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            };

            for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
            {
                vector<VkAttachmentReference> references = (*desc)->getAttachmentReferences();
                inputAttachments.insert(inputAttachments.end(), references.begin(), references.end());
            }

            const VkAttachmentDescription colorAttachmentDesc = {
                0u,                                      // VkAttachmentDescriptionFlags    flags
                VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                        format
                VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits        samples
                VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp            loadOp
                VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp            storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp            stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp            stencilStoreOp
                VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                initialLayout
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                finalLayout
            };

            attachmentDescriptions.push_back(colorAttachmentDesc);

            const VkAttachmentDescription inputAttachmentDesc = {
                0u,                                       // VkAttachmentDescriptionFlags    flags
                VK_FORMAT_R32_SFLOAT,                     // VkFormat                        format
                VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits        samples
                VK_ATTACHMENT_LOAD_OP_LOAD,               // VkAttachmentLoadOp            loadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp            storeOp
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp            stencilLoadOp
                VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp            stencilStoreOp
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // VkImageLayout                initialLayout
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // VkImageLayout                finalLayout
            };

            for (size_t inputAttachmentIdx = 0; inputAttachmentIdx < inputAttachments.size(); inputAttachmentIdx++)
                attachmentDescriptions.push_back(inputAttachmentDesc);

            const VkSubpassDescription subpassDescription = {
                0u,                                // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS,   // VkPipelineBindPoint            pipelineBindPoint
                (uint32_t)inputAttachments.size(), // uint32_t                        inputAttachmentCount
                inputAttachments.empty() ? nullptr :
                                           inputAttachments.data(), // const VkAttachmentReference*    pInputAttachments
                1u,                  // uint32_t                        colorAttachmentCount
                &colorAttachmentRef, // const VkAttachmentReference*    pColorAttachments
                nullptr,             // const VkAttachmentReference*    pResolveAttachments
                nullptr,             // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                  // uint32_t                        preserveAttachmentCount
                nullptr              // const uint32_t*                pPreserveAttachments
            };

            const VkRenderPassCreateInfo renderPassCreateInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                    sType
                nullptr,                                   // const void*                        pNext
                0u,                                        // VkRenderPassCreateFlags            flags
                (uint32_t)attachmentDescriptions.size(),   // uint32_t                            attachmentCount
                attachmentDescriptions.data(),             // const VkAttachmentDescription*    pAttachments
                1u,                                        // uint32_t                            subpassCount
                &subpassDescription,                       // const VkSubpassDescription*        pSubpasses
                0u,                                        // uint32_t                            dependencyCount
                nullptr                                    // const VkSubpassDependency*        pDependencies
            };

            renderPass = createRenderPass(vk, device, &renderPassCreateInfo);
        }

        // Create framebuffer
        {
            imageViews.push_back(*resultImageView);

            // Add input attachment image views
            for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
            {
                vector<VkImageViewSp> inputAttachmentViews = (*desc)->getImageViews();

                for (size_t imageViewIdx = 0; imageViewIdx < inputAttachmentViews.size(); imageViewIdx++)
                    imageViews.push_back(**inputAttachmentViews[imageViewIdx]);
            }

            const VkFramebufferCreateInfo framebufferCreateInfo = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType             sType
                nullptr,                                   // const void*                 pNext
                0u,                                        // VkFramebufferCreateFlags    flags
                *renderPass,                               // VkRenderPass                renderPass
                (uint32_t)imageViews.size(),               // uint32_t                    attachmentCount
                imageViews.data(),                         // const VkImageView*          pAttachments
                (uint32_t)renderSize.x(),                  // uint32_t                    width
                (uint32_t)renderSize.y(),                  // uint32_t                    height
                1u,                                        // uint32_t                    layers
            };

            framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
        }

        // Create graphics pipeline
        {
            const Unique<VkShaderModule> vertexShaderModule(
                createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0u));
            const Unique<VkShaderModule> fragmentShaderModule(
                createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));

            const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                            sType
                nullptr,                                  // const void*                                pNext
                (VkPipelineVertexInputStateCreateFlags)0, // VkPipelineVertexInputStateCreateFlags    flags
                0u,                                       // uint32_t                                    bindingCount
                nullptr, // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
                0u,      // uint32_t                                    attributeCount
                nullptr, // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
            };

            const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
            const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

            pipeline = makeGraphicsPipeline(
                vk,                    // const DeviceInterface&                        vk
                device,                // const VkDevice                                device
                *pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
                *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlShaderModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalShaderModule
                VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
                *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
                *renderPass,           // const VkRenderPass                            renderPass
                viewports,             // const std::vector<VkViewport>&                viewports
                scissors,              // const std::vector<VkRect2D>&                  scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
                0u,                                  // const uint32_t                                subpass
                0u,                                  // const uint32_t                                patchControlPoints
                &vertexInputStateParams); // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        }
    }

    // Run verification shader
    {
        const VkPipelineBindPoint pipelineBindPoint =
            m_pipelineType == PIPELINE_TYPE_COMPUTE ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
        vector<uint32_t> offsets;

        if (hasDynamicAreas())
        {
            for (size_t areaIdx = 0; areaIdx < m_dynamicAreas.size(); areaIdx++)
                offsets.push_back(m_dynamicAreas[areaIdx] * 256u);
        }

        beginCommandBuffer(vk, *commandBuffer);

        if (m_pipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            const VkRect2D renderArea = makeRect2D(renderSize);
            const tcu::Vec4 clearColor(1.0f, 0.0f, 0.0f, 1.0f);

            beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, clearColor);
        }

        vk.cmdBindPipeline(*commandBuffer, pipelineBindPoint, *pipeline);
        vk.cmdBindDescriptorSets(*commandBuffer, pipelineBindPoint, *pipelineLayout, 0u,
                                 (uint32_t)descriptorSets.size(), descriptorSets.data(), (uint32_t)offsets.size(),
                                 offsets.empty() ? nullptr : offsets.data());

        // Update descriptor sets when this should be done after bind
        if (m_useUpdateAfterBind)
            updateDescriptorSets(context, descriptorSets);

        if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
        {
            vk.cmdDispatch(*commandBuffer, 1u, 1u, 1u);
        }
        else
        {
            vk.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
            endRenderPass(vk, *commandBuffer);
            copyImageToBuffer(vk, *commandBuffer, **resultImage, resultImageBuffer->get(), renderSize);
        }

        endCommandBuffer(vk, *commandBuffer);
        submitCommandsAndWait(vk, device, queue, *commandBuffer);
    }

    if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
    {
        // Invalidate result buffer
        m_resultBuffer->invalidate(context);

        // Verify result data
        const auto data = m_resultBuffer->getData();
        if (data[0] == 1)
            return tcu::TestStatus::pass("Pass");
        else
            return tcu::TestStatus::fail("Data validation failed");
    }
    else
    {
        invalidateAlloc(vk, device, resultImageBuffer->getAllocation());

        // Verify result image
        tcu::ConstPixelBufferAccess resultBufferAccess(mapVkFormat(resultFormat), renderSize.x(), renderSize.y(), 1,
                                                       resultImageBuffer->getAllocation().getHostPtr());

        for (int32_t y = 0; y < renderSize.y(); y++)
            for (int32_t x = 0; x < renderSize.x(); x++)
            {
                Vec4 pixel = resultBufferAccess.getPixel(x, y, 0);

                if (pixel.x() != 0.0f || pixel.y() != 1.0f || pixel.z() != 0.0f || pixel.w() != 1.0f)
                {
                    // Log result image before failing.
                    log << tcu::TestLog::ImageSet("Result", "")
                        << tcu::TestLog::Image("Rendered", "Rendered image", resultBufferAccess)
                        << tcu::TestLog::EndImageSet;
                    return tcu::TestStatus::fail("Result image validation failed");
                }
            }

        return tcu::TestStatus::pass("Pass");
    }
}

void DescriptorCommands::updateDescriptorSets(Context &context, const vector<VkDescriptorSet> &descriptorSets)
{
    const DeviceInterface &vk = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    vector<VkWriteDescriptorSet> descriptorWrites;
    vector<VkCopyDescriptorSet> descriptorCopies;

    // Write descriptors that are marked as needing initialization
    for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
    {
        const vector<DescriptorSp> &bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

        for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
        {
            VkWriteDescriptorSet descriptorWrite = bindings[bindingIdx]->getDescriptorWrite();

            descriptorWrite.dstSet     = descriptorSets[descriptorSetIdx];
            descriptorWrite.dstBinding = (uint32_t)bindingIdx;

            if (descriptorWrite.descriptorCount > 0)
                descriptorWrites.push_back(descriptorWrite);
        }
    }

    for (size_t copyIdx = 0; copyIdx < m_descriptorCopies.size(); copyIdx++)
    {
        const DescriptorCopy indices   = m_descriptorCopies[copyIdx];
        const VkCopyDescriptorSet copy = {
            VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, // VkStructureType    sType
            nullptr,                               // const void*        pNext
            descriptorSets[indices.srcSet],        // VkDescriptorSet    srcSet
            indices.srcBinding,                    // uint32_t            srcBinding
            indices.srcArrayElement,               // uint32_t            srcArrayElement
            descriptorSets[indices.dstSet],        // VkDescriptorSet    dstSet
            indices.dstBinding,                    // uint32_t            dstBinding
            indices.dstArrayElement,               // uint32_t            dstArrayElement
            indices.descriptorCount                // uint32_t            descriptorCount
        };

        descriptorCopies.push_back(copy);
    }

    // Update descriptors with writes and copies
    vk.updateDescriptorSets(device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(),
                            (uint32_t)descriptorCopies.size(), descriptorCopies.data());
}

DescriptorCopyTestInstance::DescriptorCopyTestInstance(Context &context, DescriptorCommandsSp commands)
    : vkt::TestInstance(context)
    , m_commands(commands)
{
}

DescriptorCopyTestInstance::~DescriptorCopyTestInstance(void)
{
}

DescriptorCopyTestCase::DescriptorCopyTestCase(tcu::TestContext &context, const char *name,
                                               DescriptorCommandsSp commands)
    : vkt::TestCase(context, name)
    , m_commands(commands)
{
}

DescriptorCopyTestCase::~DescriptorCopyTestCase(void)
{
}

void DescriptorCopyTestCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_commands->getPipelineType() == PIPELINE_TYPE_COMPUTE)
    {
        string computeSrc = "#version 430\n"
                            "\n" +
                            m_commands->getShaderDeclarations() +
                            "\n"
                            "void main()\n"
                            "{\n"
                            "int result = 1;\n" +
                            m_commands->getDescriptorVerifications() + "storageBuffer" +
                            de::toString(m_commands->getResultBufferId()) +
                            ".data = result;\n"
                            "}\n";

        programCollection.glslSources.add("compute") << glu::ComputeSource(computeSrc);
    }
    else
    {
        // Produce quad vertices using vertex index
        string vertexSrc = "#version 450\n"
                           "out gl_PerVertex\n"
                           "{\n"
                           "    vec4 gl_Position;\n"
                           "};\n"
                           "void main()\n"
                           "{\n"
                           "    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
                           "                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
                           "}\n";

        programCollection.glslSources.add("vertex") << glu::VertexSource(vertexSrc);

        string fragmentSrc = "#version 430\n"
                             "\n" +
                             m_commands->getShaderDeclarations() +
                             "layout (location = 0) out vec4 outColor;\n"
                             "\n"
                             "void main()\n"
                             "{\n"
                             "int result = 1;\n" +
                             m_commands->getDescriptorVerifications() +
                             "if (result == 1) outColor = vec4(0, 1, 0, 1);\n"
                             "else outColor = vec4(1, 0, 1, 0);\n"
                             "}\n";

        programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc);
    }
}

TestInstance *DescriptorCopyTestCase::createInstance(Context &context) const
{
    TestInstance *result = new DescriptorCopyTestInstance(context, m_commands);
    m_commands.clear();
    return result;
}

void DescriptorCopyTestCase::checkSupport(Context &context) const
{
    m_commands->checkSupport(context);
}

tcu::TestStatus DescriptorCopyTestInstance::iterate(void)
{
    return m_commands->run(m_context);
}

template <class T>
void addDescriptorCopyTests(tcu::TestContext &testCtx, de::MovePtr<tcu::TestCaseGroup> &group, string name,
                            PipelineType pipelineType, bool useUpdateAfterBind)
{
    // Simple test copying inside the same set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);

        commands->copyDescriptor(0u, 0u,  // from
                                 0u, 1u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_0").c_str(), commands));
    }

    // Simple test copying between different sets.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 1u);

        commands->copyDescriptor(0u, 0u,  // from
                                 1u, 0u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_1").c_str(), commands));
    }

    // Simple test copying between different sets. Destination not updated.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 0u, 1u)), 1u);

        commands->copyDescriptor(0u, 0u,  // from
                                 1u, 0u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_2").c_str(), commands));
    }

    // Five sets and several copies.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 1u)), 1u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 5u)), 4u);

        commands->copyDescriptor(4u, 0u,  // from
                                 0u, 0u); // to

        commands->copyDescriptor(0u, 1u,  // from
                                 1u, 2u); // to

        commands->copyDescriptor(0u, 1u,  // from
                                 1u, 1u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(4u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_3").c_str(), commands));
    }

    // Several identical copies
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 1u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);

        for (uint32_t i = 0; i < 100; i++)
        {
            commands->copyDescriptor(0u, 0u,  // from
                                     1u, 0u); // to
        }

        commands->copyDescriptor(1u, 1u,  // from
                                 0u, 0u); // to

        for (uint32_t i = 0; i < 100; i++)
        {
            commands->copyDescriptor(1u, 0u,  // from
                                     1u, 1u); // to
        }

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_4").c_str(), commands));
    }

    // Copy descriptors back and forth
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 1u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 1u);

        commands->copyDescriptor(0u, 0u,  // from
                                 1u, 0u); // to

        commands->copyDescriptor(1u, 0u,  // from
                                 0u, 0u); // to

        commands->copyDescriptor(1u, 1u,  // from
                                 0u, 0u); // to

        commands->copyDescriptor(1u, 1u,  // from
                                 0u, 0u); // to

        commands->copyDescriptor(1u, 0u,  // from
                                 1u, 1u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(0u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_5").c_str(), commands));
    }

    // Copy between non-consecutive descriptor sets
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 5u);
        commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 5u);

        commands->copyDescriptor(0u, 0u,  // from
                                 5u, 1u); // to

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_6").c_str(), commands));
    }

    // Simple 3 sized array to 3 sized array inside the same set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 4u)), 0u);

        commands->copyDescriptor(0u, 0u, 0u, // from
                                 0u, 1u, 0u, // to
                                 3u);        // num descriptors

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(2u);

        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array0").c_str(), commands));
    }

    // Simple 2 sized array to 3 sized array into different set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(2u, 0u, 2u, 2u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 5u)), 1u);

        commands->copyDescriptor(0u, 0u, 0u, // from
                                 1u, 0u, 0u, // to
                                 2u);        // num descriptors

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);

        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array1").c_str(), commands));
    }

    // Update array partially with writes and partially with a copy
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        commands->addDescriptor(DescriptorSp(new T(4u, 0u, 4u, 3u)), 0u);
        commands->addDescriptor(DescriptorSp(new T(8u, 0u, 5u, 4u)), 0u);

        commands->copyDescriptor(0u, 0u, 1u, // from
                                 0u, 1u, 5u, // to
                                 3u);        // num descriptors

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(1u);

        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(0u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(1u);
        dynamicAreas.push_back(2u);
        commands->setDynamicAreas(dynamicAreas);

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array2").c_str(), commands));
    }
}

void addSamplerCopyTests(tcu::TestContext &testCtx, de::MovePtr<tcu::TestCaseGroup> &group, PipelineType pipelineType,
                         bool useUpdateAfterBind)
{
    // Simple copy between two samplers in the same set
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        SamplerDescriptor *sampler0(new SamplerDescriptor());
        SamplerDescriptor *sampler1(new SamplerDescriptor());
        SampledImageDescriptor *image(new SampledImageDescriptor());
        sampler0->addImage(image);
        sampler1->addImage(image);

        commands->addDescriptor(DescriptorSp(sampler0), 0u);
        commands->addDescriptor(DescriptorSp(sampler1), 0u);
        commands->addDescriptor(DescriptorSp(image), 0u);

        commands->copyDescriptor(0u, 0u,  // from
                                 0u, 1u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_0", commands));
    }

    // Simple 3 sized array to 3 sized array inside the same set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        SamplerDescriptor *sampler0(new SamplerDescriptor(3u, 0u, 3u));
        // One sampler in between to get the border colors to originally mismatch between sampler0 and sampler1.
        SamplerDescriptor *sampler1(new SamplerDescriptor());
        SamplerDescriptor *sampler2(new SamplerDescriptor(3u, 0u, 3u));
        SampledImageDescriptor *image(new SampledImageDescriptor());

        sampler0->addImage(image, 3u);
        sampler2->addImage(image, 3u);

        commands->addDescriptor(DescriptorSp(sampler0), 0u);
        commands->addDescriptor(DescriptorSp(sampler1), 0u);
        commands->addDescriptor(DescriptorSp(sampler2), 0u);
        commands->addDescriptor(DescriptorSp(image), 0u);

        commands->copyDescriptor(0u, 0u, 0u, // from
                                 0u, 2u, 0u, // to
                                 3u);        // num descriptors

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_array0", commands));
    }

    // Simple 2 sized array to 3 sized array into different set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        SamplerDescriptor *sampler0(new SamplerDescriptor(2u, 0u, 2u));
        SamplerDescriptor *sampler1(new SamplerDescriptor(3u, 0u, 3u));
        SampledImageDescriptor *image(new SampledImageDescriptor());

        sampler0->addImage(image, 2u);
        sampler1->addImage(image, 3u);

        commands->addDescriptor(DescriptorSp(sampler0), 0u);
        commands->addDescriptor(DescriptorSp(sampler1), 1u);
        commands->addDescriptor(DescriptorSp(image), 0u);

        commands->copyDescriptor(0u, 0u, 0u, // from
                                 1u, 0u, 1u, // to
                                 2u);        // num descriptors

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_array1", commands));
    }
}

void addSampledImageCopyTests(tcu::TestContext &testCtx, de::MovePtr<tcu::TestCaseGroup> &group,
                              PipelineType pipelineType, bool useUpdateAfterBind)
{
    // Simple copy between two images in the same set
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        SamplerDescriptor *sampler(new SamplerDescriptor());
        SampledImageDescriptor *image0(new SampledImageDescriptor());
        SampledImageDescriptor *image1(new SampledImageDescriptor());
        image0->addSampler(sampler);
        image1->addSampler(sampler);

        commands->addDescriptor(DescriptorSp(image0), 0u);
        commands->addDescriptor(DescriptorSp(image1), 0u);
        commands->addDescriptor(DescriptorSp(sampler), 0u);

        commands->copyDescriptor(0u, 0u,  // from
                                 0u, 1u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "sampled_image_0", commands));
    }

    // Simple 3 sized array to 3 sized array inside the same set.
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, useUpdateAfterBind));
        SamplerDescriptor *sampler(new SamplerDescriptor());
        SampledImageDescriptor *image0(new SampledImageDescriptor(3u, 0u, 3u));
        SampledImageDescriptor *image1(new SampledImageDescriptor(3u, 0u, 3u));
        image0->addSampler(sampler, 3u);
        image1->addSampler(sampler, 3u);

        commands->addDescriptor(DescriptorSp(sampler), 0u);
        commands->addDescriptor(DescriptorSp(image0), 0u);
        commands->addDescriptor(DescriptorSp(image1), 0u);

        commands->copyDescriptor(0u, 1u, 0u, // from
                                 0u, 2u, 0u, // to
                                 3u);        // num descriptors

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "sampled_image_array0", commands));
    }
}

// Mixture of different descriptors in the same test
void addMixedDescriptorCopyTests(tcu::TestContext &testCtx, de::MovePtr<tcu::TestCaseGroup> &group,
                                 PipelineType pipelineType)
{
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        SamplerDescriptor *sampler0(new SamplerDescriptor());
        SamplerDescriptor *sampler1(new SamplerDescriptor());
        SampledImageDescriptor *image0(new SampledImageDescriptor());
        SampledImageDescriptor *image1(new SampledImageDescriptor());
        StorageBufferDescriptor *storageBuffer0(new StorageBufferDescriptor());
        StorageBufferDescriptor *storageBuffer1(new StorageBufferDescriptor());
        StorageBufferDescriptor *storageBuffer2 = new StorageBufferDescriptor();
        sampler0->addImage(image0);
        sampler1->addImage(image1);

        commands->addDescriptor(DescriptorSp(sampler0), 0u);       // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(storageBuffer0), 0u); // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(image0), 0u);         // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(storageBuffer1), 0u); // Set 0, binding 3
        commands->addDescriptor(DescriptorSp(sampler1), 1u);       // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(image1), 1u);         // Set 1, binding 1
        commands->addDescriptor(DescriptorSp(storageBuffer2), 1u); // Set 1, binding 2

        // image1 to image0
        commands->copyDescriptor(1u, 1u,  // from
                                 0u, 2u); // to

        // storageBuffer0 to storageBuffer1
        commands->copyDescriptor(0u, 1u,  // from
                                 0u, 3u); // to

        // storageBuffer1 to storageBuffer2
        commands->copyDescriptor(0u, 3u,  // from
                                 1u, 2u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_0", commands));
    }

    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        StorageTexelBufferDescriptor *storageTexelBuffer0(new StorageTexelBufferDescriptor());
        StorageTexelBufferDescriptor *storageTexelBuffer1(new StorageTexelBufferDescriptor());
        UniformBufferDescriptor *uniformBuffer0(new UniformBufferDescriptor());
        UniformBufferDescriptor *uniformBuffer1(new UniformBufferDescriptor());
        UniformBufferDescriptor *uniformBuffer2(new UniformBufferDescriptor());
        DynamicStorageBufferDescriptor *dynamicStorageBuffer0(new DynamicStorageBufferDescriptor(1u, 0u, 1u, 3u));
        DynamicStorageBufferDescriptor *dynamicStorageBuffer1(new DynamicStorageBufferDescriptor(1u, 0u, 1u, 4u));

        commands->addDescriptor(DescriptorSp(storageTexelBuffer0), 0u);   // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(uniformBuffer0), 0u);        // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(dynamicStorageBuffer0), 0u); // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(uniformBuffer1), 0u);        // Set 0, binding 3
        commands->addDescriptor(DescriptorSp(dynamicStorageBuffer1), 1u); // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(storageTexelBuffer1), 1u);   // Set 1, binding 1
        commands->addDescriptor(DescriptorSp(uniformBuffer2), 1u);        // Set 1, binding 2

        vector<uint32_t> dynamicAreas;
        dynamicAreas.push_back(2u);
        dynamicAreas.push_back(1u);
        commands->setDynamicAreas(dynamicAreas);

        // uniformBuffer0 to uniformBuffer2
        commands->copyDescriptor(0u, 1u,  // from
                                 1u, 2u); // to

        // uniformBuffer1 to uniformBuffer2
        commands->copyDescriptor(0u, 3u,  // from
                                 1u, 2u); // to

        // storageTexelBuffer1 to storageTexelBuffer0
        commands->copyDescriptor(1u, 1u,  // from
                                 0u, 0u); // to

        // dynamicStorageBuffer0 to dynamicStorageBuffer1
        commands->copyDescriptor(0u, 2u,  // from
                                 1u, 0u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_1", commands));
    }

    if (pipelineType == PIPELINE_TYPE_GRAPHICS)
    {
        // Mixture of descriptors, including input attachment.
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        InputAttachmentDescriptor *inputAttachment0(new InputAttachmentDescriptor());
        InputAttachmentDescriptor *inputAttachment1(new InputAttachmentDescriptor());
        CombinedImageSamplerDescriptor *combinedImageSampler0(new CombinedImageSamplerDescriptor());
        CombinedImageSamplerDescriptor *combinedImageSampler1(new CombinedImageSamplerDescriptor());
        UniformTexelBufferDescriptor *uniformTexelBuffer0(new UniformTexelBufferDescriptor(5u, 0u, 5u));
        UniformTexelBufferDescriptor *uniformTexelBuffer1(new UniformTexelBufferDescriptor(3u, 1u, 1u));

        commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u); // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(inputAttachment0), 0u);      // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(uniformTexelBuffer0), 0u);   // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(combinedImageSampler1), 1u); // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(inputAttachment1), 1u);      // Set 1, binding 1
        commands->addDescriptor(DescriptorSp(uniformTexelBuffer1), 1u);   // Set 1, binding 2

        // uniformTexelBuffer0[1..3] to uniformTexelBuffer1[0..2]
        commands->copyDescriptor(0u, 2u, 1u, // from
                                 1u, 2u, 0u, // to
                                 3u);        // num descriptors

        // inputAttachment0 to inputAttachment1
        commands->copyDescriptor(0u, 1u,  // from
                                 1u, 1u); // to

        // combinedImageSampler0 to combinedImageSampler1
        commands->copyDescriptor(0u, 0u,  // from
                                 1u, 0u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_2", commands));
    }

#ifndef CTS_USES_VULKANSC
    if (pipelineType == PIPELINE_TYPE_GRAPHICS)
    {
        // Similar to the previous one, but adding inline uniform blocks to the mix.
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        InlineUniformBlockDescriptor *iub0(new InlineUniformBlockDescriptor(4u, 0u, 4u));
        InlineUniformBlockDescriptor *iub1(new InlineUniformBlockDescriptor(4u, 0u, 1u));
        InputAttachmentDescriptor *inputAttachment0(new InputAttachmentDescriptor());
        InputAttachmentDescriptor *inputAttachment1(new InputAttachmentDescriptor());
        CombinedImageSamplerDescriptor *combinedImageSampler0(new CombinedImageSamplerDescriptor());
        CombinedImageSamplerDescriptor *combinedImageSampler1(new CombinedImageSamplerDescriptor());
        UniformTexelBufferDescriptor *uniformTexelBuffer0(new UniformTexelBufferDescriptor(5u, 0u, 5u));
        UniformTexelBufferDescriptor *uniformTexelBuffer1(new UniformTexelBufferDescriptor(3u, 1u, 1u));

        commands->addDescriptor(DescriptorSp(iub0), 0u);                  // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u); // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(inputAttachment0), 0u);      // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(uniformTexelBuffer0), 0u);   // Set 0, binding 3
        commands->addDescriptor(DescriptorSp(iub1), 1u);                  // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(combinedImageSampler1), 1u); // Set 1, binding 1
        commands->addDescriptor(DescriptorSp(inputAttachment1), 1u);      // Set 1, binding 2
        commands->addDescriptor(DescriptorSp(uniformTexelBuffer1), 1u);   // Set 1, binding 3

        // iub0.data[0..2] to iub1.data[1..3]
        commands->copyDescriptor(0u, 0u, 0u, // from
                                 1u, 0u, 1u, // to
                                 3u);        // num descriptors

        // uniformTexelBuffer0[1..3] to uniformTexelBuffer1[0..2]
        commands->copyDescriptor(0u, 3u, 1u, // from
                                 1u, 3u, 0u, // to
                                 3u);        // num descriptors

        // inputAttachment0 to inputAttachment1
        commands->copyDescriptor(0u, 2u,  // from
                                 1u, 2u); // to

        // combinedImageSampler0 to combinedImageSampler1
        commands->copyDescriptor(0u, 1u,  // from
                                 1u, 1u); // to

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_3", commands));
    }
#endif

    // Mixture of descriptors using descriptor arrays
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        CombinedImageSamplerDescriptor *combinedImageSampler0(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
        CombinedImageSamplerDescriptor *combinedImageSampler1(new CombinedImageSamplerDescriptor(4u, 0u, 2u));
        CombinedImageSamplerDescriptor *combinedImageSampler2(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
        StorageImageDescriptor *storageImage0(new StorageImageDescriptor(5u, 0u, 5u));
        StorageImageDescriptor *storageImage1(new StorageImageDescriptor(3u, 0u, 0u));
        StorageBufferDescriptor *storageBuffer0(new StorageBufferDescriptor(2u, 0u, 1u));
        StorageBufferDescriptor *storageBuffer1(new StorageBufferDescriptor(3u, 0u, 3u));

        commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u); // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(storageImage0), 0u);         // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(combinedImageSampler1), 0u); // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(storageBuffer0), 0u);        // Set 0, binding 3
        commands->addDescriptor(DescriptorSp(storageBuffer1), 0u);        // Set 0, binding 4
        commands->addDescriptor(DescriptorSp(storageImage1), 1u);         // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(combinedImageSampler2), 1u); // Set 1, binding 1

        // combinedImageSampler0[1..2] to combinedImageSampler1[2..3]
        commands->copyDescriptor(0u, 0u, 1u, // from
                                 0u, 2u, 2u, // to
                                 2u);        // num descriptors

        // storageImage0[2..4] to storageImage1[0..2]
        commands->copyDescriptor(0u, 1u, 2u, // from
                                 1u, 0u, 0u, // to
                                 3u);        // num descriptors

        // storageBuffer1[1..2] to storageBuffer0[0..1]
        commands->copyDescriptor(0u, 4u, 1u, // from
                                 0u, 3u, 0u, // to
                                 2u);        // num descriptors

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_array0", commands));
    }

    // Similar to the previous one but including inline uniform blocks.
#ifndef CTS_USES_VULKANSC
    {
        DescriptorCommandsSp commands(new DescriptorCommands(pipelineType, false));
        InlineUniformBlockDescriptor *iub0(new InlineUniformBlockDescriptor(4u, 0u, 1u));
        InlineUniformBlockDescriptor *iub1(new InlineUniformBlockDescriptor(4u, 0u, 4u));
        CombinedImageSamplerDescriptor *combinedImageSampler0(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
        CombinedImageSamplerDescriptor *combinedImageSampler1(new CombinedImageSamplerDescriptor(4u, 0u, 2u));
        CombinedImageSamplerDescriptor *combinedImageSampler2(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
        StorageImageDescriptor *storageImage0(new StorageImageDescriptor(5u, 0u, 5u));
        StorageImageDescriptor *storageImage1(new StorageImageDescriptor(3u, 0u, 0u));
        StorageBufferDescriptor *storageBuffer0(new StorageBufferDescriptor(2u, 0u, 1u));
        StorageBufferDescriptor *storageBuffer1(new StorageBufferDescriptor(3u, 0u, 3u));

        commands->addDescriptor(DescriptorSp(iub0), 0u);                  // Set 0, binding 0
        commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u); // Set 0, binding 1
        commands->addDescriptor(DescriptorSp(storageImage0), 0u);         // Set 0, binding 2
        commands->addDescriptor(DescriptorSp(combinedImageSampler1), 0u); // Set 0, binding 3
        commands->addDescriptor(DescriptorSp(storageBuffer0), 0u);        // Set 0, binding 4
        commands->addDescriptor(DescriptorSp(storageBuffer1), 0u);        // Set 0, binding 5
        commands->addDescriptor(DescriptorSp(combinedImageSampler2), 0u); // Set 0, binding 6
        commands->addDescriptor(DescriptorSp(iub1), 1u);                  // Set 1, binding 0
        commands->addDescriptor(DescriptorSp(storageImage1), 1u);         // Set 1, binding 1

        // iub1.data[0..2] to iub0.data[1..3]
        commands->copyDescriptor(1u, 0u, 0u, // from
                                 0u, 0u, 1u, // to
                                 3u);        // num descriptors

        // combinedImageSampler0[1..2] to combinedImageSampler1[2..3]
        commands->copyDescriptor(0u, 1u, 1u, // from
                                 0u, 3u, 2u, // to
                                 2u);        // num descriptors

        // storageImage0[2..4] to storageImage1[0..2]
        commands->copyDescriptor(0u, 2u, 2u, // from
                                 1u, 1u, 0u, // to
                                 3u);        // num descriptors

        // storageBuffer1[1..2] to storageBuffer0[0..1]
        commands->copyDescriptor(0u, 5u, 1u, // from
                                 0u, 4u, 0u, // to
                                 2u);        // num descriptors

        commands->addResultBuffer();

        group->addChild(new DescriptorCopyTestCase(testCtx, "mix_array1", commands));
    }
#endif
}

// The goal of these tests is checking if the implementation correctly copies combined-image-sampler bindings that use
// immutable samplers, since those may be handled in special ways by the driver. To check this, we create a descriptor
// set layout that contains one or more combined image samplers (with immutable samplers) and a storage buffer, that
// goes before or after the samplers. Then, we create two descriptor sets, we prepare the first one normally and we
// prepare the second one by copying the samplers from the first descriptor set. However, before copying the samplers we
// write the storage buffer descriptor normally.
//
// If the driver has a bug, presumably the copies will be too small or too large, and should result in incorrect
// samplers or the descriptor buffer information being possibly overwritten.
//
struct CopyImmutableSamplerParams
{
    uint32_t samplerCount;
    bool bufferFirst;

    uint32_t getBufferBinding() const
    {
        return (bufferFirst ? 0u : samplerCount);
    }

    uint32_t getImgBindingOffset() const
    {
        return (bufferFirst ? 1u : 0u);
    }
};

class CopyImmutableSamplerTest : public vkt::TestInstance
{
public:
    CopyImmutableSamplerTest(Context &context, const CopyImmutableSamplerParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~CopyImmutableSamplerTest(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const CopyImmutableSamplerParams m_params;
};

class CopyImmutableSamplerCase : public vkt::TestCase
{
public:
    CopyImmutableSamplerCase(tcu::TestContext &testCtx, const std::string &name,
                             const CopyImmutableSamplerParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~CopyImmutableSamplerCase(void) = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new CopyImmutableSamplerTest(context, m_params);
    }

protected:
    const CopyImmutableSamplerParams m_params;
};

void CopyImmutableSamplerCase::initPrograms(vk::SourceCollections &progCollection) const
{
    DE_ASSERT(m_params.samplerCount == 1u || m_params.samplerCount == 4u);

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "layout (location=0) out flat int quadrant;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n";

    if (m_params.samplerCount == 1u)
        vert << "    quadrant = 0;\n";
    else
    {
        vert << "    if (inPos.x < 0.0) {\n"
             << "        if (inPos.y < 0.0) {\n"
             << "            quadrant = 0;\n"
             << "        } else {\n"
             << "            quadrant = 1;\n"
             << "        }\n"
             << "    } else {\n"
             << "        if (inPos.y < 0.0) {\n"
             << "            quadrant = 2;\n"
             << "        } else {\n"
             << "            quadrant = 3;\n"
             << "        }\n"
             << "    }\n";
    }
    vert << "}\n";
    progCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const uint32_t bufferBinding    = m_params.getBufferBinding();
    const uint32_t imgBindingOffset = m_params.getImgBindingOffset();

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) in flat int quadrant;\n"
         << "layout (location=0) out vec4 outColor;\n";

    for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        frag << "layout (set=0, binding=" << (i + imgBindingOffset) << ") uniform sampler2D inSampler" << i << ";\n";

    frag << "layout (set=0, binding=" << bufferBinding << ") readonly buffer BufferBlock { float red; } inBuffer;\n"
         << "void main(void) {\n"
         << "    vec4 sampledColor = vec4(0.0);\n";

    if (m_params.samplerCount == 1u)
        frag << "    sampledColor = texture(inSampler0, vec2(0.0));\n";
    else
    {
        frag << "    if (quadrant == 0) {\n"
             << "        sampledColor = texture(inSampler0, vec2(0.0));\n"
             << "    } else if (quadrant == 1) {\n"
             << "        sampledColor = texture(inSampler1, vec2(0.0));\n"
             << "    } else if (quadrant == 2) {\n"
             << "        sampledColor = texture(inSampler2, vec2(0.0));\n"
             << "    } else if (quadrant == 3) {\n"
             << "        sampledColor = texture(inSampler3, vec2(0.0));\n"
             << "    }\n";
    }

    frag << "    sampledColor.r = inBuffer.red;\n"
         << "    outColor = sampledColor;\n"
         << "}\n";
    progCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus CopyImmutableSamplerTest::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto imageExtent = makeExtent3D(1u, 1u, 1u);
    const tcu::IVec3 fbExtentV(2, 2, 1);
    const auto fbExtent   = makeExtent3D(fbExtentV);
    const auto imageUsage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto fbUsage    = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR   = makeDefaultImageSubresourceRange();

    // We will create kImageCount combined image samplers with an immutable sampler.
    const VkSamplerCreateInfo samplerCreateInfo = initVulkanStructure();
    const auto sampler                          = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    using ImageWithMemoryPtr = std::unique_ptr<ImageWithMemory>;
    std::vector<ImageWithMemoryPtr> images;
    images.reserve(m_params.samplerCount);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        imageFormat,
        imageExtent,
        1u,
        1u,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        imageUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    std::vector<Move<VkImageView>> imageViews;
    imageViews.reserve(m_params.samplerCount);

    for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
    {
        images.emplace_back(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any));
        const auto image = images.back()->get();
        imageViews.push_back(makeImageView(ctx.vkd, ctx.device, image, VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR));
    }

    // Now a storage buffer that contains different red values.
    struct BufferData
    {
        float redValues[8];
        BufferData()
        {
            redValues[0] = 0.5;
            redValues[1] = 0.5;
            redValues[2] = 0.5;
            redValues[3] = 0.5;

            redValues[4] = 1.0;
            redValues[5] = 1.0;
            redValues[6] = 1.0;
            redValues[7] = 1.0;
        }
    };

    BufferData bufferData;
    const auto redBufferSize     = static_cast<VkDeviceSize>(sizeof(bufferData));
    const auto redBufferSizeHalf = redBufferSize / 2u;
    const auto redBufferUsage    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto redBufferInfo     = makeBufferCreateInfo(redBufferSize, redBufferUsage);
    BufferWithMemory redBuffer(ctx.vkd, ctx.device, ctx.allocator, redBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = redBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), &bufferData, sizeof(bufferData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // We also need a framebuffer and a vertex buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent, imageFormat, fbUsage, VK_IMAGE_TYPE_2D);

    const std::vector<tcu::Vec4> vertices{
        // clang-format off
        tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
        tcu::Vec4(-0.5f,  0.5f, 0.0f, 1.0f),
        tcu::Vec4( 0.5f, -0.5f, 0.0f, 1.0f),
        tcu::Vec4( 0.5f,  0.5f, 0.0f, 1.0f),
        // clang-format on
    };

    const auto vertBufferSize   = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertBufferUsage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertBufferOffset = static_cast<VkDeviceSize>(0);
    const auto vertBufferInfo   = makeBufferCreateInfo(vertBufferSize, vertBufferUsage);
    BufferWithMemory vertBuffer(ctx.vkd, ctx.device, ctx.allocator, vertBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = vertBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Descriptor pool and descriptor sets.
    const auto kSetCount = 2u;
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_params.samplerCount * kSetCount);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kSetCount);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, kSetCount);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    if (m_params.bufferFirst)
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        setLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                 VK_SHADER_STAGE_FRAGMENT_BIT, &sampler.get());
    if (!m_params.bufferFirst)
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    std::vector<Move<VkDescriptorSet>> descriptorSets(kSetCount);
    for (uint32_t i = 0u; i < kSetCount; ++i)
        descriptorSets.at(i) = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    // Update the first descriptor set manually.
    const uint32_t bufferBinding    = m_params.getBufferBinding();
    const uint32_t imgBindingOffset = m_params.getImgBindingOffset();

    {
        const auto binding = DescriptorSetUpdateBuilder::Location::binding;
        const auto descSet = descriptorSets.front().get();

        DescriptorSetUpdateBuilder updateBuilder;
        for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        {
            const auto descInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, imageViews.at(i).get(),
                                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            updateBuilder.writeSingle(descSet, binding(i + imgBindingOffset), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      &descInfo);
        }
        {
            const auto descInfo = makeDescriptorBufferInfo(*redBuffer, 0u, redBufferSizeHalf);
            updateBuilder.writeSingle(descSet, binding(bufferBinding), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfo);
        }

        updateBuilder.update(ctx.vkd, ctx.device);
    }
    // Copy combined image sampler descriptors to the second set.
    {
        // First, write the red buffer descriptor info.
        const auto descInfo = makeDescriptorBufferInfo(*redBuffer, redBufferSizeHalf, redBufferSizeHalf);
        const VkWriteDescriptorSet writeDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptorSets.back().get(),
            bufferBinding,
            0u,
            1u,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr,
            &descInfo,
            nullptr,
        };

        ctx.vkd.updateDescriptorSets(ctx.device, 1u, &writeDescriptorSet, 0u, nullptr);

        // Then, copy the immutable sampler descriptors.
        const VkCopyDescriptorSet copyDescriptorSet = {
            VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
            nullptr,
            descriptorSets.front().get(),
            imgBindingOffset,
            0u,
            descriptorSets.back().get(),
            imgBindingOffset,
            0u,
            m_params.samplerCount, // Copies all descriptors at the same time.
        };

        ctx.vkd.updateDescriptorSets(ctx.device, 0u, nullptr, 1u, &copyDescriptorSet);
    }

    // Create pipeline.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, imageFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), fbExtent.width, fbExtent.height);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertShader, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    // Colors for each image, plus the clear color.
    const std::vector<tcu::Vec4> imageColors{
        tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
        tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f), // Clear color.
    };
    DE_ASSERT(de::sizeU32(imageColors) >= m_params.samplerCount + 1u);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    // Prepare images.
    {
        std::vector<VkImageMemoryBarrier> preClearBarriers;
        preClearBarriers.reserve(m_params.samplerCount);

        for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        {
            const auto barrier =
                makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, images.at(i)->get(), colorSRR);
            preClearBarriers.push_back(barrier);
        }
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, preClearBarriers.data(), preClearBarriers.size());

        for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        {
            const auto clearColor = makeClearValueColor(imageColors.at(i));
            ctx.vkd.cmdClearColorImage(cmdBuffer, images.at(i)->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &clearColor.color, 1u, &colorSRR);
        }

        std::vector<VkImageMemoryBarrier> postClearBarriers;
        postClearBarriers.reserve(m_params.samplerCount);

        for (uint32_t i = 0u; i < m_params.samplerCount; ++i)
        {
            const auto barrier = makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, images.at(i)->get(), colorSRR);
            postClearBarriers.push_back(barrier);
        }
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, postClearBarriers.data(),
                                      postClearBarriers.size());
    }
    // Draw half the vertices with the first set and the other half with the other set.
    {
        DE_ASSERT(de::sizeU32(vertices) % 2u == 0u);
        const auto halfVertexCount = de::sizeU32(vertices) / 2u;
        const auto bindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;

        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertBuffer.get(), &vertBufferOffset);

        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), imageColors.back());
        {
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.front().get(),
                                          0u, nullptr);
            ctx.vkd.cmdDraw(cmdBuffer, halfVertexCount, 1u, 0u, 0u);
        }
        {
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.back().get(),
                                          0u, nullptr);
            ctx.vkd.cmdDraw(cmdBuffer, halfVertexCount, 1u, halfVertexCount, 0u);
        }
        endRenderPass(ctx.vkd, cmdBuffer);
    }
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtentV.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const auto tcuFormat = mapVkFormat(imageFormat);
    tcu::TextureLevel refLevel(tcuFormat, fbExtentV.x(), fbExtentV.y(), fbExtentV.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    {
        const auto firstColorOffset = tcu::Vec4(bufferData.redValues[0], 0.0f, 0.0f, 0.0f);
        const auto secondColorOffset =
            tcu::Vec4(bufferData.redValues[de::arrayLength(bufferData.redValues) / 2u], 0.0f, 0.0f, 0.0f);

        // Reference: each quadrant should have its own color according to the shader.
        const auto quadrantSize = fbExtentV / tcu::IVec3(2, 2, 1);
        const auto halfSize     = fbExtentV / tcu::IVec3(2, 1, 1);

        if (m_params.samplerCount == 1u)
        {
            const auto left = tcu::getSubregion(reference, 0, 0, halfSize.x(), halfSize.y());
            tcu::clear(left, imageColors.at(0) + firstColorOffset);

            const auto right = tcu::getSubregion(reference, halfSize.x(), 0, halfSize.x(), halfSize.y());
            tcu::clear(right, imageColors.at(0) + secondColorOffset);
        }
        else
        {
            const auto topLeft = tcu::getSubregion(reference, 0, 0, quadrantSize.x(), quadrantSize.y());
            tcu::clear(topLeft, imageColors.at(0) + firstColorOffset);

            const auto bottomLeft =
                tcu::getSubregion(reference, 0, quadrantSize.y(), quadrantSize.x(), quadrantSize.y());
            tcu::clear(bottomLeft, imageColors.at(1) + firstColorOffset);

            const auto topRight = tcu::getSubregion(reference, quadrantSize.x(), 0, quadrantSize.x(), quadrantSize.y());
            tcu::clear(topRight, imageColors.at(2) + secondColorOffset);

            const auto bottomRight =
                tcu::getSubregion(reference, quadrantSize.x(), quadrantSize.y(), quadrantSize.x(), quadrantSize.y());
            tcu::clear(bottomRight, imageColors.at(3) + secondColorOffset);
        }
    }

    auto &fbAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, fbAlloc);
    tcu::ConstPixelBufferAccess result(tcuFormat, fbExtentV, fbAlloc.getHostPtr());

    auto &log                  = m_context.getTestContext().getLog();
    const float thresholdValue = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(thresholdValue);
    if (!tcu::floatThresholdCompare(log, "Result", "", reference, result, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected result in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

void createTestsForAllDescriptorTypes(tcu::TestContext &testCtx, de::MovePtr<tcu::TestCaseGroup> &parentGroup,
                                      PipelineType pipelineType, bool useUpdateAfterBind = false)
{
    addDescriptorCopyTests<UniformBufferDescriptor>(testCtx, parentGroup, "uniform_buffer", pipelineType,
                                                    useUpdateAfterBind);
    addDescriptorCopyTests<StorageBufferDescriptor>(testCtx, parentGroup, "storage_buffer", pipelineType,
                                                    useUpdateAfterBind);
    addDescriptorCopyTests<CombinedImageSamplerDescriptor>(testCtx, parentGroup, "combined_image_sampler", pipelineType,
                                                           useUpdateAfterBind);
    addDescriptorCopyTests<StorageImageDescriptor>(testCtx, parentGroup, "storage_image", pipelineType,
                                                   useUpdateAfterBind);
    addDescriptorCopyTests<UniformTexelBufferDescriptor>(testCtx, parentGroup, "uniform_texel_buffer", pipelineType,
                                                         useUpdateAfterBind);
    addDescriptorCopyTests<StorageTexelBufferDescriptor>(testCtx, parentGroup, "storage_texel_buffer", pipelineType,
                                                         useUpdateAfterBind);

#ifndef CTS_USES_VULKANSC
    addDescriptorCopyTests<InlineUniformBlockDescriptor>(testCtx, parentGroup, "inline_uniform_block", pipelineType,
                                                         useUpdateAfterBind);
#endif

    // create tests that can be run only without UpdateAfterBind
    if (useUpdateAfterBind == false)
    {
        addDescriptorCopyTests<DynamicUniformBufferDescriptor>(testCtx, parentGroup, "uniform_buffer_dynamic",
                                                               pipelineType, false);
        addDescriptorCopyTests<DynamicStorageBufferDescriptor>(testCtx, parentGroup, "storage_buffer_dynamic",
                                                               pipelineType, false);

        // create tests that are graphics pipeline specific
        if (pipelineType == PIPELINE_TYPE_GRAPHICS)
            addDescriptorCopyTests<InputAttachmentDescriptor>(testCtx, parentGroup, "input_attachment",
                                                              PIPELINE_TYPE_GRAPHICS, false);

        addMixedDescriptorCopyTests(testCtx, parentGroup, pipelineType);
    }

    addSamplerCopyTests(testCtx, parentGroup, pipelineType, useUpdateAfterBind);
    addSampledImageCopyTests(testCtx, parentGroup, pipelineType, useUpdateAfterBind);
}

tcu::TestCaseGroup *createDescriptorCopyTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr descriptorCopyGroup(new tcu::TestCaseGroup(testCtx, "descriptor_copy"));

    GroupPtr computeGroup(new tcu::TestCaseGroup(testCtx, "compute"));
    GroupPtr graphicsGroup(new tcu::TestCaseGroup(testCtx, "graphics"));
    // Graphics tests with update after bind
    GroupPtr graphicsUABGroup(new tcu::TestCaseGroup(testCtx, "graphics_uab"));
    GroupPtr miscGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    createTestsForAllDescriptorTypes(testCtx, computeGroup, PIPELINE_TYPE_COMPUTE);
    createTestsForAllDescriptorTypes(testCtx, graphicsGroup, PIPELINE_TYPE_GRAPHICS);
    createTestsForAllDescriptorTypes(testCtx, graphicsUABGroup, PIPELINE_TYPE_GRAPHICS, true);

    {
        for (const uint32_t samplerCount : {1u, 4u})
            for (const bool bufferFirst : {false, true})
            {
                const CopyImmutableSamplerParams params{samplerCount, bufferFirst};
                const auto testName = "copy_immutable_sampler_" + std::to_string(samplerCount) + "_images" +
                                      (bufferFirst ? "_buffer_first" : "");
                miscGroup->addChild(new CopyImmutableSamplerCase(testCtx, testName, params));
            }
    }

    descriptorCopyGroup->addChild(computeGroup.release());
    descriptorCopyGroup->addChild(graphicsGroup.release());
    descriptorCopyGroup->addChild(graphicsUABGroup.release());
    descriptorCopyGroup->addChild(miscGroup.release());

    return descriptorCopyGroup.release();
}

} // namespace BindingModel
} // namespace vkt
