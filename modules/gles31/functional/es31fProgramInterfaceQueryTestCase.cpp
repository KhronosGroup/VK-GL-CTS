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
 * \brief Program interface query test case
 *//*--------------------------------------------------------------------*/

#include "es31fProgramInterfaceQueryTestCase.hpp"
#include "es31fProgramInterfaceDefinitionUtil.hpp"
#include "tcuTestLog.hpp"
#include "gluVarTypeUtil.hpp"
#include "gluStrUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "deSTLUtil.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

using ProgramInterfaceDefinition::VariablePathComponent;
using ProgramInterfaceDefinition::VariableSearchFilter;

static bool stringEndsWith (const std::string& str, const std::string& suffix)
{
	if (suffix.length() > str.length())
		return false;
	else
		return str.substr(str.length() - suffix.length()) == suffix;
}

static glw::GLenum getProgramDefaultBlockInterfaceFromStorage (glu::Storage storage)
{
	switch (storage)
	{
		case glu::STORAGE_IN:		return GL_PROGRAM_INPUT;
		case glu::STORAGE_OUT:		return GL_PROGRAM_OUTPUT;
		case glu::STORAGE_UNIFORM:	return GL_UNIFORM;
		default:
			DE_ASSERT(false);
			return 0;
	}
}

static bool isBufferBackedInterfaceBlockStorage (glu::Storage storage)
{
	return storage == glu::STORAGE_BUFFER || storage == glu::STORAGE_UNIFORM;
}

static int getTypeSize (glu::DataType type)
{
	if (type == glu::TYPE_FLOAT)
		return 4;
	else if (type == glu::TYPE_INT || type == glu::TYPE_UINT)
		return 4;
	else if (type == glu::TYPE_BOOL)
		return 4; // uint

	DE_ASSERT(false);
	return 0;
}

static int getVarTypeSize (const glu::VarType& type)
{
	if (type.isBasicType())
	{
		// return in basic machine units
		return glu::getDataTypeScalarSize(type.getBasicType()) * getTypeSize(glu::getDataTypeScalarType(type.getBasicType()));
	}
	else if (type.isStructType())
	{
		int size = 0;
		for (int ndx = 0; ndx < type.getStructPtr()->getNumMembers(); ++ndx)
			size += getVarTypeSize(type.getStructPtr()->getMember(ndx).getType());
		return size;
	}
	else if (type.isArrayType())
	{
		// unsized arrays are handled as if they had only one element
		if (type.getArraySize() == glu::VarType::UNSIZED_ARRAY)
			return getVarTypeSize(type.getElementType());
		else
			return type.getArraySize() * getVarTypeSize(type.getElementType());
	}
	else
	{
		DE_ASSERT(false);
		return 0;
	}
}

static glu::MatrixOrder getMatrixOrderFromPath (const std::vector<VariablePathComponent>& path)
{
	glu::MatrixOrder order = glu::MATRIXORDER_LAST;

	// inherit majority
	for (int pathNdx = 0; pathNdx < (int)path.size(); ++pathNdx)
	{
		glu::MatrixOrder matOrder;

		if (path[pathNdx].isInterfaceBlock())
			matOrder = path[pathNdx].getInterfaceBlock()->layout.matrixOrder;
		else if (path[pathNdx].isDeclaration())
			matOrder = path[pathNdx].getDeclaration()->layout.matrixOrder;
		else if (path[pathNdx].isVariableType())
			matOrder = glu::MATRIXORDER_LAST;
		else
		{
			DE_ASSERT(false);
			return glu::MATRIXORDER_LAST;
		}

		if (matOrder != glu::MATRIXORDER_LAST)
			order = matOrder;
	}

	return order;
}

class PropValidator
{
public:
									PropValidator					(Context& context, ProgramResourcePropFlags validationProp, const char* requiredExtension = "");

	virtual std::string				getHumanReadablePropertyString	(glw::GLint propVal) const;
	virtual void					validate						(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const = 0;

	bool							isSupported						(void) const;
	bool							isSelected						(deUint32 caseFlags) const;

protected:
	void							setError						(const std::string& err) const;

	tcu::TestContext&				m_testCtx;
	const glu::RenderContext&		m_renderContext;

private:
	const glu::ContextInfo&			m_contextInfo;
	const std::string				m_extension;
	const ProgramResourcePropFlags	m_validationProp;
};

PropValidator::PropValidator (Context& context, ProgramResourcePropFlags validationProp, const char* requiredExtension)
	: m_testCtx			(context.getTestContext())
	, m_renderContext	(context.getRenderContext())
	, m_contextInfo		(context.getContextInfo())
	, m_extension		(requiredExtension)
	, m_validationProp	(validationProp)
{
}

std::string PropValidator::getHumanReadablePropertyString (glw::GLint propVal) const
{
	return de::toString(propVal);
}

bool PropValidator::isSupported (void) const
{
	return m_extension.empty() || m_contextInfo.isExtensionSupported(m_extension.c_str());
}

bool PropValidator::isSelected (deUint32 caseFlags) const
{
	return (caseFlags & (deUint32)m_validationProp) != 0;
}

void PropValidator::setError (const std::string& err) const
{
	// don't overwrite earlier errors
	if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, err.c_str());
}

class SingleVariableValidator : public PropValidator
{
public:
					SingleVariableValidator	(Context& context, ProgramResourcePropFlags validationProp, glw::GLuint programID, const VariableSearchFilter& filter, const char* requiredExtension = "");

	void			validate				(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const;
	virtual void	validateSingleVariable	(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const = 0;
	virtual void	validateBuiltinVariable	(const std::string& resource, glw::GLint propValue) const;

protected:
	const VariableSearchFilter	m_filter;
	const glw::GLuint			m_programID;
};

SingleVariableValidator::SingleVariableValidator (Context& context, ProgramResourcePropFlags validationProp, glw::GLuint programID, const VariableSearchFilter& filter, const char* requiredExtension)
	: PropValidator	(context, validationProp, requiredExtension)
	, m_filter		(filter)
	, m_programID	(programID)
{
}

void SingleVariableValidator::validate (const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const
{
	std::vector<VariablePathComponent> path;

	if (findProgramVariablePathByPathName(path, program, resource, m_filter))
	{
		const glu::VarType* variable = (path.back().isVariableType()) ? (path.back().getVariableType()) : (DE_NULL);

		if (!variable || !variable->isBasicType())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Error, resource name \"" << resource << "\" refers to a non-basic type." << tcu::TestLog::EndMessage;
			setError("resource not basic type");
		}
		else
			validateSingleVariable(path, resource, propValue);

		// finding matching variable in any shader is sufficient
		return;
	}
	else if (deStringBeginsWith(resource.c_str(), "gl_"))
	{
		// special case for builtins
		validateBuiltinVariable(resource, propValue);
		return;
	}

	m_testCtx.getLog() << tcu::TestLog::Message << "Error, could not find resource \"" << resource << "\" in the program" << tcu::TestLog::EndMessage;
	setError("could not find resource");
}

void SingleVariableValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(propValue);

	m_testCtx.getLog() << tcu::TestLog::Message << "Error, could not find builtin resource \"" << resource << "\" in the program" << tcu::TestLog::EndMessage;
	setError("could not find builtin resource");
}

class SingleBlockValidator : public PropValidator
{
public:
								SingleBlockValidator	(Context& context, ProgramResourcePropFlags validationProp, glw::GLuint programID, const VariableSearchFilter& filter, const char* requiredExtension = "");

	void						validate				(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const;
	virtual void				validateSingleBlock		(const glu::InterfaceBlock& block, const std::vector<int>& instanceIndex, const std::string& resource, glw::GLint propValue) const = 0;

protected:
	const VariableSearchFilter	m_filter;
	const glw::GLuint			m_programID;
};

SingleBlockValidator::SingleBlockValidator (Context& context, ProgramResourcePropFlags validationProp, glw::GLuint programID, const VariableSearchFilter& filter, const char* requiredExtension)
	: PropValidator	(context, validationProp, requiredExtension)
	, m_filter		(filter)
	, m_programID	(programID)
{
}

void SingleBlockValidator::validate (const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const
{
	glu::VarTokenizer	tokenizer		(resource.c_str());
	const std::string	blockName		= tokenizer.getIdentifier();
	std::vector<int>	instanceIndex;

	tokenizer.advance();

	// array index
	while (tokenizer.getToken() == glu::VarTokenizer::TOKEN_LEFT_BRACKET)
	{
		tokenizer.advance();
		DE_ASSERT(tokenizer.getToken() == glu::VarTokenizer::TOKEN_NUMBER);

		instanceIndex.push_back(tokenizer.getNumber());

		tokenizer.advance();
		DE_ASSERT(tokenizer.getToken() == glu::VarTokenizer::TOKEN_RIGHT_BRACKET);

		tokenizer.advance();
	}

	// no trailing garbage
	DE_ASSERT(tokenizer.getToken() == glu::VarTokenizer::TOKEN_END);

	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
	{
		const ProgramInterfaceDefinition::Shader* const shader = program->getShaders()[shaderNdx];
		if (!m_filter.matchesFilter(shader))
			continue;

		for (int blockNdx = 0; blockNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++blockNdx)
		{
			const glu::InterfaceBlock& block = shader->getDefaultBlock().interfaceBlocks[blockNdx];

			if (m_filter.matchesFilter(block) && block.interfaceName == blockName)
			{
				// dimensions match
				DE_ASSERT(instanceIndex.size() == block.dimensions.size());

				validateSingleBlock(block, instanceIndex, resource, propValue);
				return;
			}
		}
	}
	m_testCtx.getLog() << tcu::TestLog::Message << "Error, could not find resource \"" << resource << "\" in the program" << tcu::TestLog::EndMessage;
	setError("could not find resource");
}

class TypeValidator : public SingleVariableValidator
{
public:
				TypeValidator					(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);

	std::string	getHumanReadablePropertyString	(glw::GLint propVal) const;
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
	void		validateBuiltinVariable			(const std::string& resource, glw::GLint propValue) const;
};

TypeValidator::TypeValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_TYPE, programID, filter)
{
}

std::string TypeValidator::getHumanReadablePropertyString (glw::GLint propVal) const
{
	return de::toString(glu::getShaderVarTypeStr(propVal));
}

void TypeValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const glu::VarType* variable = path.back().getVariableType();

	DE_UNREF(resource);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying type, expecting " << glu::getDataTypeName(variable->getBasicType()) << tcu::TestLog::EndMessage;

	if (variable->getBasicType() != glu::getDataTypeFromGLType(propValue))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << glu::getDataTypeName(glu::getDataTypeFromGLType(propValue)) << tcu::TestLog::EndMessage;
		setError("resource type invalid");
	}
}

void TypeValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	static const struct
	{
		const char*		name;
		glu::DataType	type;
	} builtins[] =
	{
		{ "gl_Position",			glu::TYPE_FLOAT_VEC4	},
		{ "gl_FragCoord",			glu::TYPE_FLOAT_VEC4	},
		{ "gl_in[0].gl_Position",	glu::TYPE_FLOAT_VEC4	},
		{ "gl_VertexID",			glu::TYPE_INT			},
		{ "gl_InvocationID",		glu::TYPE_INT			},
		{ "gl_NumWorkGroups",		glu::TYPE_UINT_VEC3		},
		{ "gl_FragDepth",			glu::TYPE_FLOAT			},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(builtins); ++ndx)
	{
		if (resource == builtins[ndx].name)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Verifying type, expecting " << glu::getDataTypeName(builtins[ndx].type) << tcu::TestLog::EndMessage;

			if (glu::getDataTypeFromGLType(propValue) != builtins[ndx].type)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << glu::getDataTypeName(glu::getDataTypeFromGLType(propValue)) << tcu::TestLog::EndMessage;
				setError("resource type invalid");
			}
			return;
		}
	}

	DE_ASSERT(false);
}

class ArraySizeValidator : public SingleVariableValidator
{
public:
				ArraySizeValidator				(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
	void		validateBuiltinVariable			(const std::string& resource, glw::GLint propValue) const;
};

ArraySizeValidator::ArraySizeValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_ARRAY_SIZE, programID, filter)
{
}

void ArraySizeValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const VariablePathComponent		nullComponent;
	const VariablePathComponent&	enclosingcomponent	= (path.size() > 1) ? (path[path.size()-2]) : (nullComponent);

	const bool						isArray				= enclosingcomponent.isVariableType() && enclosingcomponent.getVariableType()->isArrayType();
	const bool						inUnsizedArray		= isArray && (enclosingcomponent.getVariableType()->getArraySize() == glu::VarType::UNSIZED_ARRAY);
	const int						arraySize			= (!isArray) ? (1) : (inUnsizedArray) ? (0) : (enclosingcomponent.getVariableType()->getArraySize());

	DE_UNREF(resource);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array size, expecting " << arraySize << tcu::TestLog::EndMessage;

	if (arraySize != propValue)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
		setError("resource array size invalid");
	}
}

void ArraySizeValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	// support all built-ins that getProgramInterfaceResourceList supports
	if (resource == "gl_Position"			||
		resource == "gl_VertexID"			||
		resource == "gl_FragCoord"			||
		resource == "gl_in[0].gl_Position"	||
		resource == "gl_InvocationID"		||
		resource == "gl_NumWorkGroups"		||
		resource == "gl_FragDepth")
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array size, expecting 1" << tcu::TestLog::EndMessage;

		if (propValue != 1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource array size invalid");
		}
	}
	else
		DE_ASSERT(false);
}

class ArrayStrideValidator : public SingleVariableValidator
{
public:
				ArrayStrideValidator			(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

ArrayStrideValidator::ArrayStrideValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_ARRAY_STRIDE, programID, filter)
{
}

void ArrayStrideValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const VariablePathComponent		nullComponent;
	const VariablePathComponent&	component			= path.back();
	const VariablePathComponent&	enclosingcomponent	= (path.size() > 1) ? (path[path.size()-2]) : (nullComponent);
	const VariablePathComponent&	firstComponent		= path.front();

	const bool						isBufferBlock		= firstComponent.isInterfaceBlock() && isBufferBackedInterfaceBlockStorage(firstComponent.getInterfaceBlock()->storage);
	const bool						isArray				= enclosingcomponent.isVariableType() && enclosingcomponent.getVariableType()->isArrayType();
	const bool						isAtomicCounter		= glu::isDataTypeAtomicCounter(component.getVariableType()->getBasicType()); // atomic counters are buffer backed with a stride of 4 basic machine units

	DE_UNREF(resource);

	// Layout tests will verify layouts of buffer backed arrays properly. Here we just check values are greater or equal to the element size
	if (isBufferBlock && isArray)
	{
		const int elementSize = glu::getDataTypeScalarSize(component.getVariableType()->getBasicType()) * getTypeSize(glu::getDataTypeScalarType(component.getVariableType()->getBasicType()));
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array stride, expecting greater or equal to " << elementSize << tcu::TestLog::EndMessage;

		if (propValue < elementSize)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource array stride invalid");
		}
	}
	else
	{
		// Atomics are buffer backed with stride of 4 even though they are not in an interface block
		const int arrayStride = (isAtomicCounter && isArray) ? (4) : (!isBufferBlock && !isAtomicCounter) ? (-1) : (0);

		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array stride, expecting " << arrayStride << tcu::TestLog::EndMessage;

		if (arrayStride != propValue)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource array stride invalid");
		}
	}
}

class BlockIndexValidator : public SingleVariableValidator
{
public:
				BlockIndexValidator				(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

BlockIndexValidator::BlockIndexValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_BLOCK_INDEX, programID, filter)
{
}

void BlockIndexValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const VariablePathComponent& firstComponent = path.front();

	DE_UNREF(resource);

	if (!firstComponent.isInterfaceBlock())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying block index, expecting -1" << tcu::TestLog::EndMessage;

		if (propValue != -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource block index invalid");
		}
	}
	else
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying block index, expecting a valid block index" << tcu::TestLog::EndMessage;

		if (propValue == -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource block index invalid");
		}
		else
		{
			const glw::Functions&	gl			= m_renderContext.getFunctions();
			const glw::GLenum		interface	= (firstComponent.getInterfaceBlock()->storage == glu::STORAGE_UNIFORM) ? (GL_UNIFORM_BLOCK) :
												  (firstComponent.getInterfaceBlock()->storage == glu::STORAGE_BUFFER) ? (GL_SHADER_STORAGE_BLOCK) :
												  (0);
			glw::GLint				written		= 0;
			std::vector<char>		nameBuffer	(firstComponent.getInterfaceBlock()->interfaceName.size() + 3 * firstComponent.getInterfaceBlock()->dimensions.size() + 2, '\0'); // +3 for appended "[N]", +1 for '\0' and +1 just for safety

			gl.getProgramResourceName(m_programID, interface, propValue, (int)nameBuffer.size() - 1, &written, &nameBuffer[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "query block name");
			TCU_CHECK(written < (int)nameBuffer.size());
			TCU_CHECK(nameBuffer.back() == '\0');

			{
				const std::string	blockName		(&nameBuffer[0], written);
				std::ostringstream	expectedName;

				expectedName << firstComponent.getInterfaceBlock()->interfaceName;
				for (int dimensionNdx = 0; dimensionNdx < (int)firstComponent.getInterfaceBlock()->dimensions.size(); ++dimensionNdx)
					expectedName << "[0]";

				m_testCtx.getLog() << tcu::TestLog::Message << "Block name with index " << propValue << " is \"" << blockName << "\"" << tcu::TestLog::EndMessage;
				if (blockName != expectedName.str())
				{
					m_testCtx.getLog() << tcu::TestLog::Message << "\tError, expected " << expectedName.str() << tcu::TestLog::EndMessage;
					setError("resource block index invalid");
				}
			}
		}
	}
}

class IsRowMajorValidator : public SingleVariableValidator
{
public:
				IsRowMajorValidator				(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);

	std::string getHumanReadablePropertyString	(glw::GLint propVal) const;
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

IsRowMajorValidator::IsRowMajorValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_MATRIX_ROW_MAJOR, programID, filter)
{
}

std::string IsRowMajorValidator::getHumanReadablePropertyString	(glw::GLint propVal) const
{
	return de::toString(glu::getBooleanStr(propVal));
}

void IsRowMajorValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const VariablePathComponent&	component			= path.back();
	const VariablePathComponent&	firstComponent		= path.front();

	const bool						isBufferBlock		= firstComponent.isInterfaceBlock() && isBufferBackedInterfaceBlockStorage(firstComponent.getInterfaceBlock()->storage);
	const bool						isMatrix			= glu::isDataTypeMatrix(component.getVariableType()->getBasicType());
	const int						expected			= (isBufferBlock && isMatrix && getMatrixOrderFromPath(path) == glu::MATRIXORDER_ROW_MAJOR) ? (1) : (0);

	DE_UNREF(resource);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying matrix order, expecting IS_ROW_MAJOR = " << expected << tcu::TestLog::EndMessage;

	if (propValue != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
		setError("resource matrix order invalid");
	}
}

class MatrixStrideValidator : public SingleVariableValidator
{
public:
				MatrixStrideValidator			(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

MatrixStrideValidator::MatrixStrideValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_MATRIX_STRIDE, programID, filter)
{
}

void MatrixStrideValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const VariablePathComponent&	component			= path.back();
	const VariablePathComponent&	firstComponent		= path.front();

	const bool						isBufferBlock		= firstComponent.isInterfaceBlock() && isBufferBackedInterfaceBlockStorage(firstComponent.getInterfaceBlock()->storage);
	const bool						isMatrix			= glu::isDataTypeMatrix(component.getVariableType()->getBasicType());

	DE_UNREF(resource);

	// Layout tests will verify layouts of buffer backed arrays properly. Here we just check the stride is is greater or equal to the row/column size
	if (isBufferBlock && isMatrix)
	{
		const bool	columnMajor			= getMatrixOrderFromPath(path) != glu::MATRIXORDER_ROW_MAJOR;
		const int	numMajorElements	= (columnMajor) ? (glu::getDataTypeMatrixNumRows(component.getVariableType()->getBasicType())) : (glu::getDataTypeMatrixNumColumns(component.getVariableType()->getBasicType()));
		const int	majorSize			= numMajorElements * getTypeSize(glu::getDataTypeScalarType(component.getVariableType()->getBasicType()));

		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying matrix stride, expecting greater or equal to " << majorSize << tcu::TestLog::EndMessage;

		if (propValue < majorSize)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource matrix stride invalid");
		}
	}
	else
	{
		const int matrixStride = (!isBufferBlock && !glu::isDataTypeAtomicCounter(component.getVariableType()->getBasicType())) ? (-1) : (0);

		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying matrix stride, expecting " << matrixStride << tcu::TestLog::EndMessage;

		if (matrixStride != propValue)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource matrix stride invalid");
		}
	}
}

class AtomicCounterBufferIndexVerifier : public SingleVariableValidator
{
public:
				AtomicCounterBufferIndexVerifier	(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable				(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

AtomicCounterBufferIndexVerifier::AtomicCounterBufferIndexVerifier (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_ATOMIC_COUNTER_BUFFER_INDEX, programID, filter)
{
}

void AtomicCounterBufferIndexVerifier::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(resource);

	if (!glu::isDataTypeAtomicCounter(path.back().getVariableType()->getBasicType()))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying atomic counter buffer index, expecting -1" << tcu::TestLog::EndMessage;

		if (propValue != -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource atomic counter buffer index invalid");
		}
	}
	else
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying atomic counter buffer index, expecting a valid index" << tcu::TestLog::EndMessage;

		if (propValue == -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource atomic counter buffer index invalid");
		}
		else
		{
			const glw::Functions&	gl					= m_renderContext.getFunctions();
			glw::GLint				numActiveResources	= 0;

			gl.getProgramInterfaceiv(m_programID, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &numActiveResources);
			GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramInterfaceiv(..., GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, ...)");

			if (propValue >= numActiveResources)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << ", GL_ACTIVE_RESOURCES = " << numActiveResources << tcu::TestLog::EndMessage;
				setError("resource atomic counter buffer index invalid");
			}
		}
	}
}

class LocationValidator : public SingleVariableValidator
{
public:
				LocationValidator		(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable	(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
	void		validateBuiltinVariable	(const std::string& resource, glw::GLint propValue) const;
};

LocationValidator::LocationValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_LOCATION, programID, filter)
{
}

static int getVariableLocationLength (const glu::VarType& type)
{
	if (type.isBasicType())
	{
		if (glu::isDataTypeMatrix(type.getBasicType()))
			return glu::getDataTypeMatrixNumColumns(type.getBasicType());
		else
			return 1;
	}
	else if (type.isStructType())
	{
		int size = 0;
		for (int ndx = 0; ndx < type.getStructPtr()->getNumMembers(); ++ndx)
			size += getVariableLocationLength(type.getStructPtr()->getMember(ndx).getType());
		return size;
	}
	else if (type.isArrayType())
		return type.getArraySize() * getVariableLocationLength(type.getElementType());
	else
	{
		DE_ASSERT(false);
		return 0;
	}
}

static int getIOSubVariableLocation (const std::vector<VariablePathComponent>& path, int startNdx, int currentLocation)
{
	if (currentLocation == -1)
		return -1;

	if (path[startNdx].getVariableType()->isBasicType())
		return currentLocation;
	else if (path[startNdx].getVariableType()->isArrayType())
		return getIOSubVariableLocation(path, startNdx+1, currentLocation);
	else if (path[startNdx].getVariableType()->isStructType())
	{
		for (int ndx = 0; ndx < path[startNdx].getVariableType()->getStructPtr()->getNumMembers(); ++ndx)
		{
			if (&path[startNdx].getVariableType()->getStructPtr()->getMember(ndx).getType() == path[startNdx + 1].getVariableType())
				return getIOSubVariableLocation(path, startNdx + 1, currentLocation);

			if (currentLocation != -1)
				currentLocation += getVariableLocationLength(path[startNdx].getVariableType()->getStructPtr()->getMember(ndx).getType());
		}

		// could not find member, never happens
		DE_ASSERT(false);
		return -1;
	}
	else
	{
		DE_ASSERT(false);
		return -1;
	}
}

static int getIOBlockVariableLocation (const std::vector<VariablePathComponent>& path)
{
	const glu::InterfaceBlock*	block			= path.front().getInterfaceBlock();
	int							currentLocation	= block->layout.location;

	// Find the block member
	for (int memberNdx = 0; memberNdx < (int)block->variables.size(); ++memberNdx)
	{
		if (&block->variables[memberNdx] == path[1].getDeclaration())
			break;

		if (block->variables[memberNdx].layout.location != -1)
			currentLocation = block->variables[memberNdx].layout.location;

		currentLocation += getVariableLocationLength(block->variables[memberNdx].varType);
	}

	// Find subtype location in the complex type
	return getIOSubVariableLocation(path, 2, currentLocation);
}

static int getExplicitLocationFromPath (const std::vector<VariablePathComponent>& path)
{
	const glu::VariableDeclaration* varDecl = (path[0].isInterfaceBlock()) ? (path[1].getDeclaration()) : (path[0].getDeclaration());

	if (path.front().isInterfaceBlock() && path.front().getInterfaceBlock()->storage == glu::STORAGE_UNIFORM)
	{
		// inside uniform block
		return -1;
	}
	else if (path.front().isInterfaceBlock() && (path.front().getInterfaceBlock()->storage == glu::STORAGE_IN || path.front().getInterfaceBlock()->storage == glu::STORAGE_OUT))
	{
		// inside ioblock
		return getIOBlockVariableLocation(path);
	}
	else if (varDecl->storage == glu::STORAGE_UNIFORM)
	{
		// default block uniform
		return varDecl->layout.location;
	}
	else if (varDecl->storage == glu::STORAGE_IN || varDecl->storage == glu::STORAGE_OUT)
	{
		// default block input/output
		return getIOSubVariableLocation(path, 1, varDecl->layout.location);
	}
	else
	{
		DE_ASSERT(false);
		return -1;
	}
}

void LocationValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const bool	isAtomicCounterUniform	= glu::isDataTypeAtomicCounter(path.back().getVariableType()->getBasicType());
	const bool	isUniformBlockVariable	= path.front().isInterfaceBlock() && path.front().getInterfaceBlock()->storage == glu::STORAGE_UNIFORM;
	const bool	isVertexShader			= m_filter.getShaderTypeFilter() == glu::SHADERTYPE_VERTEX;
	const bool	isFragmentShader		= m_filter.getShaderTypeFilter() == glu::SHADERTYPE_FRAGMENT;
	const bool	isInputVariable			= (path.front().isInterfaceBlock()) ? (path.front().getInterfaceBlock()->storage == glu::STORAGE_IN) : (path.front().getDeclaration()->storage == glu::STORAGE_IN);
	const bool	isOutputVariable		= (path.front().isInterfaceBlock()) ? (path.front().getInterfaceBlock()->storage == glu::STORAGE_OUT) : (path.front().getDeclaration()->storage == glu::STORAGE_OUT);
	const int	explicitLayoutLocation	= getExplicitLocationFromPath(path);

	bool		expectLocation;
	std::string	reasonStr;

	if (isAtomicCounterUniform)
	{
		expectLocation = false;
		reasonStr = "Atomic counter uniforms have effective location of -1";
	}
	else if (isUniformBlockVariable)
	{
		expectLocation = false;
		reasonStr = "Uniform block variables have effective location of -1";
	}
	else if (isInputVariable && !isVertexShader && explicitLayoutLocation == -1)
	{
		expectLocation = false;
		reasonStr = "Inputs (except for vertex shader inputs) not declared with a location layout qualifier have effective location of -1";
	}
	else if (isOutputVariable && !isFragmentShader && explicitLayoutLocation == -1)
	{
		expectLocation = false;
		reasonStr = "Outputs (except for fragment shader outputs) not declared with a location layout qualifier have effective location of -1";
	}
	else
	{
		expectLocation = true;
	}

	if (!expectLocation)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying uniform location, expecting -1. (" << reasonStr << ")" << tcu::TestLog::EndMessage;

		if (propValue != -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource location invalid");
		}
	}
	else
	{
		bool locationOk;

		if (explicitLayoutLocation == -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Verifying location, expecting a valid location" << tcu::TestLog::EndMessage;
			locationOk = (propValue != -1);
		}
		else
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Verifying location, expecting " << explicitLayoutLocation << tcu::TestLog::EndMessage;
			locationOk = (propValue == explicitLayoutLocation);
		}

		if (!locationOk)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
			setError("resource location invalid");
		}
		else
		{
			const VariablePathComponent		nullComponent;
			const VariablePathComponent&	enclosingcomponent	= (path.size() > 1) ? (path[path.size()-2]) : (nullComponent);
			const bool						isArray				= enclosingcomponent.isVariableType() && enclosingcomponent.getVariableType()->isArrayType();

			const glw::Functions&			gl					= m_renderContext.getFunctions();
			const glu::Storage				storage				= (path.front().isInterfaceBlock()) ? (path.front().getInterfaceBlock()->storage) : (path.front().getDeclaration()->storage);
			const glw::GLenum				interface			= getProgramDefaultBlockInterfaceFromStorage(storage);

			m_testCtx.getLog() << tcu::TestLog::Message << "Comparing location to the values returned by GetProgramResourceLocation" << tcu::TestLog::EndMessage;

			// Test all bottom-level array elements
			if (isArray)
			{
				const std::string arrayResourceName = (resource.size() > 3) ? (resource.substr(0, resource.size() - 3)) : (""); // chop "[0]"

				for (int arrayElementNdx = 0; arrayElementNdx < enclosingcomponent.getVariableType()->getArraySize(); ++arrayElementNdx)
				{
					const std::string	elementResourceName	= arrayResourceName + "[" + de::toString(arrayElementNdx) + "]";
					const glw::GLint	location			= gl.getProgramResourceLocation(m_programID, interface, elementResourceName.c_str());

					if (location != propValue+arrayElementNdx)
					{
						m_testCtx.getLog()
							<< tcu::TestLog::Message
							<< "\tError, getProgramResourceLocation (resource=\"" << elementResourceName << "\") returned location " << location
							<< ", expected " << (propValue+arrayElementNdx)
							<< tcu::TestLog::EndMessage;
						setError("resource location invalid");
					}
					else
						m_testCtx.getLog() << tcu::TestLog::Message << "\tLocation of \"" << elementResourceName << "\":\t" << location << tcu::TestLog::EndMessage;
				}
			}
			else
			{
				const glw::GLint location = gl.getProgramResourceLocation(m_programID, interface, resource.c_str());

				if (location != propValue)
				{
					m_testCtx.getLog() << tcu::TestLog::Message << "\tError, getProgramResourceLocation returned location " << location << ", expected " << propValue << tcu::TestLog::EndMessage;
					setError("resource location invalid");
				}
			}

		}
	}
}

void LocationValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(resource);

	// built-ins have no location

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying location, expecting -1" << tcu::TestLog::EndMessage;

	if (propValue != -1)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
		setError("resource location invalid");
	}
}

class VariableNameLengthValidator : public SingleVariableValidator
{
public:
				VariableNameLengthValidator	(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable		(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
	void		validateBuiltinVariable		(const std::string& resource, glw::GLint propValue) const;
	void		validateNameLength			(const std::string& resource, glw::GLint propValue) const;
};

VariableNameLengthValidator::VariableNameLengthValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_NAME_LENGTH, programID, filter)
{
}

void VariableNameLengthValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(path);
	validateNameLength(resource, propValue);
}

void VariableNameLengthValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	validateNameLength(resource, propValue);
}

void VariableNameLengthValidator::validateNameLength (const std::string& resource, glw::GLint propValue) const
{
	const int expected = (int)resource.length() + 1; // includes null byte
	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying name length, expecting " << expected << " (" << (int)resource.length() << " for \"" << resource << "\" + 1 byte for terminating null character)" << tcu::TestLog::EndMessage;

	if (propValue != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid name length, got " << propValue << tcu::TestLog::EndMessage;
		setError("name length invalid");
	}
}

class OffsetValidator : public SingleVariableValidator
{
public:
				OffsetValidator			(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable	(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

OffsetValidator::OffsetValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_OFFSET, programID, filter)
{
}

void OffsetValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	const bool isAtomicCounterUniform		= glu::isDataTypeAtomicCounter(path.back().getVariableType()->getBasicType());
	const bool isBufferBackedBlockStorage	= path.front().isInterfaceBlock() && isBufferBackedInterfaceBlockStorage(path.front().getInterfaceBlock()->storage);

	DE_UNREF(resource);

	if (!isAtomicCounterUniform && !isBufferBackedBlockStorage)
	{
		// Not buffer backed
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying offset, expecting -1" << tcu::TestLog::EndMessage;

		if (propValue != -1)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid offset, got " << propValue << tcu::TestLog::EndMessage;
			setError("offset invalid");
		}
	}
	else
	{
		// Expect a valid offset
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying offset, expecting a valid offset" << tcu::TestLog::EndMessage;

		if (propValue < 0)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid offset, got " << propValue << tcu::TestLog::EndMessage;
			setError("offset invalid");
		}
	}
}

class VariableReferencedByShaderValidator : public PropValidator
{
public:
								VariableReferencedByShaderValidator	(Context& context, const VariableSearchFilter& searchFilter);

	std::string					getHumanReadablePropertyString		(glw::GLint propVal) const;
	void						validate							(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const;

private:
	const VariableSearchFilter	m_filter;
};

VariableReferencedByShaderValidator::VariableReferencedByShaderValidator (Context& context, const VariableSearchFilter& searchFilter)
	: PropValidator	(context, PROGRAMRESOURCEPROP_REFERENCED_BY_SHADER)
	, m_filter		(searchFilter)
{
}

std::string VariableReferencedByShaderValidator::getHumanReadablePropertyString	(glw::GLint propVal) const
{
	return de::toString(glu::getBooleanStr(propVal));
}

void VariableReferencedByShaderValidator::validate (const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const
{
	std::vector<VariablePathComponent>	dummyPath;
	const bool							referencedByShader = findProgramVariablePathByPathName(dummyPath, program, resource, m_filter);

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying referenced by " << glu::getShaderTypeName(m_filter.getShaderTypeFilter()) << " shader, expecting "
		<< ((referencedByShader) ? ("GL_TRUE") : ("GL_FALSE"))
		<< tcu::TestLog::EndMessage;

	if (propValue != ((referencedByShader) ? (1) : (0)))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid referenced_by_" << glu::getShaderTypeName(m_filter.getShaderTypeFilter()) << ", got " << propValue << tcu::TestLog::EndMessage;
		setError("referenced_by_" + std::string(glu::getShaderTypeName(m_filter.getShaderTypeFilter())) + " invalid");
	}
}

class BlockNameLengthValidator : public SingleBlockValidator
{
public:
			BlockNameLengthValidator	(Context& context, const glw::GLuint programID, const VariableSearchFilter& filter);
	void	validateSingleBlock			(const glu::InterfaceBlock& block, const std::vector<int>& instanceIndex, const std::string& resource, glw::GLint propValue) const;
};

BlockNameLengthValidator::BlockNameLengthValidator (Context& context, const glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleBlockValidator(context, PROGRAMRESOURCEPROP_NAME_LENGTH, programID, filter)
{
}

void BlockNameLengthValidator::validateSingleBlock (const glu::InterfaceBlock& block, const std::vector<int>& instanceIndex, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(instanceIndex);
	DE_UNREF(block);

	const int expected = (int)resource.length() + 1; // includes null byte
	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying name length, expecting " << expected << " (" << (int)resource.length() << " for \"" << resource << "\" + 1 byte for terminating null character)" << tcu::TestLog::EndMessage;

	if (propValue != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid name length, got " << propValue << tcu::TestLog::EndMessage;
		setError("name length invalid");
	}
}

class BufferBindingValidator : public SingleBlockValidator
{
public:
			BufferBindingValidator	(Context& context, const glw::GLuint programID, const VariableSearchFilter& filter);
	void	validateSingleBlock		(const glu::InterfaceBlock& block, const std::vector<int>& instanceIndex, const std::string& resource, glw::GLint propValue) const;
};

BufferBindingValidator::BufferBindingValidator (Context& context, const glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleBlockValidator(context, PROGRAMRESOURCEPROP_BUFFER_BINDING, programID, filter)
{
}

void BufferBindingValidator::validateSingleBlock (const glu::InterfaceBlock& block, const std::vector<int>& instanceIndex, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(resource);

	if (block.layout.binding != -1)
	{
		int flatIndex		= 0;
		int dimensionSize	= 1;

		for (int dimensionNdx = (int)(block.dimensions.size()) - 1; dimensionNdx >= 0; --dimensionNdx)
		{
			flatIndex += dimensionSize * instanceIndex[dimensionNdx];
			dimensionSize *= block.dimensions[dimensionNdx];
		}

		const int expected = (block.dimensions.empty()) ? (block.layout.binding) : (block.layout.binding + flatIndex);
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying block binding, expecting " << expected << tcu::TestLog::EndMessage;

		if (propValue != expected)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid buffer binding, got " << propValue << tcu::TestLog::EndMessage;
			setError("buffer binding invalid");
		}
	}
	else
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying buffer binding, expecting a valid binding" << tcu::TestLog::EndMessage;

		if (propValue < 0)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid buffer binding, got " << propValue << tcu::TestLog::EndMessage;
			setError("buffer binding invalid");
		}
	}
}

class BlockReferencedByShaderValidator : public PropValidator
{
public:
								BlockReferencedByShaderValidator	(Context& context, const VariableSearchFilter& searchFilter);

	std::string					getHumanReadablePropertyString		(glw::GLint propVal) const;
	void						validate							(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const;

private:
	const VariableSearchFilter	m_filter;
};

BlockReferencedByShaderValidator::BlockReferencedByShaderValidator (Context& context, const VariableSearchFilter& searchFilter)
	: PropValidator	(context, PROGRAMRESOURCEPROP_REFERENCED_BY_SHADER)
	, m_filter		(searchFilter)
{
}

std::string BlockReferencedByShaderValidator::getHumanReadablePropertyString	(glw::GLint propVal) const
{
	return de::toString(glu::getBooleanStr(propVal));
}

void BlockReferencedByShaderValidator::validate (const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const
{
	const std::string	blockName			= glu::parseVariableName(resource.c_str());
	bool				referencedByShader	= false;

	DE_UNREF(resource);

	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
	{
		const ProgramInterfaceDefinition::Shader* const shader = program->getShaders()[shaderNdx];
		if (!m_filter.matchesFilter(shader))
			continue;

		for (int blockNdx = 0; blockNdx < (int)shader->getDefaultBlock().interfaceBlocks.size(); ++blockNdx)
		{
			const glu::InterfaceBlock& block = shader->getDefaultBlock().interfaceBlocks[blockNdx];

			if (m_filter.matchesFilter(block) && block.interfaceName == blockName)
				referencedByShader = true;
		}
	}

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying referenced by " << glu::getShaderTypeName(m_filter.getShaderTypeFilter()) << " shader, expecting "
		<< ((referencedByShader) ? ("GL_TRUE") : ("GL_FALSE"))
		<< tcu::TestLog::EndMessage;

	if (propValue != ((referencedByShader) ? (1) : (0)))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid referenced_by_" << glu::getShaderTypeName(m_filter.getShaderTypeFilter()) << ", got " << propValue << tcu::TestLog::EndMessage;
		setError("referenced_by_" + std::string(glu::getShaderTypeName(m_filter.getShaderTypeFilter())) + " invalid");
	}
}

class TopLevelArraySizeValidator : public SingleVariableValidator
{
public:
				TopLevelArraySizeValidator	(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable		(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

TopLevelArraySizeValidator::TopLevelArraySizeValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_TOP_LEVEL_ARRAY_SIZE, programID, filter)
{
}

void TopLevelArraySizeValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	int			expected;
	std::string	reason;

	DE_ASSERT(path.front().isInterfaceBlock() && path.front().getInterfaceBlock()->storage == glu::STORAGE_BUFFER);
	DE_UNREF(resource);

	if (!path[1].getDeclaration()->varType.isArrayType())
	{
		expected = 1;
		reason = "Top-level block member is not an array";
	}
	else if (path[1].getDeclaration()->varType.getElementType().isBasicType())
	{
		expected = 1;
		reason = "Top-level block member is not an array of an aggregate type";
	}
	else if (path[1].getDeclaration()->varType.getArraySize() == glu::VarType::UNSIZED_ARRAY)
	{
		expected = 0;
		reason = "Top-level block member is an unsized top-level array";
	}
	else
	{
		expected = path[1].getDeclaration()->varType.getArraySize();
		reason = "Top-level block member is a sized top-level array";
	}

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying top level array size, expecting " << expected << ". (" << reason << ")." << tcu::TestLog::EndMessage;

	if (propValue != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid top level array size, got " << propValue << tcu::TestLog::EndMessage;
		setError("top level array size invalid");
	}
}

class TopLevelArrayStrideValidator : public SingleVariableValidator
{
public:
				TopLevelArrayStrideValidator	(Context& context, glw::GLuint programID, const VariableSearchFilter& filter);
	void		validateSingleVariable			(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

TopLevelArrayStrideValidator::TopLevelArrayStrideValidator (Context& context, glw::GLuint programID, const VariableSearchFilter& filter)
	: SingleVariableValidator(context, PROGRAMRESOURCEPROP_TOP_LEVEL_ARRAY_STRIDE, programID, filter)
{
}

void TopLevelArrayStrideValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_ASSERT(path.front().isInterfaceBlock() && path.front().getInterfaceBlock()->storage == glu::STORAGE_BUFFER);
	DE_UNREF(resource);

	if (!path[1].getDeclaration()->varType.isArrayType())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying top level array stride, expecting 0. (Top-level block member is not an array)." << tcu::TestLog::EndMessage;

		if (propValue != 0)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, top level array stride, got " << propValue << tcu::TestLog::EndMessage;
			setError("top level array stride invalid");
		}
	}
	else if (path[1].getDeclaration()->varType.getElementType().isBasicType())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying top level array stride, expecting 0. (Top-level block member is not an array of an aggregate type)." << tcu::TestLog::EndMessage;

		if (propValue != 0)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, top level array stride, got " << propValue << tcu::TestLog::EndMessage;
			setError("top level array stride invalid");
		}
	}
	else
	{
		const int minimumStride = getVarTypeSize(path[1].getDeclaration()->varType.getElementType());

		m_testCtx.getLog() << tcu::TestLog::Message << "Verifying top level array stride, expecting greater or equal to " << minimumStride << "." << tcu::TestLog::EndMessage;

		if (propValue < minimumStride)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid top level array stride, got " << propValue << tcu::TestLog::EndMessage;
			setError("top level array stride invalid");
		}
	}
}

class TransformFeedbackResourceValidator : public PropValidator
{
public:
					TransformFeedbackResourceValidator	(Context& context, ProgramResourcePropFlags validationProp);
	void			validate							(const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const;

private:
	virtual void	validateBuiltinVariable				(const std::string& resource, glw::GLint propValue) const = 0;
	virtual void	validateSingleVariable				(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const = 0;
};


TransformFeedbackResourceValidator::TransformFeedbackResourceValidator (Context& context, ProgramResourcePropFlags validationProp)
	: PropValidator(context, validationProp)
{
}

void TransformFeedbackResourceValidator::validate (const ProgramInterfaceDefinition::Program* program, const std::string& resource, glw::GLint propValue) const
{
	if (deStringBeginsWith(resource.c_str(), "gl_"))
	{
		validateBuiltinVariable(resource, propValue);
	}
	else
	{
		// Check resource name is a xfb output. (sanity check)
#if defined(DE_DEBUG)
		bool generatorFound = false;

		// Check the resource name is a valid transform feedback resource and find the name generating resource
		for (int varyingNdx = 0; varyingNdx < (int)program->getTransformFeedbackVaryings().size(); ++varyingNdx)
		{
			const std::string					varyingName = program->getTransformFeedbackVaryings()[varyingNdx];
			std::vector<VariablePathComponent>	path;
			std::vector<std::string>			resources;

			if (!findProgramVariablePathByPathName(path, program, varyingName, VariableSearchFilter(glu::SHADERTYPE_VERTEX, glu::STORAGE_OUT)))
			{
				// program does not contain feedback varying, not valid program
				DE_ASSERT(false);
				return;
			}

			generateVariableTypeResourceNames(resources, varyingName, *path.back().getVariableType(), RESOURCE_NAME_GENERATION_FLAG_TRANSFORM_FEEDBACK_VARIABLE);

			if (de::contains(resources.begin(), resources.end(), resource))
			{
				generatorFound = true;
				break;
			}
		}

		// resource name was not found, should never happen
		DE_ASSERT(generatorFound);
		DE_UNREF(generatorFound);
#endif

		// verify resource
		{
			std::vector<VariablePathComponent> path;

			if (!findProgramVariablePathByPathName(path, program, resource, VariableSearchFilter(glu::SHADERTYPE_VERTEX, glu::STORAGE_OUT)))
				DE_ASSERT(false);

			validateSingleVariable(path, resource, propValue);
		}
	}
}

class TransformFeedbackArraySizeValidator : public TransformFeedbackResourceValidator
{
public:
				TransformFeedbackArraySizeValidator	(Context& context);

	void		validateBuiltinVariable				(const std::string& resource, glw::GLint propValue) const;
	void		validateSingleVariable				(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

TransformFeedbackArraySizeValidator::TransformFeedbackArraySizeValidator (Context& context)
	: TransformFeedbackResourceValidator(context, PROGRAMRESOURCEPROP_ARRAY_SIZE)
{
}

void TransformFeedbackArraySizeValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	int arraySize = 0;

	if (resource == "gl_Position")
		arraySize = 1;
	else
		DE_ASSERT(false);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array size, expecting " << arraySize << tcu::TestLog::EndMessage;
	if (arraySize != propValue)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
		setError("resource array size invalid");
	}
}

void TransformFeedbackArraySizeValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(resource);

	const int arraySize = (path.back().getVariableType()->isArrayType()) ? (path.back().getVariableType()->getArraySize()) : (1);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying array size, expecting " << arraySize << tcu::TestLog::EndMessage;
	if (arraySize != propValue)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << propValue << tcu::TestLog::EndMessage;
		setError("resource array size invalid");
	}
}

class TransformFeedbackNameLengthValidator : public TransformFeedbackResourceValidator
{
public:
				TransformFeedbackNameLengthValidator	(Context& context);

private:
	void		validateBuiltinVariable					(const std::string& resource, glw::GLint propValue) const;
	void		validateSingleVariable					(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
	void		validateVariable						(const std::string& resource, glw::GLint propValue) const;
};

TransformFeedbackNameLengthValidator::TransformFeedbackNameLengthValidator (Context& context)
	: TransformFeedbackResourceValidator(context, PROGRAMRESOURCEPROP_NAME_LENGTH)
{
}

void TransformFeedbackNameLengthValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	validateVariable(resource, propValue);
}

void TransformFeedbackNameLengthValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(path);
	validateVariable(resource, propValue);
}

void TransformFeedbackNameLengthValidator::validateVariable (const std::string& resource, glw::GLint propValue) const
{
	const int expected = (int)resource.length() + 1; // includes null byte
	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying name length, expecting " << expected << " (" << (int)resource.length() << " for \"" << resource << "\" + 1 byte for terminating null character)" << tcu::TestLog::EndMessage;

	if (propValue != expected)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, invalid name length, got " << propValue << tcu::TestLog::EndMessage;
		setError("name length invalid");
	}
}

class TransformFeedbackTypeValidator : public TransformFeedbackResourceValidator
{
public:
				TransformFeedbackTypeValidator		(Context& context);

	void		validateBuiltinVariable				(const std::string& resource, glw::GLint propValue) const;
	void		validateSingleVariable				(const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const;
};

TransformFeedbackTypeValidator::TransformFeedbackTypeValidator (Context& context)
	: TransformFeedbackResourceValidator(context, PROGRAMRESOURCEPROP_TYPE)
{
}

void TransformFeedbackTypeValidator::validateBuiltinVariable (const std::string& resource, glw::GLint propValue) const
{
	glu::DataType varType = glu::TYPE_INVALID;

	if (resource == "gl_Position")
		varType = glu::TYPE_FLOAT_VEC4;
	else
		DE_ASSERT(false);

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying type, expecting " << glu::getDataTypeName(varType) << tcu::TestLog::EndMessage;
	if (glu::getDataTypeFromGLType(propValue) != varType)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << glu::getDataTypeName(glu::getDataTypeFromGLType(propValue)) << tcu::TestLog::EndMessage;
		setError("resource type invalid");
	}
	return;
}

void TransformFeedbackTypeValidator::validateSingleVariable (const std::vector<VariablePathComponent>& path, const std::string& resource, glw::GLint propValue) const
{
	DE_UNREF(resource);

	// Unlike other interfaces, xfb program interface uses just variable name to refer to arrays of basic types. (Others use "variable[0]")
	// Thus we might end up querying a type for an array. In this case, return the type of an array element.
	const glu::VarType& variable    = *path.back().getVariableType();
	const glu::VarType& elementType = (variable.isArrayType()) ? (variable.getElementType()) : (variable);

	DE_ASSERT(elementType.isBasicType());

	m_testCtx.getLog() << tcu::TestLog::Message << "Verifying type, expecting " << glu::getDataTypeName(elementType.getBasicType()) << tcu::TestLog::EndMessage;
	if (elementType.getBasicType() != glu::getDataTypeFromGLType(propValue))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "\tError, got " << glu::getDataTypeName(glu::getDataTypeFromGLType(propValue)) << tcu::TestLog::EndMessage;
		setError("resource type invalid");
	}
}

} // anonymous

ProgramResourceQueryTestTarget::ProgramResourceQueryTestTarget (ProgramInterface interface_, deUint32 propFlags_)
	: interface(interface_)
	, propFlags(propFlags_)
{
	switch (interface)
	{
		case PROGRAMINTERFACE_UNIFORM:						DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_UNIFORM_INTERFACE_MASK)			== propFlags);	break;
		case PROGRAMINTERFACE_UNIFORM_BLOCK:				DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_UNIFORM_BLOCK_INTERFACE_MASK)	== propFlags);	break;
		case PROGRAMINTERFACE_SHADER_STORAGE_BLOCK:			DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_SHADER_STORAGE_BLOCK_MASK)		== propFlags);	break;
		case PROGRAMINTERFACE_PROGRAM_INPUT:				DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_PROGRAM_INPUT_MASK)				== propFlags);	break;
		case PROGRAMINTERFACE_PROGRAM_OUTPUT:				DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_PROGRAM_OUTPUT_MASK)				== propFlags);	break;
		case PROGRAMINTERFACE_BUFFER_VARIABLE:				DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_BUFFER_VARIABLE_MASK)			== propFlags);	break;
		case PROGRAMINTERFACE_TRANSFORM_FEEDBACK_VARYING:	DE_ASSERT((propFlags & PROGRAMRESOURCEPROP_TRANSFORM_FEEDBACK_VARYING_MASK)	== propFlags);	break;

		default:
			DE_ASSERT(false);
	}
}

ProgramInterfaceQueryTestCase::ProgramInterfaceQueryTestCase (Context& context, const char* name, const char* description, ProgramResourceQueryTestTarget queryTarget)
	: TestCase		(context, name, description)
	, m_queryTarget	(queryTarget)
{
}

ProgramInterfaceQueryTestCase::~ProgramInterfaceQueryTestCase (void)
{
}

ProgramInterface ProgramInterfaceQueryTestCase::getTargetInterface (void) const
{
	return m_queryTarget.interface;
}

static glw::GLenum getGLInterfaceEnumValue (ProgramInterface interface)
{
	switch (interface)
	{
		case PROGRAMINTERFACE_UNIFORM:						return GL_UNIFORM;
		case PROGRAMINTERFACE_UNIFORM_BLOCK:				return GL_UNIFORM_BLOCK;
		case PROGRAMINTERFACE_ATOMIC_COUNTER_BUFFER:		return GL_ATOMIC_COUNTER_BUFFER;
		case PROGRAMINTERFACE_PROGRAM_INPUT:				return GL_PROGRAM_INPUT;
		case PROGRAMINTERFACE_PROGRAM_OUTPUT:				return GL_PROGRAM_OUTPUT;
		case PROGRAMINTERFACE_TRANSFORM_FEEDBACK_VARYING:	return GL_TRANSFORM_FEEDBACK_VARYING;
		case PROGRAMINTERFACE_BUFFER_VARIABLE:				return GL_BUFFER_VARIABLE;
		case PROGRAMINTERFACE_SHADER_STORAGE_BLOCK:			return GL_SHADER_STORAGE_BLOCK;
		default:
			DE_ASSERT(false);
			return 0;
	};
}

static void queryAndValidateProps (tcu::TestContext& testCtx, const glw::Functions& gl, glw::GLuint programID, ProgramInterface interface, const char* targetResourceName, const ProgramInterfaceDefinition::Program* programDefinition, const std::vector<glw::GLenum>& props, const std::vector<const PropValidator*>& validators)
{
	const glw::GLenum			glInterface		= getGLInterfaceEnumValue(interface);
	glw::GLuint					resourceNdx;
	glw::GLint					written			= -1;
	std::vector<glw::GLint>		propValues		(props.size() + 1, -2);	// prefill result buffer with an invalid value. -1 might be valid sometimes, avoid it. Make buffer one larger to allow detection of too many return values

	DE_ASSERT(props.size() == validators.size());

	// query

	resourceNdx = gl.getProgramResourceIndex(programID, glInterface, targetResourceName);
	GLU_EXPECT_NO_ERROR(gl.getError(), "get resource index");

	if (resourceNdx == GL_INVALID_INDEX)
	{
		testCtx.getLog() << tcu::TestLog::Message << "getProgramResourceIndex returned GL_INVALID_INDEX for \"" << targetResourceName << "\"" << tcu::TestLog::EndMessage;
		testCtx.setTestResult(QP_TEST_RESULT_FAIL, "could not find target resource");

		// try to recover but keep the test result as failure
		{
			const std::string	resourceName			= std::string(targetResourceName);
			std::string			simplifiedResourceName;

			if (stringEndsWith(resourceName, "[0]"))
				simplifiedResourceName = resourceName.substr(0, resourceName.length() - 3);
			else
			{
				const size_t lastMember = resourceName.find_last_of('.');
				if (lastMember != std::string::npos)
					simplifiedResourceName = resourceName.substr(0, lastMember);
			}

			if (simplifiedResourceName.empty())
				return;

			resourceNdx = gl.getProgramResourceIndex(programID, glInterface, simplifiedResourceName.c_str());
			GLU_EXPECT_NO_ERROR(gl.getError(), "get resource index");

			if (resourceNdx == GL_INVALID_INDEX)
				return;

			testCtx.getLog() << tcu::TestLog::Message << "\tResource not found, continuing anyway using index obtained for resource \"" << simplifiedResourceName << "\"" << tcu::TestLog::EndMessage;
		}
	}

	gl.getProgramResourceiv(programID, glInterface, resourceNdx, (int)props.size(), &props[0], (int)propValues.size(), &written, &propValues[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "get props");

	if (written != (int)props.size())
	{
		testCtx.getLog() << tcu::TestLog::Message << "getProgramResourceiv returned unexpected number of values, expected " << (int)props.size() << ", got " << written << tcu::TestLog::EndMessage;
		testCtx.setTestResult(QP_TEST_RESULT_FAIL, "getProgramResourceiv returned unexpected number of values");
		return;
	}

	if (propValues.back() != -2)
	{
		testCtx.getLog() << tcu::TestLog::Message << "getProgramResourceiv post write buffer guard value was modified, too many return values" << tcu::TestLog::EndMessage;
		testCtx.setTestResult(QP_TEST_RESULT_FAIL, "getProgramResourceiv returned unexpected number of values");
		return;
	}
	propValues.pop_back();
	DE_ASSERT(validators.size() == propValues.size());

	// log

	{
		tcu::MessageBuilder message(&testCtx.getLog());
		message << "For resource index " << resourceNdx << " (\"" << targetResourceName << "\") got following properties:\n";

		for (int propNdx = 0; propNdx < (int)propValues.size(); ++propNdx)
			message << "\t" << glu::getProgramResourcePropertyName(props[propNdx]) << ":\t" << validators[propNdx]->getHumanReadablePropertyString(propValues[propNdx]) << "\n";

		message << tcu::TestLog::EndMessage;
	}

	// validate

	for (int propNdx = 0; propNdx < (int)propValues.size(); ++propNdx)
		validators[propNdx]->validate(programDefinition, targetResourceName, propValues[propNdx]);
}

ProgramInterfaceQueryTestCase::IterateResult ProgramInterfaceQueryTestCase::iterate (void)
{
	struct TestProperty
	{
		glw::GLenum				prop;
		const PropValidator*	validator;
	};

	const ProgramInterfaceDefinition::Program*	programDefinition	= getProgramDefinition();
	const std::vector<std::string>				targetResources		= getQueryTargetResources();
	glu::ShaderProgram							program				(m_context.getRenderContext(), generateProgramInterfaceProgramSources(programDefinition));

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	DE_ASSERT(programDefinition->isValid());

	// Log program
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "Program", "Program");

		// Feedback varyings
		if (!programDefinition->getTransformFeedbackVaryings().empty())
		{
			tcu::MessageBuilder builder(&m_testCtx.getLog());
			builder << "Transform feedback varyings: {";
			for (int ndx = 0; ndx < (int)programDefinition->getTransformFeedbackVaryings().size(); ++ndx)
			{
				if (ndx)
					builder << ", ";
				builder << "\"" << programDefinition->getTransformFeedbackVaryings()[ndx] << "\"";
			}
			builder << "}" << tcu::TestLog::EndMessage;
		}

		m_testCtx.getLog() << program;
		if (!program.isOk())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Program build failed, checking if program exceeded implementation limits" << tcu::TestLog::EndMessage;
			checkProgramResourceUsage(programDefinition, m_context.getRenderContext().getFunctions(), m_testCtx.getLog());

			// within limits
			throw tcu::TestError("could not build program");
		}
	}

	// Check interface props

	switch (m_queryTarget.interface)
	{
		case PROGRAMINTERFACE_UNIFORM:
		{
			const VariableSearchFilter					uniformFilter						(glu::SHADERTYPE_LAST, glu::STORAGE_UNIFORM);

			const TypeValidator							typeValidator						(m_context, program.getProgram(), uniformFilter);
			const ArraySizeValidator					arraySizeValidator					(m_context, program.getProgram(), uniformFilter);
			const ArrayStrideValidator					arrayStrideValidator				(m_context, program.getProgram(), uniformFilter);
			const BlockIndexValidator					blockIndexValidator					(m_context, program.getProgram(), uniformFilter);
			const IsRowMajorValidator					isRowMajorValidator					(m_context, program.getProgram(), uniformFilter);
			const MatrixStrideValidator					matrixStrideValidator				(m_context, program.getProgram(), uniformFilter);
			const AtomicCounterBufferIndexVerifier		atomicCounterBufferIndexVerifier	(m_context, program.getProgram(), uniformFilter);
			const LocationValidator						locationValidator					(m_context, program.getProgram(), uniformFilter);
			const VariableNameLengthValidator			nameLengthValidator					(m_context, program.getProgram(), uniformFilter);
			const OffsetValidator						offsetVerifier						(m_context, program.getProgram(), uniformFilter);
			const VariableReferencedByShaderValidator	referencedByVertexVerifier			(m_context, VariableSearchFilter(glu::SHADERTYPE_VERTEX,	glu::STORAGE_UNIFORM));
			const VariableReferencedByShaderValidator	referencedByFragmentVerifier		(m_context, VariableSearchFilter(glu::SHADERTYPE_FRAGMENT,	glu::STORAGE_UNIFORM));
			const VariableReferencedByShaderValidator	referencedByComputeVerifier			(m_context, VariableSearchFilter(glu::SHADERTYPE_COMPUTE,	glu::STORAGE_UNIFORM));

			const TestProperty allProperties[] =
			{
				{ GL_ARRAY_SIZE,					&arraySizeValidator					},
				{ GL_ARRAY_STRIDE,					&arrayStrideValidator				},
				{ GL_ATOMIC_COUNTER_BUFFER_INDEX,	&atomicCounterBufferIndexVerifier	},
				{ GL_BLOCK_INDEX,					&blockIndexValidator				},
				{ GL_IS_ROW_MAJOR,					&isRowMajorValidator				},
				{ GL_LOCATION,						&locationValidator					},
				{ GL_MATRIX_STRIDE,					&matrixStrideValidator				},
				{ GL_NAME_LENGTH,					&nameLengthValidator				},
				{ GL_OFFSET,						&offsetVerifier						},
				{ GL_REFERENCED_BY_VERTEX_SHADER,	&referencedByVertexVerifier			},
				{ GL_REFERENCED_BY_FRAGMENT_SHADER,	&referencedByFragmentVerifier		},
				{ GL_REFERENCED_BY_COMPUTE_SHADER,	&referencedByComputeVerifier		},
				{ GL_TYPE,							&typeValidator						},
			};

			for (int targetResourceNdx = 0; targetResourceNdx < (int)targetResources.size(); ++targetResourceNdx)
			{
				const tcu::ScopedLogSection			section			(m_testCtx.getLog(), "UniformResource", "Uniform resource \"" +  targetResources[targetResourceNdx] + "\"");
				const glw::Functions&				gl				= m_context.getRenderContext().getFunctions();
				std::vector<glw::GLenum>			props;
				std::vector<const PropValidator*>	validators;

				for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(allProperties); ++propNdx)
				{
					if (allProperties[propNdx].validator->isSelected(m_queryTarget.propFlags) &&
						allProperties[propNdx].validator->isSupported())
					{
						props.push_back(allProperties[propNdx].prop);
						validators.push_back(allProperties[propNdx].validator);
					}
				}

				DE_ASSERT(!props.empty());

				queryAndValidateProps(m_testCtx, gl, program.getProgram(), m_queryTarget.interface, targetResources[targetResourceNdx].c_str(), programDefinition, props, validators);
			}

			break;
		}

		case PROGRAMINTERFACE_UNIFORM_BLOCK:
		case PROGRAMINTERFACE_SHADER_STORAGE_BLOCK:
		{
			const glu::Storage						storage								= (m_queryTarget.interface == PROGRAMINTERFACE_UNIFORM_BLOCK) ? (glu::STORAGE_UNIFORM) : (glu::STORAGE_BUFFER);
			const VariableSearchFilter				blockFilter							(glu::SHADERTYPE_LAST, storage);

			const BlockNameLengthValidator			nameLengthValidator					(m_context, program.getProgram(), blockFilter);
			const BlockReferencedByShaderValidator	referencedByVertexVerifier			(m_context, VariableSearchFilter(glu::SHADERTYPE_VERTEX,	storage));
			const BlockReferencedByShaderValidator	referencedByFragmentVerifier		(m_context, VariableSearchFilter(glu::SHADERTYPE_FRAGMENT,	storage));
			const BlockReferencedByShaderValidator	referencedByComputeVerifier			(m_context, VariableSearchFilter(glu::SHADERTYPE_COMPUTE,	storage));
			const BufferBindingValidator			bufferBindingValidator				(m_context, program.getProgram(), blockFilter);

			const TestProperty allProperties[] =
			{
				{ GL_NAME_LENGTH,					&nameLengthValidator				},
				{ GL_REFERENCED_BY_VERTEX_SHADER,	&referencedByVertexVerifier			},
				{ GL_REFERENCED_BY_FRAGMENT_SHADER,	&referencedByFragmentVerifier		},
				{ GL_REFERENCED_BY_COMPUTE_SHADER,	&referencedByComputeVerifier		},
				{ GL_BUFFER_BINDING,				&bufferBindingValidator				},
			};

			for (int targetResourceNdx = 0; targetResourceNdx < (int)targetResources.size(); ++targetResourceNdx)
			{
				const tcu::ScopedLogSection			section			(m_testCtx.getLog(), "BlockResource", "Interface block \"" +  targetResources[targetResourceNdx] + "\"");
				const glw::Functions&				gl				= m_context.getRenderContext().getFunctions();
				std::vector<glw::GLenum>			props;
				std::vector<const PropValidator*>	validators;

				for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(allProperties); ++propNdx)
				{
					if (allProperties[propNdx].validator->isSelected(m_queryTarget.propFlags) &&
						allProperties[propNdx].validator->isSupported())
					{
						props.push_back(allProperties[propNdx].prop);
						validators.push_back(allProperties[propNdx].validator);
					}
				}

				DE_ASSERT(!props.empty());

				queryAndValidateProps(m_testCtx, gl, program.getProgram(), m_queryTarget.interface, targetResources[targetResourceNdx].c_str(), programDefinition, props, validators);
			}

			break;
		}

		case PROGRAMINTERFACE_PROGRAM_INPUT:
		case PROGRAMINTERFACE_PROGRAM_OUTPUT:
		{
			const glu::Storage							storage							= (m_queryTarget.interface == PROGRAMINTERFACE_PROGRAM_INPUT) ? (glu::STORAGE_IN) : (glu::STORAGE_OUT);
			const glu::ShaderType						shaderType						= (m_queryTarget.interface == PROGRAMINTERFACE_PROGRAM_INPUT) ? (programDefinition->getFirstStage()) : (programDefinition->getLastStage());
			const VariableSearchFilter					variableFilter					(shaderType, storage);

			const TypeValidator							typeValidator					(m_context, program.getProgram(), variableFilter);
			const ArraySizeValidator					arraySizeValidator				(m_context, program.getProgram(), variableFilter);
			const LocationValidator						locationValidator				(m_context, program.getProgram(), variableFilter);
			const VariableNameLengthValidator			nameLengthValidator				(m_context, program.getProgram(), variableFilter);
			const VariableReferencedByShaderValidator	referencedByVertexVerifier		(m_context, VariableSearchFilter::intersection(VariableSearchFilter(glu::SHADERTYPE_VERTEX,		storage), variableFilter));
			const VariableReferencedByShaderValidator	referencedByFragmentVerifier	(m_context, VariableSearchFilter::intersection(VariableSearchFilter(glu::SHADERTYPE_FRAGMENT,	storage), variableFilter));
			const VariableReferencedByShaderValidator	referencedByComputeVerifier		(m_context, VariableSearchFilter::intersection(VariableSearchFilter(glu::SHADERTYPE_COMPUTE,	storage), variableFilter));

			const TestProperty allProperties[] =
			{
				{ GL_ARRAY_SIZE,					&arraySizeValidator				},
				{ GL_LOCATION,						&locationValidator				},
				{ GL_NAME_LENGTH,					&nameLengthValidator			},
				{ GL_REFERENCED_BY_VERTEX_SHADER,	&referencedByVertexVerifier		},
				{ GL_REFERENCED_BY_FRAGMENT_SHADER,	&referencedByFragmentVerifier	},
				{ GL_REFERENCED_BY_COMPUTE_SHADER,	&referencedByComputeVerifier	},
				{ GL_TYPE,							&typeValidator					},
			};

			for (int targetResourceNdx = 0; targetResourceNdx < (int)targetResources.size(); ++targetResourceNdx)
			{
				const std::string					resourceInterfaceName	= (m_queryTarget.interface == PROGRAMINTERFACE_PROGRAM_INPUT) ? ("Input") : ("Output");
				const tcu::ScopedLogSection			section					(m_testCtx.getLog(), "BlockResource", resourceInterfaceName + " resource \"" +  targetResources[targetResourceNdx] + "\"");
				const glw::Functions&				gl						= m_context.getRenderContext().getFunctions();
				std::vector<glw::GLenum>			props;
				std::vector<const PropValidator*>	validators;

				for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(allProperties); ++propNdx)
				{
					if (allProperties[propNdx].validator->isSelected(m_queryTarget.propFlags) &&
						allProperties[propNdx].validator->isSupported())
					{
						props.push_back(allProperties[propNdx].prop);
						validators.push_back(allProperties[propNdx].validator);
					}
				}

				DE_ASSERT(!props.empty());

				queryAndValidateProps(m_testCtx, gl, program.getProgram(), m_queryTarget.interface, targetResources[targetResourceNdx].c_str(), programDefinition, props, validators);
			}

			break;
		}

		case PROGRAMINTERFACE_BUFFER_VARIABLE:
		{
			const VariableSearchFilter					variableFilter					(glu::SHADERTYPE_LAST, glu::STORAGE_BUFFER);

			const TypeValidator							typeValidator					(m_context, program.getProgram(), variableFilter);
			const ArraySizeValidator					arraySizeValidator				(m_context, program.getProgram(), variableFilter);
			const ArrayStrideValidator					arrayStrideValidator			(m_context, program.getProgram(), variableFilter);
			const BlockIndexValidator					blockIndexValidator				(m_context, program.getProgram(), variableFilter);
			const IsRowMajorValidator					isRowMajorValidator				(m_context, program.getProgram(), variableFilter);
			const MatrixStrideValidator					matrixStrideValidator			(m_context, program.getProgram(), variableFilter);
			const OffsetValidator						offsetValidator					(m_context, program.getProgram(), variableFilter);
			const VariableNameLengthValidator			nameLengthValidator				(m_context, program.getProgram(), variableFilter);
			const VariableReferencedByShaderValidator	referencedByVertexVerifier		(m_context, VariableSearchFilter(glu::SHADERTYPE_VERTEX,	glu::STORAGE_BUFFER));
			const VariableReferencedByShaderValidator	referencedByFragmentVerifier	(m_context, VariableSearchFilter(glu::SHADERTYPE_FRAGMENT,	glu::STORAGE_BUFFER));
			const VariableReferencedByShaderValidator	referencedByComputeVerifier		(m_context, VariableSearchFilter(glu::SHADERTYPE_COMPUTE,	glu::STORAGE_BUFFER));
			const TopLevelArraySizeValidator			topLevelArraySizeValidator		(m_context, program.getProgram(), variableFilter);
			const TopLevelArrayStrideValidator			topLevelArrayStrideValidator	(m_context, program.getProgram(), variableFilter);

			const TestProperty allProperties[] =
			{
				{ GL_ARRAY_SIZE,					&arraySizeValidator				},
				{ GL_ARRAY_STRIDE,					&arrayStrideValidator			},
				{ GL_BLOCK_INDEX,					&blockIndexValidator			},
				{ GL_IS_ROW_MAJOR,					&isRowMajorValidator			},
				{ GL_MATRIX_STRIDE,					&matrixStrideValidator			},
				{ GL_NAME_LENGTH,					&nameLengthValidator			},
				{ GL_OFFSET,						&offsetValidator				},
				{ GL_REFERENCED_BY_VERTEX_SHADER,	&referencedByVertexVerifier		},
				{ GL_REFERENCED_BY_FRAGMENT_SHADER,	&referencedByFragmentVerifier	},
				{ GL_REFERENCED_BY_COMPUTE_SHADER,	&referencedByComputeVerifier	},
				{ GL_TOP_LEVEL_ARRAY_SIZE,			&topLevelArraySizeValidator		},
				{ GL_TOP_LEVEL_ARRAY_STRIDE,		&topLevelArrayStrideValidator	},
				{ GL_TYPE,							&typeValidator					},
			};

			for (int targetResourceNdx = 0; targetResourceNdx < (int)targetResources.size(); ++targetResourceNdx)
			{
				const tcu::ScopedLogSection			section			(m_testCtx.getLog(), "BufferVariableResource", "Buffer variable \"" +  targetResources[targetResourceNdx] + "\"");
				const glw::Functions&				gl				= m_context.getRenderContext().getFunctions();
				std::vector<glw::GLenum>			props;
				std::vector<const PropValidator*>	validators;

				for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(allProperties); ++propNdx)
				{
					if (allProperties[propNdx].validator->isSelected(m_queryTarget.propFlags) &&
						allProperties[propNdx].validator->isSupported())
					{
						props.push_back(allProperties[propNdx].prop);
						validators.push_back(allProperties[propNdx].validator);
					}
				}

				DE_ASSERT(!props.empty());

				queryAndValidateProps(m_testCtx, gl, program.getProgram(), m_queryTarget.interface, targetResources[targetResourceNdx].c_str(), programDefinition, props, validators);
			}

			break;
		}

		case PROGRAMINTERFACE_TRANSFORM_FEEDBACK_VARYING:
		{
			const TransformFeedbackTypeValidator		typeValidator			(m_context);
			const TransformFeedbackArraySizeValidator	arraySizeValidator		(m_context);
			const TransformFeedbackNameLengthValidator	nameLengthValidator		(m_context);

			const TestProperty allProperties[] =
			{
				{ GL_ARRAY_SIZE,					&arraySizeValidator				},
				{ GL_NAME_LENGTH,					&nameLengthValidator			},
				{ GL_TYPE,							&typeValidator					},
			};

			for (int targetResourceNdx = 0; targetResourceNdx < (int)targetResources.size(); ++targetResourceNdx)
			{
				const tcu::ScopedLogSection			section			(m_testCtx.getLog(), "XFBVariableResource", "Transform feedback varying \"" +  targetResources[targetResourceNdx] + "\"");
				const glw::Functions&				gl				= m_context.getRenderContext().getFunctions();
				std::vector<glw::GLenum>			props;
				std::vector<const PropValidator*>	validators;

				for (int propNdx = 0; propNdx < DE_LENGTH_OF_ARRAY(allProperties); ++propNdx)
				{
					if (allProperties[propNdx].validator->isSelected(m_queryTarget.propFlags) &&
						allProperties[propNdx].validator->isSupported())
					{
						props.push_back(allProperties[propNdx].prop);
						validators.push_back(allProperties[propNdx].validator);
					}
				}

				DE_ASSERT(!props.empty());

				queryAndValidateProps(m_testCtx, gl, program.getProgram(), m_queryTarget.interface, targetResources[targetResourceNdx].c_str(), programDefinition, props, validators);
			}

			break;
		}

		default:
			DE_ASSERT(false);
	}

	return STOP;
}

static bool checkLimit (glw::GLenum pname, int usage, const glw::Functions& gl, tcu::TestLog& log)
{
	if (usage > 0)
	{
		glw::GLint limit = 0;
		gl.getIntegerv(pname, &limit);
		GLU_EXPECT_NO_ERROR(gl.getError(), "query limits");

		log << tcu::TestLog::Message << "\t" << glu::getGettableStateStr(pname) << " = " << limit << ", test requires " << usage << tcu::TestLog::EndMessage;

		if (limit < usage)
		{
			log << tcu::TestLog::Message << "\t\tLimit exceeded" << tcu::TestLog::EndMessage;
			return false;
		}
	}

	return true;
}

static bool checkShaderResourceUsage (const ProgramInterfaceDefinition::Shader* shader, const glw::Functions& gl, tcu::TestLog& log)
{
	const ProgramInterfaceDefinition::ShaderResourceUsage usage = getShaderResourceUsage(shader);

	switch (shader->getType())
	{
		case glu::SHADERTYPE_VERTEX:
		{
			const struct
			{
				glw::GLenum	pname;
				int			usage;
			} restrictions[] =
			{
				{ GL_MAX_VERTEX_ATTRIBS,						usage.numInputVectors					},
				{ GL_MAX_VERTEX_UNIFORM_COMPONENTS,				usage.numDefaultBlockUniformComponents	},
				{ GL_MAX_VERTEX_UNIFORM_VECTORS,				usage.numUniformVectors					},
				{ GL_MAX_VERTEX_UNIFORM_BLOCKS,					usage.numUniformBlocks					},
				{ GL_MAX_VERTEX_OUTPUT_COMPONENTS,				usage.numOutputComponents				},
				{ GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,			usage.numSamplers						},
				{ GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS,			usage.numAtomicCounterBuffers			},
				{ GL_MAX_VERTEX_ATOMIC_COUNTERS,				usage.numAtomicCounters					},
				{ GL_MAX_VERTEX_IMAGE_UNIFORMS,					usage.numImages							},
				{ GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,	usage.numCombinedUniformComponents		},
				{ GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS,			usage.numShaderStorageBlocks			},
			};

			bool allOk = true;

			log << tcu::TestLog::Message << "Vertex shader:" << tcu::TestLog::EndMessage;
			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(restrictions); ++ndx)
				allOk &= checkLimit(restrictions[ndx].pname, restrictions[ndx].usage, gl, log);

			return allOk;
		}

		case glu::SHADERTYPE_FRAGMENT:
		{
			const struct
			{
				glw::GLenum	pname;
				int			usage;
			} restrictions[] =
			{
				{ GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,			usage.numDefaultBlockUniformComponents		},
				{ GL_MAX_FRAGMENT_UNIFORM_VECTORS,				usage.numUniformVectors						},
				{ GL_MAX_FRAGMENT_UNIFORM_BLOCKS,				usage.numUniformBlocks						},
				{ GL_MAX_FRAGMENT_INPUT_COMPONENTS,				usage.numInputComponents					},
				{ GL_MAX_TEXTURE_IMAGE_UNITS,					usage.numSamplers							},
				{ GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS,		usage.numAtomicCounterBuffers				},
				{ GL_MAX_FRAGMENT_ATOMIC_COUNTERS,				usage.numAtomicCounters						},
				{ GL_MAX_FRAGMENT_IMAGE_UNIFORMS,				usage.numImages								},
				{ GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,	usage.numCombinedUniformComponents			},
				{ GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS,		usage.numShaderStorageBlocks				},
			};

			bool allOk = true;

			log << tcu::TestLog::Message << "Fragment shader:" << tcu::TestLog::EndMessage;
			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(restrictions); ++ndx)
				allOk &= checkLimit(restrictions[ndx].pname, restrictions[ndx].usage, gl, log);

			return allOk;
		}

		case glu::SHADERTYPE_COMPUTE:
		{
			const struct
			{
				glw::GLenum	pname;
				int			usage;
			} restrictions[] =
			{
				{ GL_MAX_COMPUTE_UNIFORM_BLOCKS,				usage.numUniformBlocks					},
				{ GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS,			usage.numSamplers						},
				{ GL_MAX_COMPUTE_UNIFORM_COMPONENTS,			usage.numDefaultBlockUniformComponents	},
				{ GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS,		usage.numAtomicCounterBuffers			},
				{ GL_MAX_COMPUTE_ATOMIC_COUNTERS,				usage.numAtomicCounters					},
				{ GL_MAX_COMPUTE_IMAGE_UNIFORMS,				usage.numImages							},
				{ GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS,	usage.numCombinedUniformComponents		},
				{ GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,			usage.numShaderStorageBlocks			},
			};

			bool allOk = true;

			log << tcu::TestLog::Message << "Compute shader:" << tcu::TestLog::EndMessage;
			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(restrictions); ++ndx)
				allOk &= checkLimit(restrictions[ndx].pname, restrictions[ndx].usage, gl, log);

			return allOk;
		}

		default:
			DE_ASSERT(false);
			return false;
	}
}

static bool checkProgramCombinedResourceUsage (const ProgramInterfaceDefinition::Program* program, const glw::Functions& gl, tcu::TestLog& log)
{
	const ProgramInterfaceDefinition::ProgramResourceUsage usage = getCombinedProgramResourceUsage(program);

	const struct
	{
		glw::GLenum	pname;
		int			usage;
	} restrictions[] =
	{
		{ GL_MAX_UNIFORM_BUFFER_BINDINGS,						usage.uniformBufferMaxBinding+1				},
		{ GL_MAX_UNIFORM_BLOCK_SIZE,							usage.uniformBufferMaxSize					},
		{ GL_MAX_COMBINED_UNIFORM_BLOCKS,						usage.numUniformBlocks						},
		{ GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,			usage.numCombinedVertexUniformComponents	},
		{ GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,			usage.numCombinedFragmentUniformComponents	},
		{ GL_MAX_VARYING_COMPONENTS,							usage.numVaryingComponents					},
		{ GL_MAX_VARYING_VECTORS,								usage.numVaryingVectors						},
		{ GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,					usage.numCombinedSamplers					},
		{ GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES,				usage.numCombinedOutputResources			},
		{ GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,				usage.atomicCounterBufferMaxBinding+1		},
		{ GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE,					usage.atomicCounterBufferMaxSize			},
		{ GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS,				usage.numAtomicCounterBuffers				},
		{ GL_MAX_COMBINED_ATOMIC_COUNTERS,						usage.numAtomicCounters						},
		{ GL_MAX_IMAGE_UNITS,									usage.maxImageBinding+1						},
		{ GL_MAX_COMBINED_IMAGE_UNIFORMS,						usage.numCombinedImages						},
		{ GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,				usage.shaderStorageBufferMaxBinding+1		},
		{ GL_MAX_SHADER_STORAGE_BLOCK_SIZE,						usage.shaderStorageBufferMaxSize			},
		{ GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS,				usage.numShaderStorageBlocks				},
		{ GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,		usage.numXFBInterleavedComponents			},
		{ GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,			usage.numXFBSeparateAttribs					},
		{ GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,		usage.numXFBSeparateComponents				},
		{ GL_MAX_DRAW_BUFFERS,									usage.fragmentOutputMaxBinding+1			},
	};

	bool allOk = true;

	log << tcu::TestLog::Message << "Program combined:" << tcu::TestLog::EndMessage;
	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(restrictions); ++ndx)
		allOk &= checkLimit(restrictions[ndx].pname, restrictions[ndx].usage, gl, log);

	return allOk;
}

void checkProgramResourceUsage (const ProgramInterfaceDefinition::Program* program, const glw::Functions& gl, tcu::TestLog& log)
{
	bool limitExceeded = false;

	for (int shaderNdx = 0; shaderNdx < (int)program->getShaders().size(); ++shaderNdx)
		limitExceeded |= !checkShaderResourceUsage(program->getShaders()[shaderNdx], gl, log);

	limitExceeded |= !checkProgramCombinedResourceUsage(program, gl, log);

	if (limitExceeded)
	{
		log << tcu::TestLog::Message << "One or more resource limits exceeded" << tcu::TestLog::EndMessage;
		throw tcu::NotSupportedError("one or more resource limits exceeded");
	}
}

} // Functional
} // gles31
} // deqp
