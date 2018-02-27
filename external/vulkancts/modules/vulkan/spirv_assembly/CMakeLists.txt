# SPIR-V assembly tests

include_directories(..)

set(DEQP_VK_SPIRV_ASSEMBLY_SRCS
	vktSpvAsmCrossStageInterfaceTests.cpp
	vktSpvAsmCrossStageInterfaceTests.hpp
	vktSpvAsm16bitStorageTests.cpp
	vktSpvAsm16bitStorageTests.hpp
	vktSpvAsmUboMatrixPaddingTests.cpp
	vktSpvAsmUboMatrixPaddingTests.hpp
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
	vktSpvAsmTypeTests.cpp
	vktSpvAsmTypeTests.hpp
	vktSpvAsmTests.cpp
	vktSpvAsmTests.hpp
	vktSpvAsmUtils.cpp
	vktSpvAsmUtils.hpp
	vktSpvAsmVariableInitTests.cpp
	vktSpvAsmVariableInitTests.hpp
	vktSpvAsmVariablePointersTests.cpp
	vktSpvAsmVariablePointersTests.hpp
	vktSpvAsmSpirvVersionTests.cpp
	vktSpvAsmSpirvVersionTests.hpp
	vktSpvAsmLoopDepLenTests.cpp
	vktSpvAsmLoopDepLenTests.hpp
	vktSpvAsmLoopDepInfTests.cpp
	vktSpvAsmLoopDepInfTests.hpp
	)

set(DEQP_VK_SPIRV_ASSEMBLY_LIBS
	tcutil
	vkutil
	)

add_library(deqp-vk-spirv-assembly STATIC ${DEQP_VK_SPIRV_ASSEMBLY_SRCS})
target_link_libraries(deqp-vk-spirv-assembly ${DEQP_VK_SPIRV_ASSEMBLY_LIBS})
