#ifndef _VKTDYNAMICRENDERINGSUSPENDRESUMETESTSUTIL_HPP
#define _VKTDYNAMICRENDERINGSUSPENDRESUMETESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Dynamic rendering suspend/resume tests common code
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassGroupParams.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"

#include <memory>

namespace vkt
{
namespace renderpass
{
namespace SuspendResume
{

struct Params
{
    const SharedGroupParams groupParams;
    uint32_t seedOffset   = 0u;
    bool resolveDepth     = false;
    bool customResolve    = false;
    bool smallFramebuffer = false;

    // Extent of the framebuffer.
    tcu::IVec3 getExtent() const;

    // How many quadrants we will cover.
    uint32_t getQuadrantCount() const;

    // How many areas is a full framebuffer pixel subdivided in.
    uint32_t getSubpixelAreas() const;

    // How many triangles in each subpixel area.
    uint32_t getSubpixelTriangleCount() const;

    // How many draws calls in each full quadrant.
    uint32_t getQuadrantDraws() const;

    // How many vertices in a triangle (hint: 3).
    uint32_t getTriangleVertexCount() const;

    // Sample count to use for multisample attachments.
    vk::VkSampleCountFlagBits getSampleCount() const;

    // Maximum number of color attachments that we will consider (for indices).
    uint32_t getMaxColorAttachmentCount() const;

    // Seed for the pseudorandom number generator.
    uint32_t getSeed() const;

    // Actual number of color attachments used.
    static constexpr uint32_t kColorAttCount = 2u;

    struct DrawInfo
    {
        bool remapColorAtt                            = false;
        uint32_t colorAttIndices[kColorAttCount]      = {0u, 1u};
        bool remapInputAtt                            = false;
        uint32_t colorInputAttIndices[kColorAttCount] = {0u, 1u};
        uint32_t depthInputAttIndex                   = std::numeric_limits<uint32_t>::max();
        uint32_t pixelCount                           = 0u;
    };

    // Gets parameters for each draw.
    std::vector<DrawInfo> genDrawInfos() const;
};
using ParamsPtr = std::shared_ptr<const Params>;

class Instance : public vkt::TestInstance
{
public:
    Instance(Context &context, ParamsPtr params);
    virtual ~Instance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    ParamsPtr m_params;
};

class Case : public vkt::TestCase
{
public:
    Case(tcu::TestContext &testCtx, const std::string &name, ParamsPtr params);
    virtual ~Case(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &dst) const override;
    vkt::TestInstance *createInstance(Context &context) const override;

protected:
    ParamsPtr m_params;
};

} // namespace SuspendResume
} // namespace renderpass
} // namespace vkt

#endif // _VKTDYNAMICRENDERINGSUSPENDRESUMETESTSUTIL_HPP
