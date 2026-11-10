/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderTests.hpp"
#include "vktMeshShaderSmokeTests.hpp"
#include "vktMeshShaderSmokeTestsEXT.hpp"
#include "vktMeshShaderSyncTests.hpp"
#include "vktMeshShaderSyncTestsEXT.hpp"
#include "vktMeshShaderApiTests.hpp"
#include "vktMeshShaderApiTestsEXT.hpp"
#include "vktMeshShaderPropertyTests.hpp"
#include "vktMeshShaderBuiltinTests.hpp"
#include "vktMeshShaderBuiltinTestsEXT.hpp"
#include "vktMeshShaderMiscTests.hpp"
#include "vktMeshShaderMiscTestsEXT.hpp"
#include "vktMeshShaderInOutTestsEXT.hpp"
#include "vktMeshShaderPropertyTestsEXT.hpp"
#include "vktMeshShaderConditionalRenderingTestsEXT.hpp"
#include "vktMeshShaderProvokingVertexTestsEXT.hpp"
#include "vktMeshShaderQueryTestsEXT.hpp"
#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace MeshShader
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
}

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx_, const std::string &name)
{
    return createTestGroup2(testCtx_, name,
                            [](tcu::TestCaseGroup *mainGroup)
                            {
                                addTestGroup2(mainGroup, "nv",
                                              [](tcu::TestCaseGroup *nvGroup)
                                              {
                                                  auto &testCtx = nvGroup->getTestContext();
                                                  nvGroup->addChild(createMeshShaderSmokeTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderApiTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderSyncTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderPropertyTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderBuiltinTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderMiscTests(testCtx));
                                                  nvGroup->addChild(createMeshShaderInOutTests(testCtx));
                                              });
                                addTestGroup2(mainGroup, "ext",
                                              [](tcu::TestCaseGroup *extGroup)
                                              {
                                                  auto &testCtx = extGroup->getTestContext();
                                                  extGroup->addChild(createMeshShaderSmokeTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderApiTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderSyncTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderBuiltinTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderPipelineTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderMiscTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderInOutTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderPropertyTestsEXT(testCtx));
                                                  extGroup->addChild(
                                                      createMeshShaderConditionalRenderingTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderProvokingVertexTestsEXT(testCtx));
                                                  extGroup->addChild(createMeshShaderQueryTestsEXT(testCtx));
                                              });
                            });
}

} // namespace MeshShader
} // namespace vkt
