/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Program interface utilities
 *//*--------------------------------------------------------------------*/

#include "es31fProgramInterfaceDefinitionUtil.hpp"
#include "es31fProgramInterfaceDefinition.hpp"
#include "gluVarType.hpp"
#include "gluVarTypeUtil.hpp"
#include "gluShaderUtil.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "glwEnums.hpp"

#include <set>
#include <map>
#include <sstream>
#include <vector>
#include <algorithm>

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace ProgramInterfaceDefinition
{

VariableSearchFilter VariableSearchFilter::intersection (const VariableSearchFilter& a, const VariableSearchFilter& b)
{
	const bool storageNonEmpty		= (a.m_storage == b.m_storage) || (a.m_storage == glu::STORAGE_LAST) || (b.m_storage == glu::STORAGE_LAST);
	const bool shaderTypeNonEmpty	= (a.m_shaderType == b.m_shaderType) || (a.m_shaderType == glu::SHADERTYPE_LAST) || (b.m_shaderType == glu::SHADERTYPE_LAST);

	return VariableSearchFilter((a.m_shaderType == glu::SHADERTYPE_LAST) ? (b.m_shaderType) : (a.m_shaderType),
								(a.m_storage == glu::STORAGE_LAST) ? (b.m_storage) : (a.m_storage),
								!storageNonEmpty || !shaderTypeNonEmpty || a.m_null || b.m_null);
}

} // ProgramInterfaceDefinition

static bool incrementMultiDimensionIndex (std::vector<int>& index, const std::vector<int>& dimensions)
{
	int incrementDimensionNdx = (int)(index.size() - 1);

	while (incrementDimensionNdx >= 0)
	{
		if (++index[incrementDimensionNdx] == dimensions[incrementDimensionNdx])
			index[incrementDimensionNdx--] = 0;
		else
			break;
	}

	return (incrementDimensionNdx != -1);
}

void generateVariableTypeResourceNames (std::vector<std::string>& resources, const std::string& name, const glu::VarType& type, deUint32 resourceNameGenerationFlags)
{
	DE_ASSERT((resourceNameGenerationFlags & (~RESOURCE_NAME_GENERATION_FLAG_MASK)) == 0);

	// remove top-level flag from children
	const deUint32 childFlags = resourceNameGenerationFlags & ~((deUint32)RESOURCE_NAME_GENERATION_FLAG_TOP_LEVEL_BUFFER_VARIABLE);

	if (type.isBasicType())
		resources.push_back(name);
	else if (type.isStructType())
	{
		const glu::StructType* structType = type.getStructPtr();
		for (int ndx = 0; ndx < structType->getNumMembers(); ++ndx)
			generateVariableTypeResourceNames(resources, name + "." + structType->getMember(ndx).getName(), structType->getMember(ndx).getType(), childFlags);
	}
	else if (type.isArrayType())
	{
		// Bottom-level arrays of basic types of a transform feedback variable will produce only the first
		// element but without the trailing "[0]"
		if (type.getElementType().isBasicType() &&
			(resourceNameGenerationFlags & RESOURCE_NAME_GENERATION_FLAG_TRANSFORM_FEEDBACK_VARIABLE) != 0)
		{
			resources.push_back(name);
		}
		// Bottom-level arrays of basic types and SSBO top-level arrays of any type procude only first element
		else if (type.getElementType().isBasicType() ||
				 (resourceNameGenerationFlags & RESOURCE_NAME_GENERATION_FLAG_TOP_LEVEL_BUFFER_VARIABLE) != 0)
		{
			generateVariableTypeResourceNames(resources, name + "[0]", type.getElementType(), childFlags);
		}
		// Other arrays of aggregate types are expanded
		else
		{
			for (int ndx = 0; ndx < type.getArraySize(); ++ndx)
				generateVariableTypeResourceNames(resources, name + "[" + de::toString(ndx) + "]", type.getElementType(), childFlags);
		}
	}
	else
		DE_ASSERT(false);
}

// Program source generation

namespace
{

using ProgramInterfaceDefinition::VariablePathComponent;
using ProgramInterfaceDefinition::VariableSearchFilter;

static const char* getShaderTypeDeclarations (glu::ShaderType type)
{
	switch (type)
	{
		case glu::SHADERTYPE_VERTEX:
			return	"";

		case glu::SHADERTYPE_FRAGMENT:
			return	"";

		case glu::SHADERTYPE_GEOMETRY:
			return	"layout(points) in;\n"
					"layout(points, max_vertices=3) out;\n";

		case glu::SHADERTYPE_TESSELLATION_CONTROL:
			return	"layout(vertices=1) out;\n";

		case glu::SHADERTYPE_TESSELLATION_EVALUATION:
			return	"layout(triangle, point_mode) in;\n";

		case glu::SHADERTYPE_COMPUTE:
			return	"layout(local_size_x=1) in;\n";

		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

class StructNameEqualPredicate
{
public:
				StructNameEqualPredicate	(const char* name) : m_name(name) { }
	bool		operator()					(const glu::StructType* type) { return type->hasTypeName() && (deStringEqual(m_name, type->getTypeName()) == DE_TRUE); }
private:
	const char*	m_name;
};

static void collectNamedStructureDefinitions (std::vector<const glu::StructType*>& dst, const glu::VarType& type)
{
	if (type.isBasicType())
		return;
	else if (type.isArrayType())
		return collectNamedStructureDefinitions(dst, type.getElementType());
	else if (type.isStructType())
	{
		if (type.getStructPtr()->hasTypeName())
		{
			// must be unique (may share the the same struct)
			std::vector<const glu::StructType*>::iterator where = std::find_if(dst.begin(), dst.end(), StructNameEqualPredicate(type.getStructPtr()->getTypeName()));
			if (where != dst.end())
			{
				DE_ASSERT(**where == *type.getStructPtr());

				// identical type has been added already, types of members must be added too
				return;
			}
		}

		// Add types of members first
		for (int ndx = 0; ndx < type.getStructPtr()->getNumMembers(); ++ndx)
			collectNamedStructureDefinitions(dst, type.getStructPtr()->getMember(ndx).getType());

		dst.push_back(type.getStructPtr());
	}
	else
		DE_ASSERT(false);
}

static void writeStructureDefinitions (std::ostringstream& buf, const ProgramInterfaceDefinition::DefaultBlock& defaultBlock)
{
	std::vector<const glu::StructType*> namedStructs;

	// Collect all structs in post order

	for (int ndx = 0; ndx < (int)defaultBlock.variables.size(); ++ndx)
		collectNamedStructureDefinitions(namedStructs, defaultBlock.variables[ndx].varType);

	for (int blockNdx = 0; blockNdx < (int)defaultBlock.interfaceBlocks.size(); ++blockNdx)
		for (int ndx = 0; ndx < (int)defaultBlock.interfaceBlocks[blockNdx].variables.size(); ++ndx)
			collectNamedStructureDefinitions(namedStructs, defaultBlock.interfaceBlocks[blockNdx].variables[ndx].varType);

	// Write

	for (int structNdx = 0; structNdx < (int)namedStructs.size(); ++structNdx)
	{
		buf <<	"struct " << namedStructs[structNdx]->getTypeName() << "\n"
				"{\n";

		for (int memberNdx = 0; memberNdx < namedStructs[structNdx]->getNumMembers(); ++memberNdx)
			buf << glu::indent(1) << glu::declare(namedStructs[structNdx]->getMember(memberNdx).getType(), namedStructs[structNdx]->getMember(memberNdx).getName(), 1) << ";\n";

		buf <<	"};\n";
	}

	if (!namedStructs.empty())
		buf << "\n";
}

static void writeInterfaceBlock (std::ostringstream& buf, const glu::InterfaceBlock& interfaceBlock)
{
	buf << interfaceBlock.layout;

	if (interfaceBlock.layout != glu::Layout())
		buf << " ";

	buf	<< glu::getStorageName(interfaceBlock.storage) << " " << interfaceBlock.interfaceName << "\n"
		<< "{\n";

	for (int ndx = 0; ndx < (int)interfaceBlock.variables.size(); ++ndx)
		buf << glu::indent(1) << interfaceBlock.variables[ndx] << ";\n";

	buf << "}";

	if (!interfaceBlock.instanceName.empty())
		buf << " " << interfaceBlock.instanceName;

	for (int dimensionNdx = 0; dimensionNdx < (int)interfaceBlock.dimensions.size(); ++dimensionNdx)
		buf << "[" << interfaceBlock.dimensions[dimensionNdx] << "]";

	buf << ";\n\n";
}

static void writeVariableReadAccumulateExpression (std::ostringstream& buf, const std::string& accumulatorName, const std::string& name, const glu::VarType& varType)
{
	if (varType.isBasicType())
	{
		buf << "\t" << accumulatorName << " += ";

		if (glu::isDataTypeScalar(varType.getBasicType()))
			buf << "vec4(float(" << name << "))";
		else if (glu::isDataTypeVector(varType.getBasicType()))
			buf << "vec4(" << name << ".xyxy)";
		else if (glu::isDataTypeMatrix(varType.getBasicType()))
			buf << "vec4(float(" << name << "[0][0]))";
		else if (glu::isDataTypeSamplerMultisample(varType.getBasicType()))
			buf << "vec4(float(textureSize(" << name << ").x))";
		else if (glu::isDataTypeSampler(varType.getBasicType()))
			buf << "vec4(float(textureSize(" << name << ", 0).x))";
		else if (glu::isDataTypeImage(varType.getBasicType()))
			buf << "vec4(float(imageSize(" << name << ").x))";
		else if (varType.getBasicType() == glu::TYPE_UINT_ATOMIC_COUNTER)
			buf << "vec4(float(atomicCounterIncrement(" << name << ")))";
		else
			DE_ASSERT(false);

		buf << ";\n";
	}
	else if (varType.isStructType())
	{
		for (int ndx = 0; ndx < varType.getStructPtr()->getNumMembers(); ++ndx)
			writeVariableReadAccumulateExpression(buf, accumulatorName, name + "." + varType.getStructPtr()->getMember(ndx).getName(), varType.getStructPtr()->getMember(ndx).getType());
	}
	else if (varType.isArrayType())
	{
		if (varType.getArraySize() != glu::VarType::UNSIZED_ARRAY)
			for (int ndx = 0; ndx < varType.getArraySize(); ++ndx)
				writeVariableReadAccumulateExpression(buf, accumulatorName, name + "[" + de::toString(ndx) + "]", varType.getElementType());
		else
			writeVariableReadAccumulateExpression(buf, accumulatorName, name + "[8]", varType.getElementType());
	}
	else
		DE_ASSERT(false);
}

static void writeInterfaceReadAccumulateExpression (std::ostringstream& buf, const std::string& accumulatorName, const glu::InterfaceBlock& block)
{
	if (block.dimensions.empty())
	{
		const std::string prefix = (block.instanceName.empty()) ? ("") : (block.instanceName + ".");

		for (int ndx = 0; ndx < (int)block.variables.size(); ++ndx)
			writeVariableReadAccumulateExpression(buf, accumulatorName, prefix + block.variables[ndx].name, block.variables[ndx].varType);
	}
	else
	{
		std::vector<int> index(block.dimensions.size(), 0);

		for (;;)
		{
			// access element
			{
				std::ostringstream name;
				name << block.instanceName;

				for (int dimensionNdx = 0; dimensionNdx < (int)block.dimensions.size(); ++dimensionNdx)
					name << "[" << index[dimensionNdx] << "]";

				for (int ndx = 0; ndx < (int)block.variables.size(); ++ndx)
					writeVariableReadAccumulateExpression(buf, accumulatorName, name.str() + "." + block.variables[ndx].name, block.variables[ndx].varType);
			}

			// increment index
			if (!incrementMultiDimensionIndex(index, block.dimensions))
				break;
		}
	}
}

static void writeVariableWriteExpression (std::ostringstream& buf, const std::string& sourceVec4Name, const std::string& name, const glu::VarType& varType)
{
	if (varType.isBasicType())
	{
		buf << "\t" << name << " = ";

		if (glu::isDataTypeScalar(varType.getBasicType()))
			buf << glu::getDataTypeName(varType.getBasicType()) << "(" << sourceVec4Name << ".y)";
		else if (glu::isDataTypeVector(varType.getBasicType()) || glu::isDataTypeMatrix(varType.getBasicType()))
			buf << glu::getDataTypeName(varType.getBasicType()) << "(" << glu::getDataTypeName(glu::getDataTypeScalarType(varType.getBasicType())) << "(" << sourceVec4Name << ".y))";
		else
			DE_ASSERT(false);

		buf << ";\n";
	}
	else if (varType.isStructType())
	{
		for (int ndx = 0; ndx < varType.getStructPtr()->getNumMembers(); ++ndx)
			writeVariableWriteExpression(buf, sourceVec4Name, name + "." + varType.getStructPtr()->getMember(ndx).getName(), varType.getStructPtr()->getMember(ndx).getType());
	}
	else if (varType.isArrayType())
	{
		if (varType.getArraySize() != glu::VarType::UNSIZED_ARRAY)
			for (int ndx = 0; ndx < varType.getArraySize(); ++ndx)
				writeVariableWriteExpression(buf, sourceVec4Name, name + "[" + de::toString(ndx) + "]", varType.getElementType());
		else
			writeVariableWriteExpression(buf, sourceVec4Name, name + "[9]", varType.getElementType());
	}
	else
		DE_ASSERT(false);
}

static void writeInterfaceWriteExpression (std::ostringstream& buf, const std::string& sourceVec4Name, const glu::InterfaceBlock& block)
{
	if (block.dimensions.empty())
	{
		const std::string prefix = (block.instanceName.empty()) ? ("") : (block.instanceName + ".");

		for (int ndx = 0; ndx < (int)block.variables.size(); ++ndx)
			writeVariableWriteExpression(buf, sourceVec4Name, prefix + block.variables[ndx].name, block.variables[ndx].varType);
	}
	else
	{
		std::vector<int> index(block.dimensions.size(), 0);

		for (;;)
		{
			// access element
			{
				std::ostringstream name;
				name << block.instanceName;

				for (int dimensionNdx = 0; dimensionNdx < (int)block.dimensions.size(); ++dimensionNdx)
					name << "[" << index[dimensionNdx] << "]";

				for (int ndx = 0; ndx < (int)block.variables.size(); ++ndx)
					writeVariableWriteExpression(buf, sourceVec4Name, name.str() + "." + block.variables[ndx].name, block.variables[ndx].varType);
			}

			// increment index
			if (!incrementMultiDimensionIndex(index, block.dimensions))
				break;
		}
	}
}

static bool traverseVariablePath (std::vector<VariablePathComponent>& typePath, const char* subPath, const glu::VarType& type)
{
	glu::VarTokenizer tokenizer(subPath);

	typePath.push_back(VariablePathComponent(&type));

	if (tokenizer.getToken() == glu::VarTokenizer::TOKEN_END)
		return true;

	if (type.isStructType() && tokenizer.getToken() == glu::VarTokenizer::TOKEN_PERIOD)
	{
		tokenizer.advance();

		// malformed path
		if (tokenizer.getToken() != glu::VarTokenizer::TOKEN_IDENTIFIER)
			return false;

		for (int memberNdx = 0; memberNdx < type.getStructPtr()->getNumMembers(); ++memberNdx)
			if (type.getStructPtr()->getMember(memberNdx).getName() == tokenizer.getIdentifier())
				return traverseVariablePath(typePath, subPath + tokenizer.getCurrentTokenEndLocation(), type.getStructPtr()->getMember(memberNdx).getType());

		// malformed path, no such member
		return false;
	}
	else if (type.isArrayType() && tokenizer.getToken() == glu::VarTokenizer::TOKEN_LEFT_BRACKET)
	{
		tokenizer.advance();

		// malformed path
		if (tokenizer.getToken() != glu::VarTokenizer::TOKEN_NUMBER)
			return false;

		tokenizer.advance();
		if (tokenizer.getToken() != glu::VarTokenizer::TOKEN_RIGHT_BRACKET)
			return false;

		return traverseVariablePath(typePath, subPath + tokenizer.getCurrentTokenEndLocation(), type.getElementType());
	}

	return false;
}

static bool traverseVariablePath (std::vector<VariablePathComponent>& typePath, const std::string& path, const glu::VariableDeclaration& var)
{
	if (glu::parseVariableName(path.c_str()) != var.name)
		return false;

	typePath.push_back(VariablePathComponent(&var));
	return traverseVariablePath(typePath, path.c_str() + var.name.length(), var.varType);
}

static bool traverseShaderVariablePath (std::vector<VariablePathComponent>& typePath, const ProgramInterfaceDefinition::Shader* shader, const std::string& path, const VariableSearchFilter& filter)
{
	// Default block variable?
	for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().variables.size(); ++varNdx)
		if (filter.matchesFilter(shader->getDefaultBlock().variables[varNdx]))
			if (traverseVariablePath(typePath, path, shader->getDefaultBlock().variables[varNdx]))
				return true;

	// is variable an interface block variable?
	{
		const std::string blockName = glu::parseVariableName(path.c_str());

		for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
		{
			if (!filter.matchesFilter(shader->getDefaultBlock().interfaceBlocks[interfaceNdx]))
				continue;

			if (shader->getDefaultBlock().interfaceBlocks[interfaceNdx].interfaceName == blockName)
			{
				// resource is a member of a named interface block
				// \note there is no array index specifier even if the interface is declared as an array of instances
				const std::string blockMemberPath = path.substr(blockName.size() + 1);
				const std::string blockMemeberName = glu::parseVariableName(blockMemberPath.c_str());

				for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables.size(); ++varNdx)
				{
					if (shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables[varNdx].name == blockMemeberName)
					{
						typePath.push_back(VariablePathComponent(&shader->getDefaultBlock().interfaceBlocks[interfaceNdx]));
						return traverseVariablePath(typePath, blockMemberPath, shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables[varNdx]);
					}
				}

				// terminate search
				return false;
			}
			else if (shader->getDefaultBlock().interfaceBlocks[interfaceNdx].instanceName.empty())
			{
				const std::string blockMemeberName = glu::parseVariableName(path.c_str());

				// unnamed block contains such variable?
				for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables.size(); ++varNdx)
				{
					if (shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables[varNdx].name == blockMemeberName)
					{
						typePath.push_back(VariablePathComponent(&shader->getDefaultBlock().interfaceBlocks[interfaceNdx]));
						return traverseVariablePath(typePath, path, shader->getDefaultBlock().interfaceBlocks[interfaceNdx].variables[varNdx]);
					}
				}

				// continue search
			}
		}
	}

	return false;
}

static bool traverseProgramVariablePath (std::vector<VariablePathComponent>& typePath, const ProgramInterfaceDefinition::Program* program, const std::string& path, const VariableSearchFilter& filter)
{
	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
	{
		const ProgramInterfaceDefinition::Shader* shader = program->getShaders()[shaderNdx];

		if (filter.matchesFilter(shader))
		{
			// \note modifying output variable even when returning false
			typePath.clear();
			if (traverseShaderVariablePath(typePath, shader, path, filter))
				return true;
		}
	}

	return false;
}

static bool containsSubType (const glu::VarType& complexType, glu::DataType basicType)
{
	if (complexType.isBasicType())
	{
		return complexType.getBasicType() == basicType;
	}
	else if (complexType.isArrayType())
	{
		return containsSubType(complexType.getElementType(), basicType);
	}
	else if (complexType.isStructType())
	{
		for (int ndx = 0; ndx < complexType.getStructPtr()->getNumMembers(); ++ndx)
			if (containsSubType(complexType.getStructPtr()->getMember(ndx).getType(), basicType))
				return true;
		return false;
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

static int getNumShaderBlocks (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	int retVal = 0;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
	{
		if (shader->getDefaultBlock().interfaceBlocks[ndx].storage == storage)
		{
			int numInstances = 1;

			for (int dimensionNdx = 0; dimensionNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].dimensions.size(); ++dimensionNdx)
				numInstances *= shader->getDefaultBlock().interfaceBlocks[ndx].dimensions[dimensionNdx];

			retVal += numInstances;
		}
	}

	return retVal;
}

static int getNumAtomicCounterBuffers (const ProgramInterfaceDefinition::Shader* shader)
{
	std::set<int> buffers;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
	{
		if (containsSubType(shader->getDefaultBlock().variables[ndx].varType, glu::TYPE_UINT_ATOMIC_COUNTER))
		{
			DE_ASSERT(shader->getDefaultBlock().variables[ndx].layout.binding != -1);
			buffers.insert(shader->getDefaultBlock().variables[ndx].layout.binding);
		}
	}

	return (int)buffers.size();
}

template <bool B>
static bool dummyConstantTypeFilter (glu::DataType d)
{
	DE_UNREF(d);
	return B;
}

static int getNumTypeInstances (const glu::VarType& complexType, bool (*predicate)(glu::DataType))
{
	if (complexType.isBasicType())
	{
		if (predicate(complexType.getBasicType()))
			return 1;
		else
			return 0;
	}
	else if (complexType.isArrayType())
	{
		const int arraySize = (complexType.getArraySize() == glu::VarType::UNSIZED_ARRAY) ? (1) : (complexType.getArraySize());
		return arraySize * getNumTypeInstances(complexType.getElementType(), predicate);
	}
	else if (complexType.isStructType())
	{
		int sum = 0;
		for (int ndx = 0; ndx < complexType.getStructPtr()->getNumMembers(); ++ndx)
			sum += getNumTypeInstances(complexType.getStructPtr()->getMember(ndx).getType(), predicate);
		return sum;
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

static int getMappedBasicTypeSum (const glu::VarType& complexType, int (*typeMap)(glu::DataType))
{
	if (complexType.isBasicType())
		return typeMap(complexType.getBasicType());
	else if (complexType.isArrayType())
	{
		const int arraySize = (complexType.getArraySize() == glu::VarType::UNSIZED_ARRAY) ? (1) : (complexType.getArraySize());
		return arraySize * getMappedBasicTypeSum(complexType.getElementType(), typeMap);
	}
	else if (complexType.isStructType())
	{
		int sum = 0;
		for (int ndx = 0; ndx < complexType.getStructPtr()->getNumMembers(); ++ndx)
			sum += getMappedBasicTypeSum(complexType.getStructPtr()->getMember(ndx).getType(), typeMap);
		return sum;
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

static int getNumTypeInstances (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage, bool (*predicate)(glu::DataType))
{
	int retVal = 0;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
	{
		if (shader->getDefaultBlock().interfaceBlocks[ndx].storage == storage)
		{
			int numInstances = 1;

			for (int dimensionNdx = 0; dimensionNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].dimensions.size(); ++dimensionNdx)
				numInstances *= shader->getDefaultBlock().interfaceBlocks[ndx].dimensions[dimensionNdx];

			for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].variables.size(); ++varNdx)
				retVal += numInstances * getNumTypeInstances(shader->getDefaultBlock().interfaceBlocks[ndx].variables[varNdx].varType, predicate);
		}
	}

	for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().variables.size(); ++varNdx)
		if (shader->getDefaultBlock().variables[varNdx].storage == storage)
			retVal += getNumTypeInstances(shader->getDefaultBlock().variables[varNdx].varType, predicate);

	return retVal;
}

static int getMappedBasicTypeSum (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage, int (*typeMap)(glu::DataType))
{
	int retVal = 0;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
	{
		if (shader->getDefaultBlock().interfaceBlocks[ndx].storage == storage)
		{
			int numInstances = 1;

			for (int dimensionNdx = 0; dimensionNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].dimensions.size(); ++dimensionNdx)
				numInstances *= shader->getDefaultBlock().interfaceBlocks[ndx].dimensions[dimensionNdx];

			for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].variables.size(); ++varNdx)
				retVal += numInstances * getMappedBasicTypeSum(shader->getDefaultBlock().interfaceBlocks[ndx].variables[varNdx].varType, typeMap);
		}
	}

	for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().variables.size(); ++varNdx)
		if (shader->getDefaultBlock().variables[varNdx].storage == storage)
			retVal += getMappedBasicTypeSum(shader->getDefaultBlock().variables[varNdx].varType, typeMap);

	return retVal;
}

static int getNumDataTypeComponents (glu::DataType type)
{
	if (glu::isDataTypeScalarOrVector(type) || glu::isDataTypeMatrix(type))
		return glu::getDataTypeScalarSize(type);
	else
		return 0;
}

static int getNumDataTypeVectors (glu::DataType type)
{
	if (glu::isDataTypeScalar(type))
		return 1;
	else if (glu::isDataTypeVector(type))
		return 1;
	else if (glu::isDataTypeMatrix(type))
		return glu::getDataTypeMatrixNumColumns(type);
	else
		return 0;
}

static int getNumComponents (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	return getMappedBasicTypeSum(shader, storage, getNumDataTypeComponents);
}

static int getNumVectors (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	return getMappedBasicTypeSum(shader, storage, getNumDataTypeVectors);
}

static int getNumDefaultBlockComponents (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	int retVal = 0;

	for (int varNdx = 0; varNdx < (int)shader->getDefaultBlock().variables.size(); ++varNdx)
		if (shader->getDefaultBlock().variables[varNdx].storage == storage)
			retVal += getMappedBasicTypeSum(shader->getDefaultBlock().variables[varNdx].varType, getNumDataTypeComponents);

	return retVal;
}

static int getMaxBufferBinding (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	int maxBinding = -1;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
	{
		if (shader->getDefaultBlock().interfaceBlocks[ndx].storage == storage)
		{
			const int	binding			= (shader->getDefaultBlock().interfaceBlocks[ndx].layout.binding == -1) ? (0) : (shader->getDefaultBlock().interfaceBlocks[ndx].layout.binding);
			int			numInstances	= 1;

			for (int dimensionNdx = 0; dimensionNdx < (int)shader->getDefaultBlock().interfaceBlocks[ndx].dimensions.size(); ++dimensionNdx)
				numInstances *= shader->getDefaultBlock().interfaceBlocks[ndx].dimensions[dimensionNdx];

			maxBinding = de::max(maxBinding, binding + numInstances - 1);
		}
	}

	return (int)maxBinding;
}

static int getBufferTypeSize (glu::DataType type, glu::MatrixOrder order)
{
	// assume vec4 alignments, should produce values greater than or equal to the actual resource usage
	int numVectors = 0;

	if (glu::isDataTypeScalarOrVector(type))
		numVectors = 1;
	else if (glu::isDataTypeMatrix(type) && order == glu::MATRIXORDER_ROW_MAJOR)
		numVectors = glu::getDataTypeMatrixNumRows(type);
	else if (glu::isDataTypeMatrix(type) && order != glu::MATRIXORDER_ROW_MAJOR)
		numVectors = glu::getDataTypeMatrixNumColumns(type);
	else
		DE_ASSERT(false);

	return 4 * numVectors;
}

static int getBufferVariableSize (const glu::VarType& type, glu::MatrixOrder order)
{
	if (type.isBasicType())
		return getBufferTypeSize(type.getBasicType(), order);
	else if (type.isArrayType())
	{
		const int arraySize = (type.getArraySize() == glu::VarType::UNSIZED_ARRAY) ? (1) : (type.getArraySize());
		return arraySize * getBufferVariableSize(type.getElementType(), order);
	}
	else if (type.isStructType())
	{
		int sum = 0;
		for (int ndx = 0; ndx < type.getStructPtr()->getNumMembers(); ++ndx)
			sum += getBufferVariableSize(type.getStructPtr()->getMember(ndx).getType(), order);
		return sum;
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

static int getBufferSize (const glu::InterfaceBlock& block, glu::MatrixOrder blockOrder)
{
	int size = 0;

	for (int ndx = 0; ndx < (int)block.variables.size(); ++ndx)
		size += getBufferVariableSize(block.variables[ndx].varType, (block.variables[ndx].layout.matrixOrder == glu::MATRIXORDER_LAST) ? (blockOrder) : (block.variables[ndx].layout.matrixOrder));

	return size;
}

static int getBufferMaxSize (const ProgramInterfaceDefinition::Shader* shader, glu::Storage storage)
{
	int maxSize = 0;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
		if (shader->getDefaultBlock().interfaceBlocks[ndx].storage == storage)
			maxSize = de::max(maxSize, getBufferSize(shader->getDefaultBlock().interfaceBlocks[ndx], shader->getDefaultBlock().interfaceBlocks[ndx].layout.matrixOrder));

	return (int)maxSize;
}

static int getAtomicCounterMaxBinding (const ProgramInterfaceDefinition::Shader* shader)
{
	int maxBinding = -1;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
	{
		if (containsSubType(shader->getDefaultBlock().variables[ndx].varType, glu::TYPE_UINT_ATOMIC_COUNTER))
		{
			DE_ASSERT(shader->getDefaultBlock().variables[ndx].layout.binding != -1);
			maxBinding = de::max(maxBinding, shader->getDefaultBlock().variables[ndx].layout.binding);
		}
	}

	return (int)maxBinding;
}

static int getUniformMaxBinding (const ProgramInterfaceDefinition::Shader* shader, bool (*predicate)(glu::DataType))
{
	int maxBinding = -1;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
		maxBinding = de::max(maxBinding, shader->getDefaultBlock().variables[ndx].layout.binding + getNumTypeInstances(shader->getDefaultBlock().variables[ndx].varType, predicate));

	return (int)maxBinding;
}

static int getAtomicCounterMaxBufferSize (const ProgramInterfaceDefinition::Shader* shader)
{
	std::map<int, int>	bufferSizes;
	int					maxSize			= 0;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
	{
		if (containsSubType(shader->getDefaultBlock().variables[ndx].varType, glu::TYPE_UINT_ATOMIC_COUNTER))
		{
			const int bufferBinding	= shader->getDefaultBlock().variables[ndx].layout.binding;
			const int offset		= (shader->getDefaultBlock().variables[ndx].layout.offset == -1) ? (0) : (shader->getDefaultBlock().variables[ndx].layout.offset);
			const int size			= offset + 4 * getNumTypeInstances(shader->getDefaultBlock().variables[ndx].varType, glu::isDataTypeAtomicCounter);

			DE_ASSERT(shader->getDefaultBlock().variables[ndx].layout.binding != -1);

			if (bufferSizes.find(bufferBinding) == bufferSizes.end())
				bufferSizes[bufferBinding] = size;
			else
				bufferSizes[bufferBinding] = de::max<int>(bufferSizes[bufferBinding], size);
		}
	}

	for (std::map<int, int>::iterator it = bufferSizes.begin(); it != bufferSizes.end(); ++it)
		maxSize = de::max<int>(maxSize, it->second);

	return maxSize;
}

static int getNumFeedbackVaryingComponents (const ProgramInterfaceDefinition::Program* program, const std::string& name)
{
	std::vector<VariablePathComponent> path;

	if (name == "gl_Position")
		return 4;

	DE_ASSERT(deStringBeginsWith(name.c_str(), "gl_") == DE_FALSE);

	if (!traverseProgramVariablePath(path, program, name, VariableSearchFilter(glu::SHADERTYPE_VERTEX, glu::STORAGE_OUT)))
		DE_ASSERT(false); // Program failed validate, invalid operation

	return getMappedBasicTypeSum(*path.back().getVariableType(), getNumDataTypeComponents);
}

static int getNumXFBComponents (const ProgramInterfaceDefinition::Program* program)
{
	int numComponents = 0;

	for (int ndx = 0; ndx < (int)program->getTransformFeedbackVaryings().size(); ++ndx)
		numComponents += getNumFeedbackVaryingComponents(program, program->getTransformFeedbackVaryings()[ndx]);

	return numComponents;
}

static int getNumMaxXFBOutputComponents (const ProgramInterfaceDefinition::Program* program)
{
	int numComponents = 0;

	for (int ndx = 0; ndx < (int)program->getTransformFeedbackVaryings().size(); ++ndx)
		numComponents = de::max(numComponents, getNumFeedbackVaryingComponents(program, program->getTransformFeedbackVaryings()[ndx]));

	return numComponents;
}

static int getFragmentOutputMaxLocation (const ProgramInterfaceDefinition::Shader* shader)
{
	DE_ASSERT(shader->getType() == glu::SHADERTYPE_FRAGMENT);

	int maxOutputLocation = -1;

	for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
	{
		if (shader->getDefaultBlock().variables[ndx].storage == glu::STORAGE_OUT)
		{
			// missing location qualifier means location == 0
			const int outputLocation 		= (shader->getDefaultBlock().variables[ndx].layout.location == -1)
												? (0)
												: (shader->getDefaultBlock().variables[ndx].layout.location);

			// only basic types or arrays of basic types possible
			DE_ASSERT(!shader->getDefaultBlock().variables[ndx].varType.isStructType());

			const int locationSlotsTaken	= (shader->getDefaultBlock().variables[ndx].varType.isArrayType())
												? (shader->getDefaultBlock().variables[ndx].varType.getArraySize())
												: (1);

			maxOutputLocation = de::max(maxOutputLocation, outputLocation + locationSlotsTaken - 1);
		}
	}

	return maxOutputLocation;
}

} // anonymous

std::vector<std::string> getProgramInterfaceBlockMemberResourceList (const glu::InterfaceBlock& interfaceBlock)
{
	const std::string			namePrefix					= (!interfaceBlock.instanceName.empty()) ? (interfaceBlock.interfaceName + ".") : ("");
	const bool					isTopLevelBufferVariable	= (interfaceBlock.storage == glu::STORAGE_BUFFER);
	std::vector<std::string>	resources;

	for (int variableNdx = 0; variableNdx < (int)interfaceBlock.variables.size(); ++variableNdx)
		generateVariableTypeResourceNames(resources,
										  namePrefix + interfaceBlock.variables[variableNdx].name,
										  interfaceBlock.variables[variableNdx].varType,
										  (isTopLevelBufferVariable) ?
											(RESOURCE_NAME_GENERATION_FLAG_TOP_LEVEL_BUFFER_VARIABLE) :
											(RESOURCE_NAME_GENERATION_FLAG_DEFAULT));

	return resources;
}

std::vector<std::string> getProgramInterfaceResourceList (const ProgramInterfaceDefinition::Program* program, ProgramInterface interface)
{
	// The same {uniform (block), buffer (variable)} can exist in multiple shaders, remove duplicates but keep order
	const bool					removeDuplicated	= (interface == PROGRAMINTERFACE_UNIFORM)			||
													  (interface == PROGRAMINTERFACE_UNIFORM_BLOCK)		||
													  (interface == PROGRAMINTERFACE_BUFFER_VARIABLE)	||
													  (interface == PROGRAMINTERFACE_SHADER_STORAGE_BLOCK);
	std::vector<std::string>	resources;

	switch (interface)
	{
		case PROGRAMINTERFACE_UNIFORM:
		case PROGRAMINTERFACE_BUFFER_VARIABLE:
		{
			const glu::Storage storage = (interface == PROGRAMINTERFACE_UNIFORM) ? (glu::STORAGE_UNIFORM) : (glu::STORAGE_BUFFER);

			for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
			{
				const ProgramInterfaceDefinition::Shader* shader = program->getShaders()[shaderNdx];

				for (int variableNdx = 0; variableNdx < (int)shader->getDefaultBlock().variables.size(); ++variableNdx)
					if (shader->getDefaultBlock().variables[variableNdx].storage == storage)
						generateVariableTypeResourceNames(resources,
														  shader->getDefaultBlock().variables[variableNdx].name,
														  shader->getDefaultBlock().variables[variableNdx].varType,
														  RESOURCE_NAME_GENERATION_FLAG_DEFAULT);

				for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
				{
					const glu::InterfaceBlock& interfaceBlock = shader->getDefaultBlock().interfaceBlocks[interfaceNdx];
					if (interfaceBlock.storage == storage)
					{
						const std::vector<std::string> blockResources = getProgramInterfaceBlockMemberResourceList(interfaceBlock);
						resources.insert(resources.end(), blockResources.begin(), blockResources.end());
					}
				}
			}
			break;
		}

		case PROGRAMINTERFACE_UNIFORM_BLOCK:
		case PROGRAMINTERFACE_SHADER_STORAGE_BLOCK:
		{
			const glu::Storage storage = (interface == PROGRAMINTERFACE_UNIFORM_BLOCK) ? (glu::STORAGE_UNIFORM) : (glu::STORAGE_BUFFER);

			for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
			{
				const ProgramInterfaceDefinition::Shader* shader = program->getShaders()[shaderNdx];
				for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
				{
					const glu::InterfaceBlock& interfaceBlock = shader->getDefaultBlock().interfaceBlocks[interfaceNdx];
					if (interfaceBlock.storage == storage)
					{
						std::vector<int> index(interfaceBlock.dimensions.size(), 0);

						for (;;)
						{
							// add resource string for each element
							{
								std::ostringstream name;
								name << interfaceBlock.interfaceName;

								for (int dimensionNdx = 0; dimensionNdx < (int)interfaceBlock.dimensions.size(); ++dimensionNdx)
									name << "[" << index[dimensionNdx] << "]";

								resources.push_back(name.str());
							}

							// increment index
							if (!incrementMultiDimensionIndex(index, interfaceBlock.dimensions))
								break;
						}
					}
				}
			}
			break;
		}

		case PROGRAMINTERFACE_PROGRAM_INPUT:
		case PROGRAMINTERFACE_PROGRAM_OUTPUT:
		{
			const glu::Storage		storage		= (interface == PROGRAMINTERFACE_PROGRAM_INPUT) ? (glu::STORAGE_IN) : (glu::STORAGE_OUT);
			const glu::ShaderType	shaderType	= (interface == PROGRAMINTERFACE_PROGRAM_INPUT) ? (program->getFirstStage()) : (program->getLastStage());

			for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
			{
				const ProgramInterfaceDefinition::Shader* shader = program->getShaders()[shaderNdx];

				if (shader->getType() != shaderType)
					continue;

				for (int variableNdx = 0; variableNdx < (int)shader->getDefaultBlock().variables.size(); ++variableNdx)
					if (shader->getDefaultBlock().variables[variableNdx].storage == storage)
						generateVariableTypeResourceNames(resources,
														  shader->getDefaultBlock().variables[variableNdx].name,
														  shader->getDefaultBlock().variables[variableNdx].varType,
														  RESOURCE_NAME_GENERATION_FLAG_DEFAULT);

				for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
				{
					const glu::InterfaceBlock& interfaceBlock = shader->getDefaultBlock().interfaceBlocks[interfaceNdx];
					if (interfaceBlock.storage == storage)
					{
						const std::vector<std::string> blockResources = getProgramInterfaceBlockMemberResourceList(interfaceBlock);
						resources.insert(resources.end(), blockResources.begin(), blockResources.end());
					}
				}
			}

			// built-ins
			if (interface == PROGRAMINTERFACE_PROGRAM_INPUT)
			{
				if (shaderType == glu::SHADERTYPE_VERTEX && resources.empty())
					resources.push_back("gl_VertexID"); // only read from when there are no other inputs
				else if (shaderType == glu::SHADERTYPE_FRAGMENT && resources.empty())
					resources.push_back("gl_FragCoord"); // only read from when there are no other inputs
				else if (shaderType == glu::SHADERTYPE_GEOMETRY && resources.empty())
					resources.push_back("gl_in[0].gl_Position");
				else if (shaderType == glu::SHADERTYPE_TESSELLATION_CONTROL)
				{
					const bool noInputs = resources.empty();
					resources.push_back("gl_InvocationID");

					if (noInputs)
						resources.push_back("gl_in[0].gl_Position"); // only read from when there are no other inputs
				}
				else if (shaderType == glu::SHADERTYPE_TESSELLATION_EVALUATION && resources.empty())
					resources.push_back("gl_in[0].gl_Position"); // only read from when there are no other inputs
				else if (shaderType == glu::SHADERTYPE_COMPUTE && resources.empty())
					resources.push_back("gl_NumWorkGroups"); // only read from when there are no other inputs
			}
			else if (interface == PROGRAMINTERFACE_PROGRAM_OUTPUT)
			{
				if (shaderType == glu::SHADERTYPE_VERTEX)
					resources.push_back("gl_Position");
				else if (shaderType == glu::SHADERTYPE_FRAGMENT && resources.empty())
					resources.push_back("gl_FragDepth"); // only written to when there are no other outputs
				else if (shaderType == glu::SHADERTYPE_GEOMETRY)
					resources.push_back("gl_Position");
				else if (shaderType == glu::SHADERTYPE_TESSELLATION_CONTROL)
					resources.push_back("gl_out[0].gl_Position");
				else if (shaderType == glu::SHADERTYPE_TESSELLATION_EVALUATION)
					resources.push_back("gl_Position");
			}

			break;
		}

		case PROGRAMINTERFACE_TRANSFORM_FEEDBACK_VARYING:
		{
			for (int varyingNdx = 0; varyingNdx < (int)program->getTransformFeedbackVaryings().size(); ++varyingNdx)
			{
				const std::string& varyingName = program->getTransformFeedbackVaryings()[varyingNdx];

				if (deStringBeginsWith(varyingName.c_str(), "gl_"))
					resources.push_back(varyingName); // builtin
				else
				{
					std::vector<VariablePathComponent> path;

					if (!traverseProgramVariablePath(path, program, varyingName, VariableSearchFilter(glu::SHADERTYPE_VERTEX, glu::STORAGE_OUT)))
						DE_ASSERT(false); // Program failed validate, invalid operation

					generateVariableTypeResourceNames(resources,
													  varyingName,
													  *path.back().getVariableType(),
													  RESOURCE_NAME_GENERATION_FLAG_TRANSFORM_FEEDBACK_VARIABLE);
				}
			}

			break;
		}

		default:
			DE_ASSERT(false);
	}

	if (removeDuplicated)
	{
		std::set<std::string>		addedVariables;
		std::vector<std::string>	uniqueResouces;

		for (int ndx = 0; ndx < (int)resources.size(); ++ndx)
		{
			if (addedVariables.find(resources[ndx]) == addedVariables.end())
			{
				addedVariables.insert(resources[ndx]);
				uniqueResouces.push_back(resources[ndx]);
			}
		}

		uniqueResouces.swap(resources);
	}

	return resources;
}

glu::ProgramSources generateProgramInterfaceProgramSources (const ProgramInterfaceDefinition::Program* program)
{
	glu::ProgramSources sources;

	DE_ASSERT(program->isValid());

	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
	{
		const ProgramInterfaceDefinition::Shader*	shader						= program->getShaders()[shaderNdx];
		bool										containsUserDefinedOutputs	= false;
		bool										containsUserDefinedInputs	= false;
		std::ostringstream							sourceBuf;
		std::ostringstream							usageBuf;

		sourceBuf	<< glu::getGLSLVersionDeclaration(shader->getVersion()) << "\n"
					<< getShaderTypeDeclarations(shader->getType())
					<< "\n";

		// Struct definitions

		writeStructureDefinitions(sourceBuf, shader->getDefaultBlock());

		// variables in the default scope

		for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
			sourceBuf << shader->getDefaultBlock().variables[ndx] << ";\n";

		if (!shader->getDefaultBlock().variables.empty())
			sourceBuf << "\n";

		// Interface blocks

		for (int ndx = 0; ndx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++ndx)
			writeInterfaceBlock(sourceBuf, shader->getDefaultBlock().interfaceBlocks[ndx]);

		// Use inputs and outputs so that they won't be removed by the optimizer

		usageBuf <<	"highp vec4 readInputs()\n"
					"{\n"
					"	highp vec4 retValue = vec4(0.0);\n";

		// User-defined inputs

		for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
		{
			if (shader->getDefaultBlock().variables[ndx].storage == glu::STORAGE_IN ||
				shader->getDefaultBlock().variables[ndx].storage == glu::STORAGE_UNIFORM)
			{
				writeVariableReadAccumulateExpression(usageBuf, "retValue", shader->getDefaultBlock().variables[ndx].name, shader->getDefaultBlock().variables[ndx].varType);
				containsUserDefinedInputs = true;
			}
		}

		for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
		{
			const glu::InterfaceBlock& interface = shader->getDefaultBlock().interfaceBlocks[interfaceNdx];
			if (interface.storage == glu::STORAGE_UNIFORM ||
				(interface.storage == glu::STORAGE_BUFFER && (interface.memoryAccessQualifierFlags & glu::MEMORYACCESSQUALIFIER_WRITEONLY_BIT) == 0))
			{
				writeInterfaceReadAccumulateExpression(usageBuf, "retValue", interface);
				containsUserDefinedInputs = true;
			}
		}

		// Built-in-inputs

		if (!containsUserDefinedInputs)
		{
			if (shader->getType() == glu::SHADERTYPE_VERTEX)
				usageBuf << "	retValue += vec4(float(gl_VertexID));\n";
			else if (shader->getType() == glu::SHADERTYPE_FRAGMENT)
				usageBuf << "	retValue += gl_FragCoord;\n";
			else if (shader->getType() == glu::SHADERTYPE_GEOMETRY)
				usageBuf << "	retValue += gl_in[0].gl_Position;\n";
			else if (shader->getType() == glu::SHADERTYPE_TESSELLATION_CONTROL)
				usageBuf << "	retValue += gl_in[0].gl_Position;\n";
			else if (shader->getType() == glu::SHADERTYPE_TESSELLATION_EVALUATION)
				usageBuf << "	retValue += gl_in[0].gl_Position;\n";
			else if (shader->getType() == glu::SHADERTYPE_COMPUTE)
				usageBuf << "	retValue += vec4(float(gl_NumWorkGroups.x));\n";
		}

		usageBuf <<	"	return retValue;\n"
					"}\n\n";

		usageBuf <<	"void writeOutputs(in highp vec4 dummyValue)\n"
					"{\n";

		// User-defined outputs

		for (int ndx = 0; ndx < (int)shader->getDefaultBlock().variables.size(); ++ndx)
		{
			if (shader->getDefaultBlock().variables[ndx].storage == glu::STORAGE_OUT)
			{
				writeVariableWriteExpression(usageBuf, "dummyValue", shader->getDefaultBlock().variables[ndx].name, shader->getDefaultBlock().variables[ndx].varType);
				containsUserDefinedOutputs = true;
			}
		}

		for (int interfaceNdx = 0; interfaceNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++interfaceNdx)
		{
			const glu::InterfaceBlock& interface = shader->getDefaultBlock().interfaceBlocks[interfaceNdx];
			if (interface.storage == glu::STORAGE_BUFFER && (interface.memoryAccessQualifierFlags & glu::MEMORYACCESSQUALIFIER_READONLY_BIT) == 0)
			{
				writeInterfaceWriteExpression(usageBuf, "dummyValue", interface);
				containsUserDefinedOutputs = true;
			}
		}

		// Builtin-outputs that must be written to

		if (shader->getType() == glu::SHADERTYPE_VERTEX)
			usageBuf << "	gl_Position = dummyValue;\n";
		else if (shader->getType() == glu::SHADERTYPE_GEOMETRY)
			usageBuf << "	gl_Position = dummyValue;\n"
						 "	EmitVertex();\n";
		else if (shader->getType() == glu::SHADERTYPE_TESSELLATION_CONTROL)
			usageBuf << "	gl_out[gl_InvocationID].gl_Position = dummyValue;\n";
		else if (shader->getType() == glu::SHADERTYPE_TESSELLATION_EVALUATION)
			usageBuf << "	gl_Position = dummyValue;\n";

		// Output to sink input data to

		if (!containsUserDefinedOutputs)
		{
			if (shader->getType() == glu::SHADERTYPE_FRAGMENT)
				usageBuf << "	gl_FragDepth = dot(dummyValue.xy, dummyValue.xw);\n";
			else if (shader->getType() == glu::SHADERTYPE_COMPUTE)
				usageBuf << "	dummyOutputBlock.dummyValue = dummyValue;\n";
		}

		usageBuf <<	"}\n\n"
					"void main()\n"
					"{\n"
					"	writeOutputs(readInputs());\n"
					"}\n";

		// Interface for dummy output

		if (shader->getType() == glu::SHADERTYPE_COMPUTE && !containsUserDefinedOutputs)
		{
			sourceBuf	<< "writeonly buffer DummyOutputInterface\n"
						<< "{\n"
						<< "	highp vec4 dummyValue;\n"
						<< "} dummyOutputBlock;\n\n";
		}

		sources << glu::ShaderSource(shader->getType(), sourceBuf.str() + usageBuf.str());
	}

	if (program->isSeparable())
		sources << glu::ProgramSeparable(true);

	for (int ndx = 0; ndx < (int)program->getTransformFeedbackVaryings().size(); ++ndx)
		sources << glu::TransformFeedbackVarying(program->getTransformFeedbackVaryings()[ndx]);

	if (program->getTransformFeedbackMode())
		sources << glu::TransformFeedbackMode(program->getTransformFeedbackMode());

	return sources;
}

bool findProgramVariablePathByPathName (std::vector<VariablePathComponent>& typePath, const ProgramInterfaceDefinition::Program* program, const std::string& pathName, const VariableSearchFilter& filter)
{
	std::vector<VariablePathComponent> modifiedPath;

	if (!traverseProgramVariablePath(modifiedPath, program, pathName, filter))
		return false;

	// modify param only on success
	typePath.swap(modifiedPath);
	return true;
}

ProgramInterfaceDefinition::ShaderResourceUsage getShaderResourceUsage (const ProgramInterfaceDefinition::Shader* shader)
{
	ProgramInterfaceDefinition::ShaderResourceUsage retVal;

	retVal.numInputs						= getNumTypeInstances(shader, glu::STORAGE_IN, dummyConstantTypeFilter<true>);
	retVal.numInputVectors					= getNumVectors(shader, glu::STORAGE_IN);
	retVal.numInputComponents				= getNumComponents(shader, glu::STORAGE_IN);

	retVal.numOutputs						= getNumTypeInstances(shader, glu::STORAGE_OUT, dummyConstantTypeFilter<true>);
	retVal.numOutputVectors					= getNumVectors(shader, glu::STORAGE_OUT);
	retVal.numOutputComponents				= getNumComponents(shader, glu::STORAGE_OUT);

	retVal.numDefaultBlockUniformComponents	= getNumDefaultBlockComponents(shader, glu::STORAGE_UNIFORM);
	retVal.numCombinedUniformComponents		= getNumComponents(shader, glu::STORAGE_UNIFORM);
	retVal.numUniformVectors				= getNumVectors(shader, glu::STORAGE_UNIFORM);

	retVal.numSamplers						= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeSampler);
	retVal.numImages						= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeImage);

	retVal.numAtomicCounterBuffers			= getNumAtomicCounterBuffers(shader);
	retVal.numAtomicCounters				= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeAtomicCounter);

	retVal.numUniformBlocks					= getNumShaderBlocks(shader, glu::STORAGE_UNIFORM);
	retVal.numShaderStorageBlocks			= getNumShaderBlocks(shader, glu::STORAGE_BUFFER);

	return retVal;
}

ProgramInterfaceDefinition::ProgramResourceUsage getCombinedProgramResourceUsage (const ProgramInterfaceDefinition::Program* program)
{
	ProgramInterfaceDefinition::ProgramResourceUsage retVal;

	retVal.uniformBufferMaxBinding				= -1; // max binding is inclusive upper bound. Allow 0 bindings by using negative value
	retVal.uniformBufferMaxSize					= 0;
	retVal.numUniformBlocks						= 0;
	retVal.numCombinedVertexUniformComponents	= 0;
	retVal.numCombinedFragmentUniformComponents	= 0;
	retVal.shaderStorageBufferMaxBinding		= -1; // see above
	retVal.shaderStorageBufferMaxSize			= 0;
	retVal.numShaderStorageBlocks				= 0;
	retVal.numVaryingComponents					= 0;
	retVal.numVaryingVectors					= 0;
	retVal.numCombinedSamplers					= 0;
	retVal.atomicCounterBufferMaxBinding		= -1; // see above
	retVal.atomicCounterBufferMaxSize			= 0;
	retVal.numAtomicCounterBuffers				= 0;
	retVal.numAtomicCounters					= 0;
	retVal.maxImageBinding						= -1; // see above
	retVal.numCombinedImages					= 0;
	retVal.numCombinedOutputResources			= 0;
	retVal.numXFBInterleavedComponents			= 0;
	retVal.numXFBSeparateAttribs				= 0;
	retVal.numXFBSeparateComponents				= 0;
	retVal.fragmentOutputMaxBinding				= -1; // see above

	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
	{
		const ProgramInterfaceDefinition::Shader* const shader = program->getShaders()[shaderNdx];

		retVal.uniformBufferMaxBinding		= de::max(retVal.uniformBufferMaxBinding, getMaxBufferBinding(shader, glu::STORAGE_UNIFORM));
		retVal.uniformBufferMaxSize			= de::max(retVal.uniformBufferMaxSize, getBufferMaxSize(shader, glu::STORAGE_UNIFORM));
		retVal.numUniformBlocks				+= getNumShaderBlocks(shader, glu::STORAGE_UNIFORM);

		if (shader->getType() == glu::SHADERTYPE_VERTEX)
			retVal.numCombinedVertexUniformComponents += getNumComponents(shader, glu::STORAGE_UNIFORM);

		if (shader->getType() == glu::SHADERTYPE_FRAGMENT)
			retVal.numCombinedFragmentUniformComponents += getNumComponents(shader, glu::STORAGE_UNIFORM);

		retVal.shaderStorageBufferMaxBinding	= de::max(retVal.shaderStorageBufferMaxBinding, getMaxBufferBinding(shader, glu::STORAGE_BUFFER));
		retVal.shaderStorageBufferMaxSize		= de::max(retVal.shaderStorageBufferMaxSize, getBufferMaxSize(shader, glu::STORAGE_BUFFER));
		retVal.numShaderStorageBlocks			+= getNumShaderBlocks(shader, glu::STORAGE_BUFFER);

		if (shader->getType() == glu::SHADERTYPE_VERTEX)
		{
			retVal.numVaryingComponents += getNumComponents(shader, glu::STORAGE_OUT);
			retVal.numVaryingVectors	+= getNumVectors(shader, glu::STORAGE_OUT);
		}

		retVal.numCombinedSamplers	+= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeSampler);

		retVal.atomicCounterBufferMaxBinding	= de::max(retVal.atomicCounterBufferMaxBinding, getAtomicCounterMaxBinding(shader));
		retVal.atomicCounterBufferMaxSize		= de::max(retVal.atomicCounterBufferMaxSize, getAtomicCounterMaxBufferSize(shader));
		retVal.numAtomicCounterBuffers			+= getNumAtomicCounterBuffers(shader);
		retVal.numAtomicCounters				+= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeAtomicCounter);
		retVal.maxImageBinding					= de::max(retVal.maxImageBinding, getUniformMaxBinding(shader, glu::isDataTypeImage));
		retVal.numCombinedImages				+= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeImage);

		retVal.numCombinedOutputResources		+= getNumTypeInstances(shader, glu::STORAGE_UNIFORM, glu::isDataTypeImage);
		retVal.numCombinedOutputResources		+= getNumShaderBlocks(shader, glu::STORAGE_BUFFER);

		if (shader->getType() == glu::SHADERTYPE_FRAGMENT)
		{
			retVal.numCombinedOutputResources += getNumVectors(shader, glu::STORAGE_OUT);
			retVal.fragmentOutputMaxBinding = de::max(retVal.fragmentOutputMaxBinding, getFragmentOutputMaxLocation(shader));
		}
	}

	if (program->getTransformFeedbackMode() == GL_INTERLEAVED_ATTRIBS)
		retVal.numXFBInterleavedComponents = getNumXFBComponents(program);
	else if (program->getTransformFeedbackMode() == GL_SEPARATE_ATTRIBS)
	{
		retVal.numXFBSeparateAttribs	= (int)program->getTransformFeedbackVaryings().size();
		retVal.numXFBSeparateComponents	= getNumMaxXFBOutputComponents(program);
	}

	return retVal;
}

} // Functional
} // gles31
} // deqp
