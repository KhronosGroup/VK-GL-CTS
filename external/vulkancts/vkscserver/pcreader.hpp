#ifndef _PCREADER_HPP
#define _PCREADER_HPP

/* Copyright (c) 2021, NVIDIA CORPORATION
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Daniel Koch <dkoch@nvidia.com>
 */

#include <cstddef>

#ifndef VKSC_ASSERT
#include <cassert>
#define VKSC_ASSERT assert
#endif // VKSC_ASSERT
#ifndef VKSC_MEMCMP
#include <cstring>
#define VKSC_MEMCMP memcmp
#endif // VKSC_MEMCPY

// Must be version 1.0.6 or newer
//#include <vulkan/vulkan_sc_core.hpp>

// Legacy Header for version 1.0.4
#define VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE_LEGACY (VkPipelineCacheHeaderVersion)1000298000
typedef struct VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy {
    VkPipelineCacheHeaderVersionOne     headerVersionOne;
    VkPipelineCacheValidationVersion    validationVersion;
    uint32_t                            pipelineIndexCount;
    uint32_t                            pipelineIndexStride;
    uint64_t                            pipelineIndexOffset;
} VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy;

// VKSCPipelineCacheHeaderReader
//
// Utility class to handle extracting information about pipelines from a pipeline cache blob.
//
// Instantiate the class with a pointer to the pipeline cache blob and the size.
// The pipeline cache blob is NOT copied and the application must maintain the lifetime
// of the data that was passed in while this object is instantiated.
// The cache blob will never be modified by this class.
//
// getSafetyCriticalOneHeader - return the safety critical header field
//
// getPipelineIndexEntry(index) - return the pipeline index entry for a specified index in the header
//
// getPipelineIndexEntry(UUID) - return the pipeline index entry for a specified pipeline identifier
//
// getJson - get a pointer to the json for a specfied pipeline index entry
//
// getStageIndexEntry - return the stage index entry for a specified pipeline index entry and stage
//
// getSPIRV - get a pointer to the SPIRV code for a specified stage index entry
//


class VKSCPipelineCacheHeaderReader
{
public:
    // initialize the pipeline cache header reader with <cacheSize> bytes of data starting at <cacheData>
    // the pipeline cache is not copied, but the pointer is saved
    // cacheData is never modified
    VKSCPipelineCacheHeaderReader(uint64_t cacheSize, const uint8_t* cacheData)
        : m_CacheSize{cacheSize}, m_CacheData{cacheData}
    {
        const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 =
            reinterpret_cast<const VkPipelineCacheHeaderVersionSafetyCriticalOne*>(m_CacheData);

        m_IsLegacy = (sc1->headerVersionOne.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE_LEGACY);
    }

    // basic quick check of the referenced pipeline cache data
    // make sure m_CacheData starts with a well-formed VkPipelineCacheHeaderVersionSafetyCriticalOne structure
    bool isValid() const
    {
        const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 =
            reinterpret_cast<const VkPipelineCacheHeaderVersionSafetyCriticalOne*>(m_CacheData);

        if (sc1->headerVersionOne.headerSize != sizeof(VkPipelineCacheHeaderVersionSafetyCriticalOne) ||
            !(sc1->headerVersionOne.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE ||
              isLegacy()) ||
            sc1->validationVersion != VK_PIPELINE_CACHE_VALIDATION_VERSION_SAFETY_CRITICAL_ONE)
        {
            return false;
        }
        return true;
    }

    bool isLegacy() const { return m_IsLegacy; }

    // return pointer to the VkPipelineCacheHeaderVersionOne structure
    const VkPipelineCacheHeaderVersionOne* getHeaderVersionOne() const
    {
        const VkPipelineCacheHeaderVersionOne* const hv1 =
            reinterpret_cast<const VkPipelineCacheHeaderVersionOne*>(m_CacheData);

        return hv1;
    }

    // return the validation version from the SC1 header
    VkPipelineCacheValidationVersion getValidationVersion() const
    {
        if (isLegacy())
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* const sc1 = getSafetyCriticalOneHeaderLegacy();
            return sc1->validationVersion;
        }
        else
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 = getSafetyCriticalOneHeader();
            return sc1->validationVersion;
        }

    }

    // return the implementation data field from the SC1 header
    uint32_t getImplementationData() const
    {
        if (isLegacy())
        {
            return 0U;
        }
        else
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 = getSafetyCriticalOneHeader();
            return sc1->implementationData;
        }
    }

    // return the number of pipelines in the index
    uint32_t getPipelineIndexCount() const
    {
        if (isLegacy())
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* const sc1 = getSafetyCriticalOneHeaderLegacy();
            return sc1->pipelineIndexCount;
        }
        else
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 = getSafetyCriticalOneHeader();
            return sc1->pipelineIndexCount;
        }
    }

    // return the stride between pipeline index entries in the index
    uint32_t getPipelineIndexStride() const
    {
        if (isLegacy())
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* const sc1 = getSafetyCriticalOneHeaderLegacy();
            return sc1->pipelineIndexStride;
        }
        else
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 = getSafetyCriticalOneHeader();
            return sc1->pipelineIndexStride;
        }
    }

    // returns the offset to the start of pipeline index entries in the cache
    uint64_t getPipelineIndexOffset() const
    {
        if (isLegacy())
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* const sc1 = getSafetyCriticalOneHeaderLegacy();
            return sc1->pipelineIndexOffset;
        }
        else
        {
            const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 = getSafetyCriticalOneHeader();
            return sc1->pipelineIndexOffset;
        }
    }

    // return pointer to pipeline index entry by <index> in pipeline header
    // typically used for iterating over all pipelines in the cache
    // nullptr is returned if <index> is out of range
    const VkPipelineCacheSafetyCriticalIndexEntry* getPipelineIndexEntry(uint32_t index) const
    {
        if (index >= getPipelineIndexCount())
        {
            return nullptr;
        }

        uint64_t offset = getPipelineIndexOffset() + (index * getPipelineIndexStride());
        VKSC_ASSERT(offset + sizeof(VkPipelineCacheSafetyCriticalIndexEntry) <= m_CacheSize);

        const VkPipelineCacheSafetyCriticalIndexEntry* const pipelineIndexEntry =
            reinterpret_cast<const VkPipelineCacheSafetyCriticalIndexEntry*>(m_CacheData + offset);

        return pipelineIndexEntry;
    }

    // return pointer to pipeline index entry for requested pipeline identifier
    // nullptr is returned if not found
    const VkPipelineCacheSafetyCriticalIndexEntry* getPipelineIndexEntry(const uint8_t identifier[VK_UUID_SIZE]) const
    {
        const uint32_t pipelineIndexCount = getPipelineIndexCount();
        const uint32_t pipelineIndexStride = getPipelineIndexStride();
        const uint64_t pipelineIndexOffset = getPipelineIndexOffset();

        for (uint32_t i = 0U; i < pipelineIndexCount; ++i)
        {
            uint64_t offset = pipelineIndexOffset + (i * pipelineIndexStride);
            VKSC_ASSERT(offset + sizeof(VkPipelineCacheSafetyCriticalIndexEntry) <= m_CacheSize);

            const VkPipelineCacheSafetyCriticalIndexEntry* const pipelineIndexEntry =
                reinterpret_cast<const VkPipelineCacheSafetyCriticalIndexEntry*>(m_CacheData + offset);

            if (VKSC_MEMCMP(identifier, pipelineIndexEntry->pipelineIdentifier, VK_UUID_SIZE) == 0U)
            {
                return pipelineIndexEntry;
            }
        }

        return nullptr;
    }

    // return pointer to json for a given pipeline index entry
    // nullptr is returned if not present
    const uint8_t* getJson(const VkPipelineCacheSafetyCriticalIndexEntry* const pipelineIndexEntry) const
    {
        uint64_t offset = pipelineIndexEntry->jsonOffset;
        if (0U == offset) return nullptr;

        VKSC_ASSERT(offset + pipelineIndexEntry->jsonSize <= m_CacheSize);

        return (m_CacheData + offset);
    }

    // return pointer to stage validation index entry given a pipeline index entry <pipelineIndexEntry> and <stage>
    // nullptr is returned if not present
    const VkPipelineCacheStageValidationIndexEntry* getStageIndexEntry(const VkPipelineCacheSafetyCriticalIndexEntry* const pipelineIndexEntry, uint32_t stage) const
    {
        if (stage >= pipelineIndexEntry->stageIndexCount) return nullptr;

        uint64_t offset = pipelineIndexEntry->stageIndexOffset + (stage * pipelineIndexEntry->stageIndexStride);
        VKSC_ASSERT(offset + sizeof(VkPipelineCacheStageValidationIndexEntry) <= m_CacheSize);

        const VkPipelineCacheStageValidationIndexEntry* const stageIndexEntry =
            reinterpret_cast<const VkPipelineCacheStageValidationIndexEntry*>(m_CacheData + offset);

        return stageIndexEntry;
    }

    // return pointer to spirv code in the pipeline cache for a given stage index entry
    // nullptr is returned if not present
    const uint8_t* getSPIRV(const VkPipelineCacheStageValidationIndexEntry* const stageIndexEntry) const
    {
        uint64_t offset = stageIndexEntry->codeOffset;
        if (0U == offset) return nullptr;

        VKSC_ASSERT(offset + stageIndexEntry->codeSize <= m_CacheSize);

        return (m_CacheData + offset);
    }

private:
    // return pointer to the pipeline cache SafetyCriticalOne structure
    const VkPipelineCacheHeaderVersionSafetyCriticalOne* getSafetyCriticalOneHeader() const
    {
        const VkPipelineCacheHeaderVersionSafetyCriticalOne* const sc1 =
            reinterpret_cast<const VkPipelineCacheHeaderVersionSafetyCriticalOne*>(m_CacheData);

        return sc1;
    }

    // return pointer to the pipeline cache SafetyCriticalOneLegacy structure
    const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* getSafetyCriticalOneHeaderLegacy() const
    {
        const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy* const sc1 =
            reinterpret_cast<const VkPipelineCacheHeaderVersionSafetyCriticalOneLegacy*>(m_CacheData);

        return sc1;
    }

    const uint64_t m_CacheSize;          // size of data pointed to by m_CacheData in bytes
    const uint8_t* const m_CacheData;    // pipeline cache data being read by this reader
    bool m_IsLegacy;                     // is legacy (pre 1.0.5) pipeline cache format

};

#endif // _PCREADER_HPP
