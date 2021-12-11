# SPIR-V assembly tests

include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_SPIRV_ASSEMBLY_SRCS
	vktSpvAsm8bitStorageTests.cpp
	vktSpvAsm8bitStorageTests.hpp
	vktSpvAsmCrossStageInterfaceTests.cpp
	vktSpvAsmCrossStageInterfaceTests.hpp
	vktSpvAsm16bitStorageTests.cpp
	vktSpvAsm16bitStorageTests.hpp
	vktSpvAsmEmptyStructTests.cpp
	vktSpvAsmEmptyStructTests.hpp
	vktSpvAsmFloatControlsTests.cpp
	vktSpvAsmFloatControlsTests.hpp
	vktSpvAsmFromHlslTests.cpp
	vktSpvAsmFromHlslTests.hpp
	vktSpvAsmUboMatrixPaddingTests.cpp
	vktSpvAsmUboMatrixPaddingTests.hpp
	vktSpvAsmCompositeInsertTests.cpp
	vktSpvAsmCompositeInsertTests.hpp
	vktSpvAsmConditionalBranchTests.cpp
	vktSpvAsmConditionalBranchTests.hpp
	vktSpvAsmIndexingTests.cpp
	vktSpvAsmIndexingTests.hpp
	vktSpvAsmImageSamplerTests.cpp
	vktSpvAsmImageSamplerTests.hpp
	vktSpvAsmComputeShaderCase.cpp
	vktSpvAsmComputeShaderCase.hpp
	vktSpvAsmComputeShaderTestUtil.cpp
	vktSpvAsmComputeShaderTestUtil.hpp
	vktSpvAsmGraphicsShaderTestUtil.cpp
	vktSpvAsmGraphicsShaderTestUtil.hpp
	vktSpvAsmInstructionTests.cpp
	vktSpvAsmInstructionTests.hpp
	vktSpvAsmPointerParameterTests.cpp
	vktSpvAsmPointerParameterTests.hpp
	vktSpvAsmSignedIntCompareTests.cpp
	vktSpvAsmSignedIntCompareTests.hpp
	vktSpvAsmSignedOpTests.hpp
	vktSpvAsmSignedOpTests.cpp
	vktSpvAsm64bitCompareTests.cpp
	vktSpvAsm64bitCompareTests.hpp
	vktSpvAsmTypeTests.cpp
	vktSpvAsmTypeTests.hpp
	vktSpvAsmTerminateInvocationTests.cpp
	vktSpvAsmTerminateInvocationTests.hpp
	vktSpvAsmTests.cpp
	vktSpvAsmTests.hpp
	vktSpvAsmUtils.cpp
	vktSpvAsmUtils.hpp
	vktSpvAsmVariableInitTests.cpp
	vktSpvAsmVariableInitTests.hpp
	vktSpvAsmVariablePointersTests.cpp
	vktSpvAsmVariablePointersTests.hpp
	vktSpvAsmSpirvVersion1p4Tests.cpp
	vktSpvAsmSpirvVersion1p4Tests.hpp
	vktSpvAsmSpirvVersionTests.cpp
	vktSpvAsmSpirvVersionTests.hpp
	vktSpvAsmLoopDepLenTests.cpp
	vktSpvAsmLoopDepLenTests.hpp
	vktSpvAsmLoopDepInfTests.cpp
	vktSpvAsmLoopDepInfTests.hpp
	vktSpvAsmVaryingNameTests.cpp
	vktSpvAsmVaryingNameTests.hpp
	vktSpvAsmWorkgroupMemoryTests.cpp
	vktSpvAsmWorkgroupMemoryTests.hpp
	vktSpvAsmNonSemanticInfoTests.cpp
	vktSpvAsmNonSemanticInfoTests.hpp
	vktSpvAsmPtrAccessChainTests.cpp
	vktSpvAsmPtrAccessChainTests.hpp
	vktSpvAsmVectorShuffleTests.cpp
	vktSpvAsmVectorShuffleTests.hpp
	vktSpvAsmFloatControlsExtensionlessTests.cpp
	vktSpvAsmFloatControlsExtensionlessTests.hpp
	vktSpvAsmTrinaryMinMaxTests.cpp
	vktSpvAsmTrinaryMinMaxTests.hpp
	vktSpvAsmIntegerDotProductTests.cpp
	vktSpvAsmIntegerDotProductTests.hpp
	vktSpvAsmPhysicalStorageBufferPointerTests.cpp
	vktSpvAsmPhysicalStorageBufferPointerTests.hpp
	)

set(DEQP_VK_SPIRV_ASSEMBLY_LIBS
	libamber
	tcutil
	vkutil
	deqp-vk-amber
	)

PCH(DEQP_VK_SPIRV_ASSEMBLY_SRCS ../pch.cpp)

if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set(CMAKE_CXX_FLAGS     "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
elseif (DE_COMPILER_IS_MSC)
	set(CMAKE_CXX_FLAGS     "${CMAKE_CXX_FLAGS} /wd4068")
endif()
include_directories("../../../../amber/src/include")
include_directories("../amber")

add_library(deqp-vk-spirv-assembly STATIC ${DEQP_VK_SPIRV_ASSEMBLY_SRCS})
target_link_libraries(deqp-vk-spirv-assembly ${DEQP_VK_SPIRV_ASSEMBLY_LIBS})
