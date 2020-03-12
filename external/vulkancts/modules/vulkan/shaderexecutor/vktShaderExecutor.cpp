/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Vulkan ShaderExecutor
 *//*--------------------------------------------------------------------*/

#include "vktShaderExecutor.hpp"

#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "gluShaderUtil.hpp"

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deFloat16.h"

#include <map>
#include <sstream>
#include <iostream>

using std::vector;
using namespace vk;

namespace vkt
{
namespace shaderexecutor
{
namespace
{

enum
{
	DEFAULT_RENDER_WIDTH	= 100,
	DEFAULT_RENDER_HEIGHT	= 100,
};

// Common typedefs

typedef de::SharedPtr<Unique<VkImage> >		VkImageSp;
typedef de::SharedPtr<Unique<VkImageView> >	VkImageViewSp;
typedef de::SharedPtr<Unique<VkBuffer> >	VkBufferSp;
typedef de::SharedPtr<Allocation>			AllocationSp;

static VkFormat getAttributeFormat(const glu::DataType dataType);

// Shader utilities

static VkClearValue	getDefaultClearColor (void)
{
	return makeClearValueColorF32(0.125f, 0.25f, 0.5f, 1.0f);
}

static std::string generateEmptyFragmentSource (void)
{
	std::ostringstream src;

	src << "#version 450\n"
		   "layout(location=0) out highp vec4 o_color;\n";

	src << "void main (void)\n{\n";
	src << "	o_color = vec4(0.0);\n";
	src << "}\n";

	return src.str();
}

void packFloat16Bit (std::ostream& src, const std::vector<Symbol>& outputs)
{
	for (vector<Symbol>::const_iterator symIter = outputs.begin(); symIter != outputs.end(); ++symIter)
	{
		if(glu::isDataTypeFloatType(symIter->varType.getBasicType()))
		{
			if(glu::isDataTypeVector(symIter->varType.getBasicType()))
			{
				for(int i = 0; i < glu::getDataTypeScalarSize(symIter->varType.getBasicType()); i++)
				{
					src << "\tpacked_" << symIter->name << "[" << i << "] = uintBitsToFloat(packFloat2x16(f16vec2(" << symIter->name << "[" << i << "], -1.0)));\n";
				}
			}
			else if (glu::isDataTypeMatrix(symIter->varType.getBasicType()))
			{
				int maxRow = 0;
				int maxCol = 0;
				switch (symIter->varType.getBasicType())
				{
				case glu::TYPE_FLOAT_MAT2:
					maxRow = maxCol = 2;
					break;
				case glu::TYPE_FLOAT_MAT2X3:
					maxRow = 2;
					maxCol = 3;
					break;
				case glu::TYPE_FLOAT_MAT2X4:
					maxRow = 2;
					maxCol = 4;
					break;
				case glu::TYPE_FLOAT_MAT3X2:
					maxRow = 3;
					maxCol = 2;
					break;
				case glu::TYPE_FLOAT_MAT3:
					maxRow = maxCol = 3;
					break;
				case glu::TYPE_FLOAT_MAT3X4:
					maxRow = 3;
					maxCol = 4;
					break;
				case glu::TYPE_FLOAT_MAT4X2:
					maxRow = 4;
					maxCol = 2;
					break;
				case glu::TYPE_FLOAT_MAT4X3:
					maxRow = 4;
					maxCol = 3;
					break;
				case glu::TYPE_FLOAT_MAT4:
					maxRow = maxCol = 4;
					break;
				default:
					DE_ASSERT(false);
					break;
				}

				for(int i = 0; i < maxRow; i++)
				for(int j = 0; j < maxCol; j++)
				{
					src << "\tpacked_" << symIter->name << "[" << i << "][" << j << "] = uintBitsToFloat(packFloat2x16(f16vec2(" << symIter->name << "[" << i << "][" << j << "], -1.0)));\n";
				}
			}
			else
			{
					src << "\tpacked_" << symIter->name << " = uintBitsToFloat(packFloat2x16(f16vec2(" << symIter->name << ", -1.0)));\n";
			}
		}
	}
}

static std::string generatePassthroughVertexShader (const ShaderSpec& shaderSpec, const char* inputPrefix, const char* outputPrefix)
{
	std::ostringstream	src;
	int					location	= 0;

	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "layout(location = " << location << ") in highp vec4 a_position;\n";

	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
	{
		location++;
		src << "layout(location = "<< location << ") in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n"
			<< "layout(location = " << location - 1 << ") flat out " << glu::declare(input->varType, outputPrefix + input->name) << ";\n";
	}

	src << "\nvoid main (void)\n{\n"
		<< "	gl_Position = a_position;\n"
		<< "	gl_PointSize = 1.0;\n";

	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
		src << "\t" << outputPrefix << input->name << " = " << inputPrefix << input->name << ";\n";

	src << "}\n";

	return src.str();
}

static std::string generateVertexShader (const ShaderSpec& shaderSpec, const std::string& inputPrefix, const std::string& outputPrefix)
{
	DE_ASSERT(!inputPrefix.empty() && !outputPrefix.empty());

	std::ostringstream	src;

	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "layout(location = 0) in highp vec4 a_position;\n";

	int			locationNumber	= 1;
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input, ++locationNumber)
	{
		src <<  "layout(location = " << locationNumber << ") in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n";
	}

	locationNumber = 0;
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output, ++locationNumber)
	{
		DE_ASSERT(output->varType.isBasicType());

		if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::VarType		intType		(intBaseType, glu::PRECISION_HIGHP);

			src << "layout(location = " << locationNumber << ") flat out " << glu::declare(intType, outputPrefix + output->name) << ";\n";
		}
		else
			src << "layout(location = " << locationNumber << ") flat out " << glu::declare(output->varType, outputPrefix + output->name) << ";\n";
	}

	src << "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position = a_position;\n"
		<< "	gl_PointSize = 1.0;\n";

	// Declare & fetch local input variables
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
	{
		if (shaderSpec.packFloat16Bit && isDataTypeFloatOrVec(input->varType.getBasicType()))
		{
			const std::string tname = glu::getDataTypeName(getDataTypeFloat16Scalars(input->varType.getBasicType()));
			src << "\t" << tname << " " << input->name << " = " << tname << "(" << inputPrefix << input->name << ");\n";
		}
		else
			src << "\t" << glu::declare(input->varType, input->name) << " = " << inputPrefix << input->name << ";\n";
	}

	// Declare local output variables
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		if (shaderSpec.packFloat16Bit && isDataTypeFloatOrVec(output->varType.getBasicType()))
		{
			const std::string tname = glu::getDataTypeName(getDataTypeFloat16Scalars(output->varType.getBasicType()));
			src << "\t" << tname << " " << output->name << ";\n";
			const char* tname2 = glu::getDataTypeName(output->varType.getBasicType());
			src << "\t" << tname2 << " " << "packed_" << output->name << ";\n";
		}
		else
			src << "\t" << glu::declare(output->varType, output->name) << ";\n";
	}

	// Operation - indented to correct level.
	{
		std::istringstream	opSrc	(shaderSpec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	if (shaderSpec.packFloat16Bit)
		packFloat16Bit(src, shaderSpec.outputs);

	// Assignments to outputs.
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		if (shaderSpec.packFloat16Bit && isDataTypeFloatOrVec(output->varType.getBasicType()))
		{
			src << "\t" << outputPrefix << output->name << " = packed_" << output->name << ";\n";
		}
		else
		{
			if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
			{
				const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
				const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;

				src << "\t" << outputPrefix << output->name << " = " << glu::getDataTypeName(intBaseType) << "(" << output->name << ");\n";
			}
			else
				src << "\t" << outputPrefix << output->name << " = " << output->name << ";\n";
		}
	}

	src << "}\n";

	return src.str();
}

struct FragmentOutputLayout
{
	std::vector<const Symbol*>		locationSymbols;		//! Symbols by location
	std::map<std::string, int>		locationMap;			//! Map from symbol name to start location
};

static void generateFragShaderOutputDecl (std::ostream& src, const ShaderSpec& shaderSpec, bool useIntOutputs, const std::map<std::string, int>& outLocationMap, const std::string& outputPrefix)
{
	for (int outNdx = 0; outNdx < (int)shaderSpec.outputs.size(); ++outNdx)
	{
		const Symbol&				output		= shaderSpec.outputs[outNdx];
		const int					location	= de::lookup(outLocationMap, output.name);
		const std::string			outVarName	= outputPrefix + output.name;
		glu::VariableDeclaration	decl		(output.varType, outVarName, glu::STORAGE_OUT, glu::INTERPOLATION_LAST, glu::Layout(location));

		TCU_CHECK_INTERNAL(output.varType.isBasicType());

		if (useIntOutputs && glu::isDataTypeFloatOrVec(output.varType.getBasicType()))
		{
			const int			vecSize			= glu::getDataTypeScalarSize(output.varType.getBasicType());
			const glu::DataType	uintBasicType	= vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT;
			const glu::VarType	uintType		(uintBasicType, glu::PRECISION_HIGHP);

			decl.varType = uintType;
			src << decl << ";\n";
		}
		else if (glu::isDataTypeBoolOrBVec(output.varType.getBasicType()))
		{
			const int			vecSize			= glu::getDataTypeScalarSize(output.varType.getBasicType());
			const glu::DataType	intBasicType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::VarType	intType			(intBasicType, glu::PRECISION_HIGHP);

			decl.varType = intType;
			src << decl << ";\n";
		}
		else if (glu::isDataTypeMatrix(output.varType.getBasicType()))
		{
			const int			vecSize			= glu::getDataTypeMatrixNumRows(output.varType.getBasicType());
			const int			numVecs			= glu::getDataTypeMatrixNumColumns(output.varType.getBasicType());
			const glu::DataType	uintBasicType	= glu::getDataTypeUintVec(vecSize);
			const glu::VarType	uintType		(uintBasicType, glu::PRECISION_HIGHP);

			decl.varType = uintType;
			for (int vecNdx = 0; vecNdx < numVecs; ++vecNdx)
			{
				decl.name				= outVarName + "_" + de::toString(vecNdx);
				decl.layout.location	= location + vecNdx;
				src << decl << ";\n";
			}
		}
		else
			src << decl << ";\n";
	}
}

static void generateFragShaderOutAssign (std::ostream& src, const ShaderSpec& shaderSpec, bool useIntOutputs, const std::string& valuePrefix, const std::string& outputPrefix, const bool isInput16Bit = false)
{
	if (isInput16Bit)
		packFloat16Bit(src, shaderSpec.outputs);

	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		const std::string packPrefix = (isInput16Bit && glu::isDataTypeFloatType(output->varType.getBasicType())) ? "packed_" : "";

		if (useIntOutputs && glu::isDataTypeFloatOrVec(output->varType.getBasicType()))
			src << "	o_" << output->name << " = floatBitsToUint(" << valuePrefix << output->name << ");\n";
		else if (glu::isDataTypeMatrix(output->varType.getBasicType()))
		{
			const int	numVecs		= glu::getDataTypeMatrixNumColumns(output->varType.getBasicType());

			for (int vecNdx = 0; vecNdx < numVecs; ++vecNdx)
				if (useIntOutputs)
					src << "\t" << outputPrefix << output->name << "_" << vecNdx << " = floatBitsToUint(" << valuePrefix << output->name << "[" << vecNdx << "]);\n";
				else
					src << "\t" << outputPrefix << output->name << "_" << vecNdx << " = " << packPrefix << valuePrefix << output->name << "[" << vecNdx << "];\n";
		}
		else if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;

			src << "\t" << outputPrefix << output->name << " = " << glu::getDataTypeName(intBaseType) << "(" << valuePrefix << output->name << ");\n";
		}
		else
			src << "\t" << outputPrefix << output->name << " = " << packPrefix << valuePrefix << output->name << ";\n";
	}
}

static std::string generatePassthroughFragmentShader (const ShaderSpec& shaderSpec, bool useIntOutputs, const std::map<std::string, int>& outLocationMap, const std::string& inputPrefix, const std::string& outputPrefix)
{
	std::ostringstream	src;

	src <<"#version 450\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	int locationNumber = 0;
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output, ++locationNumber)
	{
		if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::VarType		intType		(intBaseType, glu::PRECISION_HIGHP);

			src << "layout(location = " << locationNumber << ") flat in " << glu::declare(intType, inputPrefix + output->name) << ";\n";
		}
		else
			src << "layout(location = " << locationNumber << ") flat in " << glu::declare(output->varType, inputPrefix + output->name) << ";\n";
	}

	generateFragShaderOutputDecl(src, shaderSpec, useIntOutputs, outLocationMap, outputPrefix);

	src << "\nvoid main (void)\n{\n";

	generateFragShaderOutAssign(src, shaderSpec, useIntOutputs, inputPrefix, outputPrefix);

	src << "}\n";

	return src.str();
}

static std::string generateGeometryShader (const ShaderSpec& shaderSpec, const std::string& inputPrefix, const std::string& outputPrefix, const bool pointSizeSupported)
{
	DE_ASSERT(!inputPrefix.empty() && !outputPrefix.empty());

	std::ostringstream	src;

	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";

	if (shaderSpec.glslVersion == glu::GLSL_VERSION_310_ES)
		src << "#extension GL_EXT_geometry_shader : require\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "layout(points) in;\n"
		<< "layout(points, max_vertices = 1) out;\n";

	int locationNumber = 0;
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input, ++locationNumber)
		src << "layout(location = " << locationNumber << ") flat in " << glu::declare(input->varType, inputPrefix + input->name) << "[];\n";

	locationNumber = 0;
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output, ++locationNumber)
	{
		DE_ASSERT(output->varType.isBasicType());

		if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
			const glu::VarType		intType		(intBaseType, glu::PRECISION_HIGHP);

			src << "layout(location = " << locationNumber << ") flat out " << glu::declare(intType, outputPrefix + output->name) << ";\n";
		}
		else
			src << "layout(location = " << locationNumber << ") flat out " << glu::declare(output->varType, outputPrefix + output->name) << ";\n";
	}

	src << "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position = gl_in[0].gl_Position;\n"
		<< (pointSizeSupported ? "	gl_PointSize = gl_in[0].gl_PointSize;\n\n" : "");

	// Fetch input variables
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
		src << "\t" << glu::declare(input->varType, input->name) << " = " << inputPrefix << input->name << "[0];\n";

	// Declare local output variables.
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
		src << "\t" << glu::declare(output->varType, output->name) << ";\n";

	src << "\n";

	// Operation - indented to correct level.
	{
		std::istringstream	opSrc	(shaderSpec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	// Assignments to outputs.
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;

			src << "\t" << outputPrefix << output->name << " = " << glu::getDataTypeName(intBaseType) << "(" << output->name << ");\n";
		}
		else
			src << "\t" << outputPrefix << output->name << " = " << output->name << ";\n";
	}

	src << "	EmitVertex();\n"
		<< "	EndPrimitive();\n"
		<< "}\n";

	return src.str();
}

static std::string generateFragmentShader (const ShaderSpec& shaderSpec, bool useIntOutputs, const std::map<std::string, int>& outLocationMap, const std::string& inputPrefix, const std::string& outputPrefix)
{
	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";
	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	int			locationNumber	= 0;
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input, ++locationNumber)
	{
		src << "layout(location = " << locationNumber << ") flat in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n";
	}

	generateFragShaderOutputDecl(src, shaderSpec, useIntOutputs, outLocationMap, outputPrefix);

	src << "\nvoid main (void)\n{\n";

	// Declare & fetch local input variables
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
	{
		if (shaderSpec.packFloat16Bit && isDataTypeFloatOrVec(input->varType.getBasicType()))
		{
			const std::string tname = glu::getDataTypeName(getDataTypeFloat16Scalars(input->varType.getBasicType()));
			src << "\t" << tname << " " << input->name << " = " << tname << "(" << inputPrefix << input->name << ");\n";
		}
		else
			src << "\t" << glu::declare(input->varType, input->name) << " = " << inputPrefix << input->name << ";\n";
	}

	// Declare output variables
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		if (shaderSpec.packFloat16Bit && isDataTypeFloatOrVec(output->varType.getBasicType()))
		{
			const std::string tname = glu::getDataTypeName(getDataTypeFloat16Scalars(output->varType.getBasicType()));
			src << "\t" << tname << " " << output->name << ";\n";
			const char* tname2 = glu::getDataTypeName(output->varType.getBasicType());
			src << "\t" << tname2 << " " << "packed_" << output->name << ";\n";
		}
		else
			src << "\t" << glu::declare(output->varType, output->name) << ";\n";
	}

	// Operation - indented to correct level.
	{
		std::istringstream	opSrc	(shaderSpec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	generateFragShaderOutAssign(src, shaderSpec, useIntOutputs, "", outputPrefix, shaderSpec.packFloat16Bit);

	src << "}\n";

	return src.str();
}

// FragmentOutExecutor

class FragmentOutExecutor : public ShaderExecutor
{
public:
														FragmentOutExecutor		(Context& context, glu::ShaderType shaderType, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual												~FragmentOutExecutor	(void);

	virtual void										execute					(int					numValues,
																				 const void* const*		inputs,
																				 void* const*			outputs,
																				 VkDescriptorSet		extraResources);

protected:
	const glu::ShaderType								m_shaderType;
	const FragmentOutputLayout							m_outputLayout;

private:
	void												bindAttributes			(int					numValues,
																				 const void* const*		inputs);

	void												addAttribute			(deUint32				bindingLocation,
																				 VkFormat				format,
																				 deUint32				sizePerElement,
																				 deUint32				count,
																				 const void*			dataPtr);
	// reinit render data members
	virtual void										clearRenderData			(void);

	const VkDescriptorSetLayout							m_extraResourcesLayout;

	std::vector<VkVertexInputBindingDescription>		m_vertexBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>		m_vertexAttributeDescriptions;
	std::vector<VkBufferSp>								m_vertexBuffers;
	std::vector<AllocationSp>							m_vertexBufferAllocs;
};

static FragmentOutputLayout computeFragmentOutputLayout (const std::vector<Symbol>& symbols)
{
	FragmentOutputLayout	ret;
	int						location	= 0;

	for (std::vector<Symbol>::const_iterator it = symbols.begin(); it != symbols.end(); ++it)
	{
		const int	numLocations	= glu::getDataTypeNumLocations(it->varType.getBasicType());

		TCU_CHECK_INTERNAL(!de::contains(ret.locationMap, it->name));
		de::insert(ret.locationMap, it->name, location);
		location += numLocations;

		for (int ndx = 0; ndx < numLocations; ++ndx)
			ret.locationSymbols.push_back(&*it);
	}

	return ret;
}

FragmentOutExecutor::FragmentOutExecutor (Context& context, glu::ShaderType shaderType, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: ShaderExecutor			(context, shaderSpec)
	, m_shaderType				(shaderType)
	, m_outputLayout			(computeFragmentOutputLayout(m_shaderSpec.outputs))
	, m_extraResourcesLayout	(extraResourcesLayout)
{
	const VkPhysicalDevice		physicalDevice = m_context.getPhysicalDevice();
	const InstanceInterface&	vki = m_context.getInstanceInterface();

	// Input attributes
	for (int inputNdx = 0; inputNdx < (int)m_shaderSpec.inputs.size(); inputNdx++)
	{
		const Symbol&				symbol = m_shaderSpec.inputs[inputNdx];
		const glu::DataType			basicType = symbol.varType.getBasicType();
		const VkFormat				format = getAttributeFormat(basicType);
		const VkFormatProperties	formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);
		if ((formatProperties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0)
			TCU_THROW(NotSupportedError, "format not supported by device as vertex buffer attribute format");
	}
}

FragmentOutExecutor::~FragmentOutExecutor (void)
{
}

static std::vector<tcu::Vec2> computeVertexPositions (int numValues, const tcu::IVec2& renderSize)
{
	std::vector<tcu::Vec2> positions(numValues);
	for (int valNdx = 0; valNdx < numValues; valNdx++)
	{
		const int		ix		= valNdx % renderSize.x();
		const int		iy		= valNdx / renderSize.x();
		const float		fx		= -1.0f + 2.0f*((float(ix) + 0.5f) / float(renderSize.x()));
		const float		fy		= -1.0f + 2.0f*((float(iy) + 0.5f) / float(renderSize.y()));

		positions[valNdx] = tcu::Vec2(fx, fy);
	}

	return positions;
}

static tcu::TextureFormat getRenderbufferFormatForOutput (const glu::VarType& outputType, bool useIntOutputs)
{
	const tcu::TextureFormat::ChannelOrder channelOrderMap[] =
	{
		tcu::TextureFormat::R,
		tcu::TextureFormat::RG,
		tcu::TextureFormat::RGBA,	// No RGB variants available.
		tcu::TextureFormat::RGBA
	};

	const glu::DataType					basicType		= outputType.getBasicType();
	const int							numComps		= glu::getDataTypeNumComponents(basicType);
	tcu::TextureFormat::ChannelType		channelType;

	switch (glu::getDataTypeScalarType(basicType))
	{
		case glu::TYPE_UINT:	channelType = tcu::TextureFormat::UNSIGNED_INT32;														break;
		case glu::TYPE_INT:		channelType = tcu::TextureFormat::SIGNED_INT32;															break;
		case glu::TYPE_BOOL:	channelType = tcu::TextureFormat::SIGNED_INT32;															break;
		case glu::TYPE_FLOAT:	channelType = useIntOutputs ? tcu::TextureFormat::UNSIGNED_INT32 : tcu::TextureFormat::FLOAT;			break;
		case glu::TYPE_FLOAT16:	channelType = useIntOutputs ? tcu::TextureFormat::UNSIGNED_INT32 : tcu::TextureFormat::HALF_FLOAT;		break;
		default:
			throw tcu::InternalError("Invalid output type");
	}

	DE_ASSERT(de::inRange<int>(numComps, 1, DE_LENGTH_OF_ARRAY(channelOrderMap)));

	return tcu::TextureFormat(channelOrderMap[numComps-1], channelType);
}

static VkFormat getAttributeFormat (const glu::DataType dataType)
{
	switch (dataType)
	{
		case glu::TYPE_FLOAT16:			return VK_FORMAT_R16_SFLOAT;
		case glu::TYPE_FLOAT16_VEC2:	return VK_FORMAT_R16G16_SFLOAT;
		case glu::TYPE_FLOAT16_VEC3:	return VK_FORMAT_R16G16B16_SFLOAT;
		case glu::TYPE_FLOAT16_VEC4:	return VK_FORMAT_R16G16B16A16_SFLOAT;

		case glu::TYPE_FLOAT:			return VK_FORMAT_R32_SFLOAT;
		case glu::TYPE_FLOAT_VEC2:		return VK_FORMAT_R32G32_SFLOAT;
		case glu::TYPE_FLOAT_VEC3:		return VK_FORMAT_R32G32B32_SFLOAT;
		case glu::TYPE_FLOAT_VEC4:		return VK_FORMAT_R32G32B32A32_SFLOAT;

		case glu::TYPE_INT:				return VK_FORMAT_R32_SINT;
		case glu::TYPE_INT_VEC2:		return VK_FORMAT_R32G32_SINT;
		case glu::TYPE_INT_VEC3:		return VK_FORMAT_R32G32B32_SINT;
		case glu::TYPE_INT_VEC4:		return VK_FORMAT_R32G32B32A32_SINT;

		case glu::TYPE_UINT:			return VK_FORMAT_R32_UINT;
		case glu::TYPE_UINT_VEC2:		return VK_FORMAT_R32G32_UINT;
		case glu::TYPE_UINT_VEC3:		return VK_FORMAT_R32G32B32_UINT;
		case glu::TYPE_UINT_VEC4:		return VK_FORMAT_R32G32B32A32_UINT;

		case glu::TYPE_FLOAT_MAT2:		return VK_FORMAT_R32G32_SFLOAT;
		case glu::TYPE_FLOAT_MAT2X3:	return VK_FORMAT_R32G32B32_SFLOAT;
		case glu::TYPE_FLOAT_MAT2X4:	return VK_FORMAT_R32G32B32A32_SFLOAT;
		case glu::TYPE_FLOAT_MAT3X2:	return VK_FORMAT_R32G32_SFLOAT;
		case glu::TYPE_FLOAT_MAT3:		return VK_FORMAT_R32G32B32_SFLOAT;
		case glu::TYPE_FLOAT_MAT3X4:	return VK_FORMAT_R32G32B32A32_SFLOAT;
		case glu::TYPE_FLOAT_MAT4X2:	return VK_FORMAT_R32G32_SFLOAT;
		case glu::TYPE_FLOAT_MAT4X3:	return VK_FORMAT_R32G32B32_SFLOAT;
		case glu::TYPE_FLOAT_MAT4:		return VK_FORMAT_R32G32B32A32_SFLOAT;
		default:
			DE_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
	}
}

void FragmentOutExecutor::addAttribute (deUint32 bindingLocation, VkFormat format, deUint32 sizePerElement, deUint32 count, const void* dataPtr)
{
	// Add binding specification
	const deUint32							binding = (deUint32)m_vertexBindingDescriptions.size();
	const VkVertexInputBindingDescription	bindingDescription =
	{
		binding,
		sizePerElement,
		VK_VERTEX_INPUT_RATE_VERTEX
	};

	m_vertexBindingDescriptions.push_back(bindingDescription);

	// Add location and format specification
	const VkVertexInputAttributeDescription attributeDescription =
	{
		bindingLocation,			// deUint32	location;
		binding,					// deUint32	binding;
		format,						// VkFormat	format;
		0u,							// deUint32	offsetInBytes;
	};

	m_vertexAttributeDescriptions.push_back(attributeDescription);

	// Upload data to buffer
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkDeviceSize			inputSize			= sizePerElement * count;
	const VkBufferCreateInfo	vertexBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		inputSize,									// VkDeviceSize			size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer>			buffer	= createBuffer(vk, vkDevice, &vertexBufferParams);
	de::MovePtr<Allocation>	alloc	= m_context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), alloc->getOffset()));

	deMemcpy(alloc->getHostPtr(), dataPtr, (size_t)inputSize);
	flushAlloc(vk, vkDevice, *alloc);

	m_vertexBuffers.push_back(de::SharedPtr<Unique<VkBuffer> >(new Unique<VkBuffer>(buffer)));
	m_vertexBufferAllocs.push_back(AllocationSp(alloc.release()));
}

void FragmentOutExecutor::bindAttributes (int numValues, const void* const* inputs)
{
	// Input attributes
	for (int inputNdx = 0; inputNdx < (int)m_shaderSpec.inputs.size(); inputNdx++)
	{
		const Symbol&		symbol			= m_shaderSpec.inputs[inputNdx];
		const void*			ptr				= inputs[inputNdx];
		const glu::DataType	basicType		= symbol.varType.getBasicType();
		const int			vecSize			= glu::getDataTypeScalarSize(basicType);
		const VkFormat		format			= getAttributeFormat(basicType);
		int					elementSize		= 0;
		int					numAttrsToAdd	= 1;

		if (glu::isDataTypeDoubleOrDVec(basicType))
			elementSize = sizeof(double);
		if (glu::isDataTypeFloatOrVec(basicType))
			elementSize = sizeof(float);
		else if (glu::isDataTypeFloat16OrVec(basicType))
			elementSize = sizeof(deUint16);
		else if (glu::isDataTypeIntOrIVec(basicType))
			elementSize = sizeof(int);
		else if (glu::isDataTypeUintOrUVec(basicType))
			elementSize = sizeof(deUint32);
		else if (glu::isDataTypeMatrix(basicType))
		{
			int		numRows	= glu::getDataTypeMatrixNumRows(basicType);
			int		numCols	= glu::getDataTypeMatrixNumColumns(basicType);

			elementSize = numRows * numCols * (int)sizeof(float);
			numAttrsToAdd = numCols;
		}
		else
			DE_ASSERT(false);

		// add attributes, in case of matrix every column is binded as an attribute
		for (int attrNdx = 0; attrNdx < numAttrsToAdd; attrNdx++)
		{
			addAttribute((deUint32)m_vertexBindingDescriptions.size(), format, elementSize * vecSize, numValues, ptr);
		}
	}
}

void FragmentOutExecutor::clearRenderData (void)
{
	m_vertexBindingDescriptions.clear();
	m_vertexAttributeDescriptions.clear();
	m_vertexBuffers.clear();
	m_vertexBufferAllocs.clear();
}

static Move<VkDescriptorSetLayout> createEmptyDescriptorSetLayout (const DeviceInterface& vkd, VkDevice device)
{
	const VkDescriptorSetLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0,
		0u,
		DE_NULL,
	};
	return createDescriptorSetLayout(vkd, device, &createInfo);
}

static Move<VkDescriptorPool> createDummyDescriptorPool (const DeviceInterface& vkd, VkDevice device)
{
	const VkDescriptorPoolSize			dummySize	=
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1u,
	};
	const VkDescriptorPoolCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,
		1u,
		&dummySize
	};
	return createDescriptorPool(vkd, device, &createInfo);
}

static Move<VkDescriptorSet> allocateSingleDescriptorSet (const DeviceInterface& vkd, VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
	const VkDescriptorSetAllocateInfo	allocInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		pool,
		1u,
		&layout,
	};
	return allocateDescriptorSet(vkd, device, &allocInfo);
}

void FragmentOutExecutor::execute (int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources)
{
	const VkDevice										vkDevice				= m_context.getDevice();
	const DeviceInterface&								vk						= m_context.getDeviceInterface();
	const VkQueue										queue					= m_context.getUniversalQueue();
	const deUint32										queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&											memAlloc				= m_context.getDefaultAllocator();

	const deUint32										renderSizeX				= de::min(static_cast<deUint32>(128), (deUint32)numValues);
	const deUint32										renderSizeY				= ((deUint32)numValues / renderSizeX) + (((deUint32)numValues % renderSizeX != 0) ? 1u : 0u);
	const tcu::UVec2									renderSize				(renderSizeX, renderSizeY);
	std::vector<tcu::Vec2>								positions;

	const bool											useGeometryShader		= m_shaderType == glu::SHADERTYPE_GEOMETRY;

	std::vector<VkImageSp>								colorImages;
	std::vector<VkImageMemoryBarrier>					colorImagePreRenderBarriers;
	std::vector<VkImageMemoryBarrier>					colorImagePostRenderBarriers;
	std::vector<AllocationSp>							colorImageAllocs;
	std::vector<VkAttachmentDescription>				attachments;
	std::vector<VkClearValue>							attachmentClearValues;
	std::vector<VkImageViewSp>							colorImageViews;

	std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates;
	std::vector<VkAttachmentReference>					colorAttachmentReferences;

	Move<VkRenderPass>									renderPass;
	Move<VkFramebuffer>									framebuffer;
	Move<VkPipelineLayout>								pipelineLayout;
	Move<VkPipeline>									graphicsPipeline;

	Move<VkShaderModule>								vertexShaderModule;
	Move<VkShaderModule>								geometryShaderModule;
	Move<VkShaderModule>								fragmentShaderModule;

	Move<VkCommandPool>									cmdPool;
	Move<VkCommandBuffer>								cmdBuffer;

	Unique<VkDescriptorSetLayout>						emptyDescriptorSetLayout	(createEmptyDescriptorSetLayout(vk, vkDevice));
	Unique<VkDescriptorPool>							dummyDescriptorPool			(createDummyDescriptorPool(vk, vkDevice));
	Unique<VkDescriptorSet>								emptyDescriptorSet			(allocateSingleDescriptorSet(vk, vkDevice, *dummyDescriptorPool, *emptyDescriptorSetLayout));

	clearRenderData();

	// Compute positions - 1px points are used to drive fragment shading.
	positions = computeVertexPositions(numValues, renderSize.cast<int>());

	// Bind attributes
	addAttribute(0u, VK_FORMAT_R32G32_SFLOAT, sizeof(tcu::Vec2), (deUint32)positions.size(), &positions[0]);
	bindAttributes(numValues, inputs);

	// Create color images
	{
		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			VK_FALSE,																	// VkBool32						blendEnable;
			VK_BLEND_FACTOR_ONE,														// VkBlendFactor				srcColorBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor				dstColorBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp					blendOpColor;
			VK_BLEND_FACTOR_ONE,														// VkBlendFactor				srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ZERO,														// VkBlendFactor				destAlphaBlendFactor;
			VK_BLEND_OP_ADD,															// VkBlendOp					blendOpAlpha;
			(VK_COLOR_COMPONENT_R_BIT |
			 VK_COLOR_COMPONENT_G_BIT |
			 VK_COLOR_COMPONENT_B_BIT |
			 VK_COLOR_COMPONENT_A_BIT)													// VkColorComponentFlags		colorWriteMask;
		};

		for (int outNdx = 0; outNdx < (int)m_outputLayout.locationSymbols.size(); ++outNdx)
		{
			const bool		isDouble	= glu::isDataTypeDoubleOrDVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isFloat		= isDataTypeFloatOrVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isFloat16b	= glu::isDataTypeFloat16OrVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isSigned	= isDataTypeIntOrIVec (m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isBool		= isDataTypeBoolOrBVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const VkFormat	colorFormat = (isDouble ? VK_FORMAT_R64G64B64A64_SFLOAT : (isFloat16b ? VK_FORMAT_R16G16B16A16_SFLOAT : (isFloat ? VK_FORMAT_R32G32B32A32_SFLOAT : (isSigned || isBool ? VK_FORMAT_R32G32B32A32_SINT : VK_FORMAT_R32G32B32A32_UINT))));

			{
				const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), colorFormat);
				if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
					TCU_THROW(NotSupportedError, "Image format doesn't support COLOR_ATTACHMENT_BIT");
			}

			const VkImageCreateInfo	 colorImageParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType				sType;
				DE_NULL,																	// const void*					pNext;
				0u,																			// VkImageCreateFlags			flags;
				VK_IMAGE_TYPE_2D,															// VkImageType					imageType;
				colorFormat,																// VkFormat						format;
				{ renderSize.x(), renderSize.y(), 1u },										// VkExtent3D					extent;
				1u,																			// deUint32						mipLevels;
				1u,																			// deUint32						arraySize;
				VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits		samples;
				VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling				tiling;
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags			usage;
				VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode				sharingMode;
				1u,																			// deUint32						queueFamilyCount;
				&queueFamilyIndex,															// const deUint32*				pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout				initialLayout;
			};

			const VkAttachmentDescription colorAttachmentDescription =
			{
				0u,																			// VkAttachmentDescriptorFlags	flags;
				colorFormat,																// VkFormat						format;
				VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits		samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,												// VkAttachmentLoadOp			loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,												// VkAttachmentStoreOp			storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,											// VkAttachmentLoadOp			stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,											// VkAttachmentStoreOp			stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout				initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout				finalLayout;
			};

			Move<VkImage> colorImage = createImage(vk, vkDevice, &colorImageParams);
			colorImages.push_back(de::SharedPtr<Unique<VkImage> >(new Unique<VkImage>(colorImage)));
			attachmentClearValues.push_back(getDefaultClearColor());

			// Allocate and bind color image memory
			{
				de::MovePtr<Allocation> colorImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *((const VkImage*) colorImages.back().get())), MemoryRequirement::Any);
				VK_CHECK(vk.bindImageMemory(vkDevice, colorImages.back().get()->get(), colorImageAlloc->getMemory(), colorImageAlloc->getOffset()));
				colorImageAllocs.push_back(de::SharedPtr<Allocation>(colorImageAlloc.release()));

				attachments.push_back(colorAttachmentDescription);
				colorBlendAttachmentStates.push_back(colorBlendAttachmentState);

				const VkAttachmentReference colorAttachmentReference =
				{
					(deUint32) (colorImages.size() - 1),			//	deUint32		attachment;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		//	VkImageLayout	layout;
				};

				colorAttachmentReferences.push_back(colorAttachmentReference);
			}

			// Create color attachment view
			{
				const VkImageViewCreateInfo colorImageViewParams =
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
					DE_NULL,											// const void*				pNext;
					0u,													// VkImageViewCreateFlags	flags;
					colorImages.back().get()->get(),					// VkImage					image;
					VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
					colorFormat,										// VkFormat					format;
					{
						VK_COMPONENT_SWIZZLE_R,							// VkComponentSwizzle		r;
						VK_COMPONENT_SWIZZLE_G,							// VkComponentSwizzle		g;
						VK_COMPONENT_SWIZZLE_B,							// VkComponentSwizzle		b;
						VK_COMPONENT_SWIZZLE_A							// VkComponentSwizzle		a;
					},													// VkComponentMapping		components;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags		aspectMask;
						0u,												// deUint32					baseMipLevel;
						1u,												// deUint32					mipLevels;
						0u,												// deUint32					baseArraySlice;
						1u												// deUint32					arraySize;
					}													// VkImageSubresourceRange	subresourceRange;
				};

				Move<VkImageView> colorImageView = createImageView(vk, vkDevice, &colorImageViewParams);
				colorImageViews.push_back(de::SharedPtr<Unique<VkImageView> >(new Unique<VkImageView>(colorImageView)));

				const VkImageMemoryBarrier	colorImagePreRenderBarrier =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// sType
					DE_NULL,												// pNext
					0u,														// srcAccessMask
					(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),					// dstAccessMask
					VK_IMAGE_LAYOUT_UNDEFINED,								// oldLayout
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// newLayout
					VK_QUEUE_FAMILY_IGNORED,								// srcQueueFamilyIndex
					VK_QUEUE_FAMILY_IGNORED,								// dstQueueFamilyIndex
					colorImages.back().get()->get(),						// image
					{
						VK_IMAGE_ASPECT_COLOR_BIT,								// aspectMask
						0u,														// baseMipLevel
						1u,														// levelCount
						0u,														// baseArrayLayer
						1u,														// layerCount
					}														// subresourceRange
				};
				colorImagePreRenderBarriers.push_back(colorImagePreRenderBarrier);

				const VkImageMemoryBarrier	colorImagePostRenderBarrier =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// sType
					DE_NULL,												// pNext
					(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),					// srcAccessMask
					VK_ACCESS_TRANSFER_READ_BIT,							// dstAccessMask
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// oldLayout
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,					// newLayout
					VK_QUEUE_FAMILY_IGNORED,								// srcQueueFamilyIndex
					VK_QUEUE_FAMILY_IGNORED,								// dstQueueFamilyIndex
					colorImages.back().get()->get(),						// image
					{
						VK_IMAGE_ASPECT_COLOR_BIT,								// aspectMask
						0u,														// baseMipLevel
						1u,														// levelCount
						0u,														// baseArrayLayer
						1u,														// layerCount
					}														// subresourceRange
				};
				colorImagePostRenderBarriers.push_back(colorImagePostRenderBarrier);
			}
		}
	}

	// Create render pass
	{
		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			(deUint32)colorImages.size(),						// deUint32						colorCount;
			&colorAttachmentReferences[0],						// const VkAttachmentReference*	colorAttachments;
			DE_NULL,											// const VkAttachmentReference*	resolveAttachments;
			DE_NULL,											// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
			(deUint32)attachments.size(),						// deUint32							attachmentCount;
			&attachments[0],									// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImageView> views(colorImageViews.size());
		for (size_t i = 0; i < colorImageViews.size(); i++)
		{
			views[i] = colorImageViews[i].get()->get();
		}

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*renderPass,										// VkRenderPass					renderPass;
			(deUint32)views.size(),								// deUint32						attachmentCount;
			&views[0],											// const VkImageView*			pAttachments;
			(deUint32)renderSize.x(),							// deUint32						width;
			(deUint32)renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkDescriptorSetLayout			setLayouts[]			=
		{
			*emptyDescriptorSetLayout,
			m_extraResourcesLayout
		};
		const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			(m_extraResourcesLayout != 0 ? 2u : 0u),			// deUint32						descriptorSetCount;
			setLayouts,											// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		vertexShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
		fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);

		if (useGeometryShader)
		{
			if (m_context.getDeviceFeatures().shaderTessellationAndGeometryPointSize)
				geometryShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("geom_point_size"), 0);
			else
				geometryShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("geom"), 0);
		}
	}

	// Create pipeline
	{
		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags		flags;
			(deUint32)m_vertexBindingDescriptions.size(),				// deUint32										bindingCount;
			&m_vertexBindingDescriptions[0],							// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
			(deUint32)m_vertexAttributeDescriptions.size(),				// deUint32										attributeCount;
			&m_vertexAttributeDescriptions[0],							// const VkVertexInputAttributeDescription*		pvertexAttributeDescriptions;
		};

		const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderSize));

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType;
			DE_NULL,														// const void*									pNext;
			(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags			flags;
			VK_FALSE,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,												// VkLogicOp									logicOp;
			(deUint32)colorBlendAttachmentStates.size(),					// deUint32										attachmentCount;
			&colorBlendAttachmentStates[0],									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }										// float										blendConst[4];
		};

		graphicsPipeline = makeGraphicsPipeline(vk,														// const DeviceInterface&                        vk
												vkDevice,												// const VkDevice                                device
												*pipelineLayout,										// const VkPipelineLayout                        pipelineLayout
												*vertexShaderModule,									// const VkShaderModule                          vertexShaderModule
												DE_NULL,												// const VkShaderModule                          tessellationControlShaderModule
												DE_NULL,												// const VkShaderModule                          tessellationEvalShaderModule
												useGeometryShader ? *geometryShaderModule : DE_NULL,	// const VkShaderModule                          geometryShaderModule
												*fragmentShaderModule,									// const VkShaderModule                          fragmentShaderModule
												*renderPass,											// const VkRenderPass                            renderPass
												viewports,												// const std::vector<VkViewport>&                viewports
												scissors,												// const std::vector<VkRect2D>&                  scissors
												VK_PRIMITIVE_TOPOLOGY_POINT_LIST,						// const VkPrimitiveTopology                     topology
												0u,														// const deUint32                                subpass
												0u,														// const deUint32                                patchControlPoints
												&vertexInputStateParams,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
												DE_NULL,												// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
												DE_NULL,												// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
												DE_NULL,												// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
												&colorBlendStateParams);								// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
	}

	// Create command pool
	cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  0, (const VkBufferMemoryBarrier*)DE_NULL,
							  (deUint32)colorImagePreRenderBarriers.size(), colorImagePreRenderBarriers.empty() ? DE_NULL : &colorImagePreRenderBarriers[0]);
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), (deUint32)attachmentClearValues.size(), &attachmentClearValues[0]);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		if (m_extraResourcesLayout != 0)
		{
			DE_ASSERT(extraResources != 0);
			const VkDescriptorSet	descriptorSets[]	= { *emptyDescriptorSet, extraResources };
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, DE_LENGTH_OF_ARRAY(descriptorSets), descriptorSets, 0u, DE_NULL);
		}
		else
			DE_ASSERT(extraResources == 0);

		const deUint32 numberOfVertexAttributes = (deUint32)m_vertexBuffers.size();

		std::vector<VkDeviceSize> offsets(numberOfVertexAttributes, 0);

		std::vector<VkBuffer> buffers(numberOfVertexAttributes);
		for (size_t i = 0; i < numberOfVertexAttributes; i++)
		{
			buffers[i] = m_vertexBuffers[i].get()->get();
		}

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, numberOfVertexAttributes, &buffers[0], &offsets[0]);
		vk.cmdDraw(*cmdBuffer, (deUint32)positions.size(), 1u, 0u, 0u);

		endRenderPass(vk, *cmdBuffer);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  0, (const VkBufferMemoryBarrier*)DE_NULL,
							  (deUint32)colorImagePostRenderBarriers.size(), colorImagePostRenderBarriers.empty() ? DE_NULL : &colorImagePostRenderBarriers[0]);

		endCommandBuffer(vk, *cmdBuffer);
	}

	// Execute Draw
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());

	// Read back result and output
	{
		const VkDeviceSize imageSizeBytes = (VkDeviceSize)(4 * sizeof(deUint32) * renderSize.x() * renderSize.y());
		const VkBufferCreateInfo readImageBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			imageSizeBytes,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		// constants for image copy
		Move<VkCommandPool>	copyCmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

		const VkBufferImageCopy copyParams =
		{
			0u,											// VkDeviceSize			bufferOffset;
			(deUint32)renderSize.x(),					// deUint32				bufferRowLength;
			(deUint32)renderSize.y(),					// deUint32				bufferImageHeight;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspect		aspect;
				0u,										// deUint32				mipLevel;
				0u,										// deUint32				arraySlice;
				1u,										// deUint32				arraySize;
			},											// VkImageSubresource	imageSubresource;
			{ 0u, 0u, 0u },								// VkOffset3D			imageOffset;
			{ renderSize.x(), renderSize.y(), 1u }		// VkExtent3D			imageExtent;
		};

		// Read back pixels.
		for (int outNdx = 0; outNdx < (int)m_shaderSpec.outputs.size(); ++outNdx)
		{
			const Symbol&				output			= m_shaderSpec.outputs[outNdx];
			const int					outSize			= output.varType.getScalarSize();
			const int					outVecSize		= glu::getDataTypeNumComponents(output.varType.getBasicType());
			const int					outNumLocs		= glu::getDataTypeNumLocations(output.varType.getBasicType());
			const int					outLocation		= de::lookup(m_outputLayout.locationMap, output.name);

			for (int locNdx = 0; locNdx < outNumLocs; ++locNdx)
			{
				tcu::TextureLevel			tmpBuf;
				const tcu::TextureFormat	format = getRenderbufferFormatForOutput(output.varType, false);
				const tcu::TextureFormat	readFormat (tcu::TextureFormat::RGBA, format.type);
				const Unique<VkBuffer>		readImageBuffer(createBuffer(vk, vkDevice, &readImageBufferParams));
				const de::UniquePtr<Allocation> readImageBufferMemory(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

				VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

				// Copy image to buffer
				{

					Move<VkCommandBuffer> copyCmdBuffer = allocateCommandBuffer(vk, vkDevice, *copyCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

					beginCommandBuffer(vk, *copyCmdBuffer);
					vk.cmdCopyImageToBuffer(*copyCmdBuffer, colorImages[outLocation + locNdx].get()->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &copyParams);

					// Insert a barrier so data written by the transfer is available to the host
					{
						const VkBufferMemoryBarrier barrier =
						{
							VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
							DE_NULL,									// const void*        pNext;
							VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
							VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
							VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
							VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
							*readImageBuffer,							// VkBuffer           buffer;
							0,											// VkDeviceSize       offset;
							VK_WHOLE_SIZE,								// VkDeviceSize       size;
						};

						vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
											0, (const VkMemoryBarrier*)DE_NULL,
											1, &barrier,
											0, (const VkImageMemoryBarrier*)DE_NULL);
					}

					endCommandBuffer(vk, *copyCmdBuffer);

					submitCommandsAndWait(vk, vkDevice, queue, copyCmdBuffer.get());
				}

				invalidateAlloc(vk, vkDevice, *readImageBufferMemory);

				tmpBuf.setStorage(readFormat, renderSize.x(), renderSize.y());

				const tcu::TextureFormat resultFormat(tcu::TextureFormat::RGBA, format.type);
				const tcu::ConstPixelBufferAccess resultAccess(resultFormat, renderSize.x(), renderSize.y(), 1, readImageBufferMemory->getHostPtr());

				tcu::copy(tmpBuf.getAccess(), resultAccess);

				if (isOutput16Bit(static_cast<size_t>(outNdx)))
				{
					deUint16*	dstPtrBase = static_cast<deUint16*>(outputs[outNdx]);
					if (outSize == 4 && outNumLocs == 1)
						deMemcpy(dstPtrBase, tmpBuf.getAccess().getDataPtr(), numValues * outVecSize * sizeof(deUint16));
					else
					{
						for (int valNdx = 0; valNdx < numValues; valNdx++)
						{
							const deUint16* srcPtr = (const deUint16*)tmpBuf.getAccess().getDataPtr() + valNdx * 4;
							deUint16*		dstPtr = &dstPtrBase[outSize * valNdx + outVecSize * locNdx];
							deMemcpy(dstPtr, srcPtr, outVecSize * sizeof(deUint16));
						}
					}
				}
				else
				{
					deUint32*	dstPtrBase = static_cast<deUint32*>(outputs[outNdx]);
					if (outSize == 4 && outNumLocs == 1)
						deMemcpy(dstPtrBase, tmpBuf.getAccess().getDataPtr(), numValues * outVecSize * sizeof(deUint32));
					else
					{
						for (int valNdx = 0; valNdx < numValues; valNdx++)
						{
							const deUint32* srcPtr = (const deUint32*)tmpBuf.getAccess().getDataPtr() + valNdx * 4;
							deUint32*		dstPtr = &dstPtrBase[outSize * valNdx + outVecSize * locNdx];
							deMemcpy(dstPtr, srcPtr, outVecSize * sizeof(deUint32));
						}
					}
				}
			}
		}
	}
}

// VertexShaderExecutor

class VertexShaderExecutor : public FragmentOutExecutor
{
public:
								VertexShaderExecutor	(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual						~VertexShaderExecutor	(void);

	static void					generateSources			(const ShaderSpec& shaderSpec, SourceCollections& dst);
};

VertexShaderExecutor::VertexShaderExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: FragmentOutExecutor(context, glu::SHADERTYPE_VERTEX, shaderSpec, extraResourcesLayout)
{
}

VertexShaderExecutor::~VertexShaderExecutor (void)
{
}

void VertexShaderExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	const FragmentOutputLayout	outputLayout	(computeFragmentOutputLayout(shaderSpec.outputs));

	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShader(shaderSpec, "a_", "vtx_out_")) << shaderSpec.buildOptions;
	/* \todo [2015-09-11 hegedusd] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generatePassthroughFragmentShader(shaderSpec, false, outputLayout.locationMap, "vtx_out_", "o_")) << shaderSpec.buildOptions;
}

// GeometryShaderExecutor

class GeometryShaderExecutor : public FragmentOutExecutor
{
public:
								GeometryShaderExecutor	(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual						~GeometryShaderExecutor	(void);

	static void					generateSources			(const ShaderSpec& shaderSpec, SourceCollections& programCollection);

};

GeometryShaderExecutor::GeometryShaderExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: FragmentOutExecutor(context, glu::SHADERTYPE_GEOMETRY, shaderSpec, extraResourcesLayout)
{
	const VkPhysicalDeviceFeatures& features = context.getDeviceFeatures();

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader type not supported by device");
}

GeometryShaderExecutor::~GeometryShaderExecutor (void)
{
}

void GeometryShaderExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	const FragmentOutputLayout	outputLayout	(computeFragmentOutputLayout(shaderSpec.outputs));

	programCollection.glslSources.add("vert") << glu::VertexSource(generatePassthroughVertexShader(shaderSpec, "a_", "vtx_out_")) << shaderSpec.buildOptions;

	programCollection.glslSources.add("geom") << glu::GeometrySource(generateGeometryShader(shaderSpec, "vtx_out_", "geom_out_", false)) << shaderSpec.buildOptions;
	programCollection.glslSources.add("geom_point_size") << glu::GeometrySource(generateGeometryShader(shaderSpec, "vtx_out_", "geom_out_", true)) << shaderSpec.buildOptions;

	/* \todo [2015-09-18 rsipka] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generatePassthroughFragmentShader(shaderSpec, false, outputLayout.locationMap, "geom_out_", "o_")) << shaderSpec.buildOptions;

}

// FragmentShaderExecutor

class FragmentShaderExecutor : public FragmentOutExecutor
{
public:
								FragmentShaderExecutor	(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual						~FragmentShaderExecutor (void);

	static void					generateSources			(const ShaderSpec& shaderSpec, SourceCollections& programCollection);

};

FragmentShaderExecutor::FragmentShaderExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: FragmentOutExecutor(context, glu::SHADERTYPE_FRAGMENT, shaderSpec, extraResourcesLayout)
{
}

FragmentShaderExecutor::~FragmentShaderExecutor (void)
{
}

void FragmentShaderExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	const FragmentOutputLayout	outputLayout	(computeFragmentOutputLayout(shaderSpec.outputs));

	programCollection.glslSources.add("vert") << glu::VertexSource(generatePassthroughVertexShader(shaderSpec, "a_", "vtx_out_")) << shaderSpec.buildOptions;
	/* \todo [2015-09-11 hegedusd] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateFragmentShader(shaderSpec, false, outputLayout.locationMap, "vtx_out_", "o_")) << shaderSpec.buildOptions;
}

// Shared utilities for compute and tess executors

static deUint32 getVecStd430ByteAlignment (glu::DataType type)
{
	deUint32 baseSize;

	switch (glu::getDataTypeScalarType(type))
	{
		case glu::TYPE_FLOAT16:	baseSize = 2u; break;
		case glu::TYPE_DOUBLE:	baseSize = 8u; break;
		default:				baseSize = 4u; break;
	}

	switch (glu::getDataTypeScalarSize(type))
	{
		case 1:		return baseSize;
		case 2:		return baseSize * 2u;;
		case 3:		// fallthrough.
		case 4:		return baseSize * 4u;
		default:
			DE_ASSERT(false);
			return 0u;
	}
}

class BufferIoExecutor : public ShaderExecutor
{
public:
							BufferIoExecutor	(Context& context, const ShaderSpec& shaderSpec);
	virtual					~BufferIoExecutor	(void);

protected:
	enum
	{
		INPUT_BUFFER_BINDING	= 0,
		OUTPUT_BUFFER_BINDING	= 1,
	};

	void					initBuffers			(int numValues);
	VkBuffer				getInputBuffer		(void) const		{ return *m_inputBuffer;					}
	VkBuffer				getOutputBuffer		(void) const		{ return *m_outputBuffer;					}
	deUint32				getInputStride		(void) const		{ return getLayoutStride(m_inputLayout);	}
	deUint32				getOutputStride		(void) const		{ return getLayoutStride(m_outputLayout);	}

	void					uploadInputBuffer	(const void* const* inputPtrs, int numValues, bool packFloat16Bit);
	void					readOutputBuffer	(void* const* outputPtrs, int numValues);

	static void				declareBufferBlocks	(std::ostream& src, const ShaderSpec& spec);
	static void				generateExecBufferIo(std::ostream& src, const ShaderSpec& spec, const char* invocationNdxName);

protected:
	Move<VkBuffer>			m_inputBuffer;
	Move<VkBuffer>			m_outputBuffer;

private:
	struct VarLayout
	{
		deUint32		offset;
		deUint32		stride;
		deUint32		matrixStride;

		VarLayout (void) : offset(0), stride(0), matrixStride(0) {}
	};

	static void				computeVarLayout	(const std::vector<Symbol>& symbols, std::vector<VarLayout>* layout);
	static deUint32			getLayoutStride		(const vector<VarLayout>& layout);

	static void				copyToBuffer		(const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr, bool packFloat16Bit);
	static void				copyFromBuffer		(const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr);

	de::MovePtr<Allocation>	m_inputAlloc;
	de::MovePtr<Allocation>	m_outputAlloc;

	vector<VarLayout>		m_inputLayout;
	vector<VarLayout>		m_outputLayout;
};

BufferIoExecutor::BufferIoExecutor (Context& context, const ShaderSpec& shaderSpec)
	: ShaderExecutor(context, shaderSpec)
{
	computeVarLayout(m_shaderSpec.inputs, &m_inputLayout);
	computeVarLayout(m_shaderSpec.outputs, &m_outputLayout);
}

BufferIoExecutor::~BufferIoExecutor (void)
{
}

inline deUint32 BufferIoExecutor::getLayoutStride (const vector<VarLayout>& layout)
{
	return layout.empty() ? 0 : layout[0].stride;
}

void BufferIoExecutor::computeVarLayout (const std::vector<Symbol>& symbols, std::vector<VarLayout>* layout)
{
	deUint32	maxAlignment	= 0;
	deUint32	curOffset		= 0;

	DE_ASSERT(layout != DE_NULL);
	DE_ASSERT(layout->empty());
	layout->resize(symbols.size());

	for (size_t varNdx = 0; varNdx < symbols.size(); varNdx++)
	{
		const Symbol&		symbol		= symbols[varNdx];
		const glu::DataType	basicType	= symbol.varType.getBasicType();
		VarLayout&			layoutEntry	= (*layout)[varNdx];

		if (glu::isDataTypeScalarOrVector(basicType))
		{
			const deUint32	alignment	= getVecStd430ByteAlignment(basicType);
			const deUint32	size		= (deUint32)glu::getDataTypeScalarSize(basicType) * (isDataTypeDoubleType(basicType) ? (int)(sizeof(deUint64)) : (isDataTypeFloat16OrVec(basicType) ? (int)sizeof(deUint16) : (int)sizeof(deUint32)));

			curOffset		= (deUint32)deAlign32((int)curOffset, (int)alignment);
			maxAlignment	= de::max(maxAlignment, alignment);

			layoutEntry.offset			= curOffset;
			layoutEntry.matrixStride	= 0;

			curOffset += size;
		}
		else if (glu::isDataTypeMatrix(basicType))
		{
			const int				numVecs			= glu::getDataTypeMatrixNumColumns(basicType);
			const glu::DataType		vecType			= glu::getDataTypeVector(glu::getDataTypeScalarType(basicType), glu::getDataTypeMatrixNumRows(basicType));
			const deUint32			vecAlignment	= getVecStd430ByteAlignment(vecType);

			curOffset		= (deUint32)deAlign32((int)curOffset, (int)vecAlignment);
			maxAlignment	= de::max(maxAlignment, vecAlignment);

			layoutEntry.offset			= curOffset;
			layoutEntry.matrixStride	= vecAlignment;

			curOffset += vecAlignment*numVecs;
		}
		else
			DE_ASSERT(false);
	}

	{
		const deUint32	totalSize	= (deUint32)deAlign32(curOffset, maxAlignment);

		for (vector<VarLayout>::iterator varIter = layout->begin(); varIter != layout->end(); ++varIter)
			varIter->stride = totalSize;
	}
}

void BufferIoExecutor::declareBufferBlocks (std::ostream& src, const ShaderSpec& spec)
{
	// Input struct
	if (!spec.inputs.empty())
	{
		glu::StructType inputStruct("Inputs");
		for (vector<Symbol>::const_iterator symIter = spec.inputs.begin(); symIter != spec.inputs.end(); ++symIter)
			inputStruct.addMember(symIter->name.c_str(), symIter->varType);
		src << glu::declare(&inputStruct) << ";\n";
	}

	// Output struct
	{
		glu::StructType outputStruct("Outputs");
		for (vector<Symbol>::const_iterator symIter = spec.outputs.begin(); symIter != spec.outputs.end(); ++symIter)
			outputStruct.addMember(symIter->name.c_str(), symIter->varType);
		src << glu::declare(&outputStruct) << ";\n";
	}

	src << "\n";

	if (!spec.inputs.empty())
	{
		src	<< "layout(set = 0, binding = " << int(INPUT_BUFFER_BINDING) << ", std430) buffer InBuffer\n"
			<< "{\n"
			<< "	Inputs inputs[];\n"
			<< "};\n";
	}

	src	<< "layout(set = 0, binding = " << int(OUTPUT_BUFFER_BINDING) << ", std430) buffer OutBuffer\n"
		<< "{\n"
		<< "	Outputs outputs[];\n"
		<< "};\n"
		<< "\n";
}

void BufferIoExecutor::generateExecBufferIo (std::ostream& src, const ShaderSpec& spec, const char* invocationNdxName)
{
	std::string	tname;
	for (vector<Symbol>::const_iterator symIter = spec.inputs.begin(); symIter != spec.inputs.end(); ++symIter)
	{
		const bool f16BitTest = spec.packFloat16Bit && glu::isDataTypeFloatType(symIter->varType.getBasicType());
		if (f16BitTest)
		{
			tname = glu::getDataTypeName(getDataTypeFloat16Scalars(symIter->varType.getBasicType()));
		}
		else
		{
			tname = glu::getDataTypeName(symIter->varType.getBasicType());
		}
		src << "\t" << tname << " "<< symIter->name << " = " << tname << "(inputs[" << invocationNdxName << "]." << symIter->name << ");\n";
	}

	for (vector<Symbol>::const_iterator symIter = spec.outputs.begin(); symIter != spec.outputs.end(); ++symIter)
	{
		const bool f16BitTest = spec.packFloat16Bit && glu::isDataTypeFloatType(symIter->varType.getBasicType());
		if (f16BitTest)
		{
			tname = glu::getDataTypeName(getDataTypeFloat16Scalars(symIter->varType.getBasicType()));
		}
		else
		{
			tname = glu::getDataTypeName(symIter->varType.getBasicType());
		}
		src << "\t" << tname << " " << symIter->name << ";\n";
		if (f16BitTest)
		{
			const char* ttname = glu::getDataTypeName(symIter->varType.getBasicType());
			src << "\t" << ttname << " " << "packed_" << symIter->name << ";\n";
		}
	}

	src << "\n";

	{
		std::istringstream	opSrc	(spec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	if (spec.packFloat16Bit)
		packFloat16Bit (src, spec.outputs);

	src << "\n";
	for (vector<Symbol>::const_iterator symIter = spec.outputs.begin(); symIter != spec.outputs.end(); ++symIter)
	{
		const bool f16BitTest = spec.packFloat16Bit && glu::isDataTypeFloatType(symIter->varType.getBasicType());
		if(f16BitTest)
			src << "\toutputs[" << invocationNdxName << "]." << symIter->name << " = packed_" << symIter->name << ";\n";
		else
			src << "\toutputs[" << invocationNdxName << "]." << symIter->name << " = " << symIter->name << ";\n";
	}
}

void BufferIoExecutor::copyToBuffer (const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr, bool packFloat16Bit)
{
	if (varType.isBasicType())
	{
		const glu::DataType		basicType		= varType.getBasicType();
		const bool				isMatrix		= glu::isDataTypeMatrix(basicType);
		const int				scalarSize		= glu::getDataTypeScalarSize(basicType);
		const int				numVecs			= isMatrix ? glu::getDataTypeMatrixNumColumns(basicType) : 1;
		const int				numComps		= scalarSize / numVecs;
		const int				size			= (glu::isDataTypeDoubleType(basicType) ? (int)sizeof(deUint64) : (glu::isDataTypeFloat16OrVec(basicType) ? (int)sizeof(deUint16) : (int)sizeof(deUint32)));

		for (int elemNdx = 0; elemNdx < numValues; elemNdx++)
		{
			for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
			{
				const int		srcOffset		= size * (elemNdx * scalarSize + vecNdx * numComps);
				const int		dstOffset		= layout.offset + layout.stride * elemNdx + (isMatrix ? layout.matrixStride * vecNdx : 0);
				const deUint8*	srcPtr			= (const deUint8*)srcBasePtr + srcOffset;
				deUint8*		dstPtr			= (deUint8*)dstBasePtr + dstOffset;

				if (packFloat16Bit)
				{
					// Convert the float values to 16 bit and store in the lower 16 bits of 32 bit ints.
					for (int cmpNdx=0; cmpNdx < numComps; ++cmpNdx)
					{
						deFloat16 f16vals[2] = {};
						f16vals[0] = deFloat32To16Round(((float*)srcPtr)[cmpNdx], DE_ROUNDINGMODE_TO_ZERO);
						deMemcpy(dstPtr + cmpNdx * size, &f16vals[0], size);
					}
				}
				else
				{
					deMemcpy(dstPtr, srcPtr, size * numComps);
				}
			}
		}
	}
	else
		throw tcu::InternalError("Unsupported type");
}

void BufferIoExecutor::copyFromBuffer (const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr)
{
	if (varType.isBasicType())
	{
		const glu::DataType		basicType		= varType.getBasicType();
		const bool				isMatrix		= glu::isDataTypeMatrix(basicType);
		const int				scalarSize		= glu::getDataTypeScalarSize(basicType);
		const int				numVecs			= isMatrix ? glu::getDataTypeMatrixNumColumns(basicType) : 1;
		const int				numComps		= scalarSize / numVecs;

		for (int elemNdx = 0; elemNdx < numValues; elemNdx++)
		{
			for (int vecNdx = 0; vecNdx < numVecs; vecNdx++)
			{
				const int		size			= (glu::isDataTypeDoubleType(basicType) ? (int)sizeof(deUint64) : (glu::isDataTypeFloat16OrVec(basicType) ? (int)sizeof(deUint16) : (int)sizeof(deUint32)));
				const int		srcOffset		= layout.offset + layout.stride * elemNdx + (isMatrix ? layout.matrixStride * vecNdx : 0);
				const int		dstOffset		= size * (elemNdx * scalarSize + vecNdx * numComps);
				const deUint8*	srcPtr			= (const deUint8*)srcBasePtr + srcOffset;
				deUint8*		dstPtr			= (deUint8*)dstBasePtr + dstOffset;

				deMemcpy(dstPtr, srcPtr, size * numComps);
			}
		}
	}
	else
		throw tcu::InternalError("Unsupported type");
}

void BufferIoExecutor::uploadInputBuffer (const void* const* inputPtrs, int numValues, bool packFloat16Bit)
{
	const VkDevice			vkDevice			= m_context.getDevice();
	const DeviceInterface&	vk					= m_context.getDeviceInterface();

	const deUint32			inputStride			= getLayoutStride(m_inputLayout);
	const int				inputBufferSize		= inputStride * numValues;

	if (inputBufferSize == 0)
		return; // No inputs

	DE_ASSERT(m_shaderSpec.inputs.size() == m_inputLayout.size());
	for (size_t inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
	{
		const glu::VarType&		varType		= m_shaderSpec.inputs[inputNdx].varType;
		const VarLayout&		layout		= m_inputLayout[inputNdx];

		copyToBuffer(varType, layout, numValues, inputPtrs[inputNdx], m_inputAlloc->getHostPtr(), packFloat16Bit);
	}

	flushAlloc(vk, vkDevice, *m_inputAlloc);
}

void BufferIoExecutor::readOutputBuffer (void* const* outputPtrs, int numValues)
{
	const VkDevice			vkDevice			= m_context.getDevice();
	const DeviceInterface&	vk					= m_context.getDeviceInterface();

	DE_ASSERT(numValues > 0); // At least some outputs are required.

	invalidateAlloc(vk, vkDevice, *m_outputAlloc);

	DE_ASSERT(m_shaderSpec.outputs.size() == m_outputLayout.size());
	for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
	{
		const glu::VarType&		varType		= m_shaderSpec.outputs[outputNdx].varType;
		const VarLayout&		layout		= m_outputLayout[outputNdx];

		copyFromBuffer(varType, layout, numValues, m_outputAlloc->getHostPtr(), outputPtrs[outputNdx]);
	}
}

void BufferIoExecutor::initBuffers (int numValues)
{
	const deUint32				inputStride			= getLayoutStride(m_inputLayout);
	const deUint32				outputStride		= getLayoutStride(m_outputLayout);
	// Avoid creating zero-sized buffer/memory
	const size_t				inputBufferSize		= de::max(numValues * inputStride, 1u);
	const size_t				outputBufferSize	= numValues * outputStride;

	// Upload data to buffer
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= m_context.getDefaultAllocator();

	const VkBufferCreateInfo inputBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		inputBufferSize,							// VkDeviceSize			size;
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	m_inputBuffer = createBuffer(vk, vkDevice, &inputBufferParams);
	m_inputAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_inputBuffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(vkDevice, *m_inputBuffer, m_inputAlloc->getMemory(), m_inputAlloc->getOffset()));

	const VkBufferCreateInfo outputBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		outputBufferSize,							// VkDeviceSize			size;
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	m_outputBuffer = createBuffer(vk, vkDevice, &outputBufferParams);
	m_outputAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_outputBuffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(vkDevice, *m_outputBuffer, m_outputAlloc->getMemory(), m_outputAlloc->getOffset()));
}

// ComputeShaderExecutor

class ComputeShaderExecutor : public BufferIoExecutor
{
public:
						ComputeShaderExecutor	(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual				~ComputeShaderExecutor	(void);

	static void			generateSources			(const ShaderSpec& shaderSpec, SourceCollections& programCollection);

	virtual void		execute					(int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources);

protected:
	static std::string	generateComputeShader	(const ShaderSpec& spec);

private:
	const VkDescriptorSetLayout					m_extraResourcesLayout;
};

ComputeShaderExecutor::ComputeShaderExecutor(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: BufferIoExecutor			(context, shaderSpec)
	, m_extraResourcesLayout	(extraResourcesLayout)
{
}

ComputeShaderExecutor::~ComputeShaderExecutor	(void)
{
}

std::string getTypeSpirv(const glu::DataType type, const bool packFloat16Bit = false)
{
	switch(type)
	{
	case glu::TYPE_FLOAT16:
		return "%f16";
	case glu::TYPE_FLOAT16_VEC2:
		return "%v2f16";
	case glu::TYPE_FLOAT16_VEC3:
		return "%v3f16";
	case glu::TYPE_FLOAT16_VEC4:
		return "%v4f16";
	case glu::TYPE_FLOAT:
		return packFloat16Bit ? "%u32" : "%f32";		// f16 values will be bitcast from ui32.
	case glu::TYPE_FLOAT_VEC2:
		return packFloat16Bit ? "%v2u32" : "%v2f32";	// f16 values will be bitcast from ui32.
	case glu::TYPE_FLOAT_VEC3:
		return packFloat16Bit ? "%v3u32" : "%v3f32";	// f16 values will be bitcast from ui32.
	case glu::TYPE_FLOAT_VEC4:
		return packFloat16Bit ? "%v4u32" : "%v4f32";	// f16 values will be bitcast from ui32.
	case glu::TYPE_INT:
		return "%i32";
	case glu::TYPE_INT_VEC2:
		return "%v2i32";
	case glu::TYPE_INT_VEC3:
		return "%v3i32";
	case glu::TYPE_INT_VEC4:
		return "%v4i32";
	case glu::TYPE_DOUBLE:
		return "%f64";
	case glu::TYPE_DOUBLE_VEC2:
		return "%v2f64";
	case glu::TYPE_DOUBLE_VEC3:
		return "%v3f64";
	case glu::TYPE_DOUBLE_VEC4:
		return "%v4f64";
	default:
		DE_ASSERT(0);
		return "";
		break;
	}
}

std::string moveBitOperation (std::string variableName, const int operationNdx)
{
	std::ostringstream	src;
	src << "\n"
	<< "%operation_move_" << operationNdx << " = OpLoad %i32 " << variableName << "\n"
	<< "%move1_" << operationNdx << " = OpShiftLeftLogical %i32 %operation_move_"<< operationNdx <<" %c_i32_1\n"
	<< "OpStore " << variableName << " %move1_" << operationNdx << "\n";
	return src.str();
}

std::string scalarComparison(const std::string operation, const int operationNdx, const glu::DataType type, const std::string& outputType, const int scalarSize)
{
	std::ostringstream	src;
	std::string			boolType;

	switch (type)
	{
	case glu::TYPE_FLOAT16:
	case glu::TYPE_FLOAT:
	case glu::TYPE_DOUBLE:
		src << "\n"
			<< "%operation_result_" << operationNdx << " = " << operation << " %bool %in0_val %in1_val\n"
			<< "OpSelectionMerge %IF_" << operationNdx << " None\n"
			<< "OpBranchConditional %operation_result_" << operationNdx << " %label_IF_" << operationNdx << " %IF_" << operationNdx << "\n"
			<< "%label_IF_" << operationNdx << " = OpLabel\n"
			<< "%operation_val_" << operationNdx << " = OpLoad %i32 %operation\n"
			<< "%out_val_" << operationNdx << " = OpLoad %i32 %out0\n"
			<< "%add_if_" << operationNdx << " = OpIAdd %i32 %out_val_" << operationNdx << " %operation_val_" << operationNdx << "\n"
			<< "OpStore %out0 %add_if_" << operationNdx << "\n"
			<< "OpBranch %IF_" << operationNdx << "\n"
			<< "%IF_" << operationNdx << " = OpLabel\n";
		return src.str();
	case glu::TYPE_FLOAT16_VEC2:
	case glu::TYPE_FLOAT_VEC2:
	case glu::TYPE_DOUBLE_VEC2:
		boolType = "%v2bool";
		break;
	case glu::TYPE_FLOAT16_VEC3:
	case glu::TYPE_FLOAT_VEC3:
	case glu::TYPE_DOUBLE_VEC3:
		boolType = "%v3bool";
		break;
	case glu::TYPE_FLOAT16_VEC4:
	case glu::TYPE_FLOAT_VEC4:
	case glu::TYPE_DOUBLE_VEC4:
		boolType = "%v4bool";
		break;
	default:
		DE_ASSERT(0);
		return "";
		break;
	}

	src << "\n"
		<< "%operation_result_" << operationNdx << " = " << operation << " " << boolType << " %in0_val %in1_val\n"
		<< "%ivec_result_" << operationNdx << " = OpSelect " << outputType << " %operation_result_" << operationNdx << " %c_" << &outputType[1] << "_1 %c_" << &outputType[1] << "_0\n"
		<< "%operation_val_" << operationNdx << " = OpLoad %i32 %operation\n";

	src << "%operation_vec_" << operationNdx << " = OpCompositeConstruct " << outputType;
	for(int ndx = 0; ndx < scalarSize; ++ndx)
		src << " %operation_val_" << operationNdx;
	src << "\n";

	src << "%toAdd" << operationNdx << " = OpIMul "<< outputType << " %ivec_result_" << operationNdx << " %operation_vec_" << operationNdx <<"\n"
		<< "%out_val_" << operationNdx << " = OpLoad "<< outputType << " %out0\n"

		<< "%add_if_" << operationNdx << " = OpIAdd " << outputType << " %out_val_" << operationNdx << " %toAdd" << operationNdx << "\n"
		<< "OpStore %out0 %add_if_" << operationNdx << "\n";

	return src.str();
}

std::string generateSpirv(const ShaderSpec& spec, const bool are16Bit, const bool are64Bit, const bool isMediump)
{
	static const std::string COMPARE_OPERATIONS[] =
	{
		"OpFOrdEqual",
		"OpFOrdGreaterThan",
		"OpFOrdLessThan",
		"OpFOrdGreaterThanEqual",
		"OpFOrdLessThanEqual",
		"OpFUnordEqual",
		"OpFUnordGreaterThan",
		"OpFUnordLessThan",
		"OpFUnordGreaterThanEqual",
		"OpFUnordLessThanEqual"
	};

	int					moveBitNdx		= 0;
	vector<std::string>	inputTypes;
	vector<std::string>	outputTypes;
	const std::string	packType		= spec.packFloat16Bit ? getTypeSpirv(getDataTypeFloat16Scalars(spec.inputs[0].varType.getBasicType())) : "";

	vector<bool>		floatResult;
	for (const auto& symbol : spec.outputs)
		floatResult.push_back(glu::isDataTypeFloatType(symbol.varType.getBasicType()));

	const bool			anyFloatResult	= std::any_of(begin(floatResult), end(floatResult), [](bool b) { return b; });

	vector<bool>		packFloatRes;
	for (const auto& floatRes : floatResult)
		packFloatRes.push_back(floatRes && spec.packFloat16Bit);

	const bool			useF32Types		= (!are16Bit && !are64Bit);
	const bool			useF64Types		= are64Bit;
	const bool			useF16Types		= (spec.packFloat16Bit || are16Bit);

	for (const auto& symbol : spec.inputs)
		inputTypes.push_back(getTypeSpirv(symbol.varType.getBasicType(), spec.packFloat16Bit));

	for (const auto& symbol : spec.outputs)
		outputTypes.push_back(getTypeSpirv(symbol.varType.getBasicType(), spec.packFloat16Bit));

	DE_ASSERT(!inputTypes.empty());
	DE_ASSERT(!outputTypes.empty());

	// Assert input and output types match the expected operations.
	switch (spec.spirvCase)
	{
	case SPIRV_CASETYPE_COMPARE:
	case SPIRV_CASETYPE_FREM:
		DE_ASSERT(inputTypes.size() == 2);
		DE_ASSERT(outputTypes.size() == 1);
		break;
	case SPIRV_CASETYPE_MODFSTRUCT:
	case SPIRV_CASETYPE_FREXPSTRUCT:
		DE_ASSERT(inputTypes.size() == 1);
		DE_ASSERT(outputTypes.size() == 2);
		break;
	default:
		DE_ASSERT(false);
		break;
	};

	std::ostringstream	src;
	src << "; SPIR-V\n"
		"; Version: 1.0\n"
		"; Generator: Khronos Glslang Reference Front End; 4\n"
		"; Bound: 114\n"
		"; Schema: 0\n"
		"OpCapability Shader\n";

	if (useF16Types)
		src << "OpCapability Float16\n";

	if (are16Bit)
		src << "OpCapability StorageBuffer16BitAccess\n"
			"OpCapability UniformAndStorageBuffer16BitAccess\n";

	if (useF64Types)
		src << "OpCapability Float64\n";

	if (are16Bit)
		src << "OpExtension \"SPV_KHR_16bit_storage\"\n";

	src << "%glslstd450 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %BP_main \"main\" %BP_id3uNum %BP_id3uID\n"
		"OpExecutionMode %BP_main LocalSize 1 1 1\n"
		"OpDecorate %BP_id3uNum BuiltIn NumWorkgroups\n"
		"OpDecorate %BP_id3uID BuiltIn WorkgroupId\n";

	// Input offsets and stride.
	{
		int offset	= 0;
		int ndx		= 0;
		int largest	= 0;
		for (const auto& symbol : spec.inputs)
		{
			const int scalarSize		= symbol.varType.getScalarSize();
			const int memberSize		= (scalarSize + ((scalarSize == 3) ? 1 : 0)) * (isDataTypeDoubleType(symbol.varType.getBasicType()) ? (int)sizeof(deUint64) : (isDataTypeFloat16OrVec(symbol.varType.getBasicType()) ? (int)sizeof(deUint16) : (int)sizeof(deUint32)));
			const int extraMemberBytes	= (offset % memberSize);

			offset += ((extraMemberBytes == 0) ? 0 : (memberSize - extraMemberBytes));
			src << "OpMemberDecorate %SSB0_IN "<< ndx <<" Offset " << offset << "\n";
			++ndx;

			if (memberSize > largest)
				largest = memberSize;

			offset += memberSize;
		}
		DE_ASSERT(largest > 0);
		const int extraBytes	= (offset % largest);
		const int stride		= offset + (extraBytes == 0 ? 0 : (largest - extraBytes));
		src << "OpDecorate %up_SSB0_IN ArrayStride "<< stride << "\n";
	}

	src << "OpMemberDecorate %ssboIN 0 Offset 0\n"
		"OpDecorate %ssboIN BufferBlock\n"
		"OpDecorate %ssbo_src DescriptorSet 0\n"
		"OpDecorate %ssbo_src Binding 0\n"
		"\n";

	if (isMediump)
	{
		for (size_t i = 0; i < inputTypes.size(); ++i)
		{
			src <<
				"OpMemberDecorate %SSB0_IN " << i << " RelaxedPrecision\n"
				"OpDecorate %in" << i << " RelaxedPrecision\n"
				"OpDecorate %src_val_0_" << i << " RelaxedPrecision\n"
				"OpDecorate %in" << i << "_val RelaxedPrecision\n"
				;
		}

			if (anyFloatResult)
			{
				switch (spec.spirvCase)
				{
				case SPIRV_CASETYPE_FREM:
					src << "OpDecorate %frem_result RelaxedPrecision\n";
					break;
				case SPIRV_CASETYPE_MODFSTRUCT:
					src << "OpDecorate %modfstruct_result RelaxedPrecision\n";
					break;
				case SPIRV_CASETYPE_FREXPSTRUCT:
					src << "OpDecorate %frexpstruct_result RelaxedPrecision\n";
					break;
				default:
					DE_ASSERT(false);
					break;
				}

				for (size_t i = 0; i < outputTypes.size(); ++i)
				{
					src << "OpMemberDecorate %SSB0_OUT " << i << " RelaxedPrecision\n";
					src << "OpDecorate %out_val_final_" << i << " RelaxedPrecision\n";
					src << "OpDecorate %out" << i << " RelaxedPrecision\n";
				}
			}
	}

	// Output offsets and stride.
	{
		int offset	= 0;
		int ndx		= 0;
		int largest	= 0;
		for (const auto& symbol : spec.outputs)
		{
			const int scalarSize		= symbol.varType.getScalarSize();
			const int memberSize		= (scalarSize + ((scalarSize == 3) ? 1 : 0)) * (isDataTypeDoubleType(symbol.varType.getBasicType()) ? (int)sizeof(deUint64) : (isDataTypeFloat16OrVec(symbol.varType.getBasicType()) ? (int)sizeof(deUint16) : (int)sizeof(deUint32)));
			const int extraMemberBytes	= (offset % memberSize);

			offset += ((extraMemberBytes == 0) ? 0 : (memberSize - extraMemberBytes));
			src << "OpMemberDecorate %SSB0_OUT " << ndx << " Offset " << offset << "\n";
			++ndx;

			if (memberSize > largest)
				largest = memberSize;

			offset += memberSize;
		}
		DE_ASSERT(largest > 0);
		const int extraBytes	= (offset % largest);
		const int stride		= offset + ((extraBytes == 0) ? 0 : (largest - extraBytes));
		src << "OpDecorate %up_SSB0_OUT ArrayStride " << stride << "\n";
	}

	src << "OpMemberDecorate %ssboOUT 0 Offset 0\n"
		"OpDecorate %ssboOUT BufferBlock\n"
		"OpDecorate %ssbo_dst DescriptorSet 0\n"
		"OpDecorate %ssbo_dst Binding 1\n"
		"\n"
		"%void  = OpTypeVoid\n"
		"%bool  = OpTypeBool\n"
		"%v2bool = OpTypeVector %bool 2\n"
		"%v3bool = OpTypeVector %bool 3\n"
		"%v4bool = OpTypeVector %bool 4\n"
		"%u32   = OpTypeInt 32 0\n";

	if (useF32Types)
		src << "%f32   = OpTypeFloat 32\n"
			"%v2f32 = OpTypeVector %f32 2\n"
			"%v3f32 = OpTypeVector %f32 3\n"
			"%v4f32 = OpTypeVector %f32 4\n";

	if (useF64Types)
		src << "%f64   = OpTypeFloat 64\n"
			"%v2f64 = OpTypeVector %f64 2\n"
			"%v3f64 = OpTypeVector %f64 3\n"
			"%v4f64 = OpTypeVector %f64 4\n";

	if (useF16Types)
		src << "%f16   = OpTypeFloat 16\n"
			"%v2f16 = OpTypeVector %f16 2\n"
			"%v3f16 = OpTypeVector %f16 3\n"
			"%v4f16 = OpTypeVector %f16 4\n";

	src << "%i32   = OpTypeInt 32 1\n"
		"%v2i32 = OpTypeVector %i32 2\n"
		"%v3i32 = OpTypeVector %i32 3\n"
		"%v4i32 = OpTypeVector %i32 4\n"
		"%v2u32 = OpTypeVector %u32 2\n"
		"%v3u32 = OpTypeVector %u32 3\n"
		"%v4u32 = OpTypeVector %u32 4\n"
		"\n"
		"%ip_u32   = OpTypePointer Input %u32\n"
		"%ip_v3u32 = OpTypePointer Input %v3u32\n"
		"%up_float = OpTypePointer Uniform " << inputTypes[0] << "\n"
		"\n"
		"%fp_operation = OpTypePointer Function %i32\n"
		"%voidf        = OpTypeFunction %void\n"
		"%fp_u32       = OpTypePointer Function %u32\n"
		"%fp_it1       = OpTypePointer Function " << inputTypes[0] << "\n"
		;

	for (size_t i = 0; i < outputTypes.size(); ++i)
	{
		src << "%fp_out_" << i << "     = OpTypePointer Function " << outputTypes[i] << "\n"
			<< "%up_out_" << i << "     = OpTypePointer Uniform " << outputTypes[i] << "\n";
	}

	if (spec.packFloat16Bit)
		src << "%fp_f16  = OpTypePointer Function " << packType << "\n";

	src << "%BP_id3uID = OpVariable %ip_v3u32 Input\n"
		"%BP_id3uNum = OpVariable %ip_v3u32 Input\n"
		"\n"
		"%c_u32_0 = OpConstant %u32 0\n"
		"%c_u32_1 = OpConstant %u32 1\n"
		"%c_u32_2 = OpConstant %u32 2\n"
		"%c_i32_0 = OpConstant %i32 0\n"
		"%c_i32_1 = OpConstant %i32 1\n"
		"\n";

	if (useF32Types)
		src <<
			"%c_f32_0 = OpConstant %f32 0\n"
			"%c_f32_1 = OpConstant %f32 1\n"
			;

	if (useF16Types)
		src <<
			"%c_f16_0 = OpConstant %f16 0\n"
			"%c_f16_1 = OpConstant %f16 1\n"
			"%c_f16_minus1 = OpConstant %f16 -0x1p+0"
			;

	if (useF64Types)
		src <<
			"%c_f64_0 = OpConstant %f64 0\n"
			"%c_f64_1 = OpConstant %f64 1\n"
		;

	src << "\n"
		"%c_v2i32_0 = OpConstantComposite %v2i32 %c_i32_0 %c_i32_0\n"
		"%c_v2i32_1 = OpConstantComposite %v2i32 %c_i32_1 %c_i32_1\n"
		"%c_v3i32_0 = OpConstantComposite %v3i32 %c_i32_0 %c_i32_0 %c_i32_0\n"
		"%c_v3i32_1 = OpConstantComposite %v3i32 %c_i32_1 %c_i32_1 %c_i32_1\n"
		"%c_v4i32_0 = OpConstantComposite %v4i32 %c_i32_0 %c_i32_0 %c_i32_0 %c_i32_0\n"
		"%c_v4i32_1 = OpConstantComposite %v4i32 %c_i32_1 %c_i32_1 %c_i32_1 %c_i32_1\n"
		"\n";

	if (useF32Types)
		src <<
			"%c_v2f32_0 = OpConstantComposite %v2f32 %c_f32_0 %c_f32_0\n"
			"%c_v2f32_1 = OpConstantComposite %v2f32 %c_f32_1 %c_f32_1\n"
			"%c_v3f32_0 = OpConstantComposite %v3f32 %c_f32_0 %c_f32_0 %c_f32_0\n"
			"%c_v3f32_1 = OpConstantComposite %v3f32 %c_f32_1 %c_f32_1 %c_f32_1\n"
			"%c_v4f32_0 = OpConstantComposite %v4f32 %c_f32_0 %c_f32_0 %c_f32_0 %c_f32_0\n"
			"%c_v4f32_1 = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"
			;

	if (useF16Types)
		src <<
			"%c_v2f16_0 = OpConstantComposite %v2f16 %c_f16_0 %c_f16_0\n"
			"%c_v2f16_1 = OpConstantComposite %v2f16 %c_f16_1 %c_f16_1\n"
			"%c_v3f16_0 = OpConstantComposite %v3f16 %c_f16_0 %c_f16_0 %c_f16_0\n"
			"%c_v3f16_1 = OpConstantComposite %v3f16 %c_f16_1 %c_f16_1 %c_f16_1\n"
			"%c_v4f16_0 = OpConstantComposite %v4f16 %c_f16_0 %c_f16_0 %c_f16_0 %c_f16_0\n"
			"%c_v4f16_1 = OpConstantComposite %v4f16 %c_f16_1 %c_f16_1 %c_f16_1 %c_f16_1\n"
			;

	if (useF64Types)
		src <<
			"%c_v2f64_0 = OpConstantComposite %v2f64 %c_f64_0 %c_f64_0\n"
			"%c_v2f64_1 = OpConstantComposite %v2f64 %c_f64_1 %c_f64_1\n"
			"%c_v3f64_0 = OpConstantComposite %v3f64 %c_f64_0 %c_f64_0 %c_f64_0\n"
			"%c_v3f64_1 = OpConstantComposite %v3f64 %c_f64_1 %c_f64_1 %c_f64_1\n"
			"%c_v4f64_0 = OpConstantComposite %v4f64 %c_f64_0 %c_f64_0 %c_f64_0 %c_f64_0\n"
			"%c_v4f64_1 = OpConstantComposite %v4f64 %c_f64_1 %c_f64_1 %c_f64_1 %c_f64_1\n"
			"\n";

	// Input struct.
	{
		src << "%SSB0_IN    = OpTypeStruct";
		for (const auto& t : inputTypes)
			src << " " << t;
		src << "\n";
	}

	src <<
		"%up_SSB0_IN = OpTypeRuntimeArray %SSB0_IN\n"
		"%ssboIN     = OpTypeStruct %up_SSB0_IN\n"
		"%up_ssboIN  = OpTypePointer Uniform %ssboIN\n"
		"%ssbo_src   = OpVariable %up_ssboIN Uniform\n"
		"\n";

	// Output struct.
	{
		src << "%SSB0_OUT    = OpTypeStruct";
		for (const auto& t : outputTypes)
			src << " " << t;
		src << "\n";
	}

	std::string modfStructMemberType;
	std::string frexpStructFirstMemberType;
	if (spec.spirvCase == SPIRV_CASETYPE_MODFSTRUCT)
	{
		modfStructMemberType = (packFloatRes[0] ? packType : outputTypes[0]);
		src << "%modfstruct_ret_t = OpTypeStruct " << modfStructMemberType << " " << modfStructMemberType << "\n";
	}
	else if (spec.spirvCase == SPIRV_CASETYPE_FREXPSTRUCT)
	{
		frexpStructFirstMemberType = (packFloatRes[0] ? packType : outputTypes[0]);
		src << "%frexpstruct_ret_t = OpTypeStruct " << frexpStructFirstMemberType << " " << outputTypes[1] << "\n";
	}

	src <<
		"%up_SSB0_OUT = OpTypeRuntimeArray %SSB0_OUT\n"
		"%ssboOUT     = OpTypeStruct %up_SSB0_OUT\n"
		"%up_ssboOUT  = OpTypePointer Uniform %ssboOUT\n"
		"%ssbo_dst    = OpVariable %up_ssboOUT Uniform\n"
		"\n"
		"%BP_main = OpFunction %void None %voidf\n"
		"%BP_label = OpLabel\n"
		"%invocationNdx = OpVariable %fp_u32 Function\n";

	// Note: here we are supposing all inputs have the same type.
	for (size_t i = 0; i < inputTypes.size(); ++i)
		src << "%in" << i << " = OpVariable " << (spec.packFloat16Bit ? "%fp_f16" : "%fp_it1") << " Function\n";

	for (size_t i = 0; i < outputTypes.size(); ++i)
		src << "%out" << i << " = OpVariable " << (packFloatRes[i] ? std::string("%fp_f16") : std::string("%fp_out_") + de::toString(i)) << " Function\n";

	src << "%operation = OpVariable %fp_operation Function\n"
		"%BP_id_0_ptr  = OpAccessChain %ip_u32 %BP_id3uID %c_u32_0\n"
		"%BP_id_1_ptr  = OpAccessChain %ip_u32 %BP_id3uID %c_u32_1\n"
		"%BP_id_2_ptr  = OpAccessChain %ip_u32 %BP_id3uID %c_u32_2\n"
		"%BP_num_0_ptr  = OpAccessChain %ip_u32 %BP_id3uNum %c_u32_0\n"
		"%BP_num_1_ptr  = OpAccessChain %ip_u32 %BP_id3uNum %c_u32_1\n"
		"%BP_id_0_val = OpLoad %u32 %BP_id_0_ptr\n"
		"%BP_id_1_val = OpLoad %u32 %BP_id_1_ptr\n"
		"%BP_id_2_val = OpLoad %u32 %BP_id_2_ptr\n"
		"%BP_num_0_val = OpLoad %u32 %BP_num_0_ptr\n"
		"%BP_num_1_val = OpLoad %u32 %BP_num_1_ptr\n"
		"\n"
		"%mul_1 = OpIMul %u32 %BP_num_0_val %BP_num_1_val\n"
		"%mul_2 = OpIMul %u32 %mul_1 %BP_id_2_val\n"
		"%mul_3 = OpIMul %u32 %BP_num_0_val %BP_id_1_val\n"
		"%add_1 = OpIAdd %u32 %mul_2 %mul_3\n"
		"%add_2 = OpIAdd %u32 %add_1 %BP_id_0_val\n"
		"OpStore %invocationNdx %add_2\n"
		"%invocationNdx_val = OpLoad %u32 %invocationNdx\n";

	// Load input values.
	for (size_t inputNdx = 0; inputNdx < inputTypes.size(); ++inputNdx)
	{
		src << "\n"
			<< "%src_ptr_0_" << inputNdx << " = OpAccessChain %up_float %ssbo_src %c_i32_0 %invocationNdx_val %c_i32_" << inputNdx << "\n"
			<< "%src_val_0_" << inputNdx << " = OpLoad " << inputTypes[inputNdx] << " %src_ptr_0_" << inputNdx << "\n";

		if (spec.packFloat16Bit)
		{
			if (spec.inputs[inputNdx].varType.getScalarSize() > 1)
			{
				// Extract the val<inputNdx> u32 input channels into individual f16 values.
				for (int i = 0; i < spec.inputs[inputNdx].varType.getScalarSize(); ++i)
				{
					src << "%src_val_0_" << inputNdx << "_" << i << " = OpCompositeExtract %u32 %src_val_0_" << inputNdx << " " << i << "\n"
						"%val_v2f16_0_" << inputNdx << "_" << i << " = OpBitcast %v2f16 %src_val_0_" << inputNdx << "_" << i << "\n"
						"%val_f16_0_" << inputNdx << "_" << i << " = OpCompositeExtract %f16 %val_v2f16_0_" << inputNdx << "_" << i << " 0\n";
				}

				// Construct the input vector.
				src << "%val_f16_0_" << inputNdx << "   = OpCompositeConstruct " << packType;
				for (int i = 0; i < spec.inputs[inputNdx].varType.getScalarSize(); ++i)
				{
					src << " %val_f16_0_" << inputNdx << "_" << i;
				}

				src << "\n";
				src << "OpStore %in" << inputNdx << " %val_f16_0_" << inputNdx << "\n";
			}
			else
			{
				src << "%val_v2f16_0_" << inputNdx << " = OpBitcast %v2f16 %src_val_0_" << inputNdx << "\n"
					"%val_f16_0_" << inputNdx << " = OpCompositeExtract %f16 %val_v2f16_0_" << inputNdx << " 0\n";

				src <<	"OpStore %in" << inputNdx << " %val_f16_0_" << inputNdx << "\n";
			}
		}
		else
			src << "OpStore %in" << inputNdx << " %src_val_0_" << inputNdx << "\n";

		src << "%in" << inputNdx << "_val = OpLoad " << (spec.packFloat16Bit ? packType : inputTypes[inputNdx]) << " %in" << inputNdx << "\n";
	}

	src << "\n"
		"OpStore %operation %c_i32_1\n";

	// Fill output values with dummy data.
	for (size_t i = 0; i < outputTypes.size(); ++i)
		src << "OpStore %out" << i << " %c_" << (packFloatRes[i] ? &packType[1] : &outputTypes[i][1]) << "_0\n";

	src << "\n";

	// Run operation.
	switch (spec.spirvCase)
	{
	case SPIRV_CASETYPE_COMPARE:
		for (int operationNdx = 0; operationNdx < DE_LENGTH_OF_ARRAY(COMPARE_OPERATIONS); ++operationNdx)
		{
			src << scalarComparison	(COMPARE_OPERATIONS[operationNdx], operationNdx,
									spec.inputs[0].varType.getBasicType(),
									outputTypes[0],
									spec.outputs[0].varType.getScalarSize());
			src << moveBitOperation("%operation", moveBitNdx);
			++moveBitNdx;
		}
		break;
	case SPIRV_CASETYPE_FREM:
		src << "%frem_result = OpFRem " << (packFloatRes[0] ? packType : outputTypes[0]) << " %in0_val %in1_val\n"
			<< "OpStore %out0 %frem_result\n";
		break;
	case SPIRV_CASETYPE_MODFSTRUCT:
		src << "%modfstruct_result = OpExtInst %modfstruct_ret_t %glslstd450 ModfStruct %in0_val\n"
			<< "%modfstruct_result_0 = OpCompositeExtract " << modfStructMemberType << " %modfstruct_result 0\n"
			<< "%modfstruct_result_1 = OpCompositeExtract " << modfStructMemberType << " %modfstruct_result 1\n"
			<< "OpStore %out0 %modfstruct_result_0\n"
			<< "OpStore %out1 %modfstruct_result_1\n";
		break;
	case SPIRV_CASETYPE_FREXPSTRUCT:
		src << "%frexpstruct_result = OpExtInst %frexpstruct_ret_t %glslstd450 FrexpStruct %in0_val\n"
			<< "%frexpstruct_result_0 = OpCompositeExtract " << frexpStructFirstMemberType << " %frexpstruct_result 0\n"
			<< "%frexpstruct_result_1 = OpCompositeExtract " << outputTypes[1] << " %frexpstruct_result 1\n"
			<< "OpStore %out0 %frexpstruct_result_0\n"
			<< "OpStore %out1 %frexpstruct_result_1\n";
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	for (size_t outputNdx = 0; outputNdx < outputTypes.size(); ++outputNdx)
	{
		src << "\n"
			"%out_val_final_" << outputNdx << " = OpLoad " << (packFloatRes[outputNdx] ? packType : outputTypes[outputNdx]) << " %out" << outputNdx << "\n"
			"%ssbo_dst_ptr_" << outputNdx << " = OpAccessChain %up_out_" << outputNdx << " %ssbo_dst %c_i32_0 %invocationNdx_val %c_i32_" << outputNdx << "\n";

		if (packFloatRes[outputNdx])
		{
			if (spec.outputs[outputNdx].varType.getScalarSize() > 1)
			{
				for (int i = 0; i < spec.outputs[outputNdx].varType.getScalarSize(); ++i)
				{
					src << "%out_val_final_" << outputNdx << "_" << i << " = OpCompositeExtract %f16 %out_val_final_" << outputNdx << " " << i << "\n";
					src << "%out_composite_" << outputNdx << "_" << i << " = OpCompositeConstruct %v2f16 %out_val_final_" << outputNdx << "_" << i << " %c_f16_minus1\n";
					src << "%u32_val_" << outputNdx << "_" << i << " = OpBitcast %u32 %out_composite_" << outputNdx << "_" << i << "\n";
				}

				src << "%u32_final_val_" << outputNdx << " = OpCompositeConstruct " << outputTypes[outputNdx];
				for (int i = 0; i < spec.outputs[outputNdx].varType.getScalarSize(); ++i)
					src << " %u32_val_" << outputNdx << "_" << i;
				src << "\n";
				src << "OpStore %ssbo_dst_ptr_" << outputNdx << " %u32_final_val_" << outputNdx << "\n";
			}
			else
			{
				src <<
					"%out_composite_" << outputNdx << " = OpCompositeConstruct %v2f16 %out_val_final_" << outputNdx << " %c_f16_minus1\n"
					"%out_result_" << outputNdx << " = OpBitcast " << outputTypes[outputNdx] << " %out_composite_" << outputNdx << "\n"
					"OpStore %ssbo_dst_ptr_" << outputNdx << " %out_result_" << outputNdx << "\n";
			}
		}
		else
		{
			src << "OpStore %ssbo_dst_ptr_" << outputNdx << " %out_val_final_" << outputNdx << "\n";
		}
	}

	src << "\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

	return src.str();
}


std::string ComputeShaderExecutor::generateComputeShader (const ShaderSpec& spec)
{
	if (spec.spirvCase != SPIRV_CASETYPE_NONE)
	{
		bool	are16Bit	= false;
		bool	are64Bit	= false;
		bool	isMediump	= false;
		for (vector<Symbol>::const_iterator symIter = spec.inputs.begin(); symIter != spec.inputs.end(); ++symIter)
		{
			if (glu::isDataTypeFloat16OrVec(symIter->varType.getBasicType()))
				are16Bit = true;

			if (glu::isDataTypeDoubleType(symIter->varType.getBasicType()))
				are64Bit = true;

			if (symIter->varType.getPrecision() == glu::PRECISION_MEDIUMP)
				isMediump = true;

			if (isMediump && are16Bit)
				break;
		}

		return generateSpirv(spec, are16Bit, are64Bit, isMediump);
	}
	else
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(spec.glslVersion) << "\n";

		if (!spec.globalDeclarations.empty())
			src << spec.globalDeclarations << "\n";

		src << "layout(local_size_x = " << spec.localSizeX << ") in;\n"
			<< "\n";

		declareBufferBlocks(src, spec);

		src << "void main (void)\n"
			<< "{\n"
			<< "	uint invocationNdx = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z\n"
			<< "	                   + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n";

		generateExecBufferIo(src, spec, "invocationNdx");

		src << "}\n";

		return src.str();
	}
}

void ComputeShaderExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	if (shaderSpec.spirvCase != SPIRV_CASETYPE_NONE)
		programCollection.spirvAsmSources.add("compute") << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3) << generateComputeShader(shaderSpec);
	else
		programCollection.glslSources.add("compute") << glu::ComputeSource(generateComputeShader(shaderSpec)) << shaderSpec.buildOptions;
}

void ComputeShaderExecutor::execute (int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources)
{
	const VkDevice					vkDevice				= m_context.getDevice();
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	DescriptorPoolBuilder			descriptorPoolBuilder;
	DescriptorSetLayoutBuilder		descriptorSetLayoutBuilder;

	Move<VkShaderModule>			computeShaderModule;
	Move<VkPipeline>				computePipeline;
	Move<VkPipelineLayout>			pipelineLayout;
	Move<VkCommandPool>				cmdPool;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorSet>			descriptorSet;
	const deUint32					numDescriptorSets		= (m_extraResourcesLayout != 0) ? 2u : 1u;

	DE_ASSERT((m_extraResourcesLayout != 0) == (extraResources != 0));

	initBuffers(numValues);

	// Setup input buffer & copy data
	// For spirv shaders using packed 16 bit float values as input, the floats are converted to 16 bit before
	// storing in the lower 16 bits of 32 bit integers in the uniform buffer and cast back to 16 bit floats in
	// the shader.
	uploadInputBuffer(inputs, numValues, m_shaderSpec.packFloat16Bit && (m_shaderSpec.spirvCase != SPIRV_CASETYPE_NONE));

	// Create command pool
	cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer

	descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	descriptorSetLayout = descriptorSetLayoutBuilder.build(vk, vkDevice);
	descriptorPool = descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const VkDescriptorSetAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*descriptorPool,
		1u,
		&*descriptorSetLayout
	};

	descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);

	// Create pipeline layout
	{
		const VkDescriptorSetLayout			descriptorSetLayouts[]	=
		{
			*descriptorSetLayout,
			m_extraResourcesLayout
		};
		const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			numDescriptorSets,									// deUint32						CdescriptorSetCount;
			descriptorSetLayouts,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		computeShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("compute"), 0);
	}

	// create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[1] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0u,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_COMPUTE_BIT,								// VkShaderStageFlagsBit				stage;
				*computeShaderModule,										// VkShaderModule						shader;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			}
		};

		const VkComputePipelineCreateInfo computePipelineParams =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			*shaderStageParams,									// VkPipelineShaderStageCreateInfo					cs;
			*pipelineLayout,									// VkPipelineLayout									layout;
			0u,													// VkPipeline										basePipelineHandle;
			0u,													// int32_t											basePipelineIndex;
		};

		computePipeline = createComputePipeline(vk, vkDevice, DE_NULL, &computePipelineParams);
	}

	const int			maxValuesPerInvocation	= m_context.getDeviceProperties().limits.maxComputeWorkGroupSize[0];
	int					curOffset				= 0;
	const deUint32		inputStride				= getInputStride();
	const deUint32		outputStride			= getOutputStride();

	while (curOffset < numValues)
	{
		Move<VkCommandBuffer>	cmdBuffer;
		const int				numToExec	= de::min(maxValuesPerInvocation, numValues-curOffset);

		// Update descriptors
		{
			DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;

			const VkDescriptorBufferInfo outputDescriptorBufferInfo =
			{
				*m_outputBuffer,				// VkBuffer			buffer;
				curOffset * outputStride,		// VkDeviceSize		offset;
				numToExec * outputStride		// VkDeviceSize		range;
			};

			descriptorSetUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding((deUint32)OUTPUT_BUFFER_BINDING), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputDescriptorBufferInfo);

			if (inputStride)
			{
				const VkDescriptorBufferInfo inputDescriptorBufferInfo =
				{
					*m_inputBuffer,					// VkBuffer			buffer;
					curOffset * inputStride,		// VkDeviceSize		offset;
					numToExec * inputStride			// VkDeviceSize		range;
				};

				descriptorSetUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding((deUint32)INPUT_BUFFER_BINDING), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputDescriptorBufferInfo);
			}

			descriptorSetUpdateBuilder.update(vk, vkDevice);
		}

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		beginCommandBuffer(vk, *cmdBuffer);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

		{
			const VkDescriptorSet	descriptorSets[]	= { *descriptorSet, extraResources };
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, numDescriptorSets, descriptorSets, 0u, DE_NULL);
		}

		vk.cmdDispatch(*cmdBuffer, numToExec, 1, 1);

		// Insert a barrier so data written by the shader is available to the host
		{
			const VkBufferMemoryBarrier bufferBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
				DE_NULL,									// const void*        pNext;
				VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags      srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
				*m_outputBuffer,							// VkBuffer           buffer;
				0,											// VkDeviceSize       offset;
				VK_WHOLE_SIZE,								// VkDeviceSize       size;
			};

			vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
								0, (const VkMemoryBarrier*)DE_NULL,
								1, &bufferBarrier,
								0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);

		curOffset += numToExec;

		// Execute
		submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());
	}

	// Read back data
	readOutputBuffer(outputs, numValues);
}

// Tessellation utils

static std::string generateVertexShaderForTess (void)
{
	std::ostringstream	src;
	src << "#version 450\n"
		<< "void main (void)\n{\n"
		<< "	gl_Position = vec4(gl_VertexIndex/2, gl_VertexIndex%2, 0.0, 1.0);\n"
		<< "}\n";

	return src.str();
}

class TessellationExecutor : public BufferIoExecutor
{
public:
					TessellationExecutor		(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual			~TessellationExecutor		(void);

	void			renderTess					(deUint32 numValues, deUint32 vertexCount, deUint32 patchControlPoints, VkDescriptorSet extraResources);

private:
	const VkDescriptorSetLayout					m_extraResourcesLayout;
};

TessellationExecutor::TessellationExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: BufferIoExecutor			(context, shaderSpec)
	, m_extraResourcesLayout	(extraResourcesLayout)
{
	const VkPhysicalDeviceFeatures& features = context.getDeviceFeatures();

	if (!features.tessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader is not supported by device");
}

TessellationExecutor::~TessellationExecutor (void)
{
}

void TessellationExecutor::renderTess (deUint32 numValues, deUint32 vertexCount, deUint32 patchControlPoints, VkDescriptorSet extraResources)
{
	const size_t						inputBufferSize				= numValues * getInputStride();
	const VkDevice						vkDevice					= m_context.getDevice();
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkQueue						queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	Allocator&							memAlloc					= m_context.getDefaultAllocator();

	const tcu::UVec2					renderSize					(DEFAULT_RENDER_WIDTH, DEFAULT_RENDER_HEIGHT);

	Move<VkImage>						colorImage;
	de::MovePtr<Allocation>				colorImageAlloc;
	VkFormat							colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
	Move<VkImageView>					colorImageView;

	Move<VkRenderPass>					renderPass;
	Move<VkFramebuffer>					framebuffer;
	Move<VkPipelineLayout>				pipelineLayout;
	Move<VkPipeline>					graphicsPipeline;

	Move<VkShaderModule>				vertexShaderModule;
	Move<VkShaderModule>				tessControlShaderModule;
	Move<VkShaderModule>				tessEvalShaderModule;
	Move<VkShaderModule>				fragmentShaderModule;

	Move<VkCommandPool>					cmdPool;
	Move<VkCommandBuffer>				cmdBuffer;

	Move<VkDescriptorPool>				descriptorPool;
	Move<VkDescriptorSetLayout>			descriptorSetLayout;
	Move<VkDescriptorSet>				descriptorSet;
	const deUint32						numDescriptorSets			= (m_extraResourcesLayout != 0) ? 2u : 1u;

	DE_ASSERT((m_extraResourcesLayout != 0) == (extraResources != 0));

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			colorFormat,																// VkFormat					format;
			{ renderSize.x(), renderSize.y(), 1u },										// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arraySize;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		colorImage = createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		colorImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *colorImage, colorImageAlloc->getMemory(), colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorImageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			colorFormat,										// VkFormat					format;
			{
				VK_COMPONENT_SWIZZLE_R,							// VkComponentSwizzle		r;
				VK_COMPONENT_SWIZZLE_G,							// VkComponentSwizzle		g;
				VK_COMPONENT_SWIZZLE_B,							// VkComponentSwizzle		b;
				VK_COMPONENT_SWIZZLE_A							// VkComponentSwizzle		a;
			},													// VkComponentsMapping		components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags		aspectMask;
				0u,												// deUint32					baseMipLevel;
				1u,												// deUint32					mipLevels;
				0u,												// deUint32					baseArraylayer;
				1u												// deUint32					layerCount;
			}													// VkImageSubresourceRange	subresourceRange;
		};

		colorImageView = createImageView(vk, vkDevice, &colorImageViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			0u,													// VkAttachmentDescriptorFlags	flags;
			colorFormat,										// VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits		samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout				finalLayout
		};

		const VkAttachmentDescription attachments[1] =
		{
			colorAttachmentDescription
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			1u,													// deUint32						colorCount;
			&colorAttachmentReference,							// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// const VkAttachmentReference* pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			1u,													// deUint32							attachmentCount;
			attachments,										// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			&*colorImageView,									// const VkAttachmentBindInfo*	pAttachments;
			(deUint32)renderSize.x(),							// deUint32						width;
			(deUint32)renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create descriptors
	{
		DescriptorPoolBuilder		descriptorPoolBuilder;
		DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;

		descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		descriptorSetLayout	= descriptorSetLayoutBuilder.build(vk, vkDevice);
		descriptorPool		= descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		const VkDescriptorSetAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*descriptorPool,
			1u,
			&*descriptorSetLayout
		};

		descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);
		// Update descriptors
		{
			DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
			const VkDescriptorBufferInfo outputDescriptorBufferInfo =
			{
				*m_outputBuffer,				// VkBuffer			buffer;
				0u,								// VkDeviceSize		offset;
				VK_WHOLE_SIZE					// VkDeviceSize		range;
			};

			descriptorSetUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding((deUint32)OUTPUT_BUFFER_BINDING), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputDescriptorBufferInfo);

			VkDescriptorBufferInfo inputDescriptorBufferInfo =
			{
				0,							// VkBuffer			buffer;
				0u,							// VkDeviceSize		offset;
				VK_WHOLE_SIZE				// VkDeviceSize		range;
			};

			if (inputBufferSize > 0)
			{
				inputDescriptorBufferInfo.buffer = *m_inputBuffer;

				descriptorSetUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding((deUint32)INPUT_BUFFER_BINDING), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputDescriptorBufferInfo);
			}

			descriptorSetUpdateBuilder.update(vk, vkDevice);
		}
	}

	// Create pipeline layout
	{
		const VkDescriptorSetLayout			descriptorSetLayouts[]		=
		{
			*descriptorSetLayout,
			m_extraResourcesLayout
		};
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			numDescriptorSets,									// deUint32						descriptorSetCount;
			descriptorSetLayouts,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shader modules
	{
		vertexShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
		tessControlShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tess_control"), 0);
		tessEvalShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tess_eval"), 0);
		fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
	}

	// Create pipeline
	{
		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags	flags;
			0u,																// deUint32									bindingCount;
			DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,																// deUint32									attributeCount;
			DE_NULL,														// const VkVertexInputAttributeDescription*	pvertexAttributeDescriptions;
		};

		const std::vector<VkViewport>	viewports	(1, makeViewport(renderSize));
		const std::vector<VkRect2D>		scissors	(1, makeRect2D(renderSize));

		graphicsPipeline = makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
												vkDevice,							// const VkDevice                                device
												*pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
												*vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
												*tessControlShaderModule,			// const VkShaderModule                          tessellationControlShaderModule
												*tessEvalShaderModule,				// const VkShaderModule                          tessellationEvalShaderModule
												DE_NULL,							// const VkShaderModule                          geometryShaderModule
												*fragmentShaderModule,				// const VkShaderModule                          fragmentShaderModule
												*renderPass,						// const VkRenderPass                            renderPass
												viewports,							// const std::vector<VkViewport>&                viewports
												scissors,							// const std::vector<VkRect2D>&                  scissors
												VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,	// const VkPrimitiveTopology                     topology
												0u,									// const deUint32                                subpass
												patchControlPoints,					// const deUint32                                patchControlPoints
												&vertexInputStateParams);			// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
	}

	// Create command pool
	cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue clearValue = getDefaultClearColor();

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *cmdBuffer);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()), clearValue);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		{
			const VkDescriptorSet	descriptorSets[]	= { *descriptorSet, extraResources };
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, numDescriptorSets, descriptorSets, 0u, DE_NULL);
		}

		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);

		endRenderPass(vk, *cmdBuffer);

		// Insert a barrier so data written by the shader is available to the host
		{
			const VkBufferMemoryBarrier bufferBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
				DE_NULL,									// const void*        pNext;
				VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags      srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
				*m_outputBuffer,							// VkBuffer           buffer;
				0,											// VkDeviceSize       offset;
				VK_WHOLE_SIZE,								// VkDeviceSize       size;
			};

			vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
								  0, (const VkMemoryBarrier*)DE_NULL,
								  1, &bufferBarrier,
								  0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);
	}

	// Execute Draw
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());
}

// TessControlExecutor

class TessControlExecutor : public TessellationExecutor
{
public:
						TessControlExecutor			(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual				~TessControlExecutor		(void);

	static void			generateSources				(const ShaderSpec& shaderSpec, SourceCollections& programCollection);

	virtual void		execute						(int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources);

protected:
	static std::string	generateTessControlShader	(const ShaderSpec& shaderSpec);
};

TessControlExecutor::TessControlExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: TessellationExecutor(context, shaderSpec, extraResourcesLayout)
{
}

TessControlExecutor::~TessControlExecutor (void)
{
}

std::string TessControlExecutor::generateTessControlShader (const ShaderSpec& shaderSpec)
{
	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";

	if (shaderSpec.glslVersion == glu::GLSL_VERSION_310_ES)
		src << "#extension GL_EXT_tessellation_shader : require\n\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "\nlayout(vertices = 1) out;\n\n";

	declareBufferBlocks(src, shaderSpec);

	src << "void main (void)\n{\n";

	for (int ndx = 0; ndx < 2; ndx++)
		src << "\tgl_TessLevelInner[" << ndx << "] = 1.0;\n";

	for (int ndx = 0; ndx < 4; ndx++)
		src << "\tgl_TessLevelOuter[" << ndx << "] = 1.0;\n";

	src << "\n"
		<< "\thighp uint invocationId = uint(gl_PrimitiveID);\n";

	generateExecBufferIo(src, shaderSpec, "invocationId");

	src << "}\n";

	return src.str();
}

static std::string generateEmptyTessEvalShader ()
{
	std::ostringstream src;

	src << "#version 450\n"
		   "#extension GL_EXT_tessellation_shader : require\n\n";

	src << "layout(triangles, ccw) in;\n";

	src << "\nvoid main (void)\n{\n"
		<< "\tgl_Position = vec4(gl_TessCoord.xy, 0.0, 1.0);\n"
		<< "}\n";

	return src.str();
}

void TessControlExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShaderForTess()) << shaderSpec.buildOptions;
	programCollection.glslSources.add("tess_control") << glu::TessellationControlSource(generateTessControlShader(shaderSpec)) << shaderSpec.buildOptions;
	programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(generateEmptyTessEvalShader()) << shaderSpec.buildOptions;
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateEmptyFragmentSource()) << shaderSpec.buildOptions;
}

void TessControlExecutor::execute (int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources)
{
	const deUint32	patchSize	= 3;

	initBuffers(numValues);

	// Setup input buffer & copy data
	uploadInputBuffer(inputs, numValues, false);

	renderTess(numValues, patchSize * numValues, patchSize, extraResources);

	// Read back data
	readOutputBuffer(outputs, numValues);
}

// TessEvaluationExecutor

class TessEvaluationExecutor : public TessellationExecutor
{
public:
						TessEvaluationExecutor	(Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout);
	virtual				~TessEvaluationExecutor	(void);

	static void			generateSources			(const ShaderSpec& shaderSpec, SourceCollections& programCollection);

	virtual void		execute					(int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources);

protected:
	static std::string	generateTessEvalShader	(const ShaderSpec& shaderSpec);
};

TessEvaluationExecutor::TessEvaluationExecutor (Context& context, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
	: TessellationExecutor (context, shaderSpec, extraResourcesLayout)
{
}

TessEvaluationExecutor::~TessEvaluationExecutor (void)
{
}

static std::string generatePassthroughTessControlShader (void)
{
	std::ostringstream src;

	src << "#version 450\n"
		   "#extension GL_EXT_tessellation_shader : require\n\n";

	src << "layout(vertices = 1) out;\n\n";

	src << "void main (void)\n{\n";

	for (int ndx = 0; ndx < 2; ndx++)
		src << "\tgl_TessLevelInner[" << ndx << "] = 1.0;\n";

	for (int ndx = 0; ndx < 4; ndx++)
		src << "\tgl_TessLevelOuter[" << ndx << "] = 1.0;\n";

	src << "}\n";

	return src.str();
}

std::string TessEvaluationExecutor::generateTessEvalShader (const ShaderSpec& shaderSpec)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(shaderSpec.glslVersion) << "\n";

	if (shaderSpec.glslVersion == glu::GLSL_VERSION_310_ES)
		src << "#extension GL_EXT_tessellation_shader : require\n\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "\n";

	src << "layout(isolines, equal_spacing) in;\n\n";

	declareBufferBlocks(src, shaderSpec);

	src << "void main (void)\n{\n"
		<< "\tgl_Position = vec4(gl_TessCoord.x, 0.0, 0.0, 1.0);\n"
		<< "\thighp uint invocationId = uint(gl_PrimitiveID)*2u + (gl_TessCoord.x > 0.5 ? 1u : 0u);\n";

	generateExecBufferIo(src, shaderSpec, "invocationId");

	src	<< "}\n";

	return src.str();
}

void TessEvaluationExecutor::generateSources (const ShaderSpec& shaderSpec, SourceCollections& programCollection)
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShaderForTess()) << shaderSpec.buildOptions;
	programCollection.glslSources.add("tess_control") << glu::TessellationControlSource(generatePassthroughTessControlShader()) << shaderSpec.buildOptions;
	programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(generateTessEvalShader(shaderSpec)) << shaderSpec.buildOptions;
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateEmptyFragmentSource()) << shaderSpec.buildOptions;
}

void TessEvaluationExecutor::execute (int numValues, const void* const* inputs, void* const* outputs, VkDescriptorSet extraResources)
{
	const int	patchSize		= 2;
	const int	alignedValues	= deAlign32(numValues, patchSize);

	// Initialize buffers with aligned value count to make room for padding
	initBuffers(alignedValues);

	// Setup input buffer & copy data
	uploadInputBuffer(inputs, numValues, false);

	renderTess((deUint32)alignedValues, (deUint32)alignedValues, (deUint32)patchSize, extraResources);

	// Read back data
	readOutputBuffer(outputs, numValues);
}

} // anonymous

// ShaderExecutor

ShaderExecutor::~ShaderExecutor (void)
{
}

bool ShaderExecutor::areInputs16Bit (void) const
{
	for (vector<Symbol>::const_iterator symIter = m_shaderSpec.inputs.begin(); symIter != m_shaderSpec.inputs.end(); ++symIter)
	{
		if (glu::isDataTypeFloat16OrVec(symIter->varType.getBasicType()))
			return true;
	}
	return false;
}

bool ShaderExecutor::areOutputs16Bit (void) const
{
	for (vector<Symbol>::const_iterator symIter = m_shaderSpec.outputs.begin(); symIter != m_shaderSpec.outputs.end(); ++symIter)
	{
		if (glu::isDataTypeFloat16OrVec(symIter->varType.getBasicType()))
			return true;
	}
	return false;
}

bool ShaderExecutor::isOutput16Bit (const size_t ndx) const
{
	if (glu::isDataTypeFloat16OrVec(m_shaderSpec.outputs[ndx].varType.getBasicType()))
		return true;
	return false;
}

bool ShaderExecutor::areInputs64Bit (void) const
{
	for (vector<Symbol>::const_iterator symIter = m_shaderSpec.inputs.begin(); symIter != m_shaderSpec.inputs.end(); ++symIter)
	{
		if (glu::isDataTypeDoubleType(symIter->varType.getBasicType()))
			return true;
	}
	return false;
}

bool ShaderExecutor::areOutputs64Bit (void) const
{
	for (vector<Symbol>::const_iterator symIter = m_shaderSpec.outputs.begin(); symIter != m_shaderSpec.outputs.end(); ++symIter)
	{
		if (glu::isDataTypeDoubleType(symIter->varType.getBasicType()))
			return true;
	}
	return false;
}

bool ShaderExecutor::isOutput64Bit (const size_t ndx) const
{
	if (glu::isDataTypeDoubleType(m_shaderSpec.outputs[ndx].varType.getBasicType()))
		return true;
	return false;
}

// Utilities

void generateSources (glu::ShaderType shaderType, const ShaderSpec& shaderSpec, vk::SourceCollections& dst)
{
	switch (shaderType)
	{
		case glu::SHADERTYPE_VERTEX:					VertexShaderExecutor::generateSources	(shaderSpec, dst);	break;
		case glu::SHADERTYPE_TESSELLATION_CONTROL:		TessControlExecutor::generateSources	(shaderSpec, dst);	break;
		case glu::SHADERTYPE_TESSELLATION_EVALUATION:	TessEvaluationExecutor::generateSources	(shaderSpec, dst);	break;
		case glu::SHADERTYPE_GEOMETRY:					GeometryShaderExecutor::generateSources	(shaderSpec, dst);	break;
		case glu::SHADERTYPE_FRAGMENT:					FragmentShaderExecutor::generateSources	(shaderSpec, dst);	break;
		case glu::SHADERTYPE_COMPUTE:					ComputeShaderExecutor::generateSources	(shaderSpec, dst);	break;
		default:
			TCU_THROW(InternalError, "Unsupported shader type");
	}
}

ShaderExecutor* createExecutor (Context& context, glu::ShaderType shaderType, const ShaderSpec& shaderSpec, VkDescriptorSetLayout extraResourcesLayout)
{
	switch (shaderType)
	{
		case glu::SHADERTYPE_VERTEX:					return new VertexShaderExecutor		(context, shaderSpec, extraResourcesLayout);
		case glu::SHADERTYPE_TESSELLATION_CONTROL:		return new TessControlExecutor		(context, shaderSpec, extraResourcesLayout);
		case glu::SHADERTYPE_TESSELLATION_EVALUATION:	return new TessEvaluationExecutor	(context, shaderSpec, extraResourcesLayout);
		case glu::SHADERTYPE_GEOMETRY:					return new GeometryShaderExecutor	(context, shaderSpec, extraResourcesLayout);
		case glu::SHADERTYPE_FRAGMENT:					return new FragmentShaderExecutor	(context, shaderSpec, extraResourcesLayout);
		case glu::SHADERTYPE_COMPUTE:					return new ComputeShaderExecutor	(context, shaderSpec, extraResourcesLayout);
		default:
			TCU_THROW(InternalError, "Unsupported shader type");
	}
}

bool  executorSupported(glu::ShaderType shaderType)
{
	switch (shaderType)
	{
	case glu::SHADERTYPE_VERTEX:
	case glu::SHADERTYPE_TESSELLATION_CONTROL:
	case glu::SHADERTYPE_TESSELLATION_EVALUATION:
	case glu::SHADERTYPE_GEOMETRY:
	case glu::SHADERTYPE_FRAGMENT:
	case glu::SHADERTYPE_COMPUTE:
		return true;
	default:
		return false;
	}
}

void checkSupportShader(Context& context, const glu::ShaderType shaderType)
{
	if (shaderType == glu::SHADERTYPE_TESSELLATION_EVALUATION &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().tessellationIsolines)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Tessellation iso lines are not supported by this implementation");
	}
}


} // shaderexecutor
} // vkt
