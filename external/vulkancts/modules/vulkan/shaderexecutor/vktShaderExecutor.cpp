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
#include <map>
#include <sstream>
#include <iostream>

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"

#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkStrUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"

#include "gluShaderUtil.hpp"

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

// Shader utilities

static VkClearValue	getDefaultClearColor (void)
{
	return makeClearValueColorF32(0.125f, 0.25f, 0.5f, 1.0f);
}

static void checkSupported (const Context& ctx, glu::ShaderType shaderType)
{
	const VkPhysicalDeviceFeatures& features = ctx.getDeviceFeatures();

	if (shaderType == glu::SHADERTYPE_GEOMETRY && !features.geometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader type not supported by device");
	else if (shaderType == glu::SHADERTYPE_TESSELLATION_CONTROL && !features.tessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader type not supported by device");
	else if (shaderType == glu::SHADERTYPE_TESSELLATION_EVALUATION && !features.tessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader type not supported by device");
}

static std::string generateEmptyFragmentSource ()
{
	std::ostringstream src;

	src <<	"#version 310 es\n"
			"layout(location=0) out highp vec4 o_color;\n";

	src << "void main (void)\n{\n";
	src << "	o_color = vec4(0.0);\n";
	src << "}\n";

	return src.str();
}

static std::string generatePassthroughVertexShader (const std::vector<Symbol>& inputs, const char* inputPrefix, const char* outputPrefix)
{

	std::ostringstream	src;
	int					location	= 0;

	src <<	"#version 310 es\n"
			"layout(location = " << location << ") in highp vec4 a_position;\n";

	for (vector<Symbol>::const_iterator input = inputs.begin(); input != inputs.end(); ++input)
	{
		location++;
		src << "layout(location = "<< location << ") in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n"
			<< "layout(location = " << location - 1 << ") flat out " << glu::declare(input->varType, outputPrefix + input->name) << ";\n";
	}

	src << "\nvoid main (void)\n{\n"
		<< "	gl_Position = a_position;\n"
		<< "	gl_PointSize = 1.0;\n";

	for (vector<Symbol>::const_iterator input = inputs.begin(); input != inputs.end(); ++input)
		src << "\t" << outputPrefix << input->name << " = " << inputPrefix << input->name << ";\n";

	src << "}\n";

	return src.str();
}

static std::string generateVertexShader (const ShaderSpec& shaderSpec, const std::string& inputPrefix, const std::string& outputPrefix)
{
	DE_ASSERT(!inputPrefix.empty() && !outputPrefix.empty());

	std::ostringstream	src;

	src <<	"#version 310 es\n";

	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	src << "layout(location = 0) in highp vec4 a_position;\n";

	int locationNumber = 1;
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input, ++locationNumber)
		src <<  "layout(location = " << locationNumber << ") in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n";

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
		src << "\t" << glu::declare(input->varType, input->name) << " = " << inputPrefix << input->name << ";\n";

	// Declare local output variables
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
		src << "\t" << glu::declare(output->varType, output->name) << ";\n";

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

static void generateFragShaderOutAssign (std::ostream& src, const ShaderSpec& shaderSpec, bool useIntOutputs, const std::string& valuePrefix, const std::string& outputPrefix)
{
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
	{
		if (useIntOutputs && glu::isDataTypeFloatOrVec(output->varType.getBasicType()))
			src << "	o_" << output->name << " = floatBitsToUint(" << valuePrefix << output->name << ");\n";
		else if (glu::isDataTypeMatrix(output->varType.getBasicType()))
		{
			const int	numVecs		= glu::getDataTypeMatrixNumColumns(output->varType.getBasicType());

			for (int vecNdx = 0; vecNdx < numVecs; ++vecNdx)
				if (useIntOutputs)
					src << "\t" << outputPrefix << output->name << "_" << vecNdx << " = floatBitsToUint(" << valuePrefix << output->name << "[" << vecNdx << "]);\n";
				else
					src << "\t" << outputPrefix << output->name << "_" << vecNdx << " = " << valuePrefix << output->name << "[" << vecNdx << "];\n";
		}
		else if (glu::isDataTypeBoolOrBVec(output->varType.getBasicType()))
		{
			const int				vecSize		= glu::getDataTypeScalarSize(output->varType.getBasicType());
			const glu::DataType		intBaseType	= vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;

			src << "\t" << outputPrefix << output->name << " = " << glu::getDataTypeName(intBaseType) << "(" << valuePrefix << output->name << ");\n";
		}
		else
			src << "\t" << outputPrefix << output->name << " = " << valuePrefix << output->name << ";\n";
	}
}

static std::string generatePassthroughFragmentShader (const ShaderSpec& shaderSpec, bool useIntOutputs, const std::map<std::string, int>& outLocationMap, const std::string& inputPrefix, const std::string& outputPrefix)
{
	std::ostringstream	src;

	src <<	"#version 310 es\n";

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

static std::string generateGeometryShader (const ShaderSpec& shaderSpec, const std::string& inputPrefix, const std::string& outputPrefix)
{
	DE_ASSERT(!inputPrefix.empty() && !outputPrefix.empty());

	std::ostringstream	src;

	src <<	"#version 310 es\n"
		"#extension GL_EXT_geometry_shader : require\n";

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
		<< "	gl_Position = gl_in[0].gl_Position;\n\n";

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
	src <<  "#version 310 es\n";
	if (!shaderSpec.globalDeclarations.empty())
		src << shaderSpec.globalDeclarations << "\n";

	int locationNumber = 0;
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input, ++locationNumber)
		src << "layout(location = " << locationNumber << ") flat in " << glu::declare(input->varType, inputPrefix + input->name) << ";\n";

	generateFragShaderOutputDecl(src, shaderSpec, useIntOutputs, outLocationMap, outputPrefix);

	src << "\nvoid main (void)\n{\n";

	// Declare & fetch local input variables
	for (vector<Symbol>::const_iterator input = shaderSpec.inputs.begin(); input != shaderSpec.inputs.end(); ++input)
		src << "\t" << glu::declare(input->varType, input->name) << " = " << inputPrefix << input->name << ";\n";

	// Declare output variables
	for (vector<Symbol>::const_iterator output = shaderSpec.outputs.begin(); output != shaderSpec.outputs.end(); ++output)
		src << "\t" << glu::declare(output->varType, output->name) << ";\n";

	// Operation - indented to correct level.
	{
		std::istringstream	opSrc	(shaderSpec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	generateFragShaderOutAssign(src, shaderSpec, useIntOutputs, "", outputPrefix);

	src << "}\n";

	return src.str();
}

// FragmentOutExecutor

class FragmentOutExecutor : public ShaderExecutor
{
public:
														FragmentOutExecutor		(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual												~FragmentOutExecutor	(void);

	virtual void										execute					(const Context&			ctx,
																				 int					numValues,
																				 const void* const*		inputs,
																				 void* const*			outputs);

protected:
	const FragmentOutputLayout							m_outputLayout;
private:
	void												bindAttributes			(const Context&			ctx,
																				 Allocator&				memAlloc,
																				 int					numValues,
																				 const void* const*		inputs);

	void												addAttribute			(const Context&			ctx,
																				 Allocator&				memAlloc,
																				 deUint32				bindingLocation,
																				 VkFormat				format,
																				 deUint32				sizePerElement,
																				 deUint32				count,
																				 const void*			dataPtr);
	// reinit render data members
	virtual void										clearRenderData			(void);

	typedef de::SharedPtr<Unique<VkImage> >				VkImageSp;
	typedef de::SharedPtr<Unique<VkImageView> >			VkImageViewSp;
	typedef de::SharedPtr<Unique<VkBuffer> >			VkBufferSp;
	typedef de::SharedPtr<de::UniquePtr<Allocation> >	AllocationSp;

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

FragmentOutExecutor::FragmentOutExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: ShaderExecutor	(shaderSpec, shaderType)
	, m_outputLayout	(computeFragmentOutputLayout(m_shaderSpec.outputs))
{
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
		case glu::TYPE_UINT:	channelType = tcu::TextureFormat::UNSIGNED_INT32;												break;
		case glu::TYPE_INT:		channelType = tcu::TextureFormat::SIGNED_INT32;													break;
		case glu::TYPE_BOOL:	channelType = tcu::TextureFormat::SIGNED_INT32;													break;
		case glu::TYPE_FLOAT:	channelType = useIntOutputs ? tcu::TextureFormat::UNSIGNED_INT32 : tcu::TextureFormat::FLOAT;	break;
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

void FragmentOutExecutor::addAttribute (const Context& ctx, Allocator& memAlloc, deUint32 bindingLocation, VkFormat format, deUint32 sizePerElement, deUint32 count, const void* dataPtr)
{
	// Add binding specification
	const deUint32 binding = (deUint32)m_vertexBindingDescriptions.size();
	const VkVertexInputBindingDescription bindingDescription =
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
	const VkDevice				vkDevice			= ctx.getDevice();
	const DeviceInterface&		vk					= ctx.getDeviceInterface();
	const deUint32				queueFamilyIndex	= ctx.getUniversalQueueFamilyIndex();

	const VkDeviceSize inputSize = sizePerElement * count;
	const VkBufferCreateInfo vertexBufferParams =
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

	Move<VkBuffer> buffer = createBuffer(vk, vkDevice, &vertexBufferParams);
	de::MovePtr<Allocation> alloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), alloc->getOffset()));

	deMemcpy(alloc->getHostPtr(), dataPtr, (size_t)inputSize);
	flushMappedMemoryRange(vk, vkDevice, alloc->getMemory(), alloc->getOffset(), inputSize);

	m_vertexBuffers.push_back(de::SharedPtr<Unique<VkBuffer> >(new Unique<VkBuffer>(buffer)));
	m_vertexBufferAllocs.push_back(de::SharedPtr<de::UniquePtr<Allocation> >(new de::UniquePtr<Allocation>(alloc)));
}

void FragmentOutExecutor::bindAttributes (const Context& ctx, Allocator& memAlloc, int numValues, const void* const* inputs)
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

		if (glu::isDataTypeFloatOrVec(basicType))
			elementSize = sizeof(float);
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
			addAttribute(ctx, memAlloc, (deUint32)m_vertexBindingDescriptions.size(), format, elementSize * vecSize, numValues, ptr);
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

void FragmentOutExecutor::execute (const Context& ctx, int numValues, const void* const* inputs, void* const* outputs)
{
	checkSupported(ctx, m_shaderType);

	const VkDevice										vkDevice				= ctx.getDevice();
	const DeviceInterface&								vk						= ctx.getDeviceInterface();
	const VkQueue										queue					= ctx.getUniversalQueue();
	const deUint32										queueFamilyIndex		= ctx.getUniversalQueueFamilyIndex();
	Allocator&											memAlloc				= ctx.getDefaultAllocator();

	const deUint32										renderSizeX				= de::min(static_cast<deUint32>(DEFAULT_RENDER_WIDTH), (deUint32)numValues);
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

	Move<VkFence>										fence;

	Move<VkDescriptorPool>								descriptorPool;
	Move<VkDescriptorSetLayout>							descriptorSetLayout;
	Move<VkDescriptorSet>								descriptorSet;

	clearRenderData();

	// Compute positions - 1px points are used to drive fragment shading.
	positions = computeVertexPositions(numValues, renderSize.cast<int>());

	// Bind attributes
	addAttribute(ctx, memAlloc, 0u, VK_FORMAT_R32G32_SFLOAT, sizeof(tcu::Vec2), (deUint32)positions.size(), &positions[0]);
	bindAttributes(ctx, memAlloc, numValues, inputs);

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
			const bool		isFloat		= isDataTypeFloatOrVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isSigned	= isDataTypeIntOrIVec (m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const bool		isBool		= isDataTypeBoolOrBVec(m_shaderSpec.outputs[outNdx].varType.getBasicType());
			const VkFormat	colorFormat = isFloat ? VK_FORMAT_R32G32B32A32_SFLOAT : (isSigned || isBool ? VK_FORMAT_R32G32B32A32_SINT : VK_FORMAT_R32G32B32A32_UINT);

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
				colorImageAllocs.push_back(de::SharedPtr<de::UniquePtr<Allocation> >(new de::UniquePtr<Allocation>(colorImageAlloc)));

				attachments.push_back(colorAttachmentDescription);
				colorBlendAttachmentStates.push_back(colorBlendAttachmentState);

				const VkAttachmentReference colorAttachmentReference = {
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

	// Create descriptors
	{
		addUniforms(vkDevice, vk, queue, queueFamilyIndex, memAlloc);

		descriptorSetLayout = m_descriptorSetLayoutBuilder.build(vk, vkDevice);
		if (!m_uniformInfos.empty())
			descriptorPool = m_descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		else 
		{
			const VkDescriptorPoolSize 			poolSizeCount 	= { vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
			const VkDescriptorPoolCreateInfo	createInfo 		=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				DE_NULL,
				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
				1u,
				1u,
				&poolSizeCount,
			};

			descriptorPool = createDescriptorPool(vk, vkDevice, &createInfo);
		}

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
			vk::DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;

			uploadUniforms(descriptorSetUpdateBuilder, *descriptorSet);

			descriptorSetUpdateBuilder.update(vk, vkDevice);
		}
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			1,													// deUint32						descriptorSetCount;
			&*descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		vertexShaderModule		= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("vert"), 0);
		fragmentShaderModule	= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("frag"), 0);

		if (useGeometryShader)
		{
			geometryShaderModule = createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("geom"), 0);
		}
	}

	// Create pipeline
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStageParams;

		const VkPipelineShaderStageCreateInfo vertexShaderStageParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
			*vertexShaderModule,										// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkPipelineShaderStageCreateInfo fragmentShaderStageParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
			DE_NULL,													// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
			*fragmentShaderModule,										// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
		};

		shaderStageParams.push_back(vertexShaderStageParams);
		shaderStageParams.push_back(fragmentShaderStageParams);

		if (useGeometryShader)
		{
			const VkPipelineShaderStageCreateInfo geometryShaderStageParams =
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_GEOMETRY_BIT,								// VkShaderStageFlagBits				stage;
				*geometryShaderModule,										// VkShaderModule						module;
				"main",														// VkShader								shader;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			};

			shaderStageParams.push_back(geometryShaderStageParams);
		}

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

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,								// VkPrimitiveTopology						topology;
			DE_FALSE														// VkBool32									primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)renderSize.x(),		// float	width;
			(float)renderSize.y(),		// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor =
		{
			{
				0u,						// deUint32	x;
				0u,						// deUint32	y;
			},							// VkOffset2D	offset;
			{
				renderSize.x(),			// deUint32	width;
				renderSize.y(),			// deUint32	height;
			},							// VkExtent2D	extent;
		};

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType										sType;
			DE_NULL,												// const void*											pNext;
			0u,														// VkPipelineViewportStateCreateFlags					flags;
			1u,														// deUint32												viewportCount;
			&viewport,												// const VkViewport*									pViewports;
			1u,														// deUint32												scissorsCount;
			&scissor												// const VkRect2D*										pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType								sType;
			DE_NULL,														// const void*									pNext;
			(VkPipelineRasterizationStateCreateFlags)0u,					//VkPipelineRasterizationStateCreateFlags		flags;
			VK_FALSE,														// VkBool32										depthClipEnable;
			VK_FALSE,														// VkBool32										rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode								polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags								cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace									frontFace;
			VK_FALSE,														// VkBool32										depthBiasEnable;
			0.0f,															// float										depthBias;
			0.0f,															// float										depthBiasClamp;
			0.0f,															// float										slopeScaledDepthBias;
			1.0f															// float										lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,														// VkBool32									sampleShadingEnable;
			0.0f,															// float									minSampleShading;
			DE_NULL,														// const VkSampleMask*						pSampleMask;
			VK_FALSE,														// VkBool32									alphaToCoverageEnable;
			VK_FALSE														// VkBool32									alphaToOneEnable;
		};

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

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			(deUint32)shaderStageParams.size(),					// deUint32											stageCount;
			&shaderStageParams[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*pipelineLayout,									// VkPipelineLayout									layout;
			*renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		graphicsPipeline = createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,				// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,									// deUint32				queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*cmdPool,										// VkCmdPool				cmdPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			1												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkCmdBufferOptimizeFlags		flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*renderPass,											// VkRenderPass			renderPass;
			*framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 }, { renderSize.x(), renderSize.y() } },		// VkRect2D				renderArea;
			(deUint32)attachmentClearValues.size(),					// deUint32				attachmentCount;
			&attachmentClearValues[0]								// const VkClearValue*	pAttachmentClearValues;
		};

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));

		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  0, (const VkBufferMemoryBarrier*)DE_NULL,
							  (deUint32)colorImagePreRenderBarriers.size(), colorImagePreRenderBarriers.empty() ? DE_NULL : &colorImagePreRenderBarriers[0]);
		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

		const deUint32 numberOfVertexAttributes = (deUint32)m_vertexBuffers.size();

		std::vector<VkDeviceSize> offsets(numberOfVertexAttributes, 0);

		std::vector<VkBuffer> buffers(numberOfVertexAttributes);
		for (size_t i = 0; i < numberOfVertexAttributes; i++)
		{
			buffers[i] = m_vertexBuffers[i].get()->get();
		}

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, numberOfVertexAttributes, &buffers[0], &offsets[0]);
		vk.cmdDraw(*cmdBuffer, (deUint32)positions.size(), 1u, 0u, 0u);

		vk.cmdEndRenderPass(*cmdBuffer);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  0, (const VkBufferMemoryBarrier*)DE_NULL,
							  (deUint32)colorImagePostRenderBarriers.size(), colorImagePostRenderBarriers.empty() ? DE_NULL : &colorImagePostRenderBarriers[0]);

		VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, vkDevice, &fenceParams);
	}

	// Execute Draw
	{

		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
			DE_NULL,								// pNext
			0u,										// waitSemaphoreCount
			DE_NULL,								// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,
			1u,										// commandBufferCount
			&cmdBuffer.get(),						// pCommandBuffers
			0u,										// signalSemaphoreCount
			DE_NULL									// pSignalSemaphores
		};

		VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), DE_TRUE, ~(0ull) /* infinity*/));
	}

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

		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,		// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex							// deUint32				queueFamilyIndex;
		};

		Move<VkCommandPool>	copyCmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);

		const VkCommandBufferAllocateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*copyCmdPool,									// VkCmdPool				cmdPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			1u												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkCmdBufferOptimizeFlags			flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

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
			deUint32*					dstPtrBase		= static_cast<deUint32*>(outputs[outNdx]);
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

					Move<VkCommandBuffer> copyCmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

					const VkSubmitInfo submitInfo =
					{
						VK_STRUCTURE_TYPE_SUBMIT_INFO,
						DE_NULL,
						0u,
						(const VkSemaphore*)DE_NULL,
						(const VkPipelineStageFlags*)DE_NULL,
						1u,
						&copyCmdBuffer.get(),
						0u,
						(const VkSemaphore*)DE_NULL,
					};

					VK_CHECK(vk.beginCommandBuffer(*copyCmdBuffer, &cmdBufferBeginInfo));
					vk.cmdCopyImageToBuffer(*copyCmdBuffer, colorImages[outLocation + locNdx].get()->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &copyParams);
					VK_CHECK(vk.endCommandBuffer(*copyCmdBuffer));

					VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));
					VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
					VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity */));
				}

				const VkMappedMemoryRange range =
				{
					VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	// VkStructureType	sType;
					DE_NULL,								// const void*		pNext;
					readImageBufferMemory->getMemory(),		// VkDeviceMemory	mem;
					0,										// VkDeviceSize		offset;
					imageSizeBytes,							// VkDeviceSize		size;
				};

				VK_CHECK(vk.invalidateMappedMemoryRanges(vkDevice, 1u, &range));

				tmpBuf.setStorage(readFormat, renderSize.x(), renderSize.y());

				const tcu::TextureFormat resultFormat(tcu::TextureFormat::RGBA, format.type);
				const tcu::ConstPixelBufferAccess resultAccess(resultFormat, renderSize.x(), renderSize.y(), 1, readImageBufferMemory->getHostPtr());

				tcu::copy(tmpBuf.getAccess(), resultAccess);

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

// VertexShaderExecutor

class VertexShaderExecutor : public FragmentOutExecutor
{
public:
								VertexShaderExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual						~VertexShaderExecutor	(void);

	virtual void				log						(tcu::TestLog& dst) const { /* TODO */ (void)dst;}

	virtual void				setShaderSources		(SourceCollections& programCollection) const;

};

VertexShaderExecutor::VertexShaderExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: FragmentOutExecutor		(shaderSpec, shaderType)
{
}

VertexShaderExecutor::~VertexShaderExecutor (void)
{
}

void VertexShaderExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShader(m_shaderSpec, "a_", "vtx_out_"));
	/* \todo [2015-09-11 hegedusd] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generatePassthroughFragmentShader(m_shaderSpec, false, m_outputLayout.locationMap, "vtx_out_", "o_"));
}

// GeometryShaderExecutor

class GeometryShaderExecutor : public FragmentOutExecutor
{
public:
								GeometryShaderExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual						~GeometryShaderExecutor	(void);

	virtual void				log						(tcu::TestLog& dst) const	{ /* TODO */ (void)dst; }

	virtual void				setShaderSources		(SourceCollections& programCollection) const;

};

GeometryShaderExecutor::GeometryShaderExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: FragmentOutExecutor		(shaderSpec, shaderType)
{
}

GeometryShaderExecutor::~GeometryShaderExecutor (void)
{
}

void GeometryShaderExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generatePassthroughVertexShader(m_shaderSpec.inputs, "a_", "vtx_out_"));

	programCollection.glslSources.add("geom") << glu::GeometrySource(generateGeometryShader(m_shaderSpec, "vtx_out_", "geom_out_"));

	/* \todo [2015-09-18 rsipka] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generatePassthroughFragmentShader(m_shaderSpec, false, m_outputLayout.locationMap, "geom_out_", "o_"));

}

// FragmentShaderExecutor

class FragmentShaderExecutor : public FragmentOutExecutor
{
public:
								FragmentShaderExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual						~FragmentShaderExecutor (void);

	virtual void				log						(tcu::TestLog& dst) const { /* TODO */ (void)dst; }

	virtual void				setShaderSources		(SourceCollections& programCollection) const;

};

FragmentShaderExecutor::FragmentShaderExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: FragmentOutExecutor		(shaderSpec, shaderType)
{
}

FragmentShaderExecutor::~FragmentShaderExecutor (void)
{
}

void FragmentShaderExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generatePassthroughVertexShader(m_shaderSpec.inputs, "a_", "vtx_out_"));
	/* \todo [2015-09-11 hegedusd] set useIntOutputs parameter if needed. */
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateFragmentShader(m_shaderSpec, false, m_outputLayout.locationMap, "vtx_out_", "o_"));
}

// Shared utilities for compute and tess executors

static deUint32 getVecStd430ByteAlignment (glu::DataType type)
{
	switch (glu::getDataTypeScalarSize(type))
	{
		case 1:		return 4u;
		case 2:		return 8u;
		case 3:		return 16u;
		case 4:		return 16u;
		default:
			DE_ASSERT(false);
			return 0u;
	}
}

class BufferIoExecutor : public ShaderExecutor
{
public:
							BufferIoExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual					~BufferIoExecutor	(void);

	virtual void			log					(tcu::TestLog& dst) const	{ /* TODO */ (void)dst; }

protected:
	enum
	{
		INPUT_BUFFER_BINDING	= 0,
		OUTPUT_BUFFER_BINDING	= 1,
	};

	void					initBuffers			(const Context& ctx, int numValues);
	VkBuffer				getInputBuffer		(void) const		{ return *m_inputBuffer;					}
	VkBuffer				getOutputBuffer		(void) const		{ return *m_outputBuffer;					}
	deUint32				getInputStride		(void) const		{ return getLayoutStride(m_inputLayout);	}
	deUint32				getOutputStride		(void) const		{ return getLayoutStride(m_outputLayout);	}

	void					uploadInputBuffer	(const Context& ctx, const void* const* inputPtrs, int numValues);
	void					readOutputBuffer	(const Context& ctx, void* const* outputPtrs, int numValues);

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

	static void				copyToBuffer		(const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr);
	static void				copyFromBuffer		(const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr);

	de::MovePtr<Allocation>	m_inputAlloc;
	de::MovePtr<Allocation>	m_outputAlloc;

	vector<VarLayout>		m_inputLayout;
	vector<VarLayout>		m_outputLayout;
};

BufferIoExecutor::BufferIoExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: ShaderExecutor (shaderSpec, shaderType)
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
			const deUint32	size		= (deUint32)glu::getDataTypeScalarSize(basicType) * (int)sizeof(deUint32);

			curOffset		= (deUint32)deAlign32((int)curOffset, (int)alignment);
			maxAlignment	= de::max(maxAlignment, alignment);

			layoutEntry.offset			= curOffset;
			layoutEntry.matrixStride	= 0;

			curOffset += size;
		}
		else if (glu::isDataTypeMatrix(basicType))
		{
			const int				numVecs			= glu::getDataTypeMatrixNumColumns(basicType);
			const glu::DataType		vecType			= glu::getDataTypeFloatVec(glu::getDataTypeMatrixNumRows(basicType));
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
	for (vector<Symbol>::const_iterator symIter = spec.inputs.begin(); symIter != spec.inputs.end(); ++symIter)
		src << "\t" << glu::declare(symIter->varType, symIter->name) << " = inputs[" << invocationNdxName << "]." << symIter->name << ";\n";

	for (vector<Symbol>::const_iterator symIter = spec.outputs.begin(); symIter != spec.outputs.end(); ++symIter)
		src << "\t" << glu::declare(symIter->varType, symIter->name) << ";\n";

	src << "\n";

	{
		std::istringstream	opSrc	(spec.source);
		std::string			line;

		while (std::getline(opSrc, line))
			src << "\t" << line << "\n";
	}

	src << "\n";
	for (vector<Symbol>::const_iterator symIter = spec.outputs.begin(); symIter != spec.outputs.end(); ++symIter)
		src << "\toutputs[" << invocationNdxName << "]." << symIter->name << " = " << symIter->name << ";\n";
}

void BufferIoExecutor::copyToBuffer (const glu::VarType& varType, const VarLayout& layout, int numValues, const void* srcBasePtr, void* dstBasePtr)
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
				const int		srcOffset		= (int)sizeof(deUint32) * (elemNdx * scalarSize + vecNdx * numComps);
				const int		dstOffset		= layout.offset + layout.stride * elemNdx + (isMatrix ? layout.matrixStride * vecNdx : 0);
				const deUint8*	srcPtr			= (const deUint8*)srcBasePtr + srcOffset;
				deUint8*		dstPtr			= (deUint8*)dstBasePtr + dstOffset;

				deMemcpy(dstPtr, srcPtr, sizeof(deUint32) * numComps);
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
				const int		srcOffset		= layout.offset + layout.stride * elemNdx + (isMatrix ? layout.matrixStride * vecNdx : 0);
				const int		dstOffset		= (int)sizeof(deUint32) * (elemNdx * scalarSize + vecNdx * numComps);
				const deUint8*	srcPtr			= (const deUint8*)srcBasePtr + srcOffset;
				deUint8*		dstPtr			= (deUint8*)dstBasePtr + dstOffset;

				deMemcpy(dstPtr, srcPtr, sizeof(deUint32) * numComps);
			}
		}
	}
	else
		throw tcu::InternalError("Unsupported type");
}

void BufferIoExecutor::uploadInputBuffer (const Context& ctx, const void* const* inputPtrs, int numValues)
{
	const VkDevice			vkDevice			= ctx.getDevice();
	const DeviceInterface&	vk					= ctx.getDeviceInterface();

	const deUint32			inputStride			= getLayoutStride(m_inputLayout);
	const int				inputBufferSize		= inputStride * numValues;

	if (inputBufferSize == 0)
		return; // No inputs

	DE_ASSERT(m_shaderSpec.inputs.size() == m_inputLayout.size());
	for (size_t inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
	{
		const glu::VarType&		varType		= m_shaderSpec.inputs[inputNdx].varType;
		const VarLayout&		layout		= m_inputLayout[inputNdx];

		copyToBuffer(varType, layout, numValues, inputPtrs[inputNdx], m_inputAlloc->getHostPtr());
	}

	flushMappedMemoryRange(vk, vkDevice, m_inputAlloc->getMemory(), m_inputAlloc->getOffset(), inputBufferSize);
}

void BufferIoExecutor::readOutputBuffer (const Context& ctx, void* const* outputPtrs, int numValues)
{
	const VkDevice			vkDevice			= ctx.getDevice();
	const DeviceInterface&	vk					= ctx.getDeviceInterface();

	const deUint32			outputStride		= getLayoutStride(m_outputLayout);
	const int				outputBufferSize	= numValues * outputStride;

	DE_ASSERT(outputBufferSize > 0); // At least some outputs are required.

	invalidateMappedMemoryRange(vk, vkDevice, m_outputAlloc->getMemory(), m_outputAlloc->getOffset(), outputBufferSize);

	DE_ASSERT(m_shaderSpec.outputs.size() == m_outputLayout.size());
	for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
	{
		const glu::VarType&		varType		= m_shaderSpec.outputs[outputNdx].varType;
		const VarLayout&		layout		= m_outputLayout[outputNdx];

		copyFromBuffer(varType, layout, numValues, m_outputAlloc->getHostPtr(), outputPtrs[outputNdx]);
	}
}

void BufferIoExecutor::initBuffers (const Context& ctx, int numValues)
{
	const deUint32				inputStride			= getLayoutStride(m_inputLayout);
	const deUint32				outputStride		= getLayoutStride(m_outputLayout);
	// Avoid creating zero-sized buffer/memory
	const size_t				inputBufferSize		= numValues * inputStride ? (numValues * inputStride) : 1;
	const size_t				outputBufferSize	= numValues * outputStride;

	// Upload data to buffer
	const VkDevice				vkDevice			= ctx.getDevice();
	const DeviceInterface&		vk					= ctx.getDeviceInterface();
	const deUint32				queueFamilyIndex	= ctx.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= ctx.getDefaultAllocator();

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
						ComputeShaderExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual				~ComputeShaderExecutor	(void);

	virtual void		setShaderSources		(SourceCollections& programCollection) const;

	virtual void		execute					(const Context& ctx, int numValues, const void* const* inputs, void* const* outputs);

protected:
	static std::string	generateComputeShader	(const ShaderSpec& spec);
};

ComputeShaderExecutor::ComputeShaderExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: BufferIoExecutor	(shaderSpec, shaderType)
{
}

ComputeShaderExecutor::~ComputeShaderExecutor	(void)
{
}

std::string ComputeShaderExecutor::generateComputeShader (const ShaderSpec& spec)
{
	std::ostringstream src;
	src <<  "#version 310 es\n";

	if (!spec.globalDeclarations.empty())
		src << spec.globalDeclarations << "\n";

	src << "layout(local_size_x = 1) in;\n"
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

void ComputeShaderExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("compute") << glu::ComputeSource(generateComputeShader(m_shaderSpec));
}

void ComputeShaderExecutor::execute (const Context& ctx, int numValues, const void* const* inputs, void* const* outputs)
{
	checkSupported(ctx, m_shaderType);

	const VkDevice					vkDevice				= ctx.getDevice();
	const DeviceInterface&			vk						= ctx.getDeviceInterface();
	const VkQueue					queue					= ctx.getUniversalQueue();
	const deUint32					queueFamilyIndex		= ctx.getUniversalQueueFamilyIndex();
	Allocator&						memAlloc				= ctx.getDefaultAllocator();

	Move<VkShaderModule>			computeShaderModule;
	Move<VkPipeline>				computePipeline;
	Move<VkPipelineLayout>			pipelineLayout;
	Move<VkCommandPool>				cmdPool;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkFence>					fence;

	initBuffers(ctx, numValues);

	// Setup input buffer & copy data
	uploadInputBuffer(ctx, inputs, numValues);

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex								// deUint32				queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	const VkCommandBufferAllocateInfo cmdBufferParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		*cmdPool,										// VkCmdPool				cmdPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
		1u												// deUint32					bufferCount;
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		0u,												// VkCmdBufferOptimizeFlags			flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	m_descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	m_descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	addUniforms(vkDevice, vk, queue, queueFamilyIndex, memAlloc);

	descriptorSetLayout = m_descriptorSetLayoutBuilder.build(vk, vkDevice);
	descriptorPool = m_descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

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
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						CdescriptorSetCount;
			&*descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		computeShaderModule		= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("compute"), 0);
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

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};
		fence = createFence(vk, vkDevice, &fenceParams);
	}

	const int maxValuesPerInvocation	= ctx.getDeviceProperties().limits.maxComputeWorkGroupSize[0];
	int					curOffset		= 0;
	const deUint32		inputStride		= getInputStride();
	const deUint32		outputStride	= getOutputStride();

	while (curOffset < numValues)
	{
		Move<VkCommandBuffer>		cmdBuffer;
		const int numToExec = de::min(maxValuesPerInvocation, numValues-curOffset);

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

			uploadUniforms(descriptorSetUpdateBuilder, *descriptorSet);

			descriptorSetUpdateBuilder.update(vk, vkDevice);
		}

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);
		VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

		vk.cmdDispatch(*cmdBuffer, numToExec, 1, 1);

		VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

		curOffset += numToExec;

		// Execute
		{
			VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));

			const VkSubmitInfo submitInfo =
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,
				DE_NULL,
				0u,
				(const VkSemaphore*)DE_NULL,
				(const VkPipelineStageFlags*)DE_NULL,
				1u,
				&cmdBuffer.get(),
				0u,
				(const VkSemaphore*)DE_NULL,
			};

			VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
			VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity*/));
		}
	}

	// Read back data
	readOutputBuffer(ctx, outputs, numValues);
}

// Tessellation utils

static std::string generateVertexShaderForTess (void)
{
	std::ostringstream	src;
	src <<  "#version 310 es\n"
		<< "void main (void)\n{\n"
		<< "	gl_Position = vec4(gl_VertexIndex/2, gl_VertexIndex%2, 0.0, 1.0);\n"
		<< "}\n";

	return src.str();
}

class TessellationExecutor : public BufferIoExecutor
{
public:
						TessellationExecutor		(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual				~TessellationExecutor		(void);

	void				renderTess					(const Context& ctx, deUint32 numValues, deUint32 vertexCount, deUint32 patchControlPoints);
};

TessellationExecutor::TessellationExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: BufferIoExecutor	(shaderSpec, shaderType)
{
}

TessellationExecutor::~TessellationExecutor (void)
{
}

void TessellationExecutor::renderTess (const Context& ctx, deUint32 numValues, deUint32 vertexCount, deUint32 patchControlPoints)
{
	const size_t						inputBufferSize				= numValues * getInputStride();
	const VkDevice						vkDevice					= ctx.getDevice();
	const DeviceInterface&				vk							= ctx.getDeviceInterface();
	const VkQueue						queue						= ctx.getUniversalQueue();
	const deUint32						queueFamilyIndex			= ctx.getUniversalQueueFamilyIndex();
	Allocator&							memAlloc					= ctx.getDefaultAllocator();

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

	Move<VkFence>						fence;

	Move<VkDescriptorPool>				descriptorPool;
	Move<VkDescriptorSetLayout>			descriptorSetLayout;
	Move<VkDescriptorSet>				descriptorSet;

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
		m_descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		m_descriptorSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		addUniforms(vkDevice, vk, queue, queueFamilyIndex, memAlloc);

		descriptorSetLayout = m_descriptorSetLayoutBuilder.build(vk, vkDevice);
		descriptorPool = m_descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

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

			uploadUniforms(descriptorSetUpdateBuilder, *descriptorSet);

			descriptorSetUpdateBuilder.update(vk, vkDevice);
		}
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						descriptorSetCount;
			&*descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shader modules
	{
		vertexShaderModule		= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("vert"), 0);
		tessControlShaderModule	= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("tess_control"), 0);
		tessEvalShaderModule	= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("tess_eval"), 0);
		fragmentShaderModule	= createShaderModule(vk, vkDevice, ctx.getBinaryCollection().get("frag"), 0);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[4] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBit					stage;
				*vertexShaderModule,										// VkShaderModule						shader;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,					// VkShaderStageFlagBit					stage;
				*tessControlShaderModule,									// VkShaderModule						shader;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,				// VkShaderStageFlagBit					stage;
				*tessEvalShaderModule,										// VkShaderModule						shader;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBit					stage;
				*fragmentShaderModule,										// VkShaderModule						shader;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			}
		};

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

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,							// VkPipelineShaderStageCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,								// VkPrimitiveTopology					topology;
			DE_FALSE														// VkBool32								primitiveRestartEnable;
		};

		struct VkPipelineTessellationStateCreateInfo tessellationStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags	flags;
			patchControlPoints												// uint32_t									patchControlPoints;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)renderSize.x(),		// float	width;
			(float)renderSize.y(),		// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor =
		{
			{
				0u,						// deUint32	x;
				0u,						// deUint32	y;
			},							// VkOffset2D	offset;
			{
				renderSize.x(),			// deUint32	width;
				renderSize.y(),			// deUint32	height;
			},							// VkExtent2D	extent;
		};

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewPortStateCreateFlags	flags;
			1u,														// deUint32								viewportCount;
			&viewport,												// const VkViewport*					pViewports;
			1u,														// deUint32								scissorsCount;
			&scissor												// const VkRect2D*						pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStageCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClipEnable;
			VK_FALSE,														// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullMode								cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBias;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									slopeScaledDepthBias;
			1.0f															// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,														// VkBool32									sampleShadingEnable;
			0.0f,															// float									minSampleShading;
			DE_NULL,														// const VkSampleMask*						pSampleMask;
			VK_FALSE,														// VkBool32									alphaToCoverageEnable;
			VK_FALSE														// VkBool32									alphaToOneEnable;
		};

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			VK_FALSE,						// VkBool32					blendEnable;
			VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcBlendColor;
			VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			destBlendColor;
			VK_BLEND_OP_ADD,				// VkBlendOp				blendOpColor;
			VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcBlendAlpha;
			VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			destBlendAlpha;
			VK_BLEND_OP_ADD,				// VkBlendOp				blendOpAlpha;
			(VK_COLOR_COMPONENT_R_BIT |
			 VK_COLOR_COMPONENT_G_BIT |
			 VK_COLOR_COMPONENT_B_BIT |
			 VK_COLOR_COMPONENT_A_BIT)		// VkColorComponentFlags	colorWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,													// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConst[4];
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			0u,													// VkPipelineCreateFlags							flags;
			4u,													// deUint32											stageCount;
			shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			&tessellationStateParams,							// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*pipelineLayout,									// VkPipelineLayout									layout;
			*renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		graphicsPipeline = createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*cmdPool,										// VkCmdPool				cmdPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			1u												// uint32_t					bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkCmdBufferOptimizeFlags			flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const VkClearValue clearValues[1] =
		{
			getDefaultClearColor()
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*renderPass,											// VkRenderPass			renderPass;
			*framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 }, { renderSize.x(), renderSize.y() } },		// VkRect2D				renderArea;
			1,														// deUint32				attachmentCount;
			clearValues												// const VkClearValue*	pClearValues;
		};

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));

		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);

		vk.cmdEndRenderPass(*cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};
		fence = createFence(vk, vkDevice, &fenceParams);
	}

	// Execute Draw
	{
		VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));
		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			DE_NULL,
			0u,
			(const VkSemaphore*)0,
			(const VkPipelineStageFlags*)DE_NULL,
			1u,
			&cmdBuffer.get(),
			0u,
			(const VkSemaphore*)0,
		};
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity*/));
	}
}

// TessControlExecutor

class TessControlExecutor : public TessellationExecutor
{
public:
						TessControlExecutor			(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual				~TessControlExecutor		(void);

	virtual void		setShaderSources			(SourceCollections& programCollection) const;

	virtual void		execute						(const Context& ctx, int numValues, const void* const* inputs, void* const* outputs);

protected:
	static std::string	generateTessControlShader	(const ShaderSpec& shaderSpec);
};

TessControlExecutor::TessControlExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: TessellationExecutor (shaderSpec, shaderType)
{
}

TessControlExecutor::~TessControlExecutor (void)
{
}

std::string TessControlExecutor::generateTessControlShader (const ShaderSpec& shaderSpec)
{
	std::ostringstream src;
	src <<  "#version 310 es\n"
			"#extension GL_EXT_tessellation_shader : require\n\n";

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

	src <<  "#version 310 es\n"
			"#extension GL_EXT_tessellation_shader : require\n\n";

	src << "layout(triangles, ccw) in;\n";

	src << "\nvoid main (void)\n{\n"
		<< "\tgl_Position = vec4(gl_TessCoord.xy, 0.0, 1.0);\n"
		<< "}\n";

	return src.str();
}

void TessControlExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShaderForTess());
	programCollection.glslSources.add("tess_control") << glu::TessellationControlSource(generateTessControlShader(m_shaderSpec));
	programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(generateEmptyTessEvalShader());
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateEmptyFragmentSource());
}

void TessControlExecutor::execute (const Context& ctx, int numValues, const void* const* inputs, void* const* outputs)
{
	const deUint32	patchSize	= 3;

	checkSupported(ctx, m_shaderType);

	initBuffers(ctx, numValues);

	// Setup input buffer & copy data
	uploadInputBuffer(ctx, inputs, numValues);

	renderTess(ctx, numValues, patchSize * numValues, patchSize);

	// Read back data
	readOutputBuffer(ctx, outputs, numValues);
}

// TessEvaluationExecutor

class TessEvaluationExecutor : public TessellationExecutor
{
public:
						TessEvaluationExecutor	(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);
	virtual				~TessEvaluationExecutor	(void);

	virtual void		setShaderSources		(SourceCollections& programCollection) const;

	virtual void		execute					(const Context& ctx, int numValues, const void* const* inputs, void* const* outputs);

protected:
	static std::string	generateTessEvalShader	(const ShaderSpec& shaderSpec);
};

TessEvaluationExecutor::TessEvaluationExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: TessellationExecutor (shaderSpec, shaderType)
{
}

TessEvaluationExecutor::~TessEvaluationExecutor (void)
{
}

static std::string generatePassthroughTessControlShader (void)
{
	std::ostringstream src;

	src <<  "#version 310 es\n"
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

	src <<  "#version 310 es\n"
			"#extension GL_EXT_tessellation_shader : require\n\n";

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

void TessEvaluationExecutor::setShaderSources (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(generateVertexShaderForTess());
	programCollection.glslSources.add("tess_control") << glu::TessellationControlSource(generatePassthroughTessControlShader());
	programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(generateTessEvalShader(m_shaderSpec));
	programCollection.glslSources.add("frag") << glu::FragmentSource(generateEmptyFragmentSource());
}

void TessEvaluationExecutor::execute (const Context& ctx, int numValues, const void* const* inputs, void* const* outputs)
{
	checkSupported(ctx, m_shaderType);

	const int	patchSize		= 2;
	const int	alignedValues	= deAlign32(numValues, patchSize);

	// Initialize buffers with aligned value count to make room for padding
	initBuffers(ctx, alignedValues);

	// Setup input buffer & copy data
	uploadInputBuffer(ctx, inputs, numValues);

	renderTess(ctx, (deUint32)alignedValues, (deUint32)alignedValues, (deUint32)patchSize);

	// Read back data
	readOutputBuffer(ctx, outputs, numValues);
}

} // anonymous

// ShaderExecutor

ShaderExecutor::ShaderExecutor (const ShaderSpec& shaderSpec, glu::ShaderType shaderType)
	: m_shaderSpec	(shaderSpec)
	, m_shaderType	(shaderType)
{
}

ShaderExecutor::~ShaderExecutor (void)
{
}

// Utilities

ShaderExecutor* createExecutor (glu::ShaderType shaderType, const ShaderSpec& shaderSpec)
{
	switch (shaderType)
	{
		case glu::SHADERTYPE_VERTEX:					return new VertexShaderExecutor		(shaderSpec, shaderType);
		case glu::SHADERTYPE_TESSELLATION_CONTROL:		return new TessControlExecutor		(shaderSpec, shaderType);
		case glu::SHADERTYPE_TESSELLATION_EVALUATION:	return new TessEvaluationExecutor	(shaderSpec, shaderType);
		case glu::SHADERTYPE_GEOMETRY:					return new GeometryShaderExecutor	(shaderSpec, shaderType);
		case glu::SHADERTYPE_FRAGMENT:					return new FragmentShaderExecutor	(shaderSpec, shaderType);
		case glu::SHADERTYPE_COMPUTE:					return new ComputeShaderExecutor	(shaderSpec, shaderType);
		default:
			throw tcu::InternalError("Unsupported shader type");
	}
}

de::MovePtr<ShaderExecutor::BufferUniform> ShaderExecutor::createBufferUniform (const VkDevice&				vkDevice,
																				const DeviceInterface&		vk,
																				const VkQueue				/*queue*/,
																				const deUint32				queueFamilyIndex,
																				Allocator&					memAlloc,
																				deUint32					bindingLocation,
																				VkDescriptorType			descriptorType,
																				deUint32					size,
																				const void*					dataPtr)
{
	DE_ASSERT(descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	VkImageUsageFlags usage = descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	const VkBufferCreateInfo uniformBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		size,										// VkDeviceSize			size;
		usage,										// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer>					buffer				= createBuffer(vk, vkDevice, &uniformBufferParams);
	de::MovePtr<Allocation>			alloc				= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), alloc->getOffset()));

	deMemcpy(alloc->getHostPtr(), dataPtr, size);
	flushMappedMemoryRange(vk, vkDevice, alloc->getMemory(), alloc->getOffset(), size);

	de::MovePtr<BufferUniform> uniformInfo(new BufferUniform());
	uniformInfo->type = descriptorType;
	uniformInfo->descriptor = makeDescriptorBufferInfo(*buffer, 0u, size);
	uniformInfo->location = bindingLocation;
	uniformInfo->buffer = VkBufferSp(new Unique<VkBuffer>(buffer));
	uniformInfo->alloc = AllocationSp(alloc.release());

	return uniformInfo;
}

void ShaderExecutor::setupUniformData (const VkDevice&				vkDevice,
									   const DeviceInterface&		vk,
									   const VkQueue				queue,
									   const deUint32				queueFamilyIndex,
									   Allocator&					memAlloc,
									   deUint32						bindingLocation,
									   VkDescriptorType				descriptorType,
									   deUint32						size,
									   const void*					dataPtr)
{
	de::MovePtr<BufferUniform>			uniform			= createBufferUniform(vkDevice, vk, queue, queueFamilyIndex, memAlloc, bindingLocation, descriptorType, size, dataPtr);

	m_descriptorSetLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_ALL);
	m_descriptorPoolBuilder.addType(descriptorType);

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(uniform)));
}

void ShaderExecutor::setupUniformArray (const VkDevice&				vkDevice,
										const DeviceInterface&		vk,
										const VkQueue				queue,
										const deUint32				queueFamilyIndex,
										Allocator&					memAlloc,
										deUint32					bindingLocation,
										VkDescriptorType			descriptorType,
										deUint32					arraySize,
										deUint32					size,
										const void*					dataPtr)
{
	DE_ASSERT(arraySize > 0);

	de::MovePtr<BufferArrayUniform>		bufferArray		(new BufferArrayUniform());

	bufferArray->type = descriptorType;
	bufferArray->location = bindingLocation;

	for (deUint32 ndx = 0; ndx < arraySize; ++ndx)
	{
		const void*						bufferData		= ((deUint8*)dataPtr) + (ndx * size);
		de::MovePtr<BufferUniform>		uniform			= createBufferUniform(vkDevice, vk, queue, queueFamilyIndex, memAlloc, bindingLocation, descriptorType, size, bufferData);

		bufferArray->uniforms.push_back(BufferUniformSp(new de::UniquePtr<BufferUniform>(uniform)));
	}

	m_descriptorSetLayoutBuilder.addArrayBinding(descriptorType, arraySize, VK_SHADER_STAGE_ALL);
	m_descriptorPoolBuilder.addType(descriptorType, arraySize);

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(bufferArray)));
}

void ShaderExecutor::setupSamplerData (const VkDevice&				vkDevice,
									   const DeviceInterface&		vk,
									   const VkQueue				queue,
									   const deUint32				queueFamilyIndex,
									   Allocator&					memAlloc,
									   deUint32						bindingLocation,
									   deUint32						numSamplers,
									   const tcu::Sampler&			refSampler,
									   const tcu::TextureFormat&	texFormat,
									   const tcu::IVec3&			texSize,
									   VkImageType					imageType,
									   VkImageViewType				imageViewType,
									   const void*					data)
{
	DE_ASSERT(numSamplers > 0);

	de::MovePtr<SamplerArrayUniform>	samplers		(new SamplerArrayUniform());

	samplers->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplers->location = bindingLocation;

	for (deUint32 ndx = 0; ndx < numSamplers; ++ndx)
	{
		const int						offset			= ndx * texSize.x() * texSize.y() * texSize.z() * texFormat.getPixelSize();
		const void*						samplerData		= ((deUint8*)data) + offset;
		de::MovePtr<SamplerUniform>		uniform			= createSamplerUniform(vkDevice, vk, queue, queueFamilyIndex, memAlloc, bindingLocation, refSampler, texFormat, texSize, imageType, imageViewType, samplerData);

		samplers->uniforms.push_back(SamplerUniformSp(new de::UniquePtr<SamplerUniform>(uniform)));
	}

	m_descriptorSetLayoutBuilder.addArraySamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numSamplers, VK_SHADER_STAGE_ALL, DE_NULL);
	m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numSamplers);

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(samplers)));
}

const void*	ShaderExecutor::getBufferPtr (const deUint32 bindingLocation) const
{
	std::vector<UniformInfoSp>::const_iterator it = m_uniformInfos.begin();
	for (; it != m_uniformInfos.end(); it++)
	{
		const UniformInfo* uniformInfo = it->get()->get();
		if (uniformInfo->getType() == UniformInfo::UNIFORM_TYPE_BUFFER && uniformInfo->location == bindingLocation)
		{
			const BufferUniform* bufferUniform = static_cast<const BufferUniform*>(uniformInfo);
			return bufferUniform->alloc->getHostPtr();
		}
	}

	return DE_NULL;
}

void ShaderExecutor::addUniforms (const VkDevice& vkDevice, const DeviceInterface& vk, const VkQueue queue, const deUint32 queueFamilyIndex, Allocator& memAlloc)
{
	if (!m_uniformSetup)
		return;

	for (std::vector<UniformDataSp>::const_iterator it = m_uniformSetup->uniforms().begin(); it != m_uniformSetup->uniforms().end(); ++it)
	{
		const UniformDataBase* uniformData = it->get()->get();
		uniformData->setup(*this, vkDevice, vk, queue, queueFamilyIndex, memAlloc);
	}
}

void ShaderExecutor::uploadUniforms (DescriptorSetUpdateBuilder& descriptorSetUpdateBuilder, VkDescriptorSet descriptorSet)
{
	for (std::vector<UniformInfoSp>::const_iterator it = m_uniformInfos.begin(); it != m_uniformInfos.end(); ++it)
	{
		const UniformInfo*							uniformInfo		= it->get()->get();
		UniformInfo::UniformType					uniformType		= uniformInfo->getType();

		if (uniformType == UniformInfo::UNIFORM_TYPE_BUFFER_ARRAY)
		{
			const BufferArrayUniform*				arrayInfo		= static_cast<const BufferArrayUniform*>(uniformInfo);
			std::vector<VkDescriptorBufferInfo>		descriptors;

			for (std::vector<BufferUniformSp>::const_iterator ait = arrayInfo->uniforms.begin(); ait != arrayInfo->uniforms.end(); ++ait)
			{
				descriptors.push_back(ait->get()->get()->descriptor);
			}

			descriptorSetUpdateBuilder.writeArray(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(uniformInfo->location), uniformInfo->type, (deUint32)descriptors.size(), &descriptors[0]);
		}
		else if (uniformType == UniformInfo::UNIFORM_TYPE_SAMPLER_ARRAY)
		{
			const SamplerArrayUniform*				arrayInfo		= static_cast<const SamplerArrayUniform*>(uniformInfo);
			std::vector<VkDescriptorImageInfo>		descriptors;

			for (std::vector<SamplerUniformSp>::const_iterator ait = arrayInfo->uniforms.begin(); ait != arrayInfo->uniforms.end(); ++ait)
			{
				descriptors.push_back(ait->get()->get()->descriptor);
			}

			descriptorSetUpdateBuilder.writeArray(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(uniformInfo->location), uniformInfo->type, (deUint32)descriptors.size(), &descriptors[0]);
		}
		else if (uniformType == UniformInfo::UNIFORM_TYPE_BUFFER)
		{
			const BufferUniform*					bufferUniform	= static_cast<const BufferUniform*>(uniformInfo);
			descriptorSetUpdateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(bufferUniform->location), bufferUniform->type, &bufferUniform->descriptor);
		}
		else if (uniformType == UniformInfo::UNIFORM_TYPE_SAMPLER)
		{
			const SamplerUniform*					samplerUniform	= static_cast<const SamplerUniform*>(uniformInfo);
			descriptorSetUpdateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(samplerUniform->location), samplerUniform->type, &samplerUniform->descriptor);
		}
	}
}

void ShaderExecutor::uploadImage (const VkDevice&				vkDevice,
								  const DeviceInterface&		vk,
								  const VkQueue					queue,
								  const deUint32				queueFamilyIndex,
								  Allocator&					memAlloc,
								  const tcu::TextureFormat&		texFormat,
								  const tcu::IVec3&				texSize,
								  const void*					data,
								  const deUint32				arraySize,
								  const VkImageAspectFlags		aspectMask,
								  VkImage						destImage)
{
	deUint32						textureSize			= texSize.x() * texSize.y() * texSize.z() * texFormat.getPixelSize();
	deUint32						bufferSize;
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			cmdBuffer;
	Move<VkFence>					fence;
	std::vector<deUint32>			levelDataSizes;

	// Calculate buffer size
	bufferSize = arraySize * textureSize;

	// Create source buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, vkDevice, &bufferParams);
		bufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCommandPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32					queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);

		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};

		cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u											// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, vkDevice, &fenceParams);
	}

	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier preBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		bufferSize									// VkDeviceSize		size;
	};

	const VkImageMemoryBarrier preImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			aspectMask,								// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			1u,										// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			arraySize								// deUint32			arraySize;
		}
	};

	const VkImageMemoryBarrier postImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			aspectMask,								// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			1u,										// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			arraySize								// deUint32			arraySize;
		}
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	std::vector<VkBufferImageCopy>		copyRegions;

	{
		deUint32 layerDataOffset = 0;

		for (deUint32 layerNdx = 0; layerNdx < arraySize; ++layerNdx)
		{
			const VkBufferImageCopy layerRegion =
			{
				layerDataOffset,						// VkDeviceSize				bufferOffset;
				(deUint32)texSize.x(),					// deUint32					bufferRowLength;
				(deUint32)texSize.y(),					// deUint32					bufferImageHeight;
				{										// VkImageSubresourceLayers	imageSubresource;
					aspectMask,
					0u,
					(deUint32)layerNdx,
					1u
				},
				{ 0u, 0u, 0u },							// VkOffset3D			imageOffset;
				{										// VkExtent3D			imageExtent;
					(deUint32)texSize.x(),
					(deUint32)texSize.y(),
					(deUint32)texSize.z()
				}
			};

			copyRegions.push_back(layerRegion);
			layerDataOffset += textureSize;
		}
	}

	// Write buffer data
	{
		deUint8*	destPtr				= (deUint8*)bufferAlloc->getHostPtr();
		deUint32	levelOffset			= 0;

		for (deUint32 layerNdx = 0; layerNdx < arraySize; ++layerNdx)
		{
			tcu::ConstPixelBufferAccess		access		(texFormat, texSize, data);
			tcu::PixelBufferAccess			destAccess	(texFormat, texSize, destPtr + levelOffset);

			tcu::copy(destAccess, access);
			levelOffset += textureSize;
		}
	}

	flushMappedMemoryRange(vk, vkDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	// Copy buffer to image
	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, *buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), copyRegions.data());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity */));
}

de::MovePtr<ShaderExecutor::SamplerUniform> ShaderExecutor::createSamplerUniform (const VkDevice&				vkDevice,
																				  const DeviceInterface&		vk,
																				  const VkQueue					queue,
																				  const deUint32				queueFamilyIndex,
																				  Allocator&					memAlloc,
																				  deUint32						bindingLocation,
																				  const tcu::Sampler&			refSampler,
																				  const tcu::TextureFormat&		texFormat,
																				  const tcu::IVec3&				texSize,
																				  VkImageType					imageType,
																				  VkImageViewType				imageViewType,
																				  const void*					data)
{
	const VkFormat					format			= mapTextureFormat(texFormat);
	const bool						isCube			= imageViewType == VK_IMAGE_VIEW_TYPE_CUBE;
	const bool						isShadowSampler	= texFormat == tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
	const VkImageCreateFlags		imageFlags		= isCube ? (VkImageCreateFlags)VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlags)0;
	const deUint32					arraySize		= isCube ? 6u : 1u;
	const VkImageAspectFlags		aspectMask		= isShadowSampler ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageUsageFlags				imageUsage		= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	Move<VkImage>					vkTexture;
	de::MovePtr<Allocation>			allocation;

	if (isShadowSampler)
		imageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	// Create image
	const VkImageCreateInfo	imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		imageFlags,														// VkImageCreateFlags		flags;
		imageType,														// VkImageType				imageType;
		format,															// VkFormat					format;
		{																// VkExtent3D				extent;
			(deUint32)texSize.x(),
			(deUint32)texSize.y(),
			(deUint32)texSize.z()
		},
		1u,																// deUint32					mipLevels;
		arraySize,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
		imageUsage,														// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
		1u,																// deUint32					queueFamilyIndexCount;
		&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
	};

	vkTexture		= createImage(vk, vkDevice, &imageParams);
	allocation		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *vkTexture), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *vkTexture, allocation->getMemory(), allocation->getOffset()));

	// Upload texture data
	uploadImage(vkDevice, vk, queue, queueFamilyIndex, memAlloc, texFormat, texSize, data, arraySize, aspectMask, *vkTexture);

	// Create sampler
	const VkSamplerCreateInfo		samplerParams	= mapSampler(refSampler, texFormat);
	Move<VkSampler>					sampler			= createSampler(vk, vkDevice, &samplerParams);

	const VkImageViewCreateInfo		viewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		NULL,										// const voide*				pNexŧ;
		0u,											// VkImageViewCreateFlags	flags;
		*vkTexture,									// VkImage					image;
		imageViewType,								// VkImageViewType			viewType;
		format,										// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,						// VkComponentSwizzle		r;
			VK_COMPONENT_SWIZZLE_G,						// VkComponentSwizzle		g;
			VK_COMPONENT_SWIZZLE_B,						// VkComponentSwizzle		b;
			VK_COMPONENT_SWIZZLE_A						// VkComponentSwizzle		a;
		},											// VkComponentMapping			components;
		{
			aspectMask,									// VkImageAspectFlags	aspectMask;
			0,											// deUint32				baseMipLevel;
			1,											// deUint32				mipLevels;
			0,											// deUint32				baseArraySlice;
			arraySize									// deUint32				arraySize;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	Move<VkImageView>				imageView		= createImageView(vk, vkDevice, &viewParams);

	const VkDescriptorImageInfo descriptor			=
	{
		sampler.get(),								// VkSampler				sampler;
		imageView.get(),							// VkImageView				imageView;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout			imageLayout;
	};

	de::MovePtr<SamplerUniform> uniform(new SamplerUniform());
	uniform->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uniform->descriptor = descriptor;
	uniform->location = bindingLocation;
	uniform->image = VkImageSp(new Unique<VkImage>(vkTexture));
	uniform->imageView = VkImageViewSp(new Unique<VkImageView>(imageView));
	uniform->sampler = VkSamplerSp(new Unique<VkSampler>(sampler));
	uniform->alloc = AllocationSp(allocation.release());

	return uniform;
}

SamplerUniformData::SamplerUniformData (deUint32						bindingLocation,
										deUint32						numSamplers,
										const tcu::Sampler&				refSampler,
										const tcu::TextureFormat&		texFormat,
										const tcu::IVec3&				texSize,
										VkImageType						imageType,
										VkImageViewType					imageViewType,
										const void*						data)
	: UniformDataBase		(bindingLocation)
	, m_numSamplers			(numSamplers)
	, m_refSampler			(refSampler)
	, m_texFormat			(texFormat)
	, m_texSize				(texSize)
	, m_imageType			(imageType)
	, m_imageViewType		(imageViewType)
	, m_data				(data)
{
}

SamplerUniformData::~SamplerUniformData (void)
{
}

void SamplerUniformData::setup (ShaderExecutor& executor, const VkDevice& vkDevice, const DeviceInterface& vk, const VkQueue queue, const deUint32 queueFamilyIndex, Allocator& memAlloc) const
{
	executor.setupSamplerData(vkDevice, vk, queue, queueFamilyIndex, memAlloc, m_bindingLocation, m_numSamplers, m_refSampler, m_texFormat, m_texSize, m_imageType, m_imageViewType, m_data);
}

} // shaderexecutor
} // vkt
