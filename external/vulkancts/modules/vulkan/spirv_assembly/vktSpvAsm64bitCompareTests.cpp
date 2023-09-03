/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief 64-bit data type comparison operations.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsm64bitCompareTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktSpvAsmUtils.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"

#include "tcuStringTemplate.hpp"

#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include <sstream>
#include <memory>
#include <limits>

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

template <typename T>
class CompareOperation
{
public:
	virtual std::string		spirvName	()					const = 0;
	virtual bool			run			(T left, T right)	const = 0;
};

// Helper intermediate class to be able to implement Ordered and Unordered floating point operations in a simpler way.
class DoubleCompareOperation: public CompareOperation<double>
{
public:
	struct BasicImplementation
	{
		virtual std::string	nameSuffix	()							const = 0;
		virtual bool		run			(double left, double right)	const = 0; // No NaNs here.
	};

	virtual std::string		spirvName	() const
	{
		return "OpF" + std::string(m_ordered ? "Ord" : "Unord") + m_impl.nameSuffix();
	}

	virtual bool			run			(double left, double right)	const
	{
		if (nanInvolved(left, right))
			return !m_ordered; // Ordered operations return false when NaN is involved.
		return m_impl.run(left, right);
	}

	DoubleCompareOperation(bool ordered, const BasicImplementation& impl)
		: m_ordered(ordered), m_impl(impl)
		{}

private:
	bool nanInvolved(double left, double right) const
	{
		return std::isnan(left) || std::isnan(right);
	}

	const bool					m_ordered;
	const BasicImplementation&	m_impl;
};

#define GEN_DOUBLE_BASIC_IMPL(NAME, OPERATION)																	\
	struct NAME##DoubleBasicImplClass : public DoubleCompareOperation::BasicImplementation						\
	{																											\
		virtual std::string	nameSuffix	()							const	{ return #NAME; }					\
		virtual bool		run			(double left, double right)	const	{ return left OPERATION right; }	\
	};																											\
	NAME##DoubleBasicImplClass NAME##DoubleBasicImplInstance;

GEN_DOUBLE_BASIC_IMPL(Equal,			==	)
GEN_DOUBLE_BASIC_IMPL(NotEqual,			!=	)
GEN_DOUBLE_BASIC_IMPL(LessThan,			<	)
GEN_DOUBLE_BASIC_IMPL(GreaterThan,		>	)
GEN_DOUBLE_BASIC_IMPL(LessThanEqual,	<=	)
GEN_DOUBLE_BASIC_IMPL(GreaterThanEqual,	>=	)

#define GEN_FORDERED_OP(NAME)	DoubleCompareOperation FOrdered##NAME##Op(true, NAME##DoubleBasicImplInstance)
#define GEN_FUNORDERED_OP(NAME)	DoubleCompareOperation FUnordered##NAME##Op(false, NAME##DoubleBasicImplInstance)
#define GEN_FBOTH_OP(NAME)		GEN_FORDERED_OP(NAME); GEN_FUNORDERED_OP(NAME);

GEN_FBOTH_OP(Equal)
GEN_FBOTH_OP(NotEqual)
GEN_FBOTH_OP(LessThan)
GEN_FBOTH_OP(GreaterThan)
GEN_FBOTH_OP(LessThanEqual)
GEN_FBOTH_OP(GreaterThanEqual)

template <typename IntClass>
class IntCompareOperation: public CompareOperation<IntClass>
{
public:
	struct Implementation
	{
		virtual std::string	typeChar	()								const = 0;
		virtual std::string	opName		()								const = 0;
		virtual bool		run			(IntClass left, IntClass right)	const = 0;
	};

	virtual std::string		spirvName	()								const
	{
		return "Op" + m_impl.typeChar() + m_impl.opName();
	}

	virtual bool			run			(IntClass left, IntClass right)	const
	{
		return m_impl.run(left, right);
	}

	IntCompareOperation(const Implementation& impl)
		: m_impl(impl)
		{}

private:
	const Implementation& m_impl;
};

#define GEN_INT_IMPL(INTTYPE, PREFIX, TYPECHAR, OPNAME, OPERATOR)													\
	struct PREFIX##OPNAME##IntImplClass : public IntCompareOperation<INTTYPE>::Implementation						\
	{																												\
		virtual std::string	typeChar	()								const	{ return #TYPECHAR;	}				\
		virtual std::string	opName		()								const	{ return #OPNAME;	}				\
		virtual bool		run			(INTTYPE left, INTTYPE right)	const	{ return left OPERATOR right; }		\
	};																												\
	PREFIX##OPNAME##IntImplClass PREFIX##OPNAME##IntImplInstance;

#define GEN_ALL_INT_TYPE_IMPL(INTTYPE, PREFIX, TYPECHAR)				\
	GEN_INT_IMPL(INTTYPE, PREFIX, I,		Equal,				==	)	\
	GEN_INT_IMPL(INTTYPE, PREFIX, I,		NotEqual,			!=	)	\
	GEN_INT_IMPL(INTTYPE, PREFIX, TYPECHAR,	GreaterThan,		>	)	\
	GEN_INT_IMPL(INTTYPE, PREFIX, TYPECHAR,	GreaterThanEqual,	>=	)	\
	GEN_INT_IMPL(INTTYPE, PREFIX, TYPECHAR,	LessThan,			<	)	\
	GEN_INT_IMPL(INTTYPE, PREFIX, TYPECHAR,	LessThanEqual,		<=	)

GEN_ALL_INT_TYPE_IMPL(deInt64,	int64, S)
GEN_ALL_INT_TYPE_IMPL(deUint64,	uint64, U)

#define GEN_INT_OP(INTTYPE, PREFIX, OPNAME)																\
	struct PREFIX##OPNAME##OpClass: public IntCompareOperation<INTTYPE>									\
	{																									\
		PREFIX##OPNAME##OpClass () : IntCompareOperation<INTTYPE>(PREFIX##OPNAME##IntImplInstance) {}	\
	};																									\
	PREFIX##OPNAME##OpClass PREFIX##OPNAME##Op;

#define GEN_ALL_INT_OPS(INTTYPE, PREFIX)				\
	GEN_INT_OP(INTTYPE, PREFIX, Equal				)	\
	GEN_INT_OP(INTTYPE, PREFIX, NotEqual			)	\
	GEN_INT_OP(INTTYPE, PREFIX, GreaterThan			)	\
	GEN_INT_OP(INTTYPE, PREFIX, GreaterThanEqual	)	\
	GEN_INT_OP(INTTYPE, PREFIX, LessThan			)	\
	GEN_INT_OP(INTTYPE, PREFIX, LessThanEqual		)

GEN_ALL_INT_OPS(deInt64, int64)
GEN_ALL_INT_OPS(deUint64, uint64)

enum DataType {
	DATA_TYPE_SINGLE = 0,
	DATA_TYPE_VECTOR,
	DATA_TYPE_MAX_ENUM,
};

template <class T>
using OperandsVector = std::vector<std::pair<T, T>>;

template <class T>
struct TestParameters
{
	DataType					dataType;
	const CompareOperation<T>&	operation;
	vk::VkShaderStageFlagBits	stage;
	const OperandsVector<T>&	operands;
	bool						requireNanPreserve;
};

// Shader template for the compute stage using single scalars.
// Generated from the following GLSL shader, replacing some bits by template parameters.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { double values[];	} input1;
layout(binding = 1) buffer Input2  { double values[];	} input2;
layout(binding = 2) buffer Output1 { int values[];		} output1;

void main()
{
        for (int i = 0; i < 20; i++) {
                output1.values[i] = int(input1.values[i] == input2.values[i]);
        }
}
#endif
const tcu::StringTemplate CompShaderSingle(R"(
                        OpCapability Shader
                        ${OPCAPABILITY}
                        ${NANCAP}
                        ${NANEXT}
                   %1 = OpExtInstImport "GLSL.std.450"
                        OpMemoryModel Logical GLSL450
                        OpEntryPoint GLCompute %main "main"
                        ${NANMODE}
                        OpExecutionMode %main LocalSize 1 1 1
                        OpName %main "main"
                        OpName %i "i"
                        OpName %Output1 "Output1"
                        OpMemberName %Output1 0 "values"
                        OpName %output1 "output1"
                        OpName %Input1 "Input1"
                        OpMemberName %Input1 0 "values"
                        OpName %input1 "input1"
                        OpName %Input2 "Input2"
                        OpMemberName %Input2 0 "values"
                        OpName %input2 "input2"
                        OpDecorate %_runtimearr_int ArrayStride 4
                        OpMemberDecorate %Output1 0 Offset 0
                        OpDecorate %Output1 BufferBlock
                        OpDecorate %output1 DescriptorSet 0
                        OpDecorate %output1 Binding 2
                        OpDecorate %_runtimearr_tinput ArrayStride 8
                        OpMemberDecorate %Input1 0 Offset 0
                        OpDecorate %Input1 BufferBlock
                        OpDecorate %input1 DescriptorSet 0
                        OpDecorate %input1 Binding 0
                        OpDecorate %_runtimearr_tinput_0 ArrayStride 8
                        OpMemberDecorate %Input2 0 Offset 0
                        OpDecorate %Input2 BufferBlock
                        OpDecorate %input2 DescriptorSet 0
                        OpDecorate %input2 Binding 1
                %void = OpTypeVoid
                   %3 = OpTypeFunction %void
                 %int = OpTypeInt 32 1
   %_ptr_Function_int = OpTypePointer Function %int
               %int_0 = OpConstant %int 0
              %niters = OpConstant %int ${ITERS}
                %bool = OpTypeBool
     %_runtimearr_int = OpTypeRuntimeArray %int
             %Output1 = OpTypeStruct %_runtimearr_int
%_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
             %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
              %tinput = ${OPTYPE}
  %_runtimearr_tinput = OpTypeRuntimeArray %tinput
              %Input1 = OpTypeStruct %_runtimearr_tinput
 %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
              %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
 %_ptr_Uniform_tinput = OpTypePointer Uniform %tinput
%_runtimearr_tinput_0 = OpTypeRuntimeArray %tinput
              %Input2 = OpTypeStruct %_runtimearr_tinput_0
 %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
              %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
               %int_1 = OpConstant %int 1
    %_ptr_Uniform_int = OpTypePointer Uniform %int
                %main = OpFunction %void None %3
                   %5 = OpLabel
                   %i = OpVariable %_ptr_Function_int Function
                        OpStore %i %int_0
                        OpBranch %10
                  %10 = OpLabel
                        OpLoopMerge %12 %13 None
                        OpBranch %14
                  %14 = OpLabel
                  %15 = OpLoad %int %i
                  %18 = OpSLessThan %bool %15 %niters
                        OpBranchConditional %18 %11 %12
                  %11 = OpLabel
                  %23 = OpLoad %int %i
                  %29 = OpLoad %int %i
                  %31 = OpAccessChain %_ptr_Uniform_tinput %input1 %int_0 %29
                  %32 = OpLoad %tinput %31
                  %37 = OpLoad %int %i
                  %38 = OpAccessChain %_ptr_Uniform_tinput %input2 %int_0 %37
                  %39 = OpLoad %tinput %38
                  %40 = ${OPNAME} %bool %32 %39
                  %42 = OpSelect %int %40 %int_1 %int_0
                  %44 = OpAccessChain %_ptr_Uniform_int %output1 %int_0 %23
                        OpStore %44 %42
                        OpBranch %13
                  %13 = OpLabel
                  %45 = OpLoad %int %i
                  %46 = OpIAdd %int %45 %int_1
                        OpStore %i %46
                        OpBranch %10
                  %12 = OpLabel
                        OpReturn
                        OpFunctionEnd
)");

// Shader template for the compute stage using vectors.
// Generated from the following GLSL shader, replacing some bits by template parameters.
// Note the number of iterations needs to be divided by 4 as the shader will consume 4 doubles at a time.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { dvec4 values[];	} input1;
layout(binding = 1) buffer Input2  { dvec4 values[];	} input2;
layout(binding = 2) buffer Output1 { ivec4 values[];	} output1;

void main()
{
        for (int i = 0; i < 5; i++) {
                output1.values[i] = ivec4(equal(input1.values[i], input2.values[i]));
        }
}
#endif
const tcu::StringTemplate CompShaderVector(R"(
                          OpCapability Shader
                          ${OPCAPABILITY}
                          ${NANCAP}
                          ${NANEXT}
                     %1 = OpExtInstImport "GLSL.std.450"
                          OpMemoryModel Logical GLSL450
                          OpEntryPoint GLCompute %main "main"
                          ${NANMODE}
                          OpExecutionMode %main LocalSize 1 1 1
                          OpName %main "main"
                          OpName %i "i"
                          OpName %Output1 "Output1"
                          OpMemberName %Output1 0 "values"
                          OpName %output1 "output1"
                          OpName %Input1 "Input1"
                          OpMemberName %Input1 0 "values"
                          OpName %input1 "input1"
                          OpName %Input2 "Input2"
                          OpMemberName %Input2 0 "values"
                          OpName %input2 "input2"
                          OpDecorate %_runtimearr_v4int ArrayStride 16
                          OpMemberDecorate %Output1 0 Offset 0
                          OpDecorate %Output1 BufferBlock
                          OpDecorate %output1 DescriptorSet 0
                          OpDecorate %output1 Binding 2
                          OpDecorate %_runtimearr_v4tinput ArrayStride 32
                          OpMemberDecorate %Input1 0 Offset 0
                          OpDecorate %Input1 BufferBlock
                          OpDecorate %input1 DescriptorSet 0
                          OpDecorate %input1 Binding 0
                          OpDecorate %_runtimearr_v4tinput_0 ArrayStride 32
                          OpMemberDecorate %Input2 0 Offset 0
                          OpDecorate %Input2 BufferBlock
                          OpDecorate %input2 DescriptorSet 0
                          OpDecorate %input2 Binding 1
                  %void = OpTypeVoid
                     %3 = OpTypeFunction %void
                   %int = OpTypeInt 32 1
     %_ptr_Function_int = OpTypePointer Function %int
                 %int_0 = OpConstant %int 0
                %niters = OpConstant %int ${ITERS}
                  %bool = OpTypeBool
                 %v4int = OpTypeVector %int 4
     %_runtimearr_v4int = OpTypeRuntimeArray %v4int
               %Output1 = OpTypeStruct %_runtimearr_v4int
  %_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
               %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
                %tinput = ${OPTYPE}
              %v4tinput = OpTypeVector %tinput 4
  %_runtimearr_v4tinput = OpTypeRuntimeArray %v4tinput
                %Input1 = OpTypeStruct %_runtimearr_v4tinput
   %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
                %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
 %_ptr_Uniform_v4tinput = OpTypePointer Uniform %v4tinput
%_runtimearr_v4tinput_0 = OpTypeRuntimeArray %v4tinput
                %Input2 = OpTypeStruct %_runtimearr_v4tinput_0
   %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
                %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
                %v4bool = OpTypeVector %bool 4
                 %int_1 = OpConstant %int 1
                    %45 = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
                    %46 = OpConstantComposite %v4int %int_1 %int_1 %int_1 %int_1
    %_ptr_Uniform_v4int = OpTypePointer Uniform %v4int
                  %main = OpFunction %void None %3
                     %5 = OpLabel
                     %i = OpVariable %_ptr_Function_int Function
                          OpStore %i %int_0
                          OpBranch %10
                    %10 = OpLabel
                          OpLoopMerge %12 %13 None
                          OpBranch %14
                    %14 = OpLabel
                    %15 = OpLoad %int %i
                    %18 = OpSLessThan %bool %15 %niters
                          OpBranchConditional %18 %11 %12
                    %11 = OpLabel
                    %24 = OpLoad %int %i
                    %31 = OpLoad %int %i
                    %33 = OpAccessChain %_ptr_Uniform_v4tinput %input1 %int_0 %31
                    %34 = OpLoad %v4tinput %33
                    %39 = OpLoad %int %i
                    %40 = OpAccessChain %_ptr_Uniform_v4tinput %input2 %int_0 %39
                    %41 = OpLoad %v4tinput %40
                    %43 = ${OPNAME} %v4bool %34 %41
                    %47 = OpSelect %v4int %43 %46 %45
                    %49 = OpAccessChain %_ptr_Uniform_v4int %output1 %int_0 %24
                          OpStore %49 %47
                          OpBranch %13
                    %13 = OpLabel
                    %50 = OpLoad %int %i
                    %51 = OpIAdd %int %50 %int_1
                          OpStore %i %51
                          OpBranch %10
                    %12 = OpLabel
                          OpReturn
                          OpFunctionEnd
)");

// Shader template for the vertex stage using single scalars.
// Generated from the following GLSL shader, replacing some bits by template parameters.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { double values[];	} input1;
layout(binding = 1) buffer Input2  { double values[];	} input2;
layout(binding = 2) buffer Output1 { int values[];		} output1;

void main()
{
      gl_PointSize = 1;
      gl_Position = vec4(0.0, 0.0, 0.0, 1.0);

      for (int i = 0; i < 20; i++) {
              output1.values[i] = int(input1.values[i] == input2.values[i]);
      }
}
#endif
const tcu::StringTemplate VertShaderSingle(R"(
                            OpCapability Shader
                            ${OPCAPABILITY}
                            ${NANCAP}
                            ${NANEXT}
                       %1 = OpExtInstImport "GLSL.std.450"
                            OpMemoryModel Logical GLSL450
                            OpEntryPoint Vertex %main "main" %_
                            ${NANMODE}
                            OpName %main "main"
                            OpName %gl_PerVertex "gl_PerVertex"
                            OpMemberName %gl_PerVertex 0 "gl_Position"
                            OpMemberName %gl_PerVertex 1 "gl_PointSize"
                            OpMemberName %gl_PerVertex 2 "gl_ClipDistance"
                            OpName %_ ""
                            OpName %i "i"
                            OpName %Output1 "Output1"
                            OpMemberName %Output1 0 "values"
                            OpName %output1 "output1"
                            OpName %Input1 "Input1"
                            OpMemberName %Input1 0 "values"
                            OpName %input1 "input1"
                            OpName %Input2 "Input2"
                            OpMemberName %Input2 0 "values"
                            OpName %input2 "input2"
                            OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
                            OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
                            OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
                            OpDecorate %gl_PerVertex Block
                            OpDecorate %_runtimearr_int ArrayStride 4
                            OpMemberDecorate %Output1 0 Offset 0
                            OpDecorate %Output1 BufferBlock
                            OpDecorate %output1 DescriptorSet 0
                            OpDecorate %output1 Binding 2
                            OpDecorate %_runtimearr_tinput ArrayStride 8
                            OpMemberDecorate %Input1 0 Offset 0
                            OpDecorate %Input1 BufferBlock
                            OpDecorate %input1 DescriptorSet 0
                            OpDecorate %input1 Binding 0
                            OpDecorate %_runtimearr_tinput_0 ArrayStride 8
                            OpMemberDecorate %Input2 0 Offset 0
                            OpDecorate %Input2 BufferBlock
                            OpDecorate %input2 DescriptorSet 0
                            OpDecorate %input2 Binding 1
                    %void = OpTypeVoid
                       %3 = OpTypeFunction %void
                   %float = OpTypeFloat 32
                 %v4float = OpTypeVector %float 4
                    %uint = OpTypeInt 32 0
                  %uint_1 = OpConstant %uint 1
       %_arr_float_uint_1 = OpTypeArray %float %uint_1
            %gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
                       %_ = OpVariable %_ptr_Output_gl_PerVertex Output
                     %int = OpTypeInt 32 1
                   %int_1 = OpConstant %int 1
                 %float_1 = OpConstant %float 1
       %_ptr_Output_float = OpTypePointer Output %float
                   %int_0 = OpConstant %int 0
                 %float_0 = OpConstant %float 0
                      %21 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1
     %_ptr_Output_v4float = OpTypePointer Output %v4float
       %_ptr_Function_int = OpTypePointer Function %int
                  %niters = OpConstant %int ${ITERS}
                    %bool = OpTypeBool
         %_runtimearr_int = OpTypeRuntimeArray %int
                 %Output1 = OpTypeStruct %_runtimearr_int
    %_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
                 %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
                  %tinput = ${OPTYPE}
      %_runtimearr_tinput = OpTypeRuntimeArray %tinput
                  %Input1 = OpTypeStruct %_runtimearr_tinput
     %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
                  %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
     %_ptr_Uniform_tinput = OpTypePointer Uniform %tinput
    %_runtimearr_tinput_0 = OpTypeRuntimeArray %tinput
                  %Input2 = OpTypeStruct %_runtimearr_tinput_0
     %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
                  %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
        %_ptr_Uniform_int = OpTypePointer Uniform %int
                    %main = OpFunction %void None %3
                       %5 = OpLabel
                       %i = OpVariable %_ptr_Function_int Function
                      %18 = OpAccessChain %_ptr_Output_float %_ %int_1
                            OpStore %18 %float_1
                      %23 = OpAccessChain %_ptr_Output_v4float %_ %int_0
                            OpStore %23 %21
                            OpStore %i %int_0
                            OpBranch %26
                      %26 = OpLabel
                            OpLoopMerge %28 %29 None
                            OpBranch %30
                      %30 = OpLabel
                      %31 = OpLoad %int %i
                      %34 = OpSLessThan %bool %31 %niters
                            OpBranchConditional %34 %27 %28
                      %27 = OpLabel
                      %39 = OpLoad %int %i
                      %45 = OpLoad %int %i
                      %47 = OpAccessChain %_ptr_Uniform_tinput %input1 %int_0 %45
                      %48 = OpLoad %tinput %47
                      %53 = OpLoad %int %i
                      %54 = OpAccessChain %_ptr_Uniform_tinput %input2 %int_0 %53
                      %55 = OpLoad %tinput %54
                      %56 = ${OPNAME} %bool %48 %55
                      %57 = OpSelect %int %56 %int_1 %int_0
                      %59 = OpAccessChain %_ptr_Uniform_int %output1 %int_0 %39
                            OpStore %59 %57
                            OpBranch %29
                      %29 = OpLabel
                      %60 = OpLoad %int %i
                      %61 = OpIAdd %int %60 %int_1
                            OpStore %i %61
                            OpBranch %26
                      %28 = OpLabel
                            OpReturn
                            OpFunctionEnd
)");

// Shader template for the vertex stage using vectors.
// Generated from the following GLSL shader, replacing some bits by template parameters.
// Note the number of iterations needs to be divided by 4 as the shader will consume 4 doubles at a time.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { dvec4 values[]; } input1;
layout(binding = 1) buffer Input2  { dvec4 values[]; } input2;
layout(binding = 2) buffer Output1 { ivec4 values[]; } output1;

void main()
{
      gl_PointSize = 1;
      gl_Position = vec4(0.0, 0.0, 0.0, 1.0);

      for (int i = 0; i < 5; i++) {
              output1.values[i] = ivec4(equal(input1.values[i], input2.values[i]));
      }
}
#endif
const tcu::StringTemplate VertShaderVector(R"(
                            OpCapability Shader
                            ${OPCAPABILITY}
                            ${NANCAP}
                            ${NANEXT}
                       %1 = OpExtInstImport "GLSL.std.450"
                            OpMemoryModel Logical GLSL450
                            OpEntryPoint Vertex %main "main" %_
                            ${NANMODE}
                            OpName %main "main"
                            OpName %gl_PerVertex "gl_PerVertex"
                            OpMemberName %gl_PerVertex 0 "gl_Position"
                            OpMemberName %gl_PerVertex 1 "gl_PointSize"
                            OpMemberName %gl_PerVertex 2 "gl_ClipDistance"
                            OpName %_ ""
                            OpName %i "i"
                            OpName %Output1 "Output1"
                            OpMemberName %Output1 0 "values"
                            OpName %output1 "output1"
                            OpName %Input1 "Input1"
                            OpMemberName %Input1 0 "values"
                            OpName %input1 "input1"
                            OpName %Input2 "Input2"
                            OpMemberName %Input2 0 "values"
                            OpName %input2 "input2"
                            OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
                            OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
                            OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
                            OpDecorate %gl_PerVertex Block
                            OpDecorate %_runtimearr_v4int ArrayStride 16
                            OpMemberDecorate %Output1 0 Offset 0
                            OpDecorate %Output1 BufferBlock
                            OpDecorate %output1 DescriptorSet 0
                            OpDecorate %output1 Binding 2
                            OpDecorate %_runtimearr_v4tinput ArrayStride 32
                            OpMemberDecorate %Input1 0 Offset 0
                            OpDecorate %Input1 BufferBlock
                            OpDecorate %input1 DescriptorSet 0
                            OpDecorate %input1 Binding 0
                            OpDecorate %_runtimearr_v4tinput_0 ArrayStride 32
                            OpMemberDecorate %Input2 0 Offset 0
                            OpDecorate %Input2 BufferBlock
                            OpDecorate %input2 DescriptorSet 0
                            OpDecorate %input2 Binding 1
                    %void = OpTypeVoid
                       %3 = OpTypeFunction %void
                   %float = OpTypeFloat 32
                 %v4float = OpTypeVector %float 4
                    %uint = OpTypeInt 32 0
                  %uint_1 = OpConstant %uint 1
       %_arr_float_uint_1 = OpTypeArray %float %uint_1
            %gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
                       %_ = OpVariable %_ptr_Output_gl_PerVertex Output
                     %int = OpTypeInt 32 1
                   %int_1 = OpConstant %int 1
                 %float_1 = OpConstant %float 1
       %_ptr_Output_float = OpTypePointer Output %float
                   %int_0 = OpConstant %int 0
                 %float_0 = OpConstant %float 0
                      %21 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1
     %_ptr_Output_v4float = OpTypePointer Output %v4float
       %_ptr_Function_int = OpTypePointer Function %int
                  %niters = OpConstant %int ${ITERS}
                    %bool = OpTypeBool
                   %v4int = OpTypeVector %int 4
       %_runtimearr_v4int = OpTypeRuntimeArray %v4int
                 %Output1 = OpTypeStruct %_runtimearr_v4int
    %_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
                 %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
                  %tinput = ${OPTYPE}
                %v4tinput = OpTypeVector %tinput 4
    %_runtimearr_v4tinput = OpTypeRuntimeArray %v4tinput
                  %Input1 = OpTypeStruct %_runtimearr_v4tinput
     %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
                  %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
   %_ptr_Uniform_v4tinput = OpTypePointer Uniform %v4tinput
  %_runtimearr_v4tinput_0 = OpTypeRuntimeArray %v4tinput
                  %Input2 = OpTypeStruct %_runtimearr_v4tinput_0
     %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
                  %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
                  %v4bool = OpTypeVector %bool 4
                      %60 = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
                      %61 = OpConstantComposite %v4int %int_1 %int_1 %int_1 %int_1
      %_ptr_Uniform_v4int = OpTypePointer Uniform %v4int
                    %main = OpFunction %void None %3
                       %5 = OpLabel
                       %i = OpVariable %_ptr_Function_int Function
                      %18 = OpAccessChain %_ptr_Output_float %_ %int_1
                            OpStore %18 %float_1
                      %23 = OpAccessChain %_ptr_Output_v4float %_ %int_0
                            OpStore %23 %21
                            OpStore %i %int_0
                            OpBranch %26
                      %26 = OpLabel
                            OpLoopMerge %28 %29 None
                            OpBranch %30
                      %30 = OpLabel
                      %31 = OpLoad %int %i
                      %34 = OpSLessThan %bool %31 %niters
                            OpBranchConditional %34 %27 %28
                      %27 = OpLabel
                      %40 = OpLoad %int %i
                      %47 = OpLoad %int %i
                      %49 = OpAccessChain %_ptr_Uniform_v4tinput %input1 %int_0 %47
                      %50 = OpLoad %v4tinput %49
                      %55 = OpLoad %int %i
                      %56 = OpAccessChain %_ptr_Uniform_v4tinput %input2 %int_0 %55
                      %57 = OpLoad %v4tinput %56
                      %59 = ${OPNAME} %v4bool %50 %57
                      %62 = OpSelect %v4int %59 %61 %60
                      %64 = OpAccessChain %_ptr_Uniform_v4int %output1 %int_0 %40
                            OpStore %64 %62
                            OpBranch %29
                      %29 = OpLabel
                      %65 = OpLoad %int %i
                      %66 = OpIAdd %int %65 %int_1
                            OpStore %i %66
                            OpBranch %26
                      %28 = OpLabel
                            OpReturn
                            OpFunctionEnd
)");

// GLSL passthrough vertex shader to test the fragment shader.
const std::string VertShaderPassThrough = R"(
#version 430

layout(location = 0) out vec4 out_color;

void main()
{
		gl_PointSize	= 1;
		gl_Position		= vec4(0.0, 0.0, 0.0, 1.0);
		out_color		= vec4(0.0, 0.0, 0.0, 1.0);
}
)";

// Shader template for the fragment stage using single scalars.
// Generated from the following GLSL shader, replacing some bits by template parameters.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { double values[];	} input1;
layout(binding = 1) buffer Input2  { double values[];	} input2;
layout(binding = 2) buffer Output1 { int values[];		} output1;

void main()
{
      for (int i = 0; i < 20; i++) {
              output1.values[i] = int(input1.values[i] == input2.values[i]);
      }
}
#endif
const tcu::StringTemplate FragShaderSingle(R"(
                        OpCapability Shader
                        ${OPCAPABILITY}
                        ${NANCAP}
                        ${NANEXT}
                   %1 = OpExtInstImport "GLSL.std.450"
                        OpMemoryModel Logical GLSL450
                        OpEntryPoint Fragment %main "main"
                        ${NANMODE}
                        OpExecutionMode %main OriginUpperLeft
                        OpSource GLSL 430
                        OpName %main "main"
                        OpName %i "i"
                        OpName %Output1 "Output1"
                        OpMemberName %Output1 0 "values"
                        OpName %output1 "output1"
                        OpName %Input1 "Input1"
                        OpMemberName %Input1 0 "values"
                        OpName %input1 "input1"
                        OpName %Input2 "Input2"
                        OpMemberName %Input2 0 "values"
                        OpName %input2 "input2"
                        OpDecorate %_runtimearr_int ArrayStride 4
                        OpMemberDecorate %Output1 0 Offset 0
                        OpDecorate %Output1 BufferBlock
                        OpDecorate %output1 DescriptorSet 0
                        OpDecorate %output1 Binding 2
                        OpDecorate %_runtimearr_tinput ArrayStride 8
                        OpMemberDecorate %Input1 0 Offset 0
                        OpDecorate %Input1 BufferBlock
                        OpDecorate %input1 DescriptorSet 0
                        OpDecorate %input1 Binding 0
                        OpDecorate %_runtimearr_tinput_0 ArrayStride 8
                        OpMemberDecorate %Input2 0 Offset 0
                        OpDecorate %Input2 BufferBlock
                        OpDecorate %input2 DescriptorSet 0
                        OpDecorate %input2 Binding 1
                %void = OpTypeVoid
                   %3 = OpTypeFunction %void
                 %int = OpTypeInt 32 1
   %_ptr_Function_int = OpTypePointer Function %int
               %int_0 = OpConstant %int 0
              %niters = OpConstant %int ${ITERS}
                %bool = OpTypeBool
     %_runtimearr_int = OpTypeRuntimeArray %int
             %Output1 = OpTypeStruct %_runtimearr_int
%_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
             %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
              %tinput = ${OPTYPE}
  %_runtimearr_tinput = OpTypeRuntimeArray %tinput
              %Input1 = OpTypeStruct %_runtimearr_tinput
 %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
              %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
 %_ptr_Uniform_tinput = OpTypePointer Uniform %tinput
%_runtimearr_tinput_0 = OpTypeRuntimeArray %tinput
              %Input2 = OpTypeStruct %_runtimearr_tinput_0
 %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
              %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
               %int_1 = OpConstant %int 1
    %_ptr_Uniform_int = OpTypePointer Uniform %int
                %main = OpFunction %void None %3
                   %5 = OpLabel
                   %i = OpVariable %_ptr_Function_int Function
                        OpStore %i %int_0
                        OpBranch %10
                  %10 = OpLabel
                        OpLoopMerge %12 %13 None
                        OpBranch %14
                  %14 = OpLabel
                  %15 = OpLoad %int %i
                  %18 = OpSLessThan %bool %15 %niters
                        OpBranchConditional %18 %11 %12
                  %11 = OpLabel
                  %23 = OpLoad %int %i
                  %29 = OpLoad %int %i
                  %31 = OpAccessChain %_ptr_Uniform_tinput %input1 %int_0 %29
                  %32 = OpLoad %tinput %31
                  %37 = OpLoad %int %i
                  %38 = OpAccessChain %_ptr_Uniform_tinput %input2 %int_0 %37
                  %39 = OpLoad %tinput %38
                  %40 = ${OPNAME} %bool %32 %39
                  %42 = OpSelect %int %40 %int_1 %int_0
                  %44 = OpAccessChain %_ptr_Uniform_int %output1 %int_0 %23
                        OpStore %44 %42
                        OpBranch %13
                  %13 = OpLabel
                  %45 = OpLoad %int %i
                  %46 = OpIAdd %int %45 %int_1
                        OpStore %i %46
                        OpBranch %10
                  %12 = OpLabel
                        OpReturn
                        OpFunctionEnd
)");

// Shader template for the fragment stage using vectors.
// Generated from the following GLSL shader, replacing some bits by template parameters.
// Note the number of iterations needs to be divided by 4 as the shader will consume 4 doubles at a time.
#if 0
#version 430

// Left operands, right operands and results.
layout(binding = 0) buffer Input1  { dvec4 values[]; } input1;
layout(binding = 1) buffer Input2  { dvec4 values[]; } input2;
layout(binding = 2) buffer Output1 { ivec4 values[]; } output1;

void main()
{
      for (int i = 0; i < 5; i++) {
              output1.values[i] = ivec4(equal(input1.values[i], input2.values[i]));
      }
}
#endif
const tcu::StringTemplate FragShaderVector(R"(
                          OpCapability Shader
                          ${OPCAPABILITY}
                          ${NANCAP}
                          ${NANEXT}
                     %1 = OpExtInstImport "GLSL.std.450"
                          OpMemoryModel Logical GLSL450
                          OpEntryPoint Fragment %main "main"
                          ${NANMODE}
                          OpExecutionMode %main OriginUpperLeft
                          OpName %main "main"
                          OpName %i "i"
                          OpName %Output1 "Output1"
                          OpMemberName %Output1 0 "values"
                          OpName %output1 "output1"
                          OpName %Input1 "Input1"
                          OpMemberName %Input1 0 "values"
                          OpName %input1 "input1"
                          OpName %Input2 "Input2"
                          OpMemberName %Input2 0 "values"
                          OpName %input2 "input2"
                          OpDecorate %_runtimearr_v4int ArrayStride 16
                          OpMemberDecorate %Output1 0 Offset 0
                          OpDecorate %Output1 BufferBlock
                          OpDecorate %output1 DescriptorSet 0
                          OpDecorate %output1 Binding 2
                          OpDecorate %_runtimearr_v4tinput ArrayStride 32
                          OpMemberDecorate %Input1 0 Offset 0
                          OpDecorate %Input1 BufferBlock
                          OpDecorate %input1 DescriptorSet 0
                          OpDecorate %input1 Binding 0
                          OpDecorate %_runtimearr_v4tinput_0 ArrayStride 32
                          OpMemberDecorate %Input2 0 Offset 0
                          OpDecorate %Input2 BufferBlock
                          OpDecorate %input2 DescriptorSet 0
                          OpDecorate %input2 Binding 1
                  %void = OpTypeVoid
                     %3 = OpTypeFunction %void
                   %int = OpTypeInt 32 1
     %_ptr_Function_int = OpTypePointer Function %int
                 %int_0 = OpConstant %int 0
                %niters = OpConstant %int ${ITERS}
                  %bool = OpTypeBool
                 %v4int = OpTypeVector %int 4
     %_runtimearr_v4int = OpTypeRuntimeArray %v4int
               %Output1 = OpTypeStruct %_runtimearr_v4int
  %_ptr_Uniform_Output1 = OpTypePointer Uniform %Output1
               %output1 = OpVariable %_ptr_Uniform_Output1 Uniform
                %tinput = ${OPTYPE}
              %v4tinput = OpTypeVector %tinput 4
  %_runtimearr_v4tinput = OpTypeRuntimeArray %v4tinput
                %Input1 = OpTypeStruct %_runtimearr_v4tinput
   %_ptr_Uniform_Input1 = OpTypePointer Uniform %Input1
                %input1 = OpVariable %_ptr_Uniform_Input1 Uniform
 %_ptr_Uniform_v4tinput = OpTypePointer Uniform %v4tinput
%_runtimearr_v4tinput_0 = OpTypeRuntimeArray %v4tinput
                %Input2 = OpTypeStruct %_runtimearr_v4tinput_0
   %_ptr_Uniform_Input2 = OpTypePointer Uniform %Input2
                %input2 = OpVariable %_ptr_Uniform_Input2 Uniform
                %v4bool = OpTypeVector %bool 4
                 %int_1 = OpConstant %int 1
                    %45 = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
                    %46 = OpConstantComposite %v4int %int_1 %int_1 %int_1 %int_1
    %_ptr_Uniform_v4int = OpTypePointer Uniform %v4int
                  %main = OpFunction %void None %3
                     %5 = OpLabel
                     %i = OpVariable %_ptr_Function_int Function
                          OpStore %i %int_0
                          OpBranch %10
                    %10 = OpLabel
                          OpLoopMerge %12 %13 None
                          OpBranch %14
                    %14 = OpLabel
                    %15 = OpLoad %int %i
                    %18 = OpSLessThan %bool %15 %niters
                          OpBranchConditional %18 %11 %12
                    %11 = OpLabel
                    %24 = OpLoad %int %i
                    %31 = OpLoad %int %i
                    %33 = OpAccessChain %_ptr_Uniform_v4tinput %input1 %int_0 %31
                    %34 = OpLoad %v4tinput %33
                    %39 = OpLoad %int %i
                    %40 = OpAccessChain %_ptr_Uniform_v4tinput %input2 %int_0 %39
                    %41 = OpLoad %v4tinput %40
                    %43 = ${OPNAME} %v4bool %34 %41
                    %47 = OpSelect %v4int %43 %46 %45
                    %49 = OpAccessChain %_ptr_Uniform_v4int %output1 %int_0 %24
                          OpStore %49 %47
                          OpBranch %13
                    %13 = OpLabel
                    %50 = OpLoad %int %i
                    %51 = OpIAdd %int %50 %int_1
                          OpStore %i %51
                          OpBranch %10
                    %12 = OpLabel
                          OpReturn
                          OpFunctionEnd
)");

struct SpirvTemplateManager
{
	static const tcu::StringTemplate& getTemplate (DataType type, vk::VkShaderStageFlagBits stage)
	{
		DE_ASSERT(type == DATA_TYPE_SINGLE || type == DATA_TYPE_VECTOR);
		DE_ASSERT(	stage == vk::VK_SHADER_STAGE_COMPUTE_BIT		||
					stage == vk::VK_SHADER_STAGE_VERTEX_BIT			||
					stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT		);

		if (type == DATA_TYPE_SINGLE)
		{
			if (stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)	return CompShaderSingle;
			if (stage == vk::VK_SHADER_STAGE_VERTEX_BIT)	return VertShaderSingle;
			else											return FragShaderSingle;
		}
		else
		{
			if (stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)	return CompShaderVector;
			if (stage == vk::VK_SHADER_STAGE_VERTEX_BIT)	return VertShaderVector;
			else											return FragShaderVector;
		}
	}

	// Specialized below for different types.
	template <class T>
	static std::string getOpCapability();

	// Same.
	template <class T>
	static std::string getOpType();

	// Return the capabilities, extensions and execution modes for NaN preservation.
	static std::string getNanCapability	(bool preserve);
	static std::string getNanExtension	(bool preserve);
	static std::string getNanExeMode	(bool preserve);
};

template <> std::string SpirvTemplateManager::getOpCapability<double>()		{ return "OpCapability Float64";	}
template <> std::string SpirvTemplateManager::getOpCapability<deInt64>()	{ return "OpCapability Int64";		}
template <> std::string SpirvTemplateManager::getOpCapability<deUint64>()	{ return "OpCapability Int64";		}

template <> std::string SpirvTemplateManager::getOpType<double>()	{ return "OpTypeFloat 64";	}
template <> std::string SpirvTemplateManager::getOpType<deInt64>()	{ return "OpTypeInt 64 1";	}
template <> std::string SpirvTemplateManager::getOpType<deUint64>()	{ return "OpTypeInt 64 0";	}

std::string SpirvTemplateManager::getNanCapability (bool preserve)
{
	return (preserve ? "OpCapability SignedZeroInfNanPreserve" : "");
}

std::string SpirvTemplateManager::getNanExtension (bool preserve)
{
	return (preserve ? "OpExtension \"SPV_KHR_float_controls\"" : "");
}

std::string SpirvTemplateManager::getNanExeMode (bool preserve)
{
	return (preserve ? "OpExecutionMode %main SignedZeroInfNanPreserve 64" : "");
}

struct BufferWithMemory
{
	vk::Move<vk::VkBuffer>		buffer;
	de::MovePtr<vk::Allocation>	allocation;

	BufferWithMemory ()
		: buffer(), allocation()
	{}

	BufferWithMemory (BufferWithMemory&& other)
		: buffer(other.buffer), allocation(other.allocation)
	{}

	BufferWithMemory& operator= (BufferWithMemory&& other)
	{
		buffer		= other.buffer;
		allocation	= other.allocation;
		return *this;
	}
};

// Create storage buffer, bind memory to it and return both things.
BufferWithMemory createStorageBuffer(const vk::DeviceInterface&	vkdi,
									 const vk::VkDevice			device,
									 vk::Allocator&				allocator,
									 size_t						numBytes)
{
	const vk::VkBufferCreateInfo bufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
		DE_NULL,									// pNext
		0u,											// flags
		numBytes,									// size
		vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,											// queueFamilyCount
		DE_NULL,									// pQueueFamilyIndices
	};

	BufferWithMemory bufmem;

	bufmem.buffer								= vk::createBuffer(vkdi, device, &bufferCreateInfo);
	const vk::VkMemoryRequirements requirements = getBufferMemoryRequirements(vkdi, device, *bufmem.buffer);
	bufmem.allocation							= allocator.allocate(requirements, vk::MemoryRequirement::HostVisible);

	VK_CHECK(vkdi.bindBufferMemory(device, *bufmem.buffer, bufmem.allocation->getMemory(), bufmem.allocation->getOffset()));

	return bufmem;
}

// Make sure the length of the following vectors is a multiple of 4. This will make sure operands can be reused for vectorized tests.
const OperandsVector<double>	DOUBLE_OPERANDS		=
{
	{	-8.0,	-5.0	},
	{	-5.0,	-8.0	},
	{	-5.0,	-5.0	},
	{	-5.0,	 0.0	},
	{	 0.0,	-5.0	},
	{	 5.0,	 0.0	},
	{	 0.0,	 5.0	},
	{	 0.0,	 0.0	},
	{	-5.0,	 5.0	},
	{	 5.0,	-5.0	},
	{	 5.0,	 8.0	},
	{	 8.0,	 5.0	},
	{	 5.0,	 5.0	},
	{	-6.0,	-5.0	},
	{	 6.0,	 5.0	},
	{	 0.0,	 1.0	},
	{	 1.0,	 0.0	},
	{	 0.0,	 NAN	},
	{	 NAN,	 0.0	},
	{	 NAN,	 NAN	},
};

const OperandsVector<deInt64>	INT64_OPERANDS	=
{
	{	-8,		-5		},
	{	-5,		-8		},
	{	-5,		-5		},
	{	-5,		 0		},
	{	 0,		-5		},
	{	 5,		 0		},
	{	 0,		 5		},
	{	 0,		 0		},
	{	-5,		 5		},
	{	 5,		-5		},
	{	 5,		 8		},
	{	 8,		 5		},
	{	 5,		 5		},
	{	-6,		-5		},
	{	 6,		 5		},
	{	 0,		 1		},
};

constexpr auto					MAX_DEUINT64	= std::numeric_limits<deUint64>::max();
const OperandsVector<deUint64>	UINT64_OPERANDS	=
{
	{	0,					0					},
	{	1,					0					},
	{	0,					1					},
	{	1,					1					},
	{	5,					8					},
	{	8,					5					},
	{	5,					5					},
	{	0,					MAX_DEUINT64		},
	{	MAX_DEUINT64,		0					},
	{	MAX_DEUINT64 - 1,	MAX_DEUINT64		},
	{	MAX_DEUINT64,		MAX_DEUINT64 - 1	},
	{	MAX_DEUINT64,		MAX_DEUINT64		},
};

template <class T>
class T64bitCompareTestInstance : public TestInstance
{
public:
							T64bitCompareTestInstance	(Context& ctx, const TestParameters<T>& params);
	tcu::TestStatus			iterate						(void);

private:
	const TestParameters<T>	m_params;
	const size_t			m_numOperations;
	const size_t			m_inputBufferSize;
	const size_t			m_outputBufferSize;
};

template <class T>
T64bitCompareTestInstance<T>::T64bitCompareTestInstance (Context& ctx, const TestParameters<T>& params)
	: TestInstance(ctx)
	, m_params(params)
	, m_numOperations(m_params.operands.size())
	, m_inputBufferSize(m_numOperations * sizeof(T))
	, m_outputBufferSize(m_numOperations * sizeof(int))
{
}

template<class T>
bool genericIsNan (T)
{
	return false;
}

template<>
bool genericIsNan<double> (double value)
{
	return std::isnan(value);
}

template <class T>
tcu::TestStatus T64bitCompareTestInstance<T>::iterate (void)
{
	DE_ASSERT(m_params.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT		||
			  m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT		||
			  m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT	);

	auto&			vkdi		= m_context.getDeviceInterface();
	auto			device		= m_context.getDevice();
	auto&			allocator	= m_context.getDefaultAllocator();

	// Create storage buffers (left operands, right operands and results buffer).
	BufferWithMemory input1		= createStorageBuffer(vkdi, device, allocator, m_inputBufferSize);
	BufferWithMemory input2		= createStorageBuffer(vkdi, device, allocator, m_inputBufferSize);
	BufferWithMemory output1	= createStorageBuffer(vkdi, device, allocator, m_outputBufferSize);

	// Create an array of buffers.
	std::vector<vk::VkBuffer> buffers;
	buffers.push_back(input1.buffer.get());
	buffers.push_back(input2.buffer.get());
	buffers.push_back(output1.buffer.get());

	// Create descriptor set layout.
	std::vector<vk::VkDescriptorSetLayoutBinding> bindings;
	for (size_t i = 0; i < buffers.size(); ++i)
	{
		vk::VkDescriptorSetLayoutBinding binding =
		{
			static_cast<deUint32>(i),								// uint32_t              binding;
			vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,					// VkDescriptorType      descriptorType;
			1u,														// uint32_t              descriptorCount;
			static_cast<vk::VkShaderStageFlags>(m_params.stage),	// VkShaderStageFlags    stageFlags;
			DE_NULL													// const VkSampler*      pImmutableSamplers;
		};
		bindings.push_back(binding);
	}

	const vk::VkDescriptorSetLayoutCreateInfo layoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,													// const void*                            pNext;
		0,															// VkDescriptorSetLayoutCreateFlags       flags;
		static_cast<deUint32>(bindings.size()),						// uint32_t                               bindingCount;
		bindings.data()												// const VkDescriptorSetLayoutBinding*    pBindings;
	};
	auto descriptorSetLayout = vk::createDescriptorSetLayout(vkdi, device, &layoutCreateInfo);

	// Create descriptor set.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(bindings[0].descriptorType, static_cast<deUint32>(bindings.size()));
	auto descriptorPool = poolBuilder.build(vkdi, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const vk::VkDescriptorSetAllocateInfo allocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,											// const void*                     pNext;
		*descriptorPool,									// VkDescriptorPool                descriptorPool;
		1u,													// uint32_t                        descriptorSetCount;
		&descriptorSetLayout.get()							// const VkDescriptorSetLayout*    pSetLayouts;
	};
	auto descriptorSet = vk::allocateDescriptorSet(vkdi, device, &allocateInfo);

	// Update descriptor set.
	std::vector<vk::VkDescriptorBufferInfo>	descriptorBufferInfos;
	std::vector<vk::VkWriteDescriptorSet>	descriptorWrites;

	descriptorBufferInfos.reserve(buffers.size());
	descriptorWrites.reserve(buffers.size());

	for (size_t i = 0; i < buffers.size(); ++i)
	{
		vk::VkDescriptorBufferInfo bufferInfo =
		{
			buffers[i],		// VkBuffer        buffer;
			0u,				// VkDeviceSize    offset;
			VK_WHOLE_SIZE,	// VkDeviceSize    range;
		};
		descriptorBufferInfos.push_back(bufferInfo);
	}

	for (size_t i = 0; i < buffers.size(); ++i)
	{
		vk::VkWriteDescriptorSet write =
		{
			vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
			DE_NULL,									// const void*                      pNext;
			*descriptorSet,								// VkDescriptorSet                  dstSet;
			static_cast<deUint32>(i),					// uint32_t                         dstBinding;
			0u,											// uint32_t                         dstArrayElement;
			1u,											// uint32_t                         descriptorCount;
			bindings[i].descriptorType,					// VkDescriptorType                 descriptorType;
			DE_NULL,									// const VkDescriptorImageInfo*     pImageInfo;
			&descriptorBufferInfos[i],					// const VkDescriptorBufferInfo*    pBufferInfo;
			DE_NULL,									// const VkBufferView*              pTexelBufferView;
		};
		descriptorWrites.push_back(write);
	}
	vkdi.updateDescriptorSets(device, static_cast<deUint32>(descriptorWrites.size()), descriptorWrites.data(), 0u, DE_NULL);

	// Fill storage buffers with data. Note: VkPhysicalDeviceLimits.minMemoryMapAlignment guarantees this cast is safe.
	T*		input1Ptr	= reinterpret_cast<T*>		(input1.allocation->getHostPtr());
	T*		input2Ptr	= reinterpret_cast<T*>		(input2.allocation->getHostPtr());
	int*	output1Ptr	= reinterpret_cast<int*>	(output1.allocation->getHostPtr());

	for (size_t i = 0; i < m_numOperations; ++i)
	{
		input1Ptr[i] = m_params.operands[i].first;
		input2Ptr[i] = m_params.operands[i].second;
		output1Ptr[i] = -9;
	}

	// Flush memory.
	vk::flushAlloc(vkdi, device, *input1.allocation);
	vk::flushAlloc(vkdi, device, *input2.allocation);
	vk::flushAlloc(vkdi, device, *output1.allocation);

	// Prepare barriers in advance so data is visible to the shaders and the host.
	std::vector<vk::VkBufferMemoryBarrier> hostToDevBarriers;
	std::vector<vk::VkBufferMemoryBarrier> devToHostBarriers;
	for (size_t i = 0; i < buffers.size(); ++i)
	{
		const vk::VkBufferMemoryBarrier hostDev =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType	sType;
			DE_NULL,															// const void*		pNext;
			vk::VK_ACCESS_HOST_WRITE_BIT,										// VkAccessFlags	srcAccessMask;
			(vk::VK_ACCESS_SHADER_READ_BIT | vk::VK_ACCESS_SHADER_WRITE_BIT),	// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			dstQueueFamilyIndex;
			buffers[i],															// VkBuffer			buffer;
			0u,																	// VkDeviceSize		offset;
			VK_WHOLE_SIZE,														// VkDeviceSize		size;
		};
		hostToDevBarriers.push_back(hostDev);

		const vk::VkBufferMemoryBarrier devHost =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType	sType;
			DE_NULL,															// const void*		pNext;
			vk::VK_ACCESS_SHADER_WRITE_BIT,										// VkAccessFlags	srcAccessMask;
			vk::VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			dstQueueFamilyIndex;
			buffers[i],															// VkBuffer			buffer;
			0u,																	// VkDeviceSize		offset;
			VK_WHOLE_SIZE,														// VkDeviceSize		size;
		};
		devToHostBarriers.push_back(devHost);
	}

	// Create command pool and command buffer.
	auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

	const vk::VkCommandPoolCreateInfo cmdPoolCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,		// VkCommandPoolCreateFlags		flags;
		queueFamilyIndex,								// deUint32						queueFamilyIndex;
	};
	auto cmdPool = vk::createCommandPool(vkdi, device, &cmdPoolCreateInfo);

	const vk::VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		*cmdPool,											// VkCommandPool			commandPool;
		vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
		1u,													// deUint32					commandBufferCount;
	};
	auto cmdBuffer = vk::allocateCommandBuffer(vkdi, device, &cmdBufferAllocateInfo);

	// Create pipeline layout.
	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0,													// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout.get(),							// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	auto pipelineLayout = vk::createPipelineLayout(vkdi, device, &pipelineLayoutCreateInfo);

	if (m_params.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)
	{
		// Create compute pipeline.
		auto compShaderModule = createShaderModule(vkdi, device, m_context.getBinaryCollection().get("comp"));

		const vk::VkComputePipelineCreateInfo computeCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType                    sType;
			DE_NULL,											// const void*                        pNext;
			0,													// VkPipelineCreateFlags              flags;
			{													// VkPipelineShaderStageCreateInfo    stage;
				vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
				DE_NULL,													// const void*                         pNext;
				0,															// VkPipelineShaderStageCreateFlags    flags;
				vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
				*compShaderModule,											// VkShaderModule                      module;
				"main",														// const char*                         pName;
				DE_NULL,													// const VkSpecializationInfo*         pSpecializationInfo;
			},
			*pipelineLayout,									// VkPipelineLayout                   layout;
			DE_NULL,											// VkPipeline                         basePipelineHandle;
			0,													// int32_t                            basePipelineIndex;
		};
		auto computePipeline = vk::createComputePipeline(vkdi, device, DE_NULL, &computeCreateInfo);

		// Run the shader.
		vk::beginCommandBuffer(vkdi, *cmdBuffer);
			vkdi.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
			vkdi.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1u, &descriptorSet.get(), 0u, DE_NULL);
			vkdi.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0u, DE_NULL, static_cast<deUint32>(hostToDevBarriers.size()), hostToDevBarriers.data(), 0u, DE_NULL);
			vkdi.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
			vkdi.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, DE_NULL, static_cast<deUint32>(devToHostBarriers.size()), devToHostBarriers.data(), 0u, DE_NULL);
		vk::endCommandBuffer(vkdi, *cmdBuffer);
		vk::submitCommandsAndWait(vkdi, device, m_context.getUniversalQueue(), *cmdBuffer);
	}
	else if (m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT	||
			 m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT	)
	{
		const bool isFrag = (m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT);

		// Create graphics pipeline.
		auto												vertShaderModule = createShaderModule(vkdi, device, m_context.getBinaryCollection().get("vert"));
		vk::Move<vk::VkShaderModule>						fragShaderModule;
		std::vector<vk::VkPipelineShaderStageCreateInfo>	shaderStages;

		const vk::VkPipelineShaderStageCreateInfo vertexStage =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			0,															// VkPipelineShaderStageCreateFlags		flags;
			vk::VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage;
			*vertShaderModule,											// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		};
		shaderStages.push_back(vertexStage);

		if (isFrag)
		{
			fragShaderModule = createShaderModule(vkdi, device, m_context.getBinaryCollection().get("frag"));

			const vk::VkPipelineShaderStageCreateInfo fragmentStage =
			{
				vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0,															// VkPipelineShaderStageCreateFlags		flags;
				vk::VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits				stage;
				*fragShaderModule,											// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
			};
			shaderStages.push_back(fragmentStage);
		}

        const vk::VkPipelineVertexInputStateCreateInfo vertexInputInfo =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,														// const void*									pNext;
			0,																// VkPipelineVertexInputStateCreateFlags		flags;
			0u,																// deUint32										vertexBindingDescriptionCount;
			DE_NULL,														// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
			0u,																// deUint32										vertexAttributeDescriptionCount;
			DE_NULL,														// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
		};

        const vk::VkPipelineInputAssemblyStateCreateInfo inputAssembly =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,															// const void*								pNext;
			0u,																	// VkPipelineInputAssemblyStateCreateFlags	flags;
			vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST,								// VkPrimitiveTopology						topology;
			VK_FALSE,															// VkBool32									primitiveRestartEnable;
		};

		const vk::VkPipelineRasterizationStateCreateInfo rasterizationState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0,																// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClampEnable;
			(isFrag ? VK_FALSE : VK_TRUE),									// VkBool32									rasterizerDiscardEnable;
			vk::VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
			vk::VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
			vk::VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f,															// float									lineWidth;
		};

		const vk::VkSubpassDescription subpassDescription =
		{
			0,										// VkSubpassDescriptionFlags		flags;
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
			0u,										// deUint32							inputAttachmentCount;
			DE_NULL,								// const VkAttachmentReference*		pInputAttachments;
			0u,										// deUint32							colorAttachmentCount;
			DE_NULL,								// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,								// const VkAttachmentReference*		pResolveAttachments;
			DE_NULL,								// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,										// deUint32							preserveAttachmentCount;
			0u,										// const deUint32*					pPreserveAttachments;
		};

		const vk::VkRenderPassCreateInfo renderPassCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0,												// VkRenderPassCreateFlags			flags;
			0u,												// deUint32							attachmentCount;
			DE_NULL,										// const VkAttachmentDescription*	pAttachments;
			1u,												// deUint32							subpassCount;
			&subpassDescription,							// const VkSubpassDescription*		pSubpasses;
			0u,												// deUint32							dependencyCount;
			DE_NULL,										// const VkSubpassDependency*		pDependencies;
		};
		auto renderPass = vk::createRenderPass(vkdi, device, &renderPassCreateInfo);

		std::unique_ptr<vk::VkPipelineMultisampleStateCreateInfo> multisampleState;
		if (isFrag)
		{
			multisampleState.reset(new vk::VkPipelineMultisampleStateCreateInfo);
			*multisampleState =
			{
				vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0,																// VkPipelineMultisampleStateCreateFlags	flags;
				vk::VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
				VK_FALSE,														// VkBool32									sampleShadingEnable;
				0.0f,															// float									minSampleShading;
				DE_NULL,														// const VkSampleMask*						pSampleMask;
				VK_FALSE,														// VkBool32									alphaToCoverageEnable;
				VK_FALSE,														// VkBool32									alphaToOneEnable;
			};
		}

		const vk::VkViewport viewport =
		{
			0.0f,	// float	x;
			0.0f,	// float	y;
			1.0f,	// float	width;
			1.0f,	// float	height;
			0.0f,	// float	minDepth;
			1.0f,	// float	maxDepth;
		};

		const vk::VkRect2D renderArea = { { 0u, 0u }, { 1u, 1u } };

		std::unique_ptr<vk::VkPipelineViewportStateCreateInfo> viewportState;

		if (isFrag)
		{
			viewportState.reset(new vk::VkPipelineViewportStateCreateInfo);
			*viewportState =
			{
				vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0,															// VkPipelineViewportStateCreateFlags	flags;
				1u,															// deUint32							viewportCount;
				&viewport,													// const VkViewport*					pViewports;
				1u,															// deUint32							scissorCount;
				&renderArea,												// const VkRect2D*						pScissors;
			};
		}

		const vk::VkGraphicsPipelineCreateInfo graphicsCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,												// const void*										pNext;
			0,														// VkPipelineCreateFlags							flags;
			static_cast<deUint32>(shaderStages.size()),				// deUint32											stageCount;
			shaderStages.data(),									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputInfo,										// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssembly,											// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,												// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			viewportState.get(),									// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationState,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			multisampleState.get(),									// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,												// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			DE_NULL,												// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,												// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*pipelineLayout,										// VkPipelineLayout									layout;
			*renderPass,											// VkRenderPass										renderPass;
			0u,														// deUint32											subpass;
			DE_NULL,												// VkPipeline										basePipelineHandle;
			0u,														// deInt32											basePipelineIndex;
		};
		auto graphicsPipeline = vk::createGraphicsPipeline(vkdi, device, DE_NULL, &graphicsCreateInfo);

		const vk::VkFramebufferCreateInfo frameBufferCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0,												// VkFramebufferCreateFlags		flags;
			*renderPass,									// VkRenderPass					renderPass;
			0u,												// deUint32						attachmentCount;
			DE_NULL,										// const VkImageView*			pAttachments;
			1u,												// deUint32						width;
			1u,												// deUint32						height;
			1u,												// deUint32						layers;
		};
		auto frameBuffer = vk::createFramebuffer(vkdi, device, &frameBufferCreateInfo);

		const vk::VkRenderPassBeginInfo renderPassBeginInfo =
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*renderPass,									// VkRenderPass			renderPass;
			*frameBuffer,									// VkFramebuffer		framebuffer;
			renderArea,										// VkRect2D				renderArea;
			0u,												// deUint32				clearValueCount;
			DE_NULL,										// const VkClearValue*	pClearValues;
		};

		// Run the shader.
		vk::VkPipelineStageFlags pipelineStage = (isFrag ? vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		vk::beginCommandBuffer(vkdi, *cmdBuffer);
			vkdi.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, pipelineStage, 0, 0u, DE_NULL, static_cast<deUint32>(hostToDevBarriers.size()), hostToDevBarriers.data(), 0u, DE_NULL);
			vkdi.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, vk::VK_SUBPASS_CONTENTS_INLINE);
				vkdi.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
				vkdi.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1u, &descriptorSet.get(), 0u, DE_NULL);
				vkdi.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
			vkdi.cmdEndRenderPass(*cmdBuffer);
			vkdi.cmdPipelineBarrier(*cmdBuffer, pipelineStage, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, DE_NULL, static_cast<deUint32>(devToHostBarriers.size()), devToHostBarriers.data(), 0u, DE_NULL);
		vk::endCommandBuffer(vkdi, *cmdBuffer);
		vk::submitCommandsAndWait(vkdi, device, m_context.getUniversalQueue(), *cmdBuffer);
	}

	// Invalidate allocations.
	vk::invalidateAlloc(vkdi, device, *input1.allocation);
	vk::invalidateAlloc(vkdi, device, *input2.allocation);
	vk::invalidateAlloc(vkdi, device, *output1.allocation);

	// Read and verify results.
	std::vector<int> results(m_numOperations);
	deMemcpy(results.data(), output1.allocation->getHostPtr(), m_outputBufferSize);
	for (size_t i = 0; i < m_numOperations; ++i)
	{
		int expected = static_cast<int>(m_params.operation.run(m_params.operands[i].first, m_params.operands[i].second));
		if (results[i] != expected && (m_params.requireNanPreserve || (!genericIsNan<T>(m_params.operands[i].first) && !genericIsNan<T>(m_params.operands[i].second))))
		{
			std::ostringstream msg;
			msg << "Invalid result found in position " << i << ": expected " << expected << " and found " << results[i];
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

template <class T>
class T64bitCompareTest : public TestCase
{
public:
							T64bitCompareTest	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParameters<T>& params);
	virtual void			checkSupport		(Context& context) const;
	virtual void			initPrograms		(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& ctx) const;

private:
	const TestParameters<T>	m_params;
};

template <class T>
T64bitCompareTest<T>::T64bitCompareTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParameters<T>& params)
	: TestCase(testCtx, name, description), m_params(params)
{
	// This is needed so that the same operands can be used for single-element comparisons or for vectorized comparisons (which use *vec4 types).
	DE_ASSERT(m_params.operands.size() % 4 == 0);
}

// This template checks the needed type support features in shaders for type T.
// Specializations are provided below.
template <class T>
void checkTypeSupport(const vk::VkPhysicalDeviceFeatures& features);

template <>
void checkTypeSupport<double>(const vk::VkPhysicalDeviceFeatures& features)
{
	if (!features.shaderFloat64)
		TCU_THROW(NotSupportedError, "64-bit floats not supported in shaders");
}

void check64bitIntegers(const vk::VkPhysicalDeviceFeatures& features)
{
	if (!features.shaderInt64)
		TCU_THROW(NotSupportedError, "64-bit integer types not supported in shaders");
}

template <>
void checkTypeSupport<deInt64>(const vk::VkPhysicalDeviceFeatures& features)
{
	check64bitIntegers(features);
}

template <>
void checkTypeSupport<deUint64>(const vk::VkPhysicalDeviceFeatures& features)
{
	check64bitIntegers(features);
}

template <class T>
void T64bitCompareTest<T>::checkSupport (Context& context) const
{
	auto&	vki				= context.getInstanceInterface();
	auto	physicalDevice	= context.getPhysicalDevice();
	auto	features		= vk::getPhysicalDeviceFeatures(vki, physicalDevice);

	checkTypeSupport<T>(features);

	switch (m_params.stage)
	{
	case vk::VK_SHADER_STAGE_COMPUTE_BIT:
		break;
	case vk::VK_SHADER_STAGE_VERTEX_BIT:
		if (!features.vertexPipelineStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Vertex shader does not support stores");
		break;
	case vk::VK_SHADER_STAGE_FRAGMENT_BIT:
		if (!features.fragmentStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Fragment shader does not support stores");
		break;
	default:
		DE_ASSERT(DE_NULL == "Invalid shader stage specified");
	}

	vk::VkPhysicalDeviceFloatControlsProperties fcFeatures;
	deMemset(&fcFeatures, 0, sizeof(fcFeatures));
	fcFeatures.shaderSignedZeroInfNanPreserveFloat64 = VK_TRUE;

	const char *unused;
	if (m_params.requireNanPreserve && !isFloatControlsFeaturesSupported(context, fcFeatures, &unused))
		TCU_THROW(NotSupportedError, "NaN preservation not supported");
}

template <class T>
void T64bitCompareTest<T>::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(m_params.stage == vk::VK_SHADER_STAGE_COMPUTE_BIT		||
			  m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT		||
			  m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT	);

	std::map<std::string, std::string> replacements;
	replacements["ITERS"]			= de::toString((m_params.dataType == DATA_TYPE_SINGLE) ? m_params.operands.size() : m_params.operands.size() / 4);
	replacements["OPNAME"]			= m_params.operation.spirvName();
	replacements["OPCAPABILITY"]	= SpirvTemplateManager::getOpCapability<T>();
	replacements["OPTYPE"]			= SpirvTemplateManager::getOpType<T>();
	replacements["NANCAP"]			= SpirvTemplateManager::getNanCapability(m_params.requireNanPreserve);
	replacements["NANEXT"]			= SpirvTemplateManager::getNanExtension(m_params.requireNanPreserve);
	replacements["NANMODE"]			= SpirvTemplateManager::getNanExeMode(m_params.requireNanPreserve);

	static const std::map<vk::VkShaderStageFlagBits, std::string>	sourceNames			=
	{
		std::make_pair( vk::VK_SHADER_STAGE_COMPUTE_BIT,	"comp"	),
		std::make_pair( vk::VK_SHADER_STAGE_VERTEX_BIT,		"vert"	),
		std::make_pair( vk::VK_SHADER_STAGE_FRAGMENT_BIT,	"frag"	),
	};

	// Add the proper template under the proper name.
	programCollection.spirvAsmSources.add(sourceNames.find(m_params.stage)->second) << SpirvTemplateManager::getTemplate(m_params.dataType, m_params.stage).specialize(replacements);

	// Add the passthrough vertex shader needed for the fragment shader.
	if (m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		programCollection.glslSources.add("vert") << glu::VertexSource(VertShaderPassThrough);
}

template <class T>
TestInstance* T64bitCompareTest<T>::createInstance (Context& ctx) const
{
	return new T64bitCompareTestInstance<T>(ctx, m_params);
}

const std::map<bool, std::string>		requireNanName =
{
	std::make_pair( false,	"nonan"		),
	std::make_pair( true,	"withnan"	),
};

const std::map<DataType, std::string>	dataTypeName =
{
	std::make_pair(DATA_TYPE_SINGLE, "single"),
	std::make_pair(DATA_TYPE_VECTOR, "vector"),
};

using StageName = std::map<vk::VkShaderStageFlagBits, std::string>;

void createDoubleCompareTestsInGroup (tcu::TestCaseGroup* tests, const StageName* stageNames)
{
	static const std::vector<const CompareOperation<double>*> operationList =
	{
		// Ordered operations.
		&FOrderedEqualOp,
		&FOrderedNotEqualOp,
		&FOrderedLessThanOp,
		&FOrderedLessThanEqualOp,
		&FOrderedGreaterThanOp,
		&FOrderedGreaterThanEqualOp,
		// Unordered operations.
		&FUnorderedEqualOp,
		&FUnorderedNotEqualOp,
		&FUnorderedLessThanOp,
		&FUnorderedLessThanEqualOp,
		&FUnorderedGreaterThanOp,
		&FUnorderedGreaterThanEqualOp,
	};

	for (const auto&	stageNamePair	: *stageNames)
	for (const auto&	typeNamePair	: dataTypeName)
	for (const auto&	requireNanPair	: requireNanName)
	for (const auto		opPtr			: operationList)
	{
		TestParameters<double>	params		= { typeNamePair.first, *opPtr, stageNamePair.first, DOUBLE_OPERANDS, requireNanPair.first };
		std::string				testName	= stageNamePair.second + "_" + de::toLower(opPtr->spirvName()) + "_" + requireNanPair.second + "_" + typeNamePair.second;
		tests->addChild(new T64bitCompareTest<double>(tests->getTestContext(), testName, "", params));
	}
}

void createInt64CompareTestsInGroup (tcu::TestCaseGroup* tests, const StageName* stageNames)
{
	static const std::vector<const CompareOperation<deInt64>*> operationList =
	{
		&int64EqualOp,
		&int64NotEqualOp,
		&int64LessThanOp,
		&int64LessThanEqualOp,
		&int64GreaterThanOp,
		&int64GreaterThanEqualOp,
	};

	for (const auto&	stageNamePair	: *stageNames)
	for (const auto&	typeNamePair	: dataTypeName)
	for (const auto		opPtr			: operationList)
	{
		TestParameters<deInt64>	params		= { typeNamePair.first, *opPtr, stageNamePair.first, INT64_OPERANDS, false };
		std::string				testName	= stageNamePair.second + "_" + de::toLower(opPtr->spirvName()) + "_" + typeNamePair.second;
		tests->addChild(new T64bitCompareTest<deInt64>(tests->getTestContext(), testName, "", params));
	}
}

void createUint64CompareTestsInGroup (tcu::TestCaseGroup* tests, const StageName* stageNames)
{
	static const std::vector<const CompareOperation<deUint64>*> operationList =
	{
		&uint64EqualOp,
		&uint64NotEqualOp,
		&uint64LessThanOp,
		&uint64LessThanEqualOp,
		&uint64GreaterThanOp,
		&uint64GreaterThanEqualOp,
	};

	for (const auto&	stageNamePair	: *stageNames)
	for (const auto&	typeNamePair	: dataTypeName)
	for (const auto		opPtr			: operationList)
	{
		TestParameters<deUint64>	params		= { typeNamePair.first, *opPtr, stageNamePair.first, UINT64_OPERANDS, false };
		std::string					testName	= stageNamePair.second + "_" + de::toLower(opPtr->spirvName()) + "_" + typeNamePair.second;
		tests->addChild(new T64bitCompareTest<deUint64>(tests->getTestContext(), testName, "", params));
	}
}

struct TestMgr
{
	typedef void (*CreationFunctionPtr)(tcu::TestCaseGroup*, const StageName*);

	static const char* getParentGroupName () { return "64bit_compare"; }
	static const char* getParentGroupDesc () { return "64-bit type comparison operations"; }

	template <class T>
	static std::string getGroupName ();

	template <class T>
	static std::string getGroupDesc ();

	template <class T>
	static CreationFunctionPtr getCreationFunction ();
};

template <> std::string TestMgr::getGroupName<double>()		{ return "double";	}
template <> std::string TestMgr::getGroupName<deInt64>()	{ return "int64";	}
template <> std::string TestMgr::getGroupName<deUint64>()	{ return "uint64";	}

template <> std::string TestMgr::getGroupDesc<double>()		{ return "64-bit floating point tests";		}
template <> std::string TestMgr::getGroupDesc<deInt64>()	{ return "64-bit signed integer tests";		}
template <> std::string TestMgr::getGroupDesc<deUint64>()	{ return "64-bit unsigned integer tests";	}

template <> TestMgr::CreationFunctionPtr TestMgr::getCreationFunction<double> ()	{ return createDoubleCompareTestsInGroup;	}
template <> TestMgr::CreationFunctionPtr TestMgr::getCreationFunction<deInt64> ()	{ return createInt64CompareTestsInGroup;	}
template <> TestMgr::CreationFunctionPtr TestMgr::getCreationFunction<deUint64> ()	{ return createUint64CompareTestsInGroup;	}

} // anonymous

tcu::TestCaseGroup* create64bitCompareGraphicsGroup (tcu::TestContext& testCtx)
{
	static const StageName graphicStages =
	{
		std::make_pair(vk::VK_SHADER_STAGE_VERTEX_BIT,		"vert"),
		std::make_pair(vk::VK_SHADER_STAGE_FRAGMENT_BIT,	"frag"),
	};

	tcu::TestCaseGroup* newGroup = new tcu::TestCaseGroup(testCtx, TestMgr::getParentGroupName(), TestMgr::getParentGroupDesc());
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<double>(),	TestMgr::getGroupDesc<double>(),	TestMgr::getCreationFunction<double>(),		&graphicStages));
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<deInt64>(),	TestMgr::getGroupDesc<deInt64>(),	TestMgr::getCreationFunction<deInt64>(),	&graphicStages));
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<deUint64>(),	TestMgr::getGroupDesc<deUint64>(),	TestMgr::getCreationFunction<deUint64>(),	&graphicStages));
	return newGroup;
}

tcu::TestCaseGroup* create64bitCompareComputeGroup (tcu::TestContext& testCtx)
{
	static const StageName computeStages =
	{
		std::make_pair(vk::VK_SHADER_STAGE_COMPUTE_BIT,		"comp"),
	};

	tcu::TestCaseGroup* newGroup = new tcu::TestCaseGroup(testCtx, TestMgr::getParentGroupName(), TestMgr::getParentGroupDesc());
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<double>(),	TestMgr::getGroupDesc<double>(),	TestMgr::getCreationFunction<double>(),		&computeStages));
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<deInt64>(),	TestMgr::getGroupDesc<deInt64>(),	TestMgr::getCreationFunction<deInt64>(),	&computeStages));
	newGroup->addChild(createTestGroup(testCtx, TestMgr::getGroupName<deUint64>(),	TestMgr::getGroupDesc<deUint64>(),	TestMgr::getCreationFunction<deUint64>(),	&computeStages));
	return newGroup;
}

} // SpirVAssembly
} // vkt
