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
 *//*!
 * \file
 * \brief Geometry Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktGeometryTests.hpp"
#include "vktGeometryBasicGeometryShaderTests.hpp"
#include "vktGeometryInputGeometryShaderTests.hpp"
#include "vktGeometryLayeredRenderingTests.hpp"
#include "vktGeometryInstancedRenderingTests.hpp"
#include "vktGeometryVaryingGeometryShaderTests.hpp"
#include "vktGeometryEmitGeometryShaderTests.hpp"
#include "vktGeometryBuiltinVariableGeometryShaderTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace geometry
{
namespace
{

void createChildren(tcu::TestCaseGroup *geometryTests)
{
    tcu::TestContext &testCtx = geometryTests->getTestContext();

    geometryTests->addChild(createInputGeometryShaderTests(testCtx));
    geometryTests->addChild(createBasicGeometryShaderTests(testCtx));
    geometryTests->addChild(createLayeredRenderingTests(testCtx));
    geometryTests->addChild(createInstancedRenderingTests(testCtx));
    geometryTests->addChild(createVaryingGeometryShaderTests(testCtx));
    geometryTests->addChild(createEmitGeometryShaderTests(testCtx));
    geometryTests->addChild(createBuiltinVariableGeometryShaderTests(testCtx));
}

} // namespace

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name.c_str(), createChildren);
}

} // namespace geometry
} // namespace vkt
