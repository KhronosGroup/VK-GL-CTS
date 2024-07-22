/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
 * \brief Texture multisample tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureMultisampleTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

tcu::TestCaseGroup *createAtomicTests(tcu::TestContext &testCtx)
{
    // Test atomic operations on multisample textures
    de::MovePtr<tcu::TestCaseGroup> atomic(new tcu::TestCaseGroup(testCtx, "atomic"));
#ifndef CTS_USES_VULKANSC
    static const char dataDir[] = "texture/multisample/atomic";

    struct TestCase
    {
        std::string name;
        VkFormat format;
        bool requiresInt64;
    };

    TestCase cases[] = {{"storage_image_r32i", VK_FORMAT_R32_SINT, false},
                        {"storage_image_r32ui", VK_FORMAT_R32_UINT, false},
                        {"storage_image_r64i", VK_FORMAT_R64_SINT, true},
                        {"storage_image_r64ui", VK_FORMAT_R64_UINT, true}};

    std::vector<std::string> requirements;

    requirements.push_back("Features.shaderStorageImageMultisample");

    for (const auto &testCase : cases)
    {
        if (testCase.requiresInt64)
        {
            requirements.push_back("Features.shaderInt64");
        }

        const VkImageCreateInfo vkImageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // sType
            DE_NULL,                             // pNext
            0,                                   // flags
            VK_IMAGE_TYPE_2D,                    // imageType
            testCase.format,                     // format
            {64, 64, 1},                         // extent
            1,                                   // mipLevels
            1,                                   // arrayLayers
            VK_SAMPLE_COUNT_4_BIT,               // samples
            VK_IMAGE_TILING_OPTIMAL,             // tiling
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, // usage
            VK_SHARING_MODE_EXCLUSIVE, // sharingMode
            0,                         // queueFamilyIndexCount
            DE_NULL,                   // pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED, // initialLayout
        };

        std::vector<VkImageCreateInfo> imageRequirements = {vkImageCreateInfo};

        const std::string fileName              = testCase.name + ".amber";
        cts_amber::AmberTestCase *amberTestCase = cts_amber::createAmberTestCase(
            testCtx, testCase.name.c_str(), dataDir, fileName, requirements, imageRequirements);

        atomic->addChild(amberTestCase);

        // Remove the requirement after adding the test case to avoid affecting the next iteration
        if (testCase.requiresInt64)
        {
            requirements.pop_back();
        }
    }
#endif

    return atomic.release();
}

tcu::TestCaseGroup *createInvalidSampleIndexTests(tcu::TestContext &testCtx)
{
    std::pair<std::string, VkSampleCountFlagBits> cases[] = {
        {"sample_count_2", VK_SAMPLE_COUNT_2_BIT},   {"sample_count_4", VK_SAMPLE_COUNT_4_BIT},
        {"sample_count_8", VK_SAMPLE_COUNT_8_BIT},   {"sample_count_16", VK_SAMPLE_COUNT_16_BIT},
        {"sample_count_32", VK_SAMPLE_COUNT_32_BIT}, {"sample_count_64", VK_SAMPLE_COUNT_64_BIT}};

    // Writes to invalid sample indices should be discarded.
    de::MovePtr<tcu::TestCaseGroup> invalidWrites(new tcu::TestCaseGroup(testCtx, "invalid_sample_index"));
    static const char dataDir[]           = "texture/multisample/invalidsampleindex";
    std::vector<std::string> requirements = {"Features.shaderStorageImageMultisample"};

    for (const auto &testCase : cases)
    {
        const VkImageCreateInfo vkImageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // sType
            DE_NULL,                             // pNext
            0,                                   // flags
            VK_IMAGE_TYPE_2D,                    // imageType
            VK_FORMAT_R8G8B8A8_UNORM,            // format
            {16, 16, 1},                         // extent
            1,                                   // mipLevels
            1,                                   // arrayLayers
            testCase.second,                     // samples
            VK_IMAGE_TILING_OPTIMAL,             // tiling
            VK_IMAGE_USAGE_SAMPLED_BIT,          // usage
            VK_SHARING_MODE_EXCLUSIVE,           // sharingMode
            0,                                   // queueFamilyIndexCount
            DE_NULL,                             // pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED,           // initialLayout
        };

        std::vector<VkImageCreateInfo> imageRequirements = {vkImageCreateInfo};
        const std::string fileName                       = testCase.first + ".amber";

        invalidWrites->addChild(cts_amber::createAmberTestCase(testCtx, testCase.first.c_str(), dataDir, fileName,
                                                               requirements, imageRequirements));
    }

    return invalidWrites.release();
}

} // namespace

tcu::TestCaseGroup *createTextureMultisampleTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> multisample(new tcu::TestCaseGroup(testCtx, "multisample"));

    multisample->addChild(createAtomicTests(testCtx));
    multisample->addChild(createInvalidSampleIndexTests(testCtx));

    return multisample.release();
}

} // namespace texture
} // namespace vkt
