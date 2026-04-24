/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Ray Tracing Limits tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingLimitsTests.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkRayTracingUtil.hpp"

#include "deRandom.hpp"

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace tcu;

enum PropertyType
{
    PROP_TYPE_ACCELERATION_STRUCT = 0,
    PROP_TYPE_RAY_TRACING_PIPELINE
};

class RayTracingLimitsTest : public TestCase
{
public:
    RayTracingLimitsTest(TestContext &testCtx, const std::string &name, const PropertyType propType);
    ~RayTracingLimitsTest(void);

    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;

private:
    PropertyType m_propType;
};

RayTracingLimitsTest::RayTracingLimitsTest(TestContext &testCtx, const std::string &name, const PropertyType propType)
    : TestCase(testCtx, name)
    , m_propType(propType)
{
}

RayTracingLimitsTest::~RayTracingLimitsTest(void)
{
}

void RayTracingLimitsTest::checkSupport(Context &context) const
{
    if (m_propType == PROP_TYPE_ACCELERATION_STRUCT)
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");

    if (m_propType == PROP_TYPE_RAY_TRACING_PIPELINE)
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
}

class RayTracingLimitsTestInstance : public TestInstance
{
public:
    RayTracingLimitsTestInstance(Context &context, const PropertyType propType);
    ~RayTracingLimitsTestInstance(void);
    TestStatus iterate(void);

private:
    const PropertyType m_propType;
    de::Random m_rnd;
};

RayTracingLimitsTestInstance::RayTracingLimitsTestInstance(Context &context, const PropertyType propType)
    : TestInstance(context)
    , m_propType(propType)
    , m_rnd(1234)
{
}

RayTracingLimitsTestInstance::~RayTracingLimitsTestInstance(void)
{
}

TestStatus RayTracingLimitsTestInstance::iterate(void)
{
    const InstanceInterface &vki = m_context.getInstanceInterface();

    const uint32_t testIterations = m_rnd.getInt(1u, 20u);

    for (uint32_t iterIdx = 0; iterIdx < testIterations; iterIdx++)
    {
        const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

        const de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, physicalDevice);

        if (m_propType == PROP_TYPE_ACCELERATION_STRUCT)
        {
            {
                const uint64_t geometryCount    = rayTracingProperties->getMaxGeometryCount();
                const uint64_t geometryCountMin = (1 << 24u) - 1;
                const uint64_t geometryCountMax = UINT32_MAX;

                if ((geometryCount < geometryCountMin) || (geometryCount > geometryCountMax))
                    return TestStatus::fail("Property maxGeometryCount is not within supported limits");
            }

            {
                const uint64_t instanceCount    = rayTracingProperties->getMaxInstanceCount();
                const uint64_t instanceCountMin = (1 << 24u) - 1;
                const uint64_t instanceCountMax = UINT32_MAX;

                if ((instanceCount < instanceCountMin) || (instanceCount > instanceCountMax))
                    return TestStatus::fail("Property maxInstanceCount is not within supported limits");
            }

            {
                const uint64_t primitiveCount    = rayTracingProperties->getMaxPrimitiveCount();
                const uint64_t primitiveCountMin = (1 << 29u) - 1;
                const uint64_t primitiveCountMax = UINT32_MAX;

                if ((primitiveCount < primitiveCountMin) || (primitiveCount > primitiveCountMax))
                    return TestStatus::fail("Property maxPrimitiveCount is not within supported limits");
            }

            {
                const uint32_t perStageDescriptorAccelerationStructures =
                    rayTracingProperties->getMaxPerStageDescriptorAccelerationStructures();
                const uint32_t perStageDescriptorAccelerationStructuresMin = 16u;

                if (perStageDescriptorAccelerationStructures < perStageDescriptorAccelerationStructuresMin)
                    return TestStatus::fail(
                        "Property maxPerStageDescriptorAccelerationStructures is not within supported limits");
            }

            {
                const uint32_t perStageDescriptorUpdateAfterBindAccelerationStructures =
                    rayTracingProperties->getMaxPerStageDescriptorUpdateAfterBindAccelerationStructures();
                const uint32_t perStageDescriptorUpdateAfterBindAccelerationStructuresMin = 500000u;

                if (perStageDescriptorUpdateAfterBindAccelerationStructures <
                    perStageDescriptorUpdateAfterBindAccelerationStructuresMin)
                    return TestStatus::fail("Property maxPerStageDescriptorUpdateAfterBindAccelerationStructures is "
                                            "not within supported limits");
            }

            {
                const uint32_t descriptorSetAccelerationStructures =
                    rayTracingProperties->getMaxDescriptorSetAccelerationStructures();
                const uint32_t descriptorSetAccelerationStructuresMin = 16u;

                if (descriptorSetAccelerationStructures < descriptorSetAccelerationStructuresMin)
                    return TestStatus::fail(
                        "Property maxDescriptorSetAccelerationStructures is not within supported limits");
            }

            {
                const uint32_t descriptorSetUpdateAfterBindAccelerationStructures =
                    rayTracingProperties->getMaxDescriptorSetUpdateAfterBindAccelerationStructures();
                const uint32_t descriptorSetUpdateAfterBindAccelerationStructuresMin = 500000u;

                if (descriptorSetUpdateAfterBindAccelerationStructures <
                    descriptorSetUpdateAfterBindAccelerationStructuresMin)
                    return TestStatus::fail("Property  maxDescriptorSetUpdateAfterBindAccelerationStructures is not "
                                            "within supported limits");
            }

            {
                const uint32_t accelerationStructureScratchOffsetAlignment =
                    rayTracingProperties->getMinAccelerationStructureScratchOffsetAlignment();
                const uint32_t accelerationStructureScratchOffsetAlignmentMax = 256u;

                if (accelerationStructureScratchOffsetAlignment > accelerationStructureScratchOffsetAlignmentMax)
                    return TestStatus::fail(
                        "Property minAccelerationStructureScratchOffsetAlignment is not within supported limits");
            }
        }

        if (m_propType == PROP_TYPE_RAY_TRACING_PIPELINE)
        {
            {
                const uint32_t shaderGroupHandleSize      = rayTracingProperties->getShaderGroupHandleSize();
                const uint32_t shaderGroupHandleSizeConst = 32u;

                if (shaderGroupHandleSize != shaderGroupHandleSizeConst)
                    return TestStatus::fail("Property shaderGroupHandleSize is not within supported limits");
            }

            {
                const uint32_t recursionDepth    = rayTracingProperties->getMaxRecursionDepth();
                const uint32_t recursionDepthMin = 1u;

                if (recursionDepth < recursionDepthMin)
                    return TestStatus::fail("Property maxRayRecursionDepth is not within supported limits");
            }

            {
                const uint32_t shaderGroupStride    = rayTracingProperties->getMaxShaderGroupStride();
                const uint32_t shaderGroupStrideMin = 4096u;

                if (shaderGroupStride < shaderGroupStrideMin)
                    return TestStatus::fail("Property maxShaderGroupStride is not within supported limits");
            }

            {
                const uint32_t shaderGroupBaseAlignment    = rayTracingProperties->getShaderGroupBaseAlignment();
                const uint32_t shaderGroupBaseAlignmentMax = 64u;

                if (shaderGroupBaseAlignment > shaderGroupBaseAlignmentMax)
                    return TestStatus::fail("Property shaderGroupBaseAlignment is not within supported limits");
            }

            {
                const uint32_t shaderGroupHandleCaptureReplaySize =
                    rayTracingProperties->getShaderGroupHandleCaptureReplaySize();
                const uint32_t shaderGroupHandleCaptureReplaySizeMax = 64u;

                if (shaderGroupHandleCaptureReplaySize > shaderGroupHandleCaptureReplaySizeMax)
                    return TestStatus::fail(
                        "Property shaderGroupHandleCaptureReplaySize is not within supported limits");
            }

            {
                const uint32_t rayDispatchInvocationCount    = rayTracingProperties->getMaxRayDispatchInvocationCount();
                const uint32_t rayDispatchInvocationCountMin = (1 << 30u);

                if (rayDispatchInvocationCount < rayDispatchInvocationCountMin)
                    return TestStatus::fail("Property maxRayDispatchInvocationCount is not within supported limits");
            }

            {
                const uint32_t shaderGroupHandleAlignment    = rayTracingProperties->getShaderGroupHandleAlignment();
                const uint32_t shaderGroupHandleAlignmentMax = 32u;

                if (shaderGroupHandleAlignment > shaderGroupHandleAlignmentMax)
                    return TestStatus::fail("Property shaderGroupHandleAlignment is not within supported limits");
            }

            {
                const uint32_t rayHitAttributeSize    = rayTracingProperties->getMaxRayHitAttributeSize();
                const uint32_t rayHitAttributeSizeMin = 32u;

                if (rayHitAttributeSize < rayHitAttributeSizeMin)
                    return TestStatus::fail("Property maxRayHitAttributeSize is not within supported limits");
            }
        }
    }

    return TestStatus::pass("Pass");
}

TestInstance *RayTracingLimitsTest::createInstance(Context &context) const
{
    return new RayTracingLimitsTestInstance(context, m_propType);
}

} // namespace

tcu::TestCaseGroup *createLimitsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> limitsGroup(new tcu::TestCaseGroup(testCtx, "limits"));

    limitsGroup->addChild(new RayTracingLimitsTest(testCtx, "accel_struct_props", PROP_TYPE_ACCELERATION_STRUCT));
    limitsGroup->addChild(new RayTracingLimitsTest(testCtx, "ray_tracing_props", PROP_TYPE_RAY_TRACING_PIPELINE));

    return limitsGroup.release();
}

} // namespace RayTracing
} // namespace vkt
