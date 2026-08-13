#ifndef _VKTINDEPENDENTSETSUTIL_HPP
#define _VKTINDEPENDENTSETSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 LunarG, Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Independent Sets Utils
 *//*--------------------------------------------------------------------*/
#include "vktTestCase.hpp"
#include "vkDefs.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include <map>
#include <vector>
#include <string>
#include <utility>

namespace vkt
{
namespace IndependentSets
{

#ifndef CTS_USES_VULKANSC

typedef std::map<vk::VkShaderStageFlagBits, uint32_t> StageIndexMap;
typedef std::pair<vk::VkDeviceSize, vk::VkDeviceSize> StrideAndOffset;

// General information for a descriptor on a set.
struct DescriptorInfo
{
    vk::VkDescriptorType descryptorType;
    uint32_t count;        // Descriptor count: only applies when isArray is true. Should be 1 otherwise.
    uint32_t attIndex;     // Input attachment index. Only applies to input attachments.
    uint32_t offsetFactor; // Applies to dynamic buffers only: how many times to multiply the min offset alignment.
    bool isArray;
    bool write; // Only applies to descriptor types that can be written to.

    DescriptorInfo(vk::VkDescriptorType descriptorType_, uint32_t count_, uint32_t attIndex_, uint32_t offsetFactor_,
                   bool isArray_, bool write_);

    // For buffers, calculate the stride of the buffer elements and the offset of the element in the buffer. Normally
    // buffers will contain a single element, but for dynamic buffers we store the element with an offset so the offsets
    // are nonzero and a bit more interesting.
    StrideAndOffset getStrideAndOffset(const vk::VkPhysicalDeviceLimits &limits) const;
};

struct SetInfo
{
    std::vector<DescriptorInfo> descriptors;
    bool isPush;

    SetInfo(bool isPush_);
};

struct Params
{
    vk::PipelineConstructionType constructionType;
    vk::VkShaderStageFlags stages; // Which stages will be used.
    uint32_t caseIndex;            // A case index for the RNG.
    bool ioFirst;                  // Put the IO buffer in the first set.
    bool avoidMaintenance11;       // For shader object tests, do not use maintenance11.

    // Returns true if the stage is included in `stages`.
    bool hasStage(vk::VkShaderStageFlagBits stage) const;

    // Returns a string with concatenated brief stage names. E.g. vert_frag.
    std::string getStageNames() const;

    // The sets associated to each shader stage may be offsetted by 1 when the first set is used for the IO buffer.
    uint32_t getSetIndexOffset() const;

    // Assign a set index to each shader stage, according to the contents of `stages`.
    StageIndexMap getSetIndexMap() const;

    // Generate pseudorandom descriptor set information according to the current parameters. We make it part of the
    // parameters to be able to use this information both in the Test Case as well as the Test Instance, guaranteeing
    // we will obtain the same result in both calls.
    std::vector<SetInfo> genSetInfos() const;

    // Obtain a seed for the RNG.
    uint32_t getSeed() const;
};

class Instance : public vkt::TestInstance
{
public:
    Instance(Context &context, const Params &params);
    virtual ~Instance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class Case : public vkt::TestCase
{
public:
    Case(tcu::TestContext &testCtx, const std::string &name, const Params &params);
    virtual ~Case(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const Params m_params;
};

tcu::TestCaseGroup *createRandomTests(tcu::TestContext &testCtx, const std::string &groupName,
                                      const std::vector<vk::PipelineConstructionType> &constructionTypes);

#endif // CTS_USES_VULKANSC

} // namespace IndependentSets
} // namespace vkt

#endif // _VKTINDEPENDENTSETSUTIL_HPP
