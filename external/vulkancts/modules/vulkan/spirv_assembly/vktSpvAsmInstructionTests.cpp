/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief SPIR-V Assembly Tests for Instructions (special opcode/operand)
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmInstructionTests.hpp"
#include "vktAmberTestCase.hpp"

#include "tcuCommandLine.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuInterval.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deMath.h"
#include "deRandom.hpp"
#include "tcuStringTemplate.hpp"

#include "vktSpvAsmCrossStageInterfaceTests.hpp"
#include "vktSpvAsm8bitStorageTests.hpp"
#include "vktSpvAsm16bitStorageTests.hpp"
#include "vktSpvAsmUboMatrixPaddingTests.hpp"
#include "vktSpvAsmConditionalBranchTests.hpp"
#include "vktSpvAsmIndexingTests.hpp"
#include "vktSpvAsmImageSamplerTests.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmFloatControlsTests.hpp"
#include "vktSpvAsmFromHlslTests.hpp"
#include "vktSpvAsmEmptyStructTests.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktSpvAsmVariablePointersTests.hpp"
#include "vktSpvAsmVariableInitTests.hpp"
#include "vktSpvAsmPointerParameterTests.hpp"
#include "vktSpvAsmSpirvVersion1p4Tests.hpp"
#include "vktSpvAsmSpirvVersionTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSpvAsmLoopDepLenTests.hpp"
#include "vktSpvAsmLoopDepInfTests.hpp"
#include "vktSpvAsmCompositeInsertTests.hpp"
#include "vktSpvAsmVaryingNameTests.hpp"
#include "vktSpvAsmWorkgroupMemoryTests.hpp"
#include "vktSpvAsmSignedIntCompareTests.hpp"
#include "vktSpvAsmSignedOpTests.hpp"
#include "vktSpvAsmPtrAccessChainTests.hpp"
#include "vktSpvAsmVectorShuffleTests.hpp"
#include "vktSpvAsmFloatControlsExtensionlessTests.hpp"
#include "vktSpvAsmNonSemanticInfoTests.hpp"
#include "vktSpvAsm64bitCompareTests.hpp"
#include "vktSpvAsmTrinaryMinMaxTests.hpp"
#include "vktSpvAsmTerminateInvocationTests.hpp"
#include "vktSpvAsmIntegerDotProductTests.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <stack>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::IVec4;
using tcu::RGBA;
using tcu::TestLog;
using tcu::TestStatus;
using tcu::Vec4;
using de::UniquePtr;
using tcu::StringTemplate;
using tcu::Vec4;

const bool TEST_WITH_NAN	= true;
const bool TEST_WITHOUT_NAN	= false;

const string loadScalarF16FromUint =
	"%ld_arg_${var} = OpFunction %f16 None %f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_conv = OpBitcast %u32 %ld_arg_${var}_param\n"
	"%ld_arg_${var}_div = OpUDiv %u32 %ld_arg_${var}_conv %c_u32_2\n"
	"%ld_arg_${var}_and_low = OpBitwiseAnd %u32 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_div\n"
	"%ld_arg_${var}_ld = OpLoad %u32 %ld_arg_${var}_gep\n"
	"%ld_arg_${var}_unpack = OpBitcast %v2f16 %ld_arg_${var}_ld\n"
	"%ld_arg_${var}_ex = OpVectorExtractDynamic %f16 %ld_arg_${var}_unpack %ld_arg_${var}_and_low\n"
	"OpReturnValue %ld_arg_${var}_ex\n"
	"OpFunctionEnd\n";

const string loadV2F16FromUint =
	"%ld_arg_${var} = OpFunction %v2f16 None %v2f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param\n"
	"%ld_arg_${var}_ld = OpLoad %u32 %ld_arg_${var}_gep\n"
	"%ld_arg_${var}_cast = OpBitcast %v2f16 %ld_arg_${var}_ld\n"
	"OpReturnValue %ld_arg_${var}_cast\n"
	"OpFunctionEnd\n";

const string loadV3F16FromUints =
	// Since we allocate a vec4 worth of values, this case is almost the
	// same as that case.
	"%ld_arg_${var} = OpFunction %v3f16 None %v3f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_ld0 = OpLoad %u32 %ld_arg_${var}_gep0\n"
	"%ld_arg_${var}_bc0 = OpBitcast %v2f16 %ld_arg_${var}_ld0\n"
	"%ld_arg_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_ld1 = OpLoad %u32 %ld_arg_${var}_gep1\n"
	"%ld_arg_${var}_bc1 = OpBitcast %v2f16 %ld_arg_${var}_ld1\n"
	"%ld_arg_${var}_shuffle = OpVectorShuffle %v3f16 %ld_arg_${var}_bc0 %ld_arg_${var}_bc1 0 1 2\n"
	"OpReturnValue %ld_arg_${var}_shuffle\n"
	"OpFunctionEnd\n";

const string loadV4F16FromUints =
	"%ld_arg_${var} = OpFunction %v4f16 None %v4f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_ld0 = OpLoad %u32 %ld_arg_${var}_gep0\n"
	"%ld_arg_${var}_bc0 = OpBitcast %v2f16 %ld_arg_${var}_ld0\n"
	"%ld_arg_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_ld1 = OpLoad %u32 %ld_arg_${var}_gep1\n"
	"%ld_arg_${var}_bc1 = OpBitcast %v2f16 %ld_arg_${var}_ld1\n"
	"%ld_arg_${var}_shuffle = OpVectorShuffle %v4f16 %ld_arg_${var}_bc0 %ld_arg_${var}_bc1 0 1 2 3\n"
	"OpReturnValue %ld_arg_${var}_shuffle\n"
	"OpFunctionEnd\n";

const string loadM2x2F16FromUints =
	"%ld_arg_${var} = OpFunction %m2x2f16 None %m2x2f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_ld0 = OpLoad %u32 %ld_arg_${var}_gep0\n"
	"%ld_arg_${var}_bc0 = OpBitcast %v2f16 %ld_arg_${var}_ld0\n"
	"%ld_arg_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_ld1 = OpLoad %u32 %ld_arg_${var}_gep1\n"
	"%ld_arg_${var}_bc1 = OpBitcast %v2f16 %ld_arg_${var}_ld1\n"
	"%ld_arg_${var}_cons = OpCompositeConstruct %m2x2f16 %ld_arg_${var}_bc0 %ld_arg_${var}_bc1\n"
	"OpReturnValue %ld_arg_${var}_cons\n"
	"OpFunctionEnd\n";

const string loadM2x3F16FromUints =
	"%ld_arg_${var} = OpFunction %m2x3f16 None %m2x3f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m2x3f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM2x4F16FromUints =
	"%ld_arg_${var} = OpFunction %m2x4f16 None %m2x4f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2 3\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2 3\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m2x4f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM3x2F16FromUints =
	"%ld_arg_${var} = OpFunction %m3x2f16 None %m3x2f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep2 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_ld0 = OpLoad %u32 %ld_arg_${var}_gep0\n"
	"%ld_arg_${var}_ld1 = OpLoad %u32 %ld_arg_${var}_gep1\n"
	"%ld_arg_${var}_ld2 = OpLoad %u32 %ld_arg_${var}_gep2\n"
	"%ld_arg_${var}_bc0 = OpBitcast %v2f16 %ld_arg_${var}_ld0\n"
	"%ld_arg_${var}_bc1 = OpBitcast %v2f16 %ld_arg_${var}_ld1\n"
	"%ld_arg_${var}_bc2 = OpBitcast %v2f16 %ld_arg_${var}_ld2\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m3x2f16 %ld_arg_${var}_bc0 %ld_arg_${var}_bc1 %ld_arg_${var}_bc2\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM3x3F16FromUints =
	"%ld_arg_${var} = OpFunction %m3x3f16 None %m3x3f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_4\n"
	"%ld_arg_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_5\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_ld20 = OpLoad %u32 %ld_arg_${var}_gep20\n"
	"%ld_arg_${var}_ld21 = OpLoad %u32 %ld_arg_${var}_gep21\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_bc20 = OpBitcast %v2f16 %ld_arg_${var}_ld20\n"
	"%ld_arg_${var}_bc21 = OpBitcast %v2f16 %ld_arg_${var}_ld21\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2\n"
	"%ld_arg_${var}_vec2 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc20 %ld_arg_${var}_bc21 0 1 2\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m3x3f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1 %ld_arg_${var}_vec2\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM3x4F16FromUints =
	"%ld_arg_${var} = OpFunction %m3x4f16 None %m3x4f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_4\n"
	"%ld_arg_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_5\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_ld20 = OpLoad %u32 %ld_arg_${var}_gep20\n"
	"%ld_arg_${var}_ld21 = OpLoad %u32 %ld_arg_${var}_gep21\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_bc20 = OpBitcast %v2f16 %ld_arg_${var}_ld20\n"
	"%ld_arg_${var}_bc21 = OpBitcast %v2f16 %ld_arg_${var}_ld21\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2 3\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2 3\n"
	"%ld_arg_${var}_vec2 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc20 %ld_arg_${var}_bc21 0 1 2 3\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m3x4f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1 %ld_arg_${var}_vec2\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM4x2F16FromUints =
	"%ld_arg_${var} = OpFunction %m4x2f16 None %m4x2f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep2 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep3 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_ld0 = OpLoad %u32 %ld_arg_${var}_gep0\n"
	"%ld_arg_${var}_ld1 = OpLoad %u32 %ld_arg_${var}_gep1\n"
	"%ld_arg_${var}_ld2 = OpLoad %u32 %ld_arg_${var}_gep2\n"
	"%ld_arg_${var}_ld3 = OpLoad %u32 %ld_arg_${var}_gep3\n"
	"%ld_arg_${var}_bc0 = OpBitcast %v2f16 %ld_arg_${var}_ld0\n"
	"%ld_arg_${var}_bc1 = OpBitcast %v2f16 %ld_arg_${var}_ld1\n"
	"%ld_arg_${var}_bc2 = OpBitcast %v2f16 %ld_arg_${var}_ld2\n"
	"%ld_arg_${var}_bc3 = OpBitcast %v2f16 %ld_arg_${var}_ld3\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m4x2f16 %ld_arg_${var}_bc0 %ld_arg_${var}_bc1 %ld_arg_${var}_bc2 %ld_arg_${var}_bc3\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM4x3F16FromUints =
	"%ld_arg_${var} = OpFunction %m4x3f16 None %m4x3f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_4\n"
	"%ld_arg_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_5\n"
	"%ld_arg_${var}_gep30 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_6\n"
	"%ld_arg_${var}_gep31 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_7\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_ld20 = OpLoad %u32 %ld_arg_${var}_gep20\n"
	"%ld_arg_${var}_ld21 = OpLoad %u32 %ld_arg_${var}_gep21\n"
	"%ld_arg_${var}_ld30 = OpLoad %u32 %ld_arg_${var}_gep30\n"
	"%ld_arg_${var}_ld31 = OpLoad %u32 %ld_arg_${var}_gep31\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_bc20 = OpBitcast %v2f16 %ld_arg_${var}_ld20\n"
	"%ld_arg_${var}_bc21 = OpBitcast %v2f16 %ld_arg_${var}_ld21\n"
	"%ld_arg_${var}_bc30 = OpBitcast %v2f16 %ld_arg_${var}_ld30\n"
	"%ld_arg_${var}_bc31 = OpBitcast %v2f16 %ld_arg_${var}_ld31\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2\n"
	"%ld_arg_${var}_vec2 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc20 %ld_arg_${var}_bc21 0 1 2\n"
	"%ld_arg_${var}_vec3 = OpVectorShuffle %v3f16 %ld_arg_${var}_bc30 %ld_arg_${var}_bc31 0 1 2\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m4x3f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1 %ld_arg_${var}_vec2 %ld_arg_${var}_vec3\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string loadM4x4F16FromUints =
	"%ld_arg_${var} = OpFunction %m4x4f16 None %m4x4f16_i32_fn\n"
	"%ld_arg_${var}_param = OpFunctionParameter %i32\n"
	"%ld_arg_${var}_entry = OpLabel\n"
	"%ld_arg_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_0\n"
	"%ld_arg_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_1\n"
	"%ld_arg_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_2\n"
	"%ld_arg_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_3\n"
	"%ld_arg_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_4\n"
	"%ld_arg_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_5\n"
	"%ld_arg_${var}_gep30 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_6\n"
	"%ld_arg_${var}_gep31 = OpAccessChain %up_u32 %${var} %c_u32_0 %ld_arg_${var}_param %c_u32_7\n"
	"%ld_arg_${var}_ld00 = OpLoad %u32 %ld_arg_${var}_gep00\n"
	"%ld_arg_${var}_ld01 = OpLoad %u32 %ld_arg_${var}_gep01\n"
	"%ld_arg_${var}_ld10 = OpLoad %u32 %ld_arg_${var}_gep10\n"
	"%ld_arg_${var}_ld11 = OpLoad %u32 %ld_arg_${var}_gep11\n"
	"%ld_arg_${var}_ld20 = OpLoad %u32 %ld_arg_${var}_gep20\n"
	"%ld_arg_${var}_ld21 = OpLoad %u32 %ld_arg_${var}_gep21\n"
	"%ld_arg_${var}_ld30 = OpLoad %u32 %ld_arg_${var}_gep30\n"
	"%ld_arg_${var}_ld31 = OpLoad %u32 %ld_arg_${var}_gep31\n"
	"%ld_arg_${var}_bc00 = OpBitcast %v2f16 %ld_arg_${var}_ld00\n"
	"%ld_arg_${var}_bc01 = OpBitcast %v2f16 %ld_arg_${var}_ld01\n"
	"%ld_arg_${var}_bc10 = OpBitcast %v2f16 %ld_arg_${var}_ld10\n"
	"%ld_arg_${var}_bc11 = OpBitcast %v2f16 %ld_arg_${var}_ld11\n"
	"%ld_arg_${var}_bc20 = OpBitcast %v2f16 %ld_arg_${var}_ld20\n"
	"%ld_arg_${var}_bc21 = OpBitcast %v2f16 %ld_arg_${var}_ld21\n"
	"%ld_arg_${var}_bc30 = OpBitcast %v2f16 %ld_arg_${var}_ld30\n"
	"%ld_arg_${var}_bc31 = OpBitcast %v2f16 %ld_arg_${var}_ld31\n"
	"%ld_arg_${var}_vec0 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc00 %ld_arg_${var}_bc01 0 1 2 3\n"
	"%ld_arg_${var}_vec1 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc10 %ld_arg_${var}_bc11 0 1 2 3\n"
	"%ld_arg_${var}_vec2 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc20 %ld_arg_${var}_bc21 0 1 2 3\n"
	"%ld_arg_${var}_vec3 = OpVectorShuffle %v4f16 %ld_arg_${var}_bc30 %ld_arg_${var}_bc31 0 1 2 3\n"
	"%ld_arg_${var}_mat = OpCompositeConstruct %m4x4f16 %ld_arg_${var}_vec0 %ld_arg_${var}_vec1 %ld_arg_${var}_vec2 %ld_arg_${var}_vec3\n"
	"OpReturnValue %ld_arg_${var}_mat\n"
	"OpFunctionEnd\n";

const string storeScalarF16AsUint =
	// This version is sensitive to the initial value in the output buffer.
	// The infrastructure sets all output buffer bits to one before invoking
	// the shader so this version uses an atomic and to generate the correct
	// zeroes.
	"%st_fn_${var} = OpFunction %void None %void_f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_and_low = OpBitwiseAnd %u32 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_zero_vec = OpBitcast %v2f16 %c_u32_0\n"
	"%st_fn_${var}_insert = OpVectorInsertDynamic %v2f16 %st_fn_${var}_zero_vec %st_fn_${var}_param1 %st_fn_${var}_and_low\n"
	"%st_fn_${var}_odd = OpIEqual %bool %st_fn_${var}_and_low %c_u32_1\n"
	// Or 16 bits of ones into the half that was not populated with the result.
	"%st_fn_${var}_sel = OpSelect %u32 %st_fn_${var}_odd %c_u32_low_ones %c_u32_high_ones\n"
	"%st_fn_${var}_cast = OpBitcast %u32 %st_fn_${var}_insert\n"
	"%st_fn_${var}_or = OpBitwiseOr %u32 %st_fn_${var}_cast %st_fn_${var}_sel\n"
	"%st_fn_${var}_conv = OpBitcast %u32 %st_fn_${var}_param2\n"
	"%st_fn_${var}_div = OpUDiv %u32 %st_fn_${var}_conv %c_u32_2\n"
	"%st_fn_${var}_gep = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_div\n"
	"%st_fn_${var}_and = OpAtomicAnd %u32 %st_fn_${var}_gep %c_u32_1 %c_u32_0 %st_fn_${var}_or\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeV2F16AsUint =
	"%st_fn_${var} = OpFunction %void None %void_v2f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %v2f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_cast = OpBitcast %u32 %st_fn_${var}_param1\n"
	"%st_fn_${var}_gep = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2\n"
	"OpStore %st_fn_${var}_gep %st_fn_${var}_cast\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeV3F16AsUints =
	// Since we allocate a vec4 worth of values, this case can be treated
	// almost the same as a vec4 case. We will store some extra data that
	// should not be compared.
	"%st_fn_${var} = OpFunction %void None %void_v3f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %v3f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_shuffle0 = OpVectorShuffle %v2f16 %st_fn_${var}_param1 %st_fn_${var}_param1 0 1\n"
	"%st_fn_${var}_shuffle1 = OpVectorShuffle %v2f16 %st_fn_${var}_param1 %st_fn_${var}_param1 2 3\n"
	"%st_fn_${var}_bc0 = OpBitcast %u32 %st_fn_${var}_shuffle0\n"
	"%st_fn_${var}_bc1 = OpBitcast %u32 %st_fn_${var}_shuffle1\n"
	"%st_fn_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"OpStore %st_fn_${var}_gep0 %st_fn_${var}_bc0\n"
	"OpStore %st_fn_${var}_gep1 %st_fn_${var}_bc1\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeV4F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_v4f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %v4f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_shuffle0 = OpVectorShuffle %v2f16 %st_fn_${var}_param1 %st_fn_${var}_param1 0 1\n"
	"%st_fn_${var}_shuffle1 = OpVectorShuffle %v2f16 %st_fn_${var}_param1 %st_fn_${var}_param1 2 3\n"
	"%st_fn_${var}_bc0 = OpBitcast %u32 %st_fn_${var}_shuffle0\n"
	"%st_fn_${var}_bc1 = OpBitcast %u32 %st_fn_${var}_shuffle1\n"
	"%st_fn_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"OpStore %st_fn_${var}_gep0 %st_fn_${var}_bc0\n"
	"OpStore %st_fn_${var}_gep1 %st_fn_${var}_bc1\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM2x2F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m2x2f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m2x2f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_bc0 = OpBitcast %u32 %st_fn_${var}_ex0\n"
	"%st_fn_${var}_bc1 = OpBitcast %u32 %st_fn_${var}_ex1\n"
	"%st_fn_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"OpStore %st_fn_${var}_gep0 %st_fn_${var}_bc0\n"
	"OpStore %st_fn_${var}_gep1 %st_fn_${var}_bc1\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM2x3F16AsUints =
	// In the extracted elements for 01 and 11 the second element doesn't
	// matter.
	"%st_fn_${var} = OpFunction %void None %void_m2x3f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m2x3f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM2x4F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m2x4f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m2x4f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM3x2F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m3x2f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m3x2f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_bc0 = OpBitcast %u32 %st_fn_${var}_ex0\n"
	"%st_fn_${var}_bc1 = OpBitcast %u32 %st_fn_${var}_ex1\n"
	"%st_fn_${var}_bc2 = OpBitcast %u32 %st_fn_${var}_ex2\n"
	"%st_fn_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep2 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"OpStore %st_fn_${var}_gep0 %st_fn_${var}_bc0\n"
	"OpStore %st_fn_${var}_gep1 %st_fn_${var}_bc1\n"
	"OpStore %st_fn_${var}_gep2 %st_fn_${var}_bc2\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM3x3F16AsUints =
	// The second element of the each broken down vec3 doesn't matter.
	"%st_fn_${var} = OpFunction %void None %void_m3x3f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m3x3f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_ele20 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 0 1\n"
	"%st_fn_${var}_ele21 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_bc20 = OpBitcast %u32 %st_fn_${var}_ele20\n"
	"%st_fn_${var}_bc21 = OpBitcast %u32 %st_fn_${var}_ele21\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"%st_fn_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_4\n"
	"%st_fn_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_5\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpStore %st_fn_${var}_gep20 %st_fn_${var}_bc20\n"
	"OpStore %st_fn_${var}_gep21 %st_fn_${var}_bc21\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM3x4F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m3x4f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m3x4f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_ele20 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 0 1\n"
	"%st_fn_${var}_ele21 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_bc20 = OpBitcast %u32 %st_fn_${var}_ele20\n"
	"%st_fn_${var}_bc21 = OpBitcast %u32 %st_fn_${var}_ele21\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"%st_fn_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_4\n"
	"%st_fn_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_5\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpStore %st_fn_${var}_gep20 %st_fn_${var}_bc20\n"
	"OpStore %st_fn_${var}_gep21 %st_fn_${var}_bc21\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM4x2F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m4x2f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m4x2f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_ex3 = OpCompositeExtract %v2f16 %st_fn_${var}_param1 3\n"
	"%st_fn_${var}_bc0 = OpBitcast %u32 %st_fn_${var}_ex0\n"
	"%st_fn_${var}_bc1 = OpBitcast %u32 %st_fn_${var}_ex1\n"
	"%st_fn_${var}_bc2 = OpBitcast %u32 %st_fn_${var}_ex2\n"
	"%st_fn_${var}_bc3 = OpBitcast %u32 %st_fn_${var}_ex3\n"
	"%st_fn_${var}_gep0 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep1 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep2 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep3 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"OpStore %st_fn_${var}_gep0 %st_fn_${var}_bc0\n"
	"OpStore %st_fn_${var}_gep1 %st_fn_${var}_bc1\n"
	"OpStore %st_fn_${var}_gep2 %st_fn_${var}_bc2\n"
	"OpStore %st_fn_${var}_gep3 %st_fn_${var}_bc3\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM4x3F16AsUints =
	// The last element of each decomposed vec3 doesn't matter.
	"%st_fn_${var} = OpFunction %void None %void_m4x3f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m4x3f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_ex3 = OpCompositeExtract %v3f16 %st_fn_${var}_param1 3\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_ele20 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 0 1\n"
	"%st_fn_${var}_ele21 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 2 3\n"
	"%st_fn_${var}_ele30 = OpVectorShuffle %v2f16 %st_fn_${var}_ex3 %st_fn_${var}_ex3 0 1\n"
	"%st_fn_${var}_ele31 = OpVectorShuffle %v2f16 %st_fn_${var}_ex3 %st_fn_${var}_ex3 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_bc20 = OpBitcast %u32 %st_fn_${var}_ele20\n"
	"%st_fn_${var}_bc21 = OpBitcast %u32 %st_fn_${var}_ele21\n"
	"%st_fn_${var}_bc30 = OpBitcast %u32 %st_fn_${var}_ele30\n"
	"%st_fn_${var}_bc31 = OpBitcast %u32 %st_fn_${var}_ele31\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"%st_fn_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_4\n"
	"%st_fn_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_5\n"
	"%st_fn_${var}_gep30 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_6\n"
	"%st_fn_${var}_gep31 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_7\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpStore %st_fn_${var}_gep20 %st_fn_${var}_bc20\n"
	"OpStore %st_fn_${var}_gep21 %st_fn_${var}_bc21\n"
	"OpStore %st_fn_${var}_gep30 %st_fn_${var}_bc30\n"
	"OpStore %st_fn_${var}_gep31 %st_fn_${var}_bc31\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

const string storeM4x4F16AsUints =
	"%st_fn_${var} = OpFunction %void None %void_m4x4f16_i32_fn\n"
	"%st_fn_${var}_param1 = OpFunctionParameter %m4x4f16\n"
	"%st_fn_${var}_param2 = OpFunctionParameter %i32\n"
	"%st_fn_${var}_entry = OpLabel\n"
	"%st_fn_${var}_ex0 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 0\n"
	"%st_fn_${var}_ex1 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 1\n"
	"%st_fn_${var}_ex2 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 2\n"
	"%st_fn_${var}_ex3 = OpCompositeExtract %v4f16 %st_fn_${var}_param1 3\n"
	"%st_fn_${var}_ele00 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 0 1\n"
	"%st_fn_${var}_ele01 = OpVectorShuffle %v2f16 %st_fn_${var}_ex0 %st_fn_${var}_ex0 2 3\n"
	"%st_fn_${var}_ele10 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 0 1\n"
	"%st_fn_${var}_ele11 = OpVectorShuffle %v2f16 %st_fn_${var}_ex1 %st_fn_${var}_ex1 2 3\n"
	"%st_fn_${var}_ele20 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 0 1\n"
	"%st_fn_${var}_ele21 = OpVectorShuffle %v2f16 %st_fn_${var}_ex2 %st_fn_${var}_ex2 2 3\n"
	"%st_fn_${var}_ele30 = OpVectorShuffle %v2f16 %st_fn_${var}_ex3 %st_fn_${var}_ex3 0 1\n"
	"%st_fn_${var}_ele31 = OpVectorShuffle %v2f16 %st_fn_${var}_ex3 %st_fn_${var}_ex3 2 3\n"
	"%st_fn_${var}_bc00 = OpBitcast %u32 %st_fn_${var}_ele00\n"
	"%st_fn_${var}_bc01 = OpBitcast %u32 %st_fn_${var}_ele01\n"
	"%st_fn_${var}_bc10 = OpBitcast %u32 %st_fn_${var}_ele10\n"
	"%st_fn_${var}_bc11 = OpBitcast %u32 %st_fn_${var}_ele11\n"
	"%st_fn_${var}_bc20 = OpBitcast %u32 %st_fn_${var}_ele20\n"
	"%st_fn_${var}_bc21 = OpBitcast %u32 %st_fn_${var}_ele21\n"
	"%st_fn_${var}_bc30 = OpBitcast %u32 %st_fn_${var}_ele30\n"
	"%st_fn_${var}_bc31 = OpBitcast %u32 %st_fn_${var}_ele31\n"
	"%st_fn_${var}_gep00 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_0\n"
	"%st_fn_${var}_gep01 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_1\n"
	"%st_fn_${var}_gep10 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_2\n"
	"%st_fn_${var}_gep11 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_3\n"
	"%st_fn_${var}_gep20 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_4\n"
	"%st_fn_${var}_gep21 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_5\n"
	"%st_fn_${var}_gep30 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_6\n"
	"%st_fn_${var}_gep31 = OpAccessChain %up_u32 %${var} %c_u32_0 %st_fn_${var}_param2 %c_u32_7\n"
	"OpStore %st_fn_${var}_gep00 %st_fn_${var}_bc00\n"
	"OpStore %st_fn_${var}_gep01 %st_fn_${var}_bc01\n"
	"OpStore %st_fn_${var}_gep10 %st_fn_${var}_bc10\n"
	"OpStore %st_fn_${var}_gep11 %st_fn_${var}_bc11\n"
	"OpStore %st_fn_${var}_gep20 %st_fn_${var}_bc20\n"
	"OpStore %st_fn_${var}_gep21 %st_fn_${var}_bc21\n"
	"OpStore %st_fn_${var}_gep30 %st_fn_${var}_bc30\n"
	"OpStore %st_fn_${var}_gep31 %st_fn_${var}_bc31\n"
	"OpReturn\n"
	"OpFunctionEnd\n";

template<typename T>
static void fillRandomScalars (de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, int offset = 0)
{
	T* const typedPtr = (T*)dst;
	for (int ndx = 0; ndx < numValues; ndx++)
		typedPtr[offset + ndx] = de::randomScalar<T>(rnd, minValue, maxValue);
}

// Filter is a function that returns true if a value should pass, false otherwise.
template<typename T, typename FilterT>
static void fillRandomScalars (de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, FilterT filter, int offset = 0)
{
	T* const typedPtr = (T*)dst;
	T value;
	for (int ndx = 0; ndx < numValues; ndx++)
	{
		do
			value = de::randomScalar<T>(rnd, minValue, maxValue);
		while (!filter(value));

		typedPtr[offset + ndx] = value;
	}
}

// Gets a 64-bit integer with a more logarithmic distribution
deInt64 randomInt64LogDistributed (de::Random& rnd)
{
	deInt64 val = rnd.getUint64();
	val &= (1ull << rnd.getInt(1, 63)) - 1;
	if (rnd.getBool())
		val = -val;
	return val;
}

static void fillRandomInt64sLogDistributed (de::Random& rnd, vector<deInt64>& dst, int numValues)
{
	for (int ndx = 0; ndx < numValues; ndx++)
		dst[ndx] = randomInt64LogDistributed(rnd);
}

template<typename FilterT>
static void fillRandomInt64sLogDistributed (de::Random& rnd, vector<deInt64>& dst, int numValues, FilterT filter)
{
	for (int ndx = 0; ndx < numValues; ndx++)
	{
		deInt64 value;
		do {
			value = randomInt64LogDistributed(rnd);
		} while (!filter(value));
		dst[ndx] = value;
	}
}

inline bool filterNonNegative (const deInt64 value)
{
	return value >= 0;
}

inline bool filterPositive (const deInt64 value)
{
	return value > 0;
}

inline bool filterNotZero (const deInt64 value)
{
	return value != 0;
}

static void floorAll (vector<float>& values)
{
	for (size_t i = 0; i < values.size(); i++)
		values[i] = deFloatFloor(values[i]);
}

static void floorAll (vector<Vec4>& values)
{
	for (size_t i = 0; i < values.size(); i++)
		values[i] = floor(values[i]);
}

struct CaseParameter
{
	const char*		name;
	string			param;

	CaseParameter	(const char* case_, const string& param_) : name(case_), param(param_) {}
};

// Assembly code used for testing LocalSize, OpNop, OpConstant{Null|Composite}, Op[No]Line, OpSource[Continued], OpSourceExtension, OpUndef is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = -input_data.elements[x];
// }

static string getAsmForLocalSizeTest(bool useLiteralLocalSize, bool useSpecConstantWorkgroupSize, IVec3 workGroupSize, deUint32 ndx)
{
	std::ostringstream out;
	out << getComputeAsmShaderPreambleWithoutLocalSize();

	if (useLiteralLocalSize)
	{
		out << "OpExecutionMode %main LocalSize "
			<< workGroupSize.x() << " " << workGroupSize.y() << " " << workGroupSize.z() << "\n";
	}

	out << "OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n";

	if (useSpecConstantWorkgroupSize)
	{
		out << "OpDecorate %spec_0 SpecId 100\n"
			<< "OpDecorate %spec_1 SpecId 101\n"
			<< "OpDecorate %spec_2 SpecId 102\n"
			<< "OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n";
	}

	out << getComputeAsmInputOutputBufferTraits()
		<< getComputeAsmCommonTypes()
		<< getComputeAsmInputOutputBuffer()
		<< "%id        = OpVariable %uvec3ptr Input\n"
		<< "%zero      = OpConstant %i32 0 \n";

	if (useSpecConstantWorkgroupSize)
	{
		out	<< "%spec_0   = OpSpecConstant %u32 "<< workGroupSize.x() << "\n"
			<< "%spec_1   = OpSpecConstant %u32 "<< workGroupSize.y() << "\n"
			<< "%spec_2   = OpSpecConstant %u32 "<< workGroupSize.z() << "\n"
			<< "%gl_WorkGroupSize = OpSpecConstantComposite %uvec3 %spec_0 %spec_1 %spec_2\n";
	}

	out << "%main      = OpFunction %void None %voidf\n"
		<< "%label     = OpLabel\n"
		<< "%idval     = OpLoad %uvec3 %id\n"
		<< "%ndx         = OpCompositeExtract %u32 %idval " << ndx << "\n"

			"%inloc     = OpAccessChain %f32ptr %indata %zero %ndx\n"
			"%inval     = OpLoad %f32 %inloc\n"
			"%neg       = OpFNegate %f32 %inval\n"
			"%outloc    = OpAccessChain %f32ptr %outdata %zero %ndx\n"
			"             OpStore %outloc %neg\n"
			"             OpReturn\n"
			"             OpFunctionEnd\n";
	return out.str();
}

tcu::TestCaseGroup* createLocalSizeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "localsize", ""));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const deUint32					numElements		= 64u;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));

	spec.numWorkGroups = IVec3(numElements, 1, 1);

	spec.assembly = getAsmForLocalSizeTest(true, false, IVec3(1, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_localsize", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, true, IVec3(1, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_and_specid_localsize", "", spec));

	spec.assembly = getAsmForLocalSizeTest(false, true, IVec3(1, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "specid_localsize", "", spec));

	spec.numWorkGroups = IVec3(1, 1, 1);

	spec.assembly = getAsmForLocalSizeTest(true, false, IVec3(numElements, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_localsize_x", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, true, IVec3(numElements, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_and_specid_localsize_x", "", spec));

	spec.assembly = getAsmForLocalSizeTest(false, true, IVec3(numElements, 1, 1), 0u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "specid_localsize_x", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, false, IVec3(1, numElements, 1), 1u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_localsize_y", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, true, IVec3(1, numElements, 1), 1u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_and_specid_localsize_y", "", spec));

	spec.assembly = getAsmForLocalSizeTest(false, true, IVec3(1, numElements, 1), 1u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "specid_localsize_y", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, false, IVec3(1, 1, numElements), 2u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_localsize_z", "", spec));

	spec.assembly = getAsmForLocalSizeTest(true, true, IVec3(1, 1, numElements), 2u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "literal_and_specid_localsize_z", "", spec));

	spec.assembly = getAsmForLocalSizeTest(false, true, IVec3(1, 1, numElements), 2u);
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "specid_localsize_z", "", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpNopGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnop", "Test the OpNop instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes())

		+ string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"             OpNop\n" // Inside a function body

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpNop appearing at different places", spec));

	return group.release();
}

tcu::TestCaseGroup* createUnusedVariableComputeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "unused_variables", "Compute shaders with unused variables"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	const VariableLocation			testLocations[] =
	{
		// Set		Binding
		{ 0,		5			},
		{ 5,		5			},
	};

	for (size_t locationNdx = 0; locationNdx < DE_LENGTH_OF_ARRAY(testLocations); ++locationNdx)
	{
		const VariableLocation& location = testLocations[locationNdx];

		// Unused variable.
		{
			ComputeShaderSpec				spec;

			spec.assembly =
				string(getComputeAsmShaderPreamble()) +

				"OpDecorate %id BuiltIn GlobalInvocationId\n"

				+ getUnusedDecorations(location)

				+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes())

				+ getUnusedTypesAndConstants()

				+ string(getComputeAsmInputOutputBuffer())

				+ getUnusedBuffer() +

				"%id        = OpVariable %uvec3ptr Input\n"
				"%zero      = OpConstant %i32 0\n"

				"%main      = OpFunction %void None %voidf\n"
				"%label     = OpLabel\n"
				"%idval     = OpLoad %uvec3 %id\n"
				"%x         = OpCompositeExtract %u32 %idval 0\n"

				"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
				"%inval     = OpLoad %f32 %inloc\n"
				"%neg       = OpFNegate %f32 %inval\n"
				"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
				"             OpStore %outloc %neg\n"
				"             OpReturn\n"
				"             OpFunctionEnd\n";
			spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
			spec.numWorkGroups = IVec3(numElements, 1, 1);

			std::string testName		= "variable_" + location.toString();
			std::string testDescription	= "Unused variable test with " + location.toDescription();

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testDescription.c_str(), spec));
		}

		// Unused function.
		{
			ComputeShaderSpec				spec;

			spec.assembly =
				string(getComputeAsmShaderPreamble("", "", "", getUnusedEntryPoint())) +

				"OpDecorate %id BuiltIn GlobalInvocationId\n"

				+ getUnusedDecorations(location)

				+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes())

				+ getUnusedTypesAndConstants() +

				"%c_i32_0 = OpConstant %i32 0\n"
				"%c_i32_1 = OpConstant %i32 1\n"

				+ string(getComputeAsmInputOutputBuffer())

				+ getUnusedBuffer() +

				"%id        = OpVariable %uvec3ptr Input\n"
				"%zero      = OpConstant %i32 0\n"

				"%main      = OpFunction %void None %voidf\n"
				"%label     = OpLabel\n"
				"%idval     = OpLoad %uvec3 %id\n"
				"%x         = OpCompositeExtract %u32 %idval 0\n"

				"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
				"%inval     = OpLoad %f32 %inloc\n"
				"%neg       = OpFNegate %f32 %inval\n"
				"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
				"             OpStore %outloc %neg\n"
				"             OpReturn\n"
				"             OpFunctionEnd\n"

				+ getUnusedFunctionBody();

			spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
			spec.numWorkGroups = IVec3(numElements, 1, 1);

			std::string testName		= "function_" + location.toString();
			std::string testDescription	= "Unused function test with " + location.toDescription();

			group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), testDescription.c_str(), spec));
		}
	}

	return group.release();
}

template<bool nanSupported>
bool compareFUnord (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog& log)
{
	if (outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;
	vector<deUint8>	input2Bytes;
	vector<deUint8>	expectedBytes;

	inputs[0].getBytes(input1Bytes);
	inputs[1].getBytes(input2Bytes);
	expectedOutputs[0].getBytes(expectedBytes);

	const deInt32* const	expectedOutputAsInt		= reinterpret_cast<const deInt32*>(&expectedBytes.front());
	const deInt32* const	outputAsInt				= static_cast<const deInt32*>(outputAllocs[0]->getHostPtr());
	const float* const		input1AsFloat			= reinterpret_cast<const float*>(&input1Bytes.front());
	const float* const		input2AsFloat			= reinterpret_cast<const float*>(&input2Bytes.front());
	bool returnValue								= true;

	for (size_t idx = 0; idx < expectedBytes.size() / sizeof(deInt32); ++idx)
	{
		if (!nanSupported && (tcu::Float32(input1AsFloat[idx]).isNaN() || tcu::Float32(input2AsFloat[idx]).isNaN()))
			continue;

		if (outputAsInt[idx] != expectedOutputAsInt[idx])
		{
			log << TestLog::Message << "ERROR: Sub-case failed. inputs: " << input1AsFloat[idx] << "," << input2AsFloat[idx] << " output: " << outputAsInt[idx]<< " expected output: " << expectedOutputAsInt[idx] << TestLog::EndMessage;
			returnValue = false;
		}
	}
	return returnValue;
}

typedef VkBool32 (*compareFuncType) (float, float);

struct OpFUnordCase
{
	const char*		name;
	const char*		opCode;
	compareFuncType	compareFunc;

					OpFUnordCase			(const char* _name, const char* _opCode, compareFuncType _compareFunc)
						: name				(_name)
						, opCode			(_opCode)
						, compareFunc		(_compareFunc) {}
};

#define ADD_OPFUNORD_CASE(NAME, OPCODE, OPERATOR) \
do { \
	struct compare_##NAME { static VkBool32 compare(float x, float y) { return (x OPERATOR y) ? VK_TRUE : VK_FALSE; } }; \
	cases.push_back(OpFUnordCase(#NAME, OPCODE, compare_##NAME::compare)); \
} while (deGetFalse())

tcu::TestCaseGroup* createOpFUnordGroup (tcu::TestContext& testCtx, const bool testWithNan)
{
	const string					nan				= testWithNan ? "_nan" : "";
	const string					groupName		= "opfunord" + nan;
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, groupName.c_str(), "Test the OpFUnord* opcodes"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<OpFUnordCase>			cases;
	string							extensions		= testWithNan ? "OpExtension \"SPV_KHR_float_controls\"\n" : "";
	string							capabilities	= testWithNan ? "OpCapability SignedZeroInfNanPreserve\n" : "";
	string							exeModes		= testWithNan ? "OpExecutionMode %main SignedZeroInfNanPreserve 32\n" : "";
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble(capabilities, extensions, exeModes)) +
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %buf2 BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpDecorate %i32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"
		"OpMemberDecorate %buf2 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"

		"%buf2       = OpTypeStruct %i32arr\n"
		"%buf2ptr    = OpTypePointer Uniform %buf2\n"
		"%outdata    = OpVariable %buf2ptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%consti1   = OpConstant %i32 1\n"
		"%constf1   = OpConstant %f32 1.0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"

		"%result    = ${OPCODE} %bool %inval1 %inval2\n"
		"%int_res   = OpSelect %i32 %result %consti1 %zero\n"
		"             OpStore %outloc %int_res\n"

		"             OpReturn\n"
		"             OpFunctionEnd\n");

	ADD_OPFUNORD_CASE(equal, "OpFUnordEqual", ==);
	ADD_OPFUNORD_CASE(less, "OpFUnordLessThan", <);
	ADD_OPFUNORD_CASE(lessequal, "OpFUnordLessThanEqual", <=);
	ADD_OPFUNORD_CASE(greater, "OpFUnordGreaterThan", >);
	ADD_OPFUNORD_CASE(greaterequal, "OpFUnordGreaterThanEqual", >=);
	ADD_OPFUNORD_CASE(notequal, "OpFUnordNotEqual", !=);

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>			specializations;
		ComputeShaderSpec			spec;
		const float					NaN				= std::numeric_limits<float>::quiet_NaN();
		vector<float>				inputFloats1	(numElements, 0);
		vector<float>				inputFloats2	(numElements, 0);
		vector<deInt32>				expectedInts	(numElements, 0);

		specializations["OPCODE"]	= cases[caseNdx].opCode;
		spec.assembly				= shaderTemplate.specialize(specializations);

		fillRandomScalars(rnd, 1.f, 100.f, &inputFloats1[0], numElements);
		for (size_t ndx = 0; ndx < numElements; ++ndx)
		{
			switch (ndx % 6)
			{
				case 0:		inputFloats2[ndx] = inputFloats1[ndx] + 1.0f; break;
				case 1:		inputFloats2[ndx] = inputFloats1[ndx] - 1.0f; break;
				case 2:		inputFloats2[ndx] = inputFloats1[ndx]; break;
				case 3:		inputFloats2[ndx] = NaN; break;
				case 4:		inputFloats2[ndx] = inputFloats1[ndx];	inputFloats1[ndx] = NaN; break;
				case 5:		inputFloats2[ndx] = NaN;				inputFloats1[ndx] = NaN; break;
			}
			expectedInts[ndx] = tcu::Float32(inputFloats1[ndx]).isNaN() || tcu::Float32(inputFloats2[ndx]).isNaN() || cases[caseNdx].compareFunc(inputFloats1[ndx], inputFloats2[ndx]);
		}

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
		spec.outputs.push_back(BufferSp(new Int32Buffer(expectedInts)));
		spec.numWorkGroups	= IVec3(numElements, 1, 1);
		spec.verifyIO		= testWithNan ? &compareFUnord<true> : &compareFUnord<false>;

		if (testWithNan)
		{
			spec.extensions.push_back("VK_KHR_shader_float_controls");
			spec.requestedVulkanFeatures.floatControlsProperties.shaderSignedZeroInfNanPreserveFloat32 = DE_TRUE;
		}

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

struct OpAtomicCase
{
	const char*		name;
	const char*		assembly;
	const char*		retValAssembly;
	OpAtomicType	opAtomic;
	deInt32			numOutputElements;

					OpAtomicCase(const char* _name, const char* _assembly, const char* _retValAssembly, OpAtomicType _opAtomic, deInt32 _numOutputElements)
						: name				(_name)
						, assembly			(_assembly)
						, retValAssembly	(_retValAssembly)
						, opAtomic			(_opAtomic)
						, numOutputElements	(_numOutputElements) {}
};

tcu::TestCaseGroup* createOpAtomicGroup (tcu::TestContext& testCtx, bool useStorageBuffer, int numElements = 65535, bool verifyReturnValues = false, bool volatileAtomic = false)
{
	std::string						groupName			("opatomic");
	if (useStorageBuffer)
		groupName += "_storage_buffer";
	if (verifyReturnValues)
		groupName += "_return_values";
	if (volatileAtomic)
		groupName += "_volatile";
	de::MovePtr<tcu::TestCaseGroup>	group				(new tcu::TestCaseGroup(testCtx, groupName.c_str(), "Test the OpAtomic* opcodes"));
	vector<OpAtomicCase>			cases;

	const StringTemplate			shaderTemplate	(

		string("OpCapability Shader\n") +
		(volatileAtomic ? "OpCapability VulkanMemoryModelKHR\n" : "") +
		(useStorageBuffer ? "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n" : "") +
		(volatileAtomic ? "OpExtension \"SPV_KHR_vulkan_memory_model\"\n" : "") +
		(volatileAtomic ? "OpMemoryModel Logical VulkanKHR\n" : "OpMemoryModel Logical GLSL450\n") +
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n" +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf ${BLOCK_DECORATION}\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %i32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		"OpDecorate %sumbuf ${BLOCK_DECORATION}\n"
		"OpDecorate %sum DescriptorSet 0\n"
		"OpDecorate %sum Binding 1\n"
		"OpMemberDecorate %sumbuf 0 Offset 0\n"

		"${RETVAL_BUF_DECORATE}"

		+ getComputeAsmCommonTypes("${BLOCK_POINTER_TYPE}") +

		"%buf       = OpTypeStruct %i32arr\n"
		"%bufptr    = OpTypePointer ${BLOCK_POINTER_TYPE} %buf\n"
		"%indata    = OpVariable %bufptr ${BLOCK_POINTER_TYPE}\n"

		"%sumbuf    = OpTypeStruct %i32arr\n"
		"%sumbufptr = OpTypePointer ${BLOCK_POINTER_TYPE} %sumbuf\n"
		"%sum       = OpVariable %sumbufptr ${BLOCK_POINTER_TYPE}\n"

		"${RETVAL_BUF_DECL}"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%minusone  = OpConstant %i32 -1\n"
		"%zero      = OpConstant %i32 0\n"
		"%one       = OpConstant %u32 1\n"
		"%two       = OpConstant %i32 2\n"
		"%five      = OpConstant %i32 5\n"
		"%volbit    = OpConstant %i32 32768\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %i32ptr %indata %zero %x\n"
		"%inval     = OpLoad %i32 %inloc\n"

		"%outloc    = OpAccessChain %i32ptr %sum %zero ${INDEX}\n"
		"${INSTRUCTION}"
		"${RETVAL_ASSEMBLY}"

		"             OpReturn\n"
		"             OpFunctionEnd\n");

	#define ADD_OPATOMIC_CASE(NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC, NUM_OUTPUT_ELEMENTS) \
	do { \
		DE_ASSERT((NUM_OUTPUT_ELEMENTS) == 1 || (NUM_OUTPUT_ELEMENTS) == numElements); \
		cases.push_back(OpAtomicCase(#NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC, NUM_OUTPUT_ELEMENTS)); \
	} while (deGetFalse())
	#define ADD_OPATOMIC_CASE_1(NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC) ADD_OPATOMIC_CASE(NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC, 1)
	#define ADD_OPATOMIC_CASE_N(NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC) ADD_OPATOMIC_CASE(NAME, ASSEMBLY, RETVAL_ASSEMBLY, OPATOMIC, numElements)

	ADD_OPATOMIC_CASE_1(iadd,	"%retv      = OpAtomicIAdd %i32 %outloc ${SCOPE} ${SEMANTICS} %inval\n",
								"             OpStore %retloc %retv\n", OPATOMIC_IADD );
	ADD_OPATOMIC_CASE_1(isub,	"%retv      = OpAtomicISub %i32 %outloc ${SCOPE} ${SEMANTICS} %inval\n",
								"             OpStore %retloc %retv\n", OPATOMIC_ISUB );
	ADD_OPATOMIC_CASE_1(iinc,	"%retv      = OpAtomicIIncrement %i32 %outloc ${SCOPE} ${SEMANTICS}\n",
								"             OpStore %retloc %retv\n", OPATOMIC_IINC );
	ADD_OPATOMIC_CASE_1(idec,	"%retv      = OpAtomicIDecrement %i32 %outloc ${SCOPE} ${SEMANTICS}\n",
								"             OpStore %retloc %retv\n", OPATOMIC_IDEC );
	if (!verifyReturnValues)
	{
		ADD_OPATOMIC_CASE_N(load,	"%inval2    = OpAtomicLoad %i32 %inloc ${SCOPE} ${SEMANTICS}\n"
									"             OpStore %outloc %inval2\n", "", OPATOMIC_LOAD );
		ADD_OPATOMIC_CASE_N(store,	"             OpAtomicStore %outloc ${SCOPE} ${SEMANTICS} %inval\n", "", OPATOMIC_STORE );
	}

	ADD_OPATOMIC_CASE_N(compex, "%even      = OpSMod %i32 %inval %two\n"
								"             OpStore %outloc %even\n"
								"%retv      = OpAtomicCompareExchange %i32 %outloc ${SCOPE} ${SEMANTICS} ${SEMANTICS} %minusone %zero\n",
								"			  OpStore %retloc %retv\n", OPATOMIC_COMPEX );


	#undef ADD_OPATOMIC_CASE
	#undef ADD_OPATOMIC_CASE_1
	#undef ADD_OPATOMIC_CASE_N

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>			specializations;
		ComputeShaderSpec			spec;
		vector<deInt32>				inputInts		(numElements, 0);
		vector<deInt32>				expected		(cases[caseNdx].numOutputElements, -1);

		if (volatileAtomic)
		{
			spec.extensions.push_back("VK_KHR_vulkan_memory_model");
			spec.requestedVulkanFeatures.extVulkanMemoryModel = EXTVULKANMEMORYMODELFEATURES_ENABLE;

			// volatile, queuefamily scope
			specializations["SEMANTICS"] = "%volbit";
			specializations["SCOPE"] = "%five";
		}
		else
		{
			// non-volatile, device scope
			specializations["SEMANTICS"] = "%zero";
			specializations["SCOPE"] = "%one";
		}
		specializations["INDEX"]				= (cases[caseNdx].numOutputElements == 1) ? "%zero" : "%x";
		specializations["INSTRUCTION"]			= cases[caseNdx].assembly;
		specializations["BLOCK_DECORATION"]		= useStorageBuffer ? "Block" : "BufferBlock";
		specializations["BLOCK_POINTER_TYPE"]	= useStorageBuffer ? "StorageBuffer" : "Uniform";

		if (verifyReturnValues)
		{
			const StringTemplate blockDecoration	(
				"\n"
				"OpDecorate %retbuf ${BLOCK_DECORATION}\n"
				"OpDecorate %ret DescriptorSet 0\n"
				"OpDecorate %ret Binding 2\n"
				"OpMemberDecorate %retbuf 0 Offset 0\n\n");

			const StringTemplate blockDeclaration	(
				"\n"
				"%retbuf    = OpTypeStruct %i32arr\n"
				"%retbufptr = OpTypePointer ${BLOCK_POINTER_TYPE} %retbuf\n"
				"%ret       = OpVariable %retbufptr ${BLOCK_POINTER_TYPE}\n\n");

			specializations["RETVAL_ASSEMBLY"] =
				"%retloc    = OpAccessChain %i32ptr %ret %zero %x\n"
				+ std::string(cases[caseNdx].retValAssembly);

			specializations["RETVAL_BUF_DECORATE"]	= blockDecoration.specialize(specializations);
			specializations["RETVAL_BUF_DECL"]		= blockDeclaration.specialize(specializations);
		}
		else
		{
			specializations["RETVAL_ASSEMBLY"]		= "";
			specializations["RETVAL_BUF_DECORATE"]	= "";
			specializations["RETVAL_BUF_DECL"]		= "";
		}

		spec.assembly							= shaderTemplate.specialize(specializations);

		// Specialize one more time, to catch things that were in a template parameter
		const StringTemplate					assemblyTemplate(spec.assembly);
		spec.assembly							= assemblyTemplate.specialize(specializations);

		if (useStorageBuffer)
			spec.extensions.push_back("VK_KHR_storage_buffer_storage_class");

		spec.inputs.push_back(BufferSp(new OpAtomicBuffer(numElements, cases[caseNdx].numOutputElements, cases[caseNdx].opAtomic, BUFFERTYPE_INPUT)));
		spec.outputs.push_back(BufferSp(new OpAtomicBuffer(numElements, cases[caseNdx].numOutputElements, cases[caseNdx].opAtomic, BUFFERTYPE_EXPECTED)));
		if (verifyReturnValues)
			spec.outputs.push_back(BufferSp(new OpAtomicBuffer(numElements, cases[caseNdx].numOutputElements, cases[caseNdx].opAtomic, BUFFERTYPE_ATOMIC_RET)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		if (verifyReturnValues)
		{
			switch (cases[caseNdx].opAtomic)
			{
				case OPATOMIC_IADD:
					spec.verifyIO = OpAtomicBuffer::compareWithRetvals<OPATOMIC_IADD>;
					break;
				case OPATOMIC_ISUB:
					spec.verifyIO = OpAtomicBuffer::compareWithRetvals<OPATOMIC_ISUB>;
					break;
				case OPATOMIC_IINC:
					spec.verifyIO = OpAtomicBuffer::compareWithRetvals<OPATOMIC_IINC>;
					break;
				case OPATOMIC_IDEC:
					spec.verifyIO = OpAtomicBuffer::compareWithRetvals<OPATOMIC_IDEC>;
					break;
				case OPATOMIC_COMPEX:
					spec.verifyIO = OpAtomicBuffer::compareWithRetvals<OPATOMIC_COMPEX>;
					break;
				default:
					DE_FATAL("Unsupported OpAtomic type for return value verification");
			}
		}
		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpLineGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opline", "Test the OpLine instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"%fname1 = OpString \"negateInputs.comp\"\n"
		"%fname2 = OpString \"negateInputs\"\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) +

		"OpLine %fname1 0 0\n" // At the earliest possible position

		+ string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"OpLine %fname1 0 1\n" // Multiple OpLines in sequence
		"OpLine %fname2 1 0\n" // Different filenames
		"OpLine %fname1 1000 100000\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"OpLine %fname1 1 1\n" // Before a function

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"

		"OpLine %fname1 1 1\n" // In a function

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpLine appearing at different places", spec));

	return group.release();
}

bool veryfiBinaryShader (const ProgramBinary& binary)
{
	const size_t	paternCount			= 3u;
	bool paternsCheck[paternCount]		=
	{
		false, false, false
	};
	const string patersns[paternCount]	=
	{
		"VULKAN CTS",
		"Negative values",
		"Date: 2017/09/21"
	};
	size_t			paternNdx		= 0u;

	for (size_t ndx = 0u; ndx < binary.getSize(); ++ndx)
	{
		if (false == paternsCheck[paternNdx] &&
			patersns[paternNdx][0] == static_cast<char>(binary.getBinary()[ndx]) &&
			deMemoryEqual((const char*)&binary.getBinary()[ndx], &patersns[paternNdx][0], patersns[paternNdx].length()))
		{
			paternsCheck[paternNdx]= true;
			paternNdx++;
			if (paternNdx == paternCount)
				break;
		}
	}

	for (size_t ndx = 0u; ndx < paternCount; ++ndx)
	{
		if (!paternsCheck[ndx])
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpModuleProcessedGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opmoduleprocessed", "Test the OpModuleProcessed instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 10;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +
		"%fname = OpString \"negateInputs.comp\"\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"
		"OpModuleProcessed \"VULKAN CTS\"\n"					//OpModuleProcessed;
		"OpModuleProcessed \"Negative values\"\n"
		"OpModuleProcessed \"Date: 2017/09/21\"\n"
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits())

		+ string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"OpLine %fname 0 1\n"

		"OpLine %fname 1000 1\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%main      = OpFunction %void None %voidf\n"

		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyBinary = veryfiBinaryShader;
	spec.spirvVersion = SPIRV_VERSION_1_3;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpModuleProcessed Tests", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpNoLineGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnoline", "Test the OpNoLine instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"%fname = OpString \"negateInputs.comp\"\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) +

		"OpNoLine\n" // At the earliest possible position, without preceding OpLine

		+ string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"OpLine %fname 0 1\n"
		"OpNoLine\n" // Immediately following a preceding OpLine

		"OpLine %fname 1000 1\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"OpNoLine\n" // Contents after the previous OpLine

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"OpNoLine\n" // Multiple OpNoLine
		"OpNoLine\n"
		"OpNoLine\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpNoLine appearing at different places", spec));

	return group.release();
}

// Compare instruction for the contraction compute case.
// Returns true if the output is what is expected from the test case.
bool compareNoContractCase(const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	// Only size is needed because we are not comparing the exact values.
	size_t byteSize = expectedOutputs[0].getByteSize();

	const float*	outputAsFloat	= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for(size_t i = 0; i < byteSize / sizeof(float); ++i) {
		if (outputAsFloat[i] != 0.f &&
			outputAsFloat[i] != -ldexp(1, -24)) {
			return false;
		}
	}

	return true;
}

tcu::TestCaseGroup* createNoContractionGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "nocontraction", "Test the NoContraction decoration"));
	vector<CaseParameter>			cases;
	const int						numElements		= 100;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"${DECORATION}\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%c_f_m1     = OpConstant %f32 -1.\n"

		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1     = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1     = OpLoad %f32 %inloc1\n"
		"%inloc2     = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2     = OpLoad %f32 %inloc2\n"
		"%mul        = OpFMul %f32 %inval1 %inval2\n"
		"%add        = OpFAdd %f32 %mul %c_f_m1\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"              OpStore %outloc %add\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n");

	cases.push_back(CaseParameter("multiplication",	"OpDecorate %mul NoContraction"));
	cases.push_back(CaseParameter("addition",		"OpDecorate %add NoContraction"));
	cases.push_back(CaseParameter("both",			"OpDecorate %mul NoContraction\nOpDecorate %add NoContraction"));

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		inputFloats1[ndx]	= 1.f + std::ldexp(1.f, -23); // 1 + 2^-23.
		inputFloats2[ndx]	= 1.f - std::ldexp(1.f, -23); // 1 - 2^-23.
		// Result for (1 + 2^-23) * (1 - 2^-23) - 1. With NoContraction, the multiplication will be
		// conducted separately and the result is rounded to 1, or 0x1.fffffcp-1
		// So the final result will be 0.f or 0x1p-24.
		// If the operation is combined into a precise fused multiply-add, then the result would be
		// 2^-46 (0xa8800000).
		outputFloats[ndx]	= 0.f;
	}

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["DECORATION"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		// Check against the two possible answers based on rounding mode.
		spec.verifyIO = &compareNoContractCase;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}
	return group.release();
}

bool compareFRem(const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	vector<deUint8>	expectedBytes;
	expectedOutputs[0].getBytes(expectedBytes);

	const float*	expectedOutputAsFloat	= reinterpret_cast<const float*>(&expectedBytes.front());
	const float*	outputAsFloat			= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for (size_t idx = 0; idx < expectedBytes.size() / sizeof(float); ++idx)
	{
		const float f0 = expectedOutputAsFloat[idx];
		const float f1 = outputAsFloat[idx];
		// \todo relative error needs to be fairly high because FRem may be implemented as
		// (roughly) frac(a/b)*b, so LSB errors can be magnified. But this should be fine for now.
		if (deFloatAbs((f1 - f0) / f0) > 0.02)
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpFRemGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opfrem", "Test the OpFRem instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats2[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// Guard against divisors near zero.
		if (std::fabs(inputFloats2[ndx]) < 1e-3)
			inputFloats2[ndx] = 8.f;

		// The return value of std::fmod() has the same sign as its first operand, which is how OpFRem spec'd.
		outputFloats[ndx] = std::fmod(inputFloats1[ndx], inputFloats2[ndx]);
	}

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%rem       = OpFRem %f32 %inval1 %inval2\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %rem\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyIO = &compareFRem;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "", spec));

	return group.release();
}

bool compareNMin (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	const BufferSp&			expectedOutput			(expectedOutputs[0].getBuffer());
	std::vector<deUint8>	data;
	expectedOutput->getBytes(data);

	const float* const		expectedOutputAsFloat	= reinterpret_cast<const float*>(&data.front());
	const float* const		outputAsFloat			= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for (size_t idx = 0; idx < expectedOutput->getByteSize() / sizeof(float); ++idx)
	{
		const float f0 = expectedOutputAsFloat[idx];
		const float f1 = outputAsFloat[idx];

		// For NMin, we accept NaN as output if both inputs were NaN.
		// Otherwise the NaN is the wrong choise, as on architectures that
		// do not handle NaN, those are huge values.
		if (!(tcu::Float32(f1).isNaN() && tcu::Float32(f0).isNaN()) && deFloatAbs(f1 - f0) > 0.00001f)
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpNMinGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnmin", "Test the OpNMin instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats2[0], numElements);

	// Make the first case a full-NAN case.
	inputFloats1[0] = TCU_NAN;
	inputFloats2[0] = TCU_NAN;

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// By default, pick the smallest
		outputFloats[ndx] = std::min(inputFloats1[ndx], inputFloats2[ndx]);

		// Make half of the cases NaN cases
		if ((ndx & 1) == 0)
		{
			// Alternate between the NaN operand
			if ((ndx & 2) == 0)
			{
				outputFloats[ndx] = inputFloats2[ndx];
				inputFloats1[ndx] = TCU_NAN;
			}
			else
			{
				outputFloats[ndx] = inputFloats1[ndx];
				inputFloats2[ndx] = TCU_NAN;
			}
		}
	}

	spec.assembly =
		"OpCapability Shader\n"
		"%std450	= OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%rem       = OpExtInst %f32 %std450 NMin %inval1 %inval2\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %rem\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyIO = &compareNMin;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "", spec));

	return group.release();
}

bool compareNMax (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	const BufferSp&			expectedOutput			= expectedOutputs[0].getBuffer();
	std::vector<deUint8>	data;
	expectedOutput->getBytes(data);

	const float* const		expectedOutputAsFloat	= reinterpret_cast<const float*>(&data.front());
	const float* const		outputAsFloat			= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for (size_t idx = 0; idx < expectedOutput->getByteSize() / sizeof(float); ++idx)
	{
		const float f0 = expectedOutputAsFloat[idx];
		const float f1 = outputAsFloat[idx];

		// For NMax, NaN is considered acceptable result, since in
		// architectures that do not handle NaNs, those are huge values.
		if (!tcu::Float32(f1).isNaN() && deFloatAbs(f1 - f0) > 0.00001f)
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpNMaxGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "opnmax", "Test the OpNMax instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats2[0], numElements);

	// Make the first case a full-NAN case.
	inputFloats1[0] = TCU_NAN;
	inputFloats2[0] = TCU_NAN;

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// By default, pick the biggest
		outputFloats[ndx] = std::max(inputFloats1[ndx], inputFloats2[ndx]);

		// Make half of the cases NaN cases
		if ((ndx & 1) == 0)
		{
			// Alternate between the NaN operand
			if ((ndx & 2) == 0)
			{
				outputFloats[ndx] = inputFloats2[ndx];
				inputFloats1[ndx] = TCU_NAN;
			}
			else
			{
				outputFloats[ndx] = inputFloats1[ndx];
				inputFloats2[ndx] = TCU_NAN;
			}
		}
	}

	spec.assembly =
		"OpCapability Shader\n"
		"%std450	= OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 2\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%rem       = OpExtInst %f32 %std450 NMax %inval1 %inval2\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %rem\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyIO = &compareNMax;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "", spec));

	return group.release();
}

bool compareNClamp (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	const BufferSp&			expectedOutput			= expectedOutputs[0].getBuffer();
	std::vector<deUint8>	data;
	expectedOutput->getBytes(data);

	const float* const		expectedOutputAsFloat	= reinterpret_cast<const float*>(&data.front());
	const float* const		outputAsFloat			= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for (size_t idx = 0; idx < expectedOutput->getByteSize() / sizeof(float) / 2; ++idx)
	{
		const float e0 = expectedOutputAsFloat[idx * 2];
		const float e1 = expectedOutputAsFloat[idx * 2 + 1];
		const float res = outputAsFloat[idx];

		// For NClamp, we have two possible outcomes based on
		// whether NaNs are handled or not.
		// If either min or max value is NaN, the result is undefined,
		// so this test doesn't stress those. If the clamped value is
		// NaN, and NaNs are handled, the result is min; if NaNs are not
		// handled, they are big values that result in max.
		// If all three parameters are NaN, the result should be NaN.
		if (!((tcu::Float32(e0).isNaN() && tcu::Float32(res).isNaN()) ||
			 (deFloatAbs(e0 - res) < 0.00001f) ||
			 (deFloatAbs(e1 - res) < 0.00001f)))
			return false;
	}

	return true;
}

tcu::TestCaseGroup* createOpNClampGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opnclamp", "Test the OpNClamp instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					inputFloats3	(numElements, 0);
	vector<float>					outputFloats	(numElements * 2, 0);

	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats2[0], numElements);
	fillRandomScalars(rnd, -10000.f, 10000.f, &inputFloats3[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// Results are only defined if max value is bigger than min value.
		if (inputFloats2[ndx] > inputFloats3[ndx])
		{
			float t = inputFloats2[ndx];
			inputFloats2[ndx] = inputFloats3[ndx];
			inputFloats3[ndx] = t;
		}

		// By default, do the clamp, setting both possible answers
		float defaultRes = std::min(std::max(inputFloats1[ndx], inputFloats2[ndx]), inputFloats3[ndx]);

		float maxResA = std::max(inputFloats1[ndx], inputFloats2[ndx]);
		float maxResB = maxResA;

		// Alternate between the NaN cases
		if (ndx & 1)
		{
			inputFloats1[ndx] = TCU_NAN;
			// If NaN is handled, the result should be same as the clamp minimum.
			// If NaN is not handled, the result should clamp to the clamp maximum.
			maxResA = inputFloats2[ndx];
			maxResB = inputFloats3[ndx];
		}
		else
		{
			// Not a NaN case - only one legal result.
			maxResA = defaultRes;
			maxResB = defaultRes;
		}

		outputFloats[ndx * 2] = maxResA;
		outputFloats[ndx * 2 + 1] = maxResB;
	}

	// Make the first case a full-NAN case.
	inputFloats1[0] = TCU_NAN;
	inputFloats2[0] = TCU_NAN;
	inputFloats3[0] = TCU_NAN;
	outputFloats[0] = TCU_NAN;
	outputFloats[1] = TCU_NAN;

	spec.assembly =
		"OpCapability Shader\n"
		"%std450	= OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata1 DescriptorSet 0\n"
		"OpDecorate %indata1 Binding 0\n"
		"OpDecorate %indata2 DescriptorSet 0\n"
		"OpDecorate %indata2 Binding 1\n"
		"OpDecorate %indata3 DescriptorSet 0\n"
		"OpDecorate %indata3 Binding 2\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 3\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%buf        = OpTypeStruct %f32arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata1    = OpVariable %bufptr Uniform\n"
		"%indata2    = OpVariable %bufptr Uniform\n"
		"%indata3    = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc1    = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inval1    = OpLoad %f32 %inloc1\n"
		"%inloc2    = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inval2    = OpLoad %f32 %inloc2\n"
		"%inloc3    = OpAccessChain %f32ptr %indata3 %zero %x\n"
		"%inval3    = OpLoad %f32 %inloc3\n"
		"%rem       = OpExtInst %f32 %std450 NClamp %inval1 %inval2 %inval3\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %rem\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats3)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.verifyIO = &compareNClamp;

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "", spec));

	return group.release();
}

tcu::TestCaseGroup* createOpSRemComputeGroup (tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsrem", "Test the OpSRem instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessage;		// customized status message
		qpTestResult	failResult;			// override status on failure
		int				op1Min, op1Max;		// operand ranges
		int				op2Min, op2Max;
	} cases[] =
	{
		{ "positive",	"Output doesn't match with expected",				QP_TEST_RESULT_FAIL,	0,		65536,	0,		100 },
		{ "all",		"Inconsistent results, but within specification",	negFailResult,			-65536,	65536,	-100,	100 },	// see below
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params		= cases[caseNdx];
		ComputeShaderSpec	spec;
		vector<deInt32>		inputInts1	(numElements, 0);
		vector<deInt32>		inputInts2	(numElements, 0);
		vector<deInt32>		outputInts	(numElements, 0);

		fillRandomScalars(rnd, params.op1Min, params.op1Max, &inputInts1[0], numElements);
		fillRandomScalars(rnd, params.op2Min, params.op2Max, &inputInts2[0], numElements, filterNotZero);

		for (int ndx = 0; ndx < numElements; ++ndx)
		{
			// The return value of std::fmod() has the same sign as its first operand, which is how OpFRem spec'd.
			outputInts[ndx] = inputInts1[ndx] % inputInts2[ndx];
		}

		spec.assembly =
			string(getComputeAsmShaderPreamble()) +

			"OpName %main           \"main\"\n"
			"OpName %id             \"gl_GlobalInvocationID\"\n"

			"OpDecorate %id BuiltIn GlobalInvocationId\n"

			"OpDecorate %buf BufferBlock\n"
			"OpDecorate %indata1 DescriptorSet 0\n"
			"OpDecorate %indata1 Binding 0\n"
			"OpDecorate %indata2 DescriptorSet 0\n"
			"OpDecorate %indata2 Binding 1\n"
			"OpDecorate %outdata DescriptorSet 0\n"
			"OpDecorate %outdata Binding 2\n"
			"OpDecorate %i32arr ArrayStride 4\n"
			"OpMemberDecorate %buf 0 Offset 0\n"

			+ string(getComputeAsmCommonTypes()) +

			"%buf        = OpTypeStruct %i32arr\n"
			"%bufptr     = OpTypePointer Uniform %buf\n"
			"%indata1    = OpVariable %bufptr Uniform\n"
			"%indata2    = OpVariable %bufptr Uniform\n"
			"%outdata    = OpVariable %bufptr Uniform\n"

			"%id        = OpVariable %uvec3ptr Input\n"
			"%zero      = OpConstant %i32 0\n"

			"%main      = OpFunction %void None %voidf\n"
			"%label     = OpLabel\n"
			"%idval     = OpLoad %uvec3 %id\n"
			"%x         = OpCompositeExtract %u32 %idval 0\n"
			"%inloc1    = OpAccessChain %i32ptr %indata1 %zero %x\n"
			"%inval1    = OpLoad %i32 %inloc1\n"
			"%inloc2    = OpAccessChain %i32ptr %indata2 %zero %x\n"
			"%inval2    = OpLoad %i32 %inloc2\n"
			"%rem       = OpSRem %i32 %inval1 %inval2\n"
			"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
			"             OpStore %outloc %rem\n"
			"             OpReturn\n"
			"             OpFunctionEnd\n";

		spec.inputs.push_back	(BufferSp(new Int32Buffer(inputInts1)));
		spec.inputs.push_back	(BufferSp(new Int32Buffer(inputInts2)));
		spec.outputs.push_back	(BufferSp(new Int32Buffer(outputInts)));
		spec.numWorkGroups		= IVec3(numElements, 1, 1);
		spec.failResult			= params.failResult;
		spec.failMessage		= params.failMessage;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, params.name, "", spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpSRemComputeGroup64 (tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsrem64", "Test the 64-bit OpSRem instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessage;		// customized status message
		qpTestResult	failResult;			// override status on failure
		bool			positive;
	} cases[] =
	{
		{ "positive",	"Output doesn't match with expected",				QP_TEST_RESULT_FAIL,	true },
		{ "all",		"Inconsistent results, but within specification",	negFailResult,			false },	// see below
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params		= cases[caseNdx];
		ComputeShaderSpec	spec;
		vector<deInt64>		inputInts1	(numElements, 0);
		vector<deInt64>		inputInts2	(numElements, 0);
		vector<deInt64>		outputInts	(numElements, 0);

		if (params.positive)
		{
			fillRandomInt64sLogDistributed(rnd, inputInts1, numElements, filterNonNegative);
			fillRandomInt64sLogDistributed(rnd, inputInts2, numElements, filterPositive);
		}
		else
		{
			fillRandomInt64sLogDistributed(rnd, inputInts1, numElements);
			fillRandomInt64sLogDistributed(rnd, inputInts2, numElements, filterNotZero);
		}

		for (int ndx = 0; ndx < numElements; ++ndx)
		{
			// The return value of std::fmod() has the same sign as its first operand, which is how OpFRem spec'd.
			outputInts[ndx] = inputInts1[ndx] % inputInts2[ndx];
		}

		spec.assembly =
			"OpCapability Int64\n"

			+ string(getComputeAsmShaderPreamble()) +

			"OpName %main           \"main\"\n"
			"OpName %id             \"gl_GlobalInvocationID\"\n"

			"OpDecorate %id BuiltIn GlobalInvocationId\n"

			"OpDecorate %buf BufferBlock\n"
			"OpDecorate %indata1 DescriptorSet 0\n"
			"OpDecorate %indata1 Binding 0\n"
			"OpDecorate %indata2 DescriptorSet 0\n"
			"OpDecorate %indata2 Binding 1\n"
			"OpDecorate %outdata DescriptorSet 0\n"
			"OpDecorate %outdata Binding 2\n"
			"OpDecorate %i64arr ArrayStride 8\n"
			"OpMemberDecorate %buf 0 Offset 0\n"

			+ string(getComputeAsmCommonTypes())
			+ string(getComputeAsmCommonInt64Types()) +

			"%buf        = OpTypeStruct %i64arr\n"
			"%bufptr     = OpTypePointer Uniform %buf\n"
			"%indata1    = OpVariable %bufptr Uniform\n"
			"%indata2    = OpVariable %bufptr Uniform\n"
			"%outdata    = OpVariable %bufptr Uniform\n"

			"%id        = OpVariable %uvec3ptr Input\n"
			"%zero      = OpConstant %i64 0\n"

			"%main      = OpFunction %void None %voidf\n"
			"%label     = OpLabel\n"
			"%idval     = OpLoad %uvec3 %id\n"
			"%x         = OpCompositeExtract %u32 %idval 0\n"
			"%inloc1    = OpAccessChain %i64ptr %indata1 %zero %x\n"
			"%inval1    = OpLoad %i64 %inloc1\n"
			"%inloc2    = OpAccessChain %i64ptr %indata2 %zero %x\n"
			"%inval2    = OpLoad %i64 %inloc2\n"
			"%rem       = OpSRem %i64 %inval1 %inval2\n"
			"%outloc    = OpAccessChain %i64ptr %outdata %zero %x\n"
			"             OpStore %outloc %rem\n"
			"             OpReturn\n"
			"             OpFunctionEnd\n";

		spec.inputs.push_back	(BufferSp(new Int64Buffer(inputInts1)));
		spec.inputs.push_back	(BufferSp(new Int64Buffer(inputInts2)));
		spec.outputs.push_back	(BufferSp(new Int64Buffer(outputInts)));
		spec.numWorkGroups		= IVec3(numElements, 1, 1);
		spec.failResult			= params.failResult;
		spec.failMessage		= params.failMessage;

		spec.requestedVulkanFeatures.coreFeatures.shaderInt64 = VK_TRUE;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, params.name, "", spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpSModComputeGroup (tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsmod", "Test the OpSMod instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessage;		// customized status message
		qpTestResult	failResult;			// override status on failure
		int				op1Min, op1Max;		// operand ranges
		int				op2Min, op2Max;
	} cases[] =
	{
		{ "positive",	"Output doesn't match with expected",				QP_TEST_RESULT_FAIL,	0,		65536,	0,		100 },
		{ "all",		"Inconsistent results, but within specification",	negFailResult,			-65536,	65536,	-100,	100 },	// see below
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params		= cases[caseNdx];

		ComputeShaderSpec	spec;
		vector<deInt32>		inputInts1	(numElements, 0);
		vector<deInt32>		inputInts2	(numElements, 0);
		vector<deInt32>		outputInts	(numElements, 0);

		fillRandomScalars(rnd, params.op1Min, params.op1Max, &inputInts1[0], numElements);
		fillRandomScalars(rnd, params.op2Min, params.op2Max, &inputInts2[0], numElements, filterNotZero);

		for (int ndx = 0; ndx < numElements; ++ndx)
		{
			deInt32 rem = inputInts1[ndx] % inputInts2[ndx];
			if (rem == 0)
			{
				outputInts[ndx] = 0;
			}
			else if ((inputInts1[ndx] >= 0) == (inputInts2[ndx] >= 0))
			{
				// They have the same sign
				outputInts[ndx] = rem;
			}
			else
			{
				// They have opposite sign.  The remainder operation takes the
				// sign inputInts1[ndx] but OpSMod is supposed to take ths sign
				// of inputInts2[ndx].  Adding inputInts2[ndx] will ensure that
				// the result has the correct sign and that it is still
				// congruent to inputInts1[ndx] modulo inputInts2[ndx]
				//
				// See also http://mathforum.org/library/drmath/view/52343.html
				outputInts[ndx] = rem + inputInts2[ndx];
			}
		}

		spec.assembly =
			string(getComputeAsmShaderPreamble()) +

			"OpName %main           \"main\"\n"
			"OpName %id             \"gl_GlobalInvocationID\"\n"

			"OpDecorate %id BuiltIn GlobalInvocationId\n"

			"OpDecorate %buf BufferBlock\n"
			"OpDecorate %indata1 DescriptorSet 0\n"
			"OpDecorate %indata1 Binding 0\n"
			"OpDecorate %indata2 DescriptorSet 0\n"
			"OpDecorate %indata2 Binding 1\n"
			"OpDecorate %outdata DescriptorSet 0\n"
			"OpDecorate %outdata Binding 2\n"
			"OpDecorate %i32arr ArrayStride 4\n"
			"OpMemberDecorate %buf 0 Offset 0\n"

			+ string(getComputeAsmCommonTypes()) +

			"%buf        = OpTypeStruct %i32arr\n"
			"%bufptr     = OpTypePointer Uniform %buf\n"
			"%indata1    = OpVariable %bufptr Uniform\n"
			"%indata2    = OpVariable %bufptr Uniform\n"
			"%outdata    = OpVariable %bufptr Uniform\n"

			"%id        = OpVariable %uvec3ptr Input\n"
			"%zero      = OpConstant %i32 0\n"

			"%main      = OpFunction %void None %voidf\n"
			"%label     = OpLabel\n"
			"%idval     = OpLoad %uvec3 %id\n"
			"%x         = OpCompositeExtract %u32 %idval 0\n"
			"%inloc1    = OpAccessChain %i32ptr %indata1 %zero %x\n"
			"%inval1    = OpLoad %i32 %inloc1\n"
			"%inloc2    = OpAccessChain %i32ptr %indata2 %zero %x\n"
			"%inval2    = OpLoad %i32 %inloc2\n"
			"%rem       = OpSMod %i32 %inval1 %inval2\n"
			"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
			"             OpStore %outloc %rem\n"
			"             OpReturn\n"
			"             OpFunctionEnd\n";

		spec.inputs.push_back	(BufferSp(new Int32Buffer(inputInts1)));
		spec.inputs.push_back	(BufferSp(new Int32Buffer(inputInts2)));
		spec.outputs.push_back	(BufferSp(new Int32Buffer(outputInts)));
		spec.numWorkGroups		= IVec3(numElements, 1, 1);
		spec.failResult			= params.failResult;
		spec.failMessage		= params.failMessage;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, params.name, "", spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpSModComputeGroup64 (tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsmod64", "Test the OpSMod instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 200;

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessage;		// customized status message
		qpTestResult	failResult;			// override status on failure
		bool			positive;
	} cases[] =
	{
		{ "positive",	"Output doesn't match with expected",				QP_TEST_RESULT_FAIL,	true },
		{ "all",		"Inconsistent results, but within specification",	negFailResult,			false },	// see below
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params		= cases[caseNdx];

		ComputeShaderSpec	spec;
		vector<deInt64>		inputInts1	(numElements, 0);
		vector<deInt64>		inputInts2	(numElements, 0);
		vector<deInt64>		outputInts	(numElements, 0);


		if (params.positive)
		{
			fillRandomInt64sLogDistributed(rnd, inputInts1, numElements, filterNonNegative);
			fillRandomInt64sLogDistributed(rnd, inputInts2, numElements, filterPositive);
		}
		else
		{
			fillRandomInt64sLogDistributed(rnd, inputInts1, numElements);
			fillRandomInt64sLogDistributed(rnd, inputInts2, numElements, filterNotZero);
		}

		for (int ndx = 0; ndx < numElements; ++ndx)
		{
			deInt64 rem = inputInts1[ndx] % inputInts2[ndx];
			if (rem == 0)
			{
				outputInts[ndx] = 0;
			}
			else if ((inputInts1[ndx] >= 0) == (inputInts2[ndx] >= 0))
			{
				// They have the same sign
				outputInts[ndx] = rem;
			}
			else
			{
				// They have opposite sign.  The remainder operation takes the
				// sign inputInts1[ndx] but OpSMod is supposed to take ths sign
				// of inputInts2[ndx].  Adding inputInts2[ndx] will ensure that
				// the result has the correct sign and that it is still
				// congruent to inputInts1[ndx] modulo inputInts2[ndx]
				//
				// See also http://mathforum.org/library/drmath/view/52343.html
				outputInts[ndx] = rem + inputInts2[ndx];
			}
		}

		spec.assembly =
			"OpCapability Int64\n"

			+ string(getComputeAsmShaderPreamble()) +

			"OpName %main           \"main\"\n"
			"OpName %id             \"gl_GlobalInvocationID\"\n"

			"OpDecorate %id BuiltIn GlobalInvocationId\n"

			"OpDecorate %buf BufferBlock\n"
			"OpDecorate %indata1 DescriptorSet 0\n"
			"OpDecorate %indata1 Binding 0\n"
			"OpDecorate %indata2 DescriptorSet 0\n"
			"OpDecorate %indata2 Binding 1\n"
			"OpDecorate %outdata DescriptorSet 0\n"
			"OpDecorate %outdata Binding 2\n"
			"OpDecorate %i64arr ArrayStride 8\n"
			"OpMemberDecorate %buf 0 Offset 0\n"

			+ string(getComputeAsmCommonTypes())
			+ string(getComputeAsmCommonInt64Types()) +

			"%buf        = OpTypeStruct %i64arr\n"
			"%bufptr     = OpTypePointer Uniform %buf\n"
			"%indata1    = OpVariable %bufptr Uniform\n"
			"%indata2    = OpVariable %bufptr Uniform\n"
			"%outdata    = OpVariable %bufptr Uniform\n"

			"%id        = OpVariable %uvec3ptr Input\n"
			"%zero      = OpConstant %i64 0\n"

			"%main      = OpFunction %void None %voidf\n"
			"%label     = OpLabel\n"
			"%idval     = OpLoad %uvec3 %id\n"
			"%x         = OpCompositeExtract %u32 %idval 0\n"
			"%inloc1    = OpAccessChain %i64ptr %indata1 %zero %x\n"
			"%inval1    = OpLoad %i64 %inloc1\n"
			"%inloc2    = OpAccessChain %i64ptr %indata2 %zero %x\n"
			"%inval2    = OpLoad %i64 %inloc2\n"
			"%rem       = OpSMod %i64 %inval1 %inval2\n"
			"%outloc    = OpAccessChain %i64ptr %outdata %zero %x\n"
			"             OpStore %outloc %rem\n"
			"             OpReturn\n"
			"             OpFunctionEnd\n";

		spec.inputs.push_back	(BufferSp(new Int64Buffer(inputInts1)));
		spec.inputs.push_back	(BufferSp(new Int64Buffer(inputInts2)));
		spec.outputs.push_back	(BufferSp(new Int64Buffer(outputInts)));
		spec.numWorkGroups		= IVec3(numElements, 1, 1);
		spec.failResult			= params.failResult;
		spec.failMessage		= params.failMessage;

		spec.requestedVulkanFeatures.coreFeatures.shaderInt64 = VK_TRUE;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, params.name, "", spec));
	}

	return group.release();
}

// Copy contents in the input buffer to the output buffer.
tcu::TestCaseGroup* createOpCopyMemoryGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opcopymemory", "Test the OpCopyMemory instruction"));
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;

	// The following case adds vec4(0., 0.5, 1.5, 2.5) to each of the elements in the input buffer and writes output to the output buffer.
	ComputeShaderSpec				spec1;
	vector<Vec4>					inputFloats1	(numElements);
	vector<Vec4>					outputFloats1	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats1[0], numElements * 4);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats1);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats1[ndx] = inputFloats1[ndx] + Vec4(0.f, 0.5f, 1.5f, 2.5f);

	spec1.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %vec4arr ArrayStride 16\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%vec4       = OpTypeVector %f32 4\n"
		"%vec4ptr_u  = OpTypePointer Uniform %vec4\n"
		"%vec4ptr_f  = OpTypePointer Function %vec4\n"
		"%vec4arr    = OpTypeRuntimeArray %vec4\n"
		"%buf        = OpTypeStruct %vec4arr\n"
		"%bufptr     = OpTypePointer Uniform %buf\n"
		"%indata     = OpVariable %bufptr Uniform\n"
		"%outdata    = OpVariable %bufptr Uniform\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%c_f_0      = OpConstant %f32 0.\n"
		"%c_f_0_5    = OpConstant %f32 0.5\n"
		"%c_f_1_5    = OpConstant %f32 1.5\n"
		"%c_f_2_5    = OpConstant %f32 2.5\n"
		"%c_vec4     = OpConstantComposite %vec4 %c_f_0 %c_f_0_5 %c_f_1_5 %c_f_2_5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%v_vec4     = OpVariable %vec4ptr_f Function\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %vec4ptr_u %indata %zero %x\n"
		"%outloc     = OpAccessChain %vec4ptr_u %outdata %zero %x\n"
		"              OpCopyMemory %v_vec4 %inloc\n"
		"%v_vec4_val = OpLoad %vec4 %v_vec4\n"
		"%add        = OpFAdd %vec4 %v_vec4_val %c_vec4\n"
		"              OpStore %outloc %add\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";

	spec1.inputs.push_back(BufferSp(new Vec4Buffer(inputFloats1)));
	spec1.outputs.push_back(BufferSp(new Vec4Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vector", "OpCopyMemory elements of vector type", spec1));

	// The following case copies a float[100] variable from the input buffer to the output buffer.
	ComputeShaderSpec				spec2;
	vector<float>					inputFloats2	(numElements);
	vector<float>					outputFloats2	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats2[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats2[ndx] = inputFloats2[ndx];

	spec2.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %f32arr100 ArrayStride 4\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%hundred        = OpConstant %u32 100\n"
		"%f32arr100      = OpTypeArray %f32 %hundred\n"
		"%f32arr100ptr_f = OpTypePointer Function %f32arr100\n"
		"%f32arr100ptr_u = OpTypePointer Uniform %f32arr100\n"
		"%buf            = OpTypeStruct %f32arr100\n"
		"%bufptr         = OpTypePointer Uniform %buf\n"
		"%indata         = OpVariable %bufptr Uniform\n"
		"%outdata        = OpVariable %bufptr Uniform\n"

		"%id             = OpVariable %uvec3ptr Input\n"
		"%zero           = OpConstant %i32 0\n"

		"%main           = OpFunction %void None %voidf\n"
		"%label          = OpLabel\n"
		"%var            = OpVariable %f32arr100ptr_f Function\n"
		"%inarr          = OpAccessChain %f32arr100ptr_u %indata %zero\n"
		"%outarr         = OpAccessChain %f32arr100ptr_u %outdata %zero\n"
		"                  OpCopyMemory %var %inarr\n"
		"                  OpCopyMemory %outarr %var\n"
		"                  OpReturn\n"
		"                  OpFunctionEnd\n";

	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(1, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "array", "OpCopyMemory elements of array type", spec2));

	// The following case copies a struct{vec4, vec4, vec4, vec4} variable from the input buffer to the output buffer.
	ComputeShaderSpec				spec3;
	vector<float>					inputFloats3	(16);
	vector<float>					outputFloats3	(16);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats3[0], 16);

	for (size_t ndx = 0; ndx < 16; ++ndx)
		outputFloats3[ndx] = inputFloats3[ndx];

	spec3.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		//"OpMemberDecorate %buf 0 Offset 0\n"  - exists in getComputeAsmInputOutputBufferTraits
		"OpMemberDecorate %buf 1 Offset 16\n"
		"OpMemberDecorate %buf 2 Offset 32\n"
		"OpMemberDecorate %buf 3 Offset 48\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%vec4      = OpTypeVector %f32 4\n"
		"%buf       = OpTypeStruct %vec4 %vec4 %vec4 %vec4\n"
		"%bufptr    = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"
		"%vec4stptr = OpTypePointer Function %buf\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%var       = OpVariable %vec4stptr Function\n"
		"             OpCopyMemory %var %indata\n"
		"             OpCopyMemory %outdata %var\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec3.inputs.push_back(BufferSp(new Float32Buffer(inputFloats3)));
	spec3.outputs.push_back(BufferSp(new Float32Buffer(outputFloats3)));
	spec3.numWorkGroups = IVec3(1, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "struct", "OpCopyMemory elements of struct type", spec3));

	// The following case negates multiple float variables from the input buffer and stores the results to the output buffer.
	ComputeShaderSpec				spec4;
	vector<float>					inputFloats4	(numElements);
	vector<float>					outputFloats4	(numElements);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats4[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats4[ndx] = -inputFloats4[ndx];

	spec4.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%f32ptr_f  = OpTypePointer Function %f32\n"
		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%var       = OpVariable %f32ptr_f Function\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpCopyMemory %var %inloc\n"
		"%val       = OpLoad %f32 %var\n"
		"%neg       = OpFNegate %f32 %val\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";

	spec4.inputs.push_back(BufferSp(new Float32Buffer(inputFloats4)));
	spec4.outputs.push_back(BufferSp(new Float32Buffer(outputFloats4)));
	spec4.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "float", "OpCopyMemory elements of float type", spec4));

	return group.release();
}

tcu::TestCaseGroup* createOpCopyObjectGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opcopyobject", "Test the OpCopyObject instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 7.5f;

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%fmat     = OpTypeMatrix %fvec3 3\n"
		"%three    = OpConstant %u32 3\n"
		"%farr     = OpTypeArray %f32 %three\n"
		"%fst      = OpTypeStruct %f32 %f32\n"

		+ string(getComputeAsmInputOutputBuffer()) +

		"%id            = OpVariable %uvec3ptr Input\n"
		"%zero          = OpConstant %i32 0\n"
		"%c_f           = OpConstant %f32 1.5\n"
		"%c_fvec3       = OpConstantComposite %fvec3 %c_f %c_f %c_f\n"
		"%c_fmat        = OpConstantComposite %fmat %c_fvec3 %c_fvec3 %c_fvec3\n"
		"%c_farr        = OpConstantComposite %farr %c_f %c_f %c_f\n"
		"%c_fst         = OpConstantComposite %fst %c_f %c_f\n"

		"%main          = OpFunction %void None %voidf\n"
		"%label         = OpLabel\n"
		"%c_f_copy      = OpCopyObject %f32   %c_f\n"
		"%c_fvec3_copy  = OpCopyObject %fvec3 %c_fvec3\n"
		"%c_fmat_copy   = OpCopyObject %fmat  %c_fmat\n"
		"%c_farr_copy   = OpCopyObject %farr  %c_farr\n"
		"%c_fst_copy    = OpCopyObject %fst   %c_fst\n"
		"%fvec3_elem    = OpCompositeExtract %f32 %c_fvec3_copy 0\n"
		"%fmat_elem     = OpCompositeExtract %f32 %c_fmat_copy 1 2\n"
		"%farr_elem     = OpCompositeExtract %f32 %c_farr_copy 2\n"
		"%fst_elem      = OpCompositeExtract %f32 %c_fst_copy 1\n"
		// Add up. 1.5 * 5 = 7.5.
		"%add1          = OpFAdd %f32 %c_f_copy %fvec3_elem\n"
		"%add2          = OpFAdd %f32 %add1     %fmat_elem\n"
		"%add3          = OpFAdd %f32 %add2     %farr_elem\n"
		"%add4          = OpFAdd %f32 %add3     %fst_elem\n"

		"%idval         = OpLoad %uvec3 %id\n"
		"%x             = OpCompositeExtract %u32 %idval 0\n"
		"%inloc         = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc        = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval         = OpLoad %f32 %inloc\n"
		"%add           = OpFAdd %f32 %add4 %inval\n"
		"                 OpStore %outloc %add\n"
		"                 OpReturn\n"
		"                 OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "spotcheck", "OpCopyObject on different types", spec));

	return group.release();
}
// Assembly code used for testing OpUnreachable is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void not_called_func() {
//   // place OpUnreachable here
// }
//
// uint modulo4(uint val) {
//   switch (val % uint(4)) {
//     case 0:  return 3;
//     case 1:  return 2;
//     case 2:  return 1;
//     case 3:  return 0;
//     default: return 100; // place OpUnreachable here
//   }
// }
//
// uint const5() {
//   return 5;
//   // place OpUnreachable here
// }
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   if (const5() > modulo4(1000)) {
//     output_data.elements[x] = -input_data.elements[x];
//   } else {
//     // place OpUnreachable here
//     output_data.elements[x] = input_data.elements[x];
//   }
// }

void addOpUnreachableAmberTests(tcu::TestCaseGroup& group, tcu::TestContext& testCtx)
{
	static const char dataDir[] = "spirv_assembly/instruction/compute/unreachable";

	struct Case
	{
		string	name;
		string	desc;
	};

	static const Case cases[] =
	{
		{ "unreachable-switch-merge-in-loop",	"Test containing an unreachable switch merge block inside an infinite loop"	},
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		const string fileName = cases[i].name + ".amber";
		group.addChild(cts_amber::createAmberTestCase(testCtx, cases[i].name.c_str(), cases[i].desc.c_str(), dataDir, fileName));
	}
}

void addOpSwitchAmberTests(tcu::TestCaseGroup& group, tcu::TestContext& testCtx)
{
	static const char dataDir[] = "spirv_assembly/instruction/compute/switch";

	struct Case
	{
		string	name;
		string	desc;
	};

	static const Case cases[] =
	{
		{ "switch-case-to-merge-block",	"Test switch containing a case that jumps directly to the merge block"	},
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		const string fileName = cases[i].name + ".amber";
		group.addChild(cts_amber::createAmberTestCase(testCtx, cases[i].name.c_str(), cases[i].desc.c_str(), dataDir, fileName));
	}
}

tcu::TestCaseGroup* createOpArrayLengthComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group		(new tcu::TestCaseGroup(testCtx, "oparraylength", "Test the OpArrayLength instruction"));
	static const char				dataDir[]	= "spirv_assembly/instruction/compute/arraylength";

	struct Case
	{
		string	name;
		string	desc;
	};

	static const Case cases[] =
	{
		{ "array-stride-larger-than-element-size",	"Test using an unsized array with stride larger than the element size"	}
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		const string fileName = cases[i].name + ".amber";
		group->addChild(cts_amber::createAmberTestCase(testCtx, cases[i].name.c_str(), cases[i].desc.c_str(), dataDir, fileName));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpUnreachableGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opunreachable", "Test the OpUnreachable instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main            \"main\"\n"
		"OpName %func_not_called_func \"not_called_func(\"\n"
		"OpName %func_modulo4         \"modulo4(u1;\"\n"
		"OpName %func_const5          \"const5(\"\n"
		"OpName %id                   \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%u32ptr    = OpTypePointer Function %u32\n"
		"%uintfuint = OpTypeFunction %u32 %u32ptr\n"
		"%unitf     = OpTypeFunction %u32\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %u32 0\n"
		"%one       = OpConstant %u32 1\n"
		"%two       = OpConstant %u32 2\n"
		"%three     = OpConstant %u32 3\n"
		"%four      = OpConstant %u32 4\n"
		"%five      = OpConstant %u32 5\n"
		"%hundred   = OpConstant %u32 100\n"
		"%thousand  = OpConstant %u32 1000\n"

		+ string(getComputeAsmInputOutputBuffer()) +

		// Main()
		"%main   = OpFunction %void None %voidf\n"
		"%main_entry  = OpLabel\n"
		"%v_thousand  = OpVariable %u32ptr Function %thousand\n"
		"%idval       = OpLoad %uvec3 %id\n"
		"%x           = OpCompositeExtract %u32 %idval 0\n"
		"%inloc       = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval       = OpLoad %f32 %inloc\n"
		"%outloc      = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%ret_const5  = OpFunctionCall %u32 %func_const5\n"
		"%ret_modulo4 = OpFunctionCall %u32 %func_modulo4 %v_thousand\n"
		"%cmp_gt      = OpUGreaterThan %bool %ret_const5 %ret_modulo4\n"
		"               OpSelectionMerge %if_end None\n"
		"               OpBranchConditional %cmp_gt %if_true %if_false\n"
		"%if_true     = OpLabel\n"
		"%negate      = OpFNegate %f32 %inval\n"
		"               OpStore %outloc %negate\n"
		"               OpBranch %if_end\n"
		"%if_false    = OpLabel\n"
		"               OpUnreachable\n" // Unreachable else branch for if statement
		"%if_end      = OpLabel\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n"

		// not_called_function()
		"%func_not_called_func  = OpFunction %void None %voidf\n"
		"%not_called_func_entry = OpLabel\n"
		"                         OpUnreachable\n" // Unreachable entry block in not called static function
		"                         OpFunctionEnd\n"

		// modulo4()
		"%func_modulo4  = OpFunction %u32 None %uintfuint\n"
		"%valptr        = OpFunctionParameter %u32ptr\n"
		"%modulo4_entry = OpLabel\n"
		"%val           = OpLoad %u32 %valptr\n"
		"%modulo        = OpUMod %u32 %val %four\n"
		"                 OpSelectionMerge %switch_merge None\n"
		"                 OpSwitch %modulo %default 0 %case0 1 %case1 2 %case2 3 %case3\n"
		"%case0         = OpLabel\n"
		"                 OpReturnValue %three\n"
		"%case1         = OpLabel\n"
		"                 OpReturnValue %two\n"
		"%case2         = OpLabel\n"
		"                 OpReturnValue %one\n"
		"%case3         = OpLabel\n"
		"                 OpReturnValue %zero\n"
		"%default       = OpLabel\n"
		"                 OpUnreachable\n" // Unreachable default case for switch statement
		"%switch_merge  = OpLabel\n"
		"                 OpUnreachable\n" // Unreachable merge block for switch statement
		"                 OpFunctionEnd\n"

		// const5()
		"%func_const5  = OpFunction %u32 None %unitf\n"
		"%const5_entry = OpLabel\n"
		"                OpReturnValue %five\n"
		"%unreachable  = OpLabel\n"
		"                OpUnreachable\n" // Unreachable block in function
		"                OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "OpUnreachable appearing at different places", spec));

	addOpUnreachableAmberTests(*group, testCtx);

	return group.release();
}

// Assembly code used for testing decoration group is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input0 {
//   float elements[];
// } input_data0;
// layout(std140, set = 0, binding = 1) readonly buffer Input1 {
//   float elements[];
// } input_data1;
// layout(std140, set = 0, binding = 2) readonly buffer Input2 {
//   float elements[];
// } input_data2;
// layout(std140, set = 0, binding = 3) readonly buffer Input3 {
//   float elements[];
// } input_data3;
// layout(std140, set = 0, binding = 4) readonly buffer Input4 {
//   float elements[];
// } input_data4;
// layout(std140, set = 0, binding = 5) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data0.elements[x] + input_data1.elements[x] + input_data2.elements[x] + input_data3.elements[x] + input_data4.elements[x];
// }
tcu::TestCaseGroup* createDecorationGroupGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "decoration_group", "Test the OpDecorationGroup & OpGroupDecorate instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats0	(numElements, 0);
	vector<float>					inputFloats1	(numElements, 0);
	vector<float>					inputFloats2	(numElements, 0);
	vector<float>					inputFloats3	(numElements, 0);
	vector<float>					inputFloats4	(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats0[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats1[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats2[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats3[0], numElements);
	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats4[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats0);
	floorAll(inputFloats1);
	floorAll(inputFloats2);
	floorAll(inputFloats3);
	floorAll(inputFloats4);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats0[ndx] + inputFloats1[ndx] + inputFloats2[ndx] + inputFloats3[ndx] + inputFloats4[ndx];

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		// Not using group decoration on variable.
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		// Not using group decoration on type.
		"OpDecorate %f32arr ArrayStride 4\n"

		"OpDecorate %groups BufferBlock\n"
		"OpDecorate %groupm Offset 0\n"
		"%groups = OpDecorationGroup\n"
		"%groupm = OpDecorationGroup\n"

		// Group decoration on multiple structs.
		"OpGroupDecorate %groups %outbuf %inbuf0 %inbuf1 %inbuf2 %inbuf3 %inbuf4\n"
		// Group decoration on multiple struct members.
		"OpGroupMemberDecorate %groupm %outbuf 0 %inbuf0 0 %inbuf1 0 %inbuf2 0 %inbuf3 0 %inbuf4 0\n"

		"OpDecorate %group1 DescriptorSet 0\n"
		"OpDecorate %group3 DescriptorSet 0\n"
		"OpDecorate %group3 NonWritable\n"
		"OpDecorate %group3 Restrict\n"
		"%group0 = OpDecorationGroup\n"
		"%group1 = OpDecorationGroup\n"
		"%group3 = OpDecorationGroup\n"

		// Applying the same decoration group multiple times.
		"OpGroupDecorate %group1 %outdata\n"
		"OpGroupDecorate %group1 %outdata\n"
		"OpGroupDecorate %group1 %outdata\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 5\n"
		// Applying decoration group containing nothing.
		"OpGroupDecorate %group0 %indata0\n"
		"OpDecorate %indata0 DescriptorSet 0\n"
		"OpDecorate %indata0 Binding 0\n"
		// Applying decoration group containing one decoration.
		"OpGroupDecorate %group1 %indata1\n"
		"OpDecorate %indata1 Binding 1\n"
		// Applying decoration group containing multiple decorations.
		"OpGroupDecorate %group3 %indata2 %indata3\n"
		"OpDecorate %indata2 Binding 2\n"
		"OpDecorate %indata3 Binding 3\n"
		// Applying multiple decoration groups (with overlapping).
		"OpGroupDecorate %group0 %indata4\n"
		"OpGroupDecorate %group1 %indata4\n"
		"OpGroupDecorate %group3 %indata4\n"
		"OpDecorate %indata4 Binding 4\n"

		+ string(getComputeAsmCommonTypes()) +

		"%id   = OpVariable %uvec3ptr Input\n"
		"%zero = OpConstant %i32 0\n"

		"%outbuf    = OpTypeStruct %f32arr\n"
		"%outbufptr = OpTypePointer Uniform %outbuf\n"
		"%outdata   = OpVariable %outbufptr Uniform\n"
		"%inbuf0    = OpTypeStruct %f32arr\n"
		"%inbuf0ptr = OpTypePointer Uniform %inbuf0\n"
		"%indata0   = OpVariable %inbuf0ptr Uniform\n"
		"%inbuf1    = OpTypeStruct %f32arr\n"
		"%inbuf1ptr = OpTypePointer Uniform %inbuf1\n"
		"%indata1   = OpVariable %inbuf1ptr Uniform\n"
		"%inbuf2    = OpTypeStruct %f32arr\n"
		"%inbuf2ptr = OpTypePointer Uniform %inbuf2\n"
		"%indata2   = OpVariable %inbuf2ptr Uniform\n"
		"%inbuf3    = OpTypeStruct %f32arr\n"
		"%inbuf3ptr = OpTypePointer Uniform %inbuf3\n"
		"%indata3   = OpVariable %inbuf3ptr Uniform\n"
		"%inbuf4    = OpTypeStruct %f32arr\n"
		"%inbufptr  = OpTypePointer Uniform %inbuf4\n"
		"%indata4   = OpVariable %inbufptr Uniform\n"

		"%main   = OpFunction %void None %voidf\n"
		"%label  = OpLabel\n"
		"%idval  = OpLoad %uvec3 %id\n"
		"%x      = OpCompositeExtract %u32 %idval 0\n"
		"%inloc0 = OpAccessChain %f32ptr %indata0 %zero %x\n"
		"%inloc1 = OpAccessChain %f32ptr %indata1 %zero %x\n"
		"%inloc2 = OpAccessChain %f32ptr %indata2 %zero %x\n"
		"%inloc3 = OpAccessChain %f32ptr %indata3 %zero %x\n"
		"%inloc4 = OpAccessChain %f32ptr %indata4 %zero %x\n"
		"%outloc = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval0 = OpLoad %f32 %inloc0\n"
		"%inval1 = OpLoad %f32 %inloc1\n"
		"%inval2 = OpLoad %f32 %inloc2\n"
		"%inval3 = OpLoad %f32 %inloc3\n"
		"%inval4 = OpLoad %f32 %inloc4\n"
		"%add0   = OpFAdd %f32 %inval0 %inval1\n"
		"%add1   = OpFAdd %f32 %add0 %inval2\n"
		"%add2   = OpFAdd %f32 %add1 %inval3\n"
		"%add    = OpFAdd %f32 %add2 %inval4\n"
		"          OpStore %outloc %add\n"
		"          OpReturn\n"
		"          OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats0)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats1)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats2)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats3)));
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats4)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "decoration group cases", spec));

	return group.release();
}

enum SpecConstantType
{
	SC_INT8,
	SC_UINT8,
	SC_INT16,
	SC_UINT16,
	SC_INT32,
	SC_UINT32,
	SC_INT64,
	SC_UINT64,
	SC_FLOAT16,
	SC_FLOAT32,
	SC_FLOAT64,
};

struct SpecConstantValue
{
	SpecConstantType type;
	union ValueUnion {
		deInt8			i8;
		deUint8			u8;
		deInt16			i16;
		deUint16		u16;
		deInt32			i32;
		deUint32		u32;
		deInt64			i64;
		deUint64		u64;
		tcu::Float16	f16;
		tcu::Float32	f32;
		tcu::Float64	f64;

		ValueUnion (deInt8			v) : i8(v)	{}
		ValueUnion (deUint8			v) : u8(v)	{}
		ValueUnion (deInt16			v) : i16(v)	{}
		ValueUnion (deUint16		v) : u16(v)	{}
		ValueUnion (deInt32			v) : i32(v)	{}
		ValueUnion (deUint32		v) : u32(v)	{}
		ValueUnion (deInt64			v) : i64(v)	{}
		ValueUnion (deUint64		v) : u64(v)	{}
		ValueUnion (tcu::Float16	v) : f16(v)	{}
		ValueUnion (tcu::Float32	v) : f32(v)	{}
		ValueUnion (tcu::Float64	v) : f64(v)	{}
	} value;

	SpecConstantValue (deInt8			v) : type(SC_INT8)		, value(v) {}
	SpecConstantValue (deUint8			v) : type(SC_UINT8)		, value(v) {}
	SpecConstantValue (deInt16			v) : type(SC_INT16)		, value(v) {}
	SpecConstantValue (deUint16			v) : type(SC_UINT16)	, value(v) {}
	SpecConstantValue (deInt32			v) : type(SC_INT32)		, value(v) {}
	SpecConstantValue (deUint32			v) : type(SC_UINT32)	, value(v) {}
	SpecConstantValue (deInt64			v) : type(SC_INT64)		, value(v) {}
	SpecConstantValue (deUint64			v) : type(SC_UINT64)	, value(v) {}
	SpecConstantValue (tcu::Float16		v) : type(SC_FLOAT16)	, value(v) {}
	SpecConstantValue (tcu::Float32		v) : type(SC_FLOAT32)	, value(v) {}
	SpecConstantValue (tcu::Float64		v) : type(SC_FLOAT64)	, value(v) {}

	void appendTo(vkt::SpirVAssembly::SpecConstants& specConstants)
	{
		switch (type)
		{
		case SC_INT8:		specConstants.append(value.i8);		break;
		case SC_UINT8:		specConstants.append(value.u8);		break;
		case SC_INT16:		specConstants.append(value.i16);	break;
		case SC_UINT16:		specConstants.append(value.u16);	break;
		case SC_INT32:		specConstants.append(value.i32);	break;
		case SC_UINT32:		specConstants.append(value.u32);	break;
		case SC_INT64:		specConstants.append(value.i64);	break;
		case SC_UINT64:		specConstants.append(value.u64);	break;
		case SC_FLOAT16:	specConstants.append(value.f16);	break;
		case SC_FLOAT32:	specConstants.append(value.f32);	break;
		case SC_FLOAT64:	specConstants.append(value.f64);	break;
		default:
			DE_ASSERT(false);
		}
	}
};

enum CaseFlagBits
{
	FLAG_NONE		= 0,
	FLAG_CONVERT	= 1,
	FLAG_I8			= (1<<1),
	FLAG_I16		= (1<<2),
	FLAG_I64		= (1<<3),
	FLAG_F16		= (1<<4),
	FLAG_F64		= (1<<5),
};
using CaseFlags = deUint32;

struct SpecConstantTwoValCase
{
	const std::string	caseName;
	const std::string	scDefinition0;
	const std::string	scDefinition1;
	const std::string	scResultType;
	const std::string	scOperation;
	SpecConstantValue	scActualValue0;
	SpecConstantValue	scActualValue1;
	const std::string	resultOperation;
	vector<deInt32>		expectedOutput;
	CaseFlags			caseFlags;

						SpecConstantTwoValCase (const std::string& name,
												const std::string& definition0,
												const std::string& definition1,
												const std::string& resultType,
												const std::string& operation,
												SpecConstantValue value0,
												SpecConstantValue value1,
												const std::string& resultOp,
												const vector<deInt32>& output,
												CaseFlags flags = FLAG_NONE)
							: caseName				(name)
							, scDefinition0			(definition0)
							, scDefinition1			(definition1)
							, scResultType			(resultType)
							, scOperation			(operation)
							, scActualValue0		(value0)
							, scActualValue1		(value1)
							, resultOperation		(resultOp)
							, expectedOutput		(output)
							, caseFlags				(flags)
							{}
};

std::string getSpecConstantOpStructConstantsAndTypes ()
{
	return
		"%zero        = OpConstant %i32 0\n"
		"%one         = OpConstant %i32 1\n"
		"%two         = OpConstant %i32 2\n"
		"%three       = OpConstant %i32 3\n"
		"%iarr3       = OpTypeArray %i32 %three\n"
		"%imat3       = OpTypeArray %iarr3 %three\n"
		"%struct      = OpTypeStruct %imat3\n"
		;
}

std::string getSpecConstantOpStructComposites ()
{
	return
		"%iarr3_0     = OpConstantComposite %iarr3 %zero %zero %zero\n"
		"%imat3_0     = OpConstantComposite %imat3 %iarr3_0 %iarr3_0 %iarr3_0\n"
		"%struct_0    = OpConstantComposite %struct %imat3_0\n"
		;
}

std::string getSpecConstantOpStructConstBlock ()
{
	return
		"%iarr3_a     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_0        %iarr3_0     0\n"                        // Compose (sc_0, sc_1, sc_2)
		"%iarr3_b     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_1        %iarr3_a     1\n"
		"%iarr3_c     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_2        %iarr3_b     2\n"

		"%iarr3_d     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_1        %iarr3_0     0\n"                        // Compose (sc_1, sc_2, sc_0)
		"%iarr3_e     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_2        %iarr3_d     1\n"
		"%iarr3_f     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_0        %iarr3_e     2\n"

		"%iarr3_g     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_2        %iarr3_0     0\n"                        // Compose (sc_2, sc_0, sc_1)
		"%iarr3_h     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_0        %iarr3_g     1\n"
		"%iarr3_i     = OpSpecConstantOp %iarr3  CompositeInsert  %sc_1        %iarr3_h     2\n"

		"%imat3_a     = OpSpecConstantOp %imat3  CompositeInsert  %iarr3_c     %imat3_0     0\n"						// Matrix with the 3 previous arrays.
		"%imat3_b     = OpSpecConstantOp %imat3  CompositeInsert  %iarr3_f     %imat3_a     1\n"
		"%imat3_c     = OpSpecConstantOp %imat3  CompositeInsert  %iarr3_i     %imat3_b     2\n"

		"%struct_a    = OpSpecConstantOp %struct CompositeInsert  %imat3_c     %struct_0    0\n"						// Save it in the struct.

		"%comp_0_0    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 0 0\n"									// Extract some component pairs to compare them.
		"%comp_1_0    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 1 0\n"

		"%comp_0_1    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 0 1\n"
		"%comp_2_2    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 2 2\n"

		"%comp_2_0    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 2 0\n"
		"%comp_1_1    = OpSpecConstantOp %i32    CompositeExtract %struct_a    0 1 1\n"

		"%cmpres_0    = OpSpecConstantOp %bool   IEqual %comp_0_0 %comp_1_0\n"											// Must be false.
		"%cmpres_1    = OpSpecConstantOp %bool   IEqual %comp_0_1 %comp_2_2\n"											// Must be true.
		"%cmpres_2    = OpSpecConstantOp %bool   IEqual %comp_2_0 %comp_1_1\n"											// Must be true.

		"%mustbe_0    = OpSpecConstantOp %i32    Select %cmpres_0 %one %zero\n"											// Must select 0
		"%mustbe_1    = OpSpecConstantOp %i32    Select %cmpres_1 %one %zero\n"											// Must select 1
		"%mustbe_2    = OpSpecConstantOp %i32    Select %cmpres_2 %two %one\n"											// Must select 2
		;
}

std::string getSpecConstantOpStructInstructions ()
{
	return
		// Multiply final result with (1-mustbezero)*(mustbeone)*(mustbetwo-1). If everything goes right, the factor should be 1 and
		// the final result should not be altered.
		"%subf_a      = OpISub %i32 %one %mustbe_0\n"
		"%subf_b      = OpIMul %i32 %subf_a %mustbe_1\n"
		"%subf_c      = OpISub %i32 %mustbe_2 %one\n"
		"%factor      = OpIMul %i32 %subf_b %subf_c\n"
		"%sc_final    = OpIMul %i32 %factor %sc_factor\n"
		;
}

tcu::TestCaseGroup* createSpecConstantGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opspecconstantop", "Test the OpSpecConstantOp instruction"));
	vector<SpecConstantTwoValCase>	cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<deInt32>					inputInts		(numElements, 0);
	vector<deInt32>					outputInts1		(numElements, 0);
	vector<deInt32>					outputInts2		(numElements, 0);
	vector<deInt32>					outputInts3		(numElements, 0);
	vector<deInt32>					outputInts4		(numElements, 0);
	vector<deInt32>					outputInts5		(numElements, 0);
	const StringTemplate			shaderTemplate	(
		"${CAPABILITIES:opt}"
		+ string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %i32arr ArrayStride 4\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"${OPTYPE_DEFINITIONS:opt}"
		"%buf     = OpTypeStruct %i32arr\n"
		"%bufptr  = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%sc_0      = OpSpecConstant${SC_DEF0}\n"
		"%sc_1      = OpSpecConstant${SC_DEF1}\n"
		"%sc_final  = OpSpecConstantOp ${SC_RESULT_TYPE} ${SC_OP}\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"${TYPE_CONVERT:opt}"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %i32ptr %indata %zero %x\n"
		"%inval     = OpLoad %i32 %inloc\n"
		"%final     = ${GEN_RESULT}\n"
		"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	fillRandomScalars(rnd, -65536, 65536, &inputInts[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		outputInts1[ndx] = inputInts[ndx] + 42;
		outputInts2[ndx] = inputInts[ndx];
		outputInts3[ndx] = inputInts[ndx] - 11200;
		outputInts4[ndx] = inputInts[ndx] + 1;
		outputInts5[ndx] = inputInts[ndx] - 42;
	}

	const char addScToInput[]		= "OpIAdd %i32 %inval %sc_final";
	const char addSc32ToInput[]		= "OpIAdd %i32 %inval %sc_final32";
	const char selectTrueUsingSc[]	= "OpSelect %i32 %sc_final %inval %zero";
	const char selectFalseUsingSc[]	= "OpSelect %i32 %sc_final %zero %inval";

	cases.push_back(SpecConstantTwoValCase("iadd",						" %i32 0",		" %i32 0",		"%i32",		"IAdd                 %sc_0 %sc_1",			62,						-20,				addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("isub",						" %i32 0",		" %i32 0",		"%i32",		"ISub                 %sc_0 %sc_1",			100,					58,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("imul",						" %i32 0",		" %i32 0",		"%i32",		"IMul                 %sc_0 %sc_1",			-2,						-21,				addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("sdiv",						" %i32 0",		" %i32 0",		"%i32",		"SDiv                 %sc_0 %sc_1",			-126,					-3,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("udiv",						" %i32 0",		" %i32 0",		"%i32",		"UDiv                 %sc_0 %sc_1",			126,					3,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("srem",						" %i32 0",		" %i32 0",		"%i32",		"SRem                 %sc_0 %sc_1",			7,						3,					addScToInput,		outputInts4));
	cases.push_back(SpecConstantTwoValCase("smod",						" %i32 0",		" %i32 0",		"%i32",		"SMod                 %sc_0 %sc_1",			7,						3,					addScToInput,		outputInts4));
	cases.push_back(SpecConstantTwoValCase("umod",						" %i32 0",		" %i32 0",		"%i32",		"UMod                 %sc_0 %sc_1",			342,					50,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("bitwiseand",				" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",			42,						63,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("bitwiseor",					" %i32 0",		" %i32 0",		"%i32",		"BitwiseOr            %sc_0 %sc_1",			34,						8,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("bitwisexor",				" %i32 0",		" %i32 0",		"%i32",		"BitwiseXor           %sc_0 %sc_1",			18,						56,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical",			" %i32 0",		" %i32 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",			168,					2,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic",		" %i32 0",		" %i32 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",			-168,					2,					addScToInput,		outputInts5));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical",			" %i32 0",		" %i32 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",			21,						1,					addScToInput,		outputInts1));

	// Shifts for other integer sizes.
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_i64",		" %i64 0",		" %i64 0",		"%i64",		"ShiftRightLogical    %sc_0 %sc_1",			deInt64{168},			deInt64{2},			addSc32ToInput,		outputInts1, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_i64",	" %i64 0",		" %i64 0",		"%i64",		"ShiftRightArithmetic %sc_0 %sc_1",			deInt64{-168},			deInt64{2},			addSc32ToInput,		outputInts5, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_i64",		" %i64 0",		" %i64 0",		"%i64",		"ShiftLeftLogical     %sc_0 %sc_1",			deInt64{21},			deInt64{1},			addSc32ToInput,		outputInts1, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_i16",		" %i16 0",		" %i16 0",		"%i16",		"ShiftRightLogical    %sc_0 %sc_1",			deInt16{168},			deInt16{2},			addSc32ToInput,		outputInts1, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_i16",	" %i16 0",		" %i16 0",		"%i16",		"ShiftRightArithmetic %sc_0 %sc_1",			deInt16{-168},			deInt16{2},			addSc32ToInput,		outputInts5, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_i16",		" %i16 0",		" %i16 0",		"%i16",		"ShiftLeftLogical     %sc_0 %sc_1",			deInt16{21},			deInt16{1},			addSc32ToInput,		outputInts1, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_i8",		" %i8 0",		" %i8 0",		"%i8",		"ShiftRightLogical    %sc_0 %sc_1",			deInt8{84},				deInt8{1},			addSc32ToInput,		outputInts1, (FLAG_I8 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_i8",	" %i8 0",		" %i8 0",		"%i8",		"ShiftRightArithmetic %sc_0 %sc_1",			deInt8{-84},			deInt8{1},			addSc32ToInput,		outputInts5, (FLAG_I8 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_i8",		" %i8 0",		" %i8 0",		"%i8",		"ShiftLeftLogical     %sc_0 %sc_1",			deInt8{21},				deInt8{1},			addSc32ToInput,		outputInts1, (FLAG_I8 | FLAG_CONVERT)));

	// Shifts for other integer sizes but only in the shift amount.
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_s_i64",	" %i32 0",		" %i64 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",			168,					deInt64{2},			addScToInput,		outputInts1, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_s_i64"," %i32 0",		" %i64 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",			-168,					deInt64{2},			addScToInput,		outputInts5, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_s_i64",	" %i32 0",		" %i64 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",			21,						deInt64{1},			addScToInput,		outputInts1, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_s_i16",	" %i32 0",		" %i16 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",			168,					deInt16{2},			addScToInput,		outputInts1, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_s_i16"," %i32 0",		" %i16 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",			-168,					deInt16{2},			addScToInput,		outputInts5, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_s_i16",	" %i32 0",		" %i16 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",			21,						deInt16{1},			addScToInput,		outputInts1, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValCase("shiftrightlogical_s_i8",	" %i32 0",		" %i8 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",			84,						deInt8{1},			addScToInput,		outputInts1, (FLAG_I8)));
	cases.push_back(SpecConstantTwoValCase("shiftrightarithmetic_s_i8",	" %i32 0",		" %i8 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",			-84,					deInt8{1},			addScToInput,		outputInts5, (FLAG_I8)));
	cases.push_back(SpecConstantTwoValCase("shiftleftlogical_s_i8",		" %i32 0",		" %i8 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",			21,						deInt8{1},			addScToInput,		outputInts1, (FLAG_I8)));

	cases.push_back(SpecConstantTwoValCase("slessthan",					" %i32 0",		" %i32 0",		"%bool",	"SLessThan            %sc_0 %sc_1",			-20,					-10,				selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("ulessthan",					" %i32 0",		" %i32 0",		"%bool",	"ULessThan            %sc_0 %sc_1",			10,						20,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("sgreaterthan",				" %i32 0",		" %i32 0",		"%bool",	"SGreaterThan         %sc_0 %sc_1",			-1000,					50,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("ugreaterthan",				" %i32 0",		" %i32 0",		"%bool",	"UGreaterThan         %sc_0 %sc_1",			10,						5,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("slessthanequal",			" %i32 0",		" %i32 0",		"%bool",	"SLessThanEqual       %sc_0 %sc_1",			-10,					-10,				selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("ulessthanequal",			" %i32 0",		" %i32 0",		"%bool",	"ULessThanEqual       %sc_0 %sc_1",			50,						100,				selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("sgreaterthanequal",			" %i32 0",		" %i32 0",		"%bool",	"SGreaterThanEqual    %sc_0 %sc_1",			-1000,					50,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("ugreaterthanequal",			" %i32 0",		" %i32 0",		"%bool",	"UGreaterThanEqual    %sc_0 %sc_1",			10,						10,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("iequal",					" %i32 0",		" %i32 0",		"%bool",	"IEqual               %sc_0 %sc_1",			42,						24,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("inotequal",					" %i32 0",		" %i32 0",		"%bool",	"INotEqual            %sc_0 %sc_1",			42,						24,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("logicaland",				"True %bool",	"True %bool",	"%bool",	"LogicalAnd           %sc_0 %sc_1",			0,						1,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("logicalor",					"False %bool",	"False %bool",	"%bool",	"LogicalOr            %sc_0 %sc_1",			1,						0,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("logicalequal",				"True %bool",	"True %bool",	"%bool",	"LogicalEqual         %sc_0 %sc_1",			0,						1,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("logicalnotequal",			"False %bool",	"False %bool",	"%bool",	"LogicalNotEqual      %sc_0 %sc_1",			1,						0,					selectTrueUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("snegate",					" %i32 0",		" %i32 0",		"%i32",		"SNegate              %sc_0",				-42,					0,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("not",						" %i32 0",		" %i32 0",		"%i32",		"Not                  %sc_0",				-43,					0,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("logicalnot",				"False %bool",	"False %bool",	"%bool",	"LogicalNot           %sc_0",				1,						0,					selectFalseUsingSc,	outputInts2));
	cases.push_back(SpecConstantTwoValCase("select",					"False %bool",	" %i32 0",		"%i32",		"Select               %sc_0 %sc_1 %zero",	1,						42,					addScToInput,		outputInts1));
	cases.push_back(SpecConstantTwoValCase("sconvert",					" %i32 0",		" %i32 0",		"%i16",		"SConvert             %sc_0",				-11200,					0,					addSc32ToInput,		outputInts3, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("fconvert",					" %f32 0",		" %f32 0",		"%f64",		"FConvert             %sc_0",				tcu::Float32{-11200.0},	tcu::Float32{0.0},	addSc32ToInput,		outputInts3, (FLAG_F64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValCase("fconvert16",				" %f16 0",		" %f16 0",		"%f32",		"FConvert             %sc_0",				tcu::Float16{1.0},		tcu::Float16{0.0},	addSc32ToInput,		outputInts4, (FLAG_F16 | FLAG_CONVERT)));

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["SC_DEF0"]			= cases[caseNdx].scDefinition0;
		specializations["SC_DEF1"]			= cases[caseNdx].scDefinition1;
		specializations["SC_RESULT_TYPE"]	= cases[caseNdx].scResultType;
		specializations["SC_OP"]			= cases[caseNdx].scOperation;
		specializations["GEN_RESULT"]		= cases[caseNdx].resultOperation;

		// Special SPIR-V code when using 16-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I16)
		{
			spec.requestedVulkanFeatures.coreFeatures.shaderInt16	= VK_TRUE;
			specializations["CAPABILITIES"]							+= "OpCapability Int16\n";							// Adds 16-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]					+= "%i16 = OpTypeInt 16 1\n";						// Adds 16-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]						+= "%sc_final32 = OpSConvert %i32 %sc_final\n";		// Converts 16-bit integer to 32-bit integer
		}

		// Special SPIR-V code when using 64-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I64)
		{
			spec.requestedVulkanFeatures.coreFeatures.shaderInt64	= VK_TRUE;
			specializations["CAPABILITIES"]							+= "OpCapability Int64\n";							// Adds 64-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]					+= "%i64 = OpTypeInt 64 1\n";						// Adds 64-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]						+= "%sc_final32 = OpSConvert %i32 %sc_final\n";		// Converts 64-bit integer to 32-bit integer
		}

		// Special SPIR-V code when using 64-bit floats.
		if (cases[caseNdx].caseFlags & FLAG_F64)
		{
			spec.requestedVulkanFeatures.coreFeatures.shaderFloat64	= VK_TRUE;
			specializations["CAPABILITIES"]							+= "OpCapability Float64\n";						// Adds 64-bit float capability
			specializations["OPTYPE_DEFINITIONS"]					+= "%f64 = OpTypeFloat 64\n";						// Adds 64-bit float type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]						+= "%sc_final32 = OpConvertFToS %i32 %sc_final\n";	// Converts 64-bit float to 32-bit integer
		}

		// Extension needed for float16 and int8.
		if (cases[caseNdx].caseFlags & (FLAG_F16 | FLAG_I8))
			spec.extensions.push_back("VK_KHR_shader_float16_int8");

		// Special SPIR-V code when using 16-bit floats.
		if (cases[caseNdx].caseFlags & FLAG_F16)
		{
			spec.requestedVulkanFeatures.extFloat16Int8	|= EXTFLOAT16INT8FEATURES_FLOAT16;
			specializations["CAPABILITIES"]				+= "OpCapability Float16\n";						// Adds 16-bit float capability
			specializations["OPTYPE_DEFINITIONS"]		+= "%f16 = OpTypeFloat 16\n";						// Adds 16-bit float type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]			+= "%sc_final32 = OpConvertFToS %i32 %sc_final\n";	// Converts 16-bit float to 32-bit integer
		}

		// Special SPIR-V code when using 8-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I8)
		{
			spec.requestedVulkanFeatures.extFloat16Int8	|= EXTFLOAT16INT8FEATURES_INT8;
			specializations["CAPABILITIES"]				+= "OpCapability Int8\n";						// Adds 8-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]		+= "%i8 = OpTypeInt 8 1\n";						// Adds 8-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]			+= "%sc_final32 = OpSConvert %i32 %sc_final\n";	// Converts 8-bit integer to 32-bit integer
		}

		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Int32Buffer(inputInts)));
		spec.outputs.push_back(BufferSp(new Int32Buffer(cases[caseNdx].expectedOutput)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		cases[caseNdx].scActualValue0.appendTo(spec.specConstants);
		cases[caseNdx].scActualValue1.appendTo(spec.specConstants);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].caseName.c_str(), cases[caseNdx].caseName.c_str(), spec));
	}

	ComputeShaderSpec				spec;

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n"
		"OpDecorate %i32arr ArrayStride 4\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%ivec3       = OpTypeVector %i32 3\n"

		+ getSpecConstantOpStructConstantsAndTypes() +

		"%buf         = OpTypeStruct %i32arr\n"
		"%bufptr      = OpTypePointer Uniform %buf\n"
		"%indata      = OpVariable %bufptr Uniform\n"
		"%outdata     = OpVariable %bufptr Uniform\n"

		"%id          = OpVariable %uvec3ptr Input\n"
		"%ivec3_0     = OpConstantComposite %ivec3 %zero %zero %zero\n"
		"%vec3_undef  = OpUndef %ivec3\n"

		+ getSpecConstantOpStructComposites () +

		"%sc_0        = OpSpecConstant %i32 0\n"
		"%sc_1        = OpSpecConstant %i32 0\n"
		"%sc_2        = OpSpecConstant %i32 0\n"

		+ getSpecConstantOpStructConstBlock () +

		"%sc_vec3_0   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_0        %ivec3_0     0\n"							// (sc_0, 0, 0)
		"%sc_vec3_1   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_1        %ivec3_0     1\n"							// (0, sc_1, 0)
		"%sc_vec3_2   = OpSpecConstantOp %ivec3 CompositeInsert  %sc_2        %ivec3_0     2\n"							// (0, 0, sc_2)
		"%sc_vec3_0_s = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_0   %vec3_undef  0          0xFFFFFFFF 2\n"	// (sc_0, ???,  0)
		"%sc_vec3_1_s = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_1   %vec3_undef  0xFFFFFFFF 1          0\n"	// (???,  sc_1, 0)
		"%sc_vec3_2_s = OpSpecConstantOp %ivec3 VectorShuffle    %vec3_undef  %sc_vec3_2   5          0xFFFFFFFF 5\n"	// (sc_2, ???,  sc_2)
		"%sc_vec3_01  = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_0_s %sc_vec3_1_s 1 0 4\n"						// (0,    sc_0, sc_1)
		"%sc_vec3_012 = OpSpecConstantOp %ivec3 VectorShuffle    %sc_vec3_01  %sc_vec3_2_s 5 1 2\n"						// (sc_2, sc_0, sc_1)
		"%sc_ext_0    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              0\n"							// sc_2
		"%sc_ext_1    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              1\n"							// sc_0
		"%sc_ext_2    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              2\n"							// sc_1
		"%sc_sub      = OpSpecConstantOp %i32   ISub             %sc_ext_0    %sc_ext_1\n"								// (sc_2 - sc_0)
		"%sc_factor   = OpSpecConstantOp %i32   IMul             %sc_sub      %sc_ext_2\n"								// (sc_2 - sc_0) * sc_1

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"

		+ getSpecConstantOpStructInstructions() +

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %i32ptr %indata %zero %x\n"
		"%inval     = OpLoad %i32 %inloc\n"
		"%final     = OpIAdd %i32 %inval %sc_final\n"
		"%outloc    = OpAccessChain %i32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Int32Buffer(inputInts)));
	spec.outputs.push_back(BufferSp(new Int32Buffer(outputInts3)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);
	spec.specConstants.append<deInt32>(123);
	spec.specConstants.append<deInt32>(56);
	spec.specConstants.append<deInt32>(-77);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vector_related", "VectorShuffle, CompositeExtract, & CompositeInsert", spec));

	return group.release();
}

void createOpPhiVartypeTests (de::MovePtr<tcu::TestCaseGroup>& group, tcu::TestContext& testCtx)
{
	ComputeShaderSpec	specInt;
	ComputeShaderSpec	specFloat;
	ComputeShaderSpec	specFloat16;
	ComputeShaderSpec	specVec3;
	ComputeShaderSpec	specMat4;
	ComputeShaderSpec	specArray;
	ComputeShaderSpec	specStruct;
	de::Random			rnd				(deStringHash(group->getName()));
	const int			numElements		= 100;
	vector<float>		inputFloats		(numElements, 0);
	vector<float>		outputFloats	(numElements, 0);
	vector<deUint32>	inputUints		(numElements, 0);
	vector<deUint32>	outputUints		(numElements, 0);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		// Just check if the value is positive or not
		outputFloats[ndx] = (inputFloats[ndx] > 0) ? 1.0f : -1.0f;
	}

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		inputUints[ndx] = tcu::Float16(inputFloats[ndx]).bits();
		outputUints[ndx] = tcu::Float16(outputFloats[ndx]).bits();
	}

	// All of the tests are of the form:
	//
	// testtype r
	//
	// if (inputdata > 0)
	//   r = 1
	// else
	//   r = -1
	//
	// return (float)r

	specFloat.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%float_1    = OpConstant %f32 1.0\n"
		"%float_n1   = OpConstant %f32 -1.0\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%res      = OpPhi %f32 %float_1 %tb %float_n1 %fb\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specFloat.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specFloat.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specFloat.numWorkGroups = IVec3(numElements, 1, 1);

	specFloat16.assembly =
		"OpCapability Shader\n"
		"OpCapability Float16\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %u32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%f16      = OpTypeFloat 16\n"
		"%f16vec2  = OpTypeVector %f16 2\n"
		"%fvec2    = OpTypeVector %f32 2\n"
		"%u32ptr   = OpTypePointer Uniform %u32\n"
		"%u32arr   = OpTypeRuntimeArray %u32\n"
		"%f16_0    = OpConstant %f16 0.0\n"


		"%buf      = OpTypeStruct %u32arr\n"
		"%bufptr   = OpTypePointer Uniform %buf\n"
		"%indata   = OpVariable %bufptr Uniform\n"
		"%outdata  = OpVariable %bufptr Uniform\n"

		"%id       = OpVariable %uvec3ptr Input\n"
		"%zero     = OpConstant %i32 0\n"
		"%float_0  = OpConstant %f32 0.0\n"
		"%float_1  = OpConstant %f32 1.0\n"
		"%float_n1 = OpConstant %f32 -1.0\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %u32ptr %indata %zero %x\n"
		"%inval    = OpLoad %u32 %inloc\n"
		"%f16_vec2_inval = OpBitcast %f16vec2 %inval\n"
		"%f16_inval = OpCompositeExtract %f16 %f16_vec2_inval 0\n"
		"%f32_inval = OpFConvert %f32 %f16_inval\n"

		"%comp     = OpFOrdGreaterThan %bool %f32_inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%res      = OpPhi %f32 %float_1 %tb %float_n1 %fb\n"
		"%f16_res  = OpFConvert %f16 %res\n"

		"%f16vec2_res = OpCompositeConstruct %f16vec2 %f16_res %f16_0\n"
		"%u32_res  = OpBitcast %u32 %f16vec2_res\n"

		"%outloc   = OpAccessChain %u32ptr %outdata %zero %x\n"
		"            OpStore %outloc %u32_res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";

	specFloat16.inputs.push_back(BufferSp(new Uint32Buffer(inputUints)));
	specFloat16.outputs.push_back(BufferSp(new Uint32Buffer(outputUints)));
	specFloat16.numWorkGroups = IVec3(numElements, 1, 1);
	specFloat16.requestedVulkanFeatures.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;

	specMat4.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%v4f32      = OpTypeVector %f32 4\n"
		"%mat4v4f32  = OpTypeMatrix %v4f32 4\n"
		"%zero       = OpConstant %i32 0\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%float_1    = OpConstant %f32 1.0\n"
		"%float_n1   = OpConstant %f32 -1.0\n"
		"%m11        = OpConstantComposite %v4f32 %float_1 %float_0 %float_0 %float_0\n"
		"%m12        = OpConstantComposite %v4f32 %float_0 %float_1 %float_0 %float_0\n"
		"%m13        = OpConstantComposite %v4f32 %float_0 %float_0 %float_1 %float_0\n"
		"%m14        = OpConstantComposite %v4f32 %float_0 %float_0 %float_0 %float_1\n"
		"%m1         = OpConstantComposite %mat4v4f32 %m11 %m12 %m13 %m14\n"
		"%m21        = OpConstantComposite %v4f32 %float_n1 %float_0 %float_0 %float_0\n"
		"%m22        = OpConstantComposite %v4f32 %float_0 %float_n1 %float_0 %float_0\n"
		"%m23        = OpConstantComposite %v4f32 %float_0 %float_0 %float_n1 %float_0\n"
		"%m24        = OpConstantComposite %v4f32 %float_0 %float_0 %float_0 %float_n1\n"
		"%m2         = OpConstantComposite %mat4v4f32 %m21 %m22 %m23 %m24\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%mres     = OpPhi %mat4v4f32 %m1 %tb %m2 %fb\n"
		"%res      = OpCompositeExtract %f32 %mres 2 2\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specMat4.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specMat4.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specMat4.numWorkGroups = IVec3(numElements, 1, 1);

	specVec3.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%float_1    = OpConstant %f32 1.0\n"
		"%float_n1   = OpConstant %f32 -1.0\n"
		"%v1         = OpConstantComposite %fvec3 %float_1 %float_1 %float_1\n"
		"%v2         = OpConstantComposite %fvec3 %float_n1 %float_n1 %float_n1\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%vres     = OpPhi %fvec3 %v1 %tb %v2 %fb\n"
		"%res      = OpCompositeExtract %f32 %vres 2\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specVec3.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specVec3.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specVec3.numWorkGroups = IVec3(numElements, 1, 1);

	specInt.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%i1         = OpConstant %i32 1\n"
		"%i2         = OpConstant %i32 -1\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%ires     = OpPhi %i32 %i1 %tb %i2 %fb\n"
		"%res      = OpConvertSToF %f32 %ires\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specInt.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specInt.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specInt.numWorkGroups = IVec3(numElements, 1, 1);

	specArray.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%u7         = OpConstant %u32 7\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%float_1    = OpConstant %f32 1.0\n"
		"%float_n1   = OpConstant %f32 -1.0\n"
		"%f32a7      = OpTypeArray %f32 %u7\n"
		"%a1         = OpConstantComposite %f32a7 %float_1 %float_1 %float_1 %float_1 %float_1 %float_1 %float_1\n"
		"%a2         = OpConstantComposite %f32a7 %float_n1 %float_n1 %float_n1 %float_n1 %float_n1 %float_n1 %float_n1\n"
		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%ares     = OpPhi %f32a7 %a1 %tb %a2 %fb\n"
		"%res      = OpCompositeExtract %f32 %ares 5\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specArray.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specArray.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specArray.numWorkGroups = IVec3(numElements, 1, 1);

	specStruct.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%float_0    = OpConstant %f32 0.0\n"
		"%float_1    = OpConstant %f32 1.0\n"
		"%float_n1   = OpConstant %f32 -1.0\n"

		"%v2f32      = OpTypeVector %f32 2\n"
		"%Data2      = OpTypeStruct %f32 %v2f32\n"
		"%Data       = OpTypeStruct %Data2 %f32\n"

		"%in1a       = OpConstantComposite %v2f32 %float_1 %float_1\n"
		"%in1b       = OpConstantComposite %Data2 %float_1 %in1a\n"
		"%s1         = OpConstantComposite %Data %in1b %float_1\n"
		"%in2a       = OpConstantComposite %v2f32 %float_n1 %float_n1\n"
		"%in2b       = OpConstantComposite %Data2 %float_n1 %in2a\n"
		"%s2         = OpConstantComposite %Data %in2b %float_n1\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%comp     = OpFOrdGreaterThan %bool %inval %float_0\n"
		"            OpSelectionMerge %cm None\n"
		"            OpBranchConditional %comp %tb %fb\n"
		"%tb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%fb       = OpLabel\n"
		"            OpBranch %cm\n"
		"%cm       = OpLabel\n"
		"%sres     = OpPhi %Data %s1 %tb %s2 %fb\n"
		"%res      = OpCompositeExtract %f32 %sres 0 0\n"

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	specStruct.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	specStruct.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	specStruct.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_int", "OpPhi with int variables", specInt));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_float", "OpPhi with float variables", specFloat));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_float16", "OpPhi with 16bit float variables", specFloat16));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_vec3", "OpPhi with vec3 variables", specVec3));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_mat4", "OpPhi with mat4 variables", specMat4));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_array", "OpPhi with array variables", specArray));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "vartype_struct", "OpPhi with struct variables", specStruct));
}

string generateConstantDefinitions (int count)
{
	std::ostringstream	r;
	for (int i = 0; i < count; i++)
		r << "%cf" << (i * 10 + 5) << " = OpConstant %f32 " <<(i * 10 + 5) << ".0\n";
	r << "\n";
	return r.str();
}

string generateSwitchCases (int count)
{
	std::ostringstream	r;
	for (int i = 0; i < count; i++)
		r << " " << i << " %case" << i;
	r << "\n";
	return r.str();
}

string generateSwitchTargets (int count)
{
	std::ostringstream	r;
	for (int i = 0; i < count; i++)
		r << "%case" << i << " = OpLabel\n            OpBranch %phi\n";
	r << "\n";
	return r.str();
}

string generateOpPhiParams (int count)
{
	std::ostringstream	r;
	for (int i = 0; i < count; i++)
		r << " %cf" << (i * 10 + 5) << " %case" << i;
	r << "\n";
	return r.str();
}

string generateIntWidth (int value)
{
	std::ostringstream	r;
	r << value;
	return r.str();
}

// Expand input string by injecting "ABC" between the input
// string characters. The acc/add/treshold parameters are used
// to skip some of the injections to make the result less
// uniform (and a lot shorter).
string expandOpPhiCase5 (const string& s, int &acc, int add, int treshold)
{
	std::ostringstream	res;
	const char*			p = s.c_str();

	while (*p)
	{
		res << *p;
		acc += add;
		if (acc > treshold)
		{
			acc -= treshold;
			res << "ABC";
		}
		p++;
	}
	return res.str();
}

// Calculate expected result based on the code string
float calcOpPhiCase5 (float val, const string& s)
{
	const char*		p		= s.c_str();
	float			x[8];
	bool			b[8];
	const float		tv[8]	= { 0.5f, 1.5f, 3.5f, 7.5f, 15.5f, 31.5f, 63.5f, 127.5f };
	const float		v		= deFloatAbs(val);
	float			res		= 0;
	int				depth	= -1;
	int				skip	= 0;

	for (int i = 7; i >= 0; --i)
		x[i] = std::fmod((float)v, (float)(2 << i));
	for (int i = 7; i >= 0; --i)
		b[i] = x[i] > tv[i];

	while (*p)
	{
		if (*p == 'A')
		{
			depth++;
			if (skip == 0 && b[depth])
			{
				res++;
			}
			else
				skip++;
		}
		if (*p == 'B')
		{
			if (skip)
				skip--;
			if (b[depth] || skip)
				skip++;
		}
		if (*p == 'C')
		{
			depth--;
			if (skip)
				skip--;
		}
		p++;
	}
	return res;
}

// In the code string, the letters represent the following:
//
// A:
//     if (certain bit is set)
//     {
//       result++;
//
// B:
//     } else {
//
// C:
//     }
//
// examples:
// AABCBC leads to if(){r++;if(){r++;}else{}}else{}
// ABABCC leads to if(){r++;}else{if(){r++;}else{}}
// ABCABC leads to if(){r++;}else{}if(){r++;}else{}
//
// Code generation gets a bit complicated due to the else-branches,
// which do not generate new values. Thus, the generator needs to
// keep track of the previous variable change seen by the else
// branch.
string generateOpPhiCase5 (const string& s)
{
	std::stack<int>				idStack;
	std::stack<std::string>		value;
	std::stack<std::string>		valueLabel;
	std::stack<std::string>		mergeLeft;
	std::stack<std::string>		mergeRight;
	std::ostringstream			res;
	const char*					p			= s.c_str();
	int							depth		= -1;
	int							currId		= 0;
	int							iter		= 0;

	idStack.push(-1);
	value.push("%f32_0");
	valueLabel.push("%f32_0 %entry");

	while (*p)
	{
		if (*p == 'A')
		{
			depth++;
			currId = iter;
			idStack.push(currId);
			res << "\tOpSelectionMerge %m" << currId << " None\n";
			res << "\tOpBranchConditional %b" << depth << " %t" << currId << " %f" << currId << "\n";
			res << "%t" << currId << " = OpLabel\n";
			res << "%rt" << currId << " = OpFAdd %f32 " << value.top() << " %f32_1\n";
			std::ostringstream tag;
			tag << "%rt" << currId;
			value.push(tag.str());
			tag << " %t" << currId;
			valueLabel.push(tag.str());
		}

		if (*p == 'B')
		{
			mergeLeft.push(valueLabel.top());
			value.pop();
			valueLabel.pop();
			res << "\tOpBranch %m" << currId << "\n";
			res << "%f" << currId << " = OpLabel\n";
			std::ostringstream tag;
			tag << value.top() << " %f" << currId;
			valueLabel.pop();
			valueLabel.push(tag.str());
		}

		if (*p == 'C')
		{
			mergeRight.push(valueLabel.top());
			res << "\tOpBranch %m" << currId << "\n";
			res << "%m" << currId << " = OpLabel\n";
			if (*(p + 1) == 0)
				res << "%res"; // last result goes to %res
			else
				res << "%rm" << currId;
			res << " = OpPhi %f32  " << mergeLeft.top() << "  " << mergeRight.top() << "\n";
			std::ostringstream tag;
			tag << "%rm" << currId;
			value.pop();
			value.push(tag.str());
			tag << " %m" << currId;
			valueLabel.pop();
			valueLabel.push(tag.str());
			mergeLeft.pop();
			mergeRight.pop();
			depth--;
			idStack.pop();
			currId = idStack.top();
		}
		p++;
		iter++;
	}
	return res.str();
}

tcu::TestCaseGroup* createOpPhiGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opphi", "Test the OpPhi instruction"));
	ComputeShaderSpec				spec1;
	ComputeShaderSpec				spec2;
	ComputeShaderSpec				spec3;
	ComputeShaderSpec				spec4;
	ComputeShaderSpec				spec5;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats1	(numElements, 0);
	vector<float>					outputFloats2	(numElements, 0);
	vector<float>					outputFloats3	(numElements, 0);
	vector<float>					outputFloats4	(numElements, 0);
	vector<float>					outputFloats5	(numElements, 0);
	std::string						codestring		= "ABC";
	const int						test4Width		= 512;

	// Build case 5 code string. Each iteration makes the hierarchy more complicated.
	// 9 iterations with (7, 24) parameters makes the hierarchy 8 deep with about 1500 lines of
	// shader code.
	for (int i = 0, acc = 0; i < 9; i++)
		codestring = expandOpPhiCase5(codestring, acc, 7, 24);

	fillRandomScalars(rnd, -300.f, 300.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		switch (ndx % 3)
		{
			case 0:		outputFloats1[ndx] = inputFloats[ndx] + 5.5f;	break;
			case 1:		outputFloats1[ndx] = inputFloats[ndx] + 20.5f;	break;
			case 2:		outputFloats1[ndx] = inputFloats[ndx] + 1.75f;	break;
			default:	break;
		}
		outputFloats2[ndx] = inputFloats[ndx] + 6.5f * 3;
		outputFloats3[ndx] = 8.5f - inputFloats[ndx];

		int index4 = (int)deFloor(deAbs((float)ndx * inputFloats[ndx]));
		outputFloats4[ndx] = (float)(index4 % test4Width) * 10.0f + 5.0f;

		outputFloats5[ndx] = calcOpPhiCase5(inputFloats[ndx], codestring);
	}

	spec1.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%three      = OpConstant %u32 3\n"
		"%constf5p5  = OpConstant %f32 5.5\n"
		"%constf20p5 = OpConstant %f32 20.5\n"
		"%constf1p75 = OpConstant %f32 1.75\n"
		"%constf8p5  = OpConstant %f32 8.5\n"
		"%constf6p5  = OpConstant %f32 6.5\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%selector = OpUMod %u32 %x %three\n"
		"            OpSelectionMerge %phi None\n"
		"            OpSwitch %selector %default 0 %case0 1 %case1 2 %case2\n"

		// Case 1 before OpPhi.
		"%case1    = OpLabel\n"
		"            OpBranch %phi\n"

		"%default  = OpLabel\n"
		"            OpUnreachable\n"

		"%phi      = OpLabel\n"
		"%operand  = OpPhi %f32   %constf1p75 %case2   %constf20p5 %case1   %constf5p5 %case0\n" // not in the order of blocks
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"
		"%add      = OpFAdd %f32 %inval %operand\n"
		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %add\n"
		"            OpReturn\n"

		// Case 0 after OpPhi.
		"%case0    = OpLabel\n"
		"            OpBranch %phi\n"


		// Case 2 after OpPhi.
		"%case2    = OpLabel\n"
		"            OpBranch %phi\n"

		"            OpFunctionEnd\n";
	spec1.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec1.outputs.push_back(BufferSp(new Float32Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "block", "out-of-order and unreachable blocks for OpPhi", spec1));

	spec2.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id         = OpVariable %uvec3ptr Input\n"
		"%zero       = OpConstant %i32 0\n"
		"%one        = OpConstant %i32 1\n"
		"%three      = OpConstant %i32 3\n"
		"%constf6p5  = OpConstant %f32 6.5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%entry      = OpLabel\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%inval      = OpLoad %f32 %inloc\n"
		"              OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%step       = OpPhi %i32 %zero  %entry %step_next  %phi\n"
		"%accum      = OpPhi %f32 %inval %entry %accum_next %phi\n"
		"%step_next  = OpIAdd %i32 %step %one\n"
		"%accum_next = OpFAdd %f32 %accum %constf6p5\n"
		"%still_loop = OpSLessThan %bool %step %three\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %outloc %accum\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";
	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "induction", "The usual way induction variables are handled in LLVM IR", spec2));

	spec3.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%f32ptr_f   = OpTypePointer Function %f32\n"
		"%id         = OpVariable %uvec3ptr Input\n"
		"%true       = OpConstantTrue %bool\n"
		"%false      = OpConstantFalse %bool\n"
		"%zero       = OpConstant %i32 0\n"
		"%constf8p5  = OpConstant %f32 8.5\n"

		"%main       = OpFunction %void None %voidf\n"
		"%entry      = OpLabel\n"
		"%b          = OpVariable %f32ptr_f Function %constf8p5\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %f32ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%a_init     = OpLoad %f32 %inloc\n"
		"%b_init     = OpLoad %f32 %b\n"
		"              OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%still_loop = OpPhi %bool %true   %entry %false  %phi\n"
		"%a_next     = OpPhi %f32  %a_init %entry %b_next %phi\n"
		"%b_next     = OpPhi %f32  %b_init %entry %a_next %phi\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"%sub        = OpFSub %f32 %a_next %b_next\n"
		"              OpStore %outloc %sub\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n";
	spec3.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec3.outputs.push_back(BufferSp(new Float32Buffer(outputFloats3)));
	spec3.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "swap", "Swap the values of two variables using OpPhi", spec3));

	spec4.assembly =
		"OpCapability Shader\n"
		"%ext = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id       = OpVariable %uvec3ptr Input\n"
		"%zero     = OpConstant %i32 0\n"
		"%cimod    = OpConstant %u32 " + generateIntWidth(test4Width) + "\n"

		+ generateConstantDefinitions(test4Width) +

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"
		"%xf       = OpConvertUToF %f32 %x\n"
		"%xm       = OpFMul %f32 %xf %inval\n"
		"%xa       = OpExtInst %f32 %ext FAbs %xm\n"
		"%xi       = OpConvertFToU %u32 %xa\n"
		"%selector = OpUMod %u32 %xi %cimod\n"
		"            OpSelectionMerge %phi None\n"
		"            OpSwitch %selector %default "

		+ generateSwitchCases(test4Width) +

		"%default  = OpLabel\n"
		"            OpUnreachable\n"

		+ generateSwitchTargets(test4Width) +

		"%phi      = OpLabel\n"
		"%result   = OpPhi %f32"

		+ generateOpPhiParams(test4Width) +

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %result\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	spec4.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec4.outputs.push_back(BufferSp(new Float32Buffer(outputFloats4)));
	spec4.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "wide", "OpPhi with a lot of parameters", spec4));

	spec5.assembly =
		"OpCapability Shader\n"
		"%ext      = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"%code     = OpString \"" + codestring + "\"\n"

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id       = OpVariable %uvec3ptr Input\n"
		"%zero     = OpConstant %i32 0\n"
		"%f32_0    = OpConstant %f32 0.0\n"
		"%f32_0_5  = OpConstant %f32 0.5\n"
		"%f32_1    = OpConstant %f32 1.0\n"
		"%f32_1_5  = OpConstant %f32 1.5\n"
		"%f32_2    = OpConstant %f32 2.0\n"
		"%f32_3_5  = OpConstant %f32 3.5\n"
		"%f32_4    = OpConstant %f32 4.0\n"
		"%f32_7_5  = OpConstant %f32 7.5\n"
		"%f32_8    = OpConstant %f32 8.0\n"
		"%f32_15_5 = OpConstant %f32 15.5\n"
		"%f32_16   = OpConstant %f32 16.0\n"
		"%f32_31_5 = OpConstant %f32 31.5\n"
		"%f32_32   = OpConstant %f32 32.0\n"
		"%f32_63_5 = OpConstant %f32 63.5\n"
		"%f32_64   = OpConstant %f32 64.0\n"
		"%f32_127_5 = OpConstant %f32 127.5\n"
		"%f32_128  = OpConstant %f32 128.0\n"
		"%f32_256  = OpConstant %f32 256.0\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"

		"%xabs     = OpExtInst %f32 %ext FAbs %inval\n"
		"%x8       = OpFMod %f32 %xabs %f32_256\n"
		"%x7       = OpFMod %f32 %xabs %f32_128\n"
		"%x6       = OpFMod %f32 %xabs %f32_64\n"
		"%x5       = OpFMod %f32 %xabs %f32_32\n"
		"%x4       = OpFMod %f32 %xabs %f32_16\n"
		"%x3       = OpFMod %f32 %xabs %f32_8\n"
		"%x2       = OpFMod %f32 %xabs %f32_4\n"
		"%x1       = OpFMod %f32 %xabs %f32_2\n"

		"%b7       = OpFOrdGreaterThanEqual %bool %x8 %f32_127_5\n"
		"%b6       = OpFOrdGreaterThanEqual %bool %x7 %f32_63_5\n"
		"%b5       = OpFOrdGreaterThanEqual %bool %x6 %f32_31_5\n"
		"%b4       = OpFOrdGreaterThanEqual %bool %x5 %f32_15_5\n"
		"%b3       = OpFOrdGreaterThanEqual %bool %x4 %f32_7_5\n"
		"%b2       = OpFOrdGreaterThanEqual %bool %x3 %f32_3_5\n"
		"%b1       = OpFOrdGreaterThanEqual %bool %x2 %f32_1_5\n"
		"%b0       = OpFOrdGreaterThanEqual %bool %x1 %f32_0_5\n"

		+ generateOpPhiCase5(codestring) +

		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"            OpStore %outloc %res\n"
		"            OpReturn\n"

		"            OpFunctionEnd\n";
	spec5.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec5.outputs.push_back(BufferSp(new Float32Buffer(outputFloats5)));
	spec5.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "nested", "Stress OpPhi with a lot of nesting", spec5));

	createOpPhiVartypeTests(group, testCtx);

	return group.release();
}

// Assembly code used for testing block order is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x];
//   if (x > uint(50)) {
//     switch (x % uint(3)) {
//       case 0: output_data.elements[x] += 1.5f; break;
//       case 1: output_data.elements[x] += 42.f; break;
//       case 2: output_data.elements[x] -= 27.f; break;
//       default: break;
//     }
//   } else {
//     output_data.elements[x] = -input_data.elements[x];
//   }
// }
tcu::TestCaseGroup* createBlockOrderGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "block_order", "Test block orders"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx <= 50; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	for (size_t ndx = 51; ndx < numElements; ++ndx)
	{
		switch (ndx % 3)
		{
			case 0:		outputFloats[ndx] = inputFloats[ndx] + 1.5f; break;
			case 1:		outputFloats[ndx] = inputFloats[ndx] + 42.f; break;
			case 2:		outputFloats[ndx] = inputFloats[ndx] - 27.f; break;
			default:	break;
		}
	}

	spec.assembly =
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%u32ptr       = OpTypePointer Function %u32\n"
		"%u32ptr_input = OpTypePointer Input %u32\n"

		+ string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%const3    = OpConstant %u32 3\n"
		"%const50   = OpConstant %u32 50\n"
		"%constf1p5 = OpConstant %f32 1.5\n"
		"%constf27  = OpConstant %f32 27.0\n"
		"%constf42  = OpConstant %f32 42.0\n"

		"%main = OpFunction %void None %voidf\n"

		// entry block.
		"%entry    = OpLabel\n"

		// Create a temporary variable to hold the value of gl_GlobalInvocationID.x.
		"%xvar     = OpVariable %u32ptr Function\n"
		"%xptr     = OpAccessChain %u32ptr_input %id %zero\n"
		"%x        = OpLoad %u32 %xptr\n"
		"            OpStore %xvar %x\n"

		"%cmp      = OpUGreaterThan %bool %x %const50\n"
		"            OpSelectionMerge %if_merge None\n"
		"            OpBranchConditional %cmp %if_true %if_false\n"

		// False branch for if-statement: placed in the middle of switch cases and before true branch.
		"%if_false = OpLabel\n"
		"%x_f      = OpLoad %u32 %xvar\n"
		"%inloc_f  = OpAccessChain %f32ptr %indata %zero %x_f\n"
		"%inval_f  = OpLoad %f32 %inloc_f\n"
		"%negate   = OpFNegate %f32 %inval_f\n"
		"%outloc_f = OpAccessChain %f32ptr %outdata %zero %x_f\n"
		"            OpStore %outloc_f %negate\n"
		"            OpBranch %if_merge\n"

		// Merge block for if-statement: placed in the middle of true and false branch.
		"%if_merge = OpLabel\n"
		"            OpReturn\n"

		// True branch for if-statement: placed in the middle of swtich cases and after the false branch.
		"%if_true  = OpLabel\n"
		"%xval_t   = OpLoad %u32 %xvar\n"
		"%mod      = OpUMod %u32 %xval_t %const3\n"
		"            OpSelectionMerge %switch_merge None\n"
		"            OpSwitch %mod %default 0 %case0 1 %case1 2 %case2\n"

		// Merge block for switch-statement: placed before the case
		// bodies.  But it must follow OpSwitch which dominates it.
		"%switch_merge = OpLabel\n"
		"                OpBranch %if_merge\n"

		// Case 1 for switch-statement: placed before case 0.
		// It must follow the OpSwitch that dominates it.
		"%case1    = OpLabel\n"
		"%x_1      = OpLoad %u32 %xvar\n"
		"%inloc_1  = OpAccessChain %f32ptr %indata %zero %x_1\n"
		"%inval_1  = OpLoad %f32 %inloc_1\n"
		"%addf42   = OpFAdd %f32 %inval_1 %constf42\n"
		"%outloc_1 = OpAccessChain %f32ptr %outdata %zero %x_1\n"
		"            OpStore %outloc_1 %addf42\n"
		"            OpBranch %switch_merge\n"

		// Case 2 for switch-statement.
		"%case2    = OpLabel\n"
		"%x_2      = OpLoad %u32 %xvar\n"
		"%inloc_2  = OpAccessChain %f32ptr %indata %zero %x_2\n"
		"%inval_2  = OpLoad %f32 %inloc_2\n"
		"%subf27   = OpFSub %f32 %inval_2 %constf27\n"
		"%outloc_2 = OpAccessChain %f32ptr %outdata %zero %x_2\n"
		"            OpStore %outloc_2 %subf27\n"
		"            OpBranch %switch_merge\n"

		// Default case for switch-statement: placed in the middle of normal cases.
		"%default = OpLabel\n"
		"           OpBranch %switch_merge\n"

		// Case 0 for switch-statement: out of order.
		"%case0    = OpLabel\n"
		"%x_0      = OpLoad %u32 %xvar\n"
		"%inloc_0  = OpAccessChain %f32ptr %indata %zero %x_0\n"
		"%inval_0  = OpLoad %f32 %inloc_0\n"
		"%addf1p5  = OpFAdd %f32 %inval_0 %constf1p5\n"
		"%outloc_0 = OpAccessChain %f32ptr %outdata %zero %x_0\n"
		"            OpStore %outloc_0 %addf1p5\n"
		"            OpBranch %switch_merge\n"

		"            OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "all", "various out-of-order blocks", spec));

	return group.release();
}

tcu::TestCaseGroup* createMultipleShaderGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "multiple_shaders", "Test multiple shaders in the same module"));
	ComputeShaderSpec				spec1;
	ComputeShaderSpec				spec2;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats1	(numElements, 0);
	vector<float>					outputFloats2	(numElements, 0);
	fillRandomScalars(rnd, -500.f, 500.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
	{
		outputFloats1[ndx] = inputFloats[ndx] + inputFloats[ndx];
		outputFloats2[ndx] = -inputFloats[ndx];
	}

	const string assembly(
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %comp_main1 \"entrypoint1\" %id\n"
		"OpEntryPoint GLCompute %comp_main2 \"entrypoint2\" %id\n"
		// A module cannot have two OpEntryPoint instructions with the same Execution Model and the same Name string.
		"OpEntryPoint Vertex    %vert_main  \"entrypoint2\" %vert_builtins %vertexIndex %instanceIndex\n"
		"OpExecutionMode %comp_main1 LocalSize 1 1 1\n"
		"OpExecutionMode %comp_main2 LocalSize 1 1 1\n"

		"OpName %comp_main1              \"entrypoint1\"\n"
		"OpName %comp_main2              \"entrypoint2\"\n"
		"OpName %vert_main               \"entrypoint2\"\n"
		"OpName %id                      \"gl_GlobalInvocationID\"\n"
		"OpName %vert_builtin_st         \"gl_PerVertex\"\n"
		"OpName %vertexIndex             \"gl_VertexIndex\"\n"
		"OpName %instanceIndex           \"gl_InstanceIndex\"\n"
		"OpMemberName %vert_builtin_st 0 \"gl_Position\"\n"
		"OpMemberName %vert_builtin_st 1 \"gl_PointSize\"\n"
		"OpMemberName %vert_builtin_st 2 \"gl_ClipDistance\"\n"

		"OpDecorate %id                      BuiltIn GlobalInvocationId\n"
		"OpDecorate %vertexIndex             BuiltIn VertexIndex\n"
		"OpDecorate %instanceIndex           BuiltIn InstanceIndex\n"
		"OpDecorate %vert_builtin_st         Block\n"
		"OpMemberDecorate %vert_builtin_st 0 BuiltIn Position\n"
		"OpMemberDecorate %vert_builtin_st 1 BuiltIn PointSize\n"
		"OpMemberDecorate %vert_builtin_st 2 BuiltIn ClipDistance\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%zero       = OpConstant %i32 0\n"
		"%one        = OpConstant %u32 1\n"
		"%c_f32_1    = OpConstant %f32 1\n"

		"%i32inputptr         = OpTypePointer Input %i32\n"
		"%vec4                = OpTypeVector %f32 4\n"
		"%vec4ptr             = OpTypePointer Output %vec4\n"
		"%f32arr1             = OpTypeArray %f32 %one\n"
		"%vert_builtin_st     = OpTypeStruct %vec4 %f32 %f32arr1\n"
		"%vert_builtin_st_ptr = OpTypePointer Output %vert_builtin_st\n"
		"%vert_builtins       = OpVariable %vert_builtin_st_ptr Output\n"

		"%id         = OpVariable %uvec3ptr Input\n"
		"%vertexIndex = OpVariable %i32inputptr Input\n"
		"%instanceIndex = OpVariable %i32inputptr Input\n"
		"%c_vec4_1   = OpConstantComposite %vec4 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"

		// gl_Position = vec4(1.);
		"%vert_main  = OpFunction %void None %voidf\n"
		"%vert_entry = OpLabel\n"
		"%position   = OpAccessChain %vec4ptr %vert_builtins %zero\n"
		"              OpStore %position %c_vec4_1\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n"

		// Double inputs.
		"%comp_main1  = OpFunction %void None %voidf\n"
		"%comp1_entry = OpLabel\n"
		"%idval1      = OpLoad %uvec3 %id\n"
		"%x1          = OpCompositeExtract %u32 %idval1 0\n"
		"%inloc1      = OpAccessChain %f32ptr %indata %zero %x1\n"
		"%inval1      = OpLoad %f32 %inloc1\n"
		"%add         = OpFAdd %f32 %inval1 %inval1\n"
		"%outloc1     = OpAccessChain %f32ptr %outdata %zero %x1\n"
		"               OpStore %outloc1 %add\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n"

		// Negate inputs.
		"%comp_main2  = OpFunction %void None %voidf\n"
		"%comp2_entry = OpLabel\n"
		"%idval2      = OpLoad %uvec3 %id\n"
		"%x2          = OpCompositeExtract %u32 %idval2 0\n"
		"%inloc2      = OpAccessChain %f32ptr %indata %zero %x2\n"
		"%inval2      = OpLoad %f32 %inloc2\n"
		"%neg         = OpFNegate %f32 %inval2\n"
		"%outloc2     = OpAccessChain %f32ptr %outdata %zero %x2\n"
		"               OpStore %outloc2 %neg\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n");

	spec1.assembly = assembly;
	spec1.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec1.outputs.push_back(BufferSp(new Float32Buffer(outputFloats1)));
	spec1.numWorkGroups = IVec3(numElements, 1, 1);
	spec1.entryPoint = "entrypoint1";

	spec2.assembly = assembly;
	spec2.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
	spec2.outputs.push_back(BufferSp(new Float32Buffer(outputFloats2)));
	spec2.numWorkGroups = IVec3(numElements, 1, 1);
	spec2.entryPoint = "entrypoint2";

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "shader1", "multiple shaders in the same module", spec1));
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "shader2", "multiple shaders in the same module", spec2));

	return group.release();
}

inline std::string makeLongUTF8String (size_t num4ByteChars)
{
	// An example of a longest valid UTF-8 character.  Be explicit about the
	// character type because Microsoft compilers can otherwise interpret the
	// character string as being over wide (16-bit) characters. Ideally, we
	// would just use a C++11 UTF-8 string literal, but we want to support older
	// Microsoft compilers.
	const std::basic_string<char> earthAfrica("\xF0\x9F\x8C\x8D");
	std::string longString;
	longString.reserve(num4ByteChars * 4);
	for (size_t count = 0; count < num4ByteChars; count++)
	{
		longString += earthAfrica;
	}
	return longString;
}

tcu::TestCaseGroup* createOpSourceGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsource", "Tests the OpSource & OpSourceContinued instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"

		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"${SOURCE}\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("unknown_source",							"OpSource Unknown 0"));
	cases.push_back(CaseParameter("wrong_source",							"OpSource OpenCL_C 210"));
	cases.push_back(CaseParameter("normal_filename",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname"));
	cases.push_back(CaseParameter("empty_filename",							"%fname = OpString \"\"\n"
																			"OpSource GLSL 430 %fname"));
	cases.push_back(CaseParameter("normal_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\""));
	cases.push_back(CaseParameter("empty_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\""));
	cases.push_back(CaseParameter("long_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"" + makeLongUTF8String(65530) + "ccc\"")); // word count: 65535
	cases.push_back(CaseParameter("utf8_source_code",						"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\xE2\x98\x82\xE2\x98\x85\"")); // umbrella & black star symbol
	cases.push_back(CaseParameter("normal_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvo\"\n"
																			"OpSourceContinued \"id main() {}\""));
	cases.push_back(CaseParameter("empty_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"\""));
	cases.push_back(CaseParameter("long_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"" + makeLongUTF8String(65533) + "ccc\"")); // word count: 65535
	cases.push_back(CaseParameter("utf8_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\nvoid main() {}\"\n"
																			"OpSourceContinued \"\xE2\x98\x8E\xE2\x9A\x91\"")); // white telephone & black flag symbol
	cases.push_back(CaseParameter("multi_sourcecontinued",					"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"#version 430\n\"\n"
																			"OpSourceContinued \"void\"\n"
																			"OpSourceContinued \"main()\"\n"
																			"OpSourceContinued \"{}\""));
	cases.push_back(CaseParameter("empty_source_before_sourcecontinued",	"%fname = OpString \"filename\"\n"
																			"OpSource GLSL 430 %fname \"\"\n"
																			"OpSourceContinued \"#version 430\nvoid main() {}\""));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["SOURCE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createOpSourceExtensionGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opsourceextension", "Tests the OpSource instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSourceExtension \"${EXTENSION}\"\n"

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("empty_extension",	""));
	cases.push_back(CaseParameter("real_extension",		"GL_ARB_texture_rectangle"));
	cases.push_back(CaseParameter("fake_extension",		"GL_ARB_im_the_ultimate_extension"));
	cases.push_back(CaseParameter("utf8_extension",		"GL_ARB_\xE2\x98\x82\xE2\x98\x85"));
	cases.push_back(CaseParameter("long_extension",		makeLongUTF8String(65533) + "ccc")); // word count: 65535

	fillRandomScalars(rnd, -200.f, 200.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["EXTENSION"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that a compute shader can generate a constant null value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpConstantNullGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantnull", "Tests the OpConstantNull instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +
		"%uvec2     = OpTypeVector %u32 2\n"
		"%bvec3     = OpTypeVector %bool 3\n"
		"%fvec4     = OpTypeVector %f32 4\n"
		"%fmat33    = OpTypeMatrix %fvec3 3\n"
		"%const100  = OpConstant %u32 100\n"
		"%uarr100   = OpTypeArray %i32 %const100\n"
		"%struct    = OpTypeStruct %f32 %i32 %u32\n"
		"%pointer   = OpTypePointer Function %i32\n"
		+ string(getComputeAsmInputOutputBuffer()) +

		"%null      = OpConstantNull ${TYPE}\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("bool",			"%bool"));
	cases.push_back(CaseParameter("sint32",			"%i32"));
	cases.push_back(CaseParameter("uint32",			"%u32"));
	cases.push_back(CaseParameter("float32",		"%f32"));
	cases.push_back(CaseParameter("vec4float32",	"%fvec4"));
	cases.push_back(CaseParameter("vec3bool",		"%bvec3"));
	cases.push_back(CaseParameter("vec2uint32",		"%uvec2"));
	cases.push_back(CaseParameter("matrix",			"%fmat33"));
	cases.push_back(CaseParameter("array",			"%uarr100"));
	cases.push_back(CaseParameter("struct",			"%struct"));
	cases.push_back(CaseParameter("pointer",		"%pointer"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["TYPE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that a compute shader can generate a constant composite value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpConstantCompositeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantcomposite", "Tests the OpConstantComposite instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"${CONSTANT}\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("vector",			"%five = OpConstant %u32 5\n"
													"%const = OpConstantComposite %uvec3 %five %zero %five"));
	cases.push_back(CaseParameter("matrix",			"%m3fvec3 = OpTypeMatrix %fvec3 3\n"
													"%ten = OpConstant %f32 10.\n"
													"%fzero = OpConstant %f32 0.\n"
													"%vec = OpConstantComposite %fvec3 %ten %fzero %ten\n"
													"%mat = OpConstantComposite %m3fvec3 %vec %vec %vec"));
	cases.push_back(CaseParameter("struct",			"%m2vec3 = OpTypeMatrix %fvec3 2\n"
													"%struct = OpTypeStruct %i32 %f32 %fvec3 %m2vec3\n"
													"%fzero = OpConstant %f32 0.\n"
													"%one = OpConstant %f32 1.\n"
													"%point5 = OpConstant %f32 0.5\n"
													"%vec = OpConstantComposite %fvec3 %one %one %fzero\n"
													"%mat = OpConstantComposite %m2vec3 %vec %vec\n"
													"%const = OpConstantComposite %struct %zero %point5 %vec %mat"));
	cases.push_back(CaseParameter("nested_struct",	"%st1 = OpTypeStruct %u32 %f32\n"
													"%st2 = OpTypeStruct %i32 %i32\n"
													"%struct = OpTypeStruct %st1 %st2\n"
													"%point5 = OpConstant %f32 0.5\n"
													"%one = OpConstant %u32 1\n"
													"%ten = OpConstant %i32 10\n"
													"%st1val = OpConstantComposite %st1 %one %point5\n"
													"%st2val = OpConstantComposite %st2 %ten %ten\n"
													"%const = OpConstantComposite %struct %st1val %st2val"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONSTANT"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Creates a floating point number with the given exponent, and significand
// bits set. It can only create normalized numbers. Only the least significant
// 24 bits of the significand will be examined. The final bit of the
// significand will also be ignored. This allows alignment to be written
// similarly to C99 hex-floats.
// For example if you wanted to write 0x1.7f34p-12 you would call
// constructNormalizedFloat(-12, 0x7f3400)
float constructNormalizedFloat (deInt32 exponent, deUint32 significand)
{
	float f = 1.0f;

	for (deInt32 idx = 0; idx < 23; ++idx)
	{
		f += ((significand & 0x800000) == 0) ? 0.f : std::ldexp(1.0f, -(idx + 1));
		significand <<= 1;
	}

	return std::ldexp(f, exponent);
}

// Compare instruction for the OpQuantizeF16 compute exact case.
// Returns true if the output is what is expected from the test case.
bool compareOpQuantizeF16ComputeExactCase (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	// Only size is needed because we cannot compare Nans.
	size_t byteSize = expectedOutputs[0].getByteSize();

	const float*	outputAsFloat	= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	if (byteSize != 4*sizeof(float)) {
		return false;
	}

	if (*outputAsFloat != constructNormalizedFloat(8, 0x304000) &&
		*outputAsFloat != constructNormalizedFloat(8, 0x300000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != -constructNormalizedFloat(-7, 0x600000) &&
		*outputAsFloat != -constructNormalizedFloat(-7, 0x604000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != constructNormalizedFloat(2, 0x01C000) &&
		*outputAsFloat != constructNormalizedFloat(2, 0x020000)) {
		return false;
	}
	outputAsFloat++;

	if (*outputAsFloat != constructNormalizedFloat(1, 0xFFC000) &&
		*outputAsFloat != constructNormalizedFloat(2, 0x000000)) {
		return false;
	}

	return true;
}

// Checks that every output from a test-case is a float NaN.
bool compareNan (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	if (outputAllocs.size() != 1)
		return false;

	// Only size is needed because we cannot compare Nans.
	size_t byteSize = expectedOutputs[0].getByteSize();

	const float* const	output_as_float	= static_cast<const float*>(outputAllocs[0]->getHostPtr());

	for (size_t idx = 0; idx < byteSize / sizeof(float); ++idx)
	{
		if (!deFloatIsNaN(output_as_float[idx]))
		{
			return false;
		}
	}

	return true;
}

// Checks that a compute shader can generate a constant composite value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createOpQuantizeToF16Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opquantize", "Tests the OpQuantizeToF16 instruction"));

	const std::string shader (
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%quant     = OpQuantizeToF16 %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %quant\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	{
		ComputeShaderSpec	spec;
		const deUint32		numElements		= 100;
		vector<float>		infinities;
		vector<float>		results;

		infinities.reserve(numElements);
		results.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			switch(idx % 4)
			{
				case 0:
					infinities.push_back(std::numeric_limits<float>::infinity());
					results.push_back(std::numeric_limits<float>::infinity());
					break;
				case 1:
					infinities.push_back(-std::numeric_limits<float>::infinity());
					results.push_back(-std::numeric_limits<float>::infinity());
					break;
				case 2:
					infinities.push_back(std::ldexp(1.0f, 16));
					results.push_back(std::numeric_limits<float>::infinity());
					break;
				case 3:
					infinities.push_back(std::ldexp(-1.0f, 32));
					results.push_back(-std::numeric_limits<float>::infinity());
					break;
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(infinities)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(results)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "infinities", "Check that infinities propagated and created", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		nans;
		const deUint32		numElements		= 100;

		nans.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			if (idx % 2 == 0)
			{
				nans.push_back(std::numeric_limits<float>::quiet_NaN());
			}
			else
			{
				nans.push_back(-std::numeric_limits<float>::quiet_NaN());
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(nans)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(nans)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.verifyIO = &compareNan;

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "propagated_nans", "Check that nans are propagated", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		small;
		vector<float>		zeros;
		const deUint32		numElements		= 100;

		small.reserve(numElements);
		zeros.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
		{
			switch(idx % 6)
			{
				case 0:
					small.push_back(0.f);
					zeros.push_back(0.f);
					break;
				case 1:
					small.push_back(-0.f);
					zeros.push_back(-0.f);
					break;
				case 2:
					small.push_back(std::ldexp(1.0f, -16));
					zeros.push_back(0.f);
					break;
				case 3:
					small.push_back(std::ldexp(-1.0f, -32));
					zeros.push_back(-0.f);
					break;
				case 4:
					small.push_back(std::ldexp(1.0f, -127));
					zeros.push_back(0.f);
					break;
				case 5:
					small.push_back(-std::ldexp(1.0f, -128));
					zeros.push_back(-0.f);
					break;
			}
		}

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(small)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(zeros)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "flush_to_zero", "Check that values are zeroed correctly", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		exact;
		const deUint32		numElements		= 200;

		exact.reserve(numElements);

		for (size_t idx = 0; idx < numElements; ++idx)
			exact.push_back(static_cast<float>(static_cast<int>(idx) - 100));

		spec.assembly = shader;
		spec.inputs.push_back(BufferSp(new Float32Buffer(exact)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(exact)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "exact", "Check that values exactly preserved where appropriate", spec));
	}

	{
		ComputeShaderSpec	spec;
		vector<float>		inputs;
		const deUint32		numElements		= 4;

		inputs.push_back(constructNormalizedFloat(8,	0x300300));
		inputs.push_back(-constructNormalizedFloat(-7,	0x600800));
		inputs.push_back(constructNormalizedFloat(2,	0x01E000));
		inputs.push_back(constructNormalizedFloat(1,	0xFFE000));

		spec.assembly = shader;
		spec.verifyIO = &compareOpQuantizeF16ComputeExactCase;
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "rounded", "Check that are rounded when needed", spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createSpecConstantOpQuantizeToF16Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opspecconstantop_opquantize", "Tests the OpQuantizeToF16 opcode for the OpSpecConstantOp instruction"));

	const std::string shader (
		string(getComputeAsmShaderPreamble()) +

		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n"
		"OpDecorate %sc_3  SpecId 3\n"
		"OpDecorate %sc_4  SpecId 4\n"
		"OpDecorate %sc_5  SpecId 5\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%c_u32_6   = OpConstant %u32 6\n"

		"%sc_0      = OpSpecConstant %f32 0.\n"
		"%sc_1      = OpSpecConstant %f32 0.\n"
		"%sc_2      = OpSpecConstant %f32 0.\n"
		"%sc_3      = OpSpecConstant %f32 0.\n"
		"%sc_4      = OpSpecConstant %f32 0.\n"
		"%sc_5      = OpSpecConstant %f32 0.\n"

		"%sc_0_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_0\n"
		"%sc_1_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_1\n"
		"%sc_2_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_2\n"
		"%sc_3_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_3\n"
		"%sc_4_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_4\n"
		"%sc_5_quant = OpSpecConstantOp %f32 QuantizeToF16 %sc_5\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%selector  = OpUMod %u32 %x %c_u32_6\n"
		"            OpSelectionMerge %exit None\n"
		"            OpSwitch %selector %exit 0 %case0 1 %case1 2 %case2 3 %case3 4 %case4 5 %case5\n"

		"%case0     = OpLabel\n"
		"             OpStore %outloc %sc_0_quant\n"
		"             OpBranch %exit\n"

		"%case1     = OpLabel\n"
		"             OpStore %outloc %sc_1_quant\n"
		"             OpBranch %exit\n"

		"%case2     = OpLabel\n"
		"             OpStore %outloc %sc_2_quant\n"
		"             OpBranch %exit\n"

		"%case3     = OpLabel\n"
		"             OpStore %outloc %sc_3_quant\n"
		"             OpBranch %exit\n"

		"%case4     = OpLabel\n"
		"             OpStore %outloc %sc_4_quant\n"
		"             OpBranch %exit\n"

		"%case5     = OpLabel\n"
		"             OpStore %outloc %sc_5_quant\n"
		"             OpBranch %exit\n"

		"%exit      = OpLabel\n"
		"             OpReturn\n"

		"             OpFunctionEnd\n");

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 4;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::numeric_limits<float>::infinity()));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(-std::numeric_limits<float>::infinity()));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::ldexp(1.0f, 16)));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::ldexp(-1.0f, 32)));

		outputs.push_back(std::numeric_limits<float>::infinity());
		outputs.push_back(-std::numeric_limits<float>::infinity());
		outputs.push_back(std::numeric_limits<float>::infinity());
		outputs.push_back(-std::numeric_limits<float>::infinity());

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "infinities", "Check that infinities propagated and created", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 2;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);
		spec.verifyIO		= &compareNan;

		outputs.push_back(std::numeric_limits<float>::quiet_NaN());
		outputs.push_back(-std::numeric_limits<float>::quiet_NaN());

		for (deUint8 idx = 0; idx < numCases; ++idx)
			spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(outputs[idx]));

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "propagated_nans", "Check that nans are propagated", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 6;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(0.f));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(-0.f));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::ldexp(1.0f, -16)));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::ldexp(-1.0f, -32)));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(std::ldexp(1.0f, -127)));
		spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(-std::ldexp(1.0f, -128)));

		outputs.push_back(0.f);
		outputs.push_back(-0.f);
		outputs.push_back(0.f);
		outputs.push_back(-0.f);
		outputs.push_back(0.f);
		outputs.push_back(-0.f);

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "flush_to_zero", "Check that values are zeroed correctly", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 6;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);

		for (deUint8 idx = 0; idx < 6; ++idx)
		{
			const float f = static_cast<float>(idx * 10 - 30) / 4.f;
			spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(f));
			outputs.push_back(f);
		}

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "exact", "Check that values exactly preserved where appropriate", spec));
	}

	{
		ComputeShaderSpec	spec;
		const deUint8		numCases	= 4;
		vector<float>		inputs		(numCases, 0.f);
		vector<float>		outputs;

		spec.assembly		= shader;
		spec.numWorkGroups	= IVec3(numCases, 1, 1);
		spec.verifyIO		= &compareOpQuantizeF16ComputeExactCase;

		outputs.push_back(constructNormalizedFloat(8, 0x300300));
		outputs.push_back(-constructNormalizedFloat(-7, 0x600800));
		outputs.push_back(constructNormalizedFloat(2, 0x01E000));
		outputs.push_back(constructNormalizedFloat(1, 0xFFE000));

		for (deUint8 idx = 0; idx < numCases; ++idx)
			spec.specConstants.append<deInt32>(bitwiseCast<deUint32>(outputs[idx]));

		spec.inputs.push_back(BufferSp(new Float32Buffer(inputs)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputs)));

		group->addChild(new SpvAsmComputeShaderCase(
			testCtx, "rounded", "Check that are rounded when needed", spec));
	}

	return group.release();
}

// Checks that constant null/composite values can be used in computation.
tcu::TestCaseGroup* createOpConstantUsageGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantnullcomposite", "Spotcheck the OpConstantNull & OpConstantComposite instruction"));
	ComputeShaderSpec				spec;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	spec.assembly =
		"OpCapability Shader\n"
		"%std450 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +

		"%fmat      = OpTypeMatrix %fvec3 3\n"
		"%ten       = OpConstant %u32 10\n"
		"%f32arr10  = OpTypeArray %f32 %ten\n"
		"%fst       = OpTypeStruct %f32 %f32\n"

		+ string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		// Create a bunch of null values
		"%unull     = OpConstantNull %u32\n"
		"%fnull     = OpConstantNull %f32\n"
		"%vnull     = OpConstantNull %fvec3\n"
		"%mnull     = OpConstantNull %fmat\n"
		"%anull     = OpConstantNull %f32arr10\n"
		"%snull     = OpConstantComposite %fst %fnull %fnull\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"

		// Get the abs() of (a certain element of) those null values
		"%unull_cov = OpConvertUToF %f32 %unull\n"
		"%unull_abs = OpExtInst %f32 %std450 FAbs %unull_cov\n"
		"%fnull_abs = OpExtInst %f32 %std450 FAbs %fnull\n"
		"%vnull_0   = OpCompositeExtract %f32 %vnull 0\n"
		"%vnull_abs = OpExtInst %f32 %std450 FAbs %vnull_0\n"
		"%mnull_12  = OpCompositeExtract %f32 %mnull 1 2\n"
		"%mnull_abs = OpExtInst %f32 %std450 FAbs %mnull_12\n"
		"%anull_3   = OpCompositeExtract %f32 %anull 3\n"
		"%anull_abs = OpExtInst %f32 %std450 FAbs %anull_3\n"
		"%snull_1   = OpCompositeExtract %f32 %snull 1\n"
		"%snull_abs = OpExtInst %f32 %std450 FAbs %snull_1\n"

		// Add them all
		"%add1      = OpFAdd %f32 %neg  %unull_abs\n"
		"%add2      = OpFAdd %f32 %add1 %fnull_abs\n"
		"%add3      = OpFAdd %f32 %add2 %vnull_abs\n"
		"%add4      = OpFAdd %f32 %add3 %mnull_abs\n"
		"%add5      = OpFAdd %f32 %add4 %anull_abs\n"
		"%final     = OpFAdd %f32 %add5 %snull_abs\n"

		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %final\n" // write to output
		"             OpReturn\n"
		"             OpFunctionEnd\n";
	spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
	spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
	spec.numWorkGroups = IVec3(numElements, 1, 1);

	group->addChild(new SpvAsmComputeShaderCase(testCtx, "spotcheck", "Check that values constructed via OpConstantNull & OpConstantComposite can be used", spec));

	return group.release();
}

// Assembly code used for testing loop control is based on GLSL source code:
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x];
//   for (uint i = 0; i < 4; ++i)
//     output_data.elements[x] += 1.f;
// }
tcu::TestCaseGroup* createLoopControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "loop_control", "Tests loop control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%u32ptr      = OpTypePointer Function %u32\n"

		"%id          = OpVariable %uvec3ptr Input\n"
		"%zero        = OpConstant %i32 0\n"
		"%uzero       = OpConstant %u32 0\n"
		"%one         = OpConstant %i32 1\n"
		"%constf1     = OpConstant %f32 1.0\n"
		"%four        = OpConstant %u32 4\n"

		"%main        = OpFunction %void None %voidf\n"
		"%entry       = OpLabel\n"
		"%i           = OpVariable %u32ptr Function\n"
		"               OpStore %i %uzero\n"

		"%idval       = OpLoad %uvec3 %id\n"
		"%x           = OpCompositeExtract %u32 %idval 0\n"
		"%inloc       = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval       = OpLoad %f32 %inloc\n"
		"%outloc      = OpAccessChain %f32ptr %outdata %zero %x\n"
		"               OpStore %outloc %inval\n"
		"               OpBranch %loop_entry\n"

		"%loop_entry  = OpLabel\n"
		"%i_val       = OpLoad %u32 %i\n"
		"%cmp_lt      = OpULessThan %bool %i_val %four\n"
		"               OpLoopMerge %loop_merge %loop_body ${CONTROL}\n"
		"               OpBranchConditional %cmp_lt %loop_body %loop_merge\n"
		"%loop_body   = OpLabel\n"
		"%outval      = OpLoad %f32 %outloc\n"
		"%addf1       = OpFAdd %f32 %outval %constf1\n"
		"               OpStore %outloc %addf1\n"
		"%new_i       = OpIAdd %u32 %i_val %one\n"
		"               OpStore %i %new_i\n"
		"               OpBranch %loop_entry\n"
		"%loop_merge  = OpLabel\n"
		"               OpReturn\n"
		"               OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",				"None"));
	cases.push_back(CaseParameter("unroll",				"Unroll"));
	cases.push_back(CaseParameter("dont_unroll",		"DontUnroll"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 4.f;

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	group->addChild(new SpvAsmLoopControlDependencyLengthCase(testCtx, "dependency_length", "dependency_length"));
	group->addChild(new SpvAsmLoopControlDependencyInfiniteCase(testCtx, "dependency_infinite", "dependency_infinite"));

	return group.release();
}

// Assembly code used for testing selection control is based on GLSL source code:
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   float val = input_data.elements[x];
//   if (val > 10.f)
//     output_data.elements[x] = val + 1.f;
//   else
//     output_data.elements[x] = val - 1.f;
// }
tcu::TestCaseGroup* createSelectionControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "selection_control", "Tests selection control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id       = OpVariable %uvec3ptr Input\n"
		"%zero     = OpConstant %i32 0\n"
		"%constf1  = OpConstant %f32 1.0\n"
		"%constf10 = OpConstant %f32 10.0\n"

		"%main     = OpFunction %void None %voidf\n"
		"%entry    = OpLabel\n"
		"%idval    = OpLoad %uvec3 %id\n"
		"%x        = OpCompositeExtract %u32 %idval 0\n"
		"%inloc    = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval    = OpLoad %f32 %inloc\n"
		"%outloc   = OpAccessChain %f32ptr %outdata %zero %x\n"
		"%cmp_gt   = OpFOrdGreaterThan %bool %inval %constf10\n"

		"            OpSelectionMerge %if_end ${CONTROL}\n"
		"            OpBranchConditional %cmp_gt %if_true %if_false\n"
		"%if_true  = OpLabel\n"
		"%addf1    = OpFAdd %f32 %inval %constf1\n"
		"            OpStore %outloc %addf1\n"
		"            OpBranch %if_end\n"
		"%if_false = OpLabel\n"
		"%subf1    = OpFSub %f32 %inval %constf1\n"
		"            OpStore %outloc %subf1\n"
		"            OpBranch %if_end\n"
		"%if_end   = OpLabel\n"
		"            OpReturn\n"
		"            OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",					"None"));
	cases.push_back(CaseParameter("flatten",				"Flatten"));
	cases.push_back(CaseParameter("dont_flatten",			"DontFlatten"));
	cases.push_back(CaseParameter("flatten_dont_flatten",	"DontFlatten|Flatten"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + (inputFloats[ndx] > 10.f ? 1.f : -1.f);

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

void getOpNameAbuseCases (vector<CaseParameter> &abuseCases)
{
	// Generate a long name.
	std::string longname;
	longname.resize(65535, 'k'); // max string literal, spir-v 2.17

	// Some bad names, abusing utf-8 encoding. This may also cause problems
	// with the logs.
	// 1. Various illegal code points in utf-8
	std::string utf8illegal =
		"Illegal bytes in UTF-8: "
		"\xc0 \xc1 \xf5 \xf6 \xf7 \xf8 \xf9 \xfa \xfb \xfc \xfd \xfe \xff"
		"illegal surrogates: \xed\xad\xbf \xed\xbe\x80";

	// 2. Zero encoded as overlong, not exactly legal but often supported to differentiate from terminating zero
	std::string utf8nul = "UTF-8 encoded nul \xC0\x80 (should not end name)";

	// 3. Some overlong encodings
	std::string utf8overlong =
		"UTF-8 overlong \xF0\x82\x82\xAC \xfc\x83\xbf\xbf\xbf\xbf \xf8\x87\xbf\xbf\xbf "
		"\xf0\x8f\xbf\xbf";

	// 4. Internet "zalgo" meme "bleeding text"
	std::string utf8zalgo =
		"\x56\xcc\xb5\xcc\x85\xcc\x94\xcc\x88\xcd\x8a\xcc\x91\xcc\x88\xcd\x91\xcc\x83\xcd\x82"
		"\xcc\x83\xcd\x90\xcc\x8a\xcc\x92\xcc\x92\xcd\x8b\xcc\x94\xcd\x9d\xcc\x98\xcc\xab\xcc"
		"\xae\xcc\xa9\xcc\xad\xcc\x97\xcc\xb0\x75\xcc\xb6\xcc\xbe\xcc\x80\xcc\x82\xcc\x84\xcd"
		"\x84\xcc\x90\xcd\x86\xcc\x9a\xcd\x84\xcc\x9b\xcd\x86\xcd\x92\xcc\x9a\xcd\x99\xcd\x99"
		"\xcc\xbb\xcc\x98\xcd\x8e\xcd\x88\xcd\x9a\xcc\xa6\xcc\x9c\xcc\xab\xcc\x99\xcd\x94\xcd"
		"\x99\xcd\x95\xcc\xa5\xcc\xab\xcd\x89\x6c\xcc\xb8\xcc\x8e\xcc\x8b\xcc\x8b\xcc\x9a\xcc"
		"\x8e\xcd\x9d\xcc\x80\xcc\xa1\xcc\xad\xcd\x9c\xcc\xba\xcc\x96\xcc\xb3\xcc\xa2\xcd\x8e"
		"\xcc\xa2\xcd\x96\x6b\xcc\xb8\xcc\x84\xcd\x81\xcc\xbf\xcc\x8d\xcc\x89\xcc\x85\xcc\x92"
		"\xcc\x84\xcc\x90\xcd\x81\xcc\x93\xcd\x90\xcd\x92\xcd\x9d\xcc\x84\xcd\x98\xcd\x9d\xcd"
		"\xa0\xcd\x91\xcc\x94\xcc\xb9\xcd\x93\xcc\xa5\xcd\x87\xcc\xad\xcc\xa7\xcd\x96\xcd\x99"
		"\xcc\x9d\xcc\xbc\xcd\x96\xcd\x93\xcc\x9d\xcc\x99\xcc\xa8\xcc\xb1\xcd\x85\xcc\xba\xcc"
		"\xa7\x61\xcc\xb8\xcc\x8e\xcc\x81\xcd\x90\xcd\x84\xcd\x8c\xcc\x8c\xcc\x85\xcd\x86\xcc"
		"\x84\xcd\x84\xcc\x90\xcc\x84\xcc\x8d\xcd\x99\xcd\x8d\xcc\xb0\xcc\xa3\xcc\xa6\xcd\x89"
		"\xcd\x8d\xcd\x87\xcc\x98\xcd\x8d\xcc\xa4\xcd\x9a\xcd\x8e\xcc\xab\xcc\xb9\xcc\xac\xcc"
		"\xa2\xcd\x87\xcc\xa0\xcc\xb3\xcd\x89\xcc\xb9\xcc\xa7\xcc\xa6\xcd\x89\xcd\x95\x6e\xcc"
		"\xb8\xcd\x8a\xcc\x8a\xcd\x82\xcc\x9b\xcd\x81\xcd\x90\xcc\x85\xcc\x9b\xcd\x80\xcd\x91"
		"\xcd\x9b\xcc\x81\xcd\x81\xcc\x9a\xcc\xb3\xcd\x9c\xcc\x9e\xcc\x9d\xcd\x99\xcc\xa2\xcd"
		"\x93\xcd\x96\xcc\x97\xff";

	// General name abuses
	abuseCases.push_back(CaseParameter("_has_very_long_name", longname));
	abuseCases.push_back(CaseParameter("_utf8_illegal", utf8illegal));
	abuseCases.push_back(CaseParameter("_utf8_nul", utf8nul));
	abuseCases.push_back(CaseParameter("_utf8_overlong", utf8overlong));
	abuseCases.push_back(CaseParameter("_utf8_zalgo", utf8zalgo));

	// GL keywords
	abuseCases.push_back(CaseParameter("_is_gl_Position", "gl_Position"));
	abuseCases.push_back(CaseParameter("_is_gl_InstanceID", "gl_InstanceID"));
	abuseCases.push_back(CaseParameter("_is_gl_PrimitiveID", "gl_PrimitiveID"));
	abuseCases.push_back(CaseParameter("_is_gl_TessCoord", "gl_TessCoord"));
	abuseCases.push_back(CaseParameter("_is_gl_PerVertex", "gl_PerVertex"));
	abuseCases.push_back(CaseParameter("_is_gl_InvocationID", "gl_InvocationID"));
	abuseCases.push_back(CaseParameter("_is_gl_PointSize", "gl_PointSize"));
	abuseCases.push_back(CaseParameter("_is_gl_PointCoord", "gl_PointCoord"));
	abuseCases.push_back(CaseParameter("_is_gl_Layer", "gl_Layer"));
	abuseCases.push_back(CaseParameter("_is_gl_FragDepth", "gl_FragDepth"));
	abuseCases.push_back(CaseParameter("_is_gl_NumWorkGroups", "gl_NumWorkGroups"));
	abuseCases.push_back(CaseParameter("_is_gl_WorkGroupID", "gl_WorkGroupID"));
	abuseCases.push_back(CaseParameter("_is_gl_LocalInvocationID", "gl_LocalInvocationID"));
	abuseCases.push_back(CaseParameter("_is_gl_GlobalInvocationID", "gl_GlobalInvocationID"));
	abuseCases.push_back(CaseParameter("_is_gl_MaxVertexAttribs", "gl_MaxVertexAttribs"));
	abuseCases.push_back(CaseParameter("_is_gl_MaxViewports", "gl_MaxViewports"));
	abuseCases.push_back(CaseParameter("_is_gl_MaxComputeWorkGroupCount", "gl_MaxComputeWorkGroupCount"));
	abuseCases.push_back(CaseParameter("_is_mat3", "mat3"));
	abuseCases.push_back(CaseParameter("_is_volatile", "volatile"));
	abuseCases.push_back(CaseParameter("_is_inout", "inout"));
	abuseCases.push_back(CaseParameter("_is_isampler3d", "isampler3d"));
}

tcu::TestCaseGroup* createOpNameGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opname", "Tests OpName cases"));
	de::MovePtr<tcu::TestCaseGroup>	entryMainGroup	(new tcu::TestCaseGroup(testCtx, "entry_main", "OpName tests with entry main"));
	de::MovePtr<tcu::TestCaseGroup>	entryNotGroup	(new tcu::TestCaseGroup(testCtx, "entry_rdc", "OpName tests with entry rdc"));
	de::MovePtr<tcu::TestCaseGroup>	abuseGroup		(new tcu::TestCaseGroup(testCtx, "abuse", "OpName abuse tests"));
	vector<CaseParameter>			cases;
	vector<CaseParameter>			abuseCases;
	vector<string>					testFunc;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 128;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);

	getOpNameAbuseCases(abuseCases);

	fillRandomScalars(rnd, -100.0f, 100.0f, &inputFloats[0], numElements);

	for(size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	const string commonShaderHeader =
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n";

	const string commonShaderFooter =
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits())
		+ string(getComputeAsmCommonTypes())
		+ string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%func      = OpFunction %void None %voidf\n"
		"%5         = OpLabel\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n"

		"%main      = OpFunction %void None %voidf\n"
		"%entry     = OpLabel\n"
		"%7         = OpFunctionCall %void %func\n"

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"

		"             OpReturn\n"
		"             OpFunctionEnd\n";

	const StringTemplate shaderTemplate (
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"${ENTRY}\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpName %${ID} \"${NAME}\"\n" +
		commonShaderFooter);

	const std::string multipleNames =
		commonShaderHeader +
		"OpName %main \"to_be\"\n"
		"OpName %id   \"or_not\"\n"
		"OpName %main \"to_be\"\n"
		"OpName %main \"makes_no\"\n"
		"OpName %func \"difference\"\n"
		"OpName %5    \"to_me\"\n" +
		commonShaderFooter;

	{
		ComputeShaderSpec	spec;

		spec.assembly		= multipleNames;
		spec.numWorkGroups	= IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "main_has_multiple_names", "multiple_names", spec));
	}

	const std::string everythingNamed =
		commonShaderHeader +
		"OpName %main   \"name1\"\n"
		"OpName %id     \"name2\"\n"
		"OpName %zero   \"name3\"\n"
		"OpName %entry  \"name4\"\n"
		"OpName %func   \"name5\"\n"
		"OpName %5      \"name6\"\n"
		"OpName %7      \"name7\"\n"
		"OpName %idval  \"name8\"\n"
		"OpName %inloc  \"name9\"\n"
		"OpName %inval  \"name10\"\n"
		"OpName %neg    \"name11\"\n"
		"OpName %outloc \"name12\"\n"+
		commonShaderFooter;
	{
		ComputeShaderSpec	spec;

		spec.assembly		= everythingNamed;
		spec.numWorkGroups	= IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "everything_named", "everything_named", spec));
	}

	const std::string everythingNamedTheSame =
		commonShaderHeader +
		"OpName %main   \"the_same\"\n"
		"OpName %id     \"the_same\"\n"
		"OpName %zero   \"the_same\"\n"
		"OpName %entry  \"the_same\"\n"
		"OpName %func   \"the_same\"\n"
		"OpName %5      \"the_same\"\n"
		"OpName %7      \"the_same\"\n"
		"OpName %idval  \"the_same\"\n"
		"OpName %inloc  \"the_same\"\n"
		"OpName %inval  \"the_same\"\n"
		"OpName %neg    \"the_same\"\n"
		"OpName %outloc \"the_same\"\n"+
		commonShaderFooter;
	{
		ComputeShaderSpec	spec;

		spec.assembly		= everythingNamedTheSame;
		spec.numWorkGroups	= IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "everything_named_the_same", "everything_named_the_same", spec));
	}

	// main_is_...
	for (size_t ndx = 0; ndx < abuseCases.size(); ++ndx)
	{
		map<string, string>	specializations;
		ComputeShaderSpec	spec;

		specializations["ENTRY"]	= "main";
		specializations["ID"]		= "main";
		specializations["NAME"]		= abuseCases[ndx].param;
		spec.assembly				= shaderTemplate.specialize(specializations);
		spec.numWorkGroups			= IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, (std::string("main") + abuseCases[ndx].name).c_str(), abuseCases[ndx].name, spec));
	}

	// x_is_....
	for (size_t ndx = 0; ndx < abuseCases.size(); ++ndx)
	{
		map<string, string>	specializations;
		ComputeShaderSpec	spec;

		specializations["ENTRY"]	= "main";
		specializations["ID"]		= "x";
		specializations["NAME"]		= abuseCases[ndx].param;
		spec.assembly				= shaderTemplate.specialize(specializations);
		spec.numWorkGroups			= IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, (std::string("x") + abuseCases[ndx].name).c_str(), abuseCases[ndx].name, spec));
	}

	cases.push_back(CaseParameter("_is_main", "main"));
	cases.push_back(CaseParameter("_is_not_main", "not_main"));
	testFunc.push_back("main");
	testFunc.push_back("func");

	for(size_t fNdx = 0; fNdx < testFunc.size(); ++fNdx)
	{
		for(size_t ndx = 0; ndx < cases.size(); ++ndx)
		{
			map<string, string>	specializations;
			ComputeShaderSpec	spec;

			specializations["ENTRY"]	= "main";
			specializations["ID"]		= testFunc[fNdx];
			specializations["NAME"]		= cases[ndx].param;
			spec.assembly				= shaderTemplate.specialize(specializations);
			spec.numWorkGroups			= IVec3(numElements, 1, 1);
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

			entryMainGroup->addChild(new SpvAsmComputeShaderCase(testCtx, (testFunc[fNdx] + cases[ndx].name).c_str(), cases[ndx].name, spec));
		}
	}

	cases.push_back(CaseParameter("_is_entry", "rdc"));

	for(size_t fNdx = 0; fNdx < testFunc.size(); ++fNdx)
	{
		for(size_t ndx = 0; ndx < cases.size(); ++ndx)
		{
			map<string, string>     specializations;
			ComputeShaderSpec       spec;

			specializations["ENTRY"]	= "rdc";
			specializations["ID"]		= testFunc[fNdx];
			specializations["NAME"]		= cases[ndx].param;
			spec.assembly				= shaderTemplate.specialize(specializations);
			spec.numWorkGroups			= IVec3(numElements, 1, 1);
			spec.entryPoint				= "rdc";
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

			entryNotGroup->addChild(new SpvAsmComputeShaderCase(testCtx, (testFunc[fNdx] + cases[ndx].name).c_str(), cases[ndx].name, spec));
		}
	}

	group->addChild(entryMainGroup.release());
	group->addChild(entryNotGroup.release());
	group->addChild(abuseGroup.release());

	return group.release();
}

tcu::TestCaseGroup* createOpMemberNameGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "opmembername", "Tests OpMemberName cases"));
	de::MovePtr<tcu::TestCaseGroup>	abuseGroup(new tcu::TestCaseGroup(testCtx, "abuse", "OpMemberName abuse tests"));
	vector<CaseParameter>			abuseCases;
	vector<string>					testFunc;
	de::Random						rnd(deStringHash(group->getName()));
	const int						numElements = 128;
	vector<float>					inputFloats(numElements, 0);
	vector<float>					outputFloats(numElements, 0);

	getOpNameAbuseCases(abuseCases);

	fillRandomScalars(rnd, -100.0f, 100.0f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = -inputFloats[ndx];

	const string commonShaderHeader =
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n";

	const string commonShaderFooter =
		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits())
		+ string(getComputeAsmCommonTypes())
		+ string(getComputeAsmInputOutputBuffer()) +

		"%u3str     = OpTypeStruct %u32 %u32 %u32\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%entry     = OpLabel\n"

		"%idval     = OpLoad %uvec3 %id\n"
		"%x0        = OpCompositeExtract %u32 %idval 0\n"

		"%idstr     = OpCompositeConstruct %u3str %x0 %x0 %x0\n"
		"%x         = OpCompositeExtract %u32 %idstr 0\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"

		"             OpReturn\n"
		"             OpFunctionEnd\n";

	const StringTemplate shaderTemplate(
		commonShaderHeader +
		"OpMemberName %u3str 0 \"${NAME}\"\n" +
		commonShaderFooter);

	const std::string multipleNames =
		commonShaderHeader +
		"OpMemberName %u3str 0 \"to_be\"\n"
		"OpMemberName %u3str 1 \"or_not\"\n"
		"OpMemberName %u3str 0 \"to_be\"\n"
		"OpMemberName %u3str 2 \"makes_no\"\n"
		"OpMemberName %u3str 0 \"difference\"\n"
		"OpMemberName %u3str 0 \"to_me\"\n" +
		commonShaderFooter;
	{
		ComputeShaderSpec	spec;

		spec.assembly = multipleNames;
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "u3str_x_has_multiple_names", "multiple_names", spec));
	}

	const std::string everythingNamedTheSame =
		commonShaderHeader +
		"OpMemberName %u3str 0 \"the_same\"\n"
		"OpMemberName %u3str 1 \"the_same\"\n"
		"OpMemberName %u3str 2 \"the_same\"\n" +
		commonShaderFooter;

	{
		ComputeShaderSpec	spec;

		spec.assembly = everythingNamedTheSame;
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "everything_named_the_same", "everything_named_the_same", spec));
	}

	// u3str_x_is_....
	for (size_t ndx = 0; ndx < abuseCases.size(); ++ndx)
	{
		map<string, string>	specializations;
		ComputeShaderSpec	spec;

		specializations["NAME"] = abuseCases[ndx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));

		abuseGroup->addChild(new SpvAsmComputeShaderCase(testCtx, (std::string("u3str_x") + abuseCases[ndx].name).c_str(), abuseCases[ndx].name, spec));
	}

	group->addChild(abuseGroup.release());

	return group.release();
}

// Assembly code used for testing function control is based on GLSL source code:
//
// #version 430
//
// layout(std140, set = 0, binding = 0) readonly buffer Input {
//   float elements[];
// } input_data;
// layout(std140, set = 0, binding = 1) writeonly buffer Output {
//   float elements[];
// } output_data;
//
// float const10() { return 10.f; }
//
// void main() {
//   uint x = gl_GlobalInvocationID.x;
//   output_data.elements[x] = input_data.elements[x] + const10();
// }
tcu::TestCaseGroup* createFunctionControlGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "function_control", "Tests function control cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main \"main\"\n"
		"OpName %func_const10 \"const10(\"\n"
		"OpName %id \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%f32f = OpTypeFunction %f32\n"
		"%id = OpVariable %uvec3ptr Input\n"
		"%zero = OpConstant %i32 0\n"
		"%constf10 = OpConstant %f32 10.0\n"

		"%main         = OpFunction %void None %voidf\n"
		"%entry        = OpLabel\n"
		"%idval        = OpLoad %uvec3 %id\n"
		"%x            = OpCompositeExtract %u32 %idval 0\n"
		"%inloc        = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval        = OpLoad %f32 %inloc\n"
		"%ret_10       = OpFunctionCall %f32 %func_const10\n"
		"%fadd         = OpFAdd %f32 %inval %ret_10\n"
		"%outloc       = OpAccessChain %f32ptr %outdata %zero %x\n"
		"                OpStore %outloc %fadd\n"
		"                OpReturn\n"
		"                OpFunctionEnd\n"

		"%func_const10 = OpFunction %f32 ${CONTROL} %f32f\n"
		"%label        = OpLabel\n"
		"                OpReturnValue %constf10\n"
		"                OpFunctionEnd\n");

	cases.push_back(CaseParameter("none",						"None"));
	cases.push_back(CaseParameter("inline",						"Inline"));
	cases.push_back(CaseParameter("dont_inline",				"DontInline"));
	cases.push_back(CaseParameter("pure",						"Pure"));
	cases.push_back(CaseParameter("const",						"Const"));
	cases.push_back(CaseParameter("inline_pure",				"Inline|Pure"));
	cases.push_back(CaseParameter("const_dont_inline",			"Const|DontInline"));
	cases.push_back(CaseParameter("inline_dont_inline",			"Inline|DontInline"));
	cases.push_back(CaseParameter("pure_inline_dont_inline",	"Pure|Inline|DontInline"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	// CPU might not use the same rounding mode as the GPU. Use whole numbers to avoid rounding differences.
	floorAll(inputFloats);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + 10.f;

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONTROL"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createMemoryAccessGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "memory_access", "Tests memory access cases"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					inputFloats		(numElements, 0);
	vector<float>					outputFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%f32ptr_f  = OpTypePointer Function %f32\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%four      = OpConstant %i32 4\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%copy      = OpVariable %f32ptr_f Function\n"
		"%idval     = OpLoad %uvec3 %id ${ACCESS}\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata  %zero %x\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpCopyMemory %copy %inloc ${ACCESS}\n"
		"%val1      = OpLoad %f32 %copy\n"
		"%val2      = OpLoad %f32 %inloc\n"
		"%add       = OpFAdd %f32 %val1 %val2\n"
		"             OpStore %outloc %add ${ACCESS}\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("null",					""));
	cases.push_back(CaseParameter("none",					"None"));
	cases.push_back(CaseParameter("volatile",				"Volatile"));
	cases.push_back(CaseParameter("aligned",				"Aligned 4"));
	cases.push_back(CaseParameter("nontemporal",			"Nontemporal"));
	cases.push_back(CaseParameter("aligned_nontemporal",	"Aligned|Nontemporal 4"));
	cases.push_back(CaseParameter("aligned_volatile",		"Volatile|Aligned 4"));

	fillRandomScalars(rnd, -100.f, 100.f, &inputFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		outputFloats[ndx] = inputFloats[ndx] + inputFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["ACCESS"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

// Checks that we can get undefined values for various types, without exercising a computation with it.
tcu::TestCaseGroup* createOpUndefGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opundef", "Tests the OpUndef instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		string(getComputeAsmShaderPreamble()) +

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) +
		"%uvec2     = OpTypeVector %u32 2\n"
		"%fvec4     = OpTypeVector %f32 4\n"
		"%fmat33    = OpTypeMatrix %fvec3 3\n"
		"%image     = OpTypeImage %f32 2D 0 0 0 1 Unknown\n"
		"%sampler   = OpTypeSampler\n"
		"%simage    = OpTypeSampledImage %image\n"
		"%const100  = OpConstant %u32 100\n"
		"%uarr100   = OpTypeArray %i32 %const100\n"
		"%struct    = OpTypeStruct %f32 %i32 %u32\n"
		"%pointer   = OpTypePointer Function %i32\n"
		+ string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"

		"%undef     = OpUndef ${TYPE}\n"

		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");

	cases.push_back(CaseParameter("bool",			"%bool"));
	cases.push_back(CaseParameter("sint32",			"%i32"));
	cases.push_back(CaseParameter("uint32",			"%u32"));
	cases.push_back(CaseParameter("float32",		"%f32"));
	cases.push_back(CaseParameter("vec4float32",	"%fvec4"));
	cases.push_back(CaseParameter("vec2uint32",		"%uvec2"));
	cases.push_back(CaseParameter("matrix",			"%fmat33"));
	cases.push_back(CaseParameter("image",			"%image"));
	cases.push_back(CaseParameter("sampler",		"%sampler"));
	cases.push_back(CaseParameter("sampledimage",	"%simage"));
	cases.push_back(CaseParameter("array",			"%uarr100"));
	cases.push_back(CaseParameter("runtimearray",	"%f32arr"));
	cases.push_back(CaseParameter("struct",			"%struct"));
	cases.push_back(CaseParameter("pointer",		"%pointer"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["TYPE"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	// OpUndef with constants.
	{
		static const char data_dir[] = "spirv_assembly/instruction/compute/undef";

		static const struct
		{
			const std::string name;
			const std::string desc;
		} amberCases[] =
		{
			{ "undefined_constant_composite",		"OpUndef value in OpConstantComposite"		},
			{ "undefined_spec_constant_composite",	"OpUndef value in OpSpecConstantComposite"	},
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(amberCases); ++i)
		{
			cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx,
																				amberCases[i].name.c_str(),
																				amberCases[i].desc.c_str(),
																				data_dir,
																				amberCases[i].name + ".amber");
			group->addChild(testCase);
		}
	}

	return group.release();
}

// Checks that a compute shader can generate a constant composite value of various types, without exercising a computation on it.
tcu::TestCaseGroup* createFloat16OpConstantCompositeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opconstantcomposite", "Tests the OpConstantComposite instruction"));
	vector<CaseParameter>			cases;
	de::Random						rnd				(deStringHash(group->getName()));
	const int						numElements		= 100;
	vector<float>					positiveFloats	(numElements, 0);
	vector<float>					negativeFloats	(numElements, 0);
	const StringTemplate			shaderTemplate	(
		"OpCapability Shader\n"
		"OpCapability Float16\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"
		"%f16       = OpTypeFloat 16\n"
		"%c_f16_0   = OpConstant %f16 0.0\n"
		"%c_f16_0_5 = OpConstant %f16 0.5\n"
		"%c_f16_1   = OpConstant %f16 1.0\n"
		"%v2f16     = OpTypeVector %f16 2\n"
		"%v3f16     = OpTypeVector %f16 3\n"
		"%v4f16     = OpTypeVector %f16 4\n"

		"${CONSTANT}\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %zero %x\n"
		"%inval     = OpLoad %f32 %inloc\n"
		"%neg       = OpFNegate %f32 %inval\n"
		"%outloc    = OpAccessChain %f32ptr %outdata %zero %x\n"
		"             OpStore %outloc %neg\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n");


	cases.push_back(CaseParameter("vector",			"%const = OpConstantComposite %v3f16 %c_f16_0 %c_f16_0_5 %c_f16_1\n"));
	cases.push_back(CaseParameter("matrix",			"%m3v3f16 = OpTypeMatrix %v3f16 3\n"
													"%vec = OpConstantComposite %v3f16 %c_f16_0 %c_f16_0_5 %c_f16_1\n"
													"%mat = OpConstantComposite %m3v3f16 %vec %vec %vec"));
	cases.push_back(CaseParameter("struct",			"%m2v3f16 = OpTypeMatrix %v3f16 2\n"
													"%struct = OpTypeStruct %i32 %f16 %v3f16 %m2v3f16\n"
													"%vec = OpConstantComposite %v3f16 %c_f16_0 %c_f16_0_5 %c_f16_1\n"
													"%mat = OpConstantComposite %m2v3f16 %vec %vec\n"
													"%const = OpConstantComposite %struct %zero %c_f16_0_5 %vec %mat\n"));
	cases.push_back(CaseParameter("nested_struct",	"%st1 = OpTypeStruct %i32 %f16\n"
													"%st2 = OpTypeStruct %i32 %i32\n"
													"%struct = OpTypeStruct %st1 %st2\n"
													"%st1val = OpConstantComposite %st1 %zero %c_f16_0_5\n"
													"%st2val = OpConstantComposite %st2 %zero %zero\n"
													"%const = OpConstantComposite %struct %st1val %st2val"));

	fillRandomScalars(rnd, 1.f, 100.f, &positiveFloats[0], numElements);

	for (size_t ndx = 0; ndx < numElements; ++ndx)
		negativeFloats[ndx] = -positiveFloats[ndx];

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>		specializations;
		ComputeShaderSpec		spec;

		specializations["CONSTANT"] = cases[caseNdx].param;
		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(positiveFloats)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(negativeFloats)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);

		spec.extensions.push_back("VK_KHR_shader_float16_int8");

		spec.requestedVulkanFeatures.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;

		group->addChild(new SpvAsmComputeShaderCase(testCtx, cases[caseNdx].name, cases[caseNdx].name, spec));
	}

	return group.release();
}

const vector<deFloat16> squarize(const vector<deFloat16>& inData, const deUint32 argNo)
{
	const size_t		inDataLength	= inData.size();
	vector<deFloat16>	result;

	result.reserve(inDataLength * inDataLength);

	if (argNo == 0)
	{
		for (size_t numIdx = 0; numIdx < inDataLength; ++numIdx)
			result.insert(result.end(), inData.begin(), inData.end());
	}

	if (argNo == 1)
	{
		for (size_t numIdx = 0; numIdx < inDataLength; ++numIdx)
		{
			const vector<deFloat16>	tmp(inDataLength, inData[numIdx]);

			result.insert(result.end(), tmp.begin(), tmp.end());
		}
	}

	return result;
}

const vector<deFloat16> squarizeVector(const vector<deFloat16>& inData, const deUint32 argNo)
{
	vector<deFloat16>	vec;
	vector<deFloat16>	result;

	// Create vectors. vec will contain each possible pair from inData
	{
		const size_t	inDataLength	= inData.size();

		DE_ASSERT(inDataLength <= 64);

		vec.reserve(2 * inDataLength * inDataLength);

		for (size_t numIdxX = 0; numIdxX < inDataLength; ++numIdxX)
		for (size_t numIdxY = 0; numIdxY < inDataLength; ++numIdxY)
		{
			vec.push_back(inData[numIdxX]);
			vec.push_back(inData[numIdxY]);
		}
	}

	// Create vector pairs. result will contain each possible pair from vec
	{
		const size_t	coordsPerVector	= 2;
		const size_t	vectorsCount	= vec.size() / coordsPerVector;

		result.reserve(coordsPerVector * vectorsCount * vectorsCount);

		if (argNo == 0)
		{
			for (size_t numIdxX = 0; numIdxX < vectorsCount; ++numIdxX)
			for (size_t numIdxY = 0; numIdxY < vectorsCount; ++numIdxY)
			{
				for (size_t coordNdx = 0; coordNdx < coordsPerVector; ++coordNdx)
					result.push_back(vec[coordsPerVector * numIdxY + coordNdx]);
			}
		}

		if (argNo == 1)
		{
			for (size_t numIdxX = 0; numIdxX < vectorsCount; ++numIdxX)
			for (size_t numIdxY = 0; numIdxY < vectorsCount; ++numIdxY)
			{
				for (size_t coordNdx = 0; coordNdx < coordsPerVector; ++coordNdx)
					result.push_back(vec[coordsPerVector * numIdxX + coordNdx]);
			}
		}
	}

	return result;
}

struct fp16isNan			{ bool operator()(const tcu::Float16 in1, const tcu::Float16)		{ return in1.isNaN(); } };
struct fp16isInf			{ bool operator()(const tcu::Float16 in1, const tcu::Float16)		{ return in1.isInf(); } };
struct fp16isEqual			{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() == in2.asFloat(); } };
struct fp16isUnequal		{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() != in2.asFloat(); } };
struct fp16isLess			{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() <  in2.asFloat(); } };
struct fp16isGreater		{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() >  in2.asFloat(); } };
struct fp16isLessOrEqual	{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() <= in2.asFloat(); } };
struct fp16isGreaterOrEqual	{ bool operator()(const tcu::Float16 in1, const tcu::Float16 in2)	{ return in1.asFloat() >= in2.asFloat(); } };

template <class TestedLogicalFunction, bool onlyTestFunc, bool unationModeAnd, bool nanSupported>
bool compareFP16Logical (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	if (inputs.size() != 2 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;
	vector<deUint8>	input2Bytes;

	inputs[0].getBytes(input1Bytes);
	inputs[1].getBytes(input2Bytes);

	const deUint32			denormModesCount			= 2;
	const deFloat16			float16one					= tcu::Float16(1.0f).bits();
	const deFloat16			float16zero					= tcu::Float16(0.0f).bits();
	const tcu::Float16		zero						= tcu::Float16::zero(1);
	const deFloat16* const	outputAsFP16				= static_cast<deFloat16*>(outputAllocs[0]->getHostPtr());
	const deFloat16* const	input1AsFP16				= reinterpret_cast<deFloat16* const>(&input1Bytes.front());
	const deFloat16* const	input2AsFP16				= reinterpret_cast<deFloat16* const>(&input2Bytes.front());
	deUint32				successfulRuns				= denormModesCount;
	std::string				results[denormModesCount];
	TestedLogicalFunction	testedLogicalFunction;

	for (deUint32 denormMode = 0; denormMode < denormModesCount; denormMode++)
	{
		const bool flushToZero = (denormMode == 1);

		for (size_t idx = 0; idx < input1Bytes.size() / sizeof(deFloat16); ++idx)
		{
			const tcu::Float16	f1pre			= tcu::Float16(input1AsFP16[idx]);
			const tcu::Float16	f2pre			= tcu::Float16(input2AsFP16[idx]);
			const tcu::Float16	f1				= (flushToZero && f1pre.isDenorm()) ? zero : f1pre;
			const tcu::Float16	f2				= (flushToZero && f2pre.isDenorm()) ? zero : f2pre;
			deFloat16			expectedOutput	= float16zero;

			if (onlyTestFunc)
			{
				if (testedLogicalFunction(f1, f2))
					expectedOutput = float16one;
			}
			else
			{
				const bool	f1nan	= f1.isNaN();
				const bool	f2nan	= f2.isNaN();

				// Skip NaN floats if not supported by implementation
				if (!nanSupported && (f1nan || f2nan))
					continue;

				if (unationModeAnd)
				{
					const bool	ordered		= !f1nan && !f2nan;

					if (ordered && testedLogicalFunction(f1, f2))
						expectedOutput = float16one;
				}
				else
				{
					const bool	unordered	= f1nan || f2nan;

					if (unordered || testedLogicalFunction(f1, f2))
						expectedOutput = float16one;
				}
			}

			if (outputAsFP16[idx] != expectedOutput)
			{
				std::ostringstream str;

				str << "ERROR: Sub-case #" << idx
					<< " flushToZero:" << flushToZero
					<< std::hex
					<< " failed, inputs: 0x" << f1.bits()
					<< ";0x" << f2.bits()
					<< " output: 0x" << outputAsFP16[idx]
					<< " expected output: 0x" << expectedOutput;

				results[denormMode] = str.str();

				successfulRuns--;

				break;
			}
		}
	}

	if (successfulRuns == 0)
		for (deUint32 denormMode = 0; denormMode < denormModesCount; denormMode++)
			log << TestLog::Message << results[denormMode] << TestLog::EndMessage;

	return successfulRuns > 0;
}

} // anonymous

tcu::TestCaseGroup* createOpSourceTests (tcu::TestContext& testCtx)
{
	struct NameCodePair { string name, code; };
	RGBA							defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup> opSourceTests			(new tcu::TestCaseGroup(testCtx, "opsource", "OpSource instruction"));
	const std::string				opsourceGLSLWithFile	= "%opsrcfile = OpString \"foo.vert\"\nOpSource GLSL 450 %opsrcfile ";
	map<string, string>				fragments				= passthruFragments();
	const NameCodePair				tests[]					=
	{
		{"unknown", "OpSource Unknown 321"},
		{"essl", "OpSource ESSL 310"},
		{"glsl", "OpSource GLSL 450"},
		{"opencl_cpp", "OpSource OpenCL_CPP 120"},
		{"opencl_c", "OpSource OpenCL_C 120"},
		{"multiple", "OpSource GLSL 450\nOpSource GLSL 450"},
		{"file", opsourceGLSLWithFile},
		{"source", opsourceGLSLWithFile + "\"void main(){}\""},
		// Longest possible source string: SPIR-V limits instructions to 65535
		// words, of which the first 4 are opsourceGLSLWithFile; the rest will
		// contain 65530 UTF8 characters (one word each) plus one last word
		// containing 3 ASCII characters and \0.
		{"longsource", opsourceGLSLWithFile + '"' + makeLongUTF8String(65530) + "ccc" + '"'}
	};

	getDefaultColors(defaultColors);
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["debug"] = tests[testNdx].code;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opSourceTests.get());
	}

	return opSourceTests.release();
}

tcu::TestCaseGroup* createOpSourceContinuedTests (tcu::TestContext& testCtx)
{
	struct NameCodePair { string name, code; };
	RGBA								defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>		opSourceTests		(new tcu::TestCaseGroup(testCtx, "opsourcecontinued", "OpSourceContinued instruction"));
	map<string, string>					fragments			= passthruFragments();
	const std::string					opsource			= "%opsrcfile = OpString \"foo.vert\"\nOpSource GLSL 450 %opsrcfile \"void main(){}\"\n";
	const NameCodePair					tests[]				=
	{
		{"empty", opsource + "OpSourceContinued \"\""},
		{"short", opsource + "OpSourceContinued \"abcde\""},
		{"multiple", opsource + "OpSourceContinued \"abcde\"\nOpSourceContinued \"fghij\""},
		// Longest possible source string: SPIR-V limits instructions to 65535
		// words, of which the first one is OpSourceContinued/length; the rest
		// will contain 65533 UTF8 characters (one word each) plus one last word
		// containing 3 ASCII characters and \0.
		{"long", opsource + "OpSourceContinued \"" + makeLongUTF8String(65533) + "ccc\""}
	};

	getDefaultColors(defaultColors);
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["debug"] = tests[testNdx].code;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opSourceTests.get());
	}

	return opSourceTests.release();
}
tcu::TestCaseGroup* createOpNoLineTests(tcu::TestContext& testCtx)
{
	RGBA								 defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>		 opLineTests		 (new tcu::TestCaseGroup(testCtx, "opnoline", "OpNoLine instruction"));
	map<string, string>					 fragments;
	getDefaultColors(defaultColors);
	fragments["debug"]			=
		"%name = OpString \"name\"\n";

	fragments["pre_main"]	=
		"OpNoLine\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpLine %name 1 1\n"
		"%second_function = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"OpLine %name 1 1\n"
		"%second_param1 = OpFunctionParameter %v4f32\n"
		"OpNoLine\n"
		"OpNoLine\n"
		"%label_secondfunction = OpLabel\n"
		"OpNoLine\n"
		"OpReturnValue %second_param1\n"
		"OpFunctionEnd\n"
		"OpNoLine\n"
		"OpNoLine\n";

	fragments["testfun"]		=
		// A %test_code function that returns its argument unchanged.
		"OpNoLine\n"
		"OpNoLine\n"
		"OpLine %name 1 1\n"
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"OpNoLine\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"OpNoLine\n"
		"OpNoLine\n"
		"%label_testfun = OpLabel\n"
		"OpNoLine\n"
		"%val1 = OpFunctionCall %v4f32 %second_function %param1\n"
		"OpReturnValue %val1\n"
		"OpFunctionEnd\n"
		"OpLine %name 1 1\n"
		"OpNoLine\n";

	createTestsForAllStages("opnoline", defaultColors, defaultColors, fragments, opLineTests.get());

	return opLineTests.release();
}

tcu::TestCaseGroup* createOpModuleProcessedTests(tcu::TestContext& testCtx)
{
	RGBA								defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>		opModuleProcessedTests			(new tcu::TestCaseGroup(testCtx, "opmoduleprocessed", "OpModuleProcessed instruction"));
	map<string, string>					fragments;
	std::vector<std::string>			noExtensions;
	GraphicsResources					resources;

	getDefaultColors(defaultColors);
	resources.verifyBinary = veryfiBinaryShader;
	resources.spirvVersion = SPIRV_VERSION_1_3;

	fragments["moduleprocessed"]							=
		"OpModuleProcessed \"VULKAN CTS\"\n"
		"OpModuleProcessed \"Negative values\"\n"
		"OpModuleProcessed \"Date: 2017/09/21\"\n";

	fragments["pre_main"]	=
		"%second_function = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%second_param1 = OpFunctionParameter %v4f32\n"
		"%label_secondfunction = OpLabel\n"
		"OpReturnValue %second_param1\n"
		"OpFunctionEnd\n";

	fragments["testfun"]		=
		// A %test_code function that returns its argument unchanged.
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%val1 = OpFunctionCall %v4f32 %second_function %param1\n"
		"OpReturnValue %val1\n"
		"OpFunctionEnd\n";

	createTestsForAllStages ("opmoduleprocessed", defaultColors, defaultColors, fragments, resources, noExtensions, opModuleProcessedTests.get());

	return opModuleProcessedTests.release();
}


tcu::TestCaseGroup* createOpLineTests(tcu::TestContext& testCtx)
{
	RGBA													defaultColors[4];
	de::MovePtr<tcu::TestCaseGroup>							opLineTests			(new tcu::TestCaseGroup(testCtx, "opline", "OpLine instruction"));
	map<string, string>										fragments;
	std::vector<std::pair<std::string, std::string> >		problemStrings;

	problemStrings.push_back(std::make_pair<std::string, std::string>("empty_name", ""));
	problemStrings.push_back(std::make_pair<std::string, std::string>("short_name", "short_name"));
	problemStrings.push_back(std::make_pair<std::string, std::string>("long_name", makeLongUTF8String(65530) + "ccc"));
	getDefaultColors(defaultColors);

	fragments["debug"]			=
		"%other_name = OpString \"other_name\"\n";

	fragments["pre_main"]	=
		"OpLine %file_name 32 0\n"
		"OpLine %file_name 32 32\n"
		"OpLine %file_name 32 40\n"
		"OpLine %other_name 32 40\n"
		"OpLine %other_name 0 100\n"
		"OpLine %other_name 0 4294967295\n"
		"OpLine %other_name 4294967295 0\n"
		"OpLine %other_name 32 40\n"
		"OpLine %file_name 0 0\n"
		"%second_function = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"OpLine %file_name 1 0\n"
		"%second_param1 = OpFunctionParameter %v4f32\n"
		"OpLine %file_name 1 3\n"
		"OpLine %file_name 1 2\n"
		"%label_secondfunction = OpLabel\n"
		"OpLine %file_name 0 2\n"
		"OpReturnValue %second_param1\n"
		"OpFunctionEnd\n"
		"OpLine %file_name 0 2\n"
		"OpLine %file_name 0 2\n";

	fragments["testfun"]		=
		// A %test_code function that returns its argument unchanged.
		"OpLine %file_name 1 0\n"
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"OpLine %file_name 16 330\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"OpLine %file_name 14 442\n"
		"%label_testfun = OpLabel\n"
		"OpLine %file_name 11 1024\n"
		"%val1 = OpFunctionCall %v4f32 %second_function %param1\n"
		"OpLine %file_name 2 97\n"
		"OpReturnValue %val1\n"
		"OpFunctionEnd\n"
		"OpLine %file_name 5 32\n";

	for (size_t i = 0; i < problemStrings.size(); ++i)
	{
		map<string, string> testFragments = fragments;
		testFragments["debug"] += "%file_name = OpString \"" + problemStrings[i].second + "\"\n";
		createTestsForAllStages(string("opline") + "_" + problemStrings[i].first, defaultColors, defaultColors, testFragments, opLineTests.get());
	}

	return opLineTests.release();
}

tcu::TestCaseGroup* createOpConstantNullTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opConstantNullTests		(new tcu::TestCaseGroup(testCtx, "opconstantnull", "OpConstantNull instruction"));
	RGBA							colors[4];


	const char						functionStart[] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%lbl    = OpLabel\n";

	const char						functionEnd[]	=
		"OpReturnValue %transformed_param\n"
		"OpFunctionEnd\n";

	struct NameConstantsCode
	{
		string name;
		string constants;
		string code;
	};

	NameConstantsCode tests[] =
	{
		{
			"vec4",
			"%cnull = OpConstantNull %v4f32\n",
			"%transformed_param = OpFAdd %v4f32 %param1 %cnull\n"
		},
		{
			"float",
			"%cnull = OpConstantNull %f32\n",
			"%vp = OpVariable %fp_v4f32 Function\n"
			"%v  = OpLoad %v4f32 %vp\n"
			"%v0 = OpVectorInsertDynamic %v4f32 %v %cnull %c_i32_0\n"
			"%v1 = OpVectorInsertDynamic %v4f32 %v0 %cnull %c_i32_1\n"
			"%v2 = OpVectorInsertDynamic %v4f32 %v1 %cnull %c_i32_2\n"
			"%v3 = OpVectorInsertDynamic %v4f32 %v2 %cnull %c_i32_3\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %v3\n"
		},
		{
			"bool",
			"%cnull             = OpConstantNull %bool\n",
			"%v                 = OpVariable %fp_v4f32 Function\n"
			"                     OpStore %v %param1\n"
			"                     OpSelectionMerge %false_label None\n"
			"                     OpBranchConditional %cnull %true_label %false_label\n"
			"%true_label        = OpLabel\n"
			"                     OpStore %v %c_v4f32_0_5_0_5_0_5_0_5\n"
			"                     OpBranch %false_label\n"
			"%false_label       = OpLabel\n"
			"%transformed_param = OpLoad %v4f32 %v\n"
		},
		{
			"i32",
			"%cnull             = OpConstantNull %i32\n",
			"%v                 = OpVariable %fp_v4f32 Function %c_v4f32_0_5_0_5_0_5_0_5\n"
			"%b                 = OpIEqual %bool %cnull %c_i32_0\n"
			"                     OpSelectionMerge %false_label None\n"
			"                     OpBranchConditional %b %true_label %false_label\n"
			"%true_label        = OpLabel\n"
			"                     OpStore %v %param1\n"
			"                     OpBranch %false_label\n"
			"%false_label       = OpLabel\n"
			"%transformed_param = OpLoad %v4f32 %v\n"
		},
		{
			"struct",
			"%stype             = OpTypeStruct %f32 %v4f32\n"
			"%fp_stype          = OpTypePointer Function %stype\n"
			"%cnull             = OpConstantNull %stype\n",
			"%v                 = OpVariable %fp_stype Function %cnull\n"
			"%f                 = OpAccessChain %fp_v4f32 %v %c_i32_1\n"
			"%f_val             = OpLoad %v4f32 %f\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %f_val\n"
		},
		{
			"array",
			"%a4_v4f32          = OpTypeArray %v4f32 %c_u32_4\n"
			"%fp_a4_v4f32       = OpTypePointer Function %a4_v4f32\n"
			"%cnull             = OpConstantNull %a4_v4f32\n",
			"%v                 = OpVariable %fp_a4_v4f32 Function %cnull\n"
			"%f                 = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
			"%f1                = OpAccessChain %fp_v4f32 %v %c_u32_1\n"
			"%f2                = OpAccessChain %fp_v4f32 %v %c_u32_2\n"
			"%f3                = OpAccessChain %fp_v4f32 %v %c_u32_3\n"
			"%f_val             = OpLoad %v4f32 %f\n"
			"%f1_val            = OpLoad %v4f32 %f1\n"
			"%f2_val            = OpLoad %v4f32 %f2\n"
			"%f3_val            = OpLoad %v4f32 %f3\n"
			"%t0                = OpFAdd %v4f32 %param1 %f_val\n"
			"%t1                = OpFAdd %v4f32 %t0 %f1_val\n"
			"%t2                = OpFAdd %v4f32 %t1 %f2_val\n"
			"%transformed_param = OpFAdd %v4f32 %t2 %f3_val\n"
		},
		{
			"matrix",
			"%mat4x4_f32        = OpTypeMatrix %v4f32 4\n"
			"%cnull             = OpConstantNull %mat4x4_f32\n",
			// Our null matrix * any vector should result in a zero vector.
			"%v                 = OpVectorTimesMatrix %v4f32 %param1 %cnull\n"
			"%transformed_param = OpFAdd %v4f32 %param1 %v\n"
		}
	};

	getHalfColorsFullAlpha(colors);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameConstantsCode); ++testNdx)
	{
		map<string, string> fragments;
		fragments["pre_main"] = tests[testNdx].constants;
		fragments["testfun"] = string(functionStart) + tests[testNdx].code + functionEnd;
		createTestsForAllStages(tests[testNdx].name, colors, colors, fragments, opConstantNullTests.get());
	}
	return opConstantNullTests.release();
}
tcu::TestCaseGroup* createOpConstantCompositeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opConstantCompositeTests		(new tcu::TestCaseGroup(testCtx, "opconstantcomposite", "OpConstantComposite instruction"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];


	const char						functionStart[]	 =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%lbl    = OpLabel\n";

	const char						functionEnd[]		=
		"OpReturnValue %transformed_param\n"
		"OpFunctionEnd\n";

	struct NameConstantsCode
	{
		string name;
		string constants;
		string code;
	};

	NameConstantsCode tests[] =
	{
		{
			"vec4",

			"%cval              = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0\n",
			"%transformed_param = OpFAdd %v4f32 %param1 %cval\n"
		},
		{
			"struct",

			"%stype             = OpTypeStruct %v4f32 %f32\n"
			"%fp_stype          = OpTypePointer Function %stype\n"
			"%f32_n_1           = OpConstant %f32 -1.0\n"
			"%f32_1_5           = OpConstant %f32 !0x3fc00000\n" // +1.5
			"%cvec              = OpConstantComposite %v4f32 %f32_1_5 %f32_1_5 %f32_1_5 %c_f32_1\n"
			"%cval              = OpConstantComposite %stype %cvec %f32_n_1\n",

			"%v                 = OpVariable %fp_stype Function %cval\n"
			"%vec_ptr           = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
			"%f32_ptr           = OpAccessChain %fp_f32 %v %c_u32_1\n"
			"%vec_val           = OpLoad %v4f32 %vec_ptr\n"
			"%f32_val           = OpLoad %f32 %f32_ptr\n"
			"%tmp1              = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_1 %f32_val\n" // vec4(-1)
			"%tmp2              = OpFAdd %v4f32 %tmp1 %param1\n" // param1 + vec4(-1)
			"%transformed_param = OpFAdd %v4f32 %tmp2 %vec_val\n" // param1 + vec4(-1) + vec4(1.5, 1.5, 1.5, 1.0)
		},
		{
			// [1|0|0|0.5] [x] = x + 0.5
			// [0|1|0|0.5] [y] = y + 0.5
			// [0|0|1|0.5] [z] = z + 0.5
			// [0|0|0|1  ] [1] = 1
			"matrix",

			"%mat4x4_f32          = OpTypeMatrix %v4f32 4\n"
			"%v4f32_1_0_0_0       = OpConstantComposite %v4f32 %c_f32_1 %c_f32_0 %c_f32_0 %c_f32_0\n"
			"%v4f32_0_1_0_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_1 %c_f32_0 %c_f32_0\n"
			"%v4f32_0_0_1_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_1 %c_f32_0\n"
			"%v4f32_0_5_0_5_0_5_1 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_1\n"
			"%cval                = OpConstantComposite %mat4x4_f32 %v4f32_1_0_0_0 %v4f32_0_1_0_0 %v4f32_0_0_1_0 %v4f32_0_5_0_5_0_5_1\n",

			"%transformed_param   = OpMatrixTimesVector %v4f32 %cval %param1\n"
		},
		{
			"array",

			"%c_v4f32_1_1_1_0     = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
			"%fp_a4f32            = OpTypePointer Function %a4f32\n"
			"%f32_n_1             = OpConstant %f32 -1.0\n"
			"%f32_1_5             = OpConstant %f32 !0x3fc00000\n" // +1.5
			"%carr                = OpConstantComposite %a4f32 %c_f32_0 %f32_n_1 %f32_1_5 %c_f32_0\n",

			"%v                   = OpVariable %fp_a4f32 Function %carr\n"
			"%f                   = OpAccessChain %fp_f32 %v %c_u32_0\n"
			"%f1                  = OpAccessChain %fp_f32 %v %c_u32_1\n"
			"%f2                  = OpAccessChain %fp_f32 %v %c_u32_2\n"
			"%f3                  = OpAccessChain %fp_f32 %v %c_u32_3\n"
			"%f_val               = OpLoad %f32 %f\n"
			"%f1_val              = OpLoad %f32 %f1\n"
			"%f2_val              = OpLoad %f32 %f2\n"
			"%f3_val              = OpLoad %f32 %f3\n"
			"%ftot1               = OpFAdd %f32 %f_val %f1_val\n"
			"%ftot2               = OpFAdd %f32 %ftot1 %f2_val\n"
			"%ftot3               = OpFAdd %f32 %ftot2 %f3_val\n"  // 0 - 1 + 1.5 + 0
			"%add_vec             = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_0 %ftot3\n"
			"%transformed_param   = OpFAdd %v4f32 %param1 %add_vec\n"
		},
		{
			//
			// [
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   },
			//   {
			//      1.0,
			//      [ 0.0, 0.5, 0.0, 0.0]
			//   }, //     ^^^
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   }
			// ]
			"array_of_struct_of_array",

			"%c_v4f32_1_1_1_0     = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_0\n"
			"%fp_a4f32            = OpTypePointer Function %a4f32\n"
			"%stype               = OpTypeStruct %f32 %a4f32\n"
			"%a3stype             = OpTypeArray %stype %c_u32_3\n"
			"%fp_a3stype          = OpTypePointer Function %a3stype\n"
			"%ca4f32_0            = OpConstantComposite %a4f32 %c_f32_0 %c_f32_0_5 %c_f32_0 %c_f32_0\n"
			"%ca4f32_1            = OpConstantComposite %a4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"
			"%cstype1             = OpConstantComposite %stype %c_f32_0 %ca4f32_1\n"
			"%cstype2             = OpConstantComposite %stype %c_f32_1 %ca4f32_0\n"
			"%carr                = OpConstantComposite %a3stype %cstype1 %cstype2 %cstype1",

			"%v                   = OpVariable %fp_a3stype Function %carr\n"
			"%f                   = OpAccessChain %fp_f32 %v %c_u32_1 %c_u32_1 %c_u32_1\n"
			"%f_l                 = OpLoad %f32 %f\n"
			"%add_vec             = OpVectorTimesScalar %v4f32 %c_v4f32_1_1_1_0 %f_l\n"
			"%transformed_param   = OpFAdd %v4f32 %param1 %add_vec\n"
		}
	};

	getHalfColorsFullAlpha(inputColors);
	outputColors[0] = RGBA(255, 255, 255, 255);
	outputColors[1] = RGBA(255, 127, 127, 255);
	outputColors[2] = RGBA(127, 255, 127, 255);
	outputColors[3] = RGBA(127, 127, 255, 255);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameConstantsCode); ++testNdx)
	{
		map<string, string> fragments;
		fragments["pre_main"] = tests[testNdx].constants;
		fragments["testfun"] = string(functionStart) + tests[testNdx].code + functionEnd;
		createTestsForAllStages(tests[testNdx].name, inputColors, outputColors, fragments, opConstantCompositeTests.get());
	}
	return opConstantCompositeTests.release();
}

tcu::TestCaseGroup* createSelectionBlockOrderTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "selection_block_order", "Out-of-order blocks for selection"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     if (i == 0) result[i] = 0.;
	//     else        result[i] = 1. - result[i];
	//   }
	//   return result;
	// }
	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		// Loop entry block.
		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %if_entry None\n"
		"             OpBranchConditional %lt_4 %if_entry %exit\n"

		// Merge block for loop.
		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		// If-statement entry block.
		"%if_entry  = OpLabel\n"
		"%loc       = OpAccessChain %fp_f32 %result %ival\n"
		"%eq_0      = OpIEqual %bool %ival %c_i32_0\n"
		"             OpSelectionMerge %if_exit None\n"
		"             OpBranchConditional %eq_0 %if_true %if_false\n"

		// False branch for if-statement.
		"%if_false  = OpLabel\n"
		"%val       = OpLoad %f32 %loc\n"
		"%sub       = OpFSub %f32 %c_f32_1 %val\n"
		"             OpStore %loc %sub\n"
		"             OpBranch %if_exit\n"

		// Merge block for if-statement.
		"%if_exit   = OpLabel\n"
		"%ival_next = OpIAdd %i32 %ival %c_i32_1\n"
		"             OpStore %iptr %ival_next\n"
		"             OpBranch %loop\n"

		// True branch for if-statement.
		"%if_true   = OpLabel\n"
		"             OpStore %loc %c_f32_0\n"
		"             OpBranch %if_exit\n"

		"             OpFunctionEnd\n";

	fragments["testfun"]	= function;

	inputColors[0]			= RGBA(127, 127, 127, 0);
	inputColors[1]			= RGBA(127, 0,   0,   0);
	inputColors[2]			= RGBA(0,   127, 0,   0);
	inputColors[3]			= RGBA(0,   0,   127, 0);

	outputColors[0]			= RGBA(0, 128, 128, 255);
	outputColors[1]			= RGBA(0, 255, 255, 255);
	outputColors[2]			= RGBA(0, 128, 255, 255);
	outputColors[3]			= RGBA(0, 255, 128, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors, fragments, group.get());

	return group.release();
}

tcu::TestCaseGroup* createSwitchBlockOrderTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "switch_block_order", "Out-of-order blocks for switch"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	const char						typesAndConstants[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n"
		"%c_f32_p4  = OpConstant %f32 0.4\n"
		"%c_f32_p6  = OpConstant %f32 0.6\n"
		"%c_f32_p8  = OpConstant %f32 0.8\n";

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     switch (i) {
	//       case 0: result[i] += .2; break;
	//       case 1: result[i] += .6; break;
	//       case 2: result[i] += .4; break;
	//       case 3: result[i] += .8; break;
	//       default: break; // unreachable
	//     }
	//   }
	//   return result;
	// }
	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		// Loop entry block.
		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %cont None\n"
		"             OpBranchConditional %lt_4 %switch_entry %exit\n"

		// Merge block for loop.
		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		// Switch-statement entry block.
		"%switch_entry   = OpLabel\n"
		"%loc            = OpAccessChain %fp_f32 %result %ival\n"
		"%val            = OpLoad %f32 %loc\n"
		"                  OpSelectionMerge %switch_exit None\n"
		"                  OpSwitch %ival %switch_default 0 %case0 1 %case1 2 %case2 3 %case3\n"

		"%case2          = OpLabel\n"
		"%addp4          = OpFAdd %f32 %val %c_f32_p4\n"
		"                  OpStore %loc %addp4\n"
		"                  OpBranch %switch_exit\n"

		"%switch_default = OpLabel\n"
		"                  OpUnreachable\n"

		"%case3          = OpLabel\n"
		"%addp8          = OpFAdd %f32 %val %c_f32_p8\n"
		"                  OpStore %loc %addp8\n"
		"                  OpBranch %switch_exit\n"

		"%case0          = OpLabel\n"
		"%addp2          = OpFAdd %f32 %val %c_f32_p2\n"
		"                  OpStore %loc %addp2\n"
		"                  OpBranch %switch_exit\n"

		// Merge block for switch-statement.
		"%switch_exit    = OpLabel\n"
		"%ival_next      = OpIAdd %i32 %ival %c_i32_1\n"
		"                  OpStore %iptr %ival_next\n"
		"                  OpBranch %cont\n"
		"%cont           = OpLabel\n"
		"                  OpBranch %loop\n"

		"%case1          = OpLabel\n"
		"%addp6          = OpFAdd %f32 %val %c_f32_p6\n"
		"                  OpStore %loc %addp6\n"
		"                  OpBranch %switch_exit\n"

		"                  OpFunctionEnd\n";

	fragments["pre_main"]	= typesAndConstants;
	fragments["testfun"]	= function;

	inputColors[0]			= RGBA(127, 27,  127, 51);
	inputColors[1]			= RGBA(127, 0,   0,   51);
	inputColors[2]			= RGBA(0,   27,  0,   51);
	inputColors[3]			= RGBA(0,   0,   127, 51);

	outputColors[0]			= RGBA(178, 180, 229, 255);
	outputColors[1]			= RGBA(178, 153, 102, 255);
	outputColors[2]			= RGBA(51,  180, 102, 255);
	outputColors[3]			= RGBA(51,  153, 229, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors, fragments, group.get());

	addOpSwitchAmberTests(*group, testCtx);

	return group.release();
}

tcu::TestCaseGroup* createDecorationGroupTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "decoration_group", "Decoration group tests"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	map<string, string>				fragments;

	const char						decorations[]		=
		"OpDecorate %array_group         ArrayStride 4\n"
		"OpDecorate %struct_member_group Offset 0\n"
		"%array_group         = OpDecorationGroup\n"
		"%struct_member_group = OpDecorationGroup\n"

		"OpDecorate %group1 RelaxedPrecision\n"
		"OpDecorate %group3 RelaxedPrecision\n"
		"OpDecorate %group3 Flat\n"
		"OpDecorate %group3 Restrict\n"
		"%group0 = OpDecorationGroup\n"
		"%group1 = OpDecorationGroup\n"
		"%group3 = OpDecorationGroup\n";

	const char						typesAndConstants[]	=
		"%a3f32     = OpTypeArray %f32 %c_u32_3\n"
		"%struct1   = OpTypeStruct %a3f32\n"
		"%struct2   = OpTypeStruct %a3f32\n"
		"%fp_struct1 = OpTypePointer Function %struct1\n"
		"%fp_struct2 = OpTypePointer Function %struct2\n"
		"%c_f32_2    = OpConstant %f32 2.\n"
		"%c_f32_n2   = OpConstant %f32 -2.\n"

		"%c_a3f32_1 = OpConstantComposite %a3f32 %c_f32_1 %c_f32_2 %c_f32_1\n"
		"%c_a3f32_2 = OpConstantComposite %a3f32 %c_f32_n1 %c_f32_n2 %c_f32_n1\n"
		"%c_struct1 = OpConstantComposite %struct1 %c_a3f32_1\n"
		"%c_struct2 = OpConstantComposite %struct2 %c_a3f32_2\n";

	const char						function[]			=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"%v_struct1 = OpVariable %fp_struct1 Function\n"
		"%v_struct2 = OpVariable %fp_struct2 Function\n"
		"             OpStore %result %param\n"
		"             OpStore %v_struct1 %c_struct1\n"
		"             OpStore %v_struct2 %c_struct2\n"
		"%ptr1      = OpAccessChain %fp_f32 %v_struct1 %c_i32_0 %c_i32_2\n"
		"%val1      = OpLoad %f32 %ptr1\n"
		"%ptr2      = OpAccessChain %fp_f32 %v_struct2 %c_i32_0 %c_i32_2\n"
		"%val2      = OpLoad %f32 %ptr2\n"
		"%addvalues = OpFAdd %f32 %val1 %val2\n"
		"%ptr       = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%val       = OpLoad %f32 %ptr\n"
		"%addresult = OpFAdd %f32 %addvalues %val\n"
		"             OpStore %ptr %addresult\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	struct CaseNameDecoration
	{
		string name;
		string decoration;
	};

	CaseNameDecoration tests[] =
	{
		{
			"same_decoration_group_on_multiple_types",
			"OpGroupMemberDecorate %struct_member_group %struct1 0 %struct2 0\n"
		},
		{
			"empty_decoration_group",
			"OpGroupDecorate %group0      %a3f32\n"
			"OpGroupDecorate %group0      %result\n"
		},
		{
			"one_element_decoration_group",
			"OpGroupDecorate %array_group %a3f32\n"
		},
		{
			"multiple_elements_decoration_group",
			"OpGroupDecorate %group3      %v_struct1\n"
		},
		{
			"multiple_decoration_groups_on_same_variable",
			"OpGroupDecorate %group0      %v_struct2\n"
			"OpGroupDecorate %group1      %v_struct2\n"
			"OpGroupDecorate %group3      %v_struct2\n"
		},
		{
			"same_decoration_group_multiple_times",
			"OpGroupDecorate %group1      %addvalues\n"
			"OpGroupDecorate %group1      %addvalues\n"
			"OpGroupDecorate %group1      %addvalues\n"
		},

	};

	getHalfColorsFullAlpha(inputColors);
	getHalfColorsFullAlpha(outputColors);

	for (size_t idx = 0; idx < (sizeof(tests) / sizeof(tests[0])); ++idx)
	{
		fragments["decoration"]	= decorations + tests[idx].decoration;
		fragments["pre_main"]	= typesAndConstants;
		fragments["testfun"]	= function;

		createTestsForAllStages(tests[idx].name, inputColors, outputColors, fragments, group.get());
	}

	return group.release();
}

struct SpecConstantTwoValGraphicsCase
{
	const std::string	caseName;
	const std::string	scDefinition0;
	const std::string	scDefinition1;
	const std::string	scResultType;
	const std::string	scOperation;
	SpecConstantValue	scActualValue0;
	SpecConstantValue	scActualValue1;
	const std::string	resultOperation;
	RGBA				expectedColors[4];
	CaseFlags			caseFlags;

						SpecConstantTwoValGraphicsCase (const std::string&			name,
														const std::string&			definition0,
														const std::string&			definition1,
														const std::string&			resultType,
														const std::string&			operation,
														const SpecConstantValue&	value0,
														const SpecConstantValue&	value1,
														const std::string&			resultOp,
														const RGBA					(&output)[4],
														CaseFlags					flags = FLAG_NONE)
							: caseName				(name)
							, scDefinition0			(definition0)
							, scDefinition1			(definition1)
							, scResultType			(resultType)
							, scOperation			(operation)
							, scActualValue0		(value0)
							, scActualValue1		(value1)
							, resultOperation		(resultOp)
							, caseFlags				(flags)
	{
		expectedColors[0] = output[0];
		expectedColors[1] = output[1];
		expectedColors[2] = output[2];
		expectedColors[3] = output[3];
	}
};

tcu::TestCaseGroup* createSpecConstantTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>			group (new tcu::TestCaseGroup(testCtx, "opspecconstantop", "Test the OpSpecConstantOp instruction"));
	vector<SpecConstantTwoValGraphicsCase>	cases;
	RGBA									inputColors[4];
	RGBA									outputColors0[4];
	RGBA									outputColors1[4];
	RGBA									outputColors2[4];

	const char	decorations1[]			=
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n";

	const char	typesAndConstants1[]	=
		"${OPTYPE_DEFINITIONS:opt}"
		"%sc_0      = OpSpecConstant${SC_DEF0}\n"
		"%sc_1      = OpSpecConstant${SC_DEF1}\n"
		"%sc_op     = OpSpecConstantOp ${SC_RESULT_TYPE} ${SC_OP}\n";

	const char	function1[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"${TYPE_CONVERT:opt}"
		"             OpStore %result %param\n"
		"%gen       = ${GEN_RESULT}\n"
		"%index     = OpIAdd %i32 %gen %c_i32_1\n"
		"%loc       = OpAccessChain %fp_f32 %result %index\n"
		"%val       = OpLoad %f32 %loc\n"
		"%add       = OpFAdd %f32 %val %c_f32_0_5\n"
		"             OpStore %loc %add\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	inputColors[0] = RGBA(127, 127, 127, 255);
	inputColors[1] = RGBA(127, 0,   0,   255);
	inputColors[2] = RGBA(0,   127, 0,   255);
	inputColors[3] = RGBA(0,   0,   127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][0].
	outputColors0[0] = RGBA(255, 127, 127, 255);
	outputColors0[1] = RGBA(255, 0,   0,   255);
	outputColors0[2] = RGBA(128, 127, 0,   255);
	outputColors0[3] = RGBA(128, 0,   127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][1].
	outputColors1[0] = RGBA(127, 255, 127, 255);
	outputColors1[1] = RGBA(127, 128, 0,   255);
	outputColors1[2] = RGBA(0,   255, 0,   255);
	outputColors1[3] = RGBA(0,   128, 127, 255);

	// Derived from inputColors[x] by adding 128 to inputColors[x][2].
	outputColors2[0] = RGBA(127, 127, 255, 255);
	outputColors2[1] = RGBA(127, 0,   128, 255);
	outputColors2[2] = RGBA(0,   127, 128, 255);
	outputColors2[3] = RGBA(0,   0,   255, 255);

	const char addZeroToSc[]		= "OpIAdd %i32 %c_i32_0 %sc_op";
	const char addZeroToSc32[]		= "OpIAdd %i32 %c_i32_0 %sc_op32";
	const char selectTrueUsingSc[]	= "OpSelect %i32 %sc_op %c_i32_1 %c_i32_0";
	const char selectFalseUsingSc[]	= "OpSelect %i32 %sc_op %c_i32_0 %c_i32_1";

	cases.push_back(SpecConstantTwoValGraphicsCase("iadd",							" %i32 0",		" %i32 0",		"%i32",		"IAdd                 %sc_0 %sc_1",				19,					-20,				addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoValGraphicsCase("isub",							" %i32 0",		" %i32 0",		"%i32",		"ISub                 %sc_0 %sc_1",				19,					20,					addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoValGraphicsCase("imul",							" %i32 0",		" %i32 0",		"%i32",		"IMul                 %sc_0 %sc_1",				-1,					-1,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("sdiv",							" %i32 0",		" %i32 0",		"%i32",		"SDiv                 %sc_0 %sc_1",				-126,				126,				addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoValGraphicsCase("udiv",							" %i32 0",		" %i32 0",		"%i32",		"UDiv                 %sc_0 %sc_1",				126,				126,				addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("srem",							" %i32 0",		" %i32 0",		"%i32",		"SRem                 %sc_0 %sc_1",				3,					2,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("smod",							" %i32 0",		" %i32 0",		"%i32",		"SMod                 %sc_0 %sc_1",				3,					2,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("umod",							" %i32 0",		" %i32 0",		"%i32",		"UMod                 %sc_0 %sc_1",				1001,				500,				addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("bitwiseand",					" %i32 0",		" %i32 0",		"%i32",		"BitwiseAnd           %sc_0 %sc_1",				0x33,				0x0d,				addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("bitwiseor",						" %i32 0",		" %i32 0",		"%i32",		"BitwiseOr            %sc_0 %sc_1",				0,					1,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("bitwisexor",					" %i32 0",		" %i32 0",		"%i32",		"BitwiseXor           %sc_0 %sc_1",				0x2e,				0x2f,				addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical",				" %i32 0",		" %i32 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",				2,					1,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic",			" %i32 0",		" %i32 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",				-4,					2,					addZeroToSc,		outputColors0));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical",				" %i32 0",		" %i32 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",				1,					0,					addZeroToSc,		outputColors2));

	// Shifts for other integer sizes.
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_i64",			" %i64 0",		" %i64 0",		"%i64",		"ShiftRightLogical    %sc_0 %sc_1",				deInt64{2},			deInt64{1},			addZeroToSc32,		outputColors2, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_i64",		" %i64 0",		" %i64 0",		"%i64",		"ShiftRightArithmetic %sc_0 %sc_1",				deInt64{-4},		deInt64{2},			addZeroToSc32,		outputColors0, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_i64",			" %i64 0",		" %i64 0",		"%i64",		"ShiftLeftLogical     %sc_0 %sc_1",				deInt64{1},			deInt64{0},			addZeroToSc32,		outputColors2, (FLAG_I64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_i16",			" %i16 0",		" %i16 0",		"%i16",		"ShiftRightLogical    %sc_0 %sc_1",				deInt16{2},			deInt16{1},			addZeroToSc32,		outputColors2, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_i16",		" %i16 0",		" %i16 0",		"%i16",		"ShiftRightArithmetic %sc_0 %sc_1",				deInt16{-4},		deInt16{2},			addZeroToSc32,		outputColors0, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_i16",			" %i16 0",		" %i16 0",		"%i16",		"ShiftLeftLogical     %sc_0 %sc_1",				deInt16{1},			deInt16{0},			addZeroToSc32,		outputColors2, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_i8",			" %i8 0",		" %i8 0",		"%i8",		"ShiftRightLogical    %sc_0 %sc_1",				deInt8{2},			deInt8{1},			addZeroToSc32,		outputColors2, (FLAG_I8 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_i8",		" %i8 0",		" %i8 0",		"%i8",		"ShiftRightArithmetic %sc_0 %sc_1",				deInt8{-4},			deInt8{2},			addZeroToSc32,		outputColors0, (FLAG_I8 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_i8",			" %i8 0",		" %i8 0",		"%i8",		"ShiftLeftLogical     %sc_0 %sc_1",				deInt8{1},			deInt8{0},			addZeroToSc32,		outputColors2, (FLAG_I8 | FLAG_CONVERT)));

	// Shifts for other integer sizes but only in the shift amount.
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_s_i64",		" %i32 0",		" %i64 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",				2,					deInt64{1},			addZeroToSc,		outputColors2, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_s_i64",	" %i32 0",		" %i64 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",				-4,					deInt64{2},			addZeroToSc,		outputColors0, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_s_i64",		" %i32 0",		" %i64 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",				1,					deInt64{0},			addZeroToSc,		outputColors2, (FLAG_I64)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_s_i16",		" %i32 0",		" %i16 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",				2,					deInt16{1},			addZeroToSc,		outputColors2, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_s_i16",	" %i32 0",		" %i16 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",				-4,					deInt16{2},			addZeroToSc,		outputColors0, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_s_i16",		" %i32 0",		" %i16 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",				1,					deInt16{0},			addZeroToSc,		outputColors2, (FLAG_I16)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightlogical_s_i8",		" %i32 0",		" %i8 0",		"%i32",		"ShiftRightLogical    %sc_0 %sc_1",				2,					deInt8{1},			addZeroToSc,		outputColors2, (FLAG_I8)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftrightarithmetic_s_i8",		" %i32 0",		" %i8 0",		"%i32",		"ShiftRightArithmetic %sc_0 %sc_1",				-4,					deInt8{2},			addZeroToSc,		outputColors0, (FLAG_I8)));
	cases.push_back(SpecConstantTwoValGraphicsCase("shiftleftlogical_s_i8",			" %i32 0",		" %i8 0",		"%i32",		"ShiftLeftLogical     %sc_0 %sc_1",				1,					deInt8{0},			addZeroToSc,		outputColors2, (FLAG_I8)));

	cases.push_back(SpecConstantTwoValGraphicsCase("slessthan",						" %i32 0",		" %i32 0",		"%bool",	"SLessThan            %sc_0 %sc_1",				-20,				-10,				selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("ulessthan",						" %i32 0",		" %i32 0",		"%bool",	"ULessThan            %sc_0 %sc_1",				10,					20,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("sgreaterthan",					" %i32 0",		" %i32 0",		"%bool",	"SGreaterThan         %sc_0 %sc_1",				-1000,				50,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("ugreaterthan",					" %i32 0",		" %i32 0",		"%bool",	"UGreaterThan         %sc_0 %sc_1",				10,					5,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("slessthanequal",				" %i32 0",		" %i32 0",		"%bool",	"SLessThanEqual       %sc_0 %sc_1",				-10,				-10,				selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("ulessthanequal",				" %i32 0",		" %i32 0",		"%bool",	"ULessThanEqual       %sc_0 %sc_1",				50,					100,				selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("sgreaterthanequal",				" %i32 0",		" %i32 0",		"%bool",	"SGreaterThanEqual    %sc_0 %sc_1",				-1000,				50,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("ugreaterthanequal",				" %i32 0",		" %i32 0",		"%bool",	"UGreaterThanEqual    %sc_0 %sc_1",				10,					10,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("iequal",						" %i32 0",		" %i32 0",		"%bool",	"IEqual               %sc_0 %sc_1",				42,					24,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("inotequal",						" %i32 0",		" %i32 0",		"%bool",	"INotEqual            %sc_0 %sc_1",				42,					24,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("logicaland",					"True %bool",	"True %bool",	"%bool",	"LogicalAnd           %sc_0 %sc_1",				0,					1,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("logicalor",						"False %bool",	"False %bool",	"%bool",	"LogicalOr            %sc_0 %sc_1",				1,					0,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("logicalequal",					"True %bool",	"True %bool",	"%bool",	"LogicalEqual         %sc_0 %sc_1",				0,					1,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("logicalnotequal",				"False %bool",	"False %bool",	"%bool",	"LogicalNotEqual      %sc_0 %sc_1",				1,					0,					selectTrueUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("snegate",						" %i32 0",		" %i32 0",		"%i32",		"SNegate              %sc_0",					-1,					0,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("not",							" %i32 0",		" %i32 0",		"%i32",		"Not                  %sc_0",					-2,					0,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("logicalnot",					"False %bool",	"False %bool",	"%bool",	"LogicalNot           %sc_0",					1,					0,					selectFalseUsingSc,	outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("select",						"False %bool",	" %i32 0",		"%i32",		"Select               %sc_0 %sc_1 %c_i32_0",	1,					1,					addZeroToSc,		outputColors2));
	cases.push_back(SpecConstantTwoValGraphicsCase("sconvert",						" %i32 0",		" %i32 0",		"%i16",		"SConvert             %sc_0",					-1,					0,					addZeroToSc32,		outputColors0, (FLAG_I16 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("fconvert",						" %f32 0",		" %f32 0",		"%f64",		"FConvert             %sc_0",					tcu::Float32(-1.0),	tcu::Float32(0.0),	addZeroToSc32,		outputColors0, (FLAG_F64 | FLAG_CONVERT)));
	cases.push_back(SpecConstantTwoValGraphicsCase("fconvert16",					" %f16 0",		" %f16 0",		"%f32",		"FConvert             %sc_0",					tcu::Float16(-1.0),	tcu::Float16(0.0),	addZeroToSc32,		outputColors0, (FLAG_F16 | FLAG_CONVERT)));
	// \todo[2015-12-1 antiagainst] OpQuantizeToF16

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		map<string, string>			specializations;
		map<string, string>			fragments;
		SpecConstants				specConstants;
		PushConstants				noPushConstants;
		GraphicsResources			noResources;
		GraphicsInterfaces			noInterfaces;
		vector<string>				extensions;
		VulkanFeatures				requiredFeatures;

		// Special SPIR-V code when using 16-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I16)
		{
			requiredFeatures.coreFeatures.shaderInt16		= VK_TRUE;
			fragments["capability"]							+= "OpCapability Int16\n";							// Adds 16-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]			+= "%i16 = OpTypeInt 16 1\n";						// Adds 16-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]				+= "%sc_op32 = OpSConvert %i32 %sc_op\n";			// Converts 16-bit integer to 32-bit integer
		}

		// Special SPIR-V code when using 64-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I64)
		{
			requiredFeatures.coreFeatures.shaderInt64		= VK_TRUE;
			fragments["capability"]							+= "OpCapability Int64\n";							// Adds 64-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]			+= "%i64 = OpTypeInt 64 1\n";						// Adds 64-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]				+= "%sc_op32 = OpSConvert %i32 %sc_op\n";			// Converts 64-bit integer to 32-bit integer
		}

		// Special SPIR-V code when using 64-bit floats.
		if (cases[caseNdx].caseFlags & FLAG_F64)
		{
			requiredFeatures.coreFeatures.shaderFloat64		= VK_TRUE;
			fragments["capability"]							+= "OpCapability Float64\n";						// Adds 64-bit float capability
			specializations["OPTYPE_DEFINITIONS"]			+= "%f64 = OpTypeFloat 64\n";						// Adds 64-bit float type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]				+= "%sc_op32 = OpConvertFToS %i32 %sc_op\n";		// Converts 64-bit float to 32-bit integer
		}

		// Extension needed for float16 and int8.
		if (cases[caseNdx].caseFlags & (FLAG_F16 | FLAG_I8))
			extensions.push_back("VK_KHR_shader_float16_int8");

		// Special SPIR-V code when using 16-bit floats.
		if (cases[caseNdx].caseFlags & FLAG_F16)
		{
			requiredFeatures.extFloat16Int8				|= EXTFLOAT16INT8FEATURES_FLOAT16;
			fragments["capability"]						+= "OpCapability Float16\n";						// Adds 16-bit float capability
			specializations["OPTYPE_DEFINITIONS"]		+= "%f16 = OpTypeFloat 16\n";						// Adds 16-bit float type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]			+= "%sc_op32 = OpConvertFToS %i32 %sc_op\n";		// Converts 16-bit float to 32-bit integer
		}

		// Special SPIR-V code when using 8-bit integers.
		if (cases[caseNdx].caseFlags & FLAG_I8)
		{
			requiredFeatures.extFloat16Int8				|= EXTFLOAT16INT8FEATURES_INT8;
			fragments["capability"]						+= "OpCapability Int8\n";						// Adds 8-bit integer capability
			specializations["OPTYPE_DEFINITIONS"]		+= "%i8 = OpTypeInt 8 1\n";						// Adds 8-bit integer type
			if (cases[caseNdx].caseFlags & FLAG_CONVERT)
				specializations["TYPE_CONVERT"]			+= "%sc_op32 = OpSConvert %i32 %sc_op\n";		// Converts 8-bit integer to 32-bit integer
		}

		specializations["SC_DEF0"]			= cases[caseNdx].scDefinition0;
		specializations["SC_DEF1"]			= cases[caseNdx].scDefinition1;
		specializations["SC_RESULT_TYPE"]	= cases[caseNdx].scResultType;
		specializations["SC_OP"]			= cases[caseNdx].scOperation;
		specializations["GEN_RESULT"]		= cases[caseNdx].resultOperation;

		fragments["decoration"]				= tcu::StringTemplate(decorations1).specialize(specializations);
		fragments["pre_main"]				= tcu::StringTemplate(typesAndConstants1).specialize(specializations);
		fragments["testfun"]				= tcu::StringTemplate(function1).specialize(specializations);

		cases[caseNdx].scActualValue0.appendTo(specConstants);
		cases[caseNdx].scActualValue1.appendTo(specConstants);

		createTestsForAllStages(
			cases[caseNdx].caseName, inputColors, cases[caseNdx].expectedColors, fragments, specConstants,
			noPushConstants, noResources, noInterfaces, extensions, requiredFeatures, group.get());
	}

	const char			decorations2[]		=
		"OpDecorate %sc_0  SpecId 0\n"
		"OpDecorate %sc_1  SpecId 1\n"
		"OpDecorate %sc_2  SpecId 2\n";

	const std::string	typesAndConstants2	=
		"%vec3_0      = OpConstantComposite %v3i32 %c_i32_0 %c_i32_0 %c_i32_0\n"
		"%vec3_undef  = OpUndef %v3i32\n"

		+ getSpecConstantOpStructConstantsAndTypes() + getSpecConstantOpStructComposites() +

		"%sc_0        = OpSpecConstant %i32 0\n"
		"%sc_1        = OpSpecConstant %i32 0\n"
		"%sc_2        = OpSpecConstant %i32 0\n"

		+ getSpecConstantOpStructConstBlock() +

		"%sc_vec3_0   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_0        %vec3_0      0\n"							// (sc_0, 0,    0)
		"%sc_vec3_1   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_1        %vec3_0      1\n"							// (0,    sc_1, 0)
		"%sc_vec3_2   = OpSpecConstantOp %v3i32 CompositeInsert  %sc_2        %vec3_0      2\n"							// (0,    0,    sc_2)
		"%sc_vec3_0_s = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_0   %vec3_undef  0          0xFFFFFFFF 2\n"	// (sc_0, ???,  0)
		"%sc_vec3_1_s = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_1   %vec3_undef  0xFFFFFFFF 1          0\n"	// (???,  sc_1, 0)
		"%sc_vec3_2_s = OpSpecConstantOp %v3i32 VectorShuffle    %vec3_undef  %sc_vec3_2   5          0xFFFFFFFF 5\n"	// (sc_2, ???,  sc_2)
		"%sc_vec3_01  = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_0_s %sc_vec3_1_s 1 0 4\n"						// (0,    sc_0, sc_1)
		"%sc_vec3_012 = OpSpecConstantOp %v3i32 VectorShuffle    %sc_vec3_01  %sc_vec3_2_s 5 1 2\n"						// (sc_2, sc_0, sc_1)
		"%sc_ext_0    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              0\n"							// sc_2
		"%sc_ext_1    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              1\n"							// sc_0
		"%sc_ext_2    = OpSpecConstantOp %i32   CompositeExtract %sc_vec3_012              2\n"							// sc_1
		"%sc_sub      = OpSpecConstantOp %i32   ISub             %sc_ext_0    %sc_ext_1\n"								// (sc_2 - sc_0)
		"%sc_factor   = OpSpecConstantOp %i32   IMul             %sc_sub      %sc_ext_2\n";								// (sc_2 - sc_0) * sc_1

	const std::string	function2			=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"

		+ getSpecConstantOpStructInstructions() +

		"             OpStore %result %param\n"
		"%loc       = OpAccessChain %fp_f32 %result %sc_final\n"
		"%val       = OpLoad %f32 %loc\n"
		"%add       = OpFAdd %f32 %val %c_f32_0_5\n"
		"             OpStore %loc %add\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"
		"             OpFunctionEnd\n";

	map<string, string>	fragments;
	SpecConstants		specConstants;

	fragments["decoration"]	= decorations2;
	fragments["pre_main"]	= typesAndConstants2;
	fragments["testfun"]	= function2;

	specConstants.append<deInt32>(56789);
	specConstants.append<deInt32>(-2);
	specConstants.append<deInt32>(56788);

	createTestsForAllStages("vector_related", inputColors, outputColors2, fragments, specConstants, group.get());

	return group.release();
}

tcu::TestCaseGroup* createOpPhiTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group				(new tcu::TestCaseGroup(testCtx, "opphi", "Test the OpPhi instruction"));
	RGBA							inputColors[4];
	RGBA							outputColors1[4];
	RGBA							outputColors2[4];
	RGBA							outputColors3[4];
	RGBA							outputColors4[4];
	map<string, string>				fragments1;
	map<string, string>				fragments2;
	map<string, string>				fragments3;
	map<string, string>				fragments4;
	std::vector<std::string>		extensions4;
	GraphicsResources				resources4;
	VulkanFeatures					vulkanFeatures4;

	const char	typesAndConstants1[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n"
		"%c_f32_p4  = OpConstant %f32 0.4\n"
		"%c_f32_p5  = OpConstant %f32 0.5\n"
		"%c_f32_p8  = OpConstant %f32 0.8\n";

	// vec4 test_code(vec4 param) {
	//   vec4 result = param;
	//   for (int i = 0; i < 4; ++i) {
	//     float operand;
	//     switch (i) {
	//       case 0: operand = .2; break;
	//       case 1: operand = .5; break;
	//       case 2: operand = .4; break;
	//       case 3: operand = .0; break;
	//       default: break; // unreachable
	//     }
	//     result[i] += operand;
	//   }
	//   return result;
	// }
	const char	function1[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1    = OpFunctionParameter %v4f32\n"
		"%lbl       = OpLabel\n"
		"%iptr      = OpVariable %fp_i32 Function\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %iptr %c_i32_0\n"
		"             OpStore %result %param1\n"
		"             OpBranch %loop\n"

		"%loop      = OpLabel\n"
		"%ival      = OpLoad %i32 %iptr\n"
		"%lt_4      = OpSLessThan %bool %ival %c_i32_4\n"
		"             OpLoopMerge %exit %cont None\n"
		"             OpBranchConditional %lt_4 %entry %exit\n"

		"%entry     = OpLabel\n"
		"%loc       = OpAccessChain %fp_f32 %result %ival\n"
		"%val       = OpLoad %f32 %loc\n"
		"             OpSelectionMerge %phi None\n"
		"             OpSwitch %ival %default 0 %case0 1 %case1 2 %case2 3 %case3\n"

		"%case0     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case1     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case2     = OpLabel\n"
		"             OpBranch %phi\n"
		"%case3     = OpLabel\n"
		"             OpBranch %phi\n"

		"%default   = OpLabel\n"
		"             OpUnreachable\n"

		"%phi       = OpLabel\n"
		"%operand   = OpPhi %f32 %c_f32_p4 %case2 %c_f32_p5 %case1 %c_f32_p2 %case0 %c_f32_0 %case3\n" // not in the order of blocks
		"             OpBranch %cont\n"
		"%cont      = OpLabel\n"
		"%add       = OpFAdd %f32 %val %operand\n"
		"             OpStore %loc %add\n"
		"%ival_next = OpIAdd %i32 %ival %c_i32_1\n"
		"             OpStore %iptr %ival_next\n"
		"             OpBranch %loop\n"

		"%exit      = OpLabel\n"
		"%ret       = OpLoad %v4f32 %result\n"
		"             OpReturnValue %ret\n"

		"             OpFunctionEnd\n";

	fragments1["pre_main"]	= typesAndConstants1;
	fragments1["testfun"]	= function1;

	getHalfColorsFullAlpha(inputColors);

	outputColors1[0]		= RGBA(178, 255, 229, 255);
	outputColors1[1]		= RGBA(178, 127, 102, 255);
	outputColors1[2]		= RGBA(51,  255, 102, 255);
	outputColors1[3]		= RGBA(51,  127, 229, 255);

	createTestsForAllStages("out_of_order", inputColors, outputColors1, fragments1, group.get());

	const char	typesAndConstants2[]	=
		"%c_f32_p2  = OpConstant %f32 0.2\n";

	// Add .4 to the second element of the given parameter.
	const char	function2[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%loc       = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%val       = OpLoad %f32 %loc\n"
		"             OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%step       = OpPhi %i32 %c_i32_0  %entry %step_next  %phi\n"
		"%accum      = OpPhi %f32 %val      %entry %accum_next %phi\n"
		"%step_next  = OpIAdd %i32 %step  %c_i32_1\n"
		"%accum_next = OpFAdd %f32 %accum %c_f32_p2\n"
		"%still_loop = OpSLessThan %bool %step %c_i32_2\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %loc %accum\n"
		"%ret        = OpLoad %v4f32 %result\n"
		"              OpReturnValue %ret\n"

		"              OpFunctionEnd\n";

	fragments2["pre_main"]	= typesAndConstants2;
	fragments2["testfun"]	= function2;

	outputColors2[0]			= RGBA(127, 229, 127, 255);
	outputColors2[1]			= RGBA(127, 102, 0,   255);
	outputColors2[2]			= RGBA(0,   229, 0,   255);
	outputColors2[3]			= RGBA(0,   102, 127, 255);

	createTestsForAllStages("induction", inputColors, outputColors2, fragments2, group.get());

	const char	typesAndConstants3[]	=
		"%true      = OpConstantTrue %bool\n"
		"%false     = OpConstantFalse %bool\n"
		"%c_f32_p2  = OpConstant %f32 0.2\n";

	// Swap the second and the third element of the given parameter.
	const char	function3[]				=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%entry     = OpLabel\n"
		"%result    = OpVariable %fp_v4f32 Function\n"
		"             OpStore %result %param\n"
		"%a_loc     = OpAccessChain %fp_f32 %result %c_i32_1\n"
		"%a_init    = OpLoad %f32 %a_loc\n"
		"%b_loc     = OpAccessChain %fp_f32 %result %c_i32_2\n"
		"%b_init    = OpLoad %f32 %b_loc\n"
		"             OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%still_loop = OpPhi %bool %true   %entry %false  %phi\n"
		"%a_next     = OpPhi %f32  %a_init %entry %b_next %phi\n"
		"%b_next     = OpPhi %f32  %b_init %entry %a_next %phi\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %a_loc %a_next\n"
		"              OpStore %b_loc %b_next\n"
		"%ret        = OpLoad %v4f32 %result\n"
		"              OpReturnValue %ret\n"

		"              OpFunctionEnd\n";

	fragments3["pre_main"]	= typesAndConstants3;
	fragments3["testfun"]	= function3;

	outputColors3[0]			= RGBA(127, 127, 127, 255);
	outputColors3[1]			= RGBA(127, 0,   0,   255);
	outputColors3[2]			= RGBA(0,   0,   127, 255);
	outputColors3[3]			= RGBA(0,   127, 0,   255);

	createTestsForAllStages("swap", inputColors, outputColors3, fragments3, group.get());

	const char	typesAndConstants4[]	=
		"%f16        = OpTypeFloat 16\n"
		"%v4f16      = OpTypeVector %f16 4\n"
		"%fp_f16     = OpTypePointer Function %f16\n"
		"%fp_v4f16   = OpTypePointer Function %v4f16\n"
		"%true       = OpConstantTrue %bool\n"
		"%false      = OpConstantFalse %bool\n"
		"%c_f32_p2   = OpConstant %f32 0.2\n";

	// Swap the second and the third element of the given parameter.
	const char	function4[]				=
		"%test_code  = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param      = OpFunctionParameter %v4f32\n"
		"%entry      = OpLabel\n"
		"%result     = OpVariable %fp_v4f16 Function\n"
		"%param16    = OpFConvert %v4f16 %param\n"
		"              OpStore %result %param16\n"
		"%a_loc      = OpAccessChain %fp_f16 %result %c_i32_1\n"
		"%a_init     = OpLoad %f16 %a_loc\n"
		"%b_loc      = OpAccessChain %fp_f16 %result %c_i32_2\n"
		"%b_init     = OpLoad %f16 %b_loc\n"
		"              OpBranch %phi\n"

		"%phi        = OpLabel\n"
		"%still_loop = OpPhi %bool %true   %entry %false  %phi\n"
		"%a_next     = OpPhi %f16  %a_init %entry %b_next %phi\n"
		"%b_next     = OpPhi %f16  %b_init %entry %a_next %phi\n"
		"              OpLoopMerge %exit %phi None\n"
		"              OpBranchConditional %still_loop %phi %exit\n"

		"%exit       = OpLabel\n"
		"              OpStore %a_loc %a_next\n"
		"              OpStore %b_loc %b_next\n"
		"%ret16      = OpLoad %v4f16 %result\n"
		"%ret        = OpFConvert %v4f32 %ret16\n"
		"              OpReturnValue %ret\n"

		"              OpFunctionEnd\n";

	fragments4["pre_main"]		= typesAndConstants4;
	fragments4["testfun"]		= function4;
	fragments4["capability"]	= "OpCapability Float16\n";

	extensions4.push_back("VK_KHR_shader_float16_int8");

	vulkanFeatures4.extFloat16Int8	= EXTFLOAT16INT8FEATURES_FLOAT16;

	outputColors4[0]			= RGBA(127, 127, 127, 255);
	outputColors4[1]			= RGBA(127, 0,   0,   255);
	outputColors4[2]			= RGBA(0,   0,   127, 255);
	outputColors4[3]			= RGBA(0,   127, 0,   255);

	createTestsForAllStages("swap16", inputColors, outputColors4, fragments4, resources4, extensions4, group.get(), vulkanFeatures4);

	return group.release();
}

tcu::TestCaseGroup* createNoContractionTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group			(new tcu::TestCaseGroup(testCtx, "nocontraction", "Test the NoContraction decoration"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];

	// With NoContraction, (1 + 2^-23) * (1 - 2^-23) - 1 should be conducted as a multiplication and an addition separately.
	// For the multiplication, the result is 1 - 2^-46, which is out of the precision range for 32-bit float. (32-bit float
	// only have 23-bit fraction.) So it will be rounded to 1. Or 0x1.fffffc. Then the final result is 0 or -0x1p-24.
	// On the contrary, the result will be 2^-46, which is a normalized number perfectly representable as 32-bit float.
	const char						constantsAndTypes[]	 =
		"%c_vec4_0       = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_0 %c_f32_1\n"
		"%c_vec4_1       = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"
		"%c_f32_1pl2_23  = OpConstant %f32 0x1.000002p+0\n" // 1 + 2^-23
		"%c_f32_1mi2_23  = OpConstant %f32 0x1.fffffcp-1\n" // 1 - 2^-23
		"%c_f32_n1pn24   = OpConstant %f32 -0x1p-24\n";

	const char						function[]	 =
		"%test_code      = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param          = OpFunctionParameter %v4f32\n"
		"%label          = OpLabel\n"
		"%var1           = OpVariable %fp_f32 Function %c_f32_1pl2_23\n"
		"%var2           = OpVariable %fp_f32 Function\n"
		"%red            = OpCompositeExtract %f32 %param 0\n"
		"%plus_red       = OpFAdd %f32 %c_f32_1mi2_23 %red\n"
		"                  OpStore %var2 %plus_red\n"
		"%val1           = OpLoad %f32 %var1\n"
		"%val2           = OpLoad %f32 %var2\n"
		"%mul            = OpFMul %f32 %val1 %val2\n"
		"%add            = OpFAdd %f32 %mul %c_f32_n1\n"
		"%is0            = OpFOrdEqual %bool %add %c_f32_0\n"
		"%isn1n24         = OpFOrdEqual %bool %add %c_f32_n1pn24\n"
		"%success        = OpLogicalOr %bool %is0 %isn1n24\n"
		"%v4success      = OpCompositeConstruct %v4bool %success %success %success %success\n"
		"%ret            = OpSelect %v4f32 %v4success %c_vec4_0 %c_vec4_1\n"
		"                  OpReturnValue %ret\n"
		"                  OpFunctionEnd\n";

	struct CaseNameDecoration
	{
		string name;
		string decoration;
	};


	CaseNameDecoration tests[] = {
		{"multiplication",	"OpDecorate %mul NoContraction"},
		{"addition",		"OpDecorate %add NoContraction"},
		{"both",			"OpDecorate %mul NoContraction\nOpDecorate %add NoContraction"},
	};

	getHalfColorsFullAlpha(inputColors);

	for (deUint8 idx = 0; idx < 4; ++idx)
	{
		inputColors[idx].setRed(0);
		outputColors[idx] = RGBA(0, 0, 0, 255);
	}

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(CaseNameDecoration); ++testNdx)
	{
		map<string, string> fragments;

		fragments["decoration"] = tests[testNdx].decoration;
		fragments["pre_main"] = constantsAndTypes;
		fragments["testfun"] = function;

		createTestsForAllStages(tests[testNdx].name, inputColors, outputColors, fragments, group.get());
	}

	return group.release();
}

tcu::TestCaseGroup* createMemoryAccessTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> memoryAccessTests (new tcu::TestCaseGroup(testCtx, "opmemoryaccess", "Memory Semantics"));
	RGBA							colors[4];

	const char						constantsAndTypes[]	 =
		"%c_a2f32_1         = OpConstantComposite %a2f32 %c_f32_1 %c_f32_1\n"
		"%fp_a2f32          = OpTypePointer Function %a2f32\n"
		"%stype             = OpTypeStruct  %v4f32 %a2f32 %f32\n"
		"%fp_stype          = OpTypePointer Function %stype\n";

	const char						function[]	 =
		"%test_code         = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1            = OpFunctionParameter %v4f32\n"
		"%lbl               = OpLabel\n"
		"%v1                = OpVariable %fp_v4f32 Function\n"
		"%v2                = OpVariable %fp_a2f32 Function\n"
		"%v3                = OpVariable %fp_f32 Function\n"
		"%v                 = OpVariable %fp_stype Function\n"
		"%vv                = OpVariable %fp_stype Function\n"
		"%vvv               = OpVariable %fp_f32 Function\n"

		"                     OpStore %v1 %c_v4f32_1_1_1_1\n"
		"                     OpStore %v2 %c_a2f32_1\n"
		"                     OpStore %v3 %c_f32_1\n"

		"%p_v4f32          = OpAccessChain %fp_v4f32 %v %c_u32_0\n"
		"%p_a2f32          = OpAccessChain %fp_a2f32 %v %c_u32_1\n"
		"%p_f32            = OpAccessChain %fp_f32 %v %c_u32_2\n"
		"%v1_v             = OpLoad %v4f32 %v1 ${access_type}\n"
		"%v2_v             = OpLoad %a2f32 %v2 ${access_type}\n"
		"%v3_v             = OpLoad %f32 %v3 ${access_type}\n"

		"                    OpStore %p_v4f32 %v1_v ${access_type}\n"
		"                    OpStore %p_a2f32 %v2_v ${access_type}\n"
		"                    OpStore %p_f32 %v3_v ${access_type}\n"

		"                    OpCopyMemory %vv %v ${access_type}\n"
		"                    OpCopyMemory %vvv %p_f32 ${access_type}\n"

		"%p_f32_2          = OpAccessChain %fp_f32 %vv %c_u32_2\n"
		"%v_f32_2          = OpLoad %f32 %p_f32_2\n"
		"%v_f32_3          = OpLoad %f32 %vvv\n"

		"%ret1             = OpVectorTimesScalar %v4f32 %param1 %v_f32_2\n"
		"%ret2             = OpVectorTimesScalar %v4f32 %ret1 %v_f32_3\n"
		"                    OpReturnValue %ret2\n"
		"                    OpFunctionEnd\n";

	struct NameMemoryAccess
	{
		string name;
		string accessType;
	};


	NameMemoryAccess tests[] =
	{
		{ "none", "" },
		{ "volatile", "Volatile" },
		{ "aligned",  "Aligned 1" },
		{ "volatile_aligned",  "Volatile|Aligned 1" },
		{ "nontemporal_aligned",  "Nontemporal|Aligned 1" },
		{ "volatile_nontemporal",  "Volatile|Nontemporal" },
		{ "volatile_nontermporal_aligned",  "Volatile|Nontemporal|Aligned 1" },
	};

	getHalfColorsFullAlpha(colors);

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameMemoryAccess); ++testNdx)
	{
		map<string, string> fragments;
		map<string, string> memoryAccess;
		memoryAccess["access_type"] = tests[testNdx].accessType;

		fragments["pre_main"] = constantsAndTypes;
		fragments["testfun"] = tcu::StringTemplate(function).specialize(memoryAccess);
		createTestsForAllStages(tests[testNdx].name, colors, colors, fragments, memoryAccessTests.get());
	}
	return memoryAccessTests.release();
}
tcu::TestCaseGroup* createOpUndefTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		opUndefTests		 (new tcu::TestCaseGroup(testCtx, "opundef", "Test OpUndef"));
	RGBA								defaultColors[4];
	map<string, string>					fragments;
	getDefaultColors(defaultColors);

	// First, simple cases that don't do anything with the OpUndef result.
	struct NameCodePair { string name, decl, type; };
	const NameCodePair tests[] =
	{
		{"bool", "", "%bool"},
		{"vec2uint32", "", "%v2u32"},
		{"image", "%type = OpTypeImage %f32 2D 0 0 0 1 Unknown", "%type"},
		{"sampler", "%type = OpTypeSampler", "%type"},
		{"sampledimage", "%img = OpTypeImage %f32 2D 0 0 0 1 Unknown\n" "%type = OpTypeSampledImage %img", "%type"},
		{"pointer", "", "%fp_i32"},
		{"runtimearray", "%type = OpTypeRuntimeArray %f32", "%type"},
		{"array", "%c_u32_100 = OpConstant %u32 100\n" "%type = OpTypeArray %i32 %c_u32_100", "%type"},
		{"struct", "%type = OpTypeStruct %f32 %i32 %u32", "%type"}};
	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameCodePair); ++testNdx)
	{
		fragments["undef_type"] = tests[testNdx].type;
		fragments["testfun"] = StringTemplate(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"%param1 = OpFunctionParameter %v4f32\n"
			"%label_testfun = OpLabel\n"
			"%undef = OpUndef ${undef_type}\n"
			"OpReturnValue %param1\n"
			"OpFunctionEnd\n").specialize(fragments);
		fragments["pre_main"] = tests[testNdx].decl;
		createTestsForAllStages(tests[testNdx].name, defaultColors, defaultColors, fragments, opUndefTests.get());
	}
	fragments.clear();

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %f32\n"
		"%zero = OpFMul %f32 %undef %c_f32_0\n"
		"%is_nan = OpIsNan %bool %zero\n" //OpUndef may result in NaN which may turn %zero into Nan.
		"%actually_zero = OpSelect %f32 %is_nan %c_f32_0 %zero\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b = OpFAdd %f32 %a %actually_zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %b %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("float32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %i32\n"
		"%zero = OpIMul %i32 %undef %c_i32_0\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %a %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("sint32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %u32\n"
		"%zero = OpIMul %u32 %undef %c_i32_0\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %a %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("uint32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %v4f32\n"
		"%vzero = OpVectorTimesScalar %v4f32 %undef %c_f32_0\n"
		"%zero_0 = OpVectorExtractDynamic %f32 %vzero %c_i32_0\n"
		"%zero_1 = OpVectorExtractDynamic %f32 %vzero %c_i32_1\n"
		"%zero_2 = OpVectorExtractDynamic %f32 %vzero %c_i32_2\n"
		"%zero_3 = OpVectorExtractDynamic %f32 %vzero %c_i32_3\n"
		"%is_nan_0 = OpIsNan %bool %zero_0\n"
		"%is_nan_1 = OpIsNan %bool %zero_1\n"
		"%is_nan_2 = OpIsNan %bool %zero_2\n"
		"%is_nan_3 = OpIsNan %bool %zero_3\n"
		"%actually_zero_0 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_0\n"
		"%actually_zero_1 = OpSelect %f32 %is_nan_1 %c_f32_0 %zero_1\n"
		"%actually_zero_2 = OpSelect %f32 %is_nan_2 %c_f32_0 %zero_2\n"
		"%actually_zero_3 = OpSelect %f32 %is_nan_3 %c_f32_0 %zero_3\n"
		"%param1_0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%param1_1 = OpVectorExtractDynamic %f32 %param1 %c_i32_1\n"
		"%param1_2 = OpVectorExtractDynamic %f32 %param1 %c_i32_2\n"
		"%param1_3 = OpVectorExtractDynamic %f32 %param1 %c_i32_3\n"
		"%sum_0 = OpFAdd %f32 %param1_0 %actually_zero_0\n"
		"%sum_1 = OpFAdd %f32 %param1_1 %actually_zero_1\n"
		"%sum_2 = OpFAdd %f32 %param1_2 %actually_zero_2\n"
		"%sum_3 = OpFAdd %f32 %param1_3 %actually_zero_3\n"
		"%ret3 = OpVectorInsertDynamic %v4f32 %param1 %sum_3 %c_i32_3\n"
		"%ret2 = OpVectorInsertDynamic %v4f32 %ret3 %sum_2 %c_i32_2\n"
		"%ret1 = OpVectorInsertDynamic %v4f32 %ret2 %sum_1 %c_i32_1\n"
		"%ret = OpVectorInsertDynamic %v4f32 %ret1 %sum_0 %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("vec4float32", defaultColors, defaultColors, fragments, opUndefTests.get());

	fragments["pre_main"] =
		"%m2x2f32 = OpTypeMatrix %v2f32 2\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%undef = OpUndef %m2x2f32\n"
		"%mzero = OpMatrixTimesScalar %m2x2f32 %undef %c_f32_0\n"
		"%zero_0 = OpCompositeExtract %f32 %mzero 0 0\n"
		"%zero_1 = OpCompositeExtract %f32 %mzero 0 1\n"
		"%zero_2 = OpCompositeExtract %f32 %mzero 1 0\n"
		"%zero_3 = OpCompositeExtract %f32 %mzero 1 1\n"
		"%is_nan_0 = OpIsNan %bool %zero_0\n"
		"%is_nan_1 = OpIsNan %bool %zero_1\n"
		"%is_nan_2 = OpIsNan %bool %zero_2\n"
		"%is_nan_3 = OpIsNan %bool %zero_3\n"
		"%actually_zero_0 = OpSelect %f32 %is_nan_0 %c_f32_0 %zero_0\n"
		"%actually_zero_1 = OpSelect %f32 %is_nan_1 %c_f32_0 %zero_1\n"
		"%actually_zero_2 = OpSelect %f32 %is_nan_2 %c_f32_0 %zero_2\n"
		"%actually_zero_3 = OpSelect %f32 %is_nan_3 %c_f32_0 %zero_3\n"
		"%param1_0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%param1_1 = OpVectorExtractDynamic %f32 %param1 %c_i32_1\n"
		"%param1_2 = OpVectorExtractDynamic %f32 %param1 %c_i32_2\n"
		"%param1_3 = OpVectorExtractDynamic %f32 %param1 %c_i32_3\n"
		"%sum_0 = OpFAdd %f32 %param1_0 %actually_zero_0\n"
		"%sum_1 = OpFAdd %f32 %param1_1 %actually_zero_1\n"
		"%sum_2 = OpFAdd %f32 %param1_2 %actually_zero_2\n"
		"%sum_3 = OpFAdd %f32 %param1_3 %actually_zero_3\n"
		"%ret3 = OpVectorInsertDynamic %v4f32 %param1 %sum_3 %c_i32_3\n"
		"%ret2 = OpVectorInsertDynamic %v4f32 %ret3 %sum_2 %c_i32_2\n"
		"%ret1 = OpVectorInsertDynamic %v4f32 %ret2 %sum_1 %c_i32_1\n"
		"%ret = OpVectorInsertDynamic %v4f32 %ret1 %sum_0 %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("matrix", defaultColors, defaultColors, fragments, opUndefTests.get());

	return opUndefTests.release();
}

void createOpQuantizeSingleOptionTests(tcu::TestCaseGroup* testCtx)
{
	const RGBA		inputColors[4]		=
	{
		RGBA(0,		0,		0,		255),
		RGBA(0,		0,		255,	255),
		RGBA(0,		255,	0,		255),
		RGBA(0,		255,	255,	255)
	};

	const RGBA		expectedColors[4]	=
	{
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255)
	};

	const struct SingleFP16Possibility
	{
		const char* name;
		const char* constant;  // Value to assign to %test_constant.
		float		valueAsFloat;
		const char* condition; // Must assign to %cond an expression that evaluates to true after %c = OpQuantizeToF16(%test_constant + 0).
	}				tests[]				=
	{
		{
			"negative",
			"-0x1.3p1\n",
			-constructNormalizedFloat(1, 0x300000),
			"%cond = OpFOrdEqual %bool %c %test_constant\n"
		}, // -19
		{
			"positive",
			"0x1.0p7\n",
			constructNormalizedFloat(7, 0x000000),
			"%cond = OpFOrdEqual %bool %c %test_constant\n"
		},  // +128
		// SPIR-V requires that OpQuantizeToF16 flushes
		// any numbers that would end up denormalized in F16 to zero.
		{
			"denorm",
			"0x0.0006p-126\n",
			std::ldexp(1.5f, -140),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},  // denorm
		{
			"negative_denorm",
			"-0x0.0006p-126\n",
			-std::ldexp(1.5f, -140),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		}, // -denorm
		{
			"too_small",
			"0x1.0p-16\n",
			std::ldexp(1.0f, -16),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},     // too small positive
		{
			"negative_too_small",
			"-0x1.0p-32\n",
			-std::ldexp(1.0f, -32),
			"%cond = OpFOrdEqual %bool %c %c_f32_0\n"
		},      // too small negative
		{
			"negative_inf",
			"-0x1.0p128\n",
			-std::ldexp(1.0f, 128),

			"%gz = OpFOrdLessThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // -inf to -inf
		{
			"inf",
			"0x1.0p128\n",
			std::ldexp(1.0f, 128),

			"%gz = OpFOrdGreaterThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // +inf to +inf
		{
			"round_to_negative_inf",
			"-0x1.0p32\n",
			-std::ldexp(1.0f, 32),

			"%gz = OpFOrdLessThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // round to -inf
		{
			"round_to_inf",
			"0x1.0p16\n",
			std::ldexp(1.0f, 16),

			"%gz = OpFOrdGreaterThan %bool %c %c_f32_0\n"
			"%inf = OpIsInf %bool %c\n"
			"%cond = OpLogicalAnd %bool %gz %inf\n"
		},     // round to +inf
		{
			"nan",
			"0x1.1p128\n",
			std::numeric_limits<float>::quiet_NaN(),

			// Test for any NaN value, as NaNs are not preserved
			"%direct_quant = OpQuantizeToF16 %f32 %test_constant\n"
			"%cond = OpIsNan %bool %direct_quant\n"
		}, // nan
		{
			"negative_nan",
			"-0x1.0001p128\n",
			std::numeric_limits<float>::quiet_NaN(),

			// Test for any NaN value, as NaNs are not preserved
			"%direct_quant = OpQuantizeToF16 %f32 %test_constant\n"
			"%cond = OpIsNan %bool %direct_quant\n"
		} // -nan
	};
	const char*		constants			=
		"%test_constant = OpConstant %f32 ";  // The value will be test.constant.

	StringTemplate	function			(
		"%test_code     = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%a             = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b             = OpFAdd %f32 %test_constant %a\n"
		"%c             = OpQuantizeToF16 %f32 %b\n"
		"${condition}\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1\n"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n"
	);

	const char*		specDecorations		= "OpDecorate %test_constant SpecId 0\n";
	const char*		specConstants		=
			"%test_constant = OpSpecConstant %f32 0.\n"
			"%c             = OpSpecConstantOp %f32 QuantizeToF16 %test_constant\n";

	StringTemplate	specConstantFunction(
		"%test_code     = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"${condition}\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1\n"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n"
	);

	for (size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx)
	{
		map<string, string>								codeSpecialization;
		map<string, string>								fragments;
		codeSpecialization["condition"]					= tests[idx].condition;
		fragments["testfun"]							= function.specialize(codeSpecialization);
		fragments["pre_main"]							= string(constants) + tests[idx].constant + "\n";
		createTestsForAllStages(tests[idx].name, inputColors, expectedColors, fragments, testCtx);
	}

	for (size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx)
	{
		map<string, string>								codeSpecialization;
		map<string, string>								fragments;
		SpecConstants									passConstants;

		codeSpecialization["condition"]					= tests[idx].condition;
		fragments["testfun"]							= specConstantFunction.specialize(codeSpecialization);
		fragments["decoration"]							= specDecorations;
		fragments["pre_main"]							= specConstants;

		passConstants.append<float>(tests[idx].valueAsFloat);

		createTestsForAllStages(string("spec_const_") + tests[idx].name, inputColors, expectedColors, fragments, passConstants, testCtx);
	}
}

void createOpQuantizeTwoPossibilityTests(tcu::TestCaseGroup* testCtx)
{
	RGBA inputColors[4] =  {
		RGBA(0,		0,		0,		255),
		RGBA(0,		0,		255,	255),
		RGBA(0,		255,	0,		255),
		RGBA(0,		255,	255,	255)
	};

	RGBA expectedColors[4] =
	{
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255),
		RGBA(255,	 0,		 0,		 255)
	};

	struct DualFP16Possibility
	{
		const char* name;
		const char* input;
		float		inputAsFloat;
		const char* possibleOutput1;
		const char* possibleOutput2;
	} tests[] = {
		{
			"positive_round_up_or_round_down",
			"0x1.3003p8",
			constructNormalizedFloat(8, 0x300300),
			"0x1.304p8",
			"0x1.3p8"
		},
		{
			"negative_round_up_or_round_down",
			"-0x1.6008p-7",
			-constructNormalizedFloat(-7, 0x600800),
			"-0x1.6p-7",
			"-0x1.604p-7"
		},
		{
			"carry_bit",
			"0x1.01ep2",
			constructNormalizedFloat(2, 0x01e000),
			"0x1.01cp2",
			"0x1.02p2"
		},
		{
			"carry_to_exponent",
			"0x1.ffep1",
			constructNormalizedFloat(1, 0xffe000),
			"0x1.ffcp1",
			"0x1.0p2"
		},
	};
	StringTemplate constants (
		"%input_const = OpConstant %f32 ${input}\n"
		"%possible_solution1 = OpConstant %f32 ${output1}\n"
		"%possible_solution2 = OpConstant %f32 ${output2}\n"
		);

	StringTemplate specConstants (
		"%input_const = OpSpecConstant %f32 0.\n"
		"%possible_solution1 = OpConstant %f32 ${output1}\n"
		"%possible_solution2 = OpConstant %f32 ${output2}\n"
	);

	const char* specDecorations = "OpDecorate %input_const  SpecId 0\n";

	const char* function  =
		"%test_code     = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1        = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%a             = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		// For the purposes of this test we assume that 0.f will always get
		// faithfully passed through the pipeline stages.
		"%b             = OpFAdd %f32 %input_const %a\n"
		"%c             = OpQuantizeToF16 %f32 %b\n"
		"%eq_1          = OpFOrdEqual %bool %c %possible_solution1\n"
		"%eq_2          = OpFOrdEqual %bool %c %possible_solution2\n"
		"%cond          = OpLogicalOr %bool %eq_1 %eq_2\n"
		"%v4cond        = OpCompositeConstruct %v4bool %cond %cond %cond %cond\n"
		"%retval        = OpSelect %v4f32 %v4cond %c_v4f32_1_0_0_1 %param1"
		"                 OpReturnValue %retval\n"
		"OpFunctionEnd\n";

	for(size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx) {
		map<string, string>									fragments;
		map<string, string>									constantSpecialization;

		constantSpecialization["input"]						= tests[idx].input;
		constantSpecialization["output1"]					= tests[idx].possibleOutput1;
		constantSpecialization["output2"]					= tests[idx].possibleOutput2;
		fragments["testfun"]								= function;
		fragments["pre_main"]								= constants.specialize(constantSpecialization);
		createTestsForAllStages(tests[idx].name, inputColors, expectedColors, fragments, testCtx);
	}

	for(size_t idx = 0; idx < (sizeof(tests)/sizeof(tests[0])); ++idx) {
		map<string, string>									fragments;
		map<string, string>									constantSpecialization;
		SpecConstants										passConstants;

		constantSpecialization["output1"]					= tests[idx].possibleOutput1;
		constantSpecialization["output2"]					= tests[idx].possibleOutput2;
		fragments["testfun"]								= function;
		fragments["decoration"]								= specDecorations;
		fragments["pre_main"]								= specConstants.specialize(constantSpecialization);

		passConstants.append<float>(tests[idx].inputAsFloat);

		createTestsForAllStages(string("spec_const_") + tests[idx].name, inputColors, expectedColors, fragments, passConstants, testCtx);
	}
}

tcu::TestCaseGroup* createOpQuantizeTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opQuantizeTests (new tcu::TestCaseGroup(testCtx, "opquantize", "Test OpQuantizeToF16"));
	createOpQuantizeSingleOptionTests(opQuantizeTests.get());
	createOpQuantizeTwoPossibilityTests(opQuantizeTests.get());
	return opQuantizeTests.release();
}

struct ShaderPermutation
{
	deUint8 vertexPermutation;
	deUint8 geometryPermutation;
	deUint8 tesscPermutation;
	deUint8 tessePermutation;
	deUint8 fragmentPermutation;
};

ShaderPermutation getShaderPermutation(deUint8 inputValue)
{
	ShaderPermutation	permutation =
	{
		static_cast<deUint8>(inputValue & 0x10? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x08? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x04? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x02? 1u: 0u),
		static_cast<deUint8>(inputValue & 0x01? 1u: 0u)
	};
	return permutation;
}

tcu::TestCaseGroup* createModuleTests(tcu::TestContext& testCtx)
{
	RGBA								defaultColors[4];
	RGBA								invertedColors[4];
	de::MovePtr<tcu::TestCaseGroup>		moduleTests			(new tcu::TestCaseGroup(testCtx, "module", "Multiple entry points into shaders"));

	getDefaultColors(defaultColors);
	getInvertedDefaultColors(invertedColors);

	// Combined module tests
	{
		// Shader stages: vertex and fragment
		{
			const ShaderElement combinedPipeline[]	=
			{
				ShaderElement("module", "main", VK_SHADER_STAGE_VERTEX_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_FRAGMENT_BIT)
			};

			addFunctionCaseWithPrograms<InstanceContext>(
				moduleTests.get(), "same_module", "", createCombinedModule, runAndVerifyDefaultPipeline,
				createInstanceContext(combinedPipeline, map<string, string>()));
		}

		// Shader stages: vertex, geometry and fragment
		{
			const ShaderElement combinedPipeline[]	=
			{
				ShaderElement("module", "main", VK_SHADER_STAGE_VERTEX_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_FRAGMENT_BIT)
			};

			addFunctionCaseWithPrograms<InstanceContext>(
				moduleTests.get(), "same_module_geom", "", createCombinedModule, runAndVerifyDefaultPipeline,
				createInstanceContext(combinedPipeline, map<string, string>()));
		}

		// Shader stages: vertex, tessellation control, tessellation evaluation and fragment
		{
			const ShaderElement combinedPipeline[]	=
			{
				ShaderElement("module", "main", VK_SHADER_STAGE_VERTEX_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_FRAGMENT_BIT)
			};

			addFunctionCaseWithPrograms<InstanceContext>(
				moduleTests.get(), "same_module_tessc_tesse", "", createCombinedModule, runAndVerifyDefaultPipeline,
				createInstanceContext(combinedPipeline, map<string, string>()));
		}

		// Shader stages: vertex, tessellation control, tessellation evaluation, geometry and fragment
		{
			const ShaderElement combinedPipeline[]	=
			{
				ShaderElement("module", "main", VK_SHADER_STAGE_VERTEX_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_GEOMETRY_BIT),
				ShaderElement("module", "main", VK_SHADER_STAGE_FRAGMENT_BIT)
			};

			addFunctionCaseWithPrograms<InstanceContext>(
				moduleTests.get(), "same_module_tessc_tesse_geom", "", createCombinedModule, runAndVerifyDefaultPipeline,
				createInstanceContext(combinedPipeline, map<string, string>()));
		}
	}

	const char* numbers[] =
	{
		"1", "2"
	};

	for (deInt8 idx = 0; idx < 32; ++idx)
	{
		ShaderPermutation			permutation		= getShaderPermutation(idx);
		string						name			= string("vert") + numbers[permutation.vertexPermutation] + "_geom" + numbers[permutation.geometryPermutation] + "_tessc" + numbers[permutation.tesscPermutation] + "_tesse" + numbers[permutation.tessePermutation] + "_frag" + numbers[permutation.fragmentPermutation];
		const ShaderElement			pipeline[]		=
		{
			ShaderElement("vert",	string("vert") +	numbers[permutation.vertexPermutation],		VK_SHADER_STAGE_VERTEX_BIT),
			ShaderElement("geom",	string("geom") +	numbers[permutation.geometryPermutation],	VK_SHADER_STAGE_GEOMETRY_BIT),
			ShaderElement("tessc",	string("tessc") +	numbers[permutation.tesscPermutation],		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
			ShaderElement("tesse",	string("tesse") +	numbers[permutation.tessePermutation],		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
			ShaderElement("frag",	string("frag") +	numbers[permutation.fragmentPermutation],	VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		// If there are an even number of swaps, then it should be no-op.
		// If there are an odd number, the color should be flipped.
		if ((permutation.vertexPermutation + permutation.geometryPermutation + permutation.tesscPermutation + permutation.tessePermutation + permutation.fragmentPermutation) % 2 == 0)
		{
			addFunctionCaseWithPrograms<InstanceContext>(
					moduleTests.get(), name, "", createMultipleEntries, runAndVerifyDefaultPipeline,
					createInstanceContext(pipeline, defaultColors, defaultColors, map<string, string>()));
		}
		else
		{
			addFunctionCaseWithPrograms<InstanceContext>(
					moduleTests.get(), name, "", createMultipleEntries, runAndVerifyDefaultPipeline,
					createInstanceContext(pipeline, defaultColors, invertedColors, map<string, string>()));
		}
	}
	return moduleTests.release();
}

std::string getUnusedVarTestNamePiece(const std::string& prefix, ShaderTask task)
{
	switch (task)
	{
		case SHADER_TASK_NONE:			return "";
		case SHADER_TASK_NORMAL:		return prefix + "_normal";
		case SHADER_TASK_UNUSED_VAR:	return prefix + "_unused_var";
		case SHADER_TASK_UNUSED_FUNC:	return prefix + "_unused_func";
		default:						DE_ASSERT(DE_FALSE);
	}
	// unreachable
	return "";
}

std::string getShaderTaskIndexName(ShaderTaskIndex index)
{
	switch (index)
	{
	case SHADER_TASK_INDEX_VERTEX:			return "vertex";
	case SHADER_TASK_INDEX_GEOMETRY:		return "geom";
	case SHADER_TASK_INDEX_TESS_CONTROL:	return "tessc";
	case SHADER_TASK_INDEX_TESS_EVAL:		return "tesse";
	case SHADER_TASK_INDEX_FRAGMENT:		return "frag";
	default:								DE_ASSERT(DE_FALSE);
	}
	// unreachable
	return "";
}

std::string getUnusedVarTestName(const ShaderTaskArray& shaderTasks, const VariableLocation& location)
{
	std::string testName = location.toString();

	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(shaderTasks); ++i)
	{
		if (shaderTasks[i] != SHADER_TASK_NONE)
		{
			testName += "_" + getUnusedVarTestNamePiece(getShaderTaskIndexName((ShaderTaskIndex)i), shaderTasks[i]);
		}
	}

	return testName;
}

tcu::TestCaseGroup* createUnusedVariableTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		moduleTests				(new tcu::TestCaseGroup(testCtx, "unused_variables", "Graphics shaders with unused variables"));

	ShaderTaskArray						shaderCombinations[]	=
	{
		// Vertex					Geometry					Tess. Control				Tess. Evaluation			Fragment
		{ SHADER_TASK_UNUSED_VAR,	SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_UNUSED_FUNC,	SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_UNUSED_VAR	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_UNUSED_FUNC	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_UNUSED_VAR,		SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_UNUSED_FUNC,	SHADER_TASK_NONE,			SHADER_TASK_NONE,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_UNUSED_VAR,		SHADER_TASK_NORMAL,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_UNUSED_FUNC,	SHADER_TASK_NORMAL,			SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_NORMAL,			SHADER_TASK_UNUSED_VAR,		SHADER_TASK_NORMAL	},
		{ SHADER_TASK_NORMAL,		SHADER_TASK_NONE,			SHADER_TASK_NORMAL,			SHADER_TASK_UNUSED_FUNC,	SHADER_TASK_NORMAL	}
	};

	const VariableLocation				testLocations[] =
	{
		// Set		Binding
		{ 0,		5			},
		{ 5,		5			},
	};

	for (size_t combNdx = 0; combNdx < DE_LENGTH_OF_ARRAY(shaderCombinations); ++combNdx)
	{
		for (size_t locationNdx = 0; locationNdx < DE_LENGTH_OF_ARRAY(testLocations); ++locationNdx)
		{
			const ShaderTaskArray&	shaderTasks		= shaderCombinations[combNdx];
			const VariableLocation&	location		= testLocations[locationNdx];
			std::string				testName		= getUnusedVarTestName(shaderTasks, location);

			addFunctionCaseWithPrograms<UnusedVariableContext>(
				moduleTests.get(), testName, "", createUnusedVariableModules, runAndVerifyUnusedVariablePipeline,
				createUnusedVariableContext(shaderTasks, location));
		}
	}

	return moduleTests.release();
}

tcu::TestCaseGroup* createLoopTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "loop", "Looping control flow"));
	RGBA defaultColors[4];
	getDefaultColors(defaultColors);
	map<string, string> fragments;
	fragments["pre_main"] =
		"%c_f32_5 = OpConstant %f32 5.\n";

	// A loop with a single block. The Continue Target is the loop block
	// itself. In SPIR-V terms, the "loop construct" contains no blocks at all
	// -- the "continue construct" forms the entire loop.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds and subtracts 1.0 to %val in alternate iterations\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %loop\n"
		"%delta = OpPhi %f32 %c_f32_1 %entry %minus_delta %loop\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %loop\n"
		"%val = OpFAdd %f32 %val1 %delta\n"
		"%minus_delta = OpFSub %f32 %c_f32_0 %delta\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpLoopMerge %exit %loop None\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val %c_i32_0\n"
		"OpReturnValue %result\n"

		"OpFunctionEnd\n";

	createTestsForAllStages("single_block", defaultColors, defaultColors, fragments, testGroup.get());

	// Body comprised of multiple basic blocks.
	const StringTemplate multiBlock(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds and subtracts 1.0 to %val in alternate iterations\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %cont\n"
		"%delta = OpPhi %f32 %c_f32_1 %entry %delta_next %cont\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %cont\n"
		// There are several possibilities for the Continue Target below.  Each
		// will be specialized into a separate test case.
		"OpLoopMerge %exit ${continue_target} None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";delta_next = (delta > 0) ? -1 : 1;\n"
		"%gt0 = OpFOrdGreaterThan %bool %delta %c_f32_0\n"
		"OpSelectionMerge %gather DontFlatten\n"
		"OpBranchConditional %gt0 %even %odd ;tells us if %count is even or odd\n"

		"%odd = OpLabel\n"
		"OpBranch %gather\n"

		"%even = OpLabel\n"
		"OpBranch %gather\n"

		"%gather = OpLabel\n"
		"%delta_next = OpPhi %f32 %c_f32_n1 %even %c_f32_1 %odd\n"
		"%val = OpFAdd %f32 %val1 %delta\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"OpBranch %cont\n"

		"%cont = OpLabel\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val %c_i32_0\n"
		"OpReturnValue %result\n"

		"OpFunctionEnd\n");

	map<string, string> continue_target;

	// The Continue Target is the loop block itself.
	continue_target["continue_target"] = "%loop";
	fragments["testfun"] = multiBlock.specialize(continue_target);
	createTestsForAllStages("multi_block_continue_construct", defaultColors, defaultColors, fragments, testGroup.get());

	// The Continue Target is at the end of the loop.
	continue_target["continue_target"] = "%cont";
	fragments["testfun"] = multiBlock.specialize(continue_target);
	createTestsForAllStages("multi_block_loop_construct", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with continue statement.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4, 3, and 1 to %val0 (skips 2)\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";skip if %count==2\n"
		"%eq2 = OpIEqual %bool %count %c_i32_2\n"
		"OpBranchConditional %eq2 %continue %body\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%val = OpPhi %f32 %val2 %body %val1 %if\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%same = OpFSub %f32 %val %c_f32_8\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("continue", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with break.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n"
		"%two = OpIAdd %i32 %zero %c_i32_2\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4 and 3 to %val0 (exits early)\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val2 %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";end loop if %count==%two\n"
		"%above2 = OpSGreaterThan %bool %count %two\n"
		"OpBranchConditional %above2 %body %exit\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%val_post = OpPhi %f32 %val2 %continue %val1 %if\n"
		"%same = OpFSub %f32 %val_post %c_f32_7\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("break", defaultColors, defaultColors, fragments, testGroup.get());

	// A loop with return.
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n"
		"%two = OpIAdd %i32 %zero %c_i32_2\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";returns early without modifying %param1\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %continue\n"
		"%val1 = OpPhi %f32 %val0 %entry %val2 %continue\n"
		"OpLoopMerge %exit %continue None\n"
		"OpBranch %if\n"

		"%if = OpLabel\n"
		";return if %count==%two\n"
		"%above2 = OpSGreaterThan %bool %count %two\n"
		"OpSelectionMerge %body DontFlatten\n"
		"OpBranchConditional %above2 %body %early_exit\n"

		"%early_exit = OpLabel\n"
		"OpReturnValue %param1\n"

		"%body = OpLabel\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val2 = OpFAdd %f32 %val1 %fcount\n"
		"OpBranch %continue\n"

		"%continue = OpLabel\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		";should never get here, so return an incorrect result\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val2 %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("return", defaultColors, defaultColors, fragments, testGroup.get());

	// Continue inside a switch block to break to enclosing loop's merge block.
	// Matches roughly the following GLSL code:
	// for (; keep_going; keep_going = false)
	// {
	//     switch (int(param1.x))
	//     {
	//         case 0: continue;
	//         case 1: continue;
	//         default: continue;
	//     }
	//     dead code: modify return value to invalid result.
	// }
	fragments["pre_main"] =
		"%fp_bool = OpTypePointer Function %bool\n"
		"%true = OpConstantTrue %bool\n"
		"%false = OpConstantFalse %bool\n";

	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"

		"%entry = OpLabel\n"
		"%keep_going = OpVariable %fp_bool Function\n"
		"%val_ptr = OpVariable %fp_f32 Function\n"
		"%param1_x = OpCompositeExtract %f32 %param1 0\n"
		"OpStore %keep_going %true\n"
		"OpBranch %forloop_begin\n"

		"%forloop_begin = OpLabel\n"
		"OpLoopMerge %forloop_merge %forloop_continue None\n"
		"OpBranch %forloop\n"

		"%forloop = OpLabel\n"
		"%for_condition = OpLoad %bool %keep_going\n"
		"OpBranchConditional %for_condition %forloop_body %forloop_merge\n"

		"%forloop_body = OpLabel\n"
		"OpStore %val_ptr %param1_x\n"
		"%param1_x_int = OpConvertFToS %i32 %param1_x\n"

		"OpSelectionMerge %switch_merge None\n"
		"OpSwitch %param1_x_int %default 0 %case_0 1 %case_1\n"
		"%case_0 = OpLabel\n"
		"OpBranch %forloop_continue\n"
		"%case_1 = OpLabel\n"
		"OpBranch %forloop_continue\n"
		"%default = OpLabel\n"
		"OpBranch %forloop_continue\n"
		"%switch_merge = OpLabel\n"
		";should never get here, so change the return value to invalid result\n"
		"OpStore %val_ptr %c_f32_1\n"
		"OpBranch %forloop_continue\n"

		"%forloop_continue = OpLabel\n"
		"OpStore %keep_going %false\n"
		"OpBranch %forloop_begin\n"
		"%forloop_merge = OpLabel\n"

		"%val = OpLoad %f32 %val_ptr\n"
		"%result = OpVectorInsertDynamic %v4f32 %param1 %val %c_i32_0\n"
		"OpReturnValue %result\n"
		"OpFunctionEnd\n";
	createTestsForAllStages("switch_continue", defaultColors, defaultColors, fragments, testGroup.get());

	return testGroup.release();
}

// A collection of tests putting OpControlBarrier in places GLSL forbids but SPIR-V allows.
tcu::TestCaseGroup* createBarrierTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "barrier", "OpControlBarrier"));
	map<string, string> fragments;

	// A barrier inside a function body.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%Invocation = OpConstant %i32 4\n"
		"%MemorySemanticsNone = OpConstant %i32 0\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"OpReturnValue %param1\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_function", fragments);

	// Common setup code for the following tests.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%Invocation = OpConstant %i32 4\n"
		"%MemorySemanticsNone = OpConstant %i32 0\n"
		"%c_f32_5 = OpConstant %f32 5.\n";
	const string setupPercentZero =	 // Begins %test_code function with code that sets %zero to 0u but cannot be optimized away.
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%entry = OpLabel\n"
		";param1 components are between 0 and 1, so dot product is 4 or less\n"
		"%dot = OpDot %f32 %param1 %param1\n"
		"%div = OpFDiv %f32 %dot %c_f32_5\n"
		"%zero = OpConvertFToU %u32 %div\n";

	// Barriers inside OpSwitch branches.
	fragments["testfun"] =
		setupPercentZero +
		"OpSelectionMerge %switch_exit None\n"
		"OpSwitch %zero %switch_default 0 %case0 1 %case1 ;should always go to %case0\n"

		"%case1 = OpLabel\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"%wrong_branch_alert1 = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		"OpBranch %switch_exit\n"

		"%switch_default = OpLabel\n"
		"%wrong_branch_alert2 = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"OpBranch %switch_exit\n"

		"%case0 = OpLabel\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"OpBranch %switch_exit\n"

		"%switch_exit = OpLabel\n"
		"%ret = OpPhi %v4f32 %param1 %case0 %wrong_branch_alert1 %case1 %wrong_branch_alert2 %switch_default\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_switch", fragments);

	// Barriers inside if-then-else.
	fragments["testfun"] =
		setupPercentZero +
		"%eq0 = OpIEqual %bool %zero %c_u32_0\n"
		"OpSelectionMerge %exit DontFlatten\n"
		"OpBranchConditional %eq0 %then %else\n"

		"%else = OpLabel\n"
		";This barrier should never be executed, but its presence makes test failure more likely when there's a bug.\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"%wrong_branch_alert = OpVectorInsertDynamic %v4f32 %param1 %c_f32_0_5 %c_i32_0\n"
		"OpBranch %exit\n"

		"%then = OpLabel\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"OpBranch %exit\n"
		"%exit = OpLabel\n"
		"%ret = OpPhi %v4f32 %param1 %then %wrong_branch_alert %else\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_if", fragments);

	// A barrier after control-flow reconvergence, tempting the compiler to attempt something like this:
	// http://lists.llvm.org/pipermail/llvm-dev/2009-October/026317.html.
	fragments["testfun"] =
		setupPercentZero +
		"%thread_id = OpLoad %i32 %BP_gl_InvocationID\n"
		"%thread0 = OpIEqual %bool %thread_id %c_i32_0\n"
		"OpSelectionMerge %exit DontFlatten\n"
		"OpBranchConditional %thread0 %then %else\n"

		"%else = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %exit\n"

		"%then = OpLabel\n"
		"%val1 = OpVectorExtractDynamic %f32 %param1 %zero\n"
		"OpBranch %exit\n"

		"%exit = OpLabel\n"
		"%val = OpPhi %f32 %val0 %else %val1 %then\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %val %zero\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "after_divergent_if", fragments);

	// A barrier inside a loop.
	fragments["pre_main"] =
		"%Workgroup = OpConstant %i32 2\n"
		"%Invocation = OpConstant %i32 4\n"
		"%MemorySemanticsNone = OpConstant %i32 0\n"
		"%c_f32_10 = OpConstant %f32 10.\n";
	fragments["testfun"] =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%entry = OpLabel\n"
		"%val0 = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"OpBranch %loop\n"

		";adds 4, 3, 2, and 1 to %val0\n"
		"%loop = OpLabel\n"
		"%count = OpPhi %i32 %c_i32_4 %entry %count__ %loop\n"
		"%val1 = OpPhi %f32 %val0 %entry %val %loop\n"
		"OpControlBarrier %Workgroup %Invocation %MemorySemanticsNone\n"
		"%fcount = OpConvertSToF %f32 %count\n"
		"%val = OpFAdd %f32 %val1 %fcount\n"
		"%count__ = OpISub %i32 %count %c_i32_1\n"
		"%again = OpSGreaterThan %bool %count__ %c_i32_0\n"
		"OpLoopMerge %exit %loop None\n"
		"OpBranchConditional %again %loop %exit\n"

		"%exit = OpLabel\n"
		"%same = OpFSub %f32 %val %c_f32_10\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %same %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";
	addTessCtrlTest(testGroup.get(), "in_loop", fragments);

	return testGroup.release();
}

// Test for the OpFRem instruction.
tcu::TestCaseGroup* createFRemTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup(new tcu::TestCaseGroup(testCtx, "frem", "OpFRem"));
	map<string, string>					fragments;
	RGBA								inputColors[4];
	RGBA								outputColors[4];

	fragments["pre_main"]				 =
		"%c_f32_3 = OpConstant %f32 3.0\n"
		"%c_f32_n3 = OpConstant %f32 -3.0\n"
		"%c_f32_4 = OpConstant %f32 4.0\n"
		"%c_f32_p75 = OpConstant %f32 0.75\n"
		"%c_v4f32_p75_p75_p75_p75 = OpConstantComposite %v4f32 %c_f32_p75 %c_f32_p75 %c_f32_p75 %c_f32_p75 \n"
		"%c_v4f32_4_4_4_4 = OpConstantComposite %v4f32 %c_f32_4 %c_f32_4 %c_f32_4 %c_f32_4\n"
		"%c_v4f32_3_n3_3_n3 = OpConstantComposite %v4f32 %c_f32_3 %c_f32_n3 %c_f32_3 %c_f32_n3\n";

	// The test does the following.
	// vec4 result = (param1 * 8.0) - 4.0;
	// return (frem(result.x,3) + 0.75, frem(result.y, -3) + 0.75, 0, 1)
	fragments["testfun"]				 =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%v_times_8 = OpVectorTimesScalar %v4f32 %param1 %c_f32_8\n"
		"%minus_4 = OpFSub %v4f32 %v_times_8 %c_v4f32_4_4_4_4\n"
		"%frem = OpFRem %v4f32 %minus_4 %c_v4f32_3_n3_3_n3\n"
		"%added = OpFAdd %v4f32 %frem %c_v4f32_p75_p75_p75_p75\n"
		"%xyz_1 = OpVectorInsertDynamic %v4f32 %added %c_f32_1 %c_i32_3\n"
		"%xy_0_1 = OpVectorInsertDynamic %v4f32 %xyz_1 %c_f32_0 %c_i32_2\n"
		"OpReturnValue %xy_0_1\n"
		"OpFunctionEnd\n";


	inputColors[0]		= RGBA(16,	16,		0, 255);
	inputColors[1]		= RGBA(232, 232,	0, 255);
	inputColors[2]		= RGBA(232, 16,		0, 255);
	inputColors[3]		= RGBA(16,	232,	0, 255);

	outputColors[0]		= RGBA(64,	64,		0, 255);
	outputColors[1]		= RGBA(255, 255,	0, 255);
	outputColors[2]		= RGBA(255, 64,		0, 255);
	outputColors[3]		= RGBA(64,	255,	0, 255);

	createTestsForAllStages("frem", inputColors, outputColors, fragments, testGroup.get());
	return testGroup.release();
}

// Test for the OpSRem instruction.
tcu::TestCaseGroup* createOpSRemGraphicsTests(tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup(new tcu::TestCaseGroup(testCtx, "srem", "OpSRem"));
	map<string, string>					fragments;

	fragments["pre_main"]				 =
		"%c_f32_255 = OpConstant %f32 255.0\n"
		"%c_i32_128 = OpConstant %i32 128\n"
		"%c_i32_255 = OpConstant %i32 255\n"
		"%c_v4f32_255 = OpConstantComposite %v4f32 %c_f32_255 %c_f32_255 %c_f32_255 %c_f32_255 \n"
		"%c_v4f32_0_5 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 \n"
		"%c_v4i32_128 = OpConstantComposite %v4i32 %c_i32_128 %c_i32_128 %c_i32_128 %c_i32_128 \n";

	// The test does the following.
	// ivec4 ints = int(param1 * 255.0 + 0.5) - 128;
	// ivec4 result = ivec4(srem(ints.x, ints.y), srem(ints.y, ints.z), srem(ints.z, ints.x), 255);
	// return float(result + 128) / 255.0;
	fragments["testfun"]				 =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%div255 = OpFMul %v4f32 %param1 %c_v4f32_255\n"
		"%add0_5 = OpFAdd %v4f32 %div255 %c_v4f32_0_5\n"
		"%uints_in = OpConvertFToS %v4i32 %add0_5\n"
		"%ints_in = OpISub %v4i32 %uints_in %c_v4i32_128\n"
		"%x_in = OpCompositeExtract %i32 %ints_in 0\n"
		"%y_in = OpCompositeExtract %i32 %ints_in 1\n"
		"%z_in = OpCompositeExtract %i32 %ints_in 2\n"
		"%x_out = OpSRem %i32 %x_in %y_in\n"
		"%y_out = OpSRem %i32 %y_in %z_in\n"
		"%z_out = OpSRem %i32 %z_in %x_in\n"
		"%ints_out = OpCompositeConstruct %v4i32 %x_out %y_out %z_out %c_i32_255\n"
		"%ints_offset = OpIAdd %v4i32 %ints_out %c_v4i32_128\n"
		"%f_ints_offset = OpConvertSToF %v4f32 %ints_offset\n"
		"%float_out = OpFDiv %v4f32 %f_ints_offset %c_v4f32_255\n"
		"OpReturnValue %float_out\n"
		"OpFunctionEnd\n";

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessageTemplate;	// customized status message
		qpTestResult	failResult;				// override status on failure
		int				operands[4][3];			// four (x, y, z) vectors of operands
		int				results[4][3];			// four (x, y, z) vectors of results
	} cases[] =
	{
		{
			"positive",
			"${reason}",
			QP_TEST_RESULT_FAIL,
			{ { 5, 12, 17 }, { 5, 5, 7 }, { 75, 8, 81 }, { 25, 60, 100 } },			// operands
			{ { 5, 12,  2 }, { 0, 5, 2 }, {  3, 8,  6 }, { 25, 60,   0 } },			// results
		},
		{
			"all",
			"Inconsistent results, but within specification: ${reason}",
			negFailResult,															// negative operands, not required by the spec
			{ { 5, 12, -17 }, { -5, -5, 7 }, { 75, 8, -81 }, { 25, -60, 100 } },	// operands
			{ { 5, 12,  -2 }, {  0, -5, 2 }, {  3, 8,  -6 }, { 25, -60,   0 } },	// results
		},
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params			= cases[caseNdx];
		RGBA				inputColors[4];
		RGBA				outputColors[4];

		for (int i = 0; i < 4; ++i)
		{
			inputColors [i] = RGBA(params.operands[i][0] + 128, params.operands[i][1] + 128, params.operands[i][2] + 128, 255);
			outputColors[i] = RGBA(params.results [i][0] + 128, params.results [i][1] + 128, params.results [i][2] + 128, 255);
		}

		createTestsForAllStages(params.name, inputColors, outputColors, fragments, testGroup.get(), params.failResult, params.failMessageTemplate);
	}

	return testGroup.release();
}

// Test for the OpSMod instruction.
tcu::TestCaseGroup* createOpSModGraphicsTests(tcu::TestContext& testCtx, qpTestResult negFailResult)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup(new tcu::TestCaseGroup(testCtx, "smod", "OpSMod"));
	map<string, string>					fragments;

	fragments["pre_main"]				 =
		"%c_f32_255 = OpConstant %f32 255.0\n"
		"%c_i32_128 = OpConstant %i32 128\n"
		"%c_i32_255 = OpConstant %i32 255\n"
		"%c_v4f32_255 = OpConstantComposite %v4f32 %c_f32_255 %c_f32_255 %c_f32_255 %c_f32_255 \n"
		"%c_v4f32_0_5 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 \n"
		"%c_v4i32_128 = OpConstantComposite %v4i32 %c_i32_128 %c_i32_128 %c_i32_128 %c_i32_128 \n";

	// The test does the following.
	// ivec4 ints = int(param1 * 255.0 + 0.5) - 128;
	// ivec4 result = ivec4(smod(ints.x, ints.y), smod(ints.y, ints.z), smod(ints.z, ints.x), 255);
	// return float(result + 128) / 255.0;
	fragments["testfun"]				 =
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"%div255 = OpFMul %v4f32 %param1 %c_v4f32_255\n"
		"%add0_5 = OpFAdd %v4f32 %div255 %c_v4f32_0_5\n"
		"%uints_in = OpConvertFToS %v4i32 %add0_5\n"
		"%ints_in = OpISub %v4i32 %uints_in %c_v4i32_128\n"
		"%x_in = OpCompositeExtract %i32 %ints_in 0\n"
		"%y_in = OpCompositeExtract %i32 %ints_in 1\n"
		"%z_in = OpCompositeExtract %i32 %ints_in 2\n"
		"%x_out = OpSMod %i32 %x_in %y_in\n"
		"%y_out = OpSMod %i32 %y_in %z_in\n"
		"%z_out = OpSMod %i32 %z_in %x_in\n"
		"%ints_out = OpCompositeConstruct %v4i32 %x_out %y_out %z_out %c_i32_255\n"
		"%ints_offset = OpIAdd %v4i32 %ints_out %c_v4i32_128\n"
		"%f_ints_offset = OpConvertSToF %v4f32 %ints_offset\n"
		"%float_out = OpFDiv %v4f32 %f_ints_offset %c_v4f32_255\n"
		"OpReturnValue %float_out\n"
		"OpFunctionEnd\n";

	const struct CaseParams
	{
		const char*		name;
		const char*		failMessageTemplate;	// customized status message
		qpTestResult	failResult;				// override status on failure
		int				operands[4][3];			// four (x, y, z) vectors of operands
		int				results[4][3];			// four (x, y, z) vectors of results
	} cases[] =
	{
		{
			"positive",
			"${reason}",
			QP_TEST_RESULT_FAIL,
			{ { 5, 12, 17 }, { 5, 5, 7 }, { 75, 8, 81 }, { 25, 60, 100 } },				// operands
			{ { 5, 12,  2 }, { 0, 5, 2 }, {  3, 8,  6 }, { 25, 60,   0 } },				// results
		},
		{
			"all",
			"Inconsistent results, but within specification: ${reason}",
			negFailResult,																// negative operands, not required by the spec
			{ { 5, 12, -17 }, { -5, -5,  7 }, { 75,   8, -81 }, {  25, -60, 100 } },	// operands
			{ { 5, -5,   3 }, {  0,  2, -3 }, {  3, -73,  69 }, { -35,  40,   0 } },	// results
		},
	};
	// If either operand is negative the result is undefined. Some implementations may still return correct values.

	for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); ++caseNdx)
	{
		const CaseParams&	params			= cases[caseNdx];
		RGBA				inputColors[4];
		RGBA				outputColors[4];

		for (int i = 0; i < 4; ++i)
		{
			inputColors [i] = RGBA(params.operands[i][0] + 128, params.operands[i][1] + 128, params.operands[i][2] + 128, 255);
			outputColors[i] = RGBA(params.results [i][0] + 128, params.results [i][1] + 128, params.results [i][2] + 128, 255);
		}

		createTestsForAllStages(params.name, inputColors, outputColors, fragments, testGroup.get(), params.failResult, params.failMessageTemplate);
	}
	return testGroup.release();
}

enum ConversionDataType
{
	DATA_TYPE_SIGNED_8,
	DATA_TYPE_SIGNED_16,
	DATA_TYPE_SIGNED_32,
	DATA_TYPE_SIGNED_64,
	DATA_TYPE_UNSIGNED_8,
	DATA_TYPE_UNSIGNED_16,
	DATA_TYPE_UNSIGNED_32,
	DATA_TYPE_UNSIGNED_64,
	DATA_TYPE_FLOAT_16,
	DATA_TYPE_FLOAT_32,
	DATA_TYPE_FLOAT_64,
	DATA_TYPE_VEC2_SIGNED_16,
	DATA_TYPE_VEC2_SIGNED_32
};

const string getBitWidthStr (ConversionDataType type)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:
		case DATA_TYPE_UNSIGNED_8:
			return "8";

		case DATA_TYPE_SIGNED_16:
		case DATA_TYPE_UNSIGNED_16:
		case DATA_TYPE_FLOAT_16:
			return "16";

		case DATA_TYPE_SIGNED_32:
		case DATA_TYPE_UNSIGNED_32:
		case DATA_TYPE_FLOAT_32:
		case DATA_TYPE_VEC2_SIGNED_16:
			return "32";

		case DATA_TYPE_SIGNED_64:
		case DATA_TYPE_UNSIGNED_64:
		case DATA_TYPE_FLOAT_64:
		case DATA_TYPE_VEC2_SIGNED_32:
			return "64";

		default:
			DE_ASSERT(false);
	}
	return "";
}

const string getByteWidthStr (ConversionDataType type)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:
		case DATA_TYPE_UNSIGNED_8:
			return "1";

		case DATA_TYPE_SIGNED_16:
		case DATA_TYPE_UNSIGNED_16:
		case DATA_TYPE_FLOAT_16:
			return "2";

		case DATA_TYPE_SIGNED_32:
		case DATA_TYPE_UNSIGNED_32:
		case DATA_TYPE_FLOAT_32:
		case DATA_TYPE_VEC2_SIGNED_16:
			return "4";

		case DATA_TYPE_SIGNED_64:
		case DATA_TYPE_UNSIGNED_64:
		case DATA_TYPE_FLOAT_64:
		case DATA_TYPE_VEC2_SIGNED_32:
			return "8";

		default:
			DE_ASSERT(false);
	}
	return "";
}

bool isSigned (ConversionDataType type)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:
		case DATA_TYPE_SIGNED_16:
		case DATA_TYPE_SIGNED_32:
		case DATA_TYPE_SIGNED_64:
		case DATA_TYPE_FLOAT_16:
		case DATA_TYPE_FLOAT_32:
		case DATA_TYPE_FLOAT_64:
		case DATA_TYPE_VEC2_SIGNED_16:
		case DATA_TYPE_VEC2_SIGNED_32:
			return true;

		case DATA_TYPE_UNSIGNED_8:
		case DATA_TYPE_UNSIGNED_16:
		case DATA_TYPE_UNSIGNED_32:
		case DATA_TYPE_UNSIGNED_64:
			return false;

		default:
			DE_ASSERT(false);
	}
	return false;
}

bool isInt (ConversionDataType type)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:
		case DATA_TYPE_SIGNED_16:
		case DATA_TYPE_SIGNED_32:
		case DATA_TYPE_SIGNED_64:
		case DATA_TYPE_UNSIGNED_8:
		case DATA_TYPE_UNSIGNED_16:
		case DATA_TYPE_UNSIGNED_32:
		case DATA_TYPE_UNSIGNED_64:
			return true;

		case DATA_TYPE_FLOAT_16:
		case DATA_TYPE_FLOAT_32:
		case DATA_TYPE_FLOAT_64:
		case DATA_TYPE_VEC2_SIGNED_16:
		case DATA_TYPE_VEC2_SIGNED_32:
			return false;

		default:
			DE_ASSERT(false);
	}
	return false;
}

bool isFloat (ConversionDataType type)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:
		case DATA_TYPE_SIGNED_16:
		case DATA_TYPE_SIGNED_32:
		case DATA_TYPE_SIGNED_64:
		case DATA_TYPE_UNSIGNED_8:
		case DATA_TYPE_UNSIGNED_16:
		case DATA_TYPE_UNSIGNED_32:
		case DATA_TYPE_UNSIGNED_64:
		case DATA_TYPE_VEC2_SIGNED_16:
		case DATA_TYPE_VEC2_SIGNED_32:
			return false;

		case DATA_TYPE_FLOAT_16:
		case DATA_TYPE_FLOAT_32:
		case DATA_TYPE_FLOAT_64:
			return true;

		default:
			DE_ASSERT(false);
	}
	return false;
}

const string getTypeName (ConversionDataType type)
{
	string prefix = isSigned(type) ? "" : "u";

	if		(isInt(type))						return prefix + "int"	+ getBitWidthStr(type);
	else if (isFloat(type))						return prefix + "float"	+ getBitWidthStr(type);
	else if (type == DATA_TYPE_VEC2_SIGNED_16)	return "i16vec2";
	else if (type == DATA_TYPE_VEC2_SIGNED_32)	return "i32vec2";
	else										DE_ASSERT(false);

	return "";
}

const string getTestName (ConversionDataType from, ConversionDataType to, const char* suffix)
{
	const string fullSuffix(suffix == DE_NULL ? "" : string("_") + string(suffix));

	return getTypeName(from) + "_to_" + getTypeName(to) + fullSuffix;
}

const string getAsmTypeName (ConversionDataType type, deUint32 elements = 1)
{
	string prefix;

	if		(isInt(type))						prefix = isSigned(type) ? "i" : "u";
	else if (isFloat(type))						prefix = "f";
	else if (type == DATA_TYPE_VEC2_SIGNED_16)	return "i16vec2";
	else if (type == DATA_TYPE_VEC2_SIGNED_32)	return "v2i32";
	else										DE_ASSERT(false);
	if ((isInt(type) || isFloat(type)) && elements == 2)
	{
		prefix = "v2" + prefix;
	}

	return prefix + getBitWidthStr(type);
}

template<typename T>
BufferSp getSpecializedBuffer (deInt64 number, deUint32 elements = 1)
{
	return BufferSp(new Buffer<T>(vector<T>(elements, (T)number)));
}

BufferSp getBuffer (ConversionDataType type, deInt64 number, deUint32 elements = 1)
{
	switch (type)
	{
		case DATA_TYPE_SIGNED_8:		return getSpecializedBuffer<deInt8>(number, elements);
		case DATA_TYPE_SIGNED_16:		return getSpecializedBuffer<deInt16>(number, elements);
		case DATA_TYPE_SIGNED_32:		return getSpecializedBuffer<deInt32>(number, elements);
		case DATA_TYPE_SIGNED_64:		return getSpecializedBuffer<deInt64>(number, elements);
		case DATA_TYPE_UNSIGNED_8:		return getSpecializedBuffer<deUint8>(number, elements);
		case DATA_TYPE_UNSIGNED_16:		return getSpecializedBuffer<deUint16>(number, elements);
		case DATA_TYPE_UNSIGNED_32:		return getSpecializedBuffer<deUint32>(number, elements);
		case DATA_TYPE_UNSIGNED_64:		return getSpecializedBuffer<deUint64>(number, elements);
		case DATA_TYPE_FLOAT_16:		return getSpecializedBuffer<deUint16>(number, elements);
		case DATA_TYPE_FLOAT_32:		return getSpecializedBuffer<deUint32>(number, elements);
		case DATA_TYPE_FLOAT_64:		return getSpecializedBuffer<deUint64>(number, elements);
		case DATA_TYPE_VEC2_SIGNED_16:	return getSpecializedBuffer<deUint32>(number, elements);
		case DATA_TYPE_VEC2_SIGNED_32:	return getSpecializedBuffer<deUint64>(number, elements);

		default:						TCU_THROW(InternalError, "Unimplemented type passed");
	}
}

bool usesInt8 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_SIGNED_8 || to == DATA_TYPE_SIGNED_8 ||
			from == DATA_TYPE_UNSIGNED_8 || to == DATA_TYPE_UNSIGNED_8);
}

bool usesInt16 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_SIGNED_16 || to == DATA_TYPE_SIGNED_16 ||
			from == DATA_TYPE_UNSIGNED_16 || to == DATA_TYPE_UNSIGNED_16 ||
			from == DATA_TYPE_VEC2_SIGNED_16 || to == DATA_TYPE_VEC2_SIGNED_16);
}

bool usesInt32 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_SIGNED_32 || to == DATA_TYPE_SIGNED_32 ||
			from == DATA_TYPE_UNSIGNED_32 || to == DATA_TYPE_UNSIGNED_32 ||
			from == DATA_TYPE_VEC2_SIGNED_32|| to == DATA_TYPE_VEC2_SIGNED_32);
}

bool usesInt64 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_SIGNED_64 || to == DATA_TYPE_SIGNED_64 ||
			from == DATA_TYPE_UNSIGNED_64 || to == DATA_TYPE_UNSIGNED_64);
}

bool usesFloat16 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_FLOAT_16 || to == DATA_TYPE_FLOAT_16);
}

bool usesFloat32 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_FLOAT_32 || to == DATA_TYPE_FLOAT_32);
}

bool usesFloat64 (ConversionDataType from, ConversionDataType to)
{
	return (from == DATA_TYPE_FLOAT_64 || to == DATA_TYPE_FLOAT_64);
}

void getVulkanFeaturesAndExtensions (ConversionDataType from, ConversionDataType to, bool useStorageExt, VulkanFeatures& vulkanFeatures, vector<string>& extensions)
{
	if (usesInt16(from, to) && !usesInt32(from, to))
		vulkanFeatures.coreFeatures.shaderInt16 = DE_TRUE;

	if (usesInt64(from, to))
		vulkanFeatures.coreFeatures.shaderInt64 = DE_TRUE;

	if (usesFloat64(from, to))
		vulkanFeatures.coreFeatures.shaderFloat64 = DE_TRUE;

	if ((usesInt16(from, to) || usesFloat16(from, to)) && useStorageExt)
	{
		extensions.push_back("VK_KHR_16bit_storage");
		vulkanFeatures.ext16BitStorage |= EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK;
	}

	if (usesFloat16(from, to) || usesInt8(from, to))
	{
		extensions.push_back("VK_KHR_shader_float16_int8");

		if (usesFloat16(from, to))
		{
			vulkanFeatures.extFloat16Int8 |= EXTFLOAT16INT8FEATURES_FLOAT16;
		}

		if (usesInt8(from, to))
		{
			vulkanFeatures.extFloat16Int8 |= EXTFLOAT16INT8FEATURES_INT8;

			extensions.push_back("VK_KHR_8bit_storage");
			vulkanFeatures.ext8BitStorage |= EXT8BITSTORAGEFEATURES_STORAGE_BUFFER;
		}
	}
}

struct ConvertCase
{
	ConvertCase (const string& instruction, ConversionDataType from, ConversionDataType to, deInt64 number, bool separateOutput = false, deInt64 outputNumber = 0, const char* suffix = DE_NULL, bool useStorageExt = true)
	: m_fromType		(from)
	, m_toType			(to)
	, m_elements		(1)
	, m_useStorageExt	(useStorageExt)
	, m_name			(getTestName(from, to, suffix))
	{
		string caps;
		string decl;
		string exts;

		m_asmTypes["inStorageType"]	= getAsmTypeName(from);
		m_asmTypes["outStorageType"] = getAsmTypeName(to);
		m_asmTypes["inCast"] = "OpCopyObject";
		m_asmTypes["outCast"] = "OpCopyObject";
		// If the storage extensions are being avoided, tests instead uses
		// vectors so that they are easily convertible to 32-bit integers.
		// |m_elements| indicates the size of the vector. It modifies how many
		// items added to the buffers and converted in the tests.
		//
		// Currently only supports 1 (default) or 2 elements.
		if (!m_useStorageExt)
		{
			bool in_change = false;
			bool out_change = false;
			if (usesFloat16(from, from) || usesInt16(from, from))
			{
				m_asmTypes["inStorageType"] = "u32";
				m_asmTypes["inCast"] = "OpBitcast";
				m_elements = 2;
				in_change = true;
			}
			if (usesFloat16(to, to) || usesInt16(to, to))
			{
				m_asmTypes["outStorageType"] = "u32";
				m_asmTypes["outCast"] = "OpBitcast";
				m_elements = 2;
				out_change = true;
			}
			if (in_change && !out_change)
			{
				m_asmTypes["outStorageType"] = getAsmTypeName(to, m_elements);
			}
			if (!in_change && out_change)
			{
				m_asmTypes["inStorageType"] = getAsmTypeName(from, m_elements);
			}
		}

		// Safety check for implementation.
		if (m_elements < 1 || m_elements > 2)
			TCU_THROW(InternalError, "Unsupported number of elements");

		m_asmTypes["inputType"]		= getAsmTypeName(from, m_elements);
		m_asmTypes["outputType"]	= getAsmTypeName(to, m_elements);

		m_inputBuffer = getBuffer(from, number, m_elements);
		if (separateOutput)
			m_outputBuffer = getBuffer(to, outputNumber, m_elements);
		else
			m_outputBuffer = getBuffer(to, number, m_elements);

		if (usesInt8(from, to))
		{
			bool requiresInt8Capability = true;
			if (instruction == "OpUConvert" || instruction == "OpSConvert")
			{
				// Conversions between 8 and 32 bit are provided by SPV_KHR_8bit_storage. The rest requires explicit Int8
				if (usesInt32(from, to))
					requiresInt8Capability = false;
			}

			caps += "OpCapability StorageBuffer8BitAccess\n";
			if (requiresInt8Capability)
				caps += "OpCapability Int8\n";

			decl += "%i8         = OpTypeInt 8 1\n"
					"%u8         = OpTypeInt 8 0\n";

			if (m_elements == 2)
			{
				decl += "%v2i8       = OpTypeVector %i8 2\n"
						"%v2u8       = OpTypeVector %u8 2\n";
			}
			exts += "OpExtension \"SPV_KHR_8bit_storage\"\n";
		}

		if (usesInt16(from, to))
		{
			bool requiresInt16Capability = true;

			if (instruction == "OpUConvert" || instruction == "OpSConvert" || instruction == "OpFConvert")
			{
				// Width-only conversions between 16 and 32 bit are provided by SPV_KHR_16bit_storage. The rest requires explicit Int16
				if (usesInt32(from, to) || usesFloat32(from, to))
					requiresInt16Capability = false;
			}

			decl += "%i16        = OpTypeInt 16 1\n"
					"%u16        = OpTypeInt 16 0\n";
			if (m_elements == 2)
			{
				decl += "%v2i16      = OpTypeVector %i16 2\n"
						"%v2u16      = OpTypeVector %u16 2\n";
			}
			else
			{
				decl += "%i16vec2    = OpTypeVector %i16 2\n";
			}

			// Conversions between 16 and 32 bit are provided by SPV_KHR_16bit_storage. The rest requires explicit Int16
			if (requiresInt16Capability || !m_useStorageExt)
				caps += "OpCapability Int16\n";
		}

		if (usesFloat16(from, to))
		{
			decl += "%f16        = OpTypeFloat 16\n";
			if (m_elements == 2)
			{
				decl += "%v2f16      = OpTypeVector %f16 2\n";
			}

			// Width-only conversions between 16 and 32 bit are provided by SPV_KHR_16bit_storage. The rest requires explicit Float16
			if (!usesFloat32(from, to) || !m_useStorageExt)
				caps += "OpCapability Float16\n";
		}

		if ((usesInt16(from, to) || usesFloat16(from, to)) && m_useStorageExt)
		{
			caps += "OpCapability StorageUniformBufferBlock16\n";
			exts += "OpExtension \"SPV_KHR_16bit_storage\"\n";
		}

		if (usesInt64(from, to))
		{
			caps += "OpCapability Int64\n";
			decl += "%i64        = OpTypeInt 64 1\n"
					"%u64        = OpTypeInt 64 0\n";
			if (m_elements == 2)
			{
				decl += "%v2i64      = OpTypeVector %i64 2\n"
						"%v2u64      = OpTypeVector %u64 2\n";
			}
		}

		if (usesFloat64(from, to))
		{
			caps += "OpCapability Float64\n";
			decl += "%f64        = OpTypeFloat 64\n";
			if (m_elements == 2)
			{
				decl += "%v2f64        = OpTypeVector %f64 2\n";
			}
		}

		m_asmTypes["datatype_capabilities"]		= caps;
		m_asmTypes["datatype_additional_decl"]	= decl;
		m_asmTypes["datatype_extensions"]		= exts;
	}

	ConversionDataType		m_fromType;
	ConversionDataType		m_toType;
	deUint32				m_elements;
	bool					m_useStorageExt;
	string					m_name;
	map<string, string>		m_asmTypes;
	BufferSp				m_inputBuffer;
	BufferSp				m_outputBuffer;
};

const string getConvertCaseShaderStr (const string& instruction, const ConvertCase& convertCase, bool addVectors = false)
{
	map<string, string> params = convertCase.m_asmTypes;

	params["instruction"]	= instruction;
	params["inDecorator"]	= getByteWidthStr(convertCase.m_fromType);
	params["outDecorator"]	= getByteWidthStr(convertCase.m_toType);

	std::string shader (
		"OpCapability Shader\n"
		"${datatype_capabilities}"
		"${datatype_extensions:opt}"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\"\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		// Decorators
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %in_buf BufferBlock\n"
		"OpDecorate %out_buf BufferBlock\n"
		"OpMemberDecorate %in_buf 0 Offset 0\n"
		"OpMemberDecorate %out_buf 0 Offset 0\n"
		// Base types
		"%void       = OpTypeVoid\n"
		"%voidf      = OpTypeFunction %void\n"
		"%u32        = OpTypeInt 32 0\n"
		"%i32        = OpTypeInt 32 1\n"
		"%f32        = OpTypeFloat 32\n"
		"%v2i32      = OpTypeVector %i32 2\n"
		"${datatype_additional_decl}"
	);
	if (addVectors)
	{
		shader += "%v2u32 = OpTypeVector %u32 2\n"
					"%v2f32 = OpTypeVector %f32 2\n";
	}
	shader +=
		"%uvec3      = OpTypeVector %u32 3\n"
		// Derived types
		"%in_ptr     = OpTypePointer Uniform %${inStorageType}\n"
		"%out_ptr    = OpTypePointer Uniform %${outStorageType}\n"
		"%in_buf     = OpTypeStruct %${inStorageType}\n"
		"%out_buf    = OpTypeStruct %${outStorageType}\n"
		"%in_bufptr  = OpTypePointer Uniform %in_buf\n"
		"%out_bufptr = OpTypePointer Uniform %out_buf\n"
		"%indata     = OpVariable %in_bufptr Uniform\n"
		"%outdata    = OpVariable %out_bufptr Uniform\n"
		// Constants
		"%zero       = OpConstant %i32 0\n"
		// Main function
		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%inloc      = OpAccessChain %in_ptr %indata %zero\n"
		"%outloc     = OpAccessChain %out_ptr %outdata %zero\n"
		"%inval      = OpLoad %${inStorageType} %inloc\n"
		"%in_cast    = ${inCast} %${inputType} %inval\n"
		"%conv       = ${instruction} %${outputType} %in_cast\n"
		"%out_cast   = ${outCast} %${outStorageType} %conv\n"
		"              OpStore %outloc %out_cast\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n"
	;

	return StringTemplate(shader).specialize(params);
}

void createConvertCases (vector<ConvertCase>& testCases, const string& instruction)
{
	if (instruction == "OpUConvert")
	{
		// Convert unsigned int to unsigned int
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_UNSIGNED_16,		42));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_UNSIGNED_32,		73));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_UNSIGNED_64,		121));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_UNSIGNED_8,		33));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_UNSIGNED_32,		60653));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_UNSIGNED_64,		17991));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_UNSIGNED_64,		904256275));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_UNSIGNED_16,		6275));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_UNSIGNED_8,		17));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_UNSIGNED_32,		701256243));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_UNSIGNED_16,		4741));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_UNSIGNED_8,		65));

		// Zero extension for int->uint
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_16,		56));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_32,		-47,								true,	209));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_64,		-5,									true,	251));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_32,		14669));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_64,		-3341,								true,	62195));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_64,		973610259));

		// Truncate for int->uint
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_8,		-25711,								true,	145));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_8,		103));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_8,		-1067742499291926803ll,				true,	237));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_16,		12382));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_32,		-972812359,							true,	3322154937u));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_16,		-1067742499291926803ll,				true,	61165));
	}
	else if (instruction == "OpSConvert")
	{
		// Sign extension int->int
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_SIGNED_16,		-30));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_SIGNED_32,		55));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_SIGNED_64,		-3));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_SIGNED_32,		14669));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_SIGNED_64,		-3341));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_SIGNED_64,		973610259));

		// Truncate for int->int
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_SIGNED_8,			81));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_SIGNED_8,			-93));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_SIGNED_8,			3182748172687672ll,					true,	56));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_SIGNED_16,		12382));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_SIGNED_32,		-972812359));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_SIGNED_16,		-1067742499291926803ll,				true,	-4371));

		// Sign extension for int->uint
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_16,		56));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_32,		-47,								true,	4294967249u));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_UNSIGNED_64,		-5,									true,	18446744073709551611ull));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_32,		14669));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_64,		-3341,								true,	18446744073709548275ull));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_64,		973610259));

		// Truncate for int->uint
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_UNSIGNED_8,		-25711,								true,	145));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_8,		103));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_8,		-1067742499291926803ll,				true,	237));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_UNSIGNED_16,		12382));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_32,		-972812359,							true,	3322154937u));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_UNSIGNED_16,		-1067742499291926803ll,				true,	61165));

		// Sign extension for uint->int
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_SIGNED_16,		71));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_SIGNED_32,		201,								true,	-55));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_SIGNED_64,		188,								true,	-68));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_SIGNED_32,		14669));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_SIGNED_64,		62195,								true,	-3341));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_SIGNED_64,		973610259));

		// Truncate for uint->int
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_SIGNED_8,			67));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_SIGNED_8,			133,								true,	-123));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_SIGNED_8,			836927654193256494ull,				true,	46));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_SIGNED_16,		12382));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_SIGNED_32,		18446744072736739257ull,			true,	-972812359));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_SIGNED_16,		17379001574417624813ull,			true,	-4371));

		// Convert i16vec2 to i32vec2 and vice versa
		// Unsigned values are used here to represent negative signed values and to allow defined shifting behaviour.
		// The actual signed value -32123 is used here as uint16 value 33413 and uint32 value 4294935173
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_VEC2_SIGNED_16,	DATA_TYPE_VEC2_SIGNED_32,	(33413u << 16)			| 27593,	true,	(4294935173ull << 32)	| 27593));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_VEC2_SIGNED_32,	DATA_TYPE_VEC2_SIGNED_16,	(4294935173ull << 32)	| 27593,	true,	(33413u << 16)			| 27593));
	}
	else if (instruction == "OpFConvert")
	{
		// All hexadecimal values below represent 1234.0 as 16/32/64-bit IEEE 754 float
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_FLOAT_64,			0x449a4000,							true,	0x4093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_FLOAT_32,			0x4093480000000000,					true,	0x449a4000));

		// Conversion to/from 32-bit floats are supported by both 16-bit
		// storage and Float16. The tests are duplicated to exercise both
		// cases.
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_FLOAT_16,			0x449a4000,							true,	0x64D2));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_FLOAT_32,			0x64D2,								true,	0x449a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_FLOAT_16,			0x449a4000,							true,	0x64D2,					"no_storage",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_FLOAT_32,			0x64D2,								true,	0x449a4000,				"no_storage",	false));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_FLOAT_64,			0x64D2,								true,	0x4093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_FLOAT_16,			0x4093480000000000,					true,	0x64D2));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_FLOAT_64,			0x64D2,								true,	0x4093480000000000,		"no_storage",	false));
	    testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_FLOAT_16,			0x4093480000000000,					true,	0x64D2,					"no_storage",	false));

	}
	else if (instruction == "OpConvertFToU")
	{
		// Normal numbers from uint8 range
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_8,		0x5020,								true,	33,									"33",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_8,		0x42280000,							true,	42,									"42"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_8,		0x4067800000000000ull,				true,	188,								"188"));

		// Maximum uint8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_8,		0x5BF8,								true,	255,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_8,		0x437F0000,							true,	255,								"max"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_8,		0x406FE00000000000ull,				true,	255,								"max"));

		// +0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_8,		0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_8,		0x00000000,							true,	0,									"p0"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_8,		0x0000000000000000ull,				true,	0,									"p0"));

		// -0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_8,		0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_8,		0x80000000,							true,	0,									"m0"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_8,		0x8000000000000000ull,				true,	0,									"m0"));

		// All hexadecimal values below represent 1234.0 as 16/32/64-bit IEEE 754 float
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_16,		0x64D2,								true,	1234,								"1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_32,		0x64D2,								true,	1234,								"1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_64,		0x64D2,								true,	1234,								"1234",	false));

		// 0x7BFF = 0111 1011 1111 1111 = 0 11110 1111111111 = 65504
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_16,		0x7BFF,								true,	65504,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_32,		0x7BFF,								true,	65504,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_64,		0x7BFF,								true,	65504,								"max",	false));

		// +0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_32,		0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_16,		0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_64,		0x0000,								true,	0,									"p0",	false));

		// -0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_16,		0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_32,		0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_UNSIGNED_64,		0x8000,								true,	0,									"m0",	false));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_16,		0x449a4000,							true,	1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_32,		0x449a4000,							true,	1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_UNSIGNED_64,		0x449a4000,							true,	1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_16,		0x4093480000000000,					true,	1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_32,		0x4093480000000000,					true,	1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_UNSIGNED_64,		0x4093480000000000,					true,	1234));
	}
	else if (instruction == "OpConvertUToF")
	{
		// Normal numbers from uint8 range
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_16,			116,								true,	0x5740,								"116",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_32,			232,								true,	0x43680000,							"232"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_64,			164,								true,	0x4064800000000000ull,				"164"));

		// Maximum uint8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_16,			255,								true,	0x5BF8,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_32,			255,								true,	0x437F0000,							"max"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_8,		DATA_TYPE_FLOAT_64,			255,								true,	0x406FE00000000000ull,				"max"));

		// All hexadecimal values below represent 1234.0 as 32/64-bit IEEE 754 float
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_FLOAT_16,			1234,								true,	0x64D2,								"1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_FLOAT_16,			1234,								true,	0x64D2,								"1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_FLOAT_16,			1234,								true,	0x64D2,								"1234",	false));

		// 0x7BFF = 0111 1011 1111 1111 = 0 11110 1111111111 = 65504
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_FLOAT_16,			65504,								true,	0x7BFF,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_FLOAT_16,			65504,								true,	0x7BFF,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_FLOAT_16,			65504,								true,	0x7BFF,								"max",	false));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_FLOAT_32,			1234,								true,	0x449a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_16,		DATA_TYPE_FLOAT_64,			1234,								true,	0x4093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_FLOAT_32,			1234,								true,	0x449a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_32,		DATA_TYPE_FLOAT_64,			1234,								true,	0x4093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_FLOAT_32,			1234,								true,	0x449a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_UNSIGNED_64,		DATA_TYPE_FLOAT_64,			1234,								true,	0x4093480000000000));
	}
	else if (instruction == "OpConvertFToS")
	{
		// Normal numbers from int8 range
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_8,			0xC980,								true,	-11,								"m11",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_8,			0xC2140000,							true,	-37,								"m37"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_8,			0xC050800000000000ull,				true,	-66,								"m66"));

		// Minimum int8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_8,			0xD800,								true,	-128,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_8,			0xC3000000,							true,	-128,								"min"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_8,			0xC060000000000000ull,				true,	-128,								"min"));

		// Maximum int8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_8,			0x57F0,								true,	127,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_8,			0x42FE0000,							true,	127,								"max"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_8,			0x405FC00000000000ull,				true,	127,								"max"));

		// +0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_8,			0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_8,			0x00000000,							true,	0,									"p0"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_8,			0x0000000000000000ull,				true,	0,									"p0"));

		// -0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_8,			0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_8,			0x80000000,							true,	0,									"m0"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_8,			0x8000000000000000ull,				true,	0,									"m0"));

		// All hexadecimal values below represent -1234.0 as 32/64-bit IEEE 754 float
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_16,		0xE4D2,								true,	-1234,								"m1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_32,		0xE4D2,								true,	-1234,								"m1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_64,		0xE4D2,								true,	-1234,								"m1234",	false));

		// 0xF800 = 1111 1000 0000 0000 = 1 11110 0000000000 = -32768
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_16,		0xF800,								true,	-32768,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_32,		0xF800,								true,	-32768,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_64,		0xF800,								true,	-32768,								"min",	false));

		// 0x77FF = 0111 0111 1111 1111 = 0 11101 1111111111 = 32752
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_16,		0x77FF,								true,	32752,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_32,		0x77FF,								true,	32752,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_64,		0x77FF,								true,	32752,								"max",	false));

		// +0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_16,		0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_32,		0x0000,								true,	0,									"p0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_64,		0x0000,								true,	0,									"p0",	false));

		// -0
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_16,		0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_32,		0x8000,								true,	0,									"m0",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_16,			DATA_TYPE_SIGNED_64,		0x8000,								true,	0,									"m0",	false));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_16,		0xc49a4000,							true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_32,		0xc49a4000,							true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_64,		0xc49a4000,							true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_16,		0xc093480000000000,					true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_32,		0xc093480000000000,					true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_64,			DATA_TYPE_SIGNED_64,		0xc093480000000000,					true,	-1234));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_16,		0x453b9000,							true,	 3001,								"p3001"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_FLOAT_32,			DATA_TYPE_SIGNED_16,		0xc53b9000,							true,	-3001,								"m3001"));
	}
	else if (instruction == "OpConvertSToF")
	{
		// Normal numbers from int8 range
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_16,			-12,								true,	0xCA00,								"m21",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_32,			-21,								true,	0xC1A80000,							"m21"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_64,			-99,								true,	0xC058C00000000000ull,				"m99"));

		// Minimum int8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_16,			-128,								true,	0xD800,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_32,			-128,								true,	0xC3000000,							"min"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_64,			-128,								true,	0xC060000000000000ull,				"min"));

		// Maximum int8 value
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_16,			127,								true,	0x57F0,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_32,			127,								true,	0x42FE0000,							"max"));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_8,			DATA_TYPE_FLOAT_64,			127,								true,	0x405FC00000000000ull,				"max"));

		// All hexadecimal values below represent 1234.0 as 32/64-bit IEEE 754 float
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_FLOAT_16,			-1234,								true,	0xE4D2,								"m1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_FLOAT_16,			-1234,								true,	0xE4D2,								"m1234",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_FLOAT_16,			-1234,								true,	0xE4D2,								"m1234",	false));

		// 0xF800 = 1111 1000 0000 0000 = 1 11110 0000000000 = -32768
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_FLOAT_16,			-32768,								true,	0xF800,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_FLOAT_16,			-32768,								true,	0xF800,								"min",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_FLOAT_16,			-32768,								true,	0xF800,								"min",	false));

		// 0x77FF = 0111 0111 1111 1111 = 0 11101 1111111111 = 32752
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_FLOAT_16,			32752,								true,	0x77FF,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_FLOAT_16,			32752,								true,	0x77FF,								"max",	false));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_FLOAT_16,			32752,								true,	0x77FF,								"max",	false));

		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_FLOAT_32,			-1234,								true,	0xc49a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_16,		DATA_TYPE_FLOAT_64,			-1234,								true,	0xc093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_FLOAT_32,			-1234,								true,	0xc49a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_32,		DATA_TYPE_FLOAT_64,			-1234,								true,	0xc093480000000000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_FLOAT_32,			-1234,								true,	0xc49a4000));
		testCases.push_back(ConvertCase(instruction,	DATA_TYPE_SIGNED_64,		DATA_TYPE_FLOAT_64,			-1234,								true,	0xc093480000000000));
	}
	else
		DE_FATAL("Unknown instruction");
}

const map<string, string> getConvertCaseFragments (string instruction, const ConvertCase& convertCase)
{
	map<string, string> params = convertCase.m_asmTypes;
	map<string, string> fragments;

	params["instruction"] = instruction;
	params["inDecorator"] = getByteWidthStr(convertCase.m_fromType);

	const StringTemplate decoration (
		"      OpDecorate %SSBOi DescriptorSet 0\n"
		"      OpDecorate %SSBOo DescriptorSet 0\n"
		"      OpDecorate %SSBOi Binding 0\n"
		"      OpDecorate %SSBOo Binding 1\n"
		"      OpDecorate %s_SSBOi Block\n"
		"      OpDecorate %s_SSBOo Block\n"
		"OpMemberDecorate %s_SSBOi 0 Offset 0\n"
		"OpMemberDecorate %s_SSBOo 0 Offset 0\n");

	const StringTemplate pre_main (
		"${datatype_additional_decl:opt}"
		"    %ptr_in = OpTypePointer StorageBuffer %${inStorageType}\n"
		"   %ptr_out = OpTypePointer StorageBuffer %${outStorageType}\n"
		"   %s_SSBOi = OpTypeStruct %${inStorageType}\n"
		"   %s_SSBOo = OpTypeStruct %${outStorageType}\n"
		" %ptr_SSBOi = OpTypePointer StorageBuffer %s_SSBOi\n"
		" %ptr_SSBOo = OpTypePointer StorageBuffer %s_SSBOo\n"
		"     %SSBOi = OpVariable %ptr_SSBOi StorageBuffer\n"
		"     %SSBOo = OpVariable %ptr_SSBOo StorageBuffer\n");

	const StringTemplate testfun (
		"%test_code  = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param      = OpFunctionParameter %v4f32\n"
		"%label      = OpLabel\n"
		"%iLoc       = OpAccessChain %ptr_in %SSBOi %c_u32_0\n"
		"%oLoc       = OpAccessChain %ptr_out %SSBOo %c_u32_0\n"
		"%valIn      = OpLoad %${inStorageType} %iLoc\n"
		"%valInCast  = ${inCast} %${inputType} %valIn\n"
		"%conv       = ${instruction} %${outputType} %valInCast\n"
		"%valOutCast = ${outCast} %${outStorageType} %conv\n"
		"              OpStore %oLoc %valOutCast\n"
		"              OpReturnValue %param\n"
		"              OpFunctionEnd\n");

	params["datatype_extensions"] =
		params["datatype_extensions"] +
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	fragments["capability"]	= params["datatype_capabilities"];
	fragments["extension"]	= params["datatype_extensions"];
	fragments["decoration"]	= decoration.specialize(params);
	fragments["pre_main"]	= pre_main.specialize(params);
	fragments["testfun"]	= testfun.specialize(params);

	return fragments;
}

const map<string, string> getConvertCaseFragmentsNoStorage(string instruction, const ConvertCase& convertCase)
{
	map<string, string> params = convertCase.m_asmTypes;
	map<string, string> fragments;

	params["instruction"] = instruction;
	params["inDecorator"] = getByteWidthStr(convertCase.m_fromType);

	const StringTemplate decoration(
		"      OpDecorate %SSBOi DescriptorSet 0\n"
		"      OpDecorate %SSBOo DescriptorSet 0\n"
		"      OpDecorate %SSBOi Binding 0\n"
		"      OpDecorate %SSBOo Binding 1\n"
		"      OpDecorate %s_SSBOi Block\n"
		"      OpDecorate %s_SSBOo Block\n"
		"OpMemberDecorate %s_SSBOi 0 Offset 0\n"
		"OpMemberDecorate %s_SSBOo 0 Offset 0\n");

	const StringTemplate pre_main(
		"${datatype_additional_decl:opt}"
		"    %ptr_in = OpTypePointer StorageBuffer %${inStorageType}\n"
		"   %ptr_out = OpTypePointer StorageBuffer %${outStorageType}\n"
		"   %s_SSBOi = OpTypeStruct %${inStorageType}\n"
		"   %s_SSBOo = OpTypeStruct %${outStorageType}\n"
		" %ptr_SSBOi = OpTypePointer StorageBuffer %s_SSBOi\n"
		" %ptr_SSBOo = OpTypePointer StorageBuffer %s_SSBOo\n"
		"     %SSBOi = OpVariable %ptr_SSBOi StorageBuffer\n"
		"     %SSBOo = OpVariable %ptr_SSBOo StorageBuffer\n");

	const StringTemplate testfun(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param     = OpFunctionParameter %v4f32\n"
		"%label     = OpLabel\n"
		"%iLoc      = OpAccessChain %ptr_in %SSBOi %c_u32_0\n"
		"%oLoc      = OpAccessChain %ptr_out %SSBOo %c_u32_0\n"
		"%inval      = OpLoad %${inStorageType} %iLoc\n"
		"%in_cast    = ${inCast} %${inputType} %inval\n"
		"%conv       = ${instruction} %${outputType} %in_cast\n"
		"%out_cast   = ${outCast} %${outStorageType} %conv\n"
		"              OpStore %oLoc %out_cast\n"
		"              OpReturnValue %param\n"
		"              OpFunctionEnd\n");

	params["datatype_extensions"] =
		params["datatype_extensions"] +
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	fragments["capability"] = params["datatype_capabilities"];
	fragments["extension"] = params["datatype_extensions"];
	fragments["decoration"] = decoration.specialize(params);
	fragments["pre_main"] = pre_main.specialize(params);
	fragments["testfun"] = testfun.specialize(params);
	return fragments;
}

// Test for OpSConvert, OpUConvert, OpFConvert and OpConvert* in compute shaders
tcu::TestCaseGroup* createConvertComputeTests (tcu::TestContext& testCtx, const string& instruction, const string& name)
{
	de::MovePtr<tcu::TestCaseGroup>		group(new tcu::TestCaseGroup(testCtx, name.c_str(), instruction.c_str()));
	vector<ConvertCase>					testCases;
	createConvertCases(testCases, instruction);

	for (vector<ConvertCase>::const_iterator test = testCases.begin(); test != testCases.end(); ++test)
	{
		ComputeShaderSpec spec;
		spec.assembly			= getConvertCaseShaderStr(instruction, *test, true);
		spec.numWorkGroups		= IVec3(1, 1, 1);
		spec.inputs.push_back	(test->m_inputBuffer);
		spec.outputs.push_back	(test->m_outputBuffer);

		getVulkanFeaturesAndExtensions(test->m_fromType, test->m_toType, test->m_useStorageExt, spec.requestedVulkanFeatures, spec.extensions);

		group->addChild(new SpvAsmComputeShaderCase(testCtx, test->m_name.c_str(), "", spec));
	}
	return group.release();
}

// Test for OpSConvert, OpUConvert, OpFConvert and OpConvert* in graphics shaders
tcu::TestCaseGroup* createConvertGraphicsTests (tcu::TestContext& testCtx, const string& instruction, const string& name)
{
	de::MovePtr<tcu::TestCaseGroup>		group(new tcu::TestCaseGroup(testCtx, name.c_str(), instruction.c_str()));
	vector<ConvertCase>					testCases;
	createConvertCases(testCases, instruction);

	for (vector<ConvertCase>::const_iterator test = testCases.begin(); test != testCases.end(); ++test)
	{
		map<string, string>	fragments		= (test->m_useStorageExt) ? getConvertCaseFragments(instruction, *test) : getConvertCaseFragmentsNoStorage(instruction,*test);
		VulkanFeatures		vulkanFeatures;
		GraphicsResources	resources;
		vector<string>		extensions;
		SpecConstants		noSpecConstants;
		PushConstants		noPushConstants;
		GraphicsInterfaces	noInterfaces;
		tcu::RGBA			defaultColors[4];

		getDefaultColors			(defaultColors);
		resources.inputs.push_back	(Resource(test->m_inputBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		resources.outputs.push_back	(Resource(test->m_outputBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		extensions.push_back		("VK_KHR_storage_buffer_storage_class");

		getVulkanFeaturesAndExtensions(test->m_fromType, test->m_toType, test->m_useStorageExt, vulkanFeatures, extensions);

		vulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics	= true;
		vulkanFeatures.coreFeatures.fragmentStoresAndAtomics		= true;

		createTestsForAllStages(
			test->m_name, defaultColors, defaultColors, fragments, noSpecConstants,
			noPushConstants, resources, noInterfaces, extensions, vulkanFeatures, group.get());
	}
	return group.release();
}

// Constant-Creation Instructions: OpConstant, OpConstantComposite
tcu::TestCaseGroup* createOpConstantFloat16Tests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> opConstantCompositeTests		(new tcu::TestCaseGroup(testCtx, "opconstant", "OpConstant and OpConstantComposite instruction"));
	RGBA							inputColors[4];
	RGBA							outputColors[4];
	vector<string>					extensions;
	GraphicsResources				resources;
	VulkanFeatures					features;

	const char						functionStart[]	 =
		"%test_code             = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1                = OpFunctionParameter %v4f32\n"
		"%lbl                   = OpLabel\n";

	const char						functionEnd[]		=
		"%transformed_param_32  = OpFConvert %v4f32 %transformed_param\n"
		"                         OpReturnValue %transformed_param_32\n"
		"                         OpFunctionEnd\n";

	struct NameConstantsCode
	{
		string name;
		string constants;
		string code;
	};

#define FLOAT_16_COMMON_TYPES_AND_CONSTS \
			"%f16                  = OpTypeFloat 16\n"                                                 \
			"%c_f16_0              = OpConstant %f16 0.0\n"                                            \
			"%c_f16_0_5            = OpConstant %f16 0.5\n"                                            \
			"%c_f16_1              = OpConstant %f16 1.0\n"                                            \
			"%v4f16                = OpTypeVector %f16 4\n"                                            \
			"%fp_f16               = OpTypePointer Function %f16\n"                                    \
			"%fp_v4f16             = OpTypePointer Function %v4f16\n"                                  \
			"%c_v4f16_1_1_1_1      = OpConstantComposite %v4f16 %c_f16_1 %c_f16_1 %c_f16_1 %c_f16_1\n" \
			"%a4f16                = OpTypeArray %f16 %c_u32_4\n"                                      \

	NameConstantsCode				tests[] =
	{
		{
			"vec4",

			FLOAT_16_COMMON_TYPES_AND_CONSTS
			"%cval                 = OpConstantComposite %v4f16 %c_f16_0_5 %c_f16_0_5 %c_f16_0_5 %c_f16_0\n",
			"%param1_16            = OpFConvert %v4f16 %param1\n"
			"%transformed_param    = OpFAdd %v4f16 %param1_16 %cval\n"
		},
		{
			"struct",

			FLOAT_16_COMMON_TYPES_AND_CONSTS
			"%stype                = OpTypeStruct %v4f16 %f16\n"
			"%fp_stype             = OpTypePointer Function %stype\n"
			"%f16_n_1              = OpConstant %f16 -1.0\n"
			"%f16_1_5              = OpConstant %f16 !0x3e00\n" // +1.5
			"%cvec                 = OpConstantComposite %v4f16 %f16_1_5 %f16_1_5 %f16_1_5 %c_f16_1\n"
			"%cval                 = OpConstantComposite %stype %cvec %f16_n_1\n",

			"%v                    = OpVariable %fp_stype Function %cval\n"
			"%vec_ptr              = OpAccessChain %fp_v4f16 %v %c_u32_0\n"
			"%f16_ptr              = OpAccessChain %fp_f16 %v %c_u32_1\n"
			"%vec_val              = OpLoad %v4f16 %vec_ptr\n"
			"%f16_val              = OpLoad %f16 %f16_ptr\n"
			"%tmp1                 = OpVectorTimesScalar %v4f16 %c_v4f16_1_1_1_1 %f16_val\n" // vec4(-1)
			"%param1_16            = OpFConvert %v4f16 %param1\n"
			"%tmp2                 = OpFAdd %v4f16 %tmp1 %param1_16\n" // param1 + vec4(-1)
			"%transformed_param    = OpFAdd %v4f16 %tmp2 %vec_val\n" // param1 + vec4(-1) + vec4(1.5, 1.5, 1.5, 1.0)
		},
		{
			// [1|0|0|0.5] [x] = x + 0.5
			// [0|1|0|0.5] [y] = y + 0.5
			// [0|0|1|0.5] [z] = z + 0.5
			// [0|0|0|1  ] [1] = 1
			"matrix",

			FLOAT_16_COMMON_TYPES_AND_CONSTS
			"%mat4x4_f16           = OpTypeMatrix %v4f16 4\n"
			"%v4f16_1_0_0_0        = OpConstantComposite %v4f16 %c_f16_1 %c_f16_0 %c_f16_0 %c_f16_0\n"
			"%v4f16_0_1_0_0        = OpConstantComposite %v4f16 %c_f16_0 %c_f16_1 %c_f16_0 %c_f16_0\n"
			"%v4f16_0_0_1_0        = OpConstantComposite %v4f16 %c_f16_0 %c_f16_0 %c_f16_1 %c_f16_0\n"
			"%v4f16_0_5_0_5_0_5_1  = OpConstantComposite %v4f16 %c_f16_0_5 %c_f16_0_5 %c_f16_0_5 %c_f16_1\n"
			"%cval                 = OpConstantComposite %mat4x4_f16 %v4f16_1_0_0_0 %v4f16_0_1_0_0 %v4f16_0_0_1_0 %v4f16_0_5_0_5_0_5_1\n",

			"%param1_16            = OpFConvert %v4f16 %param1\n"
			"%transformed_param    = OpMatrixTimesVector %v4f16 %cval %param1_16\n"
		},
		{
			"array",

			FLOAT_16_COMMON_TYPES_AND_CONSTS
			"%c_v4f16_1_1_1_0      = OpConstantComposite %v4f16 %c_f16_1 %c_f16_1 %c_f16_1 %c_f16_0\n"
			"%fp_a4f16             = OpTypePointer Function %a4f16\n"
			"%f16_n_1              = OpConstant %f16 -1.0\n"
			"%f16_1_5              = OpConstant %f16 !0x3e00\n" // +1.5
			"%carr                 = OpConstantComposite %a4f16 %c_f16_0 %f16_n_1 %f16_1_5 %c_f16_0\n",

			"%v                    = OpVariable %fp_a4f16 Function %carr\n"
			"%f                    = OpAccessChain %fp_f16 %v %c_u32_0\n"
			"%f1                   = OpAccessChain %fp_f16 %v %c_u32_1\n"
			"%f2                   = OpAccessChain %fp_f16 %v %c_u32_2\n"
			"%f3                   = OpAccessChain %fp_f16 %v %c_u32_3\n"
			"%f_val                = OpLoad %f16 %f\n"
			"%f1_val               = OpLoad %f16 %f1\n"
			"%f2_val               = OpLoad %f16 %f2\n"
			"%f3_val               = OpLoad %f16 %f3\n"
			"%ftot1                = OpFAdd %f16 %f_val %f1_val\n"
			"%ftot2                = OpFAdd %f16 %ftot1 %f2_val\n"
			"%ftot3                = OpFAdd %f16 %ftot2 %f3_val\n"  // 0 - 1 + 1.5 + 0
			"%add_vec              = OpVectorTimesScalar %v4f16 %c_v4f16_1_1_1_0 %ftot3\n"
			"%param1_16            = OpFConvert %v4f16 %param1\n"
			"%transformed_param    = OpFAdd %v4f16 %param1_16 %add_vec\n"
		},
		{
			//
			// [
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   },
			//   {
			//      1.0,
			//      [ 0.0, 0.5, 0.0, 0.0]
			//   }, //     ^^^
			//   {
			//      0.0,
			//      [ 1.0, 1.0, 1.0, 1.0]
			//   }
			// ]
			"array_of_struct_of_array",

			FLOAT_16_COMMON_TYPES_AND_CONSTS
			"%c_v4f16_1_1_1_0      = OpConstantComposite %v4f16 %c_f16_1 %c_f16_1 %c_f16_1 %c_f16_0\n"
			"%fp_a4f16             = OpTypePointer Function %a4f16\n"
			"%stype                = OpTypeStruct %f16 %a4f16\n"
			"%a3stype              = OpTypeArray %stype %c_u32_3\n"
			"%fp_a3stype           = OpTypePointer Function %a3stype\n"
			"%ca4f16_0             = OpConstantComposite %a4f16 %c_f16_0 %c_f16_0_5 %c_f16_0 %c_f16_0\n"
			"%ca4f16_1             = OpConstantComposite %a4f16 %c_f16_1 %c_f16_1 %c_f16_1 %c_f16_1\n"
			"%cstype1              = OpConstantComposite %stype %c_f16_0 %ca4f16_1\n"
			"%cstype2              = OpConstantComposite %stype %c_f16_1 %ca4f16_0\n"
			"%carr                 = OpConstantComposite %a3stype %cstype1 %cstype2 %cstype1",

			"%v                    = OpVariable %fp_a3stype Function %carr\n"
			"%f                    = OpAccessChain %fp_f16 %v %c_u32_1 %c_u32_1 %c_u32_1\n"
			"%f_l                  = OpLoad %f16 %f\n"
			"%add_vec              = OpVectorTimesScalar %v4f16 %c_v4f16_1_1_1_0 %f_l\n"
			"%param1_16            = OpFConvert %v4f16 %param1\n"
			"%transformed_param    = OpFAdd %v4f16 %param1_16 %add_vec\n"
		}
	};

	getHalfColorsFullAlpha(inputColors);
	outputColors[0] = RGBA(255, 255, 255, 255);
	outputColors[1] = RGBA(255, 127, 127, 255);
	outputColors[2] = RGBA(127, 255, 127, 255);
	outputColors[3] = RGBA(127, 127, 255, 255);

	extensions.push_back("VK_KHR_shader_float16_int8");
	features.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;

	for (size_t testNdx = 0; testNdx < sizeof(tests) / sizeof(NameConstantsCode); ++testNdx)
	{
		map<string, string> fragments;

		fragments["capability"]	= "OpCapability Float16\n";
		fragments["pre_main"]	= tests[testNdx].constants;
		fragments["testfun"]	= string(functionStart) + tests[testNdx].code + functionEnd;

		createTestsForAllStages(tests[testNdx].name, inputColors, outputColors, fragments, resources, extensions, opConstantCompositeTests.get(), features);
	}
	return opConstantCompositeTests.release();
}

template<typename T>
void finalizeTestsCreation (T&							specResource,
							const map<string, string>&	fragments,
							tcu::TestContext&			testCtx,
							tcu::TestCaseGroup&			testGroup,
							const std::string&			testName,
							const VulkanFeatures&		vulkanFeatures,
							const vector<string>&		extensions,
							const IVec3&				numWorkGroups,
							const bool					splitRenderArea = false);

template<>
void finalizeTestsCreation (GraphicsResources&			specResource,
							const map<string, string>&	fragments,
							tcu::TestContext&			,
							tcu::TestCaseGroup&			testGroup,
							const std::string&			testName,
							const VulkanFeatures&		vulkanFeatures,
							const vector<string>&		extensions,
							const IVec3&				,
							const bool					splitRenderArea)
{
	RGBA defaultColors[4];
	getDefaultColors(defaultColors);

	createTestsForAllStages(testName, defaultColors, defaultColors, fragments, specResource, extensions, &testGroup, vulkanFeatures, QP_TEST_RESULT_FAIL, std::string(), splitRenderArea);
}

template<>
void finalizeTestsCreation (ComputeShaderSpec&			specResource,
							const map<string, string>&	fragments,
							tcu::TestContext&			testCtx,
							tcu::TestCaseGroup&			testGroup,
							const std::string&			testName,
							const VulkanFeatures&		vulkanFeatures,
							const vector<string>&		extensions,
							const IVec3&				numWorkGroups,
							bool)
{
	specResource.numWorkGroups = numWorkGroups;
	specResource.requestedVulkanFeatures = vulkanFeatures;
	specResource.extensions = extensions;

	specResource.assembly = makeComputeShaderAssembly(fragments);

	testGroup.addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), "", specResource));
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16LogicalSet (tcu::TestContext& testCtx, const bool nanSupported)
{
	const string						nan					= nanSupported ? "_nan" : "";
	const string						groupName			= "logical" + nan;
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, groupName.c_str(), "Float 16 logical tests"));

	de::Random							rnd					(deStringHash(testGroup->getName()));
	const string						spvCapabilities		= string("OpCapability Float16\n") + (nanSupported ? "OpCapability SignedZeroInfNanPreserve\n" : "");
	const string						spvExtensions		= (nanSupported ? "OpExtension \"SPV_KHR_float_controls\"\n" : "");
	const string						spvExecutionMode	= nanSupported ? "OpExecutionMode %BP_main SignedZeroInfNanPreserve 16\n" : "";
	const deUint32						numDataPointsScalar	= 16;
	const deUint32						numDataPointsVector	= 14;
	const vector<deFloat16>				float16DataScalar	= getFloat16s(rnd, numDataPointsScalar);
	const vector<deFloat16>				float16DataVector	= getFloat16s(rnd, numDataPointsVector);
	const vector<deFloat16>				float16Data1		= squarize(float16DataScalar, 0);			// Total Size: square(sizeof(float16DataScalar))
	const vector<deFloat16>				float16Data2		= squarize(float16DataScalar, 1);
	const vector<deFloat16>				float16DataVec1		= squarizeVector(float16DataVector, 0);		// Total Size: 2 * (square(square(sizeof(float16DataVector))))
	const vector<deFloat16>				float16DataVec2		= squarizeVector(float16DataVector, 1);
	const vector<deFloat16>				float16OutDummy		(float16Data1.size(), 0);
	const vector<deFloat16>				float16OutVecDummy	(float16DataVec1.size(), 0);

	struct TestOp
	{
		const char*		opCode;
		VerifyIOFunc	verifyFuncNan;
		VerifyIOFunc	verifyFuncNonNan;
		const deUint32	argCount;
	};

	const TestOp	testOps[]	=
	{
		{ "OpIsNan"						,	compareFP16Logical<fp16isNan,				true,  false, true>,	compareFP16Logical<fp16isNan,				true,  false, false>,	1	},
		{ "OpIsInf"						,	compareFP16Logical<fp16isInf,				true,  false, true>,	compareFP16Logical<fp16isInf,				true,  false, false>,	1	},
		{ "OpFOrdEqual"					,	compareFP16Logical<fp16isEqual,				false, true,  true>,	compareFP16Logical<fp16isEqual,				false, true,  false>,	2	},
		{ "OpFUnordEqual"				,	compareFP16Logical<fp16isEqual,				false, false, true>,	compareFP16Logical<fp16isEqual,				false, false, false>,	2	},
		{ "OpFOrdNotEqual"				,	compareFP16Logical<fp16isUnequal,			false, true,  true>,	compareFP16Logical<fp16isUnequal,			false, true,  false>,	2	},
		{ "OpFUnordNotEqual"			,	compareFP16Logical<fp16isUnequal,			false, false, true>,	compareFP16Logical<fp16isUnequal,			false, false, false>,	2	},
		{ "OpFOrdLessThan"				,	compareFP16Logical<fp16isLess,				false, true,  true>,	compareFP16Logical<fp16isLess,				false, true,  false>,	2	},
		{ "OpFUnordLessThan"			,	compareFP16Logical<fp16isLess,				false, false, true>,	compareFP16Logical<fp16isLess,				false, false, false>,	2	},
		{ "OpFOrdGreaterThan"			,	compareFP16Logical<fp16isGreater,			false, true,  true>,	compareFP16Logical<fp16isGreater,			false, true,  false>,	2	},
		{ "OpFUnordGreaterThan"			,	compareFP16Logical<fp16isGreater,			false, false, true>,	compareFP16Logical<fp16isGreater,			false, false, false>,	2	},
		{ "OpFOrdLessThanEqual"			,	compareFP16Logical<fp16isLessOrEqual,		false, true,  true>,	compareFP16Logical<fp16isLessOrEqual,		false, true,  false>,	2	},
		{ "OpFUnordLessThanEqual"		,	compareFP16Logical<fp16isLessOrEqual,		false, false, true>,	compareFP16Logical<fp16isLessOrEqual,		false, false, false>,	2	},
		{ "OpFOrdGreaterThanEqual"		,	compareFP16Logical<fp16isGreaterOrEqual,	false, true,  true>,	compareFP16Logical<fp16isGreaterOrEqual,	false, true,  false>,	2	},
		{ "OpFUnordGreaterThanEqual"	,	compareFP16Logical<fp16isGreaterOrEqual,	false, false, true>,	compareFP16Logical<fp16isGreaterOrEqual,	false, false, false>,	2	},
	};

	{ // scalar cases
		const StringTemplate preMain
		(
			"      %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
			"     %c_i32_hndp = OpSpecConstantOp %i32 SDiv %c_i32_ndp %c_i32_2\n"
			"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
			" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n"
			"            %f16 = OpTypeFloat 16\n"
			"          %v2f16 = OpTypeVector %f16 2\n"
			"        %c_f16_0 = OpConstant %f16 0.0\n"
			"        %c_f16_1 = OpConstant %f16 1.0\n"
			"         %up_u32 = OpTypePointer Uniform %u32\n"
			"         %ra_u32 = OpTypeArray %u32 %c_i32_hndp\n"
			"         %SSBO16 = OpTypeStruct %ra_u32\n"
			"      %up_SSBO16 = OpTypePointer Uniform %SSBO16\n"
			"     %f16_i32_fn = OpTypeFunction %f16 %i32\n"
			"%void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
			"      %ssbo_src0 = OpVariable %up_SSBO16 Uniform\n"
			"      %ssbo_src1 = OpVariable %up_SSBO16 Uniform\n"
			"       %ssbo_dst = OpVariable %up_SSBO16 Uniform\n"
		);

		const StringTemplate decoration
		(
			"OpDecorate %ra_u32 ArrayStride 4\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpDecorate %SSBO16 BufferBlock\n"
			"OpDecorate %ssbo_src0 DescriptorSet 0\n"
			"OpDecorate %ssbo_src0 Binding 0\n"
			"OpDecorate %ssbo_src1 DescriptorSet 0\n"
			"OpDecorate %ssbo_src1 Binding 1\n"
			"OpDecorate %ssbo_dst DescriptorSet 0\n"
			"OpDecorate %ssbo_dst Binding 2\n"
		);

		const StringTemplate testFun
		(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"    %entry = OpLabel\n"
			"        %i = OpVariable %fp_i32 Function\n"
			"             OpStore %i %c_i32_0\n"
			"             OpBranch %loop\n"

			"     %loop = OpLabel\n"
			"    %i_cmp = OpLoad %i32 %i\n"
			"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
			"             OpLoopMerge %merge %next None\n"
			"             OpBranchConditional %lt %write %merge\n"

			"    %write = OpLabel\n"
			"      %ndx = OpLoad %i32 %i\n"

			" %val_src0 = OpFunctionCall %f16 %ld_arg_ssbo_src0 %ndx\n"

			"${op_arg1_calc}"

			" %val_bdst = ${op_code} %bool %val_src0 ${op_arg1}\n"
			"  %val_dst = OpSelect %f16 %val_bdst %c_f16_1 %c_f16_0\n"
			"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"
			"             OpBranch %next\n"

			"     %next = OpLabel\n"
			"    %i_cur = OpLoad %i32 %i\n"
			"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
			"             OpStore %i %i_new\n"
			"             OpBranch %loop\n"

			"    %merge = OpLabel\n"
			"             OpReturnValue %param\n"

			"             OpFunctionEnd\n"
		);

		const StringTemplate arg1Calc
		(
			" %val_src1 = OpFunctionCall %f16 %ld_arg_ssbo_src1 %ndx\n"
		);

		for (deUint32 testOpsIdx = 0; testOpsIdx < DE_LENGTH_OF_ARRAY(testOps); ++testOpsIdx)
		{
			const size_t		iterations		= float16Data1.size();
			const TestOp&		testOp			= testOps[testOpsIdx];
			const string		testName		= de::toLower(string(testOp.opCode)) + "_scalar";
			SpecResource		specResource;
			map<string, string>	specs;
			VulkanFeatures		features;
			map<string, string>	fragments;
			vector<string>		extensions;

			specs["num_data_points"]	= de::toString(iterations);
			specs["op_code"]			= testOp.opCode;
			specs["op_arg1"]			= (testOp.argCount == 1) ? "" : "%val_src1";
			specs["op_arg1_calc"]		= (testOp.argCount == 1) ? "" : arg1Calc.specialize(specs);

			fragments["extension"]		= spvExtensions;
			fragments["capability"]		= spvCapabilities;
			fragments["execution_mode"]	= spvExecutionMode;
			fragments["decoration"]		= decoration.specialize(specs);
			fragments["pre_main"]		= preMain.specialize(specs);
			fragments["testfun"]		= testFun.specialize(specs);
			fragments["testfun"]		+= StringTemplate(loadScalarF16FromUint).specialize({{"var", "ssbo_src0"}});
			if (testOp.argCount > 1)
			{
				fragments["testfun"]	+= StringTemplate(loadScalarF16FromUint).specialize({{"var", "ssbo_src1"}});
			}
			fragments["testfun"]		+= StringTemplate(storeScalarF16AsUint).specialize({{"var", "ssbo_dst"}});

			specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16Data1)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16Data2)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.verifyIO = nanSupported ? testOp.verifyFuncNan : testOp.verifyFuncNonNan;

			extensions.push_back("VK_KHR_shader_float16_int8");

			if (nanSupported)
			{
				extensions.push_back("VK_KHR_shader_float_controls");

				features.floatControlsProperties.shaderSignedZeroInfNanPreserveFloat16 = DE_TRUE;
			}

			features.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;

			finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
		}
	}
	{ // vector cases
		const StringTemplate preMain
		(
			"        %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
			"           %v2bool = OpTypeVector %bool 2\n"
			"              %f16 = OpTypeFloat 16\n"
			"          %c_f16_0 = OpConstant %f16 0.0\n"
			"          %c_f16_1 = OpConstant %f16 1.0\n"
			"            %v2f16 = OpTypeVector %f16 2\n"
			"      %c_v2f16_0_0 = OpConstantComposite %v2f16 %c_f16_0 %c_f16_0\n"
			"      %c_v2f16_1_1 = OpConstantComposite %v2f16 %c_f16_1 %c_f16_1\n"
			"           %up_u32 = OpTypePointer Uniform %u32\n"
			"           %ra_u32 = OpTypeArray %u32 %c_i32_ndp\n"
			"           %SSBO16 = OpTypeStruct %ra_u32\n"
			"        %up_SSBO16 = OpTypePointer Uniform %SSBO16\n"
			"     %v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
			"%void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n"
			"        %ssbo_src0 = OpVariable %up_SSBO16 Uniform\n"
			"        %ssbo_src1 = OpVariable %up_SSBO16 Uniform\n"
			"         %ssbo_dst = OpVariable %up_SSBO16 Uniform\n"
		);

		const StringTemplate decoration
		(
			"OpDecorate %ra_u32 ArrayStride 4\n"
			"OpMemberDecorate %SSBO16 0 Offset 0\n"
			"OpDecorate %SSBO16 BufferBlock\n"
			"OpDecorate %ssbo_src0 DescriptorSet 0\n"
			"OpDecorate %ssbo_src0 Binding 0\n"
			"OpDecorate %ssbo_src1 DescriptorSet 0\n"
			"OpDecorate %ssbo_src1 Binding 1\n"
			"OpDecorate %ssbo_dst DescriptorSet 0\n"
			"OpDecorate %ssbo_dst Binding 2\n"
		);

		const StringTemplate testFun
		(
			"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
			"    %param = OpFunctionParameter %v4f32\n"

			"    %entry = OpLabel\n"
			"        %i = OpVariable %fp_i32 Function\n"
			"             OpStore %i %c_i32_0\n"
			"             OpBranch %loop\n"

			"     %loop = OpLabel\n"
			"    %i_cmp = OpLoad %i32 %i\n"
			"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
			"             OpLoopMerge %merge %next None\n"
			"             OpBranchConditional %lt %write %merge\n"

			"    %write = OpLabel\n"
			"      %ndx = OpLoad %i32 %i\n"

			" %val_src0 = OpFunctionCall %v2f16 %ld_arg_ssbo_src0 %ndx\n"

			"${op_arg1_calc}"

			" %val_bdst = ${op_code} %v2bool %val_src0 ${op_arg1}\n"
			"  %val_dst = OpSelect %v2f16 %val_bdst %c_v2f16_1_1 %c_v2f16_0_0\n"
			"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"
			"             OpBranch %next\n"

			"     %next = OpLabel\n"
			"    %i_cur = OpLoad %i32 %i\n"
			"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
			"             OpStore %i %i_new\n"
			"             OpBranch %loop\n"

			"    %merge = OpLabel\n"
			"             OpReturnValue %param\n"

			"             OpFunctionEnd\n"
		);

		const StringTemplate arg1Calc
		(
			" %val_src1 = OpFunctionCall %v2f16 %ld_arg_ssbo_src1 %ndx\n"
		);

		for (deUint32 testOpsIdx = 0; testOpsIdx < DE_LENGTH_OF_ARRAY(testOps); ++testOpsIdx)
		{
			const deUint32		itemsPerVec	= 2;
			const size_t		iterations	= float16DataVec1.size() / itemsPerVec;
			const TestOp&		testOp		= testOps[testOpsIdx];
			const string		testName	= de::toLower(string(testOp.opCode)) + "_vector";
			SpecResource		specResource;
			map<string, string>	specs;
			vector<string>		extensions;
			VulkanFeatures		features;
			map<string, string>	fragments;

			specs["num_data_points"]	= de::toString(iterations);
			specs["op_code"]			= testOp.opCode;
			specs["op_arg1"]			= (testOp.argCount == 1) ? "" : "%val_src1";
			specs["op_arg1_calc"]		= (testOp.argCount == 1) ? "" : arg1Calc.specialize(specs);

			fragments["extension"]		= spvExtensions;
			fragments["capability"]		= spvCapabilities;
			fragments["execution_mode"]	= spvExecutionMode;
			fragments["decoration"]		= decoration.specialize(specs);
			fragments["pre_main"]		= preMain.specialize(specs);
			fragments["testfun"]		= testFun.specialize(specs);
			fragments["testfun"]		+= StringTemplate(loadV2F16FromUint).specialize({{"var", "ssbo_src0"}});
			if (testOp.argCount > 1)
			{
				fragments["testfun"]	+= StringTemplate(loadV2F16FromUint).specialize({{"var", "ssbo_src1"}});
			}
			fragments["testfun"]		+= StringTemplate(storeV2F16AsUint).specialize({{"var", "ssbo_dst"}});

			specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16DataVec1)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16DataVec2)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutVecDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
			specResource.verifyIO = nanSupported ? testOp.verifyFuncNan : testOp.verifyFuncNonNan;

			extensions.push_back("VK_KHR_shader_float16_int8");

			if (nanSupported)
			{
				extensions.push_back("VK_KHR_shader_float_controls");

				features.floatControlsProperties.shaderSignedZeroInfNanPreserveFloat16 = DE_TRUE;
			}

			features.extFloat16Int8 = EXTFLOAT16INT8FEATURES_FLOAT16;

			finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1), true);
		}
	}

	return testGroup.release();
}

bool compareFP16FunctionSetFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	if (inputs.size() != 1 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;

	inputs[0].getBytes(input1Bytes);

	const deUint16* const	input1AsFP16	= (const deUint16*)&input1Bytes[0];
	const deUint16* const	outputAsFP16	= (const deUint16*)outputAllocs[0]->getHostPtr();
	std::string				error;

	for (size_t idx = 0; idx < input1Bytes.size() / sizeof(deUint16); ++idx)
	{
		if (!compare16BitFloat(input1AsFP16[idx], outputAsFP16[idx], error))
		{
			log << TestLog::Message << error << TestLog::EndMessage;

			return false;
		}
	}

	return true;
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16FuncSet (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "function", "Float 16 function call related tests"));

	de::Random							rnd					(deStringHash(testGroup->getName()));
	const StringTemplate				capabilities		("OpCapability Float16\n");
	const deUint32						numDataPoints		= 256;
	const vector<deFloat16>				float16InputData	= getFloat16s(rnd, numDataPoints);
	const vector<deFloat16>				float16OutputDummy	(float16InputData.size(), 0);
	map<string, string>					fragments;

	struct TestType
	{
		const deUint32	typeComponents;
		const char*		typeName;
		const char*		typeDecls;
		const char*		typeStorage;
		const string		loadFunc;
		const string		storeFunc;
	};

	const TestType	testTypes[]	=
	{
		{
			1,
			"f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"%f16_i32_fn = OpTypeFunction %f16 %i32\n"
			"%void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
			"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
			" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n",
			"u32_hndp",
			loadScalarF16FromUint,
			storeScalarF16AsUint
		},
		{
			2,
			"v2f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"  %c_v2f16_0 = OpConstantComposite %v2f16 %c_f16_0 %c_f16_0\n"
			"%v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
			"%void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n",
			"u32_ndp",
			loadV2F16FromUint,
			storeV2F16AsUint
		},
		{
			4,
			"v4f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"      %v4f16 = OpTypeVector %f16 4\n"
			"  %c_v4f16_0 = OpConstantComposite %v4f16 %c_f16_0 %c_f16_0 %c_f16_0 %c_f16_0\n"
			"%v4f16_i32_fn = OpTypeFunction %v4f16 %i32\n"
			"%void_v4f16_i32_fn = OpTypeFunction %void %v4f16 %i32\n",
			"ra_u32_2",
			loadV4F16FromUints,
			storeV4F16AsUints
		},
	};

	const StringTemplate preMain
	(
		"  %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		" %c_i32_hndp = OpSpecConstantOp %i32 SDiv %c_i32_ndp %c_i32_2\n"
		"     %v2bool = OpTypeVector %bool 2\n"
		"        %f16 = OpTypeFloat 16\n"
		"    %c_f16_0 = OpConstant %f16 0.0\n"

		"${type_decls}"

		"  %${tt}_fun = OpTypeFunction %${tt} %${tt}\n"
		"   %ra_u32_2 = OpTypeArray %u32 %c_u32_2\n"
		"%ra_u32_hndp = OpTypeArray %u32 %c_i32_hndp\n"
		" %ra_u32_ndp = OpTypeArray %u32 %c_i32_ndp\n"
		"%ra_ra_u32_2 = OpTypeArray %ra_u32_2 %c_i32_ndp\n"
		"	  %up_u32 = OpTypePointer Uniform %u32\n"
		"     %SSBO16 = OpTypeStruct %ra_${ts}\n"
		"  %up_SSBO16 = OpTypePointer Uniform %SSBO16\n"
		"   %ssbo_src = OpVariable %up_SSBO16 Uniform\n"
		"   %ssbo_dst = OpVariable %up_SSBO16 Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %ra_u32_2 ArrayStride 4\n"
		"OpDecorate %ra_u32_hndp ArrayStride 4\n"
		"OpDecorate %ra_u32_ndp ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_2 ArrayStride 8\n"
		"OpMemberDecorate %SSBO16 0 Offset 0\n"
		"OpDecorate %SSBO16 BufferBlock\n"
		"OpDecorate %ssbo_src DescriptorSet 0\n"
		"OpDecorate %ssbo_src Binding 0\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 1\n"
	);

	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"    %entry = OpLabel\n"

		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"  %val_src = OpFunctionCall %${tt} %ld_arg_ssbo_src %ndx\n"
		"  %val_dst = OpFunctionCall %${tt} %pass_fun %val_src\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"
		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpReturnValue %param\n"

		"             OpFunctionEnd\n"

		" %pass_fun = OpFunction %${tt} None %${tt}_fun\n"
		"   %param0 = OpFunctionParameter %${tt}\n"
		" %entry_pf = OpLabel\n"
		"     %res0 = OpFAdd %${tt} %param0 %c_${tt}_0\n"
		"             OpReturnValue %res0\n"
		"             OpFunctionEnd\n"
	);

	for (deUint32 testTypeIdx = 0; testTypeIdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeIdx)
	{
		const TestType&		testType		= testTypes[testTypeIdx];
		const string		testName		= testType.typeName;
		const deUint32		itemsPerType	= testType.typeComponents;
		const size_t		iterations		= float16InputData.size() / itemsPerType;
		const size_t		typeStride		= itemsPerType * sizeof(deFloat16);
		SpecResource		specResource;
		map<string, string>	specs;
		VulkanFeatures		features;
		vector<string>		extensions;

		specs["num_data_points"]	= de::toString(iterations);
		specs["tt"]					= testType.typeName;
		specs["ts"]					= testType.typeStorage;
		specs["tt_stride"]			= de::toString(typeStride);
		specs["type_decls"]			= testType.typeDecls;

		fragments["capability"]		= capabilities.specialize(specs);
		fragments["decoration"]		= decoration.specialize(specs);
		fragments["pre_main"]		= preMain.specialize(specs);
		fragments["testfun"]		= testFun.specialize(specs);
		fragments["testfun"]		+= StringTemplate(testType.loadFunc).specialize({{"var", "ssbo_src"}});
		fragments["testfun"]		+= StringTemplate(testType.storeFunc).specialize({{"var", "ssbo_dst"}});

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16InputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutputDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.verifyIO = compareFP16FunctionSetFunc;

		extensions.push_back("VK_KHR_shader_float16_int8");

		features.extFloat16Int8	= EXTFLOAT16INT8FEATURES_FLOAT16;

		finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
	}

	return testGroup.release();
}

bool compareFP16VectorExtractFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	if (inputs.size() != 2 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;
	vector<deUint8>	input2Bytes;

	inputs[0].getBytes(input1Bytes);
	inputs[1].getBytes(input2Bytes);

	DE_ASSERT(input1Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() % sizeof(deUint32) == 0);

	const size_t			iterations		= input2Bytes.size() / sizeof(deUint32);
	const size_t			components		= input1Bytes.size() / (sizeof(deFloat16) * iterations);
	const deFloat16* const	input1AsFP16	= (const deFloat16*)&input1Bytes[0];
	const deUint32* const	inputIndices	= (const deUint32*)&input2Bytes[0];
	const deFloat16* const	outputAsFP16	= (const deFloat16*)outputAllocs[0]->getHostPtr();
	std::string				error;

	DE_ASSERT(components == 2 || components == 4);
	DE_ASSERT(input1Bytes.size() == iterations * components * sizeof(deFloat16));

	for (size_t idx = 0; idx < iterations; ++idx)
	{
		const deUint32	componentNdx	= inputIndices[idx];

		DE_ASSERT(componentNdx < components);

		const deFloat16	expected		= input1AsFP16[components * idx + componentNdx];

		if (!compare16BitFloat(expected, outputAsFP16[idx], error))
		{
			log << TestLog::Message << "At " << idx << error << TestLog::EndMessage;

			return false;
		}
	}

	return true;
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16VectorExtractSet (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "opvectorextractdynamic", "OpVectorExtractDynamic tests"));

	de::Random							rnd					(deStringHash(testGroup->getName()));
	const deUint32						numDataPoints		= 256;
	const vector<deFloat16>				float16InputData	= getFloat16s(rnd, numDataPoints);
	const vector<deFloat16>				float16OutputDummy	(float16InputData.size(), 0);

	struct TestType
	{
		const deUint32	typeComponents;
		const size_t	typeStride;
		const char*		typeName;
		const char*		typeDecls;
		const char*		typeStorage;
		const string		loadFunction;
		const string		storeFunction;
	};

	const TestType	testTypes[]	=
	{
		{
			2,
			2 * sizeof(deFloat16),
			"v2f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"%v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
			"%void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
			"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
			" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n",
			"u32",
			loadV2F16FromUint,
			storeScalarF16AsUint
		},
		{
			3,
			4 * sizeof(deFloat16),
			"v3f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"      %v3f16 = OpTypeVector %f16 3\n"
			"%v3f16_i32_fn = OpTypeFunction %v3f16 %i32\n"
			"%void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
			"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
			" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n",
			"ra_u32_2",
			loadV3F16FromUints,
			storeScalarF16AsUint
		},
		{
			4,
			4 * sizeof(deFloat16),
			"v4f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"      %v4f16 = OpTypeVector %f16 4\n"
			"%v4f16_i32_fn = OpTypeFunction %v4f16 %i32\n"
			"%void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
			"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
			" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n",
			"ra_u32_2",
			loadV4F16FromUints,
			storeScalarF16AsUint
		},
	};

	const StringTemplate preMain
	(
		"  %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		" %c_i32_hndp = OpSpecConstantOp %i32 SDiv %c_i32_ndp %c_i32_2\n"
		"        %f16 = OpTypeFloat 16\n"

		"${type_decl}"

		"     %up_u32 = OpTypePointer Uniform %u32\n"
		"     %ra_u32 = OpTypeArray %u32 %c_i32_ndp\n"
		"   %SSBO_IDX = OpTypeStruct %ra_u32\n"
		"%up_SSBO_IDX = OpTypePointer Uniform %SSBO_IDX\n"

		"   %ra_u32_2 = OpTypeArray %u32 %c_u32_2\n"
		" %ra_u32_ndp = OpTypeArray %u32 %c_i32_ndp\n"
		"%ra_ra_u32_2 = OpTypeArray %ra_u32_2 %c_i32_ndp\n"
		"   %SSBO_SRC = OpTypeStruct %ra_${ts}\n"
		"%up_SSBO_SRC = OpTypePointer Uniform %SSBO_SRC\n"

		" %ra_u32_hndp = OpTypeArray %u32 %c_i32_hndp\n"
		"   %SSBO_DST = OpTypeStruct %ra_u32_hndp\n"
		"%up_SSBO_DST = OpTypePointer Uniform %SSBO_DST\n"

		"   %ssbo_src = OpVariable %up_SSBO_SRC Uniform\n"
		"   %ssbo_idx = OpVariable %up_SSBO_IDX Uniform\n"
		"   %ssbo_dst = OpVariable %up_SSBO_DST Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %ra_u32_2 ArrayStride 4\n"
		"OpDecorate %ra_u32_hndp ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_2 ArrayStride 8\n"
		"OpMemberDecorate %SSBO_SRC 0 Offset 0\n"
		"OpDecorate %SSBO_SRC BufferBlock\n"
		"OpDecorate %ssbo_src DescriptorSet 0\n"
		"OpDecorate %ssbo_src Binding 0\n"

		"OpDecorate %ra_u32 ArrayStride 4\n"
		"OpMemberDecorate %SSBO_IDX 0 Offset 0\n"
		"OpDecorate %SSBO_IDX BufferBlock\n"
		"OpDecorate %ssbo_idx DescriptorSet 0\n"
		"OpDecorate %ssbo_idx Binding 1\n"

		"OpMemberDecorate %SSBO_DST 0 Offset 0\n"
		"OpDecorate %SSBO_DST BufferBlock\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 2\n"
	);

	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"    %entry = OpLabel\n"

		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"

		" %will_run = OpFunctionCall %bool %isUniqueIdZero\n"
		"             OpSelectionMerge %end_if None\n"
		"             OpBranchConditional %will_run %run_test %end_if\n"

		" %run_test = OpLabel\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"  %val_src = OpFunctionCall %${tt} %ld_arg_ssbo_src %ndx\n"

		"  %src_idx = OpAccessChain %up_u32 %ssbo_idx %c_i32_0 %ndx\n"
		"  %val_idx = OpLoad %u32 %src_idx\n"

		"  %val_dst = OpVectorExtractDynamic %f16 %val_src %val_idx\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"

		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpBranch %end_if\n"
		"   %end_if = OpLabel\n"
		"             OpReturnValue %param\n"

		"             OpFunctionEnd\n"
	);

	for (deUint32 testTypeIdx = 0; testTypeIdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeIdx)
	{
		const TestType&		testType		= testTypes[testTypeIdx];
		const string		testName		= testType.typeName;
		const size_t		itemsPerType	= testType.typeStride / sizeof(deFloat16);
		const size_t		iterations		= float16InputData.size() / itemsPerType;
		SpecResource		specResource;
		map<string, string>	specs;
		VulkanFeatures		features;
		vector<deUint32>	inputDataNdx;
		map<string, string>	fragments;
		vector<string>		extensions;

		for (deUint32 ndx = 0; ndx < iterations; ++ndx)
			inputDataNdx.push_back(rnd.getUint32() % testType.typeComponents);

		specs["num_data_points"]	= de::toString(iterations);
		specs["tt"]					= testType.typeName;
		specs["ts"]					= testType.typeStorage;
		specs["tt_stride"]			= de::toString(testType.typeStride);
		specs["type_decl"]			= testType.typeDecls;

		fragments["capability"]		= "OpCapability Float16\n";
		fragments["decoration"]		= decoration.specialize(specs);
		fragments["pre_main"]		= preMain.specialize(specs);
		fragments["testfun"]		= testFun.specialize(specs);
		fragments["testfun"]		+= StringTemplate(testType.loadFunction).specialize({{"var", "ssbo_src"}});
		fragments["testfun"]		+= StringTemplate(testType.storeFunction).specialize({{"var", "ssbo_dst"}});

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16InputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inputDataNdx)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutputDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.verifyIO = compareFP16VectorExtractFunc;

		extensions.push_back("VK_KHR_shader_float16_int8");

		features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

		finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
	}

	return testGroup.release();
}

template<deUint32 COMPONENTS_COUNT, deUint32 REPLACEMENT>
bool compareFP16VectorInsertFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	if (inputs.size() != 2 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;
	vector<deUint8>	input2Bytes;

	inputs[0].getBytes(input1Bytes);
	inputs[1].getBytes(input2Bytes);

	DE_ASSERT(input1Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() % sizeof(deUint32) == 0);

	const size_t			iterations			= input2Bytes.size() / sizeof(deUint32);
	const size_t			componentsStride	= input1Bytes.size() / (sizeof(deFloat16) * iterations);
	const deFloat16* const	input1AsFP16		= (const deFloat16*)&input1Bytes[0];
	const deUint32* const	inputIndices		= (const deUint32*)&input2Bytes[0];
	const deFloat16* const	outputAsFP16		= (const deFloat16*)outputAllocs[0]->getHostPtr();
	const deFloat16			magic				= tcu::Float16(float(REPLACEMENT)).bits();
	std::string				error;

	DE_ASSERT(componentsStride == 2 || componentsStride == 4);
	DE_ASSERT(input1Bytes.size() == iterations * componentsStride * sizeof(deFloat16));

	for (size_t idx = 0; idx < iterations; ++idx)
	{
		const deFloat16*	inputVec		= &input1AsFP16[componentsStride * idx];
		const deFloat16*	outputVec		= &outputAsFP16[componentsStride * idx];
		const deUint32		replacedCompNdx	= inputIndices[idx];

		DE_ASSERT(replacedCompNdx < COMPONENTS_COUNT);

		for (size_t compNdx = 0; compNdx < COMPONENTS_COUNT; ++compNdx)
		{
			const deFloat16	expected	= (compNdx == replacedCompNdx) ? magic : inputVec[compNdx];

			if (!compare16BitFloat(expected, outputVec[compNdx], error))
			{
				log << TestLog::Message << "At " << idx << "[" << compNdx << "]: " << error << TestLog::EndMessage;

				return false;
			}
		}
	}

	return true;
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16VectorInsertSet (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "opvectorinsertdynamic", "OpVectorInsertDynamic tests"));

	de::Random							rnd					(deStringHash(testGroup->getName()));
	const deUint32						replacement			= 42;
	const deUint32						numDataPoints		= 256;
	const vector<deFloat16>				float16InputData	= getFloat16s(rnd, numDataPoints);
	const vector<deFloat16>				float16OutputDummy	(float16InputData.size(), 0);

	struct TestType
	{
		const deUint32	typeComponents;
		const size_t	typeStride;
		const char*		typeName;
		const char*		typeDecls;
		VerifyIOFunc	verifyIOFunc;
		const char*		typeStorage;
		const string		loadFunction;
		const string		storeFunction;
	};

	const TestType	testTypes[]	=
	{
		{
			2,
			2 * sizeof(deFloat16),
			"v2f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"%v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
			"%void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n",
			compareFP16VectorInsertFunc<2, replacement>,
			"u32",
			loadV2F16FromUint,
			storeV2F16AsUint
		},
		{
			3,
			4 * sizeof(deFloat16),
			"v3f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"      %v3f16 = OpTypeVector %f16 3\n"
			"%v3f16_i32_fn = OpTypeFunction %v3f16 %i32\n"
			"%void_v3f16_i32_fn = OpTypeFunction %void %v3f16 %i32\n",
			compareFP16VectorInsertFunc<3, replacement>,
			"ra_u32_2",
			loadV3F16FromUints,
			storeV3F16AsUints
		},
		{
			4,
			4 * sizeof(deFloat16),
			"v4f16",
			"      %v2f16 = OpTypeVector %f16 2\n"
			"      %v4f16 = OpTypeVector %f16 4\n"
			"%v4f16_i32_fn = OpTypeFunction %v4f16 %i32\n"
			"%void_v4f16_i32_fn = OpTypeFunction %void %v4f16 %i32\n",
			compareFP16VectorInsertFunc<4, replacement>,
			"ra_u32_2",
			loadV4F16FromUints,
			storeV4F16AsUints
		},
	};

	const StringTemplate preMain
	(
		"  %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		"        %f16 = OpTypeFloat 16\n"
		"  %c_f16_ins = OpConstant %f16 ${replacement}\n"

		"${type_decl}"

		"     %ra_u32 = OpTypeArray %u32 %c_i32_ndp\n"
		"	  %up_u32 = OpTypePointer Uniform %u32\n"
		"   %SSBO_IDX = OpTypeStruct %ra_u32\n"
		"%up_SSBO_IDX = OpTypePointer Uniform %SSBO_IDX\n"

		"   %ra_u32_2 = OpTypeArray %u32 %c_u32_2\n"
		"%ra_ra_u32_2 = OpTypeArray %ra_u32_2 %c_i32_ndp\n"
		"   %SSBO_SRC = OpTypeStruct %ra_${ts}\n"
		"%up_SSBO_SRC = OpTypePointer Uniform %SSBO_SRC\n"

		"   %SSBO_DST = OpTypeStruct %ra_${ts}\n"
		"%up_SSBO_DST = OpTypePointer Uniform %SSBO_DST\n"

		"   %ssbo_src = OpVariable %up_SSBO_SRC Uniform\n"
		"   %ssbo_idx = OpVariable %up_SSBO_IDX Uniform\n"
		"   %ssbo_dst = OpVariable %up_SSBO_DST Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %ra_u32_2 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_2 ArrayStride 8\n"
		"OpMemberDecorate %SSBO_SRC 0 Offset 0\n"
		"OpDecorate %SSBO_SRC BufferBlock\n"
		"OpDecorate %ssbo_src DescriptorSet 0\n"
		"OpDecorate %ssbo_src Binding 0\n"

		"OpDecorate %ra_u32 ArrayStride 4\n"
		"OpMemberDecorate %SSBO_IDX 0 Offset 0\n"
		"OpDecorate %SSBO_IDX BufferBlock\n"
		"OpDecorate %ssbo_idx DescriptorSet 0\n"
		"OpDecorate %ssbo_idx Binding 1\n"

		"OpMemberDecorate %SSBO_DST 0 Offset 0\n"
		"OpDecorate %SSBO_DST BufferBlock\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 2\n"
	);

	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"    %entry = OpLabel\n"

		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"

		" %will_run = OpFunctionCall %bool %isUniqueIdZero\n"
		"             OpSelectionMerge %end_if None\n"
		"             OpBranchConditional %will_run %run_test %end_if\n"

		" %run_test = OpLabel\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"  %val_src = OpFunctionCall %${tt} %ld_arg_ssbo_src %ndx\n"

		"  %src_idx = OpAccessChain %up_u32 %ssbo_idx %c_i32_0 %ndx\n"
		"  %val_idx = OpLoad %u32 %src_idx\n"

		"  %val_dst = OpVectorInsertDynamic %${tt} %val_src %c_f16_ins %val_idx\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"

		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpBranch %end_if\n"
		"   %end_if = OpLabel\n"
		"             OpReturnValue %param\n"

		"             OpFunctionEnd\n"
	);

	for (deUint32 testTypeIdx = 0; testTypeIdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeIdx)
	{
		const TestType&		testType		= testTypes[testTypeIdx];
		const string		testName		= testType.typeName;
		const size_t		itemsPerType	= testType.typeStride / sizeof(deFloat16);
		const size_t		iterations		= float16InputData.size() / itemsPerType;
		SpecResource		specResource;
		map<string, string>	specs;
		VulkanFeatures		features;
		vector<deUint32>	inputDataNdx;
		map<string, string>	fragments;
		vector<string>		extensions;

		for (deUint32 ndx = 0; ndx < iterations; ++ndx)
			inputDataNdx.push_back(rnd.getUint32() % testType.typeComponents);

		specs["num_data_points"]	= de::toString(iterations);
		specs["tt"]					= testType.typeName;
		specs["ts"]					= testType.typeStorage;
		specs["tt_stride"]			= de::toString(testType.typeStride);
		specs["type_decl"]			= testType.typeDecls;
		specs["replacement"]		= de::toString(replacement);

		fragments["capability"]		= "OpCapability Float16\n";
		fragments["decoration"]		= decoration.specialize(specs);
		fragments["pre_main"]		= preMain.specialize(specs);
		fragments["testfun"]		= testFun.specialize(specs);
		fragments["testfun"]		+= StringTemplate(testType.loadFunction).specialize({{"var", "ssbo_src"}});
		fragments["testfun"]		+= StringTemplate(testType.storeFunction).specialize({{"var", "ssbo_dst"}});

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16InputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inputDataNdx)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutputDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.verifyIO = testType.verifyIOFunc;

		extensions.push_back("VK_KHR_shader_float16_int8");

		features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

		finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
	}

	return testGroup.release();
}

inline deFloat16 getShuffledComponent (const size_t iteration, const size_t componentNdx, const deFloat16* input1Vec, const deFloat16* input2Vec, size_t vec1Len, size_t vec2Len, bool& validate)
{
	const size_t	compNdxCount	= (vec1Len + vec2Len + 1);
	const size_t	compNdxLimited	= iteration % (compNdxCount * compNdxCount);
	size_t			comp;

	switch (componentNdx)
	{
		case 0: comp = compNdxLimited / compNdxCount; break;
		case 1: comp = compNdxLimited % compNdxCount; break;
		case 2: comp = 0; break;
		case 3: comp = 1; break;
		default: TCU_THROW(InternalError, "Impossible");
	}

	if (comp >= vec1Len + vec2Len)
	{
		validate = false;
		return 0;
	}
	else
	{
		validate = true;
		return (comp < vec1Len) ? input1Vec[comp] : input2Vec[comp - vec1Len];
	}
}

template<deUint32 DST_COMPONENTS_COUNT, deUint32 SRC0_COMPONENTS_COUNT, deUint32 SRC1_COMPONENTS_COUNT>
bool compareFP16VectorShuffleFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	DE_STATIC_ASSERT(DST_COMPONENTS_COUNT == 2 || DST_COMPONENTS_COUNT == 3 || DST_COMPONENTS_COUNT == 4);
	DE_STATIC_ASSERT(SRC0_COMPONENTS_COUNT == 2 || SRC0_COMPONENTS_COUNT == 3 || SRC0_COMPONENTS_COUNT == 4);
	DE_STATIC_ASSERT(SRC1_COMPONENTS_COUNT == 2 || SRC1_COMPONENTS_COUNT == 3 || SRC1_COMPONENTS_COUNT == 4);

	if (inputs.size() != 2 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;
	vector<deUint8>	input2Bytes;

	inputs[0].getBytes(input1Bytes);
	inputs[1].getBytes(input2Bytes);

	DE_ASSERT(input1Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() > 0);
	DE_ASSERT(input2Bytes.size() % sizeof(deFloat16) == 0);

	const size_t			componentsStrideDst		= (DST_COMPONENTS_COUNT == 3) ? 4 : DST_COMPONENTS_COUNT;
	const size_t			componentsStrideSrc0	= (SRC0_COMPONENTS_COUNT == 3) ? 4 : SRC0_COMPONENTS_COUNT;
	const size_t			componentsStrideSrc1	= (SRC1_COMPONENTS_COUNT == 3) ? 4 : SRC1_COMPONENTS_COUNT;
	const size_t			iterations				= input1Bytes.size() / (componentsStrideSrc0 * sizeof(deFloat16));
	const deFloat16* const	input1AsFP16			= (const deFloat16*)&input1Bytes[0];
	const deFloat16* const	input2AsFP16			= (const deFloat16*)&input2Bytes[0];
	const deFloat16* const	outputAsFP16			= (const deFloat16*)outputAllocs[0]->getHostPtr();
	std::string				error;

	DE_ASSERT(input1Bytes.size() == iterations * componentsStrideSrc0 * sizeof(deFloat16));
	DE_ASSERT(input2Bytes.size() == iterations * componentsStrideSrc1 * sizeof(deFloat16));

	for (size_t idx = 0; idx < iterations; ++idx)
	{
		const deFloat16*	input1Vec	= &input1AsFP16[componentsStrideSrc0 * idx];
		const deFloat16*	input2Vec	= &input2AsFP16[componentsStrideSrc1 * idx];
		const deFloat16*	outputVec	= &outputAsFP16[componentsStrideDst * idx];

		for (size_t compNdx = 0; compNdx < DST_COMPONENTS_COUNT; ++compNdx)
		{
			bool		validate	= true;
			deFloat16	expected	= getShuffledComponent(idx, compNdx, input1Vec, input2Vec, SRC0_COMPONENTS_COUNT, SRC1_COMPONENTS_COUNT, validate);

			if (validate && !compare16BitFloat(expected, outputVec[compNdx], error))
			{
				log << TestLog::Message << "At " << idx << "[" << compNdx << "]: " << error << TestLog::EndMessage;

				return false;
			}
		}
	}

	return true;
}

VerifyIOFunc getFloat16VectorShuffleVerifyIOFunc (deUint32 dstComponentsCount, deUint32 src0ComponentsCount, deUint32 src1ComponentsCount)
{
	DE_ASSERT(dstComponentsCount <= 4);
	DE_ASSERT(src0ComponentsCount <= 4);
	DE_ASSERT(src1ComponentsCount <= 4);
	deUint32 funcCode = 100 * dstComponentsCount + 10 * src0ComponentsCount + src1ComponentsCount;

	switch (funcCode)
	{
		case 222:return compareFP16VectorShuffleFunc<2, 2, 2>;
		case 223:return compareFP16VectorShuffleFunc<2, 2, 3>;
		case 224:return compareFP16VectorShuffleFunc<2, 2, 4>;
		case 232:return compareFP16VectorShuffleFunc<2, 3, 2>;
		case 233:return compareFP16VectorShuffleFunc<2, 3, 3>;
		case 234:return compareFP16VectorShuffleFunc<2, 3, 4>;
		case 242:return compareFP16VectorShuffleFunc<2, 4, 2>;
		case 243:return compareFP16VectorShuffleFunc<2, 4, 3>;
		case 244:return compareFP16VectorShuffleFunc<2, 4, 4>;
		case 322:return compareFP16VectorShuffleFunc<3, 2, 2>;
		case 323:return compareFP16VectorShuffleFunc<3, 2, 3>;
		case 324:return compareFP16VectorShuffleFunc<3, 2, 4>;
		case 332:return compareFP16VectorShuffleFunc<3, 3, 2>;
		case 333:return compareFP16VectorShuffleFunc<3, 3, 3>;
		case 334:return compareFP16VectorShuffleFunc<3, 3, 4>;
		case 342:return compareFP16VectorShuffleFunc<3, 4, 2>;
		case 343:return compareFP16VectorShuffleFunc<3, 4, 3>;
		case 344:return compareFP16VectorShuffleFunc<3, 4, 4>;
		case 422:return compareFP16VectorShuffleFunc<4, 2, 2>;
		case 423:return compareFP16VectorShuffleFunc<4, 2, 3>;
		case 424:return compareFP16VectorShuffleFunc<4, 2, 4>;
		case 432:return compareFP16VectorShuffleFunc<4, 3, 2>;
		case 433:return compareFP16VectorShuffleFunc<4, 3, 3>;
		case 434:return compareFP16VectorShuffleFunc<4, 3, 4>;
		case 442:return compareFP16VectorShuffleFunc<4, 4, 2>;
		case 443:return compareFP16VectorShuffleFunc<4, 4, 3>;
		case 444:return compareFP16VectorShuffleFunc<4, 4, 4>;
		default: TCU_THROW(InternalError, "Invalid number of components specified.");
	}
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16VectorShuffleSet (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "opvectorshuffle", "OpVectorShuffle tests"));
	const int							testSpecificSeed	= deStringHash(testGroup->getName());
	const int							seed				= testCtx.getCommandLine().getBaseSeed() ^ testSpecificSeed;
	de::Random							rnd					(seed);
	const deUint32						numDataPoints		= 128;
	map<string, string>					fragments;

	struct TestType
	{
		const deUint32	typeComponents;
		const char*		typeName;
		const string	loadFunction;
		const string	storeFunction;
	};

	const TestType	testTypes[]	=
	{
		{
			2,
			"v2f16",
			loadV2F16FromUint,
			storeV2F16AsUint
		},
		{
			3,
			"v3f16",
			loadV3F16FromUints,
			storeV3F16AsUints
		},
		{
			4,
			"v4f16",
			loadV4F16FromUints,
			storeV4F16AsUints
		},
	};

	const StringTemplate preMain
	(
		"    %c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		"     %c_i32_cc = OpConstant %i32 ${case_count}\n"
		"          %f16 = OpTypeFloat 16\n"
		"        %v2f16 = OpTypeVector %f16 2\n"
		"        %v3f16 = OpTypeVector %f16 3\n"
		"        %v4f16 = OpTypeVector %f16 4\n"

		"     %v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
		"     %v3f16_i32_fn = OpTypeFunction %v3f16 %i32\n"
		"     %v4f16_i32_fn = OpTypeFunction %v4f16 %i32\n"
		"%void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n"
		"%void_v3f16_i32_fn = OpTypeFunction %void %v3f16 %i32\n"
		"%void_v4f16_i32_fn = OpTypeFunction %void %v4f16 %i32\n"

		"     %ra_u32_2 = OpTypeArray %u32 %c_u32_2\n"
		"   %ra_u32_ndp = OpTypeArray %u32 %c_i32_ndp\n"
		"  %ra_ra_u32_2 = OpTypeArray %ra_u32_2 %c_i32_ndp\n"
		"       %up_u32 = OpTypePointer Uniform %u32\n"
		"   %SSBO_v2f16 = OpTypeStruct %ra_u32_ndp\n"
		"   %SSBO_v3f16 = OpTypeStruct %ra_ra_u32_2\n"
		"   %SSBO_v4f16 = OpTypeStruct %ra_ra_u32_2\n"

		"%up_SSBO_v2f16 = OpTypePointer Uniform %SSBO_v2f16\n"
		"%up_SSBO_v3f16 = OpTypePointer Uniform %SSBO_v3f16\n"
		"%up_SSBO_v4f16 = OpTypePointer Uniform %SSBO_v4f16\n"

		"        %fun_t = OpTypeFunction %${tt_dst} %${tt_src0} %${tt_src1} %i32\n"

		"    %ssbo_src0 = OpVariable %up_SSBO_${tt_src0} Uniform\n"
		"    %ssbo_src1 = OpVariable %up_SSBO_${tt_src1} Uniform\n"
		"     %ssbo_dst = OpVariable %up_SSBO_${tt_dst} Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %ra_u32_2 ArrayStride 4\n"
		"OpDecorate %ra_u32_ndp ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_2 ArrayStride 8\n"

		"OpMemberDecorate %SSBO_v2f16 0 Offset 0\n"
		"OpDecorate %SSBO_v2f16 BufferBlock\n"

		"OpMemberDecorate %SSBO_v3f16 0 Offset 0\n"
		"OpDecorate %SSBO_v3f16 BufferBlock\n"

		"OpMemberDecorate %SSBO_v4f16 0 Offset 0\n"
		"OpDecorate %SSBO_v4f16 BufferBlock\n"

		"OpDecorate %ssbo_src0 DescriptorSet 0\n"
		"OpDecorate %ssbo_src0 Binding 0\n"
		"OpDecorate %ssbo_src1 DescriptorSet 0\n"
		"OpDecorate %ssbo_src1 Binding 1\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 2\n"
	);

	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"    %entry = OpLabel\n"

		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"

		" %will_run = OpFunctionCall %bool %isUniqueIdZero\n"
		"             OpSelectionMerge %end_if None\n"
		"             OpBranchConditional %will_run %run_test %end_if\n"

		" %run_test = OpLabel\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"
		" %val_src0 = OpFunctionCall %${tt_src0} %ld_arg_ssbo_src0 %ndx\n"
		" %val_src1 = OpFunctionCall %${tt_src1} %ld_arg_ssbo_src1 %ndx\n"
		"  %val_dst = OpFunctionCall %${tt_dst} %sw_fun %val_src0 %val_src1 %ndx\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n"
		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpBranch %end_if\n"
		"   %end_if = OpLabel\n"
		"             OpReturnValue %param\n"
		"             OpFunctionEnd\n"
		"\n"

		"   %sw_fun = OpFunction %${tt_dst} None %fun_t\n"
		"%sw_param0 = OpFunctionParameter %${tt_src0}\n"
		"%sw_param1 = OpFunctionParameter %${tt_src1}\n"
		"%sw_paramn = OpFunctionParameter %i32\n"
		" %sw_entry = OpLabel\n"
		"   %modulo = OpSMod %i32 %sw_paramn %c_i32_cc\n"
		"             OpSelectionMerge %switch_e None\n"
		"             OpSwitch %modulo %default ${case_list}\n"
		"${case_bodies}"
		"%default   = OpLabel\n"
		"             OpUnreachable\n" // Unreachable default case for switch statement
		"%switch_e  = OpLabel\n"
		"             OpUnreachable\n" // Unreachable merge block for switch statement
		"             OpFunctionEnd\n"
	);

	const StringTemplate testCaseBody
	(
		"%case_${case_ndx}    = OpLabel\n"
		"%val_dst_${case_ndx} = OpVectorShuffle %${tt_dst} %sw_param0 %sw_param1 ${shuffle}\n"
		"             OpReturnValue %val_dst_${case_ndx}\n"
	);

	for (deUint32 dstTypeIdx = 0; dstTypeIdx < DE_LENGTH_OF_ARRAY(testTypes); ++dstTypeIdx)
	{
		const TestType&	dstType			= testTypes[dstTypeIdx];

		for (deUint32 comp0Idx = 0; comp0Idx < DE_LENGTH_OF_ARRAY(testTypes); ++comp0Idx)
		{
			const TestType&	src0Type	= testTypes[comp0Idx];

			for (deUint32 comp1Idx = 0; comp1Idx < DE_LENGTH_OF_ARRAY(testTypes); ++comp1Idx)
			{
				const TestType&			src1Type			= testTypes[comp1Idx];
				const deUint32			input0Stride		= (src0Type.typeComponents == 3) ? 4 : src0Type.typeComponents;
				const deUint32			input1Stride		= (src1Type.typeComponents == 3) ? 4 : src1Type.typeComponents;
				const deUint32			outputStride		= (dstType.typeComponents == 3) ? 4 : dstType.typeComponents;
				const vector<deFloat16>	float16Input0Data	= getFloat16s(rnd, input0Stride * numDataPoints);
				const vector<deFloat16>	float16Input1Data	= getFloat16s(rnd, input1Stride * numDataPoints);
				const vector<deFloat16>	float16OutputDummy	(outputStride * numDataPoints, 0);
				const string			testName			= de::toString(dstType.typeComponents) + de::toString(src0Type.typeComponents) + de::toString(src1Type.typeComponents);
				deUint32				caseCount			= 0;
				SpecResource			specResource;
				map<string, string>		specs;
				vector<string>			extensions;
				VulkanFeatures			features;
				string					caseBodies;
				string					caseList;

				// Generate case
				{
					vector<string>	componentList;

					// Generate component possible indices for OpVectorShuffle for components 0 and 1 in output vector
					{
						deUint32		caseNo		= 0;

						for (deUint32 comp0IdxLocal = 0; comp0IdxLocal < src0Type.typeComponents; ++comp0IdxLocal)
							componentList.push_back(de::toString(caseNo++));
						for (deUint32 comp1IdxLocal = 0; comp1IdxLocal < src1Type.typeComponents; ++comp1IdxLocal)
							componentList.push_back(de::toString(caseNo++));
						componentList.push_back("0xFFFFFFFF");
					}

					for (deUint32 comp0IdxLocal = 0; comp0IdxLocal < componentList.size(); ++comp0IdxLocal)
					{
						for (deUint32 comp1IdxLocal = 0; comp1IdxLocal < componentList.size(); ++comp1IdxLocal)
						{
							map<string, string>	specCase;
							string				shuffle		= componentList[comp0IdxLocal] + " " + componentList[comp1IdxLocal];

							for (deUint32 compIdx = 2; compIdx < dstType.typeComponents; ++compIdx)
								shuffle += " " + de::toString(compIdx - 2);

							specCase["case_ndx"]	= de::toString(caseCount);
							specCase["shuffle"]		= shuffle;
							specCase["tt_dst"]		= dstType.typeName;

							caseBodies	+= testCaseBody.specialize(specCase);
							caseList	+= de::toString(caseCount) + " %case_" + de::toString(caseCount) + " ";

							caseCount++;
						}
					}
				}

				specs["num_data_points"]	= de::toString(numDataPoints);
				specs["tt_dst"]				= dstType.typeName;
				specs["tt_src0"]			= src0Type.typeName;
				specs["tt_src1"]			= src1Type.typeName;
				specs["case_bodies"]		= caseBodies;
				specs["case_list"]			= caseList;
				specs["case_count"]			= de::toString(caseCount);

				fragments["capability"]		= "OpCapability Float16\n";
				fragments["decoration"]		= decoration.specialize(specs);
				fragments["pre_main"]		= preMain.specialize(specs);
				fragments["testfun"]		= testFun.specialize(specs);
				fragments["testfun"]		+= StringTemplate(src0Type.loadFunction).specialize({{"var", "ssbo_src0"}});
				fragments["testfun"]		+= StringTemplate(src1Type.loadFunction).specialize({{"var", "ssbo_src1"}});
				fragments["testfun"]		+= StringTemplate(dstType.storeFunction).specialize({{"var", "ssbo_dst"}});

				specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16Input0Data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
				specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(float16Input1Data)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
				specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16OutputDummy)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
				specResource.verifyIO = getFloat16VectorShuffleVerifyIOFunc(dstType.typeComponents, src0Type.typeComponents, src1Type.typeComponents);

				extensions.push_back("VK_KHR_shader_float16_int8");

				features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

				finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
			}
		}
	}

	return testGroup.release();
}

bool compareFP16CompositeFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>&, TestLog& log)
{
	if (inputs.size() != 1 || outputAllocs.size() != 1)
		return false;

	vector<deUint8>	input1Bytes;

	inputs[0].getBytes(input1Bytes);

	DE_ASSERT(input1Bytes.size() > 0);
	DE_ASSERT(input1Bytes.size() % sizeof(deFloat16) == 0);

	const size_t			iterations		= input1Bytes.size() / sizeof(deFloat16);
	const deFloat16* const	input1AsFP16	= (const deFloat16*)&input1Bytes[0];
	const deFloat16* const	outputAsFP16	= (const deFloat16*)outputAllocs[0]->getHostPtr();
	const deFloat16			exceptionValue	= tcu::Float16(-1.0).bits();
	std::string				error;

	for (size_t idx = 0; idx < iterations; ++idx)
	{
		if (input1AsFP16[idx] == exceptionValue)
			continue;

		if (!compare16BitFloat(input1AsFP16[idx], outputAsFP16[idx], error))
		{
			log << TestLog::Message << "At " << idx << ":" << error << TestLog::EndMessage;

			return false;
		}
	}

	return true;
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16CompositeConstructSet (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup				(new tcu::TestCaseGroup(testCtx, "opcompositeconstruct", "OpCompositeConstruct tests"));
	const deUint32						numElements				= 8;
	const string						testName				= "struct";
	const deUint32						structItemsCount		= 88;
	const deUint32						exceptionIndices[]		= { 1, 7, 15, 17, 25, 33, 51, 55, 59, 63, 67, 71, 84, 85, 86, 87 };
	const deFloat16						exceptionValue			= tcu::Float16(-1.0).bits();
	const deUint32						fieldModifier			= 2;
	const deUint32						fieldModifiedMulIndex	= 60;
	const deUint32						fieldModifiedAddIndex	= 66;

	const StringTemplate preMain
	(
		"    %c_i32_ndp = OpConstant %i32 ${num_elements}\n"
		"          %f16 = OpTypeFloat 16\n"
		"        %v2f16 = OpTypeVector %f16 2\n"
		"        %v3f16 = OpTypeVector %f16 3\n"
		"        %v4f16 = OpTypeVector %f16 4\n"
		"    %c_f16_mod = OpConstant %f16 ${field_modifier}\n"

		"${consts}"

		"     %c_f16_n1 = OpConstant %f16 -1.0\n"
		"   %c_v2f16_n1 = OpConstantComposite %v2f16 %c_f16_n1 %c_f16_n1\n"
		"      %c_u32_5 = OpConstant %u32 5\n"
		"      %c_u32_6 = OpConstant %u32 6\n"
		"      %c_u32_7 = OpConstant %u32 7\n"
		"      %c_u32_8 = OpConstant %u32 8\n"
		"      %c_u32_9 = OpConstant %u32 9\n"
		"     %c_u32_10 = OpConstant %u32 10\n"
		"     %c_u32_11 = OpConstant %u32 11\n"
		"     %c_u32_12 = OpConstant %u32 12\n"
		"     %c_u32_13 = OpConstant %u32 13\n"
		"     %c_u32_14 = OpConstant %u32 14\n"
		"     %c_u32_15 = OpConstant %u32 15\n"
		"     %c_u32_16 = OpConstant %u32 16\n"
		"     %c_u32_17 = OpConstant %u32 17\n"
		"     %c_u32_18 = OpConstant %u32 18\n"
		"     %c_u32_19 = OpConstant %u32 19\n"
		"     %c_u32_20 = OpConstant %u32 20\n"
		"     %c_u32_21 = OpConstant %u32 21\n"
		"     %c_u32_22 = OpConstant %u32 22\n"
		"     %c_u32_23 = OpConstant %u32 23\n"
		"     %c_u32_24 = OpConstant %u32 24\n"
		"     %c_u32_25 = OpConstant %u32 25\n"
		"     %c_u32_26 = OpConstant %u32 26\n"
		"     %c_u32_27 = OpConstant %u32 27\n"
		"     %c_u32_28 = OpConstant %u32 28\n"
		"     %c_u32_29 = OpConstant %u32 29\n"
		"     %c_u32_30 = OpConstant %u32 30\n"
		"     %c_u32_31 = OpConstant %u32 31\n"
		"     %c_u32_33 = OpConstant %u32 33\n"
		"     %c_u32_34 = OpConstant %u32 34\n"
		"     %c_u32_35 = OpConstant %u32 35\n"
		"     %c_u32_36 = OpConstant %u32 36\n"
		"     %c_u32_37 = OpConstant %u32 37\n"
		"     %c_u32_38 = OpConstant %u32 38\n"
		"     %c_u32_39 = OpConstant %u32 39\n"
		"     %c_u32_40 = OpConstant %u32 40\n"
		"     %c_u32_41 = OpConstant %u32 41\n"
		"     %c_u32_44 = OpConstant %u32 44\n"

		" %f16arr3      = OpTypeArray %f16 %c_u32_3\n"
		" %v2f16arr3    = OpTypeArray %v2f16 %c_u32_3\n"
		" %v2f16arr5    = OpTypeArray %v2f16 %c_u32_5\n"
		" %v3f16arr5    = OpTypeArray %v3f16 %c_u32_5\n"
		" %v4f16arr3    = OpTypeArray %v4f16 %c_u32_3\n"
		" %struct16     = OpTypeStruct %f16 %v2f16arr3\n"
		" %struct16arr3 = OpTypeArray %struct16 %c_u32_3\n"
		" %st_test      = OpTypeStruct %f16 %v2f16 %v3f16 %v4f16 %f16arr3 %struct16arr3 %v2f16arr5 %f16 %v3f16arr5 %v4f16arr3\n"

		"       %up_u32 = OpTypePointer Uniform %u32\n"
		"    %ra_u32_44 = OpTypeArray %u32 %c_u32_44\n"
		"    %ra_ra_u32 = OpTypeArray %ra_u32_44 %c_i32_ndp\n"
		"      %SSBO_st = OpTypeStruct %ra_ra_u32\n"
		"   %up_SSBO_st = OpTypePointer Uniform %SSBO_st\n"

		"     %ssbo_dst = OpVariable %up_SSBO_st Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %SSBO_st BufferBlock\n"
		"OpDecorate %ra_u32_44 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32 ArrayStride ${struct_item_size}\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 1\n"

		"OpMemberDecorate %SSBO_st 0 Offset 0\n"

		"OpDecorate %v2f16arr3 ArrayStride 4\n"
		"OpMemberDecorate %struct16 0 Offset 0\n"
		"OpMemberDecorate %struct16 1 Offset 4\n"
		"OpDecorate %struct16arr3 ArrayStride 16\n"
		"OpDecorate %f16arr3 ArrayStride 2\n"
		"OpDecorate %v2f16arr5 ArrayStride 4\n"
		"OpDecorate %v3f16arr5 ArrayStride 8\n"
		"OpDecorate %v4f16arr3 ArrayStride 8\n"

		"OpMemberDecorate %st_test 0 Offset 0\n"
		"OpMemberDecorate %st_test 1 Offset 4\n"
		"OpMemberDecorate %st_test 2 Offset 8\n"
		"OpMemberDecorate %st_test 3 Offset 16\n"
		"OpMemberDecorate %st_test 4 Offset 24\n"
		"OpMemberDecorate %st_test 5 Offset 32\n"
		"OpMemberDecorate %st_test 6 Offset 80\n"
		"OpMemberDecorate %st_test 7 Offset 100\n"
		"OpMemberDecorate %st_test 8 Offset 104\n"
		"OpMemberDecorate %st_test 9 Offset 144\n"
	);

	const StringTemplate testFun
	(
		" %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"     %param = OpFunctionParameter %v4f32\n"
		"     %entry = OpLabel\n"

		"         %i = OpVariable %fp_i32 Function\n"
		"              OpStore %i %c_i32_0\n"

		"  %will_run = OpFunctionCall %bool %isUniqueIdZero\n"
		"              OpSelectionMerge %end_if None\n"
		"              OpBranchConditional %will_run %run_test %end_if\n"

		"  %run_test = OpLabel\n"
		"              OpBranch %loop\n"

		"      %loop = OpLabel\n"
		"     %i_cmp = OpLoad %i32 %i\n"
		"        %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"              OpLoopMerge %merge %next None\n"
		"              OpBranchConditional %lt %write %merge\n"

		"     %write = OpLabel\n"
		"       %ndx = OpLoad %i32 %i\n"

		"      %fld1 = OpCompositeConstruct %v2f16 %c_f16_2 %c_f16_3\n"
		"      %fld2 = OpCompositeConstruct %v3f16 %c_f16_4 %c_f16_5 %c_f16_6\n"
		"      %fld3 = OpCompositeConstruct %v4f16 %c_f16_8 %c_f16_9 %c_f16_10 %c_f16_11\n"

		"      %fld4 = OpCompositeConstruct %f16arr3 %c_f16_12 %c_f16_13 %c_f16_14\n"

		"%fld5_0_1_0 = OpCompositeConstruct %v2f16 %c_f16_18 %c_f16_19\n"
		"%fld5_0_1_1 = OpCompositeConstruct %v2f16 %c_f16_20 %c_f16_21\n"
		"%fld5_0_1_2 = OpCompositeConstruct %v2f16 %c_f16_22 %c_f16_23\n"
		"  %fld5_0_1 = OpCompositeConstruct %v2f16arr3 %fld5_0_1_0 %fld5_0_1_1 %fld5_0_1_2\n"
		"    %fld5_0 = OpCompositeConstruct %struct16 %c_f16_16 %fld5_0_1\n"

		"%fld5_1_1_0 = OpCompositeConstruct %v2f16 %c_f16_26 %c_f16_27\n"
		"%fld5_1_1_1 = OpCompositeConstruct %v2f16 %c_f16_28 %c_f16_29\n"
		"%fld5_1_1_2 = OpCompositeConstruct %v2f16 %c_f16_30 %c_f16_31\n"
		"  %fld5_1_1 = OpCompositeConstruct %v2f16arr3 %fld5_1_1_0 %fld5_1_1_1 %fld5_1_1_2\n"
		"    %fld5_1 = OpCompositeConstruct %struct16 %c_f16_24 %fld5_1_1\n"

		"%fld5_2_1_0 = OpCompositeConstruct %v2f16 %c_f16_34 %c_f16_35\n"
		"%fld5_2_1_1 = OpCompositeConstruct %v2f16 %c_f16_36 %c_f16_37\n"
		"%fld5_2_1_2 = OpCompositeConstruct %v2f16 %c_f16_38 %c_f16_39\n"
		"  %fld5_2_1 = OpCompositeConstruct %v2f16arr3 %fld5_2_1_0 %fld5_2_1_1 %fld5_2_1_2\n"
		"    %fld5_2 = OpCompositeConstruct %struct16 %c_f16_32 %fld5_2_1\n"

		"      %fld5 = OpCompositeConstruct %struct16arr3 %fld5_0 %fld5_1 %fld5_2\n"

		"    %fld6_0 = OpCompositeConstruct %v2f16 %c_f16_40 %c_f16_41\n"
		"    %fld6_1 = OpCompositeConstruct %v2f16 %c_f16_42 %c_f16_43\n"
		"    %fld6_2 = OpCompositeConstruct %v2f16 %c_f16_44 %c_f16_45\n"
		"    %fld6_3 = OpCompositeConstruct %v2f16 %c_f16_46 %c_f16_47\n"
		"    %fld6_4 = OpCompositeConstruct %v2f16 %c_f16_48 %c_f16_49\n"
		"      %fld6 = OpCompositeConstruct %v2f16arr5 %fld6_0 %fld6_1 %fld6_2 %fld6_3 %fld6_4\n"

		"      %fndx = OpConvertSToF %f16 %ndx\n"
		"  %fld8_2a0 = OpFMul %f16 %fndx %c_f16_mod\n"
		"  %fld8_3b1 = OpFAdd %f16 %fndx %c_f16_mod\n"

		"   %fld8_2a = OpCompositeConstruct %v2f16 %fld8_2a0 %c_f16_61\n"
		"   %fld8_3b = OpCompositeConstruct %v2f16 %c_f16_65 %fld8_3b1\n"
		"    %fld8_0 = OpCompositeConstruct %v3f16 %c_f16_52 %c_f16_53 %c_f16_54\n"
		"    %fld8_1 = OpCompositeConstruct %v3f16 %c_f16_56 %c_f16_57 %c_f16_58\n"
		"    %fld8_2 = OpCompositeConstruct %v3f16 %fld8_2a %c_f16_62\n"
		"    %fld8_3 = OpCompositeConstruct %v3f16 %c_f16_64 %fld8_3b\n"
		"    %fld8_4 = OpCompositeConstruct %v3f16 %c_f16_68 %c_f16_69 %c_f16_70\n"
		"      %fld8 = OpCompositeConstruct %v3f16arr5 %fld8_0 %fld8_1 %fld8_2 %fld8_3 %fld8_4\n"

		"    %fld9_0 = OpCompositeConstruct %v4f16 %c_f16_72 %c_f16_73 %c_f16_74 %c_f16_75\n"
		"    %fld9_1 = OpCompositeConstruct %v4f16 %c_f16_76 %c_f16_77 %c_f16_78 %c_f16_79\n"
		"    %fld9_2 = OpCompositeConstruct %v4f16 %c_f16_80 %c_f16_81 %c_f16_82 %c_f16_83\n"
		"      %fld9 = OpCompositeConstruct %v4f16arr3 %fld9_0 %fld9_1 %fld9_2\n"

		"    %st_val = OpCompositeConstruct %st_test %c_f16_0 %fld1 %fld2 %fld3 %fld4 %fld5 %fld6 %c_f16_50 %fld8 %fld9\n"

		// Storage section: all elements that are not directly accessed should
		// have the value of -1.0. This means for f16 and v3f16 stores the v2f16
		// is constructed with one element from a constant -1.0.
		// half offset 0
		"      %ex_0 = OpCompositeExtract %f16 %st_val 0\n"
		"     %vec_0 = OpCompositeConstruct %v2f16 %ex_0 %c_f16_n1\n"
		"      %bc_0 = OpBitcast %u32 %vec_0\n"
		"     %gep_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_0\n"
		"              OpStore %gep_0 %bc_0\n"

		// <2 x half> offset 4
		"      %ex_1 = OpCompositeExtract %v2f16 %st_val 1\n"
		"      %bc_1 = OpBitcast %u32 %ex_1\n"
		"     %gep_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_1\n"
		"              OpStore %gep_1 %bc_1\n"

		// <3 x half> offset 8
		"      %ex_2 = OpCompositeExtract %v3f16 %st_val 2\n"
		"    %ex_2_0 = OpVectorShuffle %v2f16 %ex_2 %c_v2f16_n1 0 1\n"
		"    %ex_2_1 = OpVectorShuffle %v2f16 %ex_2 %c_v2f16_n1 2 3\n"
		"    %bc_2_0 = OpBitcast %u32 %ex_2_0\n"
		"    %bc_2_1 = OpBitcast %u32 %ex_2_1\n"
		"   %gep_2_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_2\n"
		"   %gep_2_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_3\n"
		"              OpStore %gep_2_0 %bc_2_0\n"
		"              OpStore %gep_2_1 %bc_2_1\n"

		// <4 x half> offset 16
		"      %ex_3 = OpCompositeExtract %v4f16 %st_val 3\n"
		"    %ex_3_0 = OpVectorShuffle %v2f16 %ex_3 %ex_3 0 1\n"
		"    %ex_3_1 = OpVectorShuffle %v2f16 %ex_3 %ex_3 2 3\n"
		"    %bc_3_0 = OpBitcast %u32 %ex_3_0\n"
		"    %bc_3_1 = OpBitcast %u32 %ex_3_1\n"
		"   %gep_3_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_4\n"
		"   %gep_3_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_5\n"
		"              OpStore %gep_3_0 %bc_3_0\n"
		"              OpStore %gep_3_1 %bc_3_1\n"

		// [3 x half] offset 24
		"    %ex_4_0 = OpCompositeExtract %f16 %st_val 4 0\n"
		"    %ex_4_1 = OpCompositeExtract %f16 %st_val 4 1\n"
		"    %ex_4_2 = OpCompositeExtract %f16 %st_val 4 2\n"
		"   %vec_4_0 = OpCompositeConstruct %v2f16 %ex_4_0 %ex_4_1\n"
		"   %vec_4_1 = OpCompositeConstruct %v2f16 %ex_4_2 %c_f16_n1\n"
		"    %bc_4_0 = OpBitcast %u32 %vec_4_0\n"
		"    %bc_4_1 = OpBitcast %u32 %vec_4_1\n"
		"   %gep_4_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_6\n"
		"   %gep_4_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_7\n"
		"              OpStore %gep_4_0 %bc_4_0\n"
		"              OpStore %gep_4_1 %bc_4_1\n"

		// [3 x {half, [3 x <2 x half>]}] offset 32
		"    %ex_5_0 = OpCompositeExtract %struct16 %st_val 5 0\n"
		"    %ex_5_1 = OpCompositeExtract %struct16 %st_val 5 1\n"
		"    %ex_5_2 = OpCompositeExtract %struct16 %st_val 5 2\n"
		"  %ex_5_0_0 = OpCompositeExtract %f16 %ex_5_0 0\n"
		"  %ex_5_1_0 = OpCompositeExtract %f16 %ex_5_1 0\n"
		"  %ex_5_2_0 = OpCompositeExtract %f16 %ex_5_2 0\n"
		"%ex_5_0_1_0 = OpCompositeExtract %v2f16 %ex_5_0 1 0\n"
		"%ex_5_0_1_1 = OpCompositeExtract %v2f16 %ex_5_0 1 1\n"
		"%ex_5_0_1_2 = OpCompositeExtract %v2f16 %ex_5_0 1 2\n"
		"%ex_5_1_1_0 = OpCompositeExtract %v2f16 %ex_5_1 1 0\n"
		"%ex_5_1_1_1 = OpCompositeExtract %v2f16 %ex_5_1 1 1\n"
		"%ex_5_1_1_2 = OpCompositeExtract %v2f16 %ex_5_1 1 2\n"
		"%ex_5_2_1_0 = OpCompositeExtract %v2f16 %ex_5_2 1 0\n"
		"%ex_5_2_1_1 = OpCompositeExtract %v2f16 %ex_5_2 1 1\n"
		"%ex_5_2_1_2 = OpCompositeExtract %v2f16 %ex_5_2 1 2\n"
		" %vec_5_0_0 = OpCompositeConstruct %v2f16 %ex_5_0_0 %c_f16_n1\n"
		" %vec_5_1_0 = OpCompositeConstruct %v2f16 %ex_5_1_0 %c_f16_n1\n"
		" %vec_5_2_0 = OpCompositeConstruct %v2f16 %ex_5_2_0 %c_f16_n1\n"
		"  %bc_5_0_0 = OpBitcast %u32 %vec_5_0_0\n"
		"  %bc_5_1_0 = OpBitcast %u32 %vec_5_1_0\n"
		"  %bc_5_2_0 = OpBitcast %u32 %vec_5_2_0\n"
		"%bc_5_0_1_0 = OpBitcast %u32 %ex_5_0_1_0\n"
		"%bc_5_0_1_1 = OpBitcast %u32 %ex_5_0_1_1\n"
		"%bc_5_0_1_2 = OpBitcast %u32 %ex_5_0_1_2\n"
		"%bc_5_1_1_0 = OpBitcast %u32 %ex_5_1_1_0\n"
		"%bc_5_1_1_1 = OpBitcast %u32 %ex_5_1_1_1\n"
		"%bc_5_1_1_2 = OpBitcast %u32 %ex_5_1_1_2\n"
		"%bc_5_2_1_0 = OpBitcast %u32 %ex_5_2_1_0\n"
		"%bc_5_2_1_1 = OpBitcast %u32 %ex_5_2_1_1\n"
		"%bc_5_2_1_2 = OpBitcast %u32 %ex_5_2_1_2\n"
		"  %gep_5_0_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_8\n"
		"%gep_5_0_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_9\n"
		"%gep_5_0_1_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_10\n"
		"%gep_5_0_1_2 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_11\n"
		"  %gep_5_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_12\n"
		"%gep_5_1_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_13\n"
		"%gep_5_1_1_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_14\n"
		"%gep_5_1_1_2 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_15\n"
		"  %gep_5_2_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_16\n"
		"%gep_5_2_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_17\n"
		"%gep_5_2_1_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_18\n"
		"%gep_5_2_1_2 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_19\n"
		"              OpStore %gep_5_0_0 %bc_5_0_0\n"
		"              OpStore %gep_5_0_1_0 %bc_5_0_1_0\n"
		"              OpStore %gep_5_0_1_1 %bc_5_0_1_1\n"
		"              OpStore %gep_5_0_1_2 %bc_5_0_1_2\n"
		"              OpStore %gep_5_1_0 %bc_5_1_0\n"
		"              OpStore %gep_5_1_1_0 %bc_5_1_1_0\n"
		"              OpStore %gep_5_1_1_1 %bc_5_1_1_1\n"
		"              OpStore %gep_5_1_1_2 %bc_5_1_1_2\n"
		"              OpStore %gep_5_2_0 %bc_5_2_0\n"
		"              OpStore %gep_5_2_1_0 %bc_5_2_1_0\n"
		"              OpStore %gep_5_2_1_1 %bc_5_2_1_1\n"
		"              OpStore %gep_5_2_1_2 %bc_5_2_1_2\n"

		// [5 x <2 x half>] offset 80
		"    %ex_6_0 = OpCompositeExtract %v2f16 %st_val 6 0\n"
		"    %ex_6_1 = OpCompositeExtract %v2f16 %st_val 6 1\n"
		"    %ex_6_2 = OpCompositeExtract %v2f16 %st_val 6 2\n"
		"    %ex_6_3 = OpCompositeExtract %v2f16 %st_val 6 3\n"
		"    %ex_6_4 = OpCompositeExtract %v2f16 %st_val 6 4\n"
		"    %bc_6_0 = OpBitcast %u32 %ex_6_0\n"
		"    %bc_6_1 = OpBitcast %u32 %ex_6_1\n"
		"    %bc_6_2 = OpBitcast %u32 %ex_6_2\n"
		"    %bc_6_3 = OpBitcast %u32 %ex_6_3\n"
		"    %bc_6_4 = OpBitcast %u32 %ex_6_4\n"
		"   %gep_6_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_20\n"
		"   %gep_6_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_21\n"
		"   %gep_6_2 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_22\n"
		"   %gep_6_3 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_23\n"
		"   %gep_6_4 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_24\n"
		"              OpStore %gep_6_0 %bc_6_0\n"
		"              OpStore %gep_6_1 %bc_6_1\n"
		"              OpStore %gep_6_2 %bc_6_2\n"
		"              OpStore %gep_6_3 %bc_6_3\n"
		"              OpStore %gep_6_4 %bc_6_4\n"

		// half offset 100
		"      %ex_7 = OpCompositeExtract %f16 %st_val 7\n"
		"     %vec_7 = OpCompositeConstruct %v2f16 %ex_7 %c_f16_n1\n"
		"      %bc_7 = OpBitcast %u32 %vec_7\n"
		"     %gep_7 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_25\n"
		"              OpStore %gep_7 %bc_7\n"

		// [5 x <3 x half>] offset 104
		"    %ex_8_0 = OpCompositeExtract %v3f16 %st_val 8 0\n"
		"    %ex_8_1 = OpCompositeExtract %v3f16 %st_val 8 1\n"
		"    %ex_8_2 = OpCompositeExtract %v3f16 %st_val 8 2\n"
		"    %ex_8_3 = OpCompositeExtract %v3f16 %st_val 8 3\n"
		"    %ex_8_4 = OpCompositeExtract %v3f16 %st_val 8 4\n"
		" %vec_8_0_0 = OpVectorShuffle %v2f16 %ex_8_0 %c_v2f16_n1 0 1\n"
		" %vec_8_0_1 = OpVectorShuffle %v2f16 %ex_8_0 %c_v2f16_n1 2 3\n"
		" %vec_8_1_0 = OpVectorShuffle %v2f16 %ex_8_1 %c_v2f16_n1 0 1\n"
		" %vec_8_1_1 = OpVectorShuffle %v2f16 %ex_8_1 %c_v2f16_n1 2 3\n"
		" %vec_8_2_0 = OpVectorShuffle %v2f16 %ex_8_2 %c_v2f16_n1 0 1\n"
		" %vec_8_2_1 = OpVectorShuffle %v2f16 %ex_8_2 %c_v2f16_n1 2 3\n"
		" %vec_8_3_0 = OpVectorShuffle %v2f16 %ex_8_3 %c_v2f16_n1 0 1\n"
		" %vec_8_3_1 = OpVectorShuffle %v2f16 %ex_8_3 %c_v2f16_n1 2 3\n"
		" %vec_8_4_0 = OpVectorShuffle %v2f16 %ex_8_4 %c_v2f16_n1 0 1\n"
		" %vec_8_4_1 = OpVectorShuffle %v2f16 %ex_8_4 %c_v2f16_n1 2 3\n"
		"  %bc_8_0_0 = OpBitcast %u32 %vec_8_0_0\n"
		"  %bc_8_0_1 = OpBitcast %u32 %vec_8_0_1\n"
		"  %bc_8_1_0 = OpBitcast %u32 %vec_8_1_0\n"
		"  %bc_8_1_1 = OpBitcast %u32 %vec_8_1_1\n"
		"  %bc_8_2_0 = OpBitcast %u32 %vec_8_2_0\n"
		"  %bc_8_2_1 = OpBitcast %u32 %vec_8_2_1\n"
		"  %bc_8_3_0 = OpBitcast %u32 %vec_8_3_0\n"
		"  %bc_8_3_1 = OpBitcast %u32 %vec_8_3_1\n"
		"  %bc_8_4_0 = OpBitcast %u32 %vec_8_4_0\n"
		"  %bc_8_4_1 = OpBitcast %u32 %vec_8_4_1\n"
		" %gep_8_0_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_26\n"
		" %gep_8_0_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_27\n"
		" %gep_8_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_28\n"
		" %gep_8_1_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_29\n"
		" %gep_8_2_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_30\n"
		" %gep_8_2_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_31\n"
		" %gep_8_3_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_32\n"
		" %gep_8_3_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_33\n"
		" %gep_8_4_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_34\n"
		" %gep_8_4_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_35\n"
		"              OpStore %gep_8_0_0 %bc_8_0_0\n"
		"              OpStore %gep_8_0_1 %bc_8_0_1\n"
		"              OpStore %gep_8_1_0 %bc_8_1_0\n"
		"              OpStore %gep_8_1_1 %bc_8_1_1\n"
		"              OpStore %gep_8_2_0 %bc_8_2_0\n"
		"              OpStore %gep_8_2_1 %bc_8_2_1\n"
		"              OpStore %gep_8_3_0 %bc_8_3_0\n"
		"              OpStore %gep_8_3_1 %bc_8_3_1\n"
		"              OpStore %gep_8_4_0 %bc_8_4_0\n"
		"              OpStore %gep_8_4_1 %bc_8_4_1\n"

		// [3 x <4 x half>] offset 144
		"    %ex_9_0 = OpCompositeExtract %v4f16 %st_val 9 0\n"
		"    %ex_9_1 = OpCompositeExtract %v4f16 %st_val 9 1\n"
		"    %ex_9_2 = OpCompositeExtract %v4f16 %st_val 9 2\n"
		" %vec_9_0_0 = OpVectorShuffle %v2f16 %ex_9_0 %ex_9_0 0 1\n"
		" %vec_9_0_1 = OpVectorShuffle %v2f16 %ex_9_0 %ex_9_0 2 3\n"
		" %vec_9_1_0 = OpVectorShuffle %v2f16 %ex_9_1 %ex_9_1 0 1\n"
		" %vec_9_1_1 = OpVectorShuffle %v2f16 %ex_9_1 %ex_9_1 2 3\n"
		" %vec_9_2_0 = OpVectorShuffle %v2f16 %ex_9_2 %ex_9_2 0 1\n"
		" %vec_9_2_1 = OpVectorShuffle %v2f16 %ex_9_2 %ex_9_2 2 3\n"
		"  %bc_9_0_0 = OpBitcast %u32 %vec_9_0_0\n"
		"  %bc_9_0_1 = OpBitcast %u32 %vec_9_0_1\n"
		"  %bc_9_1_0 = OpBitcast %u32 %vec_9_1_0\n"
		"  %bc_9_1_1 = OpBitcast %u32 %vec_9_1_1\n"
		"  %bc_9_2_0 = OpBitcast %u32 %vec_9_2_0\n"
		"  %bc_9_2_1 = OpBitcast %u32 %vec_9_2_1\n"
		" %gep_9_0_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_36\n"
		" %gep_9_0_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_37\n"
		" %gep_9_1_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_38\n"
		" %gep_9_1_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_39\n"
		" %gep_9_2_0 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_40\n"
		" %gep_9_2_1 = OpAccessChain %up_u32 %ssbo_dst %c_u32_0 %ndx %c_u32_41\n"
		"              OpStore %gep_9_0_0 %bc_9_0_0\n"
		"              OpStore %gep_9_0_1 %bc_9_0_1\n"
		"              OpStore %gep_9_1_0 %bc_9_1_0\n"
		"              OpStore %gep_9_1_1 %bc_9_1_1\n"
		"              OpStore %gep_9_2_0 %bc_9_2_0\n"
		"              OpStore %gep_9_2_1 %bc_9_2_1\n"

		"              OpBranch %next\n"

		"      %next = OpLabel\n"
		"     %i_cur = OpLoad %i32 %i\n"
		"     %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"              OpStore %i %i_new\n"
		"              OpBranch %loop\n"

		"     %merge = OpLabel\n"
		"              OpBranch %end_if\n"
		"    %end_if = OpLabel\n"
		"              OpReturnValue %param\n"
		"              OpFunctionEnd\n"
	);

	{
		SpecResource		specResource;
		map<string, string>	specs;
		VulkanFeatures		features;
		map<string, string>	fragments;
		vector<string>		extensions;
		vector<deFloat16>	expectedOutput;
		string				consts;

		for (deUint32 elementNdx = 0; elementNdx < numElements; ++elementNdx)
		{
			vector<deFloat16>	expectedIterationOutput;

			for (deUint32 structItemNdx = 0; structItemNdx < structItemsCount; ++structItemNdx)
				expectedIterationOutput.push_back(tcu::Float16(float(structItemNdx)).bits());

			for (deUint32 structItemNdx = 0; structItemNdx < DE_LENGTH_OF_ARRAY(exceptionIndices); ++structItemNdx)
				expectedIterationOutput[exceptionIndices[structItemNdx]] = exceptionValue;

			expectedIterationOutput[fieldModifiedMulIndex] = tcu::Float16(float(elementNdx * fieldModifier)).bits();
			expectedIterationOutput[fieldModifiedAddIndex] = tcu::Float16(float(elementNdx + fieldModifier)).bits();

			expectedOutput.insert(expectedOutput.end(), expectedIterationOutput.begin(), expectedIterationOutput.end());
		}

		for (deUint32 i = 0; i < structItemsCount; ++i)
			consts += "     %c_f16_" + de::toString(i) + " = OpConstant %f16 "  + de::toString(i) + "\n";

		specs["num_elements"]		= de::toString(numElements);
		specs["struct_item_size"]	= de::toString(structItemsCount * sizeof(deFloat16));
		specs["field_modifier"]		= de::toString(fieldModifier);
		specs["consts"]				= consts;

		fragments["capability"]		= "OpCapability Float16\n";
		fragments["decoration"]		= decoration.specialize(specs);
		fragments["pre_main"]		= preMain.specialize(specs);
		fragments["testfun"]		= testFun.specialize(specs);

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(expectedOutput)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(expectedOutput)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.verifyIO = compareFP16CompositeFunc;

		extensions.push_back("VK_KHR_shader_float16_int8");

		features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

		finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
	}

	return testGroup.release();
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16CompositeInsertExtractSet (tcu::TestContext& testCtx, const char* op)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup		(new tcu::TestCaseGroup(testCtx, de::toLower(op).c_str(), op));
	const deFloat16						exceptionValue	= tcu::Float16(-1.0).bits();
	const string						opName			(op);
	const deUint32						opIndex			= (opName == "OpCompositeInsert") ? 0
														: (opName == "OpCompositeExtract") ? 1
														: std::numeric_limits<deUint32>::max();

	const StringTemplate preMain
	(
		"   %c_i32_ndp = OpConstant %i32 ${num_elements}\n"
		"  %c_i32_hndp = OpSpecConstantOp %i32 SDiv %c_i32_ndp %c_i32_2\n"
		"  %c_i32_size = OpConstant %i32 ${struct_u32s}\n"
		"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
		" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n"
		"         %f16 = OpTypeFloat 16\n"
		"       %v2f16 = OpTypeVector %f16 2\n"
		"       %v3f16 = OpTypeVector %f16 3\n"
		"       %v4f16 = OpTypeVector %f16 4\n"
		"    %c_f16_na = OpConstant %f16 -1.0\n"
		"  %c_v2f16_n1 = OpConstantComposite %v2f16 %c_f16_na %c_f16_na\n"
		"     %c_u32_5 = OpConstant %u32 5\n"
		"     %c_i32_5 = OpConstant %i32 5\n"
		"     %c_i32_6 = OpConstant %i32 6\n"
		"     %c_i32_7 = OpConstant %i32 7\n"
		"     %c_i32_8 = OpConstant %i32 8\n"
		"     %c_i32_9 = OpConstant %i32 9\n"
		"    %c_i32_10 = OpConstant %i32 10\n"
		"    %c_i32_11 = OpConstant %i32 11\n"

		"%f16arr3      = OpTypeArray %f16 %c_u32_3\n"
		"%v2f16arr3    = OpTypeArray %v2f16 %c_u32_3\n"
		"%v2f16arr5    = OpTypeArray %v2f16 %c_u32_5\n"
		"%v3f16arr5    = OpTypeArray %v3f16 %c_u32_5\n"
		"%v4f16arr3    = OpTypeArray %v4f16 %c_u32_3\n"
		"%struct16     = OpTypeStruct %f16 %v2f16arr3\n"
		"%struct16arr3 = OpTypeArray %struct16 %c_u32_3\n"
		"%st_test      = OpTypeStruct %${field_type}\n"

		"      %ra_f16 = OpTypeArray %u32 %c_i32_hndp\n"
		"       %ra_st = OpTypeArray %u32 %c_i32_size\n"
		"      %up_u32 = OpTypePointer Uniform %u32\n"
		"     %st_test_i32_fn = OpTypeFunction %st_test %i32\n"
		"%void_st_test_i32_fn = OpTypeFunction %void %st_test %i32\n"
		"         %f16_i32_fn = OpTypeFunction %f16 %i32\n"
		"    %void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
		"       %v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
		"  %void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n"

		"${op_premain_decls}"

		" %up_SSBO_src = OpTypePointer Uniform %SSBO_src\n"
		" %up_SSBO_dst = OpTypePointer Uniform %SSBO_dst\n"

		"    %ssbo_src = OpVariable %up_SSBO_src Uniform\n"
		"    %ssbo_dst = OpVariable %up_SSBO_dst Uniform\n"
	);

	const StringTemplate decoration
	(
		"OpDecorate %SSBO_src BufferBlock\n"
		"OpDecorate %SSBO_dst BufferBlock\n"
		"OpDecorate %ra_f16 ArrayStride 4\n"
		"OpDecorate %ra_st ArrayStride 4\n"
		"OpDecorate %ssbo_src DescriptorSet 0\n"
		"OpDecorate %ssbo_src Binding 0\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 1\n"

		"OpMemberDecorate %SSBO_src 0 Offset 0\n"
		"OpMemberDecorate %SSBO_dst 0 Offset 0\n"

		"OpDecorate %v2f16arr3 ArrayStride 4\n"
		"OpMemberDecorate %struct16 0 Offset 0\n"
		"OpMemberDecorate %struct16 1 Offset 4\n"
		"OpDecorate %struct16arr3 ArrayStride 16\n"
		"OpDecorate %f16arr3 ArrayStride 2\n"
		"OpDecorate %v2f16arr5 ArrayStride 4\n"
		"OpDecorate %v3f16arr5 ArrayStride 8\n"
		"OpDecorate %v4f16arr3 ArrayStride 8\n"

		"OpMemberDecorate %st_test 0 Offset 0\n"
	);

	const StringTemplate testFun
	(
		" %test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"     %param = OpFunctionParameter %v4f32\n"
		"     %entry = OpLabel\n"

		"         %i = OpVariable %fp_i32 Function\n"
		"              OpStore %i %c_i32_0\n"

		"  %will_run = OpFunctionCall %bool %isUniqueIdZero\n"
		"              OpSelectionMerge %end_if None\n"
		"              OpBranchConditional %will_run %run_test %end_if\n"

		"  %run_test = OpLabel\n"
		"              OpBranch %loop\n"

		"      %loop = OpLabel\n"
		"     %i_cmp = OpLoad %i32 %i\n"
		"        %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"              OpLoopMerge %merge %next None\n"
		"              OpBranchConditional %lt %write %merge\n"

		"     %write = OpLabel\n"
		"       %ndx = OpLoad %i32 %i\n"

		"${op_sw_fun_call}"

		"    %dst_st = OpFunctionCall %void %${st_call} %val_dst %${st_ndx}\n"
		"              OpBranch %next\n"

		"      %next = OpLabel\n"
		"     %i_cur = OpLoad %i32 %i\n"
		"     %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"              OpStore %i %i_new\n"
		"              OpBranch %loop\n"

		"     %merge = OpLabel\n"
		"              OpBranch %end_if\n"
		"    %end_if = OpLabel\n"
		"              OpReturnValue %param\n"
		"              OpFunctionEnd\n"

		"${op_sw_fun_header}"
		" %sw_param = OpFunctionParameter %st_test\n"
		"%sw_paramn = OpFunctionParameter %i32\n"
		" %sw_entry = OpLabel\n"
		"             OpSelectionMerge %switch_e None\n"
		"             OpSwitch %sw_paramn %default ${case_list}\n"

		"${case_bodies}"

		"%default   = OpLabel\n"
		"             OpReturnValue ${op_case_default_value}\n"
		"%switch_e  = OpLabel\n"
		"             OpUnreachable\n" // Unreachable merge block for switch statement
		"             OpFunctionEnd\n"
	);

	const StringTemplate testCaseBody
	(
		"%case_${case_ndx}    = OpLabel\n"
		"%val_ret_${case_ndx} = ${op_name} ${op_args_part} ${access_path}\n"
		"             OpReturnValue %val_ret_${case_ndx}\n"
	);

	const string loadF16
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"   %ld_${var}_call = OpFunctionCall %f16 %ld_arg_${var} %ld_${var}_param\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_call\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n" +
		loadScalarF16FromUint
	);

	const string loadV2F16
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"   %ld_${var}_call = OpFunctionCall %v2f16 %ld_arg_${var} %ld_${var}_param\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_call\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n" +
		loadV2F16FromUint
	);

	const string loadV3F16
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"  %ld_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"  %ld_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"   %ld_${var}_ld_0 = OpLoad %u32 %ld_${var}_gep_0\n"
		"   %ld_${var}_ld_1 = OpLoad %u32 %ld_${var}_gep_1\n"
		"   %ld_${var}_bc_0 = OpBitcast %v2f16 %ld_${var}_ld_0\n"
		"   %ld_${var}_bc_1 = OpBitcast %v2f16 %ld_${var}_ld_1\n"
		"    %ld_${var}_vec = OpVectorShuffle %v3f16 %ld_${var}_bc_0 %ld_${var}_bc_1 0 1 2\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_vec\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadV4F16
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"  %ld_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"  %ld_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"   %ld_${var}_ld_0 = OpLoad %u32 %ld_${var}_gep_0\n"
		"   %ld_${var}_ld_1 = OpLoad %u32 %ld_${var}_gep_1\n"
		"   %ld_${var}_bc_0 = OpBitcast %v2f16 %ld_${var}_ld_0\n"
		"   %ld_${var}_bc_1 = OpBitcast %v2f16 %ld_${var}_ld_1\n"
		"    %ld_${var}_vec = OpVectorShuffle %v4f16 %ld_${var}_bc_0 %ld_${var}_bc_1 0 1 2 3\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_vec\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadF16Arr3
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"  %ld_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_u32_0 %c_u32_0\n"
		"  %ld_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_u32_0 %c_u32_1\n"
		"   %ld_${var}_ld_0 = OpLoad %u32 %ld_${var}_gep_0\n"
		"   %ld_${var}_ld_1 = OpLoad %u32 %ld_${var}_gep_1\n"
		"   %ld_${var}_bc_0 = OpBitcast %v2f16 %ld_${var}_ld_0\n"
		"   %ld_${var}_bc_1 = OpBitcast %v2f16 %ld_${var}_ld_1\n"
		"   %ld_${var}_ex_0 = OpCompositeExtract %f16 %ld_${var}_bc_0 0\n"
		"   %ld_${var}_ex_1 = OpCompositeExtract %f16 %ld_${var}_bc_0 1\n"
		"   %ld_${var}_ex_2 = OpCompositeExtract %f16 %ld_${var}_bc_1 0\n"
		"   %ld_${var}_cons = OpCompositeConstruct %f16arr3 %ld_${var}_ex_0 %ld_${var}_ex_1 %ld_${var}_ex_2\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_cons\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadV2F16Arr5
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_label = OpLabel\n"
		"  %ld_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"  %ld_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"  %ld_${var}_gep_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"  %ld_${var}_gep_3 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"  %ld_${var}_gep_4 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"   %ld_${var}_ld_0 = OpLoad %u32 %ld_${var}_gep_0\n"
		"   %ld_${var}_ld_1 = OpLoad %u32 %ld_${var}_gep_1\n"
		"   %ld_${var}_ld_2 = OpLoad %u32 %ld_${var}_gep_2\n"
		"   %ld_${var}_ld_3 = OpLoad %u32 %ld_${var}_gep_3\n"
		"   %ld_${var}_ld_4 = OpLoad %u32 %ld_${var}_gep_4\n"
		"   %ld_${var}_bc_0 = OpBitcast %v2f16 %ld_${var}_ld_0\n"
		"   %ld_${var}_bc_1 = OpBitcast %v2f16 %ld_${var}_ld_1\n"
		"   %ld_${var}_bc_2 = OpBitcast %v2f16 %ld_${var}_ld_2\n"
		"   %ld_${var}_bc_3 = OpBitcast %v2f16 %ld_${var}_ld_3\n"
		"   %ld_${var}_bc_4 = OpBitcast %v2f16 %ld_${var}_ld_4\n"
		"   %ld_${var}_cons = OpCompositeConstruct %v2f16arr5 %ld_${var}_bc_0 %ld_${var}_bc_1 %ld_${var}_bc_2 %ld_${var}_bc_3 %ld_${var}_bc_4\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_cons\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadV3F16Arr5
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"%ld_${var}_gep_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"%ld_${var}_gep_0_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"%ld_${var}_gep_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"%ld_${var}_gep_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"%ld_${var}_gep_2_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"%ld_${var}_gep_2_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		"%ld_${var}_gep_3_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_6\n"
		"%ld_${var}_gep_3_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_7\n"
		"%ld_${var}_gep_4_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_8\n"
		"%ld_${var}_gep_4_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_9\n"
		" %ld_${var}_ld_0_0 = OpLoad %u32 %ld_${var}_gep_0_0\n"
		" %ld_${var}_ld_0_1 = OpLoad %u32 %ld_${var}_gep_0_1\n"
		" %ld_${var}_ld_1_0 = OpLoad %u32 %ld_${var}_gep_1_0\n"
		" %ld_${var}_ld_1_1 = OpLoad %u32 %ld_${var}_gep_1_1\n"
		" %ld_${var}_ld_2_0 = OpLoad %u32 %ld_${var}_gep_2_0\n"
		" %ld_${var}_ld_2_1 = OpLoad %u32 %ld_${var}_gep_2_1\n"
		" %ld_${var}_ld_3_0 = OpLoad %u32 %ld_${var}_gep_3_0\n"
		" %ld_${var}_ld_3_1 = OpLoad %u32 %ld_${var}_gep_3_1\n"
		" %ld_${var}_ld_4_0 = OpLoad %u32 %ld_${var}_gep_4_0\n"
		" %ld_${var}_ld_4_1 = OpLoad %u32 %ld_${var}_gep_4_1\n"
		" %ld_${var}_bc_0_0 = OpBitcast %v2f16 %ld_${var}_ld_0_0\n"
		" %ld_${var}_bc_0_1 = OpBitcast %v2f16 %ld_${var}_ld_0_1\n"
		" %ld_${var}_bc_1_0 = OpBitcast %v2f16 %ld_${var}_ld_1_0\n"
		" %ld_${var}_bc_1_1 = OpBitcast %v2f16 %ld_${var}_ld_1_1\n"
		" %ld_${var}_bc_2_0 = OpBitcast %v2f16 %ld_${var}_ld_2_0\n"
		" %ld_${var}_bc_2_1 = OpBitcast %v2f16 %ld_${var}_ld_2_1\n"
		" %ld_${var}_bc_3_0 = OpBitcast %v2f16 %ld_${var}_ld_3_0\n"
		" %ld_${var}_bc_3_1 = OpBitcast %v2f16 %ld_${var}_ld_3_1\n"
		" %ld_${var}_bc_4_0 = OpBitcast %v2f16 %ld_${var}_ld_4_0\n"
		" %ld_${var}_bc_4_1 = OpBitcast %v2f16 %ld_${var}_ld_4_1\n"
		"  %ld_${var}_vec_0 = OpVectorShuffle %v3f16 %ld_${var}_bc_0_0 %ld_${var}_bc_0_1 0 1 2\n"
		"  %ld_${var}_vec_1 = OpVectorShuffle %v3f16 %ld_${var}_bc_1_0 %ld_${var}_bc_1_1 0 1 2\n"
		"  %ld_${var}_vec_2 = OpVectorShuffle %v3f16 %ld_${var}_bc_2_0 %ld_${var}_bc_2_1 0 1 2\n"
		"  %ld_${var}_vec_3 = OpVectorShuffle %v3f16 %ld_${var}_bc_3_0 %ld_${var}_bc_3_1 0 1 2\n"
		"  %ld_${var}_vec_4 = OpVectorShuffle %v3f16 %ld_${var}_bc_4_0 %ld_${var}_bc_4_1 0 1 2\n"
		"   %ld_${var}_cons = OpCompositeConstruct %v3f16arr5 %ld_${var}_vec_0 %ld_${var}_vec_1 %ld_${var}_vec_2 %ld_${var}_vec_3 %ld_${var}_vec_4\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_cons\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadV4F16Arr3
	(
		"        %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"  %ld_${var}_param = OpFunctionParameter %i32\n"
		"  %ld_${var}_entry = OpLabel\n"
		"%ld_${var}_gep_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"%ld_${var}_gep_0_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"%ld_${var}_gep_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"%ld_${var}_gep_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"%ld_${var}_gep_2_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"%ld_${var}_gep_2_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		" %ld_${var}_ld_0_0 = OpLoad %u32 %ld_${var}_gep_0_0\n"
		" %ld_${var}_ld_0_1 = OpLoad %u32 %ld_${var}_gep_0_1\n"
		" %ld_${var}_ld_1_0 = OpLoad %u32 %ld_${var}_gep_1_0\n"
		" %ld_${var}_ld_1_1 = OpLoad %u32 %ld_${var}_gep_1_1\n"
		" %ld_${var}_ld_2_0 = OpLoad %u32 %ld_${var}_gep_2_0\n"
		" %ld_${var}_ld_2_1 = OpLoad %u32 %ld_${var}_gep_2_1\n"
		" %ld_${var}_bc_0_0 = OpBitcast %v2f16 %ld_${var}_ld_0_0\n"
		" %ld_${var}_bc_0_1 = OpBitcast %v2f16 %ld_${var}_ld_0_1\n"
		" %ld_${var}_bc_1_0 = OpBitcast %v2f16 %ld_${var}_ld_1_0\n"
		" %ld_${var}_bc_1_1 = OpBitcast %v2f16 %ld_${var}_ld_1_1\n"
		" %ld_${var}_bc_2_0 = OpBitcast %v2f16 %ld_${var}_ld_2_0\n"
		" %ld_${var}_bc_2_1 = OpBitcast %v2f16 %ld_${var}_ld_2_1\n"
		"  %ld_${var}_vec_0 = OpVectorShuffle %v4f16 %ld_${var}_bc_0_0 %ld_${var}_bc_0_1 0 1 2 3\n"
		"  %ld_${var}_vec_1 = OpVectorShuffle %v4f16 %ld_${var}_bc_1_0 %ld_${var}_bc_1_1 0 1 2 3\n"
		"  %ld_${var}_vec_2 = OpVectorShuffle %v4f16 %ld_${var}_bc_2_0 %ld_${var}_bc_2_1 0 1 2 3\n"
		"   %ld_${var}_cons = OpCompositeConstruct %v4f16arr3 %ld_${var}_vec_0 %ld_${var}_vec_1 %ld_${var}_vec_2\n"
		"%ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_cons\n"
		"                     OpReturnValue %ld_${var}_st_test\n"
		"                     OpFunctionEnd\n"
	);

	const string loadStruct16Arr3
	(
		"          %ld_${var} = OpFunction %st_test None %st_test_i32_fn\n"
		"    %ld_${var}_param = OpFunctionParameter %i32\n"
		"    %ld_${var}_entry = OpLabel\n"
		"%ld_${var}_gep_0_0   = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"%ld_${var}_gep_0_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"%ld_${var}_gep_0_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"%ld_${var}_gep_0_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"%ld_${var}_gep_1_0   = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"%ld_${var}_gep_1_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		"%ld_${var}_gep_1_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_6\n"
		"%ld_${var}_gep_1_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_7\n"
		"%ld_${var}_gep_2_0   = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_8\n"
		"%ld_${var}_gep_2_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_9\n"
		"%ld_${var}_gep_2_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_10\n"
		"%ld_${var}_gep_2_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_11\n"
		" %ld_${var}_ld_0_0   = OpLoad %u32 %ld_${var}_gep_0_0\n"
		" %ld_${var}_ld_0_1_0 = OpLoad %u32 %ld_${var}_gep_0_1_0\n"
		" %ld_${var}_ld_0_1_1 = OpLoad %u32 %ld_${var}_gep_0_1_1\n"
		" %ld_${var}_ld_0_1_2 = OpLoad %u32 %ld_${var}_gep_0_1_2\n"
		" %ld_${var}_ld_1_0   = OpLoad %u32 %ld_${var}_gep_1_0\n"
		" %ld_${var}_ld_1_1_0 = OpLoad %u32 %ld_${var}_gep_1_1_0\n"
		" %ld_${var}_ld_1_1_1 = OpLoad %u32 %ld_${var}_gep_1_1_1\n"
		" %ld_${var}_ld_1_1_2 = OpLoad %u32 %ld_${var}_gep_1_1_2\n"
		" %ld_${var}_ld_2_0   = OpLoad %u32 %ld_${var}_gep_2_0\n"
		" %ld_${var}_ld_2_1_0 = OpLoad %u32 %ld_${var}_gep_2_1_0\n"
		" %ld_${var}_ld_2_1_1 = OpLoad %u32 %ld_${var}_gep_2_1_1\n"
		" %ld_${var}_ld_2_1_2 = OpLoad %u32 %ld_${var}_gep_2_1_2\n"
		" %ld_${var}_bc_0_0   = OpBitcast %v2f16 %ld_${var}_ld_0_0\n"
		" %ld_${var}_bc_0_1_0 = OpBitcast %v2f16 %ld_${var}_ld_0_1_0\n"
		" %ld_${var}_bc_0_1_1 = OpBitcast %v2f16 %ld_${var}_ld_0_1_1\n"
		" %ld_${var}_bc_0_1_2 = OpBitcast %v2f16 %ld_${var}_ld_0_1_2\n"
		" %ld_${var}_bc_1_0   = OpBitcast %v2f16 %ld_${var}_ld_1_0\n"
		" %ld_${var}_bc_1_1_0 = OpBitcast %v2f16 %ld_${var}_ld_1_1_0\n"
		" %ld_${var}_bc_1_1_1 = OpBitcast %v2f16 %ld_${var}_ld_1_1_1\n"
		" %ld_${var}_bc_1_1_2 = OpBitcast %v2f16 %ld_${var}_ld_1_1_2\n"
		" %ld_${var}_bc_2_0   = OpBitcast %v2f16 %ld_${var}_ld_2_0\n"
		" %ld_${var}_bc_2_1_0 = OpBitcast %v2f16 %ld_${var}_ld_2_1_0\n"
		" %ld_${var}_bc_2_1_1 = OpBitcast %v2f16 %ld_${var}_ld_2_1_1\n"
		" %ld_${var}_bc_2_1_2 = OpBitcast %v2f16 %ld_${var}_ld_2_1_2\n"
		"    %ld_${var}_arr_0 = OpCompositeConstruct %v2f16arr3 %ld_${var}_bc_0_1_0 %ld_${var}_bc_0_1_1 %ld_${var}_bc_0_1_2\n"
		"    %ld_${var}_arr_1 = OpCompositeConstruct %v2f16arr3 %ld_${var}_bc_1_1_0 %ld_${var}_bc_1_1_1 %ld_${var}_bc_1_1_2\n"
		"    %ld_${var}_arr_2 = OpCompositeConstruct %v2f16arr3 %ld_${var}_bc_2_1_0 %ld_${var}_bc_2_1_1 %ld_${var}_bc_2_1_2\n"
		"     %ld_${var}_ex_0 = OpCompositeExtract %f16 %ld_${var}_bc_0_0 0\n"
		"     %ld_${var}_ex_1 = OpCompositeExtract %f16 %ld_${var}_bc_1_0 0\n"
		"     %ld_${var}_ex_2 = OpCompositeExtract %f16 %ld_${var}_bc_2_0 0\n"
		"     %ld_${var}_st_0 = OpCompositeConstruct %struct16 %ld_${var}_ex_0 %ld_${var}_arr_0\n"
		"     %ld_${var}_st_1 = OpCompositeConstruct %struct16 %ld_${var}_ex_1 %ld_${var}_arr_1\n"
		"     %ld_${var}_st_2 = OpCompositeConstruct %struct16 %ld_${var}_ex_2 %ld_${var}_arr_2\n"
		"     %ld_${var}_cons = OpCompositeConstruct %struct16arr3 %ld_${var}_st_0 %ld_${var}_st_1 %ld_${var}_st_2\n"
		"  %ld_${var}_st_test = OpCompositeConstruct %st_test %ld_${var}_cons\n"
		"                       OpReturnValue %ld_${var}_st_test\n"
		"                      OpFunctionEnd\n"
	);

	const string storeF16
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"    %st_${var}_ex = OpCompositeExtract %f16 %st_${var}_param1 0\n"
		"  %st_${var}_call = OpFunctionCall %void %st_fn_${var} %st_${var}_ex %st_${var}_param2\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n" +
		storeScalarF16AsUint
	);

	const string storeV2F16
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"    %st_${var}_ex = OpCompositeExtract %v2f16 %st_${var}_param1 0\n"
		"  %st_${var}_call = OpFunctionCall %void %st_fn_${var} %st_${var}_ex %st_${var}_param2\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n" +
		storeV2F16AsUint
	);

	const string storeV3F16
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"    %st_${var}_ex = OpCompositeExtract %v3f16 %st_${var}_param1 0\n"
		" %st_${var}_vec_0 = OpVectorShuffle %v2f16 %st_${var}_ex %c_v2f16_n1 0 1\n"
		" %st_${var}_vec_1 = OpVectorShuffle %v2f16 %st_${var}_ex %c_v2f16_n1 2 3\n"
		"  %st_${var}_bc_0 = OpBitcast %u32 %st_${var}_vec_0\n"
		"  %st_${var}_bc_1 = OpBitcast %u32 %st_${var}_vec_1\n"
		" %st_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		" %st_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"                    OpStore %st_${var}_gep_0 %st_${var}_bc_0\n"
		"                    OpStore %st_${var}_gep_1 %st_${var}_bc_1\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n"
	);

	const string storeV4F16
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"    %st_${var}_ex = OpCompositeExtract %v4f16 %st_${var}_param1 0\n"
		" %st_${var}_vec_0 = OpVectorShuffle %v2f16 %st_${var}_ex %c_v2f16_n1 0 1\n"
		" %st_${var}_vec_1 = OpVectorShuffle %v2f16 %st_${var}_ex %c_v2f16_n1 2 3\n"
		"  %st_${var}_bc_0 = OpBitcast %u32 %st_${var}_vec_0\n"
		"  %st_${var}_bc_1 = OpBitcast %u32 %st_${var}_vec_1\n"
		" %st_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		" %st_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"                    OpStore %st_${var}_gep_0 %st_${var}_bc_0\n"
		"                    OpStore %st_${var}_gep_1 %st_${var}_bc_1\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n"
	);

	const string storeF16Arr3
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"  %st_${var}_ex_0 = OpCompositeExtract %f16 %st_${var}_param1 0 0\n"
		"  %st_${var}_ex_1 = OpCompositeExtract %f16 %st_${var}_param1 0 1\n"
		"  %st_${var}_ex_2 = OpCompositeExtract %f16 %st_${var}_param1 0 2\n"
		" %st_${var}_vec_0 = OpCompositeConstruct %v2f16 %st_${var}_ex_0 %st_${var}_ex_1\n"
		" %st_${var}_vec_1 = OpCompositeConstruct %v2f16 %st_${var}_ex_2 %c_f16_na\n"
		"  %st_${var}_bc_0 = OpBitcast %u32 %st_${var}_vec_0\n"
		"  %st_${var}_bc_1 = OpBitcast %u32 %st_${var}_vec_1\n"
		" %st_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		" %st_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"                    OpStore %st_${var}_gep_0 %st_${var}_bc_0\n"
		"                    OpStore %st_${var}_gep_1 %st_${var}_bc_1\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n"
	);

	const string storeV2F16Arr5
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"  %st_${var}_ex_0 = OpCompositeExtract %v2f16 %st_${var}_param1 0 0\n"
		"  %st_${var}_ex_1 = OpCompositeExtract %v2f16 %st_${var}_param1 0 1\n"
		"  %st_${var}_ex_2 = OpCompositeExtract %v2f16 %st_${var}_param1 0 2\n"
		"  %st_${var}_ex_3 = OpCompositeExtract %v2f16 %st_${var}_param1 0 3\n"
		"  %st_${var}_ex_4 = OpCompositeExtract %v2f16 %st_${var}_param1 0 4\n"
		"  %st_${var}_bc_0 = OpBitcast %u32 %st_${var}_ex_0\n"
		"  %st_${var}_bc_1 = OpBitcast %u32 %st_${var}_ex_1\n"
		"  %st_${var}_bc_2 = OpBitcast %u32 %st_${var}_ex_2\n"
		"  %st_${var}_bc_3 = OpBitcast %u32 %st_${var}_ex_3\n"
		"  %st_${var}_bc_4 = OpBitcast %u32 %st_${var}_ex_4\n"
		" %st_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		" %st_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		" %st_${var}_gep_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		" %st_${var}_gep_3 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		" %st_${var}_gep_4 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"                    OpStore %st_${var}_gep_0 %st_${var}_bc_0\n"
		"                    OpStore %st_${var}_gep_1 %st_${var}_bc_1\n"
		"                    OpStore %st_${var}_gep_2 %st_${var}_bc_2\n"
		"                    OpStore %st_${var}_gep_3 %st_${var}_bc_3\n"
		"                    OpStore %st_${var}_gep_4 %st_${var}_bc_4\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n"
	);

	const string storeV3F16Arr5
	(
		"       %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"%st_${var}_param1 = OpFunctionParameter %st_test\n"
		"%st_${var}_param2 = OpFunctionParameter %i32\n"
		" %st_${var}_entry = OpLabel\n"
		"  %st_${var}_ex_0 = OpCompositeExtract %v3f16 %st_${var}_param1 0 0\n"
		"  %st_${var}_ex_1 = OpCompositeExtract %v3f16 %st_${var}_param1 0 1\n"
		"  %st_${var}_ex_2 = OpCompositeExtract %v3f16 %st_${var}_param1 0 2\n"
		"  %st_${var}_ex_3 = OpCompositeExtract %v3f16 %st_${var}_param1 0 3\n"
		"  %st_${var}_ex_4 = OpCompositeExtract %v3f16 %st_${var}_param1 0 4\n"
		"%st_${var}_v2_0_0 = OpVectorShuffle %v2f16 %st_${var}_ex_0 %c_v2f16_n1 0 1\n"
		"%st_${var}_v2_0_1 = OpVectorShuffle %v2f16 %st_${var}_ex_0 %c_v2f16_n1 2 3\n"
		"%st_${var}_v2_1_0 = OpVectorShuffle %v2f16 %st_${var}_ex_1 %c_v2f16_n1 0 1\n"
		"%st_${var}_v2_1_1 = OpVectorShuffle %v2f16 %st_${var}_ex_1 %c_v2f16_n1 2 3\n"
		"%st_${var}_v2_2_0 = OpVectorShuffle %v2f16 %st_${var}_ex_2 %c_v2f16_n1 0 1\n"
		"%st_${var}_v2_2_1 = OpVectorShuffle %v2f16 %st_${var}_ex_2 %c_v2f16_n1 2 3\n"
		"%st_${var}_v2_3_0 = OpVectorShuffle %v2f16 %st_${var}_ex_3 %c_v2f16_n1 0 1\n"
		"%st_${var}_v2_3_1 = OpVectorShuffle %v2f16 %st_${var}_ex_3 %c_v2f16_n1 2 3\n"
		"%st_${var}_v2_4_0 = OpVectorShuffle %v2f16 %st_${var}_ex_4 %c_v2f16_n1 0 1\n"
		"%st_${var}_v2_4_1 = OpVectorShuffle %v2f16 %st_${var}_ex_4 %c_v2f16_n1 2 3\n"
		"%st_${var}_bc_0_0 = OpBitcast %u32 %st_${var}_v2_0_0\n"
		"%st_${var}_bc_0_1 = OpBitcast %u32 %st_${var}_v2_0_1\n"
		"%st_${var}_bc_1_0 = OpBitcast %u32 %st_${var}_v2_1_0\n"
		"%st_${var}_bc_1_1 = OpBitcast %u32 %st_${var}_v2_1_1\n"
		"%st_${var}_bc_2_0 = OpBitcast %u32 %st_${var}_v2_2_0\n"
		"%st_${var}_bc_2_1 = OpBitcast %u32 %st_${var}_v2_2_1\n"
		"%st_${var}_bc_3_0 = OpBitcast %u32 %st_${var}_v2_3_0\n"
		"%st_${var}_bc_3_1 = OpBitcast %u32 %st_${var}_v2_3_1\n"
		"%st_${var}_bc_4_0 = OpBitcast %u32 %st_${var}_v2_4_0\n"
		"%st_${var}_bc_4_1 = OpBitcast %u32 %st_${var}_v2_4_1\n"
		" %st_${var}_gep_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		" %st_${var}_gep_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		" %st_${var}_gep_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		" %st_${var}_gep_3 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		" %st_${var}_gep_4 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		" %st_${var}_gep_5 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		" %st_${var}_gep_6 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_6\n"
		" %st_${var}_gep_7 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_7\n"
		" %st_${var}_gep_8 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_8\n"
		" %st_${var}_gep_9 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_9\n"
		"                    OpStore %st_${var}_gep_0 %st_${var}_bc_0_0\n"
		"                    OpStore %st_${var}_gep_1 %st_${var}_bc_0_1\n"
		"                    OpStore %st_${var}_gep_2 %st_${var}_bc_1_0\n"
		"                    OpStore %st_${var}_gep_3 %st_${var}_bc_1_1\n"
		"                    OpStore %st_${var}_gep_4 %st_${var}_bc_2_0\n"
		"                    OpStore %st_${var}_gep_5 %st_${var}_bc_2_1\n"
		"                    OpStore %st_${var}_gep_6 %st_${var}_bc_3_0\n"
		"                    OpStore %st_${var}_gep_7 %st_${var}_bc_3_1\n"
		"                    OpStore %st_${var}_gep_8 %st_${var}_bc_4_0\n"
		"                    OpStore %st_${var}_gep_9 %st_${var}_bc_4_1\n"
		"                    OpReturn\n"
		"                    OpFunctionEnd\n"
	);

	const string storeV4F16Arr3
	(
		"        %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		" %st_${var}_param1 = OpFunctionParameter %st_test\n"
		" %st_${var}_param2 = OpFunctionParameter %i32\n"
		"  %st_${var}_entry = OpLabel\n"
		"   %st_${var}_ex_0 = OpCompositeExtract %v4f16 %st_${var}_param1 0 0\n"
		"   %st_${var}_ex_1 = OpCompositeExtract %v4f16 %st_${var}_param1 0 1\n"
		"   %st_${var}_ex_2 = OpCompositeExtract %v4f16 %st_${var}_param1 0 2\n"
		"%st_${var}_vec_0_0 = OpVectorShuffle %v2f16 %st_${var}_ex_0 %st_${var}_ex_0 0 1\n"
		"%st_${var}_vec_0_1 = OpVectorShuffle %v2f16 %st_${var}_ex_0 %st_${var}_ex_0 2 3\n"
		"%st_${var}_vec_1_0 = OpVectorShuffle %v2f16 %st_${var}_ex_1 %st_${var}_ex_1 0 1\n"
		"%st_${var}_vec_1_1 = OpVectorShuffle %v2f16 %st_${var}_ex_1 %st_${var}_ex_1 2 3\n"
		"%st_${var}_vec_2_0 = OpVectorShuffle %v2f16 %st_${var}_ex_2 %st_${var}_ex_2 0 1\n"
		"%st_${var}_vec_2_1 = OpVectorShuffle %v2f16 %st_${var}_ex_2 %st_${var}_ex_2 2 3\n"
		" %st_${var}_bc_0_0 = OpBitcast %u32 %st_${var}_vec_0_0\n"
		" %st_${var}_bc_0_1 = OpBitcast %u32 %st_${var}_vec_0_1\n"
		" %st_${var}_bc_1_0 = OpBitcast %u32 %st_${var}_vec_1_0\n"
		" %st_${var}_bc_1_1 = OpBitcast %u32 %st_${var}_vec_1_1\n"
		" %st_${var}_bc_2_0 = OpBitcast %u32 %st_${var}_vec_2_0\n"
		" %st_${var}_bc_2_1 = OpBitcast %u32 %st_${var}_vec_2_1\n"
		"%st_${var}_gep_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"%st_${var}_gep_0_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"%st_${var}_gep_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"%st_${var}_gep_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"%st_${var}_gep_2_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"%st_${var}_gep_2_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		"                     OpStore %st_${var}_gep_0_0 %st_${var}_bc_0_0\n"
		"                     OpStore %st_${var}_gep_0_1 %st_${var}_bc_0_1\n"
		"                     OpStore %st_${var}_gep_1_0 %st_${var}_bc_1_0\n"
		"                     OpStore %st_${var}_gep_1_1 %st_${var}_bc_1_1\n"
		"                     OpStore %st_${var}_gep_2_0 %st_${var}_bc_2_0\n"
		"                     OpStore %st_${var}_gep_2_1 %st_${var}_bc_2_1\n"
		"                     OpReturn\n"
		"                     OpFunctionEnd\n"
	);

	const string storeStruct16Arr3
	(
		"          %st_${var} = OpFunction %void None %void_st_test_i32_fn\n"
		"   %st_${var}_param1 = OpFunctionParameter %st_test\n"
		"   %st_${var}_param2 = OpFunctionParameter %i32\n"
		"    %st_${var}_entry = OpLabel\n"
		"     %st_${var}_st_0 = OpCompositeExtract %struct16 %st_${var}_param1 0 0\n"
		"     %st_${var}_st_1 = OpCompositeExtract %struct16 %st_${var}_param1 0 1\n"
		"     %st_${var}_st_2 = OpCompositeExtract %struct16 %st_${var}_param1 0 2\n"
		"   %st_${var}_el_0   = OpCompositeExtract   %f16 %st_${var}_st_0 0\n"
		"   %st_${var}_v2_0_0 = OpCompositeExtract %v2f16 %st_${var}_st_0 1 0\n"
		"   %st_${var}_v2_0_1 = OpCompositeExtract %v2f16 %st_${var}_st_0 1 1\n"
		"   %st_${var}_v2_0_2 = OpCompositeExtract %v2f16 %st_${var}_st_0 1 2\n"
		"   %st_${var}_el_1   = OpCompositeExtract   %f16 %st_${var}_st_1 0\n"
		"   %st_${var}_v2_1_0 = OpCompositeExtract %v2f16 %st_${var}_st_1 1 0\n"
		"   %st_${var}_v2_1_1 = OpCompositeExtract %v2f16 %st_${var}_st_1 1 1\n"
		"   %st_${var}_v2_1_2 = OpCompositeExtract %v2f16 %st_${var}_st_1 1 2\n"
		"   %st_${var}_el_2   = OpCompositeExtract   %f16 %st_${var}_st_2 0\n"
		"   %st_${var}_v2_2_0 = OpCompositeExtract %v2f16 %st_${var}_st_2 1 0\n"
		"   %st_${var}_v2_2_1 = OpCompositeExtract %v2f16 %st_${var}_st_2 1 1\n"
		"   %st_${var}_v2_2_2 = OpCompositeExtract %v2f16 %st_${var}_st_2 1 2\n"
		"     %st_${var}_v2_0 = OpCompositeConstruct %v2f16 %st_${var}_el_0 %c_f16_na\n"
		"     %st_${var}_v2_1 = OpCompositeConstruct %v2f16 %st_${var}_el_1 %c_f16_na\n"
		"     %st_${var}_v2_2 = OpCompositeConstruct %v2f16 %st_${var}_el_2 %c_f16_na\n"
		"   %st_${var}_bc_0   = OpBitcast %u32 %st_${var}_v2_0\n"
		"   %st_${var}_bc_0_0 = OpBitcast %u32 %st_${var}_v2_0_0\n"
		"   %st_${var}_bc_0_1 = OpBitcast %u32 %st_${var}_v2_0_1\n"
		"   %st_${var}_bc_0_2 = OpBitcast %u32 %st_${var}_v2_0_2\n"
		"   %st_${var}_bc_1   = OpBitcast %u32 %st_${var}_v2_1\n"
		"   %st_${var}_bc_1_0 = OpBitcast %u32 %st_${var}_v2_1_0\n"
		"   %st_${var}_bc_1_1 = OpBitcast %u32 %st_${var}_v2_1_1\n"
		"   %st_${var}_bc_1_2 = OpBitcast %u32 %st_${var}_v2_1_2\n"
		"   %st_${var}_bc_2   = OpBitcast %u32 %st_${var}_v2_2\n"
		"   %st_${var}_bc_2_0 = OpBitcast %u32 %st_${var}_v2_2_0\n"
		"   %st_${var}_bc_2_1 = OpBitcast %u32 %st_${var}_v2_2_1\n"
		"   %st_${var}_bc_2_2 = OpBitcast %u32 %st_${var}_v2_2_2\n"
		"%st_${var}_gep_0_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_0\n"
		"%st_${var}_gep_0_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_1\n"
		"%st_${var}_gep_0_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_2\n"
		"%st_${var}_gep_0_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_3\n"
		"%st_${var}_gep_1_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_4\n"
		"%st_${var}_gep_1_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_5\n"
		"%st_${var}_gep_1_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_6\n"
		"%st_${var}_gep_1_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_7\n"
		"%st_${var}_gep_2_0_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_8\n"
		"%st_${var}_gep_2_1_0 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_9\n"
		"%st_${var}_gep_2_1_1 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_10\n"
		"%st_${var}_gep_2_1_2 = OpAccessChain %up_u32 %${var} %c_i32_0 %c_i32_11\n"
		"                       OpStore %st_${var}_gep_0_0_0 %st_${var}_bc_0\n"
		"                       OpStore %st_${var}_gep_0_1_0 %st_${var}_bc_0_0\n"
		"                       OpStore %st_${var}_gep_0_1_1 %st_${var}_bc_0_1\n"
		"                       OpStore %st_${var}_gep_0_1_2 %st_${var}_bc_0_2\n"
		"                       OpStore %st_${var}_gep_1_0_0 %st_${var}_bc_1\n"
		"                       OpStore %st_${var}_gep_1_1_0 %st_${var}_bc_1_0\n"
		"                       OpStore %st_${var}_gep_1_1_1 %st_${var}_bc_1_1\n"
		"                       OpStore %st_${var}_gep_1_1_2 %st_${var}_bc_1_2\n"
		"                       OpStore %st_${var}_gep_2_0_0 %st_${var}_bc_2\n"
		"                       OpStore %st_${var}_gep_2_1_0 %st_${var}_bc_2_0\n"
		"                       OpStore %st_${var}_gep_2_1_1 %st_${var}_bc_2_1\n"
		"                       OpStore %st_${var}_gep_2_1_2 %st_${var}_bc_2_2\n"
		"                       OpReturn\n"
		"                       OpFunctionEnd\n"
	);

	struct OpParts
	{
		const char*	premainDecls;
		const char*	swFunCall;
		const char*	swFunHeader;
		const char*	caseDefaultValue;
		const char*	argsPartial;
	};

	OpParts								opPartsArray[]			=
	{
		// OpCompositeInsert
		{
			"       %fun_t = OpTypeFunction %st_test %f16 %st_test %i32\n"
			"    %SSBO_src = OpTypeStruct %ra_f16\n"
			"    %SSBO_dst = OpTypeStruct %ra_st\n",

			"   %val_new = OpFunctionCall %f16 %ld_arg_ssbo_src %ndx\n"
			"   %val_old = OpFunctionCall %st_test %ld_ssbo_dst %c_i32_0\n"
			"   %val_dst = OpFunctionCall %st_test %sw_fun %val_new %val_old %ndx\n",

			"   %sw_fun = OpFunction %st_test None %fun_t\n"
			"%sw_paramv = OpFunctionParameter %f16\n",

			"%sw_param",

			"%st_test %sw_paramv %sw_param",
		},
		// OpCompositeExtract
		{
			"       %fun_t = OpTypeFunction %f16 %st_test %i32\n"
			"    %SSBO_src = OpTypeStruct %ra_st\n"
			"    %SSBO_dst = OpTypeStruct %ra_f16\n",

			"   %val_src = OpFunctionCall %st_test %ld_ssbo_src %c_i32_0\n"
			"   %val_dst = OpFunctionCall %f16 %sw_fun %val_src %ndx\n",

			"   %sw_fun = OpFunction %f16 None %fun_t\n",

			"%c_f16_na",

			"%f16 %sw_param",
		},
	};

	DE_ASSERT(opIndex < DE_LENGTH_OF_ARRAY(opPartsArray));

	const char*	accessPathF16[] =
	{
		"0",			// %f16
		DE_NULL,
	};
	const char*	accessPathV2F16[] =
	{
		"0 0",			// %v2f16
		"0 1",
	};
	const char*	accessPathV3F16[] =
	{
		"0 0",			// %v3f16
		"0 1",
		"0 2",
		DE_NULL,
	};
	const char*	accessPathV4F16[] =
	{
		"0 0",			// %v4f16"
		"0 1",
		"0 2",
		"0 3",
	};
	const char*	accessPathF16Arr3[] =
	{
		"0 0",			// %f16arr3
		"0 1",
		"0 2",
		DE_NULL,
	};
	const char*	accessPathStruct16Arr3[] =
	{
		"0 0 0",		// %struct16arr3
		DE_NULL,
		"0 0 1 0 0",
		"0 0 1 0 1",
		"0 0 1 1 0",
		"0 0 1 1 1",
		"0 0 1 2 0",
		"0 0 1 2 1",
		"0 1 0",
		DE_NULL,
		"0 1 1 0 0",
		"0 1 1 0 1",
		"0 1 1 1 0",
		"0 1 1 1 1",
		"0 1 1 2 0",
		"0 1 1 2 1",
		"0 2 0",
		DE_NULL,
		"0 2 1 0 0",
		"0 2 1 0 1",
		"0 2 1 1 0",
		"0 2 1 1 1",
		"0 2 1 2 0",
		"0 2 1 2 1",
	};
	const char*	accessPathV2F16Arr5[] =
	{
		"0 0 0",		// %v2f16arr5
		"0 0 1",
		"0 1 0",
		"0 1 1",
		"0 2 0",
		"0 2 1",
		"0 3 0",
		"0 3 1",
		"0 4 0",
		"0 4 1",
	};
	const char*	accessPathV3F16Arr5[] =
	{
		"0 0 0",		// %v3f16arr5
		"0 0 1",
		"0 0 2",
		DE_NULL,
		"0 1 0",
		"0 1 1",
		"0 1 2",
		DE_NULL,
		"0 2 0",
		"0 2 1",
		"0 2 2",
		DE_NULL,
		"0 3 0",
		"0 3 1",
		"0 3 2",
		DE_NULL,
		"0 4 0",
		"0 4 1",
		"0 4 2",
		DE_NULL,
	};
	const char*	accessPathV4F16Arr3[] =
	{
		"0 0 0",		// %v4f16arr3
		"0 0 1",
		"0 0 2",
		"0 0 3",
		"0 1 0",
		"0 1 1",
		"0 1 2",
		"0 1 3",
		"0 2 0",
		"0 2 1",
		"0 2 2",
		"0 2 3",
		DE_NULL,
		DE_NULL,
		DE_NULL,
		DE_NULL,
	};

	struct TypeTestParameters
	{
		const char*		name;
		size_t			accessPathLength;
		const char**	accessPath;
		const string	loadFunction;
		const string	storeFunction;
	};

	const TypeTestParameters typeTestParameters[] =
	{
		{	"f16",			DE_LENGTH_OF_ARRAY(accessPathF16),			accessPathF16,			loadF16,			storeF16		 },
		{	"v2f16",		DE_LENGTH_OF_ARRAY(accessPathV2F16),		accessPathV2F16,		loadV2F16,			storeV2F16		 },
		{	"v3f16",		DE_LENGTH_OF_ARRAY(accessPathV3F16),		accessPathV3F16,		loadV3F16,			storeV3F16		 },
		{	"v4f16",		DE_LENGTH_OF_ARRAY(accessPathV4F16),		accessPathV4F16,		loadV4F16,			storeV4F16		  },
		{	"f16arr3",		DE_LENGTH_OF_ARRAY(accessPathF16Arr3),		accessPathF16Arr3,		loadF16Arr3,		storeF16Arr3	  },
		{	"v2f16arr5",	DE_LENGTH_OF_ARRAY(accessPathV2F16Arr5),	accessPathV2F16Arr5,	loadV2F16Arr5,		storeV2F16Arr5	  },
		{	"v3f16arr5",	DE_LENGTH_OF_ARRAY(accessPathV3F16Arr5),	accessPathV3F16Arr5,	loadV3F16Arr5,		storeV3F16Arr5	  },
		{	"v4f16arr3",	DE_LENGTH_OF_ARRAY(accessPathV4F16Arr3),	accessPathV4F16Arr3,	loadV4F16Arr3,		storeV4F16Arr3	  },
		{	"struct16arr3",	DE_LENGTH_OF_ARRAY(accessPathStruct16Arr3),	accessPathStruct16Arr3,	loadStruct16Arr3,	storeStruct16Arr3},
	};

	for (size_t typeTestNdx = 0; typeTestNdx < DE_LENGTH_OF_ARRAY(typeTestParameters); ++typeTestNdx)
	{
		const OpParts		opParts				= opPartsArray[opIndex];
		const string		testName			= typeTestParameters[typeTestNdx].name;
		const size_t		structItemsCount	= typeTestParameters[typeTestNdx].accessPathLength;
		const char**		accessPath			= typeTestParameters[typeTestNdx].accessPath;
		SpecResource		specResource;
		map<string, string>	specs;
		VulkanFeatures		features;
		map<string, string>	fragments;
		vector<string>		extensions;
		vector<deFloat16>	inputFP16;
		vector<deFloat16>	dummyFP16Output;

		// Generate values for input
		inputFP16.reserve(structItemsCount);
		for (deUint32 structItemNdx = 0; structItemNdx < structItemsCount; ++structItemNdx)
			inputFP16.push_back((accessPath[structItemNdx] == DE_NULL) ? exceptionValue : tcu::Float16(float(structItemNdx)).bits());

		dummyFP16Output.resize(structItemsCount);

		// Generate cases for OpSwitch
		{
			string	caseBodies;
			string	caseList;

			for (deUint32 caseNdx = 0; caseNdx < structItemsCount; ++caseNdx)
				if (accessPath[caseNdx] != DE_NULL)
				{
					map<string, string>	specCase;

					specCase["case_ndx"]		= de::toString(caseNdx);
					specCase["access_path"]		= accessPath[caseNdx];
					specCase["op_args_part"]	= opParts.argsPartial;
					specCase["op_name"]			= opName;

					caseBodies	+= testCaseBody.specialize(specCase);
					caseList	+= de::toString(caseNdx) + " %case_" + de::toString(caseNdx) + " ";
				}

			specs["case_bodies"]	= caseBodies;
			specs["case_list"]		= caseList;
		}

		specs["num_elements"]			= de::toString(structItemsCount);
		specs["field_type"]				= typeTestParameters[typeTestNdx].name;
		specs["struct_item_size"]		= de::toString(structItemsCount * sizeof(deFloat16));
		specs["struct_u32s"]			= de::toString(structItemsCount / 2);
		specs["op_premain_decls"]		= opParts.premainDecls;
		specs["op_sw_fun_call"]			= opParts.swFunCall;
		specs["op_sw_fun_header"]		= opParts.swFunHeader;
		specs["op_case_default_value"]	= opParts.caseDefaultValue;
		if (opIndex == 0) {
			specs["st_call"]			= "st_ssbo_dst";
			specs["st_ndx"]				= "c_i32_0";
		} else {
			specs["st_call"]			= "st_fn_ssbo_dst";
			specs["st_ndx"]				= "ndx";
		}

		fragments["capability"]		= "OpCapability Float16\n";
		fragments["decoration"]		= decoration.specialize(specs);
		fragments["pre_main"]		= preMain.specialize(specs);
		fragments["testfun"]		= testFun.specialize(specs);
		if (opIndex == 0) {
			fragments["testfun"]		+= StringTemplate(loadScalarF16FromUint).specialize({{"var", "ssbo_src"}});
			fragments["testfun"]		+= StringTemplate(typeTestParameters[typeTestNdx].loadFunction).specialize({{"var", "ssbo_dst"}});
			fragments["testfun"]		+= StringTemplate(typeTestParameters[typeTestNdx].storeFunction).specialize({{"var", "ssbo_dst"}});
		} else {
			fragments["testfun"]		+= StringTemplate(typeTestParameters[typeTestNdx].loadFunction).specialize({{"var", "ssbo_src"}});
			fragments["testfun"]		+= StringTemplate(storeScalarF16AsUint).specialize({{"var", "ssbo_dst"}});
		}

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(inputFP16)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(dummyFP16Output)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
		specResource.verifyIO = compareFP16CompositeFunc;

		extensions.push_back("VK_KHR_shader_float16_int8");

		features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

		finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
	}

	return testGroup.release();
}

struct fp16PerComponent
{
	fp16PerComponent()
		: flavor(0)
		, floatFormat16	(-14, 15, 10, true)
		, outCompCount(0)
		, argCompCount(3, 0)
	{
	}

	bool			callOncePerComponent	()									{ return true; }
	deUint32		getComponentValidity	()									{ return static_cast<deUint32>(-1); }

	virtual double	getULPs					(vector<const deFloat16*>&)			{ return 1.0; }
	virtual double	getMin					(double value, double ulps)			{ return value - floatFormat16.ulp(deAbs(value), ulps); }
	virtual double	getMax					(double value, double ulps)			{ return value + floatFormat16.ulp(deAbs(value), ulps); }

	virtual size_t	getFlavorCount			()									{ return flavorNames.empty() ? 1 : flavorNames.size(); }
	virtual void	setFlavor				(size_t flavorNo)					{ DE_ASSERT(flavorNo < getFlavorCount()); flavor = flavorNo; }
	virtual size_t	getFlavor				()									{ return flavor; }
	virtual string	getCurrentFlavorName	()									{ return flavorNames.empty() ? string("") : flavorNames[getFlavor()]; }

	virtual void	setOutCompCount			(size_t compCount)					{ outCompCount = compCount; }
	virtual size_t	getOutCompCount			()									{ return outCompCount; }

	virtual void	setArgCompCount			(size_t argNo, size_t compCount)	{ argCompCount[argNo] = compCount; }
	virtual size_t	getArgCompCount			(size_t argNo)						{ return argCompCount[argNo]; }

protected:
	size_t				flavor;
	tcu::FloatFormat	floatFormat16;
	size_t				outCompCount;
	vector<size_t>		argCompCount;
	vector<string>		flavorNames;
};

struct fp16OpFNegate : public fp16PerComponent
{
	template <class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(0.0 - d);

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Round : public fp16PerComponent
{
	fp16Round() : fp16PerComponent()
	{
		flavorNames.push_back("Floor(x+0.5)");
		flavorNames.push_back("Floor(x-0.5)");
		flavorNames.push_back("RoundEven");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			result	(0.0);

		switch (flavor)
		{
			case 0:		result = deRound(d);		break;
			case 1:		result = deFloor(d - 0.5);	break;
			case 2:		result = deRoundEven(d);	break;
			default:	TCU_THROW(InternalError, "Invalid flavor specified");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16RoundEven : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deRoundEven(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Trunc : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deTrunc(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FAbs : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deAbs(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FSign : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deSign(d));

		if (x.isNaN())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Floor : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deFloor(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Ceil : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deCeil(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Fract : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deFrac(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Radians : public fp16PerComponent
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2.5;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const float		d		(x.asFloat());
		const float		result	(deFloatRadians(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Degrees : public fp16PerComponent
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2.5;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const float		d		(x.asFloat());
		const float		result	(deFloatDegrees(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Sin : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x			(*in[0]);
		const double	d			(x.asDouble());
		const double	result		(deSin(d));
		const double	unspecUlp	(16.0);
		const double	err			(de::inRange(d, -DE_PI_DOUBLE, DE_PI_DOUBLE) ? deLdExp(1.0, -7) : floatFormat16.ulp(deAbs(result), unspecUlp));

		if (!de::inRange(d, -DE_PI_DOUBLE, DE_PI_DOUBLE))
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - err;
		max[0] = result + err;

		return true;
	}
};

struct fp16Cos : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x			(*in[0]);
		const double	d			(x.asDouble());
		const double	result		(deCos(d));
		const double	unspecUlp	(16.0);
		const double	err			(de::inRange(d, -DE_PI_DOUBLE, DE_PI_DOUBLE) ? deLdExp(1.0, -7) : floatFormat16.ulp(deAbs(result), unspecUlp));

		if (!de::inRange(d, -DE_PI_DOUBLE, DE_PI_DOUBLE))
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - err;
		max[0] = result + err;

		return true;
	}
};

struct fp16Tan : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deTan(d));

		if (!de::inRange(d, -DE_PI_DOUBLE, DE_PI_DOUBLE))
			return false;

		out[0] = fp16type(result).bits();
		{
			const double	err			= deLdExp(1.0, -7);
			const double	s1			= deSin(d) + err;
			const double	s2			= deSin(d) - err;
			const double	c1			= deCos(d) + err;
			const double	c2			= deCos(d) - err;
			const double	edgeVals[]	= {s1/c1, s1/c2, s2/c1, s2/c2};
			double			edgeLeft	= out[0];
			double			edgeRight	= out[0];

			if (deSign(c1 * c2) < 0.0)
			{
				edgeLeft	= -std::numeric_limits<double>::infinity();
				edgeRight	= +std::numeric_limits<double>::infinity();
			}
			else
			{
				edgeLeft	= *std::min_element(&edgeVals[0], &edgeVals[DE_LENGTH_OF_ARRAY(edgeVals)]);
				edgeRight	= *std::max_element(&edgeVals[0], &edgeVals[DE_LENGTH_OF_ARRAY(edgeVals)]);
			}

			min[0] = edgeLeft;
			max[0] = edgeRight;
		}

		return true;
	}
};

struct fp16Asin : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deAsin(d));
		const double	error	(deAtan2(d, sqrt(1.0 - d * d)));

		if (!x.isNaN() && deAbs(d) > 1.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - floatFormat16.ulp(deAbs(error), 2 * 5.0); // This is not a precision test. Value is not from spec
		max[0] = result + floatFormat16.ulp(deAbs(error), 2 * 5.0); // This is not a precision test. Value is not from spec

		return true;
	}
};

struct fp16Acos : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deAcos(d));
		const double	error	(deAtan2(sqrt(1.0 - d * d), d));

		if (!x.isNaN() && deAbs(d) > 1.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - floatFormat16.ulp(deAbs(error), 2 * 5.0); // This is not a precision test. Value is not from spec
		max[0] = result + floatFormat16.ulp(deAbs(error), 2 * 5.0); // This is not a precision test. Value is not from spec

		return true;
	}
};

struct fp16Atan : public fp16PerComponent
{
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2 * 5.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deAtanOver(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Sinh : public fp16PerComponent
{
	fp16Sinh() : fp16PerComponent()
	{
		flavorNames.push_back("Double");
		flavorNames.push_back("ExpFP16");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	ulps	(64 * (1.0 + 2 * deAbs(d))); // This is not a precision test. Value is not from spec
		double			result	(0.0);
		double			error	(0.0);

		if (getFlavor() == 0)
		{
			result	= deSinh(d);
			error	= floatFormat16.ulp(deAbs(result), ulps);
		}
		else if (getFlavor() == 1)
		{
			const fp16type	epx	(deExp(d));
			const fp16type	enx	(deExp(-d));
			const fp16type	esx	(epx.asDouble() - enx.asDouble());
			const fp16type	sx2	(esx.asDouble() / 2.0);

			result	= sx2.asDouble();
			error	= deAbs(floatFormat16.ulp(epx.asDouble(), ulps)) + deAbs(floatFormat16.ulp(enx.asDouble(), ulps));
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = result - error;
		max[0] = result + error;

		return true;
	}
};

struct fp16Cosh : public fp16PerComponent
{
	fp16Cosh() : fp16PerComponent()
	{
		flavorNames.push_back("Double");
		flavorNames.push_back("ExpFP16");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	ulps	(64 * (1.0 + 2 * deAbs(d))); // This is not a precision test. Value is not from spec
		double			result	(0.0);

		if (getFlavor() == 0)
		{
			result = deCosh(d);
		}
		else if (getFlavor() == 1)
		{
			const fp16type	epx	(deExp(d));
			const fp16type	enx	(deExp(-d));
			const fp16type	esx	(epx.asDouble() + enx.asDouble());
			const fp16type	sx2	(esx.asDouble() / 2.0);

			result = sx2.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = result - floatFormat16.ulp(deAbs(result), ulps);
		max[0] = result + floatFormat16.ulp(deAbs(result), ulps);

		return true;
	}
};

struct fp16Tanh : public fp16PerComponent
{
	fp16Tanh() : fp16PerComponent()
	{
		flavorNames.push_back("Tanh");
		flavorNames.push_back("SinhCosh");
		flavorNames.push_back("SinhCoshFP16");
		flavorNames.push_back("PolyFP16");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		const tcu::Float16	x	(*in[0]);
		const double		d	(x.asDouble());

		return 2 * (1.0 + 2 * deAbs(d)); // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	inline double calcPoly (const fp16type& espx, const fp16type& esnx, const fp16type& ecpx, const fp16type& ecnx)
	{
		const fp16type	esx	(espx.asDouble() - esnx.asDouble());
		const fp16type	sx2	(esx.asDouble() / 2.0);
		const fp16type	ecx	(ecpx.asDouble() + ecnx.asDouble());
		const fp16type	cx2	(ecx.asDouble() / 2.0);
		const fp16type	tg	(sx2.asDouble() / cx2.asDouble());
		const double	rez	(tg.asDouble());

		return rez;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			result	(0.0);

		if (getFlavor() == 0)
		{
			result	= deTanh(d);
			min[0]	= getMin(result, getULPs(in));
			max[0]	= getMax(result, getULPs(in));
		}
		else if (getFlavor() == 1)
		{
			result	= deSinh(d) / deCosh(d);
			min[0]	= getMin(result, getULPs(in));
			max[0]	= getMax(result, getULPs(in));
		}
		else if (getFlavor() == 2)
		{
			const fp16type	s	(deSinh(d));
			const fp16type	c	(deCosh(d));

			result	= s.asDouble() / c.asDouble();
			min[0]	= getMin(result, getULPs(in));
			max[0]	= getMax(result, getULPs(in));
		}
		else if (getFlavor() == 3)
		{
			const double	ulps	(getULPs(in));
			const double	epxm	(deExp( d));
			const double	enxm	(deExp(-d));
			const double	epxmerr	= floatFormat16.ulp(epxm, ulps);
			const double	enxmerr	= floatFormat16.ulp(enxm, ulps);
			const fp16type	epx[]	= { fp16type(epxm - epxmerr), fp16type(epxm + epxmerr) };
			const fp16type	enx[]	= { fp16type(enxm - enxmerr), fp16type(enxm + enxmerr) };
			const fp16type	epxm16	(epxm);
			const fp16type	enxm16	(enxm);
			vector<double>	tgs;

			for (size_t spNdx = 0; spNdx < DE_LENGTH_OF_ARRAY(epx); ++spNdx)
			for (size_t snNdx = 0; snNdx < DE_LENGTH_OF_ARRAY(enx); ++snNdx)
			for (size_t cpNdx = 0; cpNdx < DE_LENGTH_OF_ARRAY(epx); ++cpNdx)
			for (size_t cnNdx = 0; cnNdx < DE_LENGTH_OF_ARRAY(enx); ++cnNdx)
			{
				const double tgh = calcPoly(epx[spNdx], enx[snNdx], epx[cpNdx], enx[cnNdx]);

				tgs.push_back(tgh);
			}

			result = calcPoly(epxm16, enxm16, epxm16, enxm16);
			min[0] = *std::min_element(tgs.begin(), tgs.end());
			max[0] = *std::max_element(tgs.begin(), tgs.end());
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();

		return true;
	}
};

struct fp16Asinh : public fp16PerComponent
{
	fp16Asinh() : fp16PerComponent()
	{
		flavorNames.push_back("Double");
		flavorNames.push_back("PolyFP16Wiki");
		flavorNames.push_back("PolyFP16Abs");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 256.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			result	(0.0);

		if (getFlavor() == 0)
		{
			result = deAsinh(d);
		}
		else if (getFlavor() == 1)
		{
			const fp16type	x2		(d * d);
			const fp16type	x2p1	(x2.asDouble() + 1.0);
			const fp16type	sq		(deSqrt(x2p1.asDouble()));
			const fp16type	sxsq	(d + sq.asDouble());
			const fp16type	lsxsq	(deLog(sxsq.asDouble()));

			if (lsxsq.isInf())
				return false;

			result = lsxsq.asDouble();
		}
		else if (getFlavor() == 2)
		{
			const fp16type	x2		(d * d);
			const fp16type	x2p1	(x2.asDouble() + 1.0);
			const fp16type	sq		(deSqrt(x2p1.asDouble()));
			const fp16type	sxsq	(deAbs(d) + sq.asDouble());
			const fp16type	lsxsq	(deLog(sxsq.asDouble()));

			result = deSign(d) * lsxsq.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Acosh : public fp16PerComponent
{
	fp16Acosh() : fp16PerComponent()
	{
		flavorNames.push_back("Double");
		flavorNames.push_back("PolyFP16");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 16.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			result	(0.0);

		if (!x.isNaN() && d < 1.0)
			return false;

		if (getFlavor() == 0)
		{
			result = deAcosh(d);
		}
		else if (getFlavor() == 1)
		{
			const fp16type	x2		(d * d);
			const fp16type	x2m1	(x2.asDouble() - 1.0);
			const fp16type	sq		(deSqrt(x2m1.asDouble()));
			const fp16type	sxsq	(d + sq.asDouble());
			const fp16type	lsxsq	(deLog(sxsq.asDouble()));

			result = lsxsq.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Atanh : public fp16PerComponent
{
	fp16Atanh() : fp16PerComponent()
	{
		flavorNames.push_back("Double");
		flavorNames.push_back("PolyFP16");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			result	(0.0);

		if (deAbs(d) >= 1.0)
			return false;

		if (getFlavor() == 0)
		{
			const double	ulps	(16.0);	// This is not a precision test. Value is not from spec

			result = deAtanh(d);
			min[0] = getMin(result, ulps);
			max[0] = getMax(result, ulps);
		}
		else if (getFlavor() == 1)
		{
			const fp16type	x1a		(1.0 + d);
			const fp16type	x1b		(1.0 - d);
			const fp16type	x1d		(x1a.asDouble() / x1b.asDouble());
			const fp16type	lx1d	(deLog(x1d.asDouble()));
			const fp16type	lx1d2	(0.5 * lx1d.asDouble());
			const double	error	(2 * (de::inRange(deAbs(x1d.asDouble()), 0.5, 2.0) ? deLdExp(2.0, -7) : floatFormat16.ulp(deAbs(x1d.asDouble()), 3.0)));

			result = lx1d2.asDouble();
			min[0] = result - error;
			max[0] = result + error;
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();

		return true;
	}
};

struct fp16Exp : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	ulps	(10.0 * (1.0 + 2.0 * deAbs(d)));
		const double	result	(deExp(d));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, ulps);
		max[0] = getMax(result, ulps);

		return true;
	}
};

struct fp16Log : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deLog(d));
		const double	error	(de::inRange(deAbs(d), 0.5, 2.0) ? deLdExp(2.0, -7) : floatFormat16.ulp(deAbs(result), 3.0));

		if (d <= 0.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - error;
		max[0] = result + error;

		return true;
	}
};

struct fp16Exp2 : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deExp2(d));
		const double	ulps	(1.0 + 2.0 * deAbs(fp16type(in[0][0]).asDouble()));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, ulps);
		max[0] = getMax(result, ulps);

		return true;
	}
};

struct fp16Log2 : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deLog2(d));
		const double	error	(de::inRange(deAbs(d), 0.5, 2.0) ? deLdExp(2.0, -7) : floatFormat16.ulp(deAbs(result), 3.0));

		if (d <= 0.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = result - error;
		max[0] = result + error;

		return true;
	}
};

struct fp16Sqrt : public fp16PerComponent
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 6.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(deSqrt(d));

		if (!x.isNaN() && d < 0.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16InverseSqrt : public fp16PerComponent
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		const double	result	(1.0/deSqrt(d));

		if (!x.isNaN() && d <= 0.0)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16ModfFrac : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			i		(0.0);
		const double	result	(deModf(d, &i));

		if (x.isInf() || x.isNaN())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16ModfInt : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		double			i		(0.0);
		const double	dummy	(deModf(d, &i));
		const double	result	(i);

		DE_UNREF(dummy);

		if (x.isInf() || x.isNaN())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FrexpS : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		int				e		(0);
		const double	result	(deFrExp(d, &e));

		if (x.isNaN() || x.isInf())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FrexpE : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const double	d		(x.asDouble());
		int				e		(0);
		const double	dummy	(deFrExp(d, &e));
		const double	result	(static_cast<double>(e));

		DE_UNREF(dummy);

		if (x.isNaN() || x.isInf())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16OpFAdd : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	result	(xd + yd);

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16OpFSub : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	result	(xd - yd);

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16OpFMul : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	result	(xd * yd);

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16OpFDiv : public fp16PerComponent
{
	fp16OpFDiv() : fp16PerComponent()
	{
		flavorNames.push_back("DirectDiv");
		flavorNames.push_back("InverseDiv");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x			(*in[0]);
		const fp16type	y			(*in[1]);
		const double	xd			(x.asDouble());
		const double	yd			(y.asDouble());
		const double	unspecUlp	(16.0);
		const double	ulpCnt		(de::inRange(deAbs(yd), deLdExp(1, -14), deLdExp(1, 14)) ? 2.5 : unspecUlp);
		double			result		(0.0);

		if (y.isZero())
			return false;

		if (getFlavor() == 0)
		{
			result = (xd / yd);
		}
		else if (getFlavor() == 1)
		{
			const double	invyd	(1.0 / yd);
			const fp16type	invy	(invyd);

			result = (xd * invy.asDouble());
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, ulpCnt);
		max[0] = getMax(result, ulpCnt);

		return true;
	}
};

struct fp16Atan2 : public fp16PerComponent
{
	fp16Atan2() : fp16PerComponent()
	{
		flavorNames.push_back("DoubleCalc");
		flavorNames.push_back("DoubleCalc_PI");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2 * 5.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		double			result	(0.0);

		if (x.isZero() && y.isZero())
			return false;

		if (getFlavor() == 0)
		{
			result	= deAtan2(xd, yd);
		}
		else if (getFlavor() == 1)
		{
			const double	ulps	(2.0 * 5.0); // This is not a precision test. Value is not from spec
			const double	eps		(floatFormat16.ulp(DE_PI_DOUBLE, ulps));

			result	= deAtan2(xd, yd);

			if (de::inRange(deAbs(result), DE_PI_DOUBLE - eps, DE_PI_DOUBLE + eps))
				result	= -result;
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Pow : public fp16PerComponent
{
	fp16Pow() : fp16PerComponent()
	{
		flavorNames.push_back("Pow");
		flavorNames.push_back("PowLog2");
		flavorNames.push_back("PowLog2FP16");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	logxeps	(de::inRange(deAbs(xd), 0.5, 2.0) ? deLdExp(1.0, -7) : floatFormat16.ulp(deLog2(xd), 3.0));
		const double	ulps1	(1.0 + 4.0 * deAbs(yd * (deLog2(xd) - logxeps)));
		const double	ulps2	(1.0 + 4.0 * deAbs(yd * (deLog2(xd) + logxeps)));
		const double	ulps	(deMax(deAbs(ulps1), deAbs(ulps2)));
		double			result	(0.0);

		if (xd < 0.0)
			return false;

		if (x.isZero() && yd <= 0.0)
			return false;

		if (getFlavor() == 0)
		{
			result = dePow(xd, yd);
		}
		else if (getFlavor() == 1)
		{
			const double	l2d	(deLog2(xd));
			const double	e2d	(deExp2(yd * l2d));

			result = e2d;
		}
		else if (getFlavor() == 2)
		{
			const double	l2d	(deLog2(xd));
			const fp16type	l2	(l2d);
			const double	e2d	(deExp2(yd * l2.asDouble()));
			const fp16type	e2	(e2d);

			result = e2.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, ulps);
		max[0] = getMax(result, ulps);

		return true;
	}
};

struct fp16FMin : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	result	(deMin(xd, yd));

		if (x.isNaN() || y.isNaN())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FMax : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const double	yd		(y.asDouble());
		const double	result	(deMax(xd, yd));

		if (x.isNaN() || y.isNaN())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Step : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	edge	(*in[0]);
		const fp16type	x		(*in[1]);
		const double	edged	(edge.asDouble());
		const double	xd		(x.asDouble());
		const double	result	(deStep(edged, xd));

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Ldexp : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const double	xd		(x.asDouble());
		const int		yd		(static_cast<int>(deTrunc(y.asDouble())));
		const double	result	(deLdExp(xd, yd));

		if (y.isNaN() || y.isInf() || y.isDenorm() || yd < -14 || yd > 15)
			return false;

		// Spec: "If this product is too large to be represented in the floating-point type, the result is undefined."
		if (fp16type(result).isInf())
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FClamp : public fp16PerComponent
{
	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	minVal	(*in[1]);
		const fp16type	maxVal	(*in[2]);
		const double	xd		(x.asDouble());
		const double	minVald	(minVal.asDouble());
		const double	maxVald	(maxVal.asDouble());
		const double	result	(deClamp(xd, minVald, maxVald));

		if (minVal.isNaN() || maxVal.isNaN() || minVald > maxVald)
			return false;

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16FMix : public fp16PerComponent
{
	fp16FMix() : fp16PerComponent()
	{
		flavorNames.push_back("DoubleCalc");
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("EmulatingFP16YminusX");
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	x		(*in[0]);
		const fp16type	y		(*in[1]);
		const fp16type	a		(*in[2]);
		const double	ulps	(8.0); // This is not a precision test. Value is not from spec
		double			result	(0.0);

		if (getFlavor() == 0)
		{
			const double	xd		(x.asDouble());
			const double	yd		(y.asDouble());
			const double	ad		(a.asDouble());
			const double	xeps	(floatFormat16.ulp(deAbs(xd * (1.0 - ad)), ulps));
			const double	yeps	(floatFormat16.ulp(deAbs(yd * ad), ulps));
			const double	eps		(xeps + yeps);

			result = deMix(xd, yd, ad);
			min[0] = result - eps;
			max[0] = result + eps;
		}
		else if (getFlavor() == 1)
		{
			const double	xd		(x.asDouble());
			const double	yd		(y.asDouble());
			const double	ad		(a.asDouble());
			const fp16type	am		(1.0 - ad);
			const double	amd		(am.asDouble());
			const fp16type	xam		(xd * amd);
			const double	xamd	(xam.asDouble());
			const fp16type	ya		(yd * ad);
			const double	yad		(ya.asDouble());
			const double	xeps	(floatFormat16.ulp(deAbs(xd * (1.0 - ad)), ulps));
			const double	yeps	(floatFormat16.ulp(deAbs(yd * ad), ulps));
			const double	eps		(xeps + yeps);

			result = xamd + yad;
			min[0] = result - eps;
			max[0] = result + eps;
		}
		else if (getFlavor() == 2)
		{
			const double	xd		(x.asDouble());
			const double	yd		(y.asDouble());
			const double	ad		(a.asDouble());
			const fp16type	ymx		(yd - xd);
			const double	ymxd	(ymx.asDouble());
			const fp16type	ymxa	(ymxd * ad);
			const double	ymxad	(ymxa.asDouble());
			const double	xeps	(floatFormat16.ulp(deAbs(xd * (1.0 - ad)), ulps));
			const double	yeps	(floatFormat16.ulp(deAbs(yd * ad), ulps));
			const double	eps		(xeps + yeps);

			result = xd + ymxad;
			min[0] = result - eps;
			max[0] = result + eps;
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();

		return true;
	}
};

struct fp16SmoothStep : public fp16PerComponent
{
	fp16SmoothStep() : fp16PerComponent()
	{
		flavorNames.push_back("FloatCalc");
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("EmulatingFP16WClamp");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const fp16type	edge0	(*in[0]);
		const fp16type	edge1	(*in[1]);
		const fp16type	x		(*in[2]);
		double			result	(0.0);

		if (edge0.isNaN() || edge1.isNaN() || x.isNaN() || edge0.asDouble() >= edge1.asDouble())
			return false;

		if (edge0.isInf() || edge1.isInf() || x.isInf())
			return false;

		if (getFlavor() == 0)
		{
			const float	edge0d	(edge0.asFloat());
			const float	edge1d	(edge1.asFloat());
			const float	xd		(x.asFloat());
			const float	sstep	(deFloatSmoothStep(edge0d, edge1d, xd));

			result = sstep;
		}
		else if (getFlavor() == 1)
		{
			const double	edge0d	(edge0.asDouble());
			const double	edge1d	(edge1.asDouble());
			const double	xd		(x.asDouble());

			if (xd <= edge0d)
				result = 0.0;
			else if (xd >= edge1d)
				result = 1.0;
			else
			{
				const fp16type	a	(xd - edge0d);
				const fp16type	b	(edge1d - edge0d);
				const fp16type	t	(a.asDouble() / b.asDouble());
				const fp16type	t2	(2.0 * t.asDouble());
				const fp16type	t3	(3.0 - t2.asDouble());
				const fp16type	t4	(t.asDouble() * t3.asDouble());
				const fp16type	t5	(t.asDouble() * t4.asDouble());

				result = t5.asDouble();
			}
		}
		else if (getFlavor() == 2)
		{
			const double	edge0d	(edge0.asDouble());
			const double	edge1d	(edge1.asDouble());
			const double	xd		(x.asDouble());
			const fp16type	a	(xd - edge0d);
			const fp16type	b	(edge1d - edge0d);
			const fp16type	bi	(1.0 / b.asDouble());
			const fp16type	t0	(a.asDouble() * bi.asDouble());
			const double	tc	(deClamp(t0.asDouble(), 0.0, 1.0));
			const fp16type	t	(tc);
			const fp16type	t2	(2.0 * t.asDouble());
			const fp16type	t3	(3.0 - t2.asDouble());
			const fp16type	t4	(t.asDouble() * t3.asDouble());
			const fp16type	t5	(t.asDouble() * t4.asDouble());

			result = t5.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Fma : public fp16PerComponent
{
	fp16Fma()
	{
		flavorNames.push_back("DoubleCalc");
		flavorNames.push_back("EmulatingFP16");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 16.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 3);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());
		DE_ASSERT(getArgCompCount(1) == getOutCompCount());
		DE_ASSERT(getArgCompCount(2) == getOutCompCount());
		DE_ASSERT(getOutCompCount() > 0);

		const fp16type	a		(*in[0]);
		const fp16type	b		(*in[1]);
		const fp16type	c		(*in[2]);
		double			result	(0.0);

		if (getFlavor() == 0)
		{
			const double	ad	(a.asDouble());
			const double	bd	(b.asDouble());
			const double	cd	(c.asDouble());

			result	= deMadd(ad, bd, cd);
		}
		else if (getFlavor() == 1)
		{
			const double	ad	(a.asDouble());
			const double	bd	(b.asDouble());
			const double	cd	(c.asDouble());
			const fp16type	ab	(ad * bd);
			const fp16type	r	(ab.asDouble() + cd);

			result	= r.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};


struct fp16AllComponents : public fp16PerComponent
{
	bool		callOncePerComponent	()	{ return false; }
};

struct fp16Length : public fp16AllComponents
{
	fp16Length() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("DoubleCalc");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(getOutCompCount() == 1);
		DE_ASSERT(in.size() == 1);

		double	result	(0.0);

		if (getFlavor() == 0)
		{
			fp16type	r	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	q	(x.asDouble() * x.asDouble());

				r = fp16type(r.asDouble() + q.asDouble());
			}

			result = deSqrt(r.asDouble());

			out[0] = fp16type(result).bits();
		}
		else if (getFlavor() == 1)
		{
			double	r	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const double	q	(x.asDouble() * x.asDouble());

				r += q;
			}

			result = deSqrt(r);

			out[0] = fp16type(result).bits();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Distance : public fp16AllComponents
{
	fp16Distance() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("DoubleCalc");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(getOutCompCount() == 1);
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(0) == getArgCompCount(1));

		double	result	(0.0);

		if (getFlavor() == 0)
		{
			fp16type	r	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	y	(in[1][componentNdx]);
				const fp16type	d	(x.asDouble() - y.asDouble());
				const fp16type	q	(d.asDouble() * d.asDouble());

				r = fp16type(r.asDouble() + q.asDouble());
			}

			result = deSqrt(r.asDouble());
		}
		else if (getFlavor() == 1)
		{
			double	r	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	y	(in[1][componentNdx]);
				const double	d	(x.asDouble() - y.asDouble());
				const double	q	(d * d);

				r += q;
			}

			result = deSqrt(r);
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = getMin(result, getULPs(in));
		max[0] = getMax(result, getULPs(in));

		return true;
	}
};

struct fp16Cross : public fp16AllComponents
{
	fp16Cross() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("DoubleCalc");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(getOutCompCount() == 3);
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(0) == 3);
		DE_ASSERT(getArgCompCount(1) == 3);

		if (getFlavor() == 0)
		{
			const fp16type	x0		(in[0][0]);
			const fp16type	x1		(in[0][1]);
			const fp16type	x2		(in[0][2]);
			const fp16type	y0		(in[1][0]);
			const fp16type	y1		(in[1][1]);
			const fp16type	y2		(in[1][2]);
			const fp16type	x1y2	(x1.asDouble() * y2.asDouble());
			const fp16type	y1x2	(y1.asDouble() * x2.asDouble());
			const fp16type	x2y0	(x2.asDouble() * y0.asDouble());
			const fp16type	y2x0	(y2.asDouble() * x0.asDouble());
			const fp16type	x0y1	(x0.asDouble() * y1.asDouble());
			const fp16type	y0x1	(y0.asDouble() * x1.asDouble());

			out[0] = fp16type(x1y2.asDouble() - y1x2.asDouble()).bits();
			out[1] = fp16type(x2y0.asDouble() - y2x0.asDouble()).bits();
			out[2] = fp16type(x0y1.asDouble() - y0x1.asDouble()).bits();
		}
		else if (getFlavor() == 1)
		{
			const fp16type	x0		(in[0][0]);
			const fp16type	x1		(in[0][1]);
			const fp16type	x2		(in[0][2]);
			const fp16type	y0		(in[1][0]);
			const fp16type	y1		(in[1][1]);
			const fp16type	y2		(in[1][2]);
			const double	x1y2	(x1.asDouble() * y2.asDouble());
			const double	y1x2	(y1.asDouble() * x2.asDouble());
			const double	x2y0	(x2.asDouble() * y0.asDouble());
			const double	y2x0	(y2.asDouble() * x0.asDouble());
			const double	x0y1	(x0.asDouble() * y1.asDouble());
			const double	y0x1	(y0.asDouble() * x1.asDouble());

			out[0] = fp16type(x1y2 - y1x2).bits();
			out[1] = fp16type(x2y0 - y2x0).bits();
			out[2] = fp16type(x0y1 - y0x1).bits();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			min[ndx] = getMin(fp16type(out[ndx]).asDouble(), getULPs(in));
		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			max[ndx] = getMax(fp16type(out[ndx]).asDouble(), getULPs(in));

		return true;
	}
};

struct fp16Normalize : public fp16AllComponents
{
	fp16Normalize() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("DoubleCalc");

		// flavorNames will be extended later
	}

	virtual void	setArgCompCount			(size_t argNo, size_t compCount)
	{
		DE_ASSERT(argCompCount[argNo] == 0); // Once only

		if (argNo == 0 && argCompCount[argNo] == 0)
		{
			const size_t		maxPermutationsCount	= 24u; // Equal to 4!
			std::vector<int>	indices;

			for (size_t componentNdx = 0; componentNdx < compCount; ++componentNdx)
				indices.push_back(static_cast<int>(componentNdx));

			m_permutations.reserve(maxPermutationsCount);

			permutationsFlavorStart = flavorNames.size();

			do
			{
				tcu::UVec4	permutation;
				std::string	name		= "Permutted_";

				for (size_t componentNdx = 0; componentNdx < compCount; ++componentNdx)
				{
					permutation[static_cast<int>(componentNdx)] = indices[componentNdx];
					name += de::toString(indices[componentNdx]);
				}

				m_permutations.push_back(permutation);
				flavorNames.push_back(name);

			} while(std::next_permutation(indices.begin(), indices.end()));

			permutationsFlavorEnd = flavorNames.size();
		}

		fp16AllComponents::setArgCompCount(argNo, compCount);
	}
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 8.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 1);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());

		if (getFlavor() == 0)
		{
			fp16type	r(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	q	(x.asDouble() * x.asDouble());

				r = fp16type(r.asDouble() + q.asDouble());
			}

			r = fp16type(deSqrt(r.asDouble()));

			if (r.isZero())
				return false;

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);

				out[componentNdx] = fp16type(x.asDouble() / r.asDouble()).bits();
			}
		}
		else if (getFlavor() == 1)
		{
			double	r(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const double	q	(x.asDouble() * x.asDouble());

				r += q;
			}

			r = deSqrt(r);

			if (r == 0)
				return false;

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);

				out[componentNdx] = fp16type(x.asDouble() / r).bits();
			}
		}
		else if (de::inBounds<size_t>(getFlavor(), permutationsFlavorStart, permutationsFlavorEnd))
		{
			const int			compCount		(static_cast<int>(getArgCompCount(0)));
			const size_t		permutationNdx	(getFlavor() - permutationsFlavorStart);
			const tcu::UVec4&	permutation		(m_permutations[permutationNdx]);
			fp16type			r				(0.0);

			for (int permComponentNdx = 0; permComponentNdx < compCount; ++permComponentNdx)
			{
				const size_t	componentNdx	(permutation[permComponentNdx]);
				const fp16type	x				(in[0][componentNdx]);
				const fp16type	q				(x.asDouble() * x.asDouble());

				r = fp16type(r.asDouble() + q.asDouble());
			}

			r = fp16type(deSqrt(r.asDouble()));

			if (r.isZero())
				return false;

			for (int permComponentNdx = 0; permComponentNdx < compCount; ++permComponentNdx)
			{
				const size_t	componentNdx	(permutation[permComponentNdx]);
				const fp16type	x				(in[0][componentNdx]);

				out[componentNdx] = fp16type(x.asDouble() / r.asDouble()).bits();
			}
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			min[ndx] = getMin(fp16type(out[ndx]).asDouble(), getULPs(in));
		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			max[ndx] = getMax(fp16type(out[ndx]).asDouble(), getULPs(in));

		return true;
	}

private:
	std::vector<tcu::UVec4> m_permutations;
	size_t					permutationsFlavorStart;
	size_t					permutationsFlavorEnd;
};

struct fp16FaceForward : public fp16AllComponents
{
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 3);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());
		DE_ASSERT(getArgCompCount(1) == getOutCompCount());
		DE_ASSERT(getArgCompCount(2) == getOutCompCount());

		fp16type	dp(0.0);

		for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
		{
			const fp16type	x	(in[1][componentNdx]);
			const fp16type	y	(in[2][componentNdx]);
			const double	xd	(x.asDouble());
			const double	yd	(y.asDouble());
			const fp16type	q	(xd * yd);

			dp = fp16type(dp.asDouble() + q.asDouble());
		}

		if (dp.isNaN() || dp.isZero())
			return false;

		for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
		{
			const fp16type	n	(in[0][componentNdx]);

			out[componentNdx] = (dp.signBit() == 1) ? n.bits() : fp16type(-n.asDouble()).bits();
		}

		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			min[ndx] = getMin(fp16type(out[ndx]).asDouble(), getULPs(in));
		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			max[ndx] = getMax(fp16type(out[ndx]).asDouble(), getULPs(in));

		return true;
	}
};

struct fp16Reflect : public fp16AllComponents
{
	fp16Reflect() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("EmulatingFP16+KeepZeroSign");
		flavorNames.push_back("FloatCalc");
		flavorNames.push_back("FloatCalc+KeepZeroSign");
		flavorNames.push_back("EmulatingFP16+2Nfirst");
		flavorNames.push_back("EmulatingFP16+2Ifirst");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 256.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());
		DE_ASSERT(getArgCompCount(1) == getOutCompCount());

		if (getFlavor() < 4)
		{
			const bool	keepZeroSign	((flavor & 1) != 0 ? true : false);
			const bool	floatCalc		((flavor & 2) != 0 ? true : false);

			if (floatCalc)
			{
				float	dp(0.0f);

				for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
				{
					const fp16type	i	(in[0][componentNdx]);
					const fp16type	n	(in[1][componentNdx]);
					const float		id	(i.asFloat());
					const float		nd	(n.asFloat());
					const float		qd	(id * nd);

					if (keepZeroSign)
						dp = (componentNdx == 0) ? qd : dp + qd;
					else
						dp = dp + qd;
				}

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
				{
					const fp16type	i		(in[0][componentNdx]);
					const fp16type	n		(in[1][componentNdx]);
					const float		dpnd	(dp * n.asFloat());
					const float		dpn2d	(2.0f * dpnd);
					const float		idpn2d	(i.asFloat() - dpn2d);
					const fp16type	result	(idpn2d);

					out[componentNdx] = result.bits();
				}
			}
			else
			{
				fp16type	dp(0.0);

				for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
				{
					const fp16type	i	(in[0][componentNdx]);
					const fp16type	n	(in[1][componentNdx]);
					const double	id	(i.asDouble());
					const double	nd	(n.asDouble());
					const fp16type	q	(id * nd);

					if (keepZeroSign)
						dp = (componentNdx == 0) ? q : fp16type(dp.asDouble() + q.asDouble());
					else
						dp = fp16type(dp.asDouble() + q.asDouble());
				}

				if (dp.isNaN())
					return false;

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
				{
					const fp16type	i		(in[0][componentNdx]);
					const fp16type	n		(in[1][componentNdx]);
					const fp16type	dpn		(dp.asDouble() * n.asDouble());
					const fp16type	dpn2	(2 * dpn.asDouble());
					const fp16type	idpn2	(i.asDouble() - dpn2.asDouble());

					out[componentNdx] = idpn2.bits();
				}
			}
		}
		else if (getFlavor() == 4)
		{
			fp16type	dp(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	i	(in[0][componentNdx]);
				const fp16type	n	(in[1][componentNdx]);
				const double	id	(i.asDouble());
				const double	nd	(n.asDouble());
				const fp16type	q	(id * nd);

				dp = fp16type(dp.asDouble() + q.asDouble());
			}

			if (dp.isNaN())
				return false;

			for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
			{
				const fp16type	i		(in[0][componentNdx]);
				const fp16type	n		(in[1][componentNdx]);
				const fp16type	n2		(2 * n.asDouble());
				const fp16type	dpn2	(dp.asDouble() * n2.asDouble());
				const fp16type	idpn2	(i.asDouble() - dpn2.asDouble());

				out[componentNdx] = idpn2.bits();
			}
		}
		else if (getFlavor() == 5)
		{
			fp16type	dp2(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	i	(in[0][componentNdx]);
				const fp16type	n	(in[1][componentNdx]);
				const fp16type	i2	(2.0 * i.asDouble());
				const double	i2d	(i2.asDouble());
				const double	nd	(n.asDouble());
				const fp16type	q	(i2d * nd);

				dp2 = fp16type(dp2.asDouble() + q.asDouble());
			}

			if (dp2.isNaN())
				return false;

			for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
			{
				const fp16type	i		(in[0][componentNdx]);
				const fp16type	n		(in[1][componentNdx]);
				const fp16type	dpn2	(dp2.asDouble() * n.asDouble());
				const fp16type	idpn2	(i.asDouble() - dpn2.asDouble());

				out[componentNdx] = idpn2.bits();
			}
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			min[ndx] = getMin(fp16type(out[ndx]).asDouble(), getULPs(in));
		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			max[ndx] = getMax(fp16type(out[ndx]).asDouble(), getULPs(in));

		return true;
	}
};

struct fp16Refract : public fp16AllComponents
{
	fp16Refract() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("EmulatingFP16+KeepZeroSign");
		flavorNames.push_back("FloatCalc");
		flavorNames.push_back("FloatCalc+KeepZeroSign");
	}

	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 8192.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 3);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());
		DE_ASSERT(getArgCompCount(1) == getOutCompCount());
		DE_ASSERT(getArgCompCount(2) == 1);

		const bool		keepZeroSign	((flavor & 1) != 0 ? true : false);
		const bool		doubleCalc		((flavor & 2) != 0 ? true : false);
		const fp16type	eta				(*in[2]);

		if (doubleCalc)
		{
			double	dp	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	i	(in[0][componentNdx]);
				const fp16type	n	(in[1][componentNdx]);
				const double	id	(i.asDouble());
				const double	nd	(n.asDouble());
				const double	qd	(id * nd);

				if (keepZeroSign)
					dp = (componentNdx == 0) ? qd : dp + qd;
				else
					dp = dp + qd;
			}

			const double	eta2	(eta.asDouble() * eta.asDouble());
			const double	dp2		(dp * dp);
			const double	dp1		(1.0 - dp2);
			const double	dpe		(eta2 * dp1);
			const double	k		(1.0 - dpe);

			if (k < 0.0)
			{
				const fp16type	zero	(0.0);

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
					out[componentNdx] = zero.bits();
			}
			else
			{
				const double	sk	(deSqrt(k));

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
				{
					const fp16type	i		(in[0][componentNdx]);
					const fp16type	n		(in[1][componentNdx]);
					const double	etai	(i.asDouble() * eta.asDouble());
					const double	etadp	(eta.asDouble() * dp);
					const double	etadpk	(etadp + sk);
					const double	etadpkn	(etadpk * n.asDouble());
					const double	full	(etai - etadpkn);
					const fp16type	result	(full);

					if (result.isInf())
						return false;

					out[componentNdx] = result.bits();
				}
			}
		}
		else
		{
			fp16type	dp	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	i	(in[0][componentNdx]);
				const fp16type	n	(in[1][componentNdx]);
				const double	id	(i.asDouble());
				const double	nd	(n.asDouble());
				const fp16type	q	(id * nd);

				if (keepZeroSign)
					dp = (componentNdx == 0) ? q : fp16type(dp.asDouble() + q.asDouble());
				else
					dp = fp16type(dp.asDouble() + q.asDouble());
			}

			if (dp.isNaN())
				return false;

			const fp16type	eta2(eta.asDouble() * eta.asDouble());
			const fp16type	dp2	(dp.asDouble() * dp.asDouble());
			const fp16type	dp1	(1.0 - dp2.asDouble());
			const fp16type	dpe	(eta2.asDouble() * dp1.asDouble());
			const fp16type	k	(1.0 - dpe.asDouble());

			if (k.asDouble() < 0.0)
			{
				const fp16type	zero	(0.0);

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
					out[componentNdx] = zero.bits();
			}
			else
			{
				const fp16type	sk	(deSqrt(k.asDouble()));

				for (size_t componentNdx = 0; componentNdx < getOutCompCount(); ++componentNdx)
				{
					const fp16type	i		(in[0][componentNdx]);
					const fp16type	n		(in[1][componentNdx]);
					const fp16type	etai	(i.asDouble() * eta.asDouble());
					const fp16type	etadp	(eta.asDouble() * dp.asDouble());
					const fp16type	etadpk	(etadp.asDouble() + sk.asDouble());
					const fp16type	etadpkn	(etadpk.asDouble() * n.asDouble());
					const fp16type	full	(etai.asDouble() - etadpkn.asDouble());

					if (full.isNaN() || full.isInf())
						return false;

					out[componentNdx] = full.bits();
				}
			}
		}

		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			min[ndx] = getMin(fp16type(out[ndx]).asDouble(), getULPs(in));
		for (size_t ndx = 0; ndx < getOutCompCount(); ++ndx)
			max[ndx] = getMax(fp16type(out[ndx]).asDouble(), getULPs(in));

		return true;
	}
};

struct fp16Dot : public fp16AllComponents
{
	fp16Dot() : fp16AllComponents()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("FloatCalc");
		flavorNames.push_back("DoubleCalc");

		// flavorNames will be extended later
	}

	virtual void	setArgCompCount			(size_t argNo, size_t compCount)
	{
		DE_ASSERT(argCompCount[argNo] == 0); // Once only

		if (argNo == 0 && argCompCount[argNo] == 0)
		{
			const size_t		maxPermutationsCount	= 24u; // Equal to 4!
			std::vector<int>	indices;

			for (size_t componentNdx = 0; componentNdx < compCount; ++componentNdx)
				indices.push_back(static_cast<int>(componentNdx));

			m_permutations.reserve(maxPermutationsCount);

			permutationsFlavorStart = flavorNames.size();

			do
			{
				tcu::UVec4	permutation;
				std::string	name		= "Permutted_";

				for (size_t componentNdx = 0; componentNdx < compCount; ++componentNdx)
				{
					permutation[static_cast<int>(componentNdx)] = indices[componentNdx];
					name += de::toString(indices[componentNdx]);
				}

				m_permutations.push_back(permutation);
				flavorNames.push_back(name);

			} while(std::next_permutation(indices.begin(), indices.end()));

			permutationsFlavorEnd = flavorNames.size();
		}

		fp16AllComponents::setArgCompCount(argNo, compCount);
	}

	virtual double	getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 16.0; // This is not a precision test. Value is not from spec
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(0) == getArgCompCount(1));
		DE_ASSERT(getOutCompCount() == 1);

		double	result	(0.0);
		double	eps		(0.0);

		if (getFlavor() == 0)
		{
			fp16type	dp	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	y	(in[1][componentNdx]);
				const fp16type	q	(x.asDouble() * y.asDouble());

				dp = fp16type(dp.asDouble() + q.asDouble());
				eps += floatFormat16.ulp(q.asDouble(), 2.0);
			}

			result = dp.asDouble();
		}
		else if (getFlavor() == 1)
		{
			float	dp	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	y	(in[1][componentNdx]);
				const float		q	(x.asFloat() * y.asFloat());

				dp += q;
				eps += floatFormat16.ulp(static_cast<double>(q), 2.0);
			}

			result = dp;
		}
		else if (getFlavor() == 2)
		{
			double	dp	(0.0);

			for (size_t componentNdx = 0; componentNdx < getArgCompCount(1); ++componentNdx)
			{
				const fp16type	x	(in[0][componentNdx]);
				const fp16type	y	(in[1][componentNdx]);
				const double	q	(x.asDouble() * y.asDouble());

				dp += q;
				eps += floatFormat16.ulp(q, 2.0);
			}

			result = dp;
		}
		else if (de::inBounds<size_t>(getFlavor(), permutationsFlavorStart, permutationsFlavorEnd))
		{
			const int			compCount		(static_cast<int>(getArgCompCount(1)));
			const size_t		permutationNdx	(getFlavor() - permutationsFlavorStart);
			const tcu::UVec4&	permutation		(m_permutations[permutationNdx]);
			fp16type			dp				(0.0);

			for (int permComponentNdx = 0; permComponentNdx < compCount; ++permComponentNdx)
			{
				const size_t		componentNdx	(permutation[permComponentNdx]);
				const fp16type		x				(in[0][componentNdx]);
				const fp16type		y				(in[1][componentNdx]);
				const fp16type		q				(x.asDouble() * y.asDouble());

				dp = fp16type(dp.asDouble() + q.asDouble());
				eps += floatFormat16.ulp(q.asDouble(), 2.0);
			}

			result = dp.asDouble();
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		out[0] = fp16type(result).bits();
		min[0] = result - eps;
		max[0] = result + eps;

		return true;
	}

private:
	std::vector<tcu::UVec4> m_permutations;
	size_t					permutationsFlavorStart;
	size_t					permutationsFlavorEnd;
};

struct fp16VectorTimesScalar : public fp16AllComponents
{
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2.0;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(0) == getOutCompCount());
		DE_ASSERT(getArgCompCount(1) == 1);

		fp16type	s	(*in[1]);

		for (size_t componentNdx = 0; componentNdx < getArgCompCount(0); ++componentNdx)
		{
			const fp16type	x	   (in[0][componentNdx]);
			const double    result (s.asDouble() * x.asDouble());
			const fp16type	m	   (result);

			out[componentNdx] = m.bits();
			min[componentNdx] = getMin(result, getULPs(in));
			max[componentNdx] = getMax(result, getULPs(in));
		}

		return true;
	}
};

struct fp16MatrixBase : public fp16AllComponents
{
	deUint32		getComponentValidity			()
	{
		return static_cast<deUint32>(-1);
	}

	inline size_t	getNdx							(const size_t rowCount, const size_t col, const size_t row)
	{
		const size_t minComponentCount	= 0;
		const size_t maxComponentCount	= 3;
		const size_t alignedRowsCount	= (rowCount == 3) ? 4 : rowCount;

		DE_ASSERT(de::inRange(rowCount, minComponentCount + 1, maxComponentCount + 1));
		DE_ASSERT(de::inRange(col, minComponentCount, maxComponentCount));
		DE_ASSERT(de::inBounds(row, minComponentCount, rowCount));
		DE_UNREF(minComponentCount);
		DE_UNREF(maxComponentCount);

		return col * alignedRowsCount + row;
	}

	deUint32		getComponentMatrixValidityMask	(size_t cols, size_t rows)
	{
		deUint32	result	= 0u;

		for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				const size_t bitNdx = getNdx(rows, colNdx, rowNdx);

				DE_ASSERT(bitNdx < sizeof(result) * 8);

				result |= (1<<bitNdx);
			}

		return result;
	}
};

template<size_t cols, size_t rows>
struct fp16Transpose : public fp16MatrixBase
{
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 1.0;
	}

	deUint32	getComponentValidity	()
	{
		return getComponentMatrixValidityMask(rows, cols);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 1);

		const size_t		alignedCols	= (cols == 3) ? 4 : cols;
		const size_t		alignedRows	= (rows == 3) ? 4 : rows;
		vector<deFloat16>	output		(alignedCols * alignedRows, 0);

		DE_ASSERT(output.size() == alignedCols * alignedRows);

		for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
				output[rowNdx * alignedCols + colNdx] = in[0][colNdx * alignedRows + rowNdx];

		deMemcpy(out, &output[0], sizeof(deFloat16) * output.size());
		deMemcpy(min, &output[0], sizeof(deFloat16) * output.size());
		deMemcpy(max, &output[0], sizeof(deFloat16) * output.size());

		return true;
	}
};

template<size_t cols, size_t rows>
struct fp16MatrixTimesScalar : public fp16MatrixBase
{
	virtual double getULPs(vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 4.0;
	}

	deUint32	getComponentValidity	()
	{
		return getComponentMatrixValidityMask(cols, rows);
	}

	template<class fp16type>
	bool calc(vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);
		DE_ASSERT(getArgCompCount(1) == 1);

		const fp16type	y			(in[1][0]);
		const float		scalar		(y.asFloat());
		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(getArgCompCount(0) == alignedCols * alignedRows);
		DE_ASSERT(getOutCompCount() == alignedCols * alignedRows);
		DE_UNREF(alignedCols);

		for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				const size_t	ndx	(colNdx * alignedRows + rowNdx);
				const fp16type	x	(in[0][ndx]);
				const double	result	(scalar * x.asFloat());

				out[ndx] = fp16type(result).bits();
				min[ndx] = getMin(result, getULPs(in));
				max[ndx] = getMax(result, getULPs(in));
			}

		return true;
	}
};

template<size_t cols, size_t rows>
struct fp16VectorTimesMatrix : public fp16MatrixBase
{
	fp16VectorTimesMatrix() : fp16MatrixBase()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("FloatCalc");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return (8.0 * cols);
	}

	deUint32 getComponentValidity ()
	{
		return getComponentMatrixValidityMask(cols, 1);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);

		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(getOutCompCount() == cols);
		DE_ASSERT(getArgCompCount(0) == rows);
		DE_ASSERT(getArgCompCount(1) == alignedCols * alignedRows);
		DE_UNREF(alignedCols);

		if (getFlavor() == 0)
		{
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				fp16type	s	(fp16type::zero(1));

				for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
				{
					const fp16type	v	(in[0][rowNdx]);
					const float		vf	(v.asFloat());
					const size_t	ndx	(colNdx * alignedRows + rowNdx);
					const fp16type	x	(in[1][ndx]);
					const float		xf	(x.asFloat());
					const fp16type	m	(vf * xf);

					s = fp16type(s.asFloat() + m.asFloat());
				}

				out[colNdx] = s.bits();
				min[colNdx] = getMin(s.asDouble(), getULPs(in));
				max[colNdx] = getMax(s.asDouble(), getULPs(in));
			}
		}
		else if (getFlavor() == 1)
		{
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				float	s	(0.0f);

				for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
				{
					const fp16type	v	(in[0][rowNdx]);
					const float		vf	(v.asFloat());
					const size_t	ndx	(colNdx * alignedRows + rowNdx);
					const fp16type	x	(in[1][ndx]);
					const float		xf	(x.asFloat());
					const float		m	(vf * xf);

					s += m;
				}

				out[colNdx] = fp16type(s).bits();
				min[colNdx] = getMin(static_cast<double>(s), getULPs(in));
				max[colNdx] = getMax(static_cast<double>(s), getULPs(in));
			}
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		return true;
	}
};

template<size_t cols, size_t rows>
struct fp16MatrixTimesVector : public fp16MatrixBase
{
	fp16MatrixTimesVector() : fp16MatrixBase()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("FloatCalc");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return (8.0 * rows);
	}

	deUint32 getComponentValidity ()
	{
		return getComponentMatrixValidityMask(rows, 1);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);

		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(getOutCompCount() == rows);
		DE_ASSERT(getArgCompCount(0) == alignedCols * alignedRows);
		DE_ASSERT(getArgCompCount(1) == cols);
		DE_UNREF(alignedCols);

		if (getFlavor() == 0)
		{
			for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			{
				fp16type	s	(fp16type::zero(1));

				for (size_t colNdx = 0; colNdx < cols; ++colNdx)
				{
					const size_t	ndx	(colNdx * alignedRows + rowNdx);
					const fp16type	x	(in[0][ndx]);
					const float		xf	(x.asFloat());
					const fp16type	v	(in[1][colNdx]);
					const float		vf	(v.asFloat());
					const fp16type	m	(vf * xf);

					s = fp16type(s.asFloat() + m.asFloat());
				}

				out[rowNdx] = s.bits();
				min[rowNdx] = getMin(s.asDouble(), getULPs(in));
				max[rowNdx] = getMax(s.asDouble(), getULPs(in));
			}
		}
		else if (getFlavor() == 1)
		{
			for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			{
				float	s	(0.0f);

				for (size_t colNdx = 0; colNdx < cols; ++colNdx)
				{
					const size_t	ndx	(colNdx * alignedRows + rowNdx);
					const fp16type	x	(in[0][ndx]);
					const float		xf	(x.asFloat());
					const fp16type	v	(in[1][colNdx]);
					const float		vf	(v.asFloat());
					const float		m	(vf * xf);

					s += m;
				}

				out[rowNdx] = fp16type(s).bits();
				min[rowNdx] = getMin(static_cast<double>(s), getULPs(in));
				max[rowNdx] = getMax(static_cast<double>(s), getULPs(in));
			}
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		return true;
	}
};

template<size_t colsL, size_t rowsL, size_t colsR, size_t rowsR>
struct fp16MatrixTimesMatrix : public fp16MatrixBase
{
	fp16MatrixTimesMatrix() : fp16MatrixBase()
	{
		flavorNames.push_back("EmulatingFP16");
		flavorNames.push_back("FloatCalc");
	}

	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 32.0;
	}

	deUint32 getComponentValidity ()
	{
		return getComponentMatrixValidityMask(colsR, rowsL);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_STATIC_ASSERT(colsL == rowsR);

		DE_ASSERT(in.size() == 2);

		const size_t	alignedColsL	= (colsL == 3) ? 4 : colsL;
		const size_t	alignedRowsL	= (rowsL == 3) ? 4 : rowsL;
		const size_t	alignedColsR	= (colsR == 3) ? 4 : colsR;
		const size_t	alignedRowsR	= (rowsR == 3) ? 4 : rowsR;

		DE_ASSERT(getOutCompCount() == alignedColsR * alignedRowsL);
		DE_ASSERT(getArgCompCount(0) == alignedColsL * alignedRowsL);
		DE_ASSERT(getArgCompCount(1) == alignedColsR * alignedRowsR);
		DE_UNREF(alignedColsL);
		DE_UNREF(alignedColsR);

		if (getFlavor() == 0)
		{
			for (size_t rowNdx = 0; rowNdx < rowsL; ++rowNdx)
			{
				for (size_t colNdx = 0; colNdx < colsR; ++colNdx)
				{
					const size_t	ndx	(colNdx * alignedRowsL + rowNdx);
					fp16type		s	(fp16type::zero(1));

					for (size_t commonNdx = 0; commonNdx < colsL; ++commonNdx)
					{
						const size_t	ndxl	(commonNdx * alignedRowsL + rowNdx);
						const fp16type	l		(in[0][ndxl]);
						const float		lf		(l.asFloat());
						const size_t	ndxr	(colNdx * alignedRowsR + commonNdx);
						const fp16type	r		(in[1][ndxr]);
						const float		rf		(r.asFloat());
						const fp16type	m		(lf * rf);

						s = fp16type(s.asFloat() + m.asFloat());
					}

					out[ndx] = s.bits();
					min[ndx] = getMin(s.asDouble(), getULPs(in));
					max[ndx] = getMax(s.asDouble(), getULPs(in));
				}
			}
		}
		else if (getFlavor() == 1)
		{
			for (size_t rowNdx = 0; rowNdx < rowsL; ++rowNdx)
			{
				for (size_t colNdx = 0; colNdx < colsR; ++colNdx)
				{
					const size_t	ndx	(colNdx * alignedRowsL + rowNdx);
					float			s	(0.0f);

					for (size_t commonNdx = 0; commonNdx < colsL; ++commonNdx)
					{
						const size_t	ndxl	(commonNdx * alignedRowsL + rowNdx);
						const fp16type	l		(in[0][ndxl]);
						const float		lf		(l.asFloat());
						const size_t	ndxr	(colNdx * alignedRowsR + commonNdx);
						const fp16type	r		(in[1][ndxr]);
						const float		rf		(r.asFloat());
						const float		m		(lf * rf);

						s += m;
					}

					out[ndx] = fp16type(s).bits();
					min[ndx] = getMin(static_cast<double>(s), getULPs(in));
					max[ndx] = getMax(static_cast<double>(s), getULPs(in));
				}
			}
		}
		else
		{
			TCU_THROW(InternalError, "Unknown flavor");
		}

		return true;
	}
};

template<size_t cols, size_t rows>
struct fp16OuterProduct : public fp16MatrixBase
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 2.0;
	}

	deUint32 getComponentValidity ()
	{
		return getComponentMatrixValidityMask(cols, rows);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		DE_ASSERT(in.size() == 2);

		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(getArgCompCount(0) == rows);
		DE_ASSERT(getArgCompCount(1) == cols);
		DE_ASSERT(getOutCompCount() == alignedCols * alignedRows);
		DE_UNREF(alignedCols);

		for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
		{
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				const size_t	ndx	(colNdx * alignedRows + rowNdx);
				const fp16type	x	(in[0][rowNdx]);
				const float		xf	(x.asFloat());
				const fp16type	y	(in[1][colNdx]);
				const float		yf	(y.asFloat());
				const fp16type	m	(xf * yf);

				out[ndx] = m.bits();
				min[ndx] = getMin(m.asDouble(), getULPs(in));
				max[ndx] = getMax(m.asDouble(), getULPs(in));
			}
		}

		return true;
	}
};

template<size_t size>
struct fp16Determinant;

template<>
struct fp16Determinant<2> : public fp16MatrixBase
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 128.0; // This is not a precision test. Value is not from spec
	}

	deUint32 getComponentValidity ()
	{
		return 1;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const size_t	cols		= 2;
		const size_t	rows		= 2;
		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(in.size() == 1);
		DE_ASSERT(getOutCompCount() == 1);
		DE_ASSERT(getArgCompCount(0) == alignedRows * alignedCols);
		DE_UNREF(alignedCols);
		DE_UNREF(alignedRows);

		// [ a b ]
		// [ c d ]
		const float		a		(fp16type(in[0][getNdx(rows, 0, 0)]).asFloat());
		const float		b		(fp16type(in[0][getNdx(rows, 1, 0)]).asFloat());
		const float		c		(fp16type(in[0][getNdx(rows, 0, 1)]).asFloat());
		const float		d		(fp16type(in[0][getNdx(rows, 1, 1)]).asFloat());
		const float		ad		(a * d);
		const fp16type	adf16	(ad);
		const float		bc		(b * c);
		const fp16type	bcf16	(bc);
		const float		r		(adf16.asFloat() - bcf16.asFloat());
		const fp16type	rf16	(r);

		out[0] = rf16.bits();
		min[0] = getMin(r, getULPs(in));
		max[0] = getMax(r, getULPs(in));

		return true;
	}
};

template<>
struct fp16Determinant<3> : public fp16MatrixBase
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 128.0; // This is not a precision test. Value is not from spec
	}

	deUint32 getComponentValidity ()
	{
		return 1;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const size_t	cols		= 3;
		const size_t	rows		= 3;
		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(in.size() == 1);
		DE_ASSERT(getOutCompCount() == 1);
		DE_ASSERT(getArgCompCount(0) == alignedRows * alignedCols);
		DE_UNREF(alignedCols);
		DE_UNREF(alignedRows);

		// [ a b c ]
		// [ d e f ]
		// [ g h i ]
		const float		a		(fp16type(in[0][getNdx(rows, 0, 0)]).asFloat());
		const float		b		(fp16type(in[0][getNdx(rows, 1, 0)]).asFloat());
		const float		c		(fp16type(in[0][getNdx(rows, 2, 0)]).asFloat());
		const float		d		(fp16type(in[0][getNdx(rows, 0, 1)]).asFloat());
		const float		e		(fp16type(in[0][getNdx(rows, 1, 1)]).asFloat());
		const float		f		(fp16type(in[0][getNdx(rows, 2, 1)]).asFloat());
		const float		g		(fp16type(in[0][getNdx(rows, 0, 2)]).asFloat());
		const float		h		(fp16type(in[0][getNdx(rows, 1, 2)]).asFloat());
		const float		i		(fp16type(in[0][getNdx(rows, 2, 2)]).asFloat());
		const fp16type	aei		(a * e * i);
		const fp16type	bfg		(b * f * g);
		const fp16type	cdh		(c * d * h);
		const fp16type	ceg		(c * e * g);
		const fp16type	bdi		(b * d * i);
		const fp16type	afh		(a * f * h);
		const float		r		(aei.asFloat() + bfg.asFloat() + cdh.asFloat() - ceg.asFloat() - bdi.asFloat() - afh.asFloat());
		const fp16type	rf16	(r);

		out[0] = rf16.bits();
		min[0] = getMin(r, getULPs(in));
		max[0] = getMax(r, getULPs(in));

		return true;
	}
};

template<>
struct fp16Determinant<4> : public fp16MatrixBase
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 128.0; // This is not a precision test. Value is not from spec
	}

	deUint32 getComponentValidity ()
	{
		return 1;
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const size_t	rows		= 4;
		const size_t	cols		= 4;
		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(in.size() == 1);
		DE_ASSERT(getOutCompCount() == 1);
		DE_ASSERT(getArgCompCount(0) == alignedRows * alignedCols);
		DE_UNREF(alignedCols);
		DE_UNREF(alignedRows);

		// [ a b c d ]
		// [ e f g h ]
		// [ i j k l ]
		// [ m n o p ]
		const float		a		(fp16type(in[0][getNdx(rows, 0, 0)]).asFloat());
		const float		b		(fp16type(in[0][getNdx(rows, 1, 0)]).asFloat());
		const float		c		(fp16type(in[0][getNdx(rows, 2, 0)]).asFloat());
		const float		d		(fp16type(in[0][getNdx(rows, 3, 0)]).asFloat());
		const float		e		(fp16type(in[0][getNdx(rows, 0, 1)]).asFloat());
		const float		f		(fp16type(in[0][getNdx(rows, 1, 1)]).asFloat());
		const float		g		(fp16type(in[0][getNdx(rows, 2, 1)]).asFloat());
		const float		h		(fp16type(in[0][getNdx(rows, 3, 1)]).asFloat());
		const float		i		(fp16type(in[0][getNdx(rows, 0, 2)]).asFloat());
		const float		j		(fp16type(in[0][getNdx(rows, 1, 2)]).asFloat());
		const float		k		(fp16type(in[0][getNdx(rows, 2, 2)]).asFloat());
		const float		l		(fp16type(in[0][getNdx(rows, 3, 2)]).asFloat());
		const float		m		(fp16type(in[0][getNdx(rows, 0, 3)]).asFloat());
		const float		n		(fp16type(in[0][getNdx(rows, 1, 3)]).asFloat());
		const float		o		(fp16type(in[0][getNdx(rows, 2, 3)]).asFloat());
		const float		p		(fp16type(in[0][getNdx(rows, 3, 3)]).asFloat());

		// [ f g h ]
		// [ j k l ]
		// [ n o p ]
		const fp16type	fkp		(f * k * p);
		const fp16type	gln		(g * l * n);
		const fp16type	hjo		(h * j * o);
		const fp16type	hkn		(h * k * n);
		const fp16type	gjp		(g * j * p);
		const fp16type	flo		(f * l * o);
		const fp16type	detA	(a * (fkp.asFloat() + gln.asFloat() + hjo.asFloat() - hkn.asFloat() - gjp.asFloat() - flo.asFloat()));

		// [ e g h ]
		// [ i k l ]
		// [ m o p ]
		const fp16type	ekp		(e * k * p);
		const fp16type	glm		(g * l * m);
		const fp16type	hio		(h * i * o);
		const fp16type	hkm		(h * k * m);
		const fp16type	gip		(g * i * p);
		const fp16type	elo		(e * l * o);
		const fp16type	detB	(b * (ekp.asFloat() + glm.asFloat() + hio.asFloat() - hkm.asFloat() - gip.asFloat() - elo.asFloat()));

		// [ e f h ]
		// [ i j l ]
		// [ m n p ]
		const fp16type	ejp		(e * j * p);
		const fp16type	flm		(f * l * m);
		const fp16type	hin		(h * i * n);
		const fp16type	hjm		(h * j * m);
		const fp16type	fip		(f * i * p);
		const fp16type	eln		(e * l * n);
		const fp16type	detC	(c * (ejp.asFloat() + flm.asFloat() + hin.asFloat() - hjm.asFloat() - fip.asFloat() - eln.asFloat()));

		// [ e f g ]
		// [ i j k ]
		// [ m n o ]
		const fp16type	ejo		(e * j * o);
		const fp16type	fkm		(f * k * m);
		const fp16type	gin		(g * i * n);
		const fp16type	gjm		(g * j * m);
		const fp16type	fio		(f * i * o);
		const fp16type	ekn		(e * k * n);
		const fp16type	detD	(d * (ejo.asFloat() + fkm.asFloat() + gin.asFloat() - gjm.asFloat() - fio.asFloat() - ekn.asFloat()));

		const float		r		(detA.asFloat() - detB.asFloat() + detC.asFloat() - detD.asFloat());
		const fp16type	rf16	(r);

		out[0] = rf16.bits();
		min[0] = getMin(r, getULPs(in));
		max[0] = getMax(r, getULPs(in));

		return true;
	}
};

template<size_t size>
struct fp16Inverse;

template<>
struct fp16Inverse<2> : public fp16MatrixBase
{
	virtual double getULPs (vector<const deFloat16*>& in)
	{
		DE_UNREF(in);

		return 128.0; // This is not a precision test. Value is not from spec
	}

	deUint32 getComponentValidity ()
	{
		return getComponentMatrixValidityMask(2, 2);
	}

	template<class fp16type>
	bool calc (vector<const deFloat16*>& in, deFloat16* out, double* min, double* max)
	{
		const size_t	cols		= 2;
		const size_t	rows		= 2;
		const size_t	alignedCols	= (cols == 3) ? 4 : cols;
		const size_t	alignedRows	= (rows == 3) ? 4 : rows;

		DE_ASSERT(in.size() == 1);
		DE_ASSERT(getOutCompCount() == alignedRows * alignedCols);
		DE_ASSERT(getArgCompCount(0) == alignedRows * alignedCols);
		DE_UNREF(alignedCols);

		// [ a b ]
		// [ c d ]
		const float		a		(fp16type(in[0][getNdx(rows, 0, 0)]).asFloat());
		const float		b		(fp16type(in[0][getNdx(rows, 1, 0)]).asFloat());
		const float		c		(fp16type(in[0][getNdx(rows, 0, 1)]).asFloat());
		const float		d		(fp16type(in[0][getNdx(rows, 1, 1)]).asFloat());
		const float		ad		(a * d);
		const fp16type	adf16	(ad);
		const float		bc		(b * c);
		const fp16type	bcf16	(bc);
		const float		det		(adf16.asFloat() - bcf16.asFloat());
		const fp16type	det16	(det);

		out[0] = fp16type( d / det16.asFloat()).bits();
		out[1] = fp16type(-c / det16.asFloat()).bits();
		out[2] = fp16type(-b / det16.asFloat()).bits();
		out[3] = fp16type( a / det16.asFloat()).bits();

		for (size_t rowNdx = 0; rowNdx < rows; ++rowNdx)
			for (size_t colNdx = 0; colNdx < cols; ++colNdx)
			{
				const size_t	ndx	(colNdx * alignedRows + rowNdx);
				const fp16type	s	(out[ndx]);

				min[ndx] = getMin(s.asDouble(), getULPs(in));
				max[ndx] = getMax(s.asDouble(), getULPs(in));
			}

		return true;
	}
};

inline std::string fp16ToString(deFloat16 val)
{
	return tcu::toHex<4>(val).toString() + " (" + de::floatToString(tcu::Float16(val).asFloat(), 10) + ")";
}

template <size_t RES_COMPONENTS, size_t ARG0_COMPONENTS, size_t ARG1_COMPONENTS, size_t ARG2_COMPONENTS, class TestedArithmeticFunction>
bool compareFP16ArithmeticFunc (const std::vector<Resource>& inputs, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog& log)
{
	if (inputs.size() < 1 || inputs.size() > 3 || outputAllocs.size() != 1 || expectedOutputs.size() != 1)
		return false;

	const size_t	resultStep			= (RES_COMPONENTS == 3) ? 4 : RES_COMPONENTS;
	const size_t	iterationsCount		= expectedOutputs[0].getByteSize() / (sizeof(deFloat16) * resultStep);
	const size_t	inputsSteps[3]		=
	{
		(ARG0_COMPONENTS == 3) ? 4 : ARG0_COMPONENTS,
		(ARG1_COMPONENTS == 3) ? 4 : ARG1_COMPONENTS,
		(ARG2_COMPONENTS == 3) ? 4 : ARG2_COMPONENTS,
	};

	DE_ASSERT(expectedOutputs[0].getByteSize() > 0);
	DE_ASSERT(expectedOutputs[0].getByteSize() == sizeof(deFloat16) * iterationsCount * resultStep);

	for (size_t inputNdx = 0; inputNdx < inputs.size(); ++inputNdx)
	{
		DE_ASSERT(inputs[inputNdx].getByteSize() > 0);
		DE_ASSERT(inputs[inputNdx].getByteSize() == sizeof(deFloat16) * iterationsCount * inputsSteps[inputNdx]);
	}

	const deFloat16* const		outputAsFP16					= (const deFloat16*)outputAllocs[0]->getHostPtr();
	TestedArithmeticFunction	func;

	func.setOutCompCount(RES_COMPONENTS);
	func.setArgCompCount(0, ARG0_COMPONENTS);
	func.setArgCompCount(1, ARG1_COMPONENTS);
	func.setArgCompCount(2, ARG2_COMPONENTS);

	const bool					callOncePerComponent			= func.callOncePerComponent();
	const deUint32				componentValidityMask			= func.getComponentValidity();
	const size_t				denormModesCount				= 2;
	const char*					denormModes[denormModesCount]	= { "keep denormal numbers", "flush to zero" };
	const size_t				successfulRunsPerComponent		= denormModesCount * func.getFlavorCount();
	bool						success							= true;
	size_t						validatedCount					= 0;

	vector<deUint8>	inputBytes[3];

	for (size_t inputNdx = 0; inputNdx < inputs.size(); ++inputNdx)
		inputs[inputNdx].getBytes(inputBytes[inputNdx]);

	const deFloat16* const			inputsAsFP16[3]			=
	{
		inputs.size() >= 1 ? (const deFloat16*)&inputBytes[0][0] : DE_NULL,
		inputs.size() >= 2 ? (const deFloat16*)&inputBytes[1][0] : DE_NULL,
		inputs.size() >= 3 ? (const deFloat16*)&inputBytes[2][0] : DE_NULL,
	};

	for (size_t idx = 0; idx < iterationsCount; ++idx)
	{
		std::vector<size_t>			successfulRuns		(RES_COMPONENTS, successfulRunsPerComponent);
		std::vector<std::string>	errors				(RES_COMPONENTS);
		bool						iterationValidated	(true);

		for (size_t denormNdx = 0; denormNdx < 2; ++denormNdx)
		{
			for (size_t flavorNdx = 0; flavorNdx < func.getFlavorCount(); ++flavorNdx)
			{
				func.setFlavor(flavorNdx);

				const deFloat16*			iterationOutputFP16		= &outputAsFP16[idx * resultStep];
				vector<deFloat16>			iterationCalculatedFP16	(resultStep, 0);
				vector<double>				iterationEdgeMin		(resultStep, 0.0);
				vector<double>				iterationEdgeMax		(resultStep, 0.0);
				vector<const deFloat16*>	arguments;

				for (size_t componentNdx = 0; componentNdx < RES_COMPONENTS; ++componentNdx)
				{
					std::string	error;
					bool		reportError = false;

					if (callOncePerComponent || componentNdx == 0)
					{
						bool funcCallResult;

						arguments.clear();

						for (size_t inputNdx = 0; inputNdx < inputs.size(); ++inputNdx)
							arguments.push_back(&inputsAsFP16[inputNdx][idx * inputsSteps[inputNdx] + componentNdx]);

						if (denormNdx == 0)
							funcCallResult = func.template calc<tcu::Float16>(arguments, &iterationCalculatedFP16[componentNdx], &iterationEdgeMin[componentNdx], &iterationEdgeMax[componentNdx]);
						else
							funcCallResult = func.template calc<tcu::Float16Denormless>(arguments, &iterationCalculatedFP16[componentNdx], &iterationEdgeMin[componentNdx], &iterationEdgeMax[componentNdx]);

						if (!funcCallResult)
						{
							iterationValidated = false;

							if (callOncePerComponent)
								continue;
							else
								break;
						}
					}

					if ((componentValidityMask != 0) && (componentValidityMask & (1<<componentNdx)) == 0)
						continue;

					reportError = !compare16BitFloat(iterationCalculatedFP16[componentNdx], iterationOutputFP16[componentNdx], error);

					if (reportError)
					{
						tcu::Float16 expected	(iterationCalculatedFP16[componentNdx]);
						tcu::Float16 outputted	(iterationOutputFP16[componentNdx]);
						tcu::Float64 edgeMin    (iterationEdgeMin[componentNdx]);
						tcu::Float64 edgeMax    (iterationEdgeMax[componentNdx]);

						if (reportError && expected.isNaN())
							reportError = false;

						if (reportError && !expected.isNaN() && !outputted.isNaN())
						{
							if (reportError && !expected.isInf() && !outputted.isInf())
							{
								// Ignore rounding
								if (expected.bits() == outputted.bits() + 1 || expected.bits() + 1 == outputted.bits())
									reportError = false;
							}

							if (reportError && expected.isInf())
							{
								// RTZ rounding mode returns +/-65504 instead of Inf on overflow
								if (expected.sign() == 1 && outputted.bits() == 0x7bff && edgeMin.asDouble() <= std::numeric_limits<double>::max())
									reportError = false;
								else if (expected.sign() == -1 && outputted.bits() == 0xfbff && edgeMax.asDouble() >= -std::numeric_limits<double>::max())
									reportError = false;
							}

							if (reportError)
							{
								const double	outputtedDouble	= outputted.asDouble();

							    DE_ASSERT(edgeMin.isNaN() || edgeMax.isNaN() || (edgeMin.asDouble() <= edgeMax.asDouble()));

								if (de::inRange(outputtedDouble, edgeMin.asDouble(), edgeMax.asDouble()))
									reportError = false;
							}
						}

						if (reportError)
						{
							const size_t		inputsComps[3]	=
							{
								ARG0_COMPONENTS,
								ARG1_COMPONENTS,
								ARG2_COMPONENTS,
							};
							string				inputsValues	("Inputs:");
							string				flavorName		(func.getFlavorCount() == 1 ? "" : string(" flavor ") + de::toString(flavorNdx) + " (" + func.getCurrentFlavorName() + ")");
							std::stringstream	errStream;

							for (size_t inputNdx = 0; inputNdx < inputs.size(); ++inputNdx)
							{
								const size_t	inputCompsCount = inputsComps[inputNdx];

								inputsValues += " [" + de::toString(inputNdx) + "]=(";

								for (size_t compNdx = 0; compNdx < inputCompsCount; ++compNdx)
								{
									const deFloat16 inputComponentValue = inputsAsFP16[inputNdx][idx * inputsSteps[inputNdx] + compNdx];

									inputsValues += fp16ToString(inputComponentValue) + ((compNdx + 1 == inputCompsCount) ? ")": ", ");
								}
							}

							errStream	<< "At"
										<< " iteration " << de::toString(idx)
										<< " component " << de::toString(componentNdx)
										<< " denormMode " << de::toString(denormNdx)
										<< " (" << denormModes[denormNdx] << ")"
										<< " " << flavorName
										<< " " << inputsValues
										<< " outputted:" + fp16ToString(iterationOutputFP16[componentNdx])
										<< " expected:" + fp16ToString(iterationCalculatedFP16[componentNdx])
										<< " or in range: [" << iterationEdgeMin[componentNdx] << ", " << iterationEdgeMax[componentNdx] << "]."
										<< " " << error << "."
										<< std::endl;

							errors[componentNdx] += errStream.str();

							successfulRuns[componentNdx]--;
						}
					}
				}
			}
		}

		for (size_t componentNdx = 0; componentNdx < RES_COMPONENTS; ++componentNdx)
		{
			// Check if any component has total failure
			if (successfulRuns[componentNdx] == 0)
			{
				// Test failed in all denorm modes and all flavors for certain component: dump errors
				log << TestLog::Message << errors[componentNdx] << TestLog::EndMessage;

				success = false;
			}
		}

		if (iterationValidated)
			validatedCount++;
	}

	if (validatedCount < 16)
		TCU_THROW(InternalError, "Too few samples have been validated.");

	return success;
}

// IEEE-754 floating point numbers:
// +--------+------+----------+-------------+
// | binary | sign | exponent | significand |
// +--------+------+----------+-------------+
// | 16-bit |  1   |    5     |     10      |
// +--------+------+----------+-------------+
// | 32-bit |  1   |    8     |     23      |
// +--------+------+----------+-------------+
//
// 16-bit floats:
//
// 0   000 00   00 0000 0001 (0x0001: 2e-24:         minimum positive denormalized)
// 0   000 00   11 1111 1111 (0x03ff: 2e-14 - 2e-24: maximum positive denormalized)
// 0   000 01   00 0000 0000 (0x0400: 2e-14:         minimum positive normalized)
// 0   111 10   11 1111 1111 (0x7bff: 65504:         maximum positive normalized)
//
// 0   000 00   00 0000 0000 (0x0000: +0)
// 0   111 11   00 0000 0000 (0x7c00: +Inf)
// 0   000 00   11 1111 0000 (0x03f0: +Denorm)
// 0   000 01   00 0000 0001 (0x0401: +Norm)
// 0   111 11   00 0000 1111 (0x7c0f: +SNaN)
// 0   111 11   11 1111 0000 (0x7ff0: +QNaN)
// Generate and return 16-bit floats and their corresponding 32-bit values.
//
// The first 14 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 14 (numPicks).
vector<deFloat16> getFloat16a (de::Random& rnd, deUint32 count)
{
	vector<deFloat16>	float16;

	float16.reserve(count);

	// Zero
	float16.push_back(deUint16(0x0000));
	float16.push_back(deUint16(0x8000));
	// Infinity
	float16.push_back(deUint16(0x7c00));
	float16.push_back(deUint16(0xfc00));
	// Normalized
	float16.push_back(deUint16(0x0401));
	float16.push_back(deUint16(0x8401));
	// Some normal number
	float16.push_back(deUint16(0x14cb));
	float16.push_back(deUint16(0x94cb));
	// Min/max positive normal
	float16.push_back(deUint16(0x0400));
	float16.push_back(deUint16(0x7bff));
	// Min/max negative normal
	float16.push_back(deUint16(0x8400));
	float16.push_back(deUint16(0xfbff));
	// PI
	float16.push_back(deUint16(0x4248)); // 3.140625
	float16.push_back(deUint16(0xb248)); // -3.140625
	// PI/2
	float16.push_back(deUint16(0x3e48)); // 1.5703125
	float16.push_back(deUint16(0xbe48)); // -1.5703125
	float16.push_back(deUint16(0x3c00)); // 1.0
	float16.push_back(deUint16(0x3800)); // 0.5
	// Some useful constants
	float16.push_back(tcu::Float16(-2.5f).bits());
	float16.push_back(tcu::Float16(-1.0f).bits());
	float16.push_back(tcu::Float16( 0.4f).bits());
	float16.push_back(tcu::Float16( 2.5f).bits());

	const deUint32		numPicks	= static_cast<deUint32>(float16.size());

	DE_ASSERT(count >= numPicks);
	count -= numPicks;

	for (deUint32 numIdx = 0; numIdx < count; ++numIdx)
	{
		int			sign		= (rnd.getUint16() % 2 == 0) ? +1 : -1;
		int			exponent	= (rnd.getUint16() % 29) - 14 + 1;
		deUint16	mantissa	= static_cast<deUint16>(2 * (rnd.getUint16() % 512));

		// Exclude power of -14 to avoid denorms
		DE_ASSERT(de::inRange(exponent, -13, 15));

		float16.push_back(tcu::Float16::constructBits(sign, exponent, mantissa).bits());
	}

	return float16;
}

static inline vector<deFloat16> getInputData1 (deUint32 seed, size_t count, size_t argNo)
{
	DE_UNREF(argNo);

	de::Random	rnd(seed);

	return getFloat16a(rnd, static_cast<deUint32>(count));
}

static inline vector<deFloat16> getInputData2 (deUint32 seed, size_t count, size_t argNo)
{
	de::Random	rnd		(seed);
	size_t		newCount = static_cast<size_t>(deSqrt(double(count)));

	DE_ASSERT(newCount * newCount == count);

	vector<deFloat16>	float16 = getFloat16a(rnd, static_cast<deUint32>(newCount));

	return squarize(float16, static_cast<deUint32>(argNo));
}

static inline vector<deFloat16> getInputData3 (deUint32 seed, size_t count, size_t argNo)
{
	if (argNo == 0 || argNo == 1)
		return getInputData2(seed, count, argNo);
	else
		return getInputData1(seed<<argNo, count, argNo);
}

vector<deFloat16> getInputData (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(stride);

	vector<deFloat16>	result;

	switch (argCount)
	{
		case 1:result = getInputData1(seed, count, argNo); break;
		case 2:result = getInputData2(seed, count, argNo); break;
		case 3:result = getInputData3(seed, count, argNo); break;
		default: TCU_THROW(InternalError, "Invalid argument count specified");
	}

	if (compCount == 3)
	{
		const size_t		newCount = (3 * count) / 4;
		vector<deFloat16>	newResult;

		newResult.reserve(result.size());

		for (size_t ndx = 0; ndx < newCount; ++ndx)
		{
			newResult.push_back(result[ndx]);

			if (ndx % 3 == 2)
				newResult.push_back(0);
		}

		result = newResult;
	}

	DE_ASSERT(result.size() == count);

	return result;
}

// Generator for functions requiring data in range [1, inf]
vector<deFloat16> getInputDataAC (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	vector<deFloat16>	result;

	result = getInputData(seed, count, compCount, stride, argCount, argNo);

	// Filter out values below 1.0 from upper half of numbers
	for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
	{
		const float f = tcu::Float16(result[idx]).asFloat();

		if (f < 1.0f)
			result[idx] = tcu::Float16(1.0f - f).bits();
	}

	return result;
}

// Generator for functions requiring data in range [-1, 1]
vector<deFloat16> getInputDataA (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	vector<deFloat16>	result;

	result = getInputData(seed, count, compCount, stride, argCount, argNo);

	for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
	{
		const float f = tcu::Float16(result[idx]).asFloat();

		if (!de::inRange(f, -1.0f, 1.0f))
			result[idx] = tcu::Float16(deFloatFrac(f)).bits();
	}

	return result;
}

// Generator for functions requiring data in range [-pi, pi]
vector<deFloat16> getInputDataPI (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	vector<deFloat16>	result;

	result = getInputData(seed, count, compCount, stride, argCount, argNo);

	for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
	{
		const float f = tcu::Float16(result[idx]).asFloat();

		if (!de::inRange(f, -DE_PI, DE_PI))
			result[idx] = tcu::Float16(fmodf(f, DE_PI)).bits();
	}

	return result;
}

// Generator for functions requiring data in range [0, inf]
vector<deFloat16> getInputDataP (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	vector<deFloat16>	result;

	result = getInputData(seed, count, compCount, stride, argCount, argNo);

	if (argNo == 0)
	{
		for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
			result[idx] &= static_cast<deFloat16>(~0x8000);
	}

	return result;
}

vector<deFloat16> getInputDataV (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(stride);
	DE_UNREF(argCount);

	vector<deFloat16>	result;

	if (argNo == 0)
		result = getInputData2(seed, count, argNo);
	else
	{
		const size_t		alignedCount	= (compCount == 3) ? 4 : compCount;
		const size_t		newCountX		= static_cast<size_t>(deSqrt(double(count * alignedCount)));
		const size_t		newCountY		= count / newCountX;
		de::Random			rnd				(seed);
		vector<deFloat16>	float16			= getFloat16a(rnd, static_cast<deUint32>(newCountX));

		DE_ASSERT(newCountX * newCountX == alignedCount * count);

		for (size_t numIdx = 0; numIdx < newCountX; ++numIdx)
		{
			const vector<deFloat16>	tmp(newCountY, float16[numIdx]);

			result.insert(result.end(), tmp.begin(), tmp.end());
		}
	}

	DE_ASSERT(result.size() == count);

	return result;
}

vector<deFloat16> getInputDataM (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(compCount);
	DE_UNREF(stride);
	DE_UNREF(argCount);

	de::Random			rnd		(seed << argNo);
	vector<deFloat16>	result;

	result = getFloat16a(rnd, static_cast<deUint32>(count));

	DE_ASSERT(result.size() == count);

	return result;
}

vector<deFloat16> getInputDataD (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(compCount);
	DE_UNREF(argCount);

	de::Random			rnd		(seed << argNo);
	vector<deFloat16>	result;

	for (deUint32 numIdx = 0; numIdx < count; ++numIdx)
	{
		int num	= (rnd.getUint16() % 16) - 8;

		result.push_back(tcu::Float16(float(num)).bits());
	}

	result[0 * stride] = deUint16(0x7c00); // +Inf
	result[1 * stride] = deUint16(0xfc00); // -Inf

	DE_ASSERT(result.size() == count);

	return result;
}

// Generator for smoothstep function
vector<deFloat16> getInputDataSS (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	vector<deFloat16>	result;

	result = getInputDataD(seed, count, compCount, stride, argCount, argNo);

	if (argNo == 0)
	{
		for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
		{
			const float f = tcu::Float16(result[idx]).asFloat();

			if (f > 4.0f)
				result[idx] = tcu::Float16(-f).bits();
		}
	}

	if (argNo == 1)
	{
		for (size_t idx = result.size() / 2; idx < result.size(); ++idx)
		{
			const float f = tcu::Float16(result[idx]).asFloat();

			if (f < 4.0f)
				result[idx] = tcu::Float16(-f).bits();
		}
	}

	return result;
}

// Generates normalized vectors for arguments 0 and 1
vector<deFloat16> getInputDataN (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(compCount);
	DE_UNREF(argCount);

	de::Random			rnd		(seed << argNo);
	vector<deFloat16>	result;

	if (argNo == 0 || argNo == 1)
	{
		// The input parameters for the incident vector I and the surface normal N must already be normalized
		for (size_t numIdx = 0; numIdx < count; numIdx += stride)
		{
			vector <float>	unnormolized;
			float			sum				= 0;

			for (size_t compIdx = 0; compIdx < compCount; ++compIdx)
				unnormolized.push_back(float((rnd.getUint16() % 16) - 8));

			for (size_t compIdx = 0; compIdx < compCount; ++compIdx)
				sum += unnormolized[compIdx] * unnormolized[compIdx];

			sum = deFloatSqrt(sum);
			if (sum == 0.0f)
				unnormolized[0] = sum = 1.0f;

			for (size_t compIdx = 0; compIdx < compCount; ++compIdx)
				result.push_back(tcu::Float16(unnormolized[compIdx] / sum).bits());

			for (size_t compIdx = compCount; compIdx < stride; ++compIdx)
				result.push_back(0);
		}
	}
	else
	{
		// Input parameter eta
		for (deUint32 numIdx = 0; numIdx < count; ++numIdx)
		{
			int num	= (rnd.getUint16() % 16) - 8;

			result.push_back(tcu::Float16(float(num)).bits());
		}
	}

	DE_ASSERT(result.size() == count);

	return result;
}

// Data generator for complex matrix functions like determinant and inverse
vector<deFloat16> getInputDataC (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo)
{
	DE_UNREF(compCount);
	DE_UNREF(stride);
	DE_UNREF(argCount);

	de::Random			rnd		(seed << argNo);
	vector<deFloat16>	result;

	for (deUint32 numIdx = 0; numIdx < count; ++numIdx)
	{
		int num	= (rnd.getUint16() % 16) - 8;

		result.push_back(tcu::Float16(float(num)).bits());
	}

	DE_ASSERT(result.size() == count);

	return result;
}

struct Math16TestType
{
	const char*		typePrefix;
	const size_t	typeComponents;
	const size_t	typeArrayStride;
	const size_t	typeStructStride;
	const char*		storage_type;
};

enum Math16DataTypes
{
	NONE	= 0,
	SCALAR	= 1,
	VEC2	= 2,
	VEC3	= 3,
	VEC4	= 4,
	MAT2X2,
	MAT2X3,
	MAT2X4,
	MAT3X2,
	MAT3X3,
	MAT3X4,
	MAT4X2,
	MAT4X3,
	MAT4X4,
	MATH16_TYPE_LAST
};

struct Math16ArgFragments
{
	const char*	bodies;
	const char*	variables;
	const char*	decorations;
	const char*	funcVariables;
};

typedef vector<deFloat16> Math16GetInputData (deUint32 seed, size_t count, size_t compCount, size_t stride, size_t argCount, size_t argNo);

struct Math16TestFunc
{
	const char*					funcName;
	const char*					funcSuffix;
	size_t						funcArgsCount;
	size_t						typeResult;
	size_t						typeArg0;
	size_t						typeArg1;
	size_t						typeArg2;
	Math16GetInputData*			getInputDataFunc;
	VerifyIOFunc				verifyFunc;
};

template<class SpecResource>
void createFloat16ArithmeticFuncTest (tcu::TestContext& testCtx, tcu::TestCaseGroup& testGroup, const size_t testTypeIdx, const Math16TestFunc& testFunc)
{
	const int					testSpecificSeed			= deStringHash(testGroup.getName());
	const int					seed						= testCtx.getCommandLine().getBaseSeed() ^ testSpecificSeed;
	const size_t				numDataPointsByAxis			= 32;
	const size_t				numDataPoints				= numDataPointsByAxis * numDataPointsByAxis;
	const char*					componentType				= "f16";
	const Math16TestType		testTypes[MATH16_TYPE_LAST]	=
	{
		{ "",		0,	 0,						 0,						"" },
		{ "",		1,	 1 * sizeof(deFloat16),	 2 * sizeof(deFloat16),	"u32_half_ndp" },
		{ "v2",		2,	 2 * sizeof(deFloat16),	 2 * sizeof(deFloat16),	"u32_ndp" },
		{ "v3",		3,	 4 * sizeof(deFloat16),	 4 * sizeof(deFloat16),	"u32_ndp_2" },
		{ "v4",		4,	 4 * sizeof(deFloat16),	 4 * sizeof(deFloat16),	"u32_ndp_2" },
		{ "m2x2",	0,	 4 * sizeof(deFloat16),	 4 * sizeof(deFloat16),	"u32_ndp_2" },
		{ "m2x3",	0,	 8 * sizeof(deFloat16),	 8 * sizeof(deFloat16),	"u32_ndp_4" },
		{ "m2x4",	0,	 8 * sizeof(deFloat16),	 8 * sizeof(deFloat16),	"u32_ndp_4" },
		{ "m3x2",	0,	 8 * sizeof(deFloat16),	 8 * sizeof(deFloat16),	"u32_ndp_3" },
		{ "m3x3",	0,	16 * sizeof(deFloat16),	16 * sizeof(deFloat16),	"u32_ndp_6" },
		{ "m3x4",	0,	16 * sizeof(deFloat16),	16 * sizeof(deFloat16),	"u32_ndp_6" },
		{ "m4x2",	0,	 8 * sizeof(deFloat16),	 8 * sizeof(deFloat16),	"u32_ndp_4" },
		{ "m4x3",	0,	16 * sizeof(deFloat16),	16 * sizeof(deFloat16),	"u32_ndp_8" },
		{ "m4x4",	0,	16 * sizeof(deFloat16),	16 * sizeof(deFloat16),	"u32_ndp_8" },
	};

	DE_ASSERT(testTypeIdx == testTypes[testTypeIdx].typeComponents);


	const StringTemplate preMain
	(
		"     %c_i32_ndp  = OpConstant %i32 ${num_data_points}\n"

		"        %f16     = OpTypeFloat 16\n"
		"        %v2f16   = OpTypeVector %f16 2\n"
		"        %v3f16   = OpTypeVector %f16 3\n"
		"        %v4f16   = OpTypeVector %f16 4\n"
		"        %m2x2f16 = OpTypeMatrix %v2f16 2\n"
		"        %m2x3f16 = OpTypeMatrix %v3f16 2\n"
		"        %m2x4f16 = OpTypeMatrix %v4f16 2\n"
		"        %m3x2f16 = OpTypeMatrix %v2f16 3\n"
		"        %m3x3f16 = OpTypeMatrix %v3f16 3\n"
		"        %m3x4f16 = OpTypeMatrix %v4f16 3\n"
		"        %m4x2f16 = OpTypeMatrix %v2f16 4\n"
		"        %m4x3f16 = OpTypeMatrix %v3f16 4\n"
		"        %m4x4f16 = OpTypeMatrix %v4f16 4\n"

		"       %fp_v2i32 = OpTypePointer Function %v2i32\n"
		"       %fp_v3i32 = OpTypePointer Function %v3i32\n"
		"       %fp_v4i32 = OpTypePointer Function %v4i32\n"

		"      %c_u32_ndp = OpConstant %u32 ${num_data_points}\n"
		" %c_u32_half_ndp = OpSpecConstantOp %u32 UDiv %c_i32_ndp %c_u32_2\n"
		"        %c_u32_5 = OpConstant %u32 5\n"
		"        %c_u32_6 = OpConstant %u32 6\n"
		"        %c_u32_7 = OpConstant %u32 7\n"
		"        %c_u32_8 = OpConstant %u32 8\n"
		"        %c_f16_0 = OpConstant %f16 0\n"
		"        %c_f16_1 = OpConstant %f16 1\n"
		"      %c_v2f16_0 = OpConstantComposite %v2f16 %c_f16_0 %c_f16_0\n"
		"         %up_u32 = OpTypePointer Uniform %u32\n"
		"%c_u32_high_ones = OpConstant %u32 0xffff0000\n"
		" %c_u32_low_ones = OpConstant %u32 0x0000ffff\n"

		"    %ra_u32_half_ndp = OpTypeArray %u32 %c_u32_half_ndp\n"
		"  %SSBO_u32_half_ndp = OpTypeStruct %ra_u32_half_ndp\n"
		"%up_SSBO_u32_half_ndp = OpTypePointer Uniform %SSBO_u32_half_ndp\n"
		"         %ra_u32_ndp = OpTypeArray %u32 %c_u32_ndp\n"
		"       %SSBO_u32_ndp = OpTypeStruct %ra_u32_ndp\n"
		"    %up_SSBO_u32_ndp = OpTypePointer Uniform %SSBO_u32_ndp\n"
		"           %ra_u32_2 = OpTypeArray %u32 %c_u32_2\n"
		"        %up_ra_u32_2 = OpTypePointer Uniform %ra_u32_2\n"
		"      %ra_ra_u32_ndp = OpTypeArray %ra_u32_2 %c_u32_ndp\n"
		"     %SSBO_u32_ndp_2 = OpTypeStruct %ra_ra_u32_ndp\n"
		"  %up_SSBO_u32_ndp_2 = OpTypePointer Uniform %SSBO_u32_ndp_2\n"
		"           %ra_u32_4 = OpTypeArray %u32 %c_u32_4\n"
		"        %up_ra_u32_4 = OpTypePointer Uniform %ra_u32_4\n"
		"        %ra_ra_u32_4 = OpTypeArray %ra_u32_4 %c_u32_ndp\n"
		"     %SSBO_u32_ndp_4 = OpTypeStruct %ra_ra_u32_4\n"
		"  %up_SSBO_u32_ndp_4 = OpTypePointer Uniform %SSBO_u32_ndp_4\n"
		"           %ra_u32_3 = OpTypeArray %u32 %c_u32_3\n"
		"        %up_ra_u32_3 = OpTypePointer Uniform %ra_u32_3\n"
		"        %ra_ra_u32_3 = OpTypeArray %ra_u32_3 %c_u32_ndp\n"
		"     %SSBO_u32_ndp_3 = OpTypeStruct %ra_ra_u32_3\n"
		"  %up_SSBO_u32_ndp_3 = OpTypePointer Uniform %SSBO_u32_ndp_3\n"
		"           %ra_u32_6 = OpTypeArray %u32 %c_u32_6\n"
		"        %up_ra_u32_6 = OpTypePointer Uniform %ra_u32_6\n"
		"        %ra_ra_u32_6 = OpTypeArray %ra_u32_6 %c_u32_ndp\n"
		"     %SSBO_u32_ndp_6 = OpTypeStruct %ra_ra_u32_6\n"
		"  %up_SSBO_u32_ndp_6 = OpTypePointer Uniform %SSBO_u32_ndp_6\n"
		"           %ra_u32_8 = OpTypeArray %u32 %c_u32_8\n"
		"        %up_ra_u32_8 = OpTypePointer Uniform %ra_u32_8\n"
		"        %ra_ra_u32_8 = OpTypeArray %ra_u32_8 %c_u32_ndp\n"
		"     %SSBO_u32_ndp_8 = OpTypeStruct %ra_ra_u32_8\n"
		"  %up_SSBO_u32_ndp_8 = OpTypePointer Uniform %SSBO_u32_ndp_8\n"

		"         %f16_i32_fn = OpTypeFunction %f16 %i32\n"
		"       %v2f16_i32_fn = OpTypeFunction %v2f16 %i32\n"
		"       %v3f16_i32_fn = OpTypeFunction %v3f16 %i32\n"
		"       %v4f16_i32_fn = OpTypeFunction %v4f16 %i32\n"
		"     %m2x2f16_i32_fn = OpTypeFunction %m2x2f16 %i32\n"
		"     %m2x3f16_i32_fn = OpTypeFunction %m2x3f16 %i32\n"
		"     %m2x4f16_i32_fn = OpTypeFunction %m2x4f16 %i32\n"
		"     %m3x2f16_i32_fn = OpTypeFunction %m3x2f16 %i32\n"
		"     %m3x3f16_i32_fn = OpTypeFunction %m3x3f16 %i32\n"
		"     %m3x4f16_i32_fn = OpTypeFunction %m3x4f16 %i32\n"
		"     %m4x2f16_i32_fn = OpTypeFunction %m4x2f16 %i32\n"
		"     %m4x3f16_i32_fn = OpTypeFunction %m4x3f16 %i32\n"
		"     %m4x4f16_i32_fn = OpTypeFunction %m4x4f16 %i32\n"
		"    %void_f16_i32_fn = OpTypeFunction %void %f16 %i32\n"
		"  %void_v2f16_i32_fn = OpTypeFunction %void %v2f16 %i32\n"
		"  %void_v3f16_i32_fn = OpTypeFunction %void %v3f16 %i32\n"
		"  %void_v4f16_i32_fn = OpTypeFunction %void %v4f16 %i32\n"
		"%void_m2x2f16_i32_fn = OpTypeFunction %void %m2x2f16 %i32\n"
		"%void_m2x3f16_i32_fn = OpTypeFunction %void %m2x3f16 %i32\n"
		"%void_m2x4f16_i32_fn = OpTypeFunction %void %m2x4f16 %i32\n"
		"%void_m3x2f16_i32_fn = OpTypeFunction %void %m3x2f16 %i32\n"
		"%void_m3x3f16_i32_fn = OpTypeFunction %void %m3x3f16 %i32\n"
		"%void_m3x4f16_i32_fn = OpTypeFunction %void %m3x4f16 %i32\n"
		"%void_m4x2f16_i32_fn = OpTypeFunction %void %m4x2f16 %i32\n"
		"%void_m4x3f16_i32_fn = OpTypeFunction %void %m4x3f16 %i32\n"
		"%void_m4x4f16_i32_fn = OpTypeFunction %void %m4x4f16 %i32\n"
		"${arg_vars}"
	);

	const StringTemplate decoration
	(
		"OpDecorate %ra_u32_half_ndp ArrayStride 4\n"
		"OpMemberDecorate %SSBO_u32_half_ndp 0 Offset 0\n"
		"OpDecorate %SSBO_u32_half_ndp BufferBlock\n"

		"OpDecorate %ra_u32_ndp ArrayStride 4\n"
		"OpMemberDecorate %SSBO_u32_ndp 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp BufferBlock\n"

		"OpDecorate %ra_u32_2 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_ndp ArrayStride 8\n"
		"OpMemberDecorate %SSBO_u32_ndp_2 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp_2 BufferBlock\n"

		"OpDecorate %ra_u32_4 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_4 ArrayStride 16\n"
		"OpMemberDecorate %SSBO_u32_ndp_4 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp_4 BufferBlock\n"

		"OpDecorate %ra_u32_3 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_3 ArrayStride 16\n"
		"OpMemberDecorate %SSBO_u32_ndp_3 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp_3 BufferBlock\n"

		"OpDecorate %ra_u32_6 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_6 ArrayStride 32\n"
		"OpMemberDecorate %SSBO_u32_ndp_6 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp_6 BufferBlock\n"

		"OpDecorate %ra_u32_8 ArrayStride 4\n"
		"OpDecorate %ra_ra_u32_8 ArrayStride 32\n"
		"OpMemberDecorate %SSBO_u32_ndp_8 0 Offset 0\n"
		"OpDecorate %SSBO_u32_ndp_8 BufferBlock\n"

		"${arg_decorations}"
	);

	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"
		"    %entry = OpLabel\n"

		"        %i = OpVariable %fp_i32 Function\n"
		"${arg_infunc_vars}"
		"             OpStore %i %c_i32_0\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"${arg_func_call}"

		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpReturnValue %param\n"
		"             OpFunctionEnd\n"
	);

	const Math16ArgFragments	argFragment1	=
	{
		"     %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0\n"
		"     %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",
		"",
		"",
		"",
	};

	const Math16ArgFragments	argFragment2	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		" %val_src1 = OpFunctionCall %${t1} %ld_arg_ssbo_src1 %ndx\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0 %val_src1\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",
		"",
		"",
		"",
	};

	const Math16ArgFragments	argFragment3	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		" %val_src1 = OpFunctionCall %${t1} %ld_arg_ssbo_src1 %ndx\n"
		" %val_src2 = OpFunctionCall %${t2} %ld_arg_ssbo_src2 %ndx\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0 %val_src1 %val_src2\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",
		"",
		"",
		"",
	};

	const Math16ArgFragments	argFragmentLdExp	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		" %val_src1 = OpFunctionCall %${t1} %ld_arg_ssbo_src1 %ndx\n"
		"%val_src1i = OpConvertFToS %${dr}i32 %val_src1\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0 %val_src1i\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"",

		"",

		"",
	};

	const Math16ArgFragments	argFragmentModfFrac	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0 %tmp\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"   %fp_tmp = OpTypePointer Function %${tr}\n",

		"",

		"      %tmp = OpVariable %fp_tmp Function\n",
	};

	const Math16ArgFragments	argFragmentModfInt	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"%val_dummy = ${op} %${tr} ${ext_inst} %val_src0 %tmp\n"
		"     %tmp0 = OpAccessChain %fp_tmp %tmp\n"
		"  %val_dst = OpLoad %${tr} %tmp0\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"   %fp_tmp = OpTypePointer Function %${tr}\n",

		"",

		"      %tmp = OpVariable %fp_tmp Function\n",
	};

	const Math16ArgFragments	argFragmentModfStruct	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %val_tmp = ${op} %st_tmp ${ext_inst} %val_src0\n"
		"%tmp_ptr_s = OpAccessChain %fp_tmp %tmp\n"
		"             OpStore %tmp_ptr_s %val_tmp\n"
		"%tmp_ptr_l = OpAccessChain %fp_${tr} %tmp %c_${struct_member}\n"
		"  %val_dst = OpLoad %${tr} %tmp_ptr_l\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"  %fp_${tr} = OpTypePointer Function %${tr}\n"
		"   %st_tmp = OpTypeStruct %${tr} %${tr}\n"
		"   %fp_tmp = OpTypePointer Function %st_tmp\n"
		"   %c_frac = OpConstant %i32 0\n"
		"    %c_int = OpConstant %i32 1\n",

		"OpMemberDecorate %st_tmp 0 Offset 0\n"
		"OpMemberDecorate %st_tmp 1 Offset ${struct_stride}\n",

		"      %tmp = OpVariable %fp_tmp Function\n",
	};

	const Math16ArgFragments	argFragmentFrexpStructS	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %val_tmp = ${op} %st_tmp ${ext_inst} %val_src0\n"
		"%tmp_ptr_s = OpAccessChain %fp_tmp %tmp\n"
		"             OpStore %tmp_ptr_s %val_tmp\n"
		"%tmp_ptr_l = OpAccessChain %fp_${tr} %tmp %c_i32_0\n"
		"  %val_dst = OpLoad %${tr} %tmp_ptr_l\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"  %fp_${tr} = OpTypePointer Function %${tr}\n"
		"   %st_tmp = OpTypeStruct %${tr} %${dr}i32\n"
		"   %fp_tmp = OpTypePointer Function %st_tmp\n",

		"OpMemberDecorate %st_tmp 0 Offset 0\n"
		"OpMemberDecorate %st_tmp 1 Offset ${struct_stride}\n",

		"      %tmp = OpVariable %fp_tmp Function\n",
	};

	const Math16ArgFragments	argFragmentFrexpStructE	=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %val_tmp = ${op} %st_tmp ${ext_inst} %val_src0\n"
		"%tmp_ptr_s = OpAccessChain %fp_tmp %tmp\n"
		"             OpStore %tmp_ptr_s %val_tmp\n"
		"%tmp_ptr_l = OpAccessChain %fp_${dr}i32 %tmp %c_i32_1\n"
		"%val_dst_i = OpLoad %${dr}i32 %tmp_ptr_l\n"
		"  %val_dst = OpConvertSToF %${tr} %val_dst_i\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"   %st_tmp = OpTypeStruct %${tr} %${dr}i32\n"
		"   %fp_tmp = OpTypePointer Function %st_tmp\n",

		"OpMemberDecorate %st_tmp 0 Offset 0\n"
		"OpMemberDecorate %st_tmp 1 Offset ${struct_stride}\n",

		"      %tmp = OpVariable %fp_tmp Function\n",
	};

	const Math16ArgFragments	argFragmentFrexpS		=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %out_exp = OpAccessChain %fp_${dr}i32 %tmp\n"
		"  %val_dst = ${op} %${tr} ${ext_inst} %val_src0 %out_exp\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"",

		"",

		"      %tmp = OpVariable %fp_${dr}i32 Function\n",
	};

	const Math16ArgFragments	argFragmentFrexpE		=
	{
		" %val_src0 = OpFunctionCall %${t0} %ld_arg_ssbo_src0 %ndx\n"
		"  %out_exp = OpAccessChain %fp_${dr}i32 %tmp\n"
		"%val_dummy = ${op} %${tr} ${ext_inst} %val_src0 %out_exp\n"
		"%val_dst_i = OpLoad %${dr}i32 %out_exp\n"
		"  %val_dst = OpConvertSToF %${tr} %val_dst_i\n"
		"      %dst = OpFunctionCall %void %st_fn_ssbo_dst %val_dst %ndx\n",

		"",

		"",

		"      %tmp = OpVariable %fp_${dr}i32 Function\n",
	};

	string load_funcs[MATH16_TYPE_LAST];
	load_funcs[SCALAR] = loadScalarF16FromUint;
	load_funcs[VEC2]   = loadV2F16FromUint;
	load_funcs[VEC3]   = loadV3F16FromUints;
	load_funcs[VEC4]   = loadV4F16FromUints;
	load_funcs[MAT2X2] = loadM2x2F16FromUints;
	load_funcs[MAT2X3] = loadM2x3F16FromUints;
	load_funcs[MAT2X4] = loadM2x4F16FromUints;
	load_funcs[MAT3X2] = loadM3x2F16FromUints;
	load_funcs[MAT3X3] = loadM3x3F16FromUints;
	load_funcs[MAT3X4] = loadM3x4F16FromUints;
	load_funcs[MAT4X2] = loadM4x2F16FromUints;
	load_funcs[MAT4X3] = loadM4x3F16FromUints;
	load_funcs[MAT4X4] = loadM4x4F16FromUints;

	string store_funcs[MATH16_TYPE_LAST];
	store_funcs[SCALAR] = storeScalarF16AsUint;
	store_funcs[VEC2]   = storeV2F16AsUint;
	store_funcs[VEC3]   = storeV3F16AsUints;
	store_funcs[VEC4]   = storeV4F16AsUints;
	store_funcs[MAT2X2] = storeM2x2F16AsUints;
	store_funcs[MAT2X3] = storeM2x3F16AsUints;
	store_funcs[MAT2X4] = storeM2x4F16AsUints;
	store_funcs[MAT3X2] = storeM3x2F16AsUints;
	store_funcs[MAT3X3] = storeM3x3F16AsUints;
	store_funcs[MAT3X4] = storeM3x4F16AsUints;
	store_funcs[MAT4X2] = storeM4x2F16AsUints;
	store_funcs[MAT4X3] = storeM4x3F16AsUints;
	store_funcs[MAT4X4] = storeM4x4F16AsUints;

	const Math16TestType&		testType				= testTypes[testTypeIdx];
	const string				funcNameString			= string(testFunc.funcName) + string(testFunc.funcSuffix);
	const string				testName				= de::toLower(funcNameString);
	const Math16ArgFragments*	argFragments			= DE_NULL;
	const size_t				typeStructStride		= testType.typeStructStride;
	const bool					extInst					= !(testFunc.funcName[0] == 'O' && testFunc.funcName[1] == 'p');
	const size_t				numFloatsPerArg0Type	= testTypes[testFunc.typeArg0].typeArrayStride / sizeof(deFloat16);
	const size_t				iterations				= numDataPoints / numFloatsPerArg0Type;
	const size_t				numFloatsPerResultType	= testTypes[testFunc.typeResult].typeArrayStride / sizeof(deFloat16);
	const vector<deFloat16>		float16DummyOutput		(iterations * numFloatsPerResultType, 0);
	VulkanFeatures				features;
	SpecResource				specResource;
	map<string, string>			specs;
	map<string, string>			fragments;
	vector<string>				extensions;
	string						funcCall;
	string						funcVariables;
	string						variables;
	string						declarations;
	string						decorations;
	string						functions;

	switch (testFunc.funcArgsCount)
	{
		case 1:
		{
			argFragments = &argFragment1;

			if (funcNameString == "ModfFrac")		argFragments = &argFragmentModfFrac;
			if (funcNameString == "ModfInt")		argFragments = &argFragmentModfInt;
			if (funcNameString == "ModfStructFrac")	argFragments = &argFragmentModfStruct;
			if (funcNameString == "ModfStructInt")	argFragments = &argFragmentModfStruct;
			if (funcNameString == "FrexpS")			argFragments = &argFragmentFrexpS;
			if (funcNameString == "FrexpE")			argFragments = &argFragmentFrexpE;
			if (funcNameString == "FrexpStructS")	argFragments = &argFragmentFrexpStructS;
			if (funcNameString == "FrexpStructE")	argFragments = &argFragmentFrexpStructE;

			break;
		}
		case 2:
		{
			argFragments = &argFragment2;

			if (funcNameString == "Ldexp")			argFragments = &argFragmentLdExp;

			break;
		}
		case 3:
		{
			argFragments = &argFragment3;

			break;
		}
		default:
		{
			TCU_THROW(InternalError, "Invalid number of arguments");
		}
	}

	functions = StringTemplate(store_funcs[testFunc.typeResult]).specialize({{"var", "ssbo_dst"}});
	if (testFunc.funcArgsCount == 1)
	{
		functions += StringTemplate(load_funcs[testFunc.typeArg0]).specialize({{"var", "ssbo_src0"}});
		variables +=
			" %ssbo_src0 = OpVariable %up_SSBO_${store_t0} Uniform\n"
			"  %ssbo_dst = OpVariable %up_SSBO_${store_tr} Uniform\n";

		decorations +=
			"OpDecorate %ssbo_src0 DescriptorSet 0\n"
			"OpDecorate %ssbo_src0 Binding 0\n"
			"OpDecorate %ssbo_dst DescriptorSet 0\n"
			"OpDecorate %ssbo_dst Binding 1\n";
	}
	else if (testFunc.funcArgsCount == 2)
	{
		functions += StringTemplate(load_funcs[testFunc.typeArg0]).specialize({{"var", "ssbo_src0"}});
		functions += StringTemplate(load_funcs[testFunc.typeArg1]).specialize({{"var", "ssbo_src1"}});
		variables +=
			" %ssbo_src0 = OpVariable %up_SSBO_${store_t0} Uniform\n"
			" %ssbo_src1 = OpVariable %up_SSBO_${store_t1} Uniform\n"
			"  %ssbo_dst = OpVariable %up_SSBO_${store_tr} Uniform\n";

		decorations +=
			"OpDecorate %ssbo_src0 DescriptorSet 0\n"
			"OpDecorate %ssbo_src0 Binding 0\n"
			"OpDecorate %ssbo_src1 DescriptorSet 0\n"
			"OpDecorate %ssbo_src1 Binding 1\n"
			"OpDecorate %ssbo_dst DescriptorSet 0\n"
			"OpDecorate %ssbo_dst Binding 2\n";
	}
	else if (testFunc.funcArgsCount == 3)
	{
		functions += StringTemplate(load_funcs[testFunc.typeArg0]).specialize({{"var", "ssbo_src0"}});
		functions += StringTemplate(load_funcs[testFunc.typeArg1]).specialize({{"var", "ssbo_src1"}});
		functions += StringTemplate(load_funcs[testFunc.typeArg2]).specialize({{"var", "ssbo_src2"}});
		variables +=
			" %ssbo_src0 = OpVariable %up_SSBO_${store_t0} Uniform\n"
			" %ssbo_src1 = OpVariable %up_SSBO_${store_t1} Uniform\n"
			" %ssbo_src2 = OpVariable %up_SSBO_${store_t2} Uniform\n"
			"  %ssbo_dst = OpVariable %up_SSBO_${store_tr} Uniform\n";

		decorations +=
			"OpDecorate %ssbo_src0 DescriptorSet 0\n"
			"OpDecorate %ssbo_src0 Binding 0\n"
			"OpDecorate %ssbo_src1 DescriptorSet 0\n"
			"OpDecorate %ssbo_src1 Binding 1\n"
			"OpDecorate %ssbo_src2 DescriptorSet 0\n"
			"OpDecorate %ssbo_src2 Binding 2\n"
			"OpDecorate %ssbo_dst DescriptorSet 0\n"
			"OpDecorate %ssbo_dst Binding 3\n";
	}
	else
	{
		TCU_THROW(InternalError, "Invalid number of function arguments");
	}

	variables	+= argFragments->variables;
	decorations	+= argFragments->decorations;

	specs["dr"]					= testTypes[testFunc.typeResult].typePrefix;
	specs["d0"]					= testTypes[testFunc.typeArg0].typePrefix;
	specs["d1"]					= testTypes[testFunc.typeArg1].typePrefix;
	specs["d2"]					= testTypes[testFunc.typeArg2].typePrefix;
	specs["tr"]					= string(testTypes[testFunc.typeResult].typePrefix) + componentType;
	specs["t0"]					= string(testTypes[testFunc.typeArg0].typePrefix) + componentType;
	specs["t1"]					= string(testTypes[testFunc.typeArg1].typePrefix) + componentType;
	specs["t2"]					= string(testTypes[testFunc.typeArg2].typePrefix) + componentType;
	specs["store_tr"]			= string(testTypes[testFunc.typeResult].storage_type);
	specs["store_t0"]			= string(testTypes[testFunc.typeArg0].storage_type);
	specs["store_t1"]			= string(testTypes[testFunc.typeArg1].storage_type);
	specs["store_t2"]			= string(testTypes[testFunc.typeArg2].storage_type);
	specs["struct_stride"]		= de::toString(typeStructStride);
	specs["op"]					= extInst ? "OpExtInst" : testFunc.funcName;
	specs["ext_inst"]			= extInst ? string("%ext_import ") + testFunc.funcName : "";
	specs["struct_member"]		= de::toLower(testFunc.funcSuffix);

	variables					= StringTemplate(variables).specialize(specs);
	decorations					= StringTemplate(decorations).specialize(specs);
	funcVariables				= StringTemplate(argFragments->funcVariables).specialize(specs);
	funcCall					= StringTemplate(argFragments->bodies).specialize(specs);

	specs["num_data_points"]	= de::toString(iterations);
	specs["arg_vars"]			= variables;
	specs["arg_decorations"]	= decorations;
	specs["arg_infunc_vars"]	= funcVariables;
	specs["arg_func_call"]		= funcCall;

	fragments["extension"]		= "%ext_import = OpExtInstImport \"GLSL.std.450\"";
	fragments["capability"]		= "OpCapability Matrix\nOpCapability Float16\n";
	fragments["decoration"]		= decoration.specialize(specs);
	fragments["pre_main"]		= preMain.specialize(specs) + functions;
	fragments["testfun"]		= testFun.specialize(specs);

	for (size_t inputArgNdx = 0; inputArgNdx < testFunc.funcArgsCount; ++inputArgNdx)
	{
		const size_t			numFloatsPerItem	= (inputArgNdx == 0) ? testTypes[testFunc.typeArg0].typeArrayStride / sizeof(deFloat16)
													: (inputArgNdx == 1) ? testTypes[testFunc.typeArg1].typeArrayStride / sizeof(deFloat16)
													: (inputArgNdx == 2) ? testTypes[testFunc.typeArg2].typeArrayStride / sizeof(deFloat16)
													: -1;
		const vector<deFloat16>	inputData			= testFunc.getInputDataFunc(seed, numFloatsPerItem * iterations, testTypeIdx, numFloatsPerItem, testFunc.funcArgsCount, inputArgNdx);

		specResource.inputs.push_back(Resource(BufferSp(new Float16Buffer(inputData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	}

	specResource.outputs.push_back(Resource(BufferSp(new Float16Buffer(float16DummyOutput)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	specResource.verifyIO = testFunc.verifyFunc;

	extensions.push_back("VK_KHR_shader_float16_int8");

	features.extFloat16Int8		= EXTFLOAT16INT8FEATURES_FLOAT16;

	finalizeTestsCreation(specResource, fragments, testCtx, testGroup, testName, features, extensions, IVec3(1, 1, 1));
}

template<size_t C, class SpecResource>
tcu::TestCaseGroup* createFloat16ArithmeticSet (tcu::TestContext& testCtx)
{
	DE_STATIC_ASSERT(C >= 1 && C <= 4);

	const std::string				testGroupName	(string("arithmetic_") + de::toString(C));
	de::MovePtr<tcu::TestCaseGroup>	testGroup		(new tcu::TestCaseGroup(testCtx, testGroupName.c_str(), "Float 16 arithmetic and related tests"));
	const Math16TestFunc			testFuncs[]		=
	{
		{	"OpFNegate",			"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16OpFNegate>					},
		{	"Round",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Round>						},
		{	"RoundEven",			"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16RoundEven>					},
		{	"Trunc",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Trunc>						},
		{	"FAbs",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FAbs>						},
		{	"FSign",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FSign>						},
		{	"Floor",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Floor>						},
		{	"Ceil",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Ceil>						},
		{	"Fract",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Fract>						},
		{	"Radians",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Radians>						},
		{	"Degrees",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Degrees>						},
		{	"Sin",					"",			1,	C,		C,		0,		0, &getInputDataPI,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Sin>							},
		{	"Cos",					"",			1,	C,		C,		0,		0, &getInputDataPI,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Cos>							},
		{	"Tan",					"",			1,	C,		C,		0,		0, &getInputDataPI,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Tan>							},
		{	"Asin",					"",			1,	C,		C,		0,		0, &getInputDataA,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Asin>						},
		{	"Acos",					"",			1,	C,		C,		0,		0, &getInputDataA,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Acos>						},
		{	"Atan",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Atan>						},
		{	"Sinh",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Sinh>						},
		{	"Cosh",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Cosh>						},
		{	"Tanh",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Tanh>						},
		{	"Asinh",				"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Asinh>						},
		{	"Acosh",				"",			1,	C,		C,		0,		0, &getInputDataAC,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Acosh>						},
		{	"Atanh",				"",			1,	C,		C,		0,		0, &getInputDataA,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Atanh>						},
		{	"Exp",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Exp>							},
		{	"Log",					"",			1,	C,		C,		0,		0, &getInputDataP,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Log>							},
		{	"Exp2",					"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Exp2>						},
		{	"Log2",					"",			1,	C,		C,		0,		0, &getInputDataP,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Log2>						},
		{	"Sqrt",					"",			1,	C,		C,		0,		0, &getInputDataP,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Sqrt>						},
		{	"InverseSqrt",			"",			1,	C,		C,		0,		0, &getInputDataP,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16InverseSqrt>					},
		{	"Modf",					"Frac",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16ModfFrac>					},
		{	"Modf",					"Int",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16ModfInt>						},
		{	"ModfStruct",			"Frac",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16ModfFrac>					},
		{	"ModfStruct",			"Int",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16ModfInt>						},
		{	"Frexp",				"S",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FrexpS>						},
		{	"Frexp",				"E",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FrexpE>						},
		{	"FrexpStruct",			"S",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FrexpS>						},
		{	"FrexpStruct",			"E",		1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16FrexpE>						},
		{	"OpFAdd",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16OpFAdd>						},
		{	"OpFSub",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16OpFSub>						},
		{	"OpFMul",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16OpFMul>						},
		{	"OpFDiv",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16OpFDiv>						},
		{	"Atan2",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Atan2>						},
		{	"Pow",					"",			2,	C,		C,		C,		0, &getInputDataP,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Pow>							},
		{	"FMin",					"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16FMin>						},
		{	"FMax",					"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16FMax>						},
		{	"Step",					"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Step>						},
		{	"Ldexp",				"",			2,	C,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Ldexp>						},
		{	"FClamp",				"",			3,	C,		C,		C,		C, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  C, fp16FClamp>						},
		{	"FMix",					"",			3,	C,		C,		C,		C, &getInputDataD,	compareFP16ArithmeticFunc<  C,  C,  C,  C, fp16FMix>						},
		{	"SmoothStep",			"",			3,	C,		C,		C,		C, &getInputDataSS,	compareFP16ArithmeticFunc<  C,  C,  C,  C, fp16SmoothStep>					},
		{	"Fma",					"",			3,	C,		C,		C,		C, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  C,  C, fp16Fma>							},
		{	"Length",				"",			1,	1,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  1,  C,  0,  0, fp16Length>						},
		{	"Distance",				"",			2,	1,		C,		C,		0, &getInputData,	compareFP16ArithmeticFunc<  1,  C,  C,  0, fp16Distance>					},
		{	"Cross",				"",			2,	C,		C,		C,		0, &getInputDataD,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Cross>						},
		{	"Normalize",			"",			1,	C,		C,		0,		0, &getInputData,	compareFP16ArithmeticFunc<  C,  C,  0,  0, fp16Normalize>					},
		{	"FaceForward",			"",			3,	C,		C,		C,		C, &getInputDataD,	compareFP16ArithmeticFunc<  C,  C,  C,  C, fp16FaceForward>					},
		{	"Reflect",				"",			2,	C,		C,		C,		0, &getInputDataD,	compareFP16ArithmeticFunc<  C,  C,  C,  0, fp16Reflect>						},
		{	"Refract",				"",			3,	C,		C,		C,		1, &getInputDataN,	compareFP16ArithmeticFunc<  C,  C,  C,  1, fp16Refract>						},
		{	"OpDot",				"",			2,	1,		C,		C,		0, &getInputDataD,	compareFP16ArithmeticFunc<  1,  C,  C,  0, fp16Dot>							},
		{	"OpVectorTimesScalar",	"",			2,	C,		C,		1,		0, &getInputDataV,	compareFP16ArithmeticFunc<  C,  C,  1,  0, fp16VectorTimesScalar>			},
	};

	for (deUint32 testFuncIdx = 0; testFuncIdx < DE_LENGTH_OF_ARRAY(testFuncs); ++testFuncIdx)
	{
		const Math16TestFunc&	testFunc		= testFuncs[testFuncIdx];
		const string			funcNameString	= testFunc.funcName;

		if ((C != 3) && funcNameString == "Cross")
			continue;

		if ((C < 2) && funcNameString == "OpDot")
			continue;

		if ((C < 2) && funcNameString == "OpVectorTimesScalar")
			continue;

		createFloat16ArithmeticFuncTest<SpecResource>(testCtx, *testGroup.get(), C, testFunc);
	}

	return testGroup.release();
}

template<class SpecResource>
tcu::TestCaseGroup* createFloat16ArithmeticSet (tcu::TestContext& testCtx)
{
	const std::string				testGroupName	("arithmetic");
	de::MovePtr<tcu::TestCaseGroup>	testGroup		(new tcu::TestCaseGroup(testCtx, testGroupName.c_str(), "Float 16 arithmetic and related tests"));
	const Math16TestFunc			testFuncs[]		=
	{
		{	"OpTranspose",			"2x2",		1,	MAT2X2,	MAT2X2,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc<  4,  4,  0,  0, fp16Transpose<2,2> >				},
		{	"OpTranspose",			"3x2",		1,	MAT2X3,	MAT3X2,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc<  8,  8,  0,  0, fp16Transpose<3,2> >				},
		{	"OpTranspose",			"4x2",		1,	MAT2X4,	MAT4X2,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc<  8,  8,  0,  0, fp16Transpose<4,2> >				},
		{	"OpTranspose",			"2x3",		1,	MAT3X2,	MAT2X3,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc<  8,  8,  0,  0, fp16Transpose<2,3> >				},
		{	"OpTranspose",			"3x3",		1,	MAT3X3,	MAT3X3,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc< 16, 16,  0,  0, fp16Transpose<3,3> >				},
		{	"OpTranspose",			"4x3",		1,	MAT3X4,	MAT4X3,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc< 16, 16,  0,  0, fp16Transpose<4,3> >				},
		{	"OpTranspose",			"2x4",		1,	MAT4X2,	MAT2X4,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc<  8,  8,  0,  0, fp16Transpose<2,4> >				},
		{	"OpTranspose",			"3x4",		1,	MAT4X3,	MAT3X4,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc< 16, 16,  0,  0, fp16Transpose<3,4> >				},
		{	"OpTranspose",			"4x4",		1,	MAT4X4,	MAT4X4,	0,		0, &getInputDataM,	compareFP16ArithmeticFunc< 16, 16,  0,  0, fp16Transpose<4,4> >				},
		{	"OpMatrixTimesScalar",	"2x2",		2,	MAT2X2,	MAT2X2,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  4,  1,  0, fp16MatrixTimesScalar<2,2> >		},
		{	"OpMatrixTimesScalar",	"2x3",		2,	MAT2X3,	MAT2X3,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  1,  0, fp16MatrixTimesScalar<2,3> >		},
		{	"OpMatrixTimesScalar",	"2x4",		2,	MAT2X4,	MAT2X4,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  1,  0, fp16MatrixTimesScalar<2,4> >		},
		{	"OpMatrixTimesScalar",	"3x2",		2,	MAT3X2,	MAT3X2,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  1,  0, fp16MatrixTimesScalar<3,2> >		},
		{	"OpMatrixTimesScalar",	"3x3",		2,	MAT3X3,	MAT3X3,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16,  1,  0, fp16MatrixTimesScalar<3,3> >		},
		{	"OpMatrixTimesScalar",	"3x4",		2,	MAT3X4,	MAT3X4,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16,  1,  0, fp16MatrixTimesScalar<3,4> >		},
		{	"OpMatrixTimesScalar",	"4x2",		2,	MAT4X2,	MAT4X2,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  1,  0, fp16MatrixTimesScalar<4,2> >		},
		{	"OpMatrixTimesScalar",	"4x3",		2,	MAT4X3,	MAT4X3,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16,  1,  0, fp16MatrixTimesScalar<4,3> >		},
		{	"OpMatrixTimesScalar",	"4x4",		2,	MAT4X4,	MAT4X4,	1,		0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16,  1,  0, fp16MatrixTimesScalar<4,4> >		},
		{	"OpVectorTimesMatrix",	"2x2",		2,	VEC2,	VEC2,	MAT2X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  2,  4,  0, fp16VectorTimesMatrix<2,2> >		},
		{	"OpVectorTimesMatrix",	"2x3",		2,	VEC2,	VEC3,	MAT2X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  3,  8,  0, fp16VectorTimesMatrix<2,3> >		},
		{	"OpVectorTimesMatrix",	"2x4",		2,	VEC2,	VEC4,	MAT2X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  4,  8,  0, fp16VectorTimesMatrix<2,4> >		},
		{	"OpVectorTimesMatrix",	"3x2",		2,	VEC3,	VEC2,	MAT3X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3,  2,  8,  0, fp16VectorTimesMatrix<3,2> >		},
		{	"OpVectorTimesMatrix",	"3x3",		2,	VEC3,	VEC3,	MAT3X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3,  3, 16,  0, fp16VectorTimesMatrix<3,3> >		},
		{	"OpVectorTimesMatrix",	"3x4",		2,	VEC3,	VEC4,	MAT3X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3,  4, 16,  0, fp16VectorTimesMatrix<3,4> >		},
		{	"OpVectorTimesMatrix",	"4x2",		2,	VEC4,	VEC2,	MAT4X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  2,  8,  0, fp16VectorTimesMatrix<4,2> >		},
		{	"OpVectorTimesMatrix",	"4x3",		2,	VEC4,	VEC3,	MAT4X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  3, 16,  0, fp16VectorTimesMatrix<4,3> >		},
		{	"OpVectorTimesMatrix",	"4x4",		2,	VEC4,	VEC4,	MAT4X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  4, 16,  0, fp16VectorTimesMatrix<4,4> >		},
		{	"OpMatrixTimesVector",	"2x2",		2,	VEC2,	MAT2X2,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  4,  2,  0, fp16MatrixTimesVector<2,2> >		},
		{	"OpMatrixTimesVector",	"2x3",		2,	VEC3,	MAT2X3,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3,  8,  2,  0, fp16MatrixTimesVector<2,3> >		},
		{	"OpMatrixTimesVector",	"2x4",		2,	VEC4,	MAT2X4,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  8,  2,  0, fp16MatrixTimesVector<2,4> >		},
		{	"OpMatrixTimesVector",	"3x2",		2,	VEC2,	MAT3X2,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  8,  3,  0, fp16MatrixTimesVector<3,2> >		},
		{	"OpMatrixTimesVector",	"3x3",		2,	VEC3,	MAT3X3,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3, 16,  3,  0, fp16MatrixTimesVector<3,3> >		},
		{	"OpMatrixTimesVector",	"3x4",		2,	VEC4,	MAT3X4,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4, 16,  3,  0, fp16MatrixTimesVector<3,4> >		},
		{	"OpMatrixTimesVector",	"4x2",		2,	VEC2,	MAT4X2,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  2,  8,  4,  0, fp16MatrixTimesVector<4,2> >		},
		{	"OpMatrixTimesVector",	"4x3",		2,	VEC3,	MAT4X3,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  3, 16,  4,  0, fp16MatrixTimesVector<4,3> >		},
		{	"OpMatrixTimesVector",	"4x4",		2,	VEC4,	MAT4X4,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4, 16,  4,  0, fp16MatrixTimesVector<4,4> >		},
		{	"OpMatrixTimesMatrix",	"2x2_2x2",	2,	MAT2X2,	MAT2X2,	MAT2X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  4,  4,  0, fp16MatrixTimesMatrix<2,2,2,2> >	},
		{	"OpMatrixTimesMatrix",	"2x2_3x2",	2,	MAT3X2,	MAT2X2,	MAT3X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  4,  8,  0, fp16MatrixTimesMatrix<2,2,3,2> >	},
		{	"OpMatrixTimesMatrix",	"2x2_4x2",	2,	MAT4X2,	MAT2X2,	MAT4X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  4,  8,  0, fp16MatrixTimesMatrix<2,2,4,2> >	},
		{	"OpMatrixTimesMatrix",	"2x3_2x2",	2,	MAT2X3,	MAT2X3,	MAT2X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  4,  0, fp16MatrixTimesMatrix<2,3,2,2> >	},
		{	"OpMatrixTimesMatrix",	"2x3_3x2",	2,	MAT3X3,	MAT2X3,	MAT3X2,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  8,  8,  0, fp16MatrixTimesMatrix<2,3,3,2> >	},
		{	"OpMatrixTimesMatrix",	"2x3_4x2",	2,	MAT4X3,	MAT2X3,	MAT4X2,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  8,  8,  0, fp16MatrixTimesMatrix<2,3,4,2> >	},
		{	"OpMatrixTimesMatrix",	"2x4_2x2",	2,	MAT2X4,	MAT2X4,	MAT2X2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8,  4,  0, fp16MatrixTimesMatrix<2,4,2,2> >	},
		{	"OpMatrixTimesMatrix",	"2x4_3x2",	2,	MAT3X4,	MAT2X4,	MAT3X2,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  8,  8,  0, fp16MatrixTimesMatrix<2,4,3,2> >	},
		{	"OpMatrixTimesMatrix",	"2x4_4x2",	2,	MAT4X4,	MAT2X4,	MAT4X2,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  8,  8,  0, fp16MatrixTimesMatrix<2,4,4,2> >	},
		{	"OpMatrixTimesMatrix",	"3x2_2x3",	2,	MAT2X2,	MAT3X2,	MAT2X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  8,  8,  0, fp16MatrixTimesMatrix<3,2,2,3> >	},
		{	"OpMatrixTimesMatrix",	"3x2_3x3",	2,	MAT3X2,	MAT3X2,	MAT3X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8, 16,  0, fp16MatrixTimesMatrix<3,2,3,3> >	},
		{	"OpMatrixTimesMatrix",	"3x2_4x3",	2,	MAT4X2,	MAT3X2,	MAT4X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8, 16,  0, fp16MatrixTimesMatrix<3,2,4,3> >	},
		{	"OpMatrixTimesMatrix",	"3x3_2x3",	2,	MAT2X3,	MAT3X3,	MAT2X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8, 16,  8,  0, fp16MatrixTimesMatrix<3,3,2,3> >	},
		{	"OpMatrixTimesMatrix",	"3x3_3x3",	2,	MAT3X3,	MAT3X3,	MAT3X3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<3,3,3,3> >	},
		{	"OpMatrixTimesMatrix",	"3x3_4x3",	2,	MAT4X3,	MAT3X3,	MAT4X3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<3,3,4,3> >	},
		{	"OpMatrixTimesMatrix",	"3x4_2x3",	2,	MAT2X4,	MAT3X4,	MAT2X3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8, 16,  8,  0, fp16MatrixTimesMatrix<3,4,2,3> >	},
		{	"OpMatrixTimesMatrix",	"3x4_3x3",	2,	MAT3X4,	MAT3X4,	MAT3X3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<3,4,3,3> >	},
		{	"OpMatrixTimesMatrix",	"3x4_4x3",	2,	MAT4X4,	MAT3X4,	MAT4X3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<3,4,4,3> >	},
		{	"OpMatrixTimesMatrix",	"4x2_2x4",	2,	MAT2X2,	MAT4X2,	MAT2X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  8,  8,  0, fp16MatrixTimesMatrix<4,2,2,4> >	},
		{	"OpMatrixTimesMatrix",	"4x2_3x4",	2,	MAT3X2,	MAT4X2,	MAT3X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8, 16,  0, fp16MatrixTimesMatrix<4,2,3,4> >	},
		{	"OpMatrixTimesMatrix",	"4x2_4x4",	2,	MAT4X2,	MAT4X2,	MAT4X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  8, 16,  0, fp16MatrixTimesMatrix<4,2,4,4> >	},
		{	"OpMatrixTimesMatrix",	"4x3_2x4",	2,	MAT2X3,	MAT4X3,	MAT2X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8, 16,  8,  0, fp16MatrixTimesMatrix<4,3,2,4> >	},
		{	"OpMatrixTimesMatrix",	"4x3_3x4",	2,	MAT3X3,	MAT4X3,	MAT3X4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<4,3,3,4> >	},
		{	"OpMatrixTimesMatrix",	"4x3_4x4",	2,	MAT4X3,	MAT4X3,	MAT4X4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<4,3,4,4> >	},
		{	"OpMatrixTimesMatrix",	"4x4_2x4",	2,	MAT2X4,	MAT4X4,	MAT2X4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8, 16,  8,  0, fp16MatrixTimesMatrix<4,4,2,4> >	},
		{	"OpMatrixTimesMatrix",	"4x4_3x4",	2,	MAT3X4,	MAT4X4,	MAT3X4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<4,4,3,4> >	},
		{	"OpMatrixTimesMatrix",	"4x4_4x4",	2,	MAT4X4,	MAT4X4,	MAT4X4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16, 16, 16,  0, fp16MatrixTimesMatrix<4,4,4,4> >	},
		{	"OpOuterProduct",		"2x2",		2,	MAT2X2,	VEC2,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  4,  2,  2,  0, fp16OuterProduct<2,2> >			},
		{	"OpOuterProduct",		"2x3",		2,	MAT2X3,	VEC3,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  3,  2,  0, fp16OuterProduct<2,3> >			},
		{	"OpOuterProduct",		"2x4",		2,	MAT2X4,	VEC4,	VEC2,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  4,  2,  0, fp16OuterProduct<2,4> >			},
		{	"OpOuterProduct",		"3x2",		2,	MAT3X2,	VEC2,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  2,  3,  0, fp16OuterProduct<3,2> >			},
		{	"OpOuterProduct",		"3x3",		2,	MAT3X3,	VEC3,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  3,  3,  0, fp16OuterProduct<3,3> >			},
		{	"OpOuterProduct",		"3x4",		2,	MAT3X4,	VEC4,	VEC3,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  4,  3,  0, fp16OuterProduct<3,4> >			},
		{	"OpOuterProduct",		"4x2",		2,	MAT4X2,	VEC2,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc<  8,  2,  4,  0, fp16OuterProduct<4,2> >			},
		{	"OpOuterProduct",		"4x3",		2,	MAT4X3,	VEC3,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  3,  4,  0, fp16OuterProduct<4,3> >			},
		{	"OpOuterProduct",		"4x4",		2,	MAT4X4,	VEC4,	VEC4,	0, &getInputDataD,	compareFP16ArithmeticFunc< 16,  4,  4,  0, fp16OuterProduct<4,4> >			},
		{	"Determinant",			"2x2",		1,	SCALAR,	MAT2X2,	NONE,	0, &getInputDataC,	compareFP16ArithmeticFunc<  1,  4,  0,  0, fp16Determinant<2> >				},
		{	"Determinant",			"3x3",		1,	SCALAR,	MAT3X3,	NONE,	0, &getInputDataC,	compareFP16ArithmeticFunc<  1, 16,  0,  0, fp16Determinant<3> >				},
		{	"Determinant",			"4x4",		1,	SCALAR,	MAT4X4,	NONE,	0, &getInputDataC,	compareFP16ArithmeticFunc<  1, 16,  0,  0, fp16Determinant<4> >				},
		{	"MatrixInverse",		"2x2",		1,	MAT2X2,	MAT2X2,	NONE,	0, &getInputDataC,	compareFP16ArithmeticFunc<  4,  4,  0,  0, fp16Inverse<2> >					},
	};

	for (deUint32 testFuncIdx = 0; testFuncIdx < DE_LENGTH_OF_ARRAY(testFuncs); ++testFuncIdx)
	{
		const Math16TestFunc&	testFunc	= testFuncs[testFuncIdx];

		createFloat16ArithmeticFuncTest<SpecResource>(testCtx, *testGroup.get(), 0, testFunc);
	}

	return testGroup.release();
}

struct ComparisonCase
{
	string name;
	string desc;
};

template<size_t C>
tcu::TestCaseGroup* createFloat32ComparisonComputeSet (tcu::TestContext& testCtx)
{
	const string					testGroupName	("comparison_" + de::toString(C));
	de::MovePtr<tcu::TestCaseGroup>	testGroup		(new tcu::TestCaseGroup(testCtx, testGroupName.c_str(), "Float 32 comparison tests"));
	const char*						dataDir			= "spirv_assembly/instruction/float32/comparison";

	const ComparisonCase			amberTests[]	=
	{
		{ "modfstruct",		"modf and modfStruct"	},
		{ "frexpstruct",	"frexp and frexpStruct"	}
	};

	for (ComparisonCase test : amberTests)
	{
		const string caseDesc ("Compare output of " + test.desc);
		const string fileName (test.name + "_" + de::toString(C) + "_comp.amber");

		testGroup->addChild(cts_amber::createAmberTestCase(testCtx,
														   test.name.c_str(),
														   caseDesc.c_str(),
														   dataDir,
														   fileName));
	}

	return testGroup.release();
}

struct ShaderStage
{
	string			name;
	vector<string>	requirement;
};

template<size_t C>
tcu::TestCaseGroup* createFloat32ComparisonGraphicsSet (tcu::TestContext& testCtx)
{
	const string					testGroupName	("comparison_" + de::toString(C));
	de::MovePtr<tcu::TestCaseGroup>	testGroup		(new tcu::TestCaseGroup(testCtx, testGroupName.c_str(), "Float 32 comparison tests"));
	const char*						dataDir			= "spirv_assembly/instruction/float32/comparison";

	const ShaderStage				stages[]		=
	{
		{ "vert", vector<string>(0) },
		{ "tesc", vector<string>(1, "Features.tessellationShader") },
		{ "tese", vector<string>(1, "Features.tessellationShader") },
		{ "geom", vector<string>(1, "Features.geometryShader") },
		{ "frag", vector<string>(0) }
	};

	const ComparisonCase			amberTests[]	=
	{
		{ "modfstruct",		"modf and modfStruct"	},
		{ "frexpstruct",	"frexp and frexpStruct"	}
	};

	for (ComparisonCase test : amberTests)
	for (ShaderStage stage : stages)
	{
		const string caseName (test.name + "_" + stage.name);
		const string caseDesc ("Compare output of " + test.desc);
		const string fileName (test.name + "_" + de::toString(C) + "_" + stage.name + ".amber");

		testGroup->addChild(cts_amber::createAmberTestCase(testCtx,
														   caseName.c_str(),
														   caseDesc.c_str(),
														   dataDir,
														   fileName,
														   stage.requirement));
	}

	return testGroup.release();
}

const string getNumberTypeName (const NumberType type)
{
	if (type == NUMBERTYPE_INT32)
	{
		return "int";
	}
	else if (type == NUMBERTYPE_UINT32)
	{
		return "uint";
	}
	else if (type == NUMBERTYPE_FLOAT32)
	{
		return "float";
	}
	else
	{
		DE_ASSERT(false);
		return "";
	}
}

deInt32 getInt(de::Random& rnd)
{
	return rnd.getInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
}

const string repeatString (const string& str, int times)
{
	string filler;
	for (int i = 0; i < times; ++i)
	{
		filler += str;
	}
	return filler;
}

const string getRandomConstantString (const NumberType type, de::Random& rnd)
{
	if (type == NUMBERTYPE_INT32)
	{
		return numberToString<deInt32>(getInt(rnd));
	}
	else if (type == NUMBERTYPE_UINT32)
	{
		return numberToString<deUint32>(rnd.getUint32());
	}
	else if (type == NUMBERTYPE_FLOAT32)
	{
		return numberToString<float>(rnd.getFloat());
	}
	else
	{
		DE_ASSERT(false);
		return "";
	}
}

void createVectorCompositeCases (vector<map<string, string> >& testCases, de::Random& rnd, const NumberType type)
{
	map<string, string> params;

	// Vec2 to Vec4
	for (int width = 2; width <= 4; ++width)
	{
		const string randomConst = numberToString(getInt(rnd));
		const string widthStr = numberToString(width);
		const string composite_type = "${customType}vec" + widthStr;
		const int index = rnd.getInt(0, width-1);

		params["type"]			= "vec";
		params["name"]			= params["type"] + "_" + widthStr;
		params["compositeDecl"]		= composite_type + " = OpTypeVector ${customType} " + widthStr +"\n";
		params["compositeType"]		= composite_type;
		params["filler"]		= string("%filler    = OpConstant ${customType} ") + getRandomConstantString(type, rnd) + "\n";
		params["compositeConstruct"]	= "%instance  = OpCompositeConstruct " + composite_type + repeatString(" %filler", width) + "\n";
		params["indexes"]		= numberToString(index);
		testCases.push_back(params);
	}
}

void createArrayCompositeCases (vector<map<string, string> >& testCases, de::Random& rnd, const NumberType type)
{
	const int limit = 10;
	map<string, string> params;

	for (int width = 2; width <= limit; ++width)
	{
		string randomConst = numberToString(getInt(rnd));
		string widthStr = numberToString(width);
		int index = rnd.getInt(0, width-1);

		params["type"]			= "array";
		params["name"]			= params["type"] + "_" + widthStr;
		params["compositeDecl"]		= string("%arraywidth = OpConstant %u32 " + widthStr + "\n")
											+	 "%composite = OpTypeArray ${customType} %arraywidth\n";
		params["compositeType"]		= "%composite";
		params["filler"]		= string("%filler    = OpConstant ${customType} ") + getRandomConstantString(type, rnd) + "\n";
		params["compositeConstruct"]	= "%instance  = OpCompositeConstruct %composite" + repeatString(" %filler", width) + "\n";
		params["indexes"]		= numberToString(index);
		testCases.push_back(params);
	}
}

void createStructCompositeCases (vector<map<string, string> >& testCases, de::Random& rnd, const NumberType type)
{
	const int limit = 10;
	map<string, string> params;

	for (int width = 2; width <= limit; ++width)
	{
		string randomConst = numberToString(getInt(rnd));
		int index = rnd.getInt(0, width-1);

		params["type"]			= "struct";
		params["name"]			= params["type"] + "_" + numberToString(width);
		params["compositeDecl"]		= "%composite = OpTypeStruct" + repeatString(" ${customType}", width) + "\n";
		params["compositeType"]		= "%composite";
		params["filler"]		= string("%filler    = OpConstant ${customType} ") + getRandomConstantString(type, rnd) + "\n";
		params["compositeConstruct"]	= "%instance  = OpCompositeConstruct %composite" + repeatString(" %filler", width) + "\n";
		params["indexes"]		= numberToString(index);
		testCases.push_back(params);
	}
}

void createMatrixCompositeCases (vector<map<string, string> >& testCases, de::Random& rnd, const NumberType type)
{
	map<string, string> params;

	// Vec2 to Vec4
	for (int width = 2; width <= 4; ++width)
	{
		string widthStr = numberToString(width);

		for (int column = 2 ; column <= 4; ++column)
		{
			int index_0 = rnd.getInt(0, column-1);
			int index_1 = rnd.getInt(0, width-1);
			string columnStr = numberToString(column);

			params["type"]		= "matrix";
			params["name"]		= params["type"] + "_" + widthStr + "x" + columnStr;
			params["compositeDecl"]	= string("%vectype   = OpTypeVector ${customType} " + widthStr + "\n")
												+	 "%composite = OpTypeMatrix %vectype " + columnStr + "\n";
			params["compositeType"]	= "%composite";

			params["filler"]	= string("%filler    = OpConstant ${customType} ") + getRandomConstantString(type, rnd) + "\n"
												+	 "%fillerVec = OpConstantComposite %vectype" + repeatString(" %filler", width) + "\n";

			params["compositeConstruct"]	= "%instance  = OpCompositeConstruct %composite" + repeatString(" %fillerVec", column) + "\n";
			params["indexes"]	= numberToString(index_0) + " " + numberToString(index_1);
			testCases.push_back(params);
		}
	}
}

void createCompositeCases (vector<map<string, string> >& testCases, de::Random& rnd, const NumberType type)
{
	createVectorCompositeCases(testCases, rnd, type);
	createArrayCompositeCases(testCases, rnd, type);
	createStructCompositeCases(testCases, rnd, type);
	// Matrix only supports float types
	if (type == NUMBERTYPE_FLOAT32)
	{
		createMatrixCompositeCases(testCases, rnd, type);
	}
}

const string getAssemblyTypeDeclaration (const NumberType type)
{
	switch (type)
	{
		case NUMBERTYPE_INT32:		return "OpTypeInt 32 1";
		case NUMBERTYPE_UINT32:		return "OpTypeInt 32 0";
		case NUMBERTYPE_FLOAT32:	return "OpTypeFloat 32";
		default:			DE_ASSERT(false); return "";
	}
}

const string getAssemblyTypeName (const NumberType type)
{
	switch (type)
	{
		case NUMBERTYPE_INT32:		return "%i32";
		case NUMBERTYPE_UINT32:		return "%u32";
		case NUMBERTYPE_FLOAT32:	return "%f32";
		default:			DE_ASSERT(false); return "";
	}
}

const string specializeCompositeInsertShaderTemplate (const NumberType type, const map<string, string>& params)
{
	map<string, string>	parameters(params);

	const string customType = getAssemblyTypeName(type);
	map<string, string> substCustomType;
	substCustomType["customType"] = customType;
	parameters["compositeDecl"] = StringTemplate(parameters.at("compositeDecl")).specialize(substCustomType);
	parameters["compositeType"] = StringTemplate(parameters.at("compositeType")).specialize(substCustomType);
	parameters["compositeConstruct"] = StringTemplate(parameters.at("compositeConstruct")).specialize(substCustomType);
	parameters["filler"] = StringTemplate(parameters.at("filler")).specialize(substCustomType);
	parameters["customType"] = customType;
	parameters["compositeDecorator"] = (parameters["type"] == "array") ? "OpDecorate %composite ArrayStride 4\n" : "";

	if (parameters.at("compositeType") != "%u32vec3")
	{
		parameters["u32vec3Decl"] = "%u32vec3   = OpTypeVector %u32 3\n";
	}

	return StringTemplate(
		"OpCapability Shader\n"
		"OpCapability Matrix\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		// Decorators
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %customarr ArrayStride 4\n"
		"${compositeDecorator}"
		"OpMemberDecorate %buf 0 Offset 0\n"

		// General types
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%f32       = OpTypeFloat 32\n"

		// Composite declaration
		"${compositeDecl}"

		// Constants
		"${filler}"

		"${u32vec3Decl:opt}"
		"%uvec3ptr  = OpTypePointer Input %u32vec3\n"

		// Inherited from custom
		"%customptr = OpTypePointer Uniform ${customType}\n"
		"%customarr = OpTypeRuntimeArray ${customType}\n"
		"%buf       = OpTypeStruct %customarr\n"
		"%bufptr    = OpTypePointer Uniform %buf\n"

		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %i32 0\n"

		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %u32vec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"

		"%inloc     = OpAccessChain %customptr %indata %zero %x\n"
		"%outloc    = OpAccessChain %customptr %outdata %zero %x\n"
		// Read the input value
		"%inval     = OpLoad ${customType} %inloc\n"
		// Create the composite and fill it
		"${compositeConstruct}"
		// Insert the input value to a place
		"%instance2 = OpCompositeInsert ${compositeType} %inval %instance ${indexes}\n"
		// Read back the value from the position
		"%out_val   = OpCompositeExtract ${customType} %instance2 ${indexes}\n"
		// Store it in the output position
		"             OpStore %outloc %out_val\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n"
	).specialize(parameters);
}

template<typename T>
BufferSp createCompositeBuffer(T number)
{
	return BufferSp(new Buffer<T>(vector<T>(1, number)));
}

tcu::TestCaseGroup* createOpCompositeInsertGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "opcompositeinsert", "Test the OpCompositeInsert instruction"));
	de::Random						rnd		(deStringHash(group->getName()));

	for (int type = NUMBERTYPE_INT32; type != NUMBERTYPE_END32; ++type)
	{
		NumberType						numberType		= NumberType(type);
		const string					typeName		= getNumberTypeName(numberType);
		const string					description		= "Test the OpCompositeInsert instruction with " + typeName + "s";
		de::MovePtr<tcu::TestCaseGroup>	subGroup		(new tcu::TestCaseGroup(testCtx, typeName.c_str(), description.c_str()));
		vector<map<string, string> >	testCases;

		createCompositeCases(testCases, rnd, numberType);

		for (vector<map<string, string> >::const_iterator test = testCases.begin(); test != testCases.end(); ++test)
		{
			ComputeShaderSpec	spec;

			spec.assembly = specializeCompositeInsertShaderTemplate(numberType, *test);

			switch (numberType)
			{
				case NUMBERTYPE_INT32:
				{
					deInt32 number = getInt(rnd);
					spec.inputs.push_back(createCompositeBuffer<deInt32>(number));
					spec.outputs.push_back(createCompositeBuffer<deInt32>(number));
					break;
				}
				case NUMBERTYPE_UINT32:
				{
					deUint32 number = rnd.getUint32();
					spec.inputs.push_back(createCompositeBuffer<deUint32>(number));
					spec.outputs.push_back(createCompositeBuffer<deUint32>(number));
					break;
				}
				case NUMBERTYPE_FLOAT32:
				{
					float number = rnd.getFloat();
					spec.inputs.push_back(createCompositeBuffer<float>(number));
					spec.outputs.push_back(createCompositeBuffer<float>(number));
					break;
				}
				default:
					DE_ASSERT(false);
			}

			spec.numWorkGroups = IVec3(1, 1, 1);
			subGroup->addChild(new SpvAsmComputeShaderCase(testCtx, test->at("name").c_str(), "OpCompositeInsert test", spec));
		}
		group->addChild(subGroup.release());
	}
	return group.release();
}

struct AssemblyStructInfo
{
	AssemblyStructInfo (const deUint32 comp, const deUint32 idx)
	: components	(comp)
	, index			(idx)
	{}

	deUint32 components;
	deUint32 index;
};

const string specializeInBoundsShaderTemplate (const NumberType type, const AssemblyStructInfo& structInfo, const map<string, string>& params)
{
	// Create the full index string
	string				fullIndex	= numberToString(structInfo.index) + " " + params.at("indexes");
	// Convert it to list of indexes
	vector<string>		indexes		= de::splitString(fullIndex, ' ');

	map<string, string>	parameters	(params);
	parameters["structType"]	= repeatString(" ${compositeType}", structInfo.components);
	parameters["structConstruct"]	= repeatString(" %instance", structInfo.components);
	parameters["insertIndexes"]	= fullIndex;

	// In matrix cases the last two index is the CompositeExtract indexes
	const deUint32 extractIndexes = (parameters["type"] == "matrix") ? 2 : 1;

	// Construct the extractIndex
	for (vector<string>::const_iterator index = indexes.end() - extractIndexes; index != indexes.end(); ++index)
	{
		parameters["extractIndexes"] += " " + *index;
	}

	// Remove the last 1 or 2 element depends on matrix case or not
	indexes.erase(indexes.end() - extractIndexes, indexes.end());

	deUint32 id = 0;
	// Generate AccessChain index expressions (except for the last one, because we use ptr to the composite)
	for (vector<string>::const_iterator index = indexes.begin(); index != indexes.end(); ++index)
	{
		string indexId = "%index_" + numberToString(id++);
		parameters["accessChainConstDeclaration"] += indexId + "   = OpConstant %u32 " + *index + "\n";
		parameters["accessChainIndexes"] += " " + indexId;
	}

	parameters["compositeDecorator"] = (parameters["type"] == "array") ? "OpDecorate %composite ArrayStride 4\n" : "";

	const string customType = getAssemblyTypeName(type);
	map<string, string> substCustomType;
	substCustomType["customType"] = customType;
	parameters["compositeDecl"] = StringTemplate(parameters.at("compositeDecl")).specialize(substCustomType);
	parameters["compositeType"] = StringTemplate(parameters.at("compositeType")).specialize(substCustomType);
	parameters["compositeConstruct"] = StringTemplate(parameters.at("compositeConstruct")).specialize(substCustomType);
	parameters["filler"] = StringTemplate(parameters.at("filler")).specialize(substCustomType);
	parameters["customType"] = customType;

	const string compositeType = parameters.at("compositeType");
	map<string, string> substCompositeType;
	substCompositeType["compositeType"] = compositeType;
	parameters["structType"] = StringTemplate(parameters.at("structType")).specialize(substCompositeType);
	if (compositeType != "%u32vec3")
	{
		parameters["u32vec3Decl"] = "%u32vec3   = OpTypeVector %u32 3\n";
	}

	return StringTemplate(
		"OpCapability Shader\n"
		"OpCapability Matrix\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"
		// Decorators
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %customarr ArrayStride 4\n"
		"${compositeDecorator}"
		"OpMemberDecorate %buf 0 Offset 0\n"
		// General types
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%i32       = OpTypeInt 32 1\n"
		"%u32       = OpTypeInt 32 0\n"
		"%f32       = OpTypeFloat 32\n"
		// Custom types
		"${compositeDecl}"
		// %u32vec3 if not already declared in ${compositeDecl}
		"${u32vec3Decl:opt}"
		"%uvec3ptr  = OpTypePointer Input %u32vec3\n"
		// Inherited from composite
		"%composite_p = OpTypePointer Function ${compositeType}\n"
		"%struct_t  = OpTypeStruct${structType}\n"
		"%struct_p  = OpTypePointer Function %struct_t\n"
		// Constants
		"${filler}"
		"${accessChainConstDeclaration}"
		// Inherited from custom
		"%customptr = OpTypePointer Uniform ${customType}\n"
		"%customarr = OpTypeRuntimeArray ${customType}\n"
		"%buf       = OpTypeStruct %customarr\n"
		"%bufptr    = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n"

		"%id        = OpVariable %uvec3ptr Input\n"
		"%zero      = OpConstant %u32 0\n"
		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%struct_v  = OpVariable %struct_p Function\n"
		"%idval     = OpLoad %u32vec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		// Create the input/output type
		"%inloc     = OpInBoundsAccessChain %customptr %indata %zero %x\n"
		"%outloc    = OpInBoundsAccessChain %customptr %outdata %zero %x\n"
		// Read the input value
		"%inval     = OpLoad ${customType} %inloc\n"
		// Create the composite and fill it
		"${compositeConstruct}"
		// Create the struct and fill it with the composite
		"%struct    = OpCompositeConstruct %struct_t${structConstruct}\n"
		// Insert the value
		"%comp_obj  = OpCompositeInsert %struct_t %inval %struct ${insertIndexes}\n"
		// Store the object
		"             OpStore %struct_v %comp_obj\n"
		// Get deepest possible composite pointer
		"%inner_ptr = OpInBoundsAccessChain %composite_p %struct_v${accessChainIndexes}\n"
		"%read_obj  = OpLoad ${compositeType} %inner_ptr\n"
		// Read back the stored value
		"%read_val  = OpCompositeExtract ${customType} %read_obj${extractIndexes}\n"
		"             OpStore %outloc %read_val\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n"
	).specialize(parameters);
}

tcu::TestCaseGroup* createOpInBoundsAccessChainGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "opinboundsaccesschain", "Test the OpInBoundsAccessChain instruction"));
	de::Random						rnd				(deStringHash(group->getName()));

	for (int type = NUMBERTYPE_INT32; type != NUMBERTYPE_END32; ++type)
	{
		NumberType						numberType	= NumberType(type);
		const string					typeName	= getNumberTypeName(numberType);
		const string					description	= "Test the OpInBoundsAccessChain instruction with " + typeName + "s";
		de::MovePtr<tcu::TestCaseGroup>	subGroup	(new tcu::TestCaseGroup(testCtx, typeName.c_str(), description.c_str()));

		vector<map<string, string> >	testCases;
		createCompositeCases(testCases, rnd, numberType);

		for (vector<map<string, string> >::const_iterator test = testCases.begin(); test != testCases.end(); ++test)
		{
			ComputeShaderSpec	spec;

			// Number of components inside of a struct
			deUint32 structComponents = rnd.getInt(2, 8);
			// Component index value
			deUint32 structIndex = rnd.getInt(0, structComponents - 1);
			AssemblyStructInfo structInfo(structComponents, structIndex);

			spec.assembly = specializeInBoundsShaderTemplate(numberType, structInfo, *test);

			switch (numberType)
			{
				case NUMBERTYPE_INT32:
				{
					deInt32 number = getInt(rnd);
					spec.inputs.push_back(createCompositeBuffer<deInt32>(number));
					spec.outputs.push_back(createCompositeBuffer<deInt32>(number));
					break;
				}
				case NUMBERTYPE_UINT32:
				{
					deUint32 number = rnd.getUint32();
					spec.inputs.push_back(createCompositeBuffer<deUint32>(number));
					spec.outputs.push_back(createCompositeBuffer<deUint32>(number));
					break;
				}
				case NUMBERTYPE_FLOAT32:
				{
					float number = rnd.getFloat();
					spec.inputs.push_back(createCompositeBuffer<float>(number));
					spec.outputs.push_back(createCompositeBuffer<float>(number));
					break;
				}
				default:
					DE_ASSERT(false);
			}
			spec.numWorkGroups = IVec3(1, 1, 1);
			subGroup->addChild(new SpvAsmComputeShaderCase(testCtx, test->at("name").c_str(), "OpInBoundsAccessChain test", spec));
		}
		group->addChild(subGroup.release());
	}
	return group.release();
}

// If the params missing, uninitialized case
const string specializeDefaultOutputShaderTemplate (const NumberType type, const map<string, string>& params = map<string, string>())
{
	map<string, string> parameters(params);

	parameters["customType"]	= getAssemblyTypeName(type);

	// Declare the const value, and use it in the initializer
	if (params.find("constValue") != params.end())
	{
		parameters["variableInitializer"]	= " %const";
	}
	// Uninitialized case
	else
	{
		parameters["commentDecl"]	= ";";
	}

	return StringTemplate(
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"
		// Decorators
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %in_arr ArrayStride 4\n"
		"OpDecorate %in_buf BufferBlock\n"
		"OpMemberDecorate %in_buf 0 Offset 0\n"
		// Base types
		"%void       = OpTypeVoid\n"
		"%voidf      = OpTypeFunction %void\n"
		"%u32        = OpTypeInt 32 0\n"
		"%i32        = OpTypeInt 32 1\n"
		"%f32        = OpTypeFloat 32\n"
		"%uvec3      = OpTypeVector %u32 3\n"
		"%uvec3ptr   = OpTypePointer Input %uvec3\n"
		"${commentDecl:opt}%const      = OpConstant ${customType} ${constValue:opt}\n"
		// Derived types
		"%in_ptr     = OpTypePointer Uniform ${customType}\n"
		"%in_arr     = OpTypeRuntimeArray ${customType}\n"
		"%in_buf     = OpTypeStruct %in_arr\n"
		"%in_bufptr  = OpTypePointer Uniform %in_buf\n"
		"%indata     = OpVariable %in_bufptr Uniform\n"
		"%outdata    = OpVariable %in_bufptr Uniform\n"
		"%id         = OpVariable %uvec3ptr Input\n"
		"%var_ptr    = OpTypePointer Function ${customType}\n"
		// Constants
		"%zero       = OpConstant %i32 0\n"
		// Main function
		"%main       = OpFunction %void None %voidf\n"
		"%label      = OpLabel\n"
		"%out_var    = OpVariable %var_ptr Function${variableInitializer:opt}\n"
		"%idval      = OpLoad %uvec3 %id\n"
		"%x          = OpCompositeExtract %u32 %idval 0\n"
		"%inloc      = OpAccessChain %in_ptr %indata %zero %x\n"
		"%outloc     = OpAccessChain %in_ptr %outdata %zero %x\n"

		"%outval     = OpLoad ${customType} %out_var\n"
		"              OpStore %outloc %outval\n"
		"              OpReturn\n"
		"              OpFunctionEnd\n"
	).specialize(parameters);
}

bool compareFloats (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog& log)
{
	DE_ASSERT(outputAllocs.size() != 0);
	DE_ASSERT(outputAllocs.size() == expectedOutputs.size());

	// Use custom epsilon because of the float->string conversion
	const float	epsilon	= 0.00001f;

	for (size_t outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		vector<deUint8>	expectedBytes;
		float			expected;
		float			actual;

		expectedOutputs[outputNdx].getBytes(expectedBytes);
		memcpy(&expected, &expectedBytes.front(), expectedBytes.size());
		memcpy(&actual, outputAllocs[outputNdx]->getHostPtr(), expectedBytes.size());

		// Test with epsilon
		if (fabs(expected - actual) > epsilon)
		{
			log << TestLog::Message << "Error: The actual and expected values not matching."
				<< " Expected: " << expected << " Actual: " << actual << " Epsilon: " << epsilon << TestLog::EndMessage;
			return false;
		}
	}
	return true;
}

// Checks if the driver crash with uninitialized cases
bool passthruVerify (const std::vector<Resource>&, const vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, TestLog&)
{
	DE_ASSERT(outputAllocs.size() != 0);
	DE_ASSERT(outputAllocs.size() == expectedOutputs.size());

	// Copy and discard the result.
	for (size_t outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		vector<deUint8>	expectedBytes;
		expectedOutputs[outputNdx].getBytes(expectedBytes);

		const size_t	width			= expectedBytes.size();
		vector<char>	data			(width);

		memcpy(&data[0], outputAllocs[outputNdx]->getHostPtr(), width);
	}
	return true;
}

tcu::TestCaseGroup* createShaderDefaultOutputGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, "shader_default_output", "Test shader default output."));
	de::Random						rnd		(deStringHash(group->getName()));

	for (int type = NUMBERTYPE_INT32; type != NUMBERTYPE_END32; ++type)
	{
		NumberType						numberType	= NumberType(type);
		const string					typeName	= getNumberTypeName(numberType);
		const string					description	= "Test the OpVariable initializer with " + typeName + ".";
		de::MovePtr<tcu::TestCaseGroup>	subGroup	(new tcu::TestCaseGroup(testCtx, typeName.c_str(), description.c_str()));

		// 2 similar subcases (initialized and uninitialized)
		for (int subCase = 0; subCase < 2; ++subCase)
		{
			ComputeShaderSpec spec;
			spec.numWorkGroups = IVec3(1, 1, 1);

			map<string, string>				params;

			switch (numberType)
			{
				case NUMBERTYPE_INT32:
				{
					deInt32 number = getInt(rnd);
					spec.inputs.push_back(createCompositeBuffer<deInt32>(number));
					spec.outputs.push_back(createCompositeBuffer<deInt32>(number));
					params["constValue"] = numberToString(number);
					break;
				}
				case NUMBERTYPE_UINT32:
				{
					deUint32 number = rnd.getUint32();
					spec.inputs.push_back(createCompositeBuffer<deUint32>(number));
					spec.outputs.push_back(createCompositeBuffer<deUint32>(number));
					params["constValue"] = numberToString(number);
					break;
				}
				case NUMBERTYPE_FLOAT32:
				{
					float number = rnd.getFloat();
					spec.inputs.push_back(createCompositeBuffer<float>(number));
					spec.outputs.push_back(createCompositeBuffer<float>(number));
					spec.verifyIO = &compareFloats;
					params["constValue"] = numberToString(number);
					break;
				}
				default:
					DE_ASSERT(false);
			}

			// Initialized subcase
			if (!subCase)
			{
				spec.assembly = specializeDefaultOutputShaderTemplate(numberType, params);
				subGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "initialized", "OpVariable initializer tests.", spec));
			}
			// Uninitialized subcase
			else
			{
				spec.assembly = specializeDefaultOutputShaderTemplate(numberType);
				spec.verifyIO = &passthruVerify;
				subGroup->addChild(new SpvAsmComputeShaderCase(testCtx, "uninitialized", "OpVariable initializer tests.", spec));
			}
		}
		group->addChild(subGroup.release());
	}
	return group.release();
}

tcu::TestCaseGroup* createOpNopTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup (new tcu::TestCaseGroup(testCtx, "opnop", "Test OpNop"));
	RGBA							defaultColors[4];
	map<string, string>				opNopFragments;

	getDefaultColors(defaultColors);

	opNopFragments["testfun"]		=
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1 = OpFunctionParameter %v4f32\n"
		"%label_testfun = OpLabel\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"OpNop\n"
		"%a = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b = OpFAdd %f32 %a %a\n"
		"OpNop\n"
		"%c = OpFSub %f32 %b %a\n"
		"%ret = OpVectorInsertDynamic %v4f32 %param1 %c %c_i32_0\n"
		"OpNop\n"
		"OpNop\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	createTestsForAllStages("opnop", defaultColors, defaultColors, opNopFragments, testGroup.get());

	return testGroup.release();
}

tcu::TestCaseGroup* createOpNameTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, "opname","Test OpName"));
	RGBA							defaultColors[4];
	map<string, string>				opNameFragments;

	getDefaultColors(defaultColors);

	opNameFragments["testfun"] =
		"%test_code  = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1     = OpFunctionParameter %v4f32\n"
		"%label_func = OpLabel\n"
		"%a          = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b          = OpFAdd %f32 %a %a\n"
		"%c          = OpFSub %f32 %b %a\n"
		"%ret        = OpVectorInsertDynamic %v4f32 %param1 %c %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	opNameFragments["debug"] =
		"OpName %BP_main \"not_main\"";

	createTestsForAllStages("opname", defaultColors, defaultColors, opNameFragments, testGroup.get());

	return testGroup.release();
}

tcu::TestCaseGroup* createFloat16Tests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "float16", "Float 16 tests"));

	testGroup->addChild(createOpConstantFloat16Tests(testCtx));
	testGroup->addChild(createFloat16LogicalSet<GraphicsResources>(testCtx, TEST_WITH_NAN));
	testGroup->addChild(createFloat16LogicalSet<GraphicsResources>(testCtx, TEST_WITHOUT_NAN));
	testGroup->addChild(createFloat16FuncSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16VectorExtractSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16VectorInsertSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16VectorShuffleSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16CompositeConstructSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16CompositeInsertExtractSet<GraphicsResources>(testCtx, "OpCompositeExtract"));
	testGroup->addChild(createFloat16CompositeInsertExtractSet<GraphicsResources>(testCtx, "OpCompositeInsert"));
	testGroup->addChild(createFloat16ArithmeticSet<GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<1, GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<2, GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<3, GraphicsResources>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<4, GraphicsResources>(testCtx));

	return testGroup.release();
}

tcu::TestCaseGroup* createFloat32Tests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, "float32", "Float 32 tests"));

	testGroup->addChild(createFloat32ComparisonGraphicsSet<1>(testCtx));
	testGroup->addChild(createFloat32ComparisonGraphicsSet<2>(testCtx));
	testGroup->addChild(createFloat32ComparisonGraphicsSet<3>(testCtx));
	testGroup->addChild(createFloat32ComparisonGraphicsSet<4>(testCtx));

	return testGroup.release();
}

tcu::TestCaseGroup* createFloat16Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "float16", "Float 16 tests"));

	testGroup->addChild(createFloat16OpConstantCompositeGroup(testCtx));
	testGroup->addChild(createFloat16LogicalSet<ComputeShaderSpec>(testCtx, TEST_WITH_NAN));
	testGroup->addChild(createFloat16LogicalSet<ComputeShaderSpec>(testCtx, TEST_WITHOUT_NAN));
	testGroup->addChild(createFloat16FuncSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16VectorExtractSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16VectorInsertSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16VectorShuffleSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16CompositeConstructSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16CompositeInsertExtractSet<ComputeShaderSpec>(testCtx, "OpCompositeExtract"));
	testGroup->addChild(createFloat16CompositeInsertExtractSet<ComputeShaderSpec>(testCtx, "OpCompositeInsert"));
	testGroup->addChild(createFloat16ArithmeticSet<ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<1, ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<2, ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<3, ComputeShaderSpec>(testCtx));
	testGroup->addChild(createFloat16ArithmeticSet<4, ComputeShaderSpec>(testCtx));

	return testGroup.release();
}

tcu::TestCaseGroup* createFloat32Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, "float32", "Float 32 tests"));

	testGroup->addChild(createFloat32ComparisonComputeSet<1>(testCtx));
	testGroup->addChild(createFloat32ComparisonComputeSet<2>(testCtx));
	testGroup->addChild(createFloat32ComparisonComputeSet<3>(testCtx));
	testGroup->addChild(createFloat32ComparisonComputeSet<4>(testCtx));

	return testGroup.release();
}

tcu::TestCaseGroup* createBoolMixedBitSizeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, "mixed_bitsize", "Tests boolean operands produced from instructions of different bit-sizes"));

	de::Random						rnd				(deStringHash(group->getName()));
	const int		numElements		= 100;
	vector<float>	inputData		(numElements, 0);
	vector<float>	outputData		(numElements, 0);
	fillRandomScalars(rnd, 0.0f, 100.0f, &inputData[0], 100);

	const StringTemplate			shaderTemplate	(
		"${CAPS}\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"
		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		"OpDecorate %id BuiltIn GlobalInvocationId\n"

		+ string(getComputeAsmInputOutputBufferTraits()) + string(getComputeAsmCommonTypes()) + string(getComputeAsmInputOutputBuffer()) +

		"%id        = OpVariable %uvec3ptr Input\n"
		"${CONST}\n"
		"%main      = OpFunction %void None %voidf\n"
		"%label     = OpLabel\n"
		"%idval     = OpLoad %uvec3 %id\n"
		"%x         = OpCompositeExtract %u32 %idval 0\n"
		"%inloc     = OpAccessChain %f32ptr %indata %c0i32 %x\n"

		"${TEST}\n"

		"%outloc    = OpAccessChain %f32ptr %outdata %c0i32 %x\n"
		"             OpStore %outloc %res\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n"
	);

	// Each test case produces 4 boolean values, and we want each of these values
	// to come froma different combination of the available bit-sizes, so compute
	// all possible combinations here.
	vector<deUint32>	widths;
	widths.push_back(32);
	widths.push_back(16);
	widths.push_back(8);

	vector<IVec4>	cases;
	for (size_t width0 = 0; width0 < widths.size(); width0++)
	{
		for (size_t width1 = 0; width1 < widths.size(); width1++)
		{
			for (size_t width2 = 0; width2 < widths.size(); width2++)
			{
				for (size_t width3 = 0; width3 < widths.size(); width3++)
				{
					cases.push_back(IVec4(widths[width0], widths[width1], widths[width2], widths[width3]));
				}
			}
		}
	}

	for (size_t caseNdx = 0; caseNdx < cases.size(); caseNdx++)
	{
		/// Skip cases where all bitsizes are the same, we are only interested in testing booleans produced from instructions with different native bit-sizes
		if (cases[caseNdx][0] == cases[caseNdx][1] && cases[caseNdx][0] == cases[caseNdx][2] && cases[caseNdx][0] == cases[caseNdx][3])
			continue;

		map<string, string>	specializations;
		ComputeShaderSpec	spec;

		// Inject appropriate capabilities and reference constants depending
		// on the bit-sizes required by this test case
		bool hasFloat32	= cases[caseNdx][0] == 32 || cases[caseNdx][1] == 32 || cases[caseNdx][2] == 32 || cases[caseNdx][3] == 32;
		bool hasFloat16	= cases[caseNdx][0] == 16 || cases[caseNdx][1] == 16 || cases[caseNdx][2] == 16 || cases[caseNdx][3] == 16;
		bool hasInt8	= cases[caseNdx][0] == 8 || cases[caseNdx][1] == 8 || cases[caseNdx][2] == 8 || cases[caseNdx][3] == 8;

		string capsStr	= "OpCapability Shader\n";
		string constStr	=
			"%c0i32     = OpConstant %i32 0\n"
			"%c1f32     = OpConstant %f32 1.0\n"
			"%c0f32     = OpConstant %f32 0.0\n";

		if (hasFloat32)
		{
			constStr	+=
				"%c10f32    = OpConstant %f32 10.0\n"
				"%c25f32    = OpConstant %f32 25.0\n"
				"%c50f32    = OpConstant %f32 50.0\n"
				"%c90f32    = OpConstant %f32 90.0\n";
		}

		if (hasFloat16)
		{
			capsStr		+= "OpCapability Float16\n";
			constStr	+=
				"%f16       = OpTypeFloat 16\n"
				"%c10f16    = OpConstant %f16 10.0\n"
				"%c25f16    = OpConstant %f16 25.0\n"
				"%c50f16    = OpConstant %f16 50.0\n"
				"%c90f16    = OpConstant %f16 90.0\n";
		}

		if (hasInt8)
		{
			capsStr		+= "OpCapability Int8\n";
			constStr	+=
				"%i8        = OpTypeInt 8 1\n"
				"%c10i8     = OpConstant %i8 10\n"
				"%c25i8     = OpConstant %i8 25\n"
				"%c50i8     = OpConstant %i8 50\n"
				"%c90i8     = OpConstant %i8 90\n";
		}

		// Each invocation reads a different float32 value as input. Depending on
		// the bit-sizes required by the particular test case, we also produce
		// float16 and/or and int8 values by converting from the 32-bit float.
		string testStr	= "";
		testStr			+= "%inval32   = OpLoad %f32 %inloc\n";
		if (hasFloat16)
			testStr		+= "%inval16   = OpFConvert %f16 %inval32\n";
		if (hasInt8)
			testStr		+= "%inval8    = OpConvertFToS %i8 %inval32\n";

		// Because conversions from Float to Int round towards 0 we want our "greater" comparisons to be >=,
		// that way a float32/float16 comparison such as 50.6f >= 50.0f will preserve its result
		// when converted to int8, since FtoS(50.6f) results in 50. For "less" comparisons, it is the
		// other way around, so in this case we want < instead of <=.
		if (cases[caseNdx][0] == 32)
			testStr		+= "%cmp1      = OpFOrdGreaterThanEqual %bool %inval32 %c25f32\n";
		else if (cases[caseNdx][0] == 16)
			testStr		+= "%cmp1      = OpFOrdGreaterThanEqual %bool %inval16 %c25f16\n";
		else
			testStr		+= "%cmp1      = OpSGreaterThanEqual %bool %inval8 %c25i8\n";

		if (cases[caseNdx][1] == 32)
			testStr		+= "%cmp2      = OpFOrdLessThan %bool %inval32 %c50f32\n";
		else if (cases[caseNdx][1] == 16)
			testStr		+= "%cmp2      = OpFOrdLessThan %bool %inval16 %c50f16\n";
		else
			testStr		+= "%cmp2      = OpSLessThan %bool %inval8 %c50i8\n";

		if (cases[caseNdx][2] == 32)
			testStr		+= "%cmp3      = OpFOrdLessThan %bool %inval32 %c10f32\n";
		else if (cases[caseNdx][2] == 16)
			testStr		+= "%cmp3      = OpFOrdLessThan %bool %inval16 %c10f16\n";
		else
			testStr		+= "%cmp3      = OpSLessThan %bool %inval8 %c10i8\n";

		if (cases[caseNdx][3] == 32)
			testStr		+= "%cmp4      = OpFOrdGreaterThanEqual %bool %inval32 %c90f32\n";
		else if (cases[caseNdx][3] == 16)
			testStr		+= "%cmp4      = OpFOrdGreaterThanEqual %bool %inval16 %c90f16\n";
		else
			testStr		+= "%cmp4      = OpSGreaterThanEqual %bool %inval8 %c90i8\n";

		testStr			+= "%and1      = OpLogicalAnd %bool %cmp1 %cmp2\n";
		testStr			+= "%or1       = OpLogicalOr %bool %cmp3 %cmp4\n";
		testStr			+= "%or2       = OpLogicalOr %bool %and1 %or1\n";
		testStr			+= "%not1      = OpLogicalNot %bool %or2\n";
		testStr			+= "%res       = OpSelect %f32 %not1 %c1f32 %c0f32\n";

		specializations["CAPS"]		= capsStr;
		specializations["CONST"]	= constStr;
		specializations["TEST"]		= testStr;

		// Compute expected result by evaluating the boolean expression computed in the shader for each input value
		for (size_t ndx = 0; ndx < numElements; ++ndx)
			outputData[ndx] = !((inputData[ndx] >= 25.0f && inputData[ndx] < 50.0f) || (inputData[ndx] < 10.0f || inputData[ndx] >= 90.0f));

		spec.assembly = shaderTemplate.specialize(specializations);
		spec.inputs.push_back(BufferSp(new Float32Buffer(inputData)));
		spec.outputs.push_back(BufferSp(new Float32Buffer(outputData)));
		spec.numWorkGroups = IVec3(numElements, 1, 1);
		if (hasFloat16)
			spec.requestedVulkanFeatures.extFloat16Int8 |= EXTFLOAT16INT8FEATURES_FLOAT16;
		if (hasInt8)
			spec.requestedVulkanFeatures.extFloat16Int8 |= EXTFLOAT16INT8FEATURES_INT8;
		spec.extensions.push_back("VK_KHR_shader_float16_int8");

		string testName = "b" + de::toString(cases[caseNdx][0]) + "b" + de::toString(cases[caseNdx][1]) + "b" + de::toString(cases[caseNdx][2]) + "b" + de::toString(cases[caseNdx][3]);
		group->addChild(new SpvAsmComputeShaderCase(testCtx, testName.c_str(), "", spec));
	}

	return group.release();
}

tcu::TestCaseGroup* createBoolGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup			(new tcu::TestCaseGroup(testCtx, "bool", "Boolean tests"));

	testGroup->addChild(createBoolMixedBitSizeGroup(testCtx));

	return testGroup.release();
}

tcu::TestCaseGroup* createOpNameAbuseTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	abuseGroup(new tcu::TestCaseGroup(testCtx, "opname_abuse", "OpName abuse tests"));
	vector<CaseParameter>			abuseCases;
	RGBA							defaultColors[4];
	map<string, string>				opNameFragments;

	getOpNameAbuseCases(abuseCases);
	getDefaultColors(defaultColors);

	opNameFragments["testfun"] =
		"%test_code  = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1     = OpFunctionParameter %v4f32\n"
		"%label_func = OpLabel\n"
		"%a          = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b          = OpFAdd %f32 %a %a\n"
		"%c          = OpFSub %f32 %b %a\n"
		"%ret        = OpVectorInsertDynamic %v4f32 %param1 %c %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	for (unsigned int i = 0; i < abuseCases.size(); i++)
	{
		string casename;
		casename = string("main") + abuseCases[i].name;

		opNameFragments["debug"] =
			"OpName %BP_main \"" + abuseCases[i].param + "\"";

		createTestsForAllStages(casename, defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	for (unsigned int i = 0; i < abuseCases.size(); i++)
	{
		string casename;
		casename = string("b") + abuseCases[i].name;

		opNameFragments["debug"] =
			"OpName %b \"" + abuseCases[i].param + "\"";

		createTestsForAllStages(casename, defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	{
		opNameFragments["debug"] =
			"OpName %test_code \"name1\"\n"
			"OpName %param1    \"name2\"\n"
			"OpName %a         \"name3\"\n"
			"OpName %b         \"name4\"\n"
			"OpName %c         \"name5\"\n"
			"OpName %ret       \"name6\"\n";

		createTestsForAllStages("everything_named", defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	{
		opNameFragments["debug"] =
			"OpName %test_code \"the_same\"\n"
			"OpName %param1    \"the_same\"\n"
			"OpName %a         \"the_same\"\n"
			"OpName %b         \"the_same\"\n"
			"OpName %c         \"the_same\"\n"
			"OpName %ret       \"the_same\"\n";

		createTestsForAllStages("everything_named_the_same", defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	{
		opNameFragments["debug"] =
			"OpName %BP_main \"to_be\"\n"
			"OpName %BP_main \"or_not\"\n"
			"OpName %BP_main \"to_be\"\n";

		createTestsForAllStages("main_has_multiple_names", defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	{
		opNameFragments["debug"] =
			"OpName %b \"to_be\"\n"
			"OpName %b \"or_not\"\n"
			"OpName %b \"to_be\"\n";

		createTestsForAllStages("b_has_multiple_names", defaultColors, defaultColors, opNameFragments, abuseGroup.get());
	}

	return abuseGroup.release();
}


tcu::TestCaseGroup* createOpMemberNameAbuseTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	abuseGroup(new tcu::TestCaseGroup(testCtx, "opmembername_abuse", "OpName abuse tests"));
	vector<CaseParameter>			abuseCases;
	RGBA							defaultColors[4];
	map<string, string>				opMemberNameFragments;

	getOpNameAbuseCases(abuseCases);
	getDefaultColors(defaultColors);

	opMemberNameFragments["pre_main"] =
		"%f3str = OpTypeStruct %f32 %f32 %f32\n";

	opMemberNameFragments["testfun"] =
		"%test_code  = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"%param1     = OpFunctionParameter %v4f32\n"
		"%label_func = OpLabel\n"
		"%a          = OpVectorExtractDynamic %f32 %param1 %c_i32_0\n"
		"%b          = OpFAdd %f32 %a %a\n"
		"%c          = OpFSub %f32 %b %a\n"
		"%cstr       = OpCompositeConstruct %f3str %c %c %c\n"
		"%d          = OpCompositeExtract %f32 %cstr 0\n"
		"%ret        = OpVectorInsertDynamic %v4f32 %param1 %d %c_i32_0\n"
		"OpReturnValue %ret\n"
		"OpFunctionEnd\n";

	for (unsigned int i = 0; i < abuseCases.size(); i++)
	{
		string casename;
		casename = string("f3str_x") + abuseCases[i].name;

		opMemberNameFragments["debug"] =
			"OpMemberName %f3str 0 \"" + abuseCases[i].param + "\"";

		createTestsForAllStages(casename, defaultColors, defaultColors, opMemberNameFragments, abuseGroup.get());
	}

	{
		opMemberNameFragments["debug"] =
			"OpMemberName %f3str 0 \"name1\"\n"
			"OpMemberName %f3str 1 \"name2\"\n"
			"OpMemberName %f3str 2 \"name3\"\n";

		createTestsForAllStages("everything_named", defaultColors, defaultColors, opMemberNameFragments, abuseGroup.get());
	}

	{
		opMemberNameFragments["debug"] =
			"OpMemberName %f3str 0 \"the_same\"\n"
			"OpMemberName %f3str 1 \"the_same\"\n"
			"OpMemberName %f3str 2 \"the_same\"\n";

		createTestsForAllStages("everything_named_the_same", defaultColors, defaultColors, opMemberNameFragments, abuseGroup.get());
	}

	{
		opMemberNameFragments["debug"] =
			"OpMemberName %f3str 0 \"to_be\"\n"
			"OpMemberName %f3str 1 \"or_not\"\n"
			"OpMemberName %f3str 0 \"to_be\"\n"
			"OpMemberName %f3str 2 \"makes_no\"\n"
			"OpMemberName %f3str 0 \"difference\"\n"
			"OpMemberName %f3str 0 \"to_me\"\n";


		createTestsForAllStages("f3str_x_has_multiple_names", defaultColors, defaultColors, opMemberNameFragments, abuseGroup.get());
	}

	return abuseGroup.release();
}

vector<deUint32> getSparseIdsAbuseData (const deUint32 numDataPoints, const deUint32 seed)
{
	vector<deUint32>	result;
	de::Random			rnd		(seed);

	result.reserve(numDataPoints);

	for (deUint32 dataPointNdx = 0; dataPointNdx < numDataPoints; ++dataPointNdx)
		result.push_back(rnd.getUint32());

	return result;
}

vector<deUint32> getSparseIdsAbuseResults (const vector<deUint32>& inData1, const vector<deUint32>& inData2)
{
	vector<deUint32>	result;

	result.reserve(inData1.size());

	for (size_t dataPointNdx = 0; dataPointNdx < inData1.size(); ++dataPointNdx)
		result.push_back(inData1[dataPointNdx] + inData2[dataPointNdx]);

	return result;
}

template<class SpecResource>
void createSparseIdsAbuseTest (tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup>& testGroup)
{
	const deUint32			numDataPoints	= 16;
	const std::string		testName		("sparse_ids");
	const deUint32			seed			(deStringHash(testName.c_str()));
	const vector<deUint32>	inData1			(getSparseIdsAbuseData(numDataPoints, seed + 1));
	const vector<deUint32>	inData2			(getSparseIdsAbuseData(numDataPoints, seed + 2));
	const vector<deUint32>	outData			(getSparseIdsAbuseResults(inData1, inData2));
	const StringTemplate	preMain
	(
		"%c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		"   %up_u32 = OpTypePointer Uniform %u32\n"
		"   %ra_u32 = OpTypeArray %u32 %c_i32_ndp\n"
		"   %SSBO32 = OpTypeStruct %ra_u32\n"
		"%up_SSBO32 = OpTypePointer Uniform %SSBO32\n"
		"%ssbo_src0 = OpVariable %up_SSBO32 Uniform\n"
		"%ssbo_src1 = OpVariable %up_SSBO32 Uniform\n"
		" %ssbo_dst = OpVariable %up_SSBO32 Uniform\n"
	);
	const StringTemplate	decoration
	(
		"OpDecorate %ra_u32 ArrayStride 4\n"
		"OpMemberDecorate %SSBO32 0 Offset 0\n"
		"OpDecorate %SSBO32 BufferBlock\n"
		"OpDecorate %ssbo_src0 DescriptorSet 0\n"
		"OpDecorate %ssbo_src0 Binding 0\n"
		"OpDecorate %ssbo_src1 DescriptorSet 0\n"
		"OpDecorate %ssbo_src1 Binding 1\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 2\n"
	);
	const StringTemplate	testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"

		"    %entry = OpLabel\n"
		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"      %127 = OpAccessChain %up_u32 %ssbo_src0 %c_i32_0 %ndx\n"
		"      %128 = OpLoad %u32 %127\n"

		// The test relies on SPIR-V compiler option SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS set in assembleSpirV()
		"  %4194000 = OpAccessChain %up_u32 %ssbo_src1 %c_i32_0 %ndx\n"
		"  %4194001 = OpLoad %u32 %4194000\n"

		"  %2097151 = OpIAdd %u32 %128 %4194001\n"
		"  %2097152 = OpAccessChain %up_u32 %ssbo_dst %c_i32_0 %ndx\n"
		"             OpStore %2097152 %2097151\n"
		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpReturnValue %param\n"

		"             OpFunctionEnd\n"
	);
	SpecResource			specResource;
	map<string, string>		specs;
	VulkanFeatures			features;
	map<string, string>		fragments;
	vector<string>			extensions;

	specs["num_data_points"]	= de::toString(numDataPoints);

	fragments["decoration"]		= decoration.specialize(specs);
	fragments["pre_main"]		= preMain.specialize(specs);
	fragments["testfun"]		= testFun.specialize(specs);

	specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inData1)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inData2)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	specResource.outputs.push_back(Resource(BufferSp(new Uint32Buffer(outData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	features.coreFeatures.fragmentStoresAndAtomics			= true;

	finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
}

vector<deUint32> getLotsIdsAbuseData (const deUint32 numDataPoints, const deUint32 seed)
{
	vector<deUint32>	result;
	de::Random			rnd		(seed);

	result.reserve(numDataPoints);

	// Fixed value
	result.push_back(1u);

	// Random values
	for (deUint32 dataPointNdx = 1; dataPointNdx < numDataPoints; ++dataPointNdx)
		result.push_back(rnd.getUint8());

	return result;
}

vector<deUint32> getLotsIdsAbuseResults (const vector<deUint32>& inData1, const vector<deUint32>& inData2, const deUint32 count)
{
	vector<deUint32>	result;

	result.reserve(inData1.size());

	for (size_t dataPointNdx = 0; dataPointNdx < inData1.size(); ++dataPointNdx)
		result.push_back(inData1[dataPointNdx] + count * inData2[dataPointNdx]);

	return result;
}

template<class SpecResource>
void createLotsIdsAbuseTest (tcu::TestContext& testCtx, de::MovePtr<tcu::TestCaseGroup>& testGroup)
{
	const deUint32			numDataPoints	= 16;
	const deUint32			firstNdx		= 100u;
	const deUint32			sequenceCount	= 10000u;
	const std::string		testName		("lots_ids");
	const deUint32			seed			(deStringHash(testName.c_str()));
	const vector<deUint32>	inData1			(getLotsIdsAbuseData(numDataPoints, seed + 1));
	const vector<deUint32>	inData2			(getLotsIdsAbuseData(numDataPoints, seed + 2));
	const vector<deUint32>	outData			(getLotsIdsAbuseResults(inData1, inData2, sequenceCount));
	const StringTemplate preMain
	(
		"%c_i32_ndp = OpConstant %i32 ${num_data_points}\n"
		"   %up_u32 = OpTypePointer Uniform %u32\n"
		"   %ra_u32 = OpTypeArray %u32 %c_i32_ndp\n"
		"   %SSBO32 = OpTypeStruct %ra_u32\n"
		"%up_SSBO32 = OpTypePointer Uniform %SSBO32\n"
		"%ssbo_src0 = OpVariable %up_SSBO32 Uniform\n"
		"%ssbo_src1 = OpVariable %up_SSBO32 Uniform\n"
		" %ssbo_dst = OpVariable %up_SSBO32 Uniform\n"
	);
	const StringTemplate decoration
	(
		"OpDecorate %ra_u32 ArrayStride 4\n"
		"OpMemberDecorate %SSBO32 0 Offset 0\n"
		"OpDecorate %SSBO32 BufferBlock\n"
		"OpDecorate %ssbo_src0 DescriptorSet 0\n"
		"OpDecorate %ssbo_src0 Binding 0\n"
		"OpDecorate %ssbo_src1 DescriptorSet 0\n"
		"OpDecorate %ssbo_src1 Binding 1\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 2\n"
	);
	const StringTemplate testFun
	(
		"%test_code = OpFunction %v4f32 None %v4f32_v4f32_function\n"
		"    %param = OpFunctionParameter %v4f32\n"

		"    %entry = OpLabel\n"
		"        %i = OpVariable %fp_i32 Function\n"
		"             OpStore %i %c_i32_0\n"
		"             OpBranch %loop\n"

		"     %loop = OpLabel\n"
		"    %i_cmp = OpLoad %i32 %i\n"
		"       %lt = OpSLessThan %bool %i_cmp %c_i32_ndp\n"
		"             OpLoopMerge %merge %next None\n"
		"             OpBranchConditional %lt %write %merge\n"

		"    %write = OpLabel\n"
		"      %ndx = OpLoad %i32 %i\n"

		"       %90 = OpAccessChain %up_u32 %ssbo_src1 %c_i32_0 %ndx\n"
		"       %91 = OpLoad %u32 %90\n"

		"       %98 = OpAccessChain %up_u32 %ssbo_src0 %c_i32_0 %ndx\n"
		"       %${zeroth_id} = OpLoad %u32 %98\n"

		"${seq}\n"

		// The test relies on SPIR-V compiler option SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS set in assembleSpirV()
		"      %dst = OpAccessChain %up_u32 %ssbo_dst %c_i32_0 %ndx\n"
		"             OpStore %dst %${last_id}\n"
		"             OpBranch %next\n"

		"     %next = OpLabel\n"
		"    %i_cur = OpLoad %i32 %i\n"
		"    %i_new = OpIAdd %i32 %i_cur %c_i32_1\n"
		"             OpStore %i %i_new\n"
		"             OpBranch %loop\n"

		"    %merge = OpLabel\n"
		"             OpReturnValue %param\n"

		"             OpFunctionEnd\n"
	);
	deUint32				lastId			= firstNdx;
	SpecResource			specResource;
	map<string, string>		specs;
	VulkanFeatures			features;
	map<string, string>		fragments;
	vector<string>			extensions;
	std::string				sequence;

	for (deUint32 sequenceNdx = 0; sequenceNdx < sequenceCount; ++sequenceNdx)
	{
		const deUint32		sequenceId		= sequenceNdx + firstNdx;
		const std::string	sequenceIdStr	= de::toString(sequenceId);

		sequence += "%" + sequenceIdStr + " = OpIAdd %u32 %91 %" + de::toString(sequenceId - 1) + "\n";
		lastId = sequenceId;

		if (sequenceNdx == 0)
			sequence.reserve((10 + sequence.length()) * sequenceCount);
	}

	specs["num_data_points"]	= de::toString(numDataPoints);
	specs["zeroth_id"]			= de::toString(firstNdx - 1);
	specs["last_id"]			= de::toString(lastId);
	specs["seq"]				= sequence;

	fragments["decoration"]		= decoration.specialize(specs);
	fragments["pre_main"]		= preMain.specialize(specs);
	fragments["testfun"]		= testFun.specialize(specs);

	specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inData1)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	specResource.inputs.push_back(Resource(BufferSp(new Uint32Buffer(inData2)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
	specResource.outputs.push_back(Resource(BufferSp(new Uint32Buffer(outData)), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

	features.coreFeatures.vertexPipelineStoresAndAtomics	= true;
	features.coreFeatures.fragmentStoresAndAtomics			= true;

	finalizeTestsCreation(specResource, fragments, testCtx, *testGroup.get(), testName, features, extensions, IVec3(1, 1, 1));
}

tcu::TestCaseGroup* createSpirvIdsAbuseTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, "spirv_ids_abuse", "SPIR-V abuse tests"));

	createSparseIdsAbuseTest<GraphicsResources>(testCtx, testGroup);
	createLotsIdsAbuseTest<GraphicsResources>(testCtx, testGroup);

	return testGroup.release();
}

tcu::TestCaseGroup* createSpirvIdsAbuseGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, "spirv_ids_abuse", "SPIR-V abuse tests"));

	createSparseIdsAbuseTest<ComputeShaderSpec>(testCtx, testGroup);
	createLotsIdsAbuseTest<ComputeShaderSpec>(testCtx, testGroup);

	return testGroup.release();
}

tcu::TestCaseGroup* createFunctionParamsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup (new tcu::TestCaseGroup(testCtx, "function_params", "Function parameter tests"));

	static const char data_dir[] = "spirv_assembly/instruction/function_params";

	static const struct
	{
		const std::string name;
		const std::string desc;
	} cases[] =
	{
		{ "sampler_param", "Test combined image sampler as function parameter" },
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx,
																			cases[i].name.c_str(),
																			cases[i].desc.c_str(),
																			data_dir,
																			cases[i].name + ".amber");
		testGroup->addChild(testCase);
	}

	return testGroup.release();
}

tcu::TestCaseGroup* createInstructionTests (tcu::TestContext& testCtx)
{
	const bool testComputePipeline = true;

	de::MovePtr<tcu::TestCaseGroup> instructionTests	(new tcu::TestCaseGroup(testCtx, "instruction", "Instructions with special opcodes/operands"));
	de::MovePtr<tcu::TestCaseGroup> computeTests		(new tcu::TestCaseGroup(testCtx, "compute", "Compute Instructions with special opcodes/operands"));
	de::MovePtr<tcu::TestCaseGroup> graphicsTests		(new tcu::TestCaseGroup(testCtx, "graphics", "Graphics Instructions with special opcodes/operands"));

	computeTests->addChild(createSpivVersionCheckTests(testCtx, testComputePipeline));
	computeTests->addChild(createLocalSizeGroup(testCtx));
	computeTests->addChild(createNonSemanticInfoGroup(testCtx));
	computeTests->addChild(createOpNopGroup(testCtx));
	computeTests->addChild(createOpFUnordGroup(testCtx, TEST_WITHOUT_NAN));
	computeTests->addChild(createOpFUnordGroup(testCtx, TEST_WITH_NAN));
	computeTests->addChild(createOpAtomicGroup(testCtx, false));
	computeTests->addChild(createOpAtomicGroup(testCtx, true));					// Using new StorageBuffer decoration
	computeTests->addChild(createOpAtomicGroup(testCtx, false, 1024, true));	// Return value validation
	computeTests->addChild(createOpAtomicGroup(testCtx, true, 65535, false, true));	// volatile atomics
	computeTests->addChild(createOpLineGroup(testCtx));
	computeTests->addChild(createOpModuleProcessedGroup(testCtx));
	computeTests->addChild(createOpNoLineGroup(testCtx));
	computeTests->addChild(createOpConstantNullGroup(testCtx));
	computeTests->addChild(createOpConstantCompositeGroup(testCtx));
	computeTests->addChild(createOpConstantUsageGroup(testCtx));
	computeTests->addChild(createSpecConstantGroup(testCtx));
	computeTests->addChild(createOpSourceGroup(testCtx));
	computeTests->addChild(createOpSourceExtensionGroup(testCtx));
	computeTests->addChild(createDecorationGroupGroup(testCtx));
	computeTests->addChild(createOpPhiGroup(testCtx));
	computeTests->addChild(createLoopControlGroup(testCtx));
	computeTests->addChild(createFunctionControlGroup(testCtx));
	computeTests->addChild(createSelectionControlGroup(testCtx));
	computeTests->addChild(createBlockOrderGroup(testCtx));
	computeTests->addChild(createMultipleShaderGroup(testCtx));
	computeTests->addChild(createMemoryAccessGroup(testCtx));
	computeTests->addChild(createOpCopyMemoryGroup(testCtx));
	computeTests->addChild(createOpCopyObjectGroup(testCtx));
	computeTests->addChild(createNoContractionGroup(testCtx));
	computeTests->addChild(createOpUndefGroup(testCtx));
	computeTests->addChild(createOpUnreachableGroup(testCtx));
	computeTests->addChild(createOpQuantizeToF16Group(testCtx));
	computeTests->addChild(createOpFRemGroup(testCtx));
	computeTests->addChild(createOpSRemComputeGroup(testCtx, QP_TEST_RESULT_PASS));
	computeTests->addChild(createOpSRemComputeGroup64(testCtx, QP_TEST_RESULT_PASS));
	computeTests->addChild(createOpSModComputeGroup(testCtx, QP_TEST_RESULT_PASS));
	computeTests->addChild(createOpSModComputeGroup64(testCtx, QP_TEST_RESULT_PASS));
	computeTests->addChild(createOpSDotKHRComputeGroup(testCtx));
	computeTests->addChild(createOpUDotKHRComputeGroup(testCtx));
	computeTests->addChild(createOpSUDotKHRComputeGroup(testCtx));
	computeTests->addChild(createOpSDotAccSatKHRComputeGroup(testCtx));
	computeTests->addChild(createOpUDotAccSatKHRComputeGroup(testCtx));
	computeTests->addChild(createOpSUDotAccSatKHRComputeGroup(testCtx));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpSConvert", "sconvert"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpUConvert", "uconvert"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpFConvert", "fconvert"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpConvertSToF", "convertstof"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpConvertFToS", "convertftos"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpConvertUToF", "convertutof"));
	computeTests->addChild(createConvertComputeTests(testCtx, "OpConvertFToU", "convertftou"));
	computeTests->addChild(createOpCompositeInsertGroup(testCtx));
	computeTests->addChild(createOpInBoundsAccessChainGroup(testCtx));
	computeTests->addChild(createShaderDefaultOutputGroup(testCtx));
	computeTests->addChild(createOpNMinGroup(testCtx));
	computeTests->addChild(createOpNMaxGroup(testCtx));
	computeTests->addChild(createOpNClampGroup(testCtx));
	computeTests->addChild(createFloatControlsExtensionlessGroup(testCtx));
	{
		de::MovePtr<tcu::TestCaseGroup>	computeAndroidTests	(new tcu::TestCaseGroup(testCtx, "android", "Android CTS Tests"));

		computeAndroidTests->addChild(createOpSRemComputeGroup(testCtx, QP_TEST_RESULT_QUALITY_WARNING));
		computeAndroidTests->addChild(createOpSModComputeGroup(testCtx, QP_TEST_RESULT_QUALITY_WARNING));

		computeTests->addChild(computeAndroidTests.release());
	}

	computeTests->addChild(create8BitStorageComputeGroup(testCtx));
	computeTests->addChild(create16BitStorageComputeGroup(testCtx));
	computeTests->addChild(createFloatControlsComputeGroup(testCtx));
	computeTests->addChild(createUboMatrixPaddingComputeGroup(testCtx));
	computeTests->addChild(createCompositeInsertComputeGroup(testCtx));
	computeTests->addChild(createVariableInitComputeGroup(testCtx));
	computeTests->addChild(createConditionalBranchComputeGroup(testCtx));
	computeTests->addChild(createIndexingComputeGroup(testCtx));
	computeTests->addChild(createVariablePointersComputeGroup(testCtx));
	computeTests->addChild(createPhysicalPointersComputeGroup(testCtx));
	computeTests->addChild(createImageSamplerComputeGroup(testCtx));
	computeTests->addChild(createOpNameGroup(testCtx));
	computeTests->addChild(createOpMemberNameGroup(testCtx));
	computeTests->addChild(createPointerParameterComputeGroup(testCtx));
	computeTests->addChild(createFloat16Group(testCtx));
	computeTests->addChild(createFloat32Group(testCtx));
	computeTests->addChild(createBoolGroup(testCtx));
	computeTests->addChild(createWorkgroupMemoryComputeGroup(testCtx));
	computeTests->addChild(createSpirvIdsAbuseGroup(testCtx));
	computeTests->addChild(createSignedIntCompareGroup(testCtx));
	computeTests->addChild(createSignedOpTestsGroup(testCtx));
	computeTests->addChild(createUnusedVariableComputeTests(testCtx));
	computeTests->addChild(createPtrAccessChainGroup(testCtx));
	computeTests->addChild(createVectorShuffleGroup(testCtx));
	computeTests->addChild(createHlslComputeGroup(testCtx));
	computeTests->addChild(createEmptyStructComputeGroup(testCtx));
	computeTests->addChild(create64bitCompareComputeGroup(testCtx));
	computeTests->addChild(createOpArrayLengthComputeGroup(testCtx));

	graphicsTests->addChild(createCrossStageInterfaceTests(testCtx));
	graphicsTests->addChild(createSpivVersionCheckTests(testCtx, !testComputePipeline));
	graphicsTests->addChild(createOpNopTests(testCtx));
	graphicsTests->addChild(createOpSourceTests(testCtx));
	graphicsTests->addChild(createOpSourceContinuedTests(testCtx));
	graphicsTests->addChild(createOpModuleProcessedTests(testCtx));
	graphicsTests->addChild(createOpLineTests(testCtx));
	graphicsTests->addChild(createOpNoLineTests(testCtx));
	graphicsTests->addChild(createOpConstantNullTests(testCtx));
	graphicsTests->addChild(createOpConstantCompositeTests(testCtx));
	graphicsTests->addChild(createMemoryAccessTests(testCtx));
	graphicsTests->addChild(createOpUndefTests(testCtx));
	graphicsTests->addChild(createSelectionBlockOrderTests(testCtx));
	graphicsTests->addChild(createModuleTests(testCtx));
	graphicsTests->addChild(createUnusedVariableTests(testCtx));
	graphicsTests->addChild(createSwitchBlockOrderTests(testCtx));
	graphicsTests->addChild(createOpPhiTests(testCtx));
	graphicsTests->addChild(createNoContractionTests(testCtx));
	graphicsTests->addChild(createOpQuantizeTests(testCtx));
	graphicsTests->addChild(createLoopTests(testCtx));
	graphicsTests->addChild(createSpecConstantTests(testCtx));
	graphicsTests->addChild(createSpecConstantOpQuantizeToF16Group(testCtx));
	graphicsTests->addChild(createBarrierTests(testCtx));
	graphicsTests->addChild(createDecorationGroupTests(testCtx));
	graphicsTests->addChild(createFRemTests(testCtx));
	graphicsTests->addChild(createOpSRemGraphicsTests(testCtx, QP_TEST_RESULT_PASS));
	graphicsTests->addChild(createOpSModGraphicsTests(testCtx, QP_TEST_RESULT_PASS));

	{
		de::MovePtr<tcu::TestCaseGroup>	graphicsAndroidTests	(new tcu::TestCaseGroup(testCtx, "android", "Android CTS Tests"));

		graphicsAndroidTests->addChild(createOpSRemGraphicsTests(testCtx, QP_TEST_RESULT_QUALITY_WARNING));
		graphicsAndroidTests->addChild(createOpSModGraphicsTests(testCtx, QP_TEST_RESULT_QUALITY_WARNING));

		graphicsTests->addChild(graphicsAndroidTests.release());
	}

	graphicsTests->addChild(createOpNameTests(testCtx));
	graphicsTests->addChild(createOpNameAbuseTests(testCtx));
	graphicsTests->addChild(createOpMemberNameAbuseTests(testCtx));

	graphicsTests->addChild(create8BitStorageGraphicsGroup(testCtx));
	graphicsTests->addChild(create16BitStorageGraphicsGroup(testCtx));
	graphicsTests->addChild(createFloatControlsGraphicsGroup(testCtx));
	graphicsTests->addChild(createUboMatrixPaddingGraphicsGroup(testCtx));
	graphicsTests->addChild(createCompositeInsertGraphicsGroup(testCtx));
	graphicsTests->addChild(createVariableInitGraphicsGroup(testCtx));
	graphicsTests->addChild(createConditionalBranchGraphicsGroup(testCtx));
	graphicsTests->addChild(createIndexingGraphicsGroup(testCtx));
	graphicsTests->addChild(createVariablePointersGraphicsGroup(testCtx));
	graphicsTests->addChild(createImageSamplerGraphicsGroup(testCtx));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpSConvert", "sconvert"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpUConvert", "uconvert"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpFConvert", "fconvert"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpConvertSToF", "convertstof"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpConvertFToS", "convertftos"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpConvertUToF", "convertutof"));
	graphicsTests->addChild(createConvertGraphicsTests(testCtx, "OpConvertFToU", "convertftou"));
	graphicsTests->addChild(createPointerParameterGraphicsGroup(testCtx));
	graphicsTests->addChild(createVaryingNameGraphicsGroup(testCtx));
	graphicsTests->addChild(createFloat16Tests(testCtx));
	graphicsTests->addChild(createFloat32Tests(testCtx));
	graphicsTests->addChild(createSpirvIdsAbuseTests(testCtx));
	graphicsTests->addChild(create64bitCompareGraphicsGroup(testCtx));

	instructionTests->addChild(computeTests.release());
	instructionTests->addChild(graphicsTests.release());
	instructionTests->addChild(createSpirvVersion1p4Group(testCtx));
	instructionTests->addChild(createFunctionParamsGroup(testCtx));
	instructionTests->addChild(createTrinaryMinMaxGroup(testCtx));
	instructionTests->addChild(createTerminateInvocationGroup(testCtx));

	return instructionTests.release();
}

} // SpirVAssembly
} // vkt
