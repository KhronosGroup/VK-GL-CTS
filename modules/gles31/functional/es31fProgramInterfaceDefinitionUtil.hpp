#ifndef _ES31FPROGRAMINTERFACEDEFINITIONUTIL_HPP
#define _ES31FPROGRAMINTERFACEDEFINITIONUTIL_HPP
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

#include "tcuDefs.hpp"
#include "tes31TestCase.hpp"
#include "gluShaderProgram.hpp"
#include "es31fProgramInterfaceDefinition.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace ProgramInterfaceDefinition
{

class Program;

class VariablePathComponent
{
public:
									VariablePathComponent	(void)									:m_type(TYPE_LAST)				{								}
									VariablePathComponent	(const glu::VarType* type)				:m_type(TYPE_TYPE)				{ m_data.type = type;			}
									VariablePathComponent	(const glu::InterfaceBlock* block)		:m_type(TYPE_INTERFACEBLOCK)	{ m_data.block = block;			}
									VariablePathComponent	(const glu::VariableDeclaration* decl)	:m_type(TYPE_DECLARATION)		{ m_data.declaration = decl;	}

									VariablePathComponent	(const VariablePathComponent& other) : m_data(other.m_data), m_type(other.m_type) { }
	VariablePathComponent&			operator=				(const VariablePathComponent& other) { m_type = other.m_type; m_data = other.m_data; return *this; }

	bool							isVariableType			(void) const { return m_type == TYPE_TYPE;								}
	bool							isInterfaceBlock		(void) const { return m_type == TYPE_INTERFACEBLOCK;					}
	bool							isDeclaration			(void) const { return m_type == TYPE_DECLARATION;						}

	const glu::VarType*				getVariableType			(void) const { DE_ASSERT(isVariableType()); return m_data.type;			}
	const glu::InterfaceBlock*		getInterfaceBlock		(void) const { DE_ASSERT(isInterfaceBlock()); return m_data.block;		}
	const glu::VariableDeclaration*	getDeclaration			(void) const { DE_ASSERT(isDeclaration()); return m_data.declaration;	}

private:
	enum Type
	{
		TYPE_TYPE,
		TYPE_INTERFACEBLOCK,
		TYPE_DECLARATION,

		TYPE_LAST
	};

	union Data
	{
		const glu::VarType*				type;
		const glu::InterfaceBlock*		block;
		const glu::VariableDeclaration*	declaration;

		Data (void) : type(DE_NULL) { }
	} m_data;

	Type m_type;
};

struct VariableSearchFilter
{
								VariableSearchFilter	(glu::ShaderType shaderType, glu::Storage storage) : m_shaderType(shaderType), m_storage(storage), m_null(false) { }

	static VariableSearchFilter	intersection			(const VariableSearchFilter& a, const VariableSearchFilter& b);

	bool						matchesFilter			(const ProgramInterfaceDefinition::Shader* shader) const	{ return !m_null && (m_shaderType == glu::SHADERTYPE_LAST || shader->getType() == m_shaderType);	}
	bool						matchesFilter			(const glu::VariableDeclaration& variable) const			{ return !m_null && (m_storage == glu::STORAGE_LAST || variable.storage == m_storage);				}
	bool						matchesFilter			(const glu::InterfaceBlock& block) const					{ return !m_null && (m_storage == glu::STORAGE_LAST || block.storage == m_storage);				}

	glu::ShaderType				getShaderTypeFilter		(void) const												{ return m_shaderType;	}
	glu::Storage				getStorageFilter		(void) const												{ return m_storage;		}

private:
								VariableSearchFilter	(glu::ShaderType shaderType, glu::Storage storage, bool empty) : m_shaderType(shaderType), m_storage(storage), m_null(empty) { }

	const glu::ShaderType		m_shaderType;
	const glu::Storage			m_storage;
	const bool					m_null;					// !< Null filter does not match any variable
};

struct ShaderResourceUsage
{
	int numInputs;
	int numInputVectors;
	int numInputComponents;
	int numOutputs;
	int numOutputVectors;
	int numOutputComponents;

	int numDefaultBlockUniformComponents;
	int numCombinedUniformComponents;
	int numUniformVectors;

	int numSamplers;
	int numImages;

	int numAtomicCounterBuffers;
	int numAtomicCounters;

	int numUniformBlocks;
	int numShaderStorageBlocks;
};

struct ProgramResourceUsage
{
	int uniformBufferMaxBinding;
	int uniformBufferMaxSize;
	int numUniformBlocks;
	int numCombinedVertexUniformComponents;
	int numCombinedFragmentUniformComponents;
	int shaderStorageBufferMaxBinding;
	int shaderStorageBufferMaxSize;
	int numShaderStorageBlocks;
	int numVaryingComponents;
	int numVaryingVectors;
	int numCombinedSamplers;
	int atomicCounterBufferMaxBinding;
	int atomicCounterBufferMaxSize;
	int numAtomicCounterBuffers;
	int numAtomicCounters;
	int maxImageBinding;
	int numCombinedImages;
	int numCombinedOutputResources;
	int numXFBInterleavedComponents;
	int numXFBSeparateAttribs;
	int numXFBSeparateComponents;
	int fragmentOutputMaxBinding;
};

} // ProgramInterfaceDefinition

enum ResourceNameGenerationFlag
{
	RESOURCE_NAME_GENERATION_FLAG_DEFAULT 						= 0x0,
	RESOURCE_NAME_GENERATION_FLAG_TOP_LEVEL_BUFFER_VARIABLE		= 0x1,
	RESOURCE_NAME_GENERATION_FLAG_TRANSFORM_FEEDBACK_VARIABLE	= 0x2,

	RESOURCE_NAME_GENERATION_FLAG_MASK							= 0x3
};

std::vector<std::string>							getProgramInterfaceResourceList				(const ProgramInterfaceDefinition::Program* program, ProgramInterface interface);
std::vector<std::string>							getProgramInterfaceBlockMemberResourceList	(const glu::InterfaceBlock& interfaceBlock);
glu::ProgramSources									generateProgramInterfaceProgramSources		(const ProgramInterfaceDefinition::Program* program);
bool												findProgramVariablePathByPathName			(std::vector<ProgramInterfaceDefinition::VariablePathComponent>& typePath, const ProgramInterfaceDefinition::Program* program, const std::string& pathName, const ProgramInterfaceDefinition::VariableSearchFilter& filter);
void												generateVariableTypeResourceNames			(std::vector<std::string>& resources, const std::string& name, const glu::VarType& type, deUint32 resourceNameGenerationFlags);
ProgramInterfaceDefinition::ShaderResourceUsage		getShaderResourceUsage						(const ProgramInterfaceDefinition::Shader* shader);
ProgramInterfaceDefinition::ProgramResourceUsage	getCombinedProgramResourceUsage				(const ProgramInterfaceDefinition::Program* program);

} // Functional
} // gles31
} // deqp

#endif // _ES31FPROGRAMINTERFACEDEFINITIONUTIL_HPP
