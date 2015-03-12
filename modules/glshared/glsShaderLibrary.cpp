/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Compiler test case.
 *//*--------------------------------------------------------------------*/

#include "glsShaderLibrary.hpp"
#include "glsShaderLibraryCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuResource.hpp"
#include "glwEnums.hpp"

#include "deInt32.h"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

using std::string;
using std::vector;
using std::ostringstream;

using namespace glu;

#if 0
#	define PARSE_DBG(X) printf X
#else
#	define PARSE_DBG(X) DE_NULL_STATEMENT
#endif

namespace deqp
{
namespace gls
{
namespace sl
{

static const glu::GLSLVersion DEFAULT_GLSL_VERSION = glu::GLSL_VERSION_100_ES;

DE_INLINE deBool isWhitespace (char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

DE_INLINE deBool isEOL (char c)
{
	return (c == '\r') || (c == '\n');
}

DE_INLINE deBool isNumeric (char c)
{
	return deInRange32(c, '0', '9');
}

DE_INLINE deBool isAlpha (char c)
{
	return deInRange32(c, 'a', 'z') || deInRange32(c, 'A', 'Z');
}

DE_INLINE deBool isCaseNameChar (char c)
{
	return deInRange32(c, 'a', 'z') || deInRange32(c, 'A', 'Z') || deInRange32(c, '0', '9') || (c == '_') || (c == '-') || (c == '.');
}

// \todo [2011-02-11 pyry] Should not depend on Context or TestContext!
class ShaderParser
{
public:
							ShaderParser			(tcu::TestContext& testCtx, RenderContext& renderCtx, const glu::ContextInfo& contextInfo, const char* currentDir = DE_NULL);
							~ShaderParser			(void);

	vector<tcu::TestNode*>	parse					(const char* input);

private:
	enum Token
	{
		TOKEN_INVALID = 0,
		TOKEN_EOF,
		TOKEN_STRING,
		TOKEN_SHADER_SOURCE,

		TOKEN_INT_LITERAL,
		TOKEN_FLOAT_LITERAL,

		// identifiers
		TOKEN_IDENTIFIER,
		TOKEN_TRUE,
		TOKEN_FALSE,
		TOKEN_DESC,
		TOKEN_EXPECT,
		TOKEN_GROUP,
		TOKEN_CASE,
		TOKEN_END,
		TOKEN_VALUES,
		TOKEN_BOTH,
		TOKEN_VERTEX,
		TOKEN_FRAGMENT,
		TOKEN_UNIFORM,
		TOKEN_INPUT,
		TOKEN_OUTPUT,
		TOKEN_FLOAT,
		TOKEN_FLOAT_VEC2,
		TOKEN_FLOAT_VEC3,
		TOKEN_FLOAT_VEC4,
		TOKEN_FLOAT_MAT2,
		TOKEN_FLOAT_MAT2X3,
		TOKEN_FLOAT_MAT2X4,
		TOKEN_FLOAT_MAT3X2,
		TOKEN_FLOAT_MAT3,
		TOKEN_FLOAT_MAT3X4,
		TOKEN_FLOAT_MAT4X2,
		TOKEN_FLOAT_MAT4X3,
		TOKEN_FLOAT_MAT4,
		TOKEN_INT,
		TOKEN_INT_VEC2,
		TOKEN_INT_VEC3,
		TOKEN_INT_VEC4,
		TOKEN_UINT,
		TOKEN_UINT_VEC2,
		TOKEN_UINT_VEC3,
		TOKEN_UINT_VEC4,
		TOKEN_BOOL,
		TOKEN_BOOL_VEC2,
		TOKEN_BOOL_VEC3,
		TOKEN_BOOL_VEC4,
		TOKEN_VERSION,
		TOKEN_TESSELLATION_CONTROL,
		TOKEN_TESSELLATION_EVALUATION,
		TOKEN_GEOMETRY,
		TOKEN_REQUIRE,
		TOKEN_IN,
		TOKEN_IMPORT,
		TOKEN_PIPELINE_PROGRAM,
		TOKEN_ACTIVE_STAGES,

		// symbols
		TOKEN_ASSIGN,
		TOKEN_PLUS,
		TOKEN_MINUS,
		TOKEN_COMMA,
		TOKEN_VERTICAL_BAR,
		TOKEN_SEMI_COLON,
		TOKEN_LEFT_PAREN,
		TOKEN_RIGHT_PAREN,
		TOKEN_LEFT_BRACKET,
		TOKEN_RIGHT_BRACKET,
		TOKEN_LEFT_BRACE,
		TOKEN_RIGHT_BRACE,
		TOKEN_GREATER,

		TOKEN_LAST
	};

	void						parseError					(const std::string& errorStr);
	float						parseFloatLiteral			(const char* str);
	int							parseIntLiteral				(const char* str);
	string						parseStringLiteral			(const char* str);
	string						parseShaderSource			(const char* str);
	void						advanceToken				(void);
	void						advanceToken				(Token assumed);
	void						assumeToken					(Token token);
	DataType					mapDataTypeToken			(Token token);
	const char*					getTokenName				(Token token);
	deUint32					getShaderStageLiteralFlag	(void);
	deUint32					getGLEnumFromName			(const std::string& enumName);

	void						parseValueElement			(DataType dataType, ShaderCase::Value& result);
	void						parseValue					(ShaderCase::ValueBlock& valueBlock);
	void						parseValueBlock				(ShaderCase::ValueBlock& valueBlock);
	deUint32					parseShaderStageList		(void);
	void						parseRequirement			(ShaderCase::CaseRequirement& valueBlock);
	void						parseExpectResult			(ShaderCase::ExpectResult& expectResult);
	void						parseGLSLVersion			(glu::GLSLVersion& version);
	void						parsePipelineProgram		(ShaderCase::PipelineProgram& program);
	void						parseShaderCase				(vector<tcu::TestNode*>& shaderNodeList);
	void						parseShaderGroup			(vector<tcu::TestNode*>& shaderNodeList);
	void						parseImport					(vector<tcu::TestNode*>& shaderNodeList);

	// Member variables.
	tcu::TestContext&			m_testCtx;
	RenderContext&				m_renderCtx;
	const glu::ContextInfo&		m_contextInfo;
	std::string					m_input;
	const char*					m_curPtr;
	Token						m_curToken;
	std::string					m_curTokenStr;
	const char* const			m_currentDir;
};

ShaderParser::ShaderParser (tcu::TestContext& testCtx, RenderContext& renderCtx, const glu::ContextInfo& contextInfo, const char* currentDir)
	: m_testCtx			(testCtx)
	, m_renderCtx		(renderCtx)
	, m_contextInfo		(contextInfo)
	, m_curPtr			(DE_NULL)
	, m_curToken		(TOKEN_LAST)
	, m_currentDir		(currentDir)
{
}

ShaderParser::~ShaderParser (void)
{
	// nada
}

void ShaderParser::parseError (const std::string& errorStr)
{
	string atStr = string(m_curPtr, 80);
	throw tcu::InternalError((string("Parser error: ") + errorStr + " near '" + atStr + " ...'").c_str(), "", __FILE__, __LINE__);
}

float ShaderParser::parseFloatLiteral (const char* str)
{
	return (float)atof(str);
}

int ShaderParser::parseIntLiteral (const char* str)
{
	return atoi(str);
}

string ShaderParser::parseStringLiteral (const char* str)
{
	const char*		p		= str;
	char			endChar = *p++;
	ostringstream	o;

	while (*p != endChar && *p)
	{
		if (*p == '\\')
		{
			switch (p[1])
			{
				case 0:		DE_ASSERT(DE_FALSE);	break;
				case 'n':	o << '\n';				break;
				case 't':	o << '\t';				break;
				default:	o << p[1];				break;
			}

			p += 2;
		}
		else
			o << *p++;
	}

	return o.str();
}

static string removeExtraIndentation (const string& source)
{
	// Detect indentation from first line.
	int numIndentChars = 0;
	for (int ndx = 0; ndx < (int)source.length() && isWhitespace(source[ndx]); ndx++)
		numIndentChars += source[ndx] == '\t' ? 4 : 1;

	// Process all lines and remove preceding indentation.
	ostringstream processed;
	{
		bool	atLineStart			= true;
		int		indentCharsOmitted	= 0;

		for (int pos = 0; pos < (int)source.length(); pos++)
		{
			char c = source[pos];

			if (atLineStart && indentCharsOmitted < numIndentChars && (c == ' ' || c == '\t'))
			{
				indentCharsOmitted += c == '\t' ? 4 : 1;
			}
			else if (isEOL(c))
			{
				if (source[pos] == '\r' && source[pos+1] == '\n')
				{
					pos += 1;
					processed << '\n';
				}
				else
					processed << c;

				atLineStart			= true;
				indentCharsOmitted	= 0;
			}
			else
			{
				processed << c;
				atLineStart = false;
			}
		}
	}

	return processed.str();
}

string ShaderParser::parseShaderSource (const char* str)
{
	const char*		p = str+2;
	ostringstream	o;

	// Eat first empty line from beginning.
	while (*p == ' ') p++;
	if (*p == '\r') p++;
	if (*p == '\n') p++;

	while ((p[0] != '"') || (p[1] != '"'))
	{
		if (*p == '\\')
		{
			switch (p[1])
			{
				case 0:		DE_ASSERT(DE_FALSE);	break;
				case 'n':	o << '\n';				break;
				case 't':	o << '\t';				break;
				default:	o << p[1];				break;
			}

			p += 2;
		}
		else
			o << *p++;
	}

	return removeExtraIndentation(o.str());
}

void ShaderParser::advanceToken (void)
{
	// Skip old token.
	m_curPtr += m_curTokenStr.length();

	// Reset token (for safety).
	m_curToken		= TOKEN_INVALID;
	m_curTokenStr	= "";

	// Eat whitespace & comments while they last.
	for (;;)
	{
		while (isWhitespace(*m_curPtr))
			m_curPtr++;

		// Check for EOL comment.
		if (*m_curPtr == '#')
		{
			while (*m_curPtr && !isEOL(*m_curPtr))
				m_curPtr++;
		}
		else
			break;
	}

	if (!*m_curPtr)
	{
		m_curToken = TOKEN_EOF;
		m_curTokenStr = "<EOF>";
	}
	else if (isAlpha(*m_curPtr))
	{
		struct Named
		{
			const char*		str;
			Token			token;
		};

		static const Named s_named[] =
		{
			{ "true",						TOKEN_TRUE						},
			{ "false",						TOKEN_FALSE						},
			{ "desc",						TOKEN_DESC						},
			{ "expect",						TOKEN_EXPECT					},
			{ "group",						TOKEN_GROUP						},
			{ "case",						TOKEN_CASE						},
			{ "end",						TOKEN_END						},
			{ "values",						TOKEN_VALUES					},
			{ "both",						TOKEN_BOTH						},
			{ "vertex",						TOKEN_VERTEX					},
			{ "fragment",					TOKEN_FRAGMENT					},
			{ "uniform",					TOKEN_UNIFORM					},
			{ "input",						TOKEN_INPUT						},
			{ "output",						TOKEN_OUTPUT					},
			{ "float",						TOKEN_FLOAT						},
			{ "vec2",						TOKEN_FLOAT_VEC2				},
			{ "vec3",						TOKEN_FLOAT_VEC3				},
			{ "vec4",						TOKEN_FLOAT_VEC4				},
			{ "mat2",						TOKEN_FLOAT_MAT2				},
			{ "mat2x3",						TOKEN_FLOAT_MAT2X3				},
			{ "mat2x4",						TOKEN_FLOAT_MAT2X4				},
			{ "mat3x2",						TOKEN_FLOAT_MAT3X2				},
			{ "mat3",						TOKEN_FLOAT_MAT3				},
			{ "mat3x4",						TOKEN_FLOAT_MAT3X4				},
			{ "mat4x2",						TOKEN_FLOAT_MAT4X2				},
			{ "mat4x3",						TOKEN_FLOAT_MAT4X3				},
			{ "mat4",						TOKEN_FLOAT_MAT4				},
			{ "int",						TOKEN_INT						},
			{ "ivec2",						TOKEN_INT_VEC2					},
			{ "ivec3",						TOKEN_INT_VEC3					},
			{ "ivec4",						TOKEN_INT_VEC4					},
			{ "uint",						TOKEN_UINT						},
			{ "uvec2",						TOKEN_UINT_VEC2					},
			{ "uvec3",						TOKEN_UINT_VEC3					},
			{ "uvec4",						TOKEN_UINT_VEC4					},
			{ "bool",						TOKEN_BOOL						},
			{ "bvec2",						TOKEN_BOOL_VEC2					},
			{ "bvec3",						TOKEN_BOOL_VEC3					},
			{ "bvec4",						TOKEN_BOOL_VEC4					},
			{ "version",					TOKEN_VERSION					},
			{ "tessellation_control",		TOKEN_TESSELLATION_CONTROL		},
			{ "tessellation_evaluation",	TOKEN_TESSELLATION_EVALUATION	},
			{ "geometry",					TOKEN_GEOMETRY					},
			{ "require",					TOKEN_REQUIRE					},
			{ "in",							TOKEN_IN						},
			{ "import",						TOKEN_IMPORT					},
			{ "pipeline_program",			TOKEN_PIPELINE_PROGRAM			},
			{ "active_stages",				TOKEN_ACTIVE_STAGES				},
		};

		const char* end = m_curPtr + 1;
		while (isCaseNameChar(*end))
			end++;
		m_curTokenStr = string(m_curPtr, end - m_curPtr);

		m_curToken = TOKEN_IDENTIFIER;

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_named); ndx++)
		{
			if (m_curTokenStr == s_named[ndx].str)
			{
				m_curToken = s_named[ndx].token;
				break;
			}
		}
	}
	else if (isNumeric(*m_curPtr))
	{
		/* \todo [2010-03-31 petri] Hex? */
		const char* p = m_curPtr;
		while (isNumeric(*p))
			p++;
		if (*p == '.')
		{
			p++;
			while (isNumeric(*p))
				p++;

			if (*p == 'e' || *p == 'E')
			{
				p++;
				if (*p == '+' || *p == '-')
					p++;
				DE_ASSERT(isNumeric(*p));
				while (isNumeric(*p))
					p++;
			}

			m_curToken = TOKEN_FLOAT_LITERAL;
			m_curTokenStr = string(m_curPtr, p - m_curPtr);
		}
		else
		{
			m_curToken = TOKEN_INT_LITERAL;
			m_curTokenStr = string(m_curPtr, p - m_curPtr);
		}
	}
	else if (*m_curPtr == '"' && m_curPtr[1] == '"')
	{
		const char*	p = m_curPtr + 2;

		while ((p[0] != '"') || (p[1] != '"'))
		{
			DE_ASSERT(*p);
			if (*p == '\\')
			{
				DE_ASSERT(p[1] != 0);
				p += 2;
			}
			else
				p++;
		}
		p += 2;

		m_curToken		= TOKEN_SHADER_SOURCE;
		m_curTokenStr	= string(m_curPtr, (int)(p - m_curPtr));
	}
	else if (*m_curPtr == '"' || *m_curPtr == '\'')
	{
		char		endChar = *m_curPtr;
		const char*	p		= m_curPtr + 1;

		while (*p != endChar)
		{
			DE_ASSERT(*p);
			if (*p == '\\')
			{
				DE_ASSERT(p[1] != 0);
				p += 2;
			}
			else
				p++;
		}
		p++;

		m_curToken		= TOKEN_STRING;
		m_curTokenStr	= string(m_curPtr, (int)(p - m_curPtr));
	}
	else
	{
		struct SimpleToken
		{
			const char*		str;
			Token			token;
		};

		static const SimpleToken s_simple[] =
		{
			{ "=",			TOKEN_ASSIGN		},
			{ "+",			TOKEN_PLUS			},
			{ "-",			TOKEN_MINUS			},
			{ ",",			TOKEN_COMMA			},
			{ "|",			TOKEN_VERTICAL_BAR	},
			{ ";",			TOKEN_SEMI_COLON	},
			{ "(",			TOKEN_LEFT_PAREN	},
			{ ")",			TOKEN_RIGHT_PAREN	},
			{ "[",			TOKEN_LEFT_BRACKET	},
			{ "]",			TOKEN_RIGHT_BRACKET },
			{ "{",			TOKEN_LEFT_BRACE	},
			{ "}",			TOKEN_RIGHT_BRACE	},
			{ ">",			TOKEN_GREATER		},
		};

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_simple); ndx++)
		{
			if (strncmp(s_simple[ndx].str, m_curPtr, strlen(s_simple[ndx].str)) == 0)
			{
				m_curToken		= s_simple[ndx].token;
				m_curTokenStr	= s_simple[ndx].str;
				return;
			}
		}

		// Otherwise invalid token.
		m_curToken = TOKEN_INVALID;
		m_curTokenStr = *m_curPtr;
	}
}

void ShaderParser::advanceToken (Token assumed)
{
	assumeToken(assumed);
	advanceToken();
}

void ShaderParser::assumeToken (Token token)
{
	if (m_curToken != token)
		parseError((string("unexpected token '") + m_curTokenStr + "', expecting '" + getTokenName(token) + "'").c_str());
	DE_TEST_ASSERT(m_curToken == token);
}

DataType ShaderParser::mapDataTypeToken (Token token)
{
	switch (token)
	{
		case TOKEN_FLOAT:			return TYPE_FLOAT;
		case TOKEN_FLOAT_VEC2:		return TYPE_FLOAT_VEC2;
		case TOKEN_FLOAT_VEC3:		return TYPE_FLOAT_VEC3;
		case TOKEN_FLOAT_VEC4:		return TYPE_FLOAT_VEC4;
		case TOKEN_FLOAT_MAT2:		return TYPE_FLOAT_MAT2;
		case TOKEN_FLOAT_MAT2X3:	return TYPE_FLOAT_MAT2X3;
		case TOKEN_FLOAT_MAT2X4:	return TYPE_FLOAT_MAT2X4;
		case TOKEN_FLOAT_MAT3X2:	return TYPE_FLOAT_MAT3X2;
		case TOKEN_FLOAT_MAT3:		return TYPE_FLOAT_MAT3;
		case TOKEN_FLOAT_MAT3X4:	return TYPE_FLOAT_MAT3X4;
		case TOKEN_FLOAT_MAT4X2:	return TYPE_FLOAT_MAT4X2;
		case TOKEN_FLOAT_MAT4X3:	return TYPE_FLOAT_MAT4X3;
		case TOKEN_FLOAT_MAT4:		return TYPE_FLOAT_MAT4;
		case TOKEN_INT:				return TYPE_INT;
		case TOKEN_INT_VEC2:		return TYPE_INT_VEC2;
		case TOKEN_INT_VEC3:		return TYPE_INT_VEC3;
		case TOKEN_INT_VEC4:		return TYPE_INT_VEC4;
		case TOKEN_UINT:			return TYPE_UINT;
		case TOKEN_UINT_VEC2:		return TYPE_UINT_VEC2;
		case TOKEN_UINT_VEC3:		return TYPE_UINT_VEC3;
		case TOKEN_UINT_VEC4:		return TYPE_UINT_VEC4;
		case TOKEN_BOOL:			return TYPE_BOOL;
		case TOKEN_BOOL_VEC2:		return TYPE_BOOL_VEC2;
		case TOKEN_BOOL_VEC3:		return TYPE_BOOL_VEC3;
		case TOKEN_BOOL_VEC4:		return TYPE_BOOL_VEC4;
		default:					return TYPE_INVALID;
	}
}

const char* ShaderParser::getTokenName (Token token)
{
	switch (token)
	{
		case TOKEN_INVALID:					return "<invalid>";
		case TOKEN_EOF:						return "<eof>";
		case TOKEN_STRING:					return "<string>";
		case TOKEN_SHADER_SOURCE:			return "source";

		case TOKEN_INT_LITERAL:				return "<int>";
		case TOKEN_FLOAT_LITERAL:			return "<float>";

		// identifiers
		case TOKEN_IDENTIFIER:				return "<identifier>";
		case TOKEN_TRUE:					return "true";
		case TOKEN_FALSE:					return "false";
		case TOKEN_DESC:					return "desc";
		case TOKEN_EXPECT:					return "expect";
		case TOKEN_GROUP:					return "group";
		case TOKEN_CASE:					return "case";
		case TOKEN_END:						return "end";
		case TOKEN_VALUES:					return "values";
		case TOKEN_BOTH:					return "both";
		case TOKEN_VERTEX:					return "vertex";
		case TOKEN_FRAGMENT:				return "fragment";
		case TOKEN_TESSELLATION_CONTROL:	return "tessellation_control";
		case TOKEN_TESSELLATION_EVALUATION:	return "tessellation_evaluation";
		case TOKEN_GEOMETRY:				return "geometry";
		case TOKEN_REQUIRE:					return "require";
		case TOKEN_UNIFORM:					return "uniform";
		case TOKEN_INPUT:					return "input";
		case TOKEN_OUTPUT:					return "output";
		case TOKEN_FLOAT:					return "float";
		case TOKEN_FLOAT_VEC2:				return "vec2";
		case TOKEN_FLOAT_VEC3:				return "vec3";
		case TOKEN_FLOAT_VEC4:				return "vec4";
		case TOKEN_FLOAT_MAT2:				return "mat2";
		case TOKEN_FLOAT_MAT2X3:			return "mat2x3";
		case TOKEN_FLOAT_MAT2X4:			return "mat2x4";
		case TOKEN_FLOAT_MAT3X2:			return "mat3x2";
		case TOKEN_FLOAT_MAT3:				return "mat3";
		case TOKEN_FLOAT_MAT3X4:			return "mat3x4";
		case TOKEN_FLOAT_MAT4X2:			return "mat4x2";
		case TOKEN_FLOAT_MAT4X3:			return "mat4x3";
		case TOKEN_FLOAT_MAT4:				return "mat4";
		case TOKEN_INT:						return "int";
		case TOKEN_INT_VEC2:				return "ivec2";
		case TOKEN_INT_VEC3:				return "ivec3";
		case TOKEN_INT_VEC4:				return "ivec4";
		case TOKEN_UINT:					return "uint";
		case TOKEN_UINT_VEC2:				return "uvec2";
		case TOKEN_UINT_VEC3:				return "uvec3";
		case TOKEN_UINT_VEC4:				return "uvec4";
		case TOKEN_BOOL:					return "bool";
		case TOKEN_BOOL_VEC2:				return "bvec2";
		case TOKEN_BOOL_VEC3:				return "bvec3";
		case TOKEN_BOOL_VEC4:				return "bvec4";
		case TOKEN_IN:						return "in";
		case TOKEN_IMPORT:					return "import";
		case TOKEN_PIPELINE_PROGRAM:		return "pipeline_program";
		case TOKEN_ACTIVE_STAGES:			return "active_stages";

		case TOKEN_ASSIGN:					return "=";
		case TOKEN_PLUS:					return "+";
		case TOKEN_MINUS:					return "-";
		case TOKEN_COMMA:					return ",";
		case TOKEN_VERTICAL_BAR:			return "|";
		case TOKEN_SEMI_COLON:				return ";";
		case TOKEN_LEFT_PAREN:				return "(";
		case TOKEN_RIGHT_PAREN:				return ")";
		case TOKEN_LEFT_BRACKET:			return "[";
		case TOKEN_RIGHT_BRACKET:			return "]";
		case TOKEN_LEFT_BRACE:				return "{";
		case TOKEN_RIGHT_BRACE:				return "}";
		case TOKEN_GREATER:					return ">";

		default:							return "<unknown>";
	}
}

deUint32 ShaderParser::getShaderStageLiteralFlag (void)
{
	switch (m_curToken)
	{
		case TOKEN_VERTEX:					return (1 << glu::SHADERTYPE_VERTEX);
		case TOKEN_FRAGMENT:				return (1 << glu::SHADERTYPE_FRAGMENT);
		case TOKEN_GEOMETRY:				return (1 << glu::SHADERTYPE_GEOMETRY);
		case TOKEN_TESSELLATION_CONTROL:	return (1 << glu::SHADERTYPE_TESSELLATION_CONTROL);
		case TOKEN_TESSELLATION_EVALUATION:	return (1 << glu::SHADERTYPE_TESSELLATION_EVALUATION);

		default:
			parseError(std::string() + "invalid shader stage name, got " + m_curTokenStr);
			return 0;
	}
}

deUint32 ShaderParser::getGLEnumFromName (const std::string& enumName)
{
	static const struct
	{
		const char*	name;
		deUint32	value;
	} names[] =
	{
		{ "GL_MAX_VERTEX_IMAGE_UNIFORMS",			GL_MAX_VERTEX_IMAGE_UNIFORMS			},
		{ "GL_MAX_VERTEX_ATOMIC_COUNTERS",			GL_MAX_VERTEX_ATOMIC_COUNTERS			},
		{ "GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS",	GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS		},
		{ "GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS",	GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS	},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(names); ++ndx)
		if (names[ndx].name == enumName)
			return names[ndx].value;

	parseError(std::string() + "unknown enum name, got " + enumName);
	return 0;
}

void ShaderParser::parseValueElement (DataType expectedDataType, ShaderCase::Value& result)
{
	DataType	scalarType	= getDataTypeScalarType(expectedDataType);
	int			scalarSize	= getDataTypeScalarSize(expectedDataType);

	/* \todo [2010-04-19 petri] Support arrays. */
	ShaderCase::Value::Element elems[16];

	if (scalarSize > 1)
	{
		DE_ASSERT(mapDataTypeToken(m_curToken) == expectedDataType);
		advanceToken(); // data type (float, vec2, etc.)
		advanceToken(TOKEN_LEFT_PAREN);
	}

	for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
	{
		if (scalarType == TYPE_FLOAT)
		{
			float signMult = 1.0f;
			if (m_curToken == TOKEN_MINUS)
			{
				signMult = -1.0f;
				advanceToken();
			}

			assumeToken(TOKEN_FLOAT_LITERAL);
			elems[scalarNdx].float32 = signMult * parseFloatLiteral(m_curTokenStr.c_str());
			advanceToken(TOKEN_FLOAT_LITERAL);
		}
		else if (scalarType == TYPE_INT || scalarType == TYPE_UINT)
		{
			int signMult = 1;
			if (m_curToken == TOKEN_MINUS)
			{
				signMult = -1;
				advanceToken();
			}

			assumeToken(TOKEN_INT_LITERAL);
			elems[scalarNdx].int32 = signMult * parseIntLiteral(m_curTokenStr.c_str());
			advanceToken(TOKEN_INT_LITERAL);
		}
		else
		{
			DE_ASSERT(scalarType == TYPE_BOOL);
			elems[scalarNdx].bool32 = (m_curToken == TOKEN_TRUE);
			if (m_curToken != TOKEN_TRUE && m_curToken != TOKEN_FALSE)
				parseError(string("unexpected token, expecting bool: " + m_curTokenStr));
			advanceToken(); // true/false
		}

		if (scalarNdx != (scalarSize - 1))
			advanceToken(TOKEN_COMMA);
	}

	if (scalarSize > 1)
		advanceToken(TOKEN_RIGHT_PAREN);

	// Store results.
	for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
		result.elements.push_back(elems[scalarNdx]);
}

void ShaderParser::parseValue (ShaderCase::ValueBlock& valueBlock)
{
	PARSE_DBG(("      parseValue()\n"));

	// Parsed results.
	ShaderCase::Value result;

	// Parse storage.
	if (m_curToken == TOKEN_UNIFORM)
		result.storageType = ShaderCase::Value::STORAGE_UNIFORM;
	else if (m_curToken == TOKEN_INPUT)
		result.storageType = ShaderCase::Value::STORAGE_INPUT;
	else if (m_curToken == TOKEN_OUTPUT)
		result.storageType = ShaderCase::Value::STORAGE_OUTPUT;
	else
		parseError(string("unexpected token encountered when parsing value classifier"));
	advanceToken();

	// Parse data type.
	result.dataType = mapDataTypeToken(m_curToken);
	if (result.dataType == TYPE_INVALID)
		parseError(string("unexpected token when parsing value data type: " + m_curTokenStr));
	advanceToken();

	// Parse value name.
	if (m_curToken == TOKEN_IDENTIFIER || m_curToken == TOKEN_STRING)
	{
		if (m_curToken == TOKEN_IDENTIFIER)
			result.valueName = m_curTokenStr;
		else
			result.valueName = parseStringLiteral(m_curTokenStr.c_str());
	}
	else
		parseError(string("unexpected token when parsing value name: " + m_curTokenStr));
	advanceToken();

	// Parse assignment operator.
	advanceToken(TOKEN_ASSIGN);

	// Parse actual value.
	if (m_curToken == TOKEN_LEFT_BRACKET) // value list
	{
		advanceToken(TOKEN_LEFT_BRACKET);
		result.arrayLength = 0;

		for (;;)
		{
			parseValueElement(result.dataType, result);
			result.arrayLength++;

			if (m_curToken == TOKEN_RIGHT_BRACKET)
				break;
			else if (m_curToken == TOKEN_VERTICAL_BAR)
			{
				advanceToken();
				continue;
			}
			else
				parseError(string("unexpected token in value element array: " + m_curTokenStr));
		}

		advanceToken(TOKEN_RIGHT_BRACKET);
	}
	else // arrays, single elements
	{
		parseValueElement(result.dataType, result);
		result.arrayLength = 1;
	}

	advanceToken(TOKEN_SEMI_COLON); // end of declaration

	valueBlock.values.push_back(result);
}

void ShaderParser::parseValueBlock (ShaderCase::ValueBlock& valueBlock)
{
	PARSE_DBG(("    parseValueBlock()\n"));
	advanceToken(TOKEN_VALUES);
	advanceToken(TOKEN_LEFT_BRACE);

	for (;;)
	{
		if (m_curToken == TOKEN_UNIFORM || m_curToken == TOKEN_INPUT || m_curToken == TOKEN_OUTPUT)
			parseValue(valueBlock);
		else if (m_curToken == TOKEN_RIGHT_BRACE)
			break;
		else
			parseError(string("unexpected token when parsing a value block: " + m_curTokenStr));
	}

	advanceToken(TOKEN_RIGHT_BRACE);

	// Compute combined array length of value block.
	int arrayLength = 1;
	for (int valueNdx = 0; valueNdx < (int)valueBlock.values.size(); valueNdx++)
	{
		const ShaderCase::Value& val = valueBlock.values[valueNdx];
		if (val.arrayLength > 1)
		{
			DE_ASSERT(arrayLength == 1 || arrayLength == val.arrayLength);
			arrayLength = val.arrayLength;
		}
	}
	valueBlock.arrayLength = arrayLength;
}

deUint32 ShaderParser::parseShaderStageList (void)
{
	deUint32 mask = 0;

	assumeToken(TOKEN_LEFT_BRACE);

	// don't allow 0-sized lists
	advanceToken();
	mask |= getShaderStageLiteralFlag();
	advanceToken();

	for (;;)
	{
		if (m_curToken == TOKEN_RIGHT_BRACE)
			break;
		else if (m_curToken == TOKEN_COMMA)
		{
			deUint32 stageFlag;
			advanceToken();

			stageFlag = getShaderStageLiteralFlag();
			if (stageFlag & mask)
				parseError(string("stage already set in the shader stage set: " + m_curTokenStr));

			mask |= stageFlag;
			advanceToken();
		}
		else
			parseError(string("invalid shader stage set token: " + m_curTokenStr));
	}
	advanceToken(TOKEN_RIGHT_BRACE);

	return mask;
}

void ShaderParser::parseRequirement (ShaderCase::CaseRequirement& valueBlock)
{
	PARSE_DBG(("    parseRequirement()\n"));

	advanceToken();
	assumeToken(TOKEN_IDENTIFIER);

	if (m_curTokenStr == "extension")
	{
		std::vector<std::string>	anyExtensionStringList;
		deUint32					affectedCasesFlags		= -1; // by default all stages

		advanceToken();
		assumeToken(TOKEN_LEFT_BRACE);

		advanceToken();
		assumeToken(TOKEN_STRING);

		anyExtensionStringList.push_back(parseStringLiteral(m_curTokenStr.c_str()));
		advanceToken();

		for (;;)
		{
			if (m_curToken == TOKEN_RIGHT_BRACE)
				break;
			else if (m_curToken == TOKEN_VERTICAL_BAR)
			{
				advanceToken();
				assumeToken(TOKEN_STRING);

				anyExtensionStringList.push_back(parseStringLiteral(m_curTokenStr.c_str()));
				advanceToken();
			}
			else
				parseError(string("invalid extension list token: " + m_curTokenStr));
		}
		advanceToken(TOKEN_RIGHT_BRACE);

		if (m_curToken == TOKEN_IN)
		{
			advanceToken();
			affectedCasesFlags = parseShaderStageList();
		}

		valueBlock = ShaderCase::CaseRequirement::createAnyExtensionRequirement(anyExtensionStringList, affectedCasesFlags);
	}
	else if (m_curTokenStr == "limit")
	{
		deUint32	limitEnum;
		int			limitValue;

		advanceToken();

		assumeToken(TOKEN_STRING);
		limitEnum = getGLEnumFromName(parseStringLiteral(m_curTokenStr.c_str()));
		advanceToken();

		assumeToken(TOKEN_GREATER);
		advanceToken();

		assumeToken(TOKEN_INT_LITERAL);
		limitValue = parseIntLiteral(m_curTokenStr.c_str());
		advanceToken();

		valueBlock = ShaderCase::CaseRequirement::createLimitRequirement(limitEnum, limitValue);
	}
	else if (m_curTokenStr == "full_glsl_es_100_support")
	{
		advanceToken();

		valueBlock = ShaderCase::CaseRequirement::createFullGLSLES100SpecificationRequirement();
	}
	else
		parseError(string("invalid requirement value: " + m_curTokenStr));
}

void ShaderParser::parseExpectResult (ShaderCase::ExpectResult& expectResult)
{
	assumeToken(TOKEN_IDENTIFIER);

	if (m_curTokenStr == "pass")
		expectResult = ShaderCase::EXPECT_PASS;
	else if (m_curTokenStr == "compile_fail")
		expectResult = ShaderCase::EXPECT_COMPILE_FAIL;
	else if (m_curTokenStr == "link_fail")
		expectResult = ShaderCase::EXPECT_LINK_FAIL;
	else if (m_curTokenStr == "compile_or_link_fail")
		expectResult = ShaderCase::EXPECT_COMPILE_LINK_FAIL;
	else if (m_curTokenStr == "validation_fail")
		expectResult = ShaderCase::EXPECT_VALIDATION_FAIL;
	else if (m_curTokenStr == "build_successful")
		expectResult = ShaderCase::EXPECT_BUILD_SUCCESSFUL;
	else
		parseError(string("invalid expected result value: " + m_curTokenStr));

	advanceToken();
}

void ShaderParser::parseGLSLVersion (glu::GLSLVersion& version)
{
	int			versionNum		= 0;
	std::string	postfix			= "";

	assumeToken(TOKEN_INT_LITERAL);
	versionNum = parseIntLiteral(m_curTokenStr.c_str());
	advanceToken();

	if (m_curToken == TOKEN_IDENTIFIER)
	{
		postfix = m_curTokenStr;
		advanceToken();
	}

	if		(versionNum == 100 && postfix == "es")	version = glu::GLSL_VERSION_100_ES;
	else if (versionNum == 300 && postfix == "es")	version = glu::GLSL_VERSION_300_ES;
	else if (versionNum == 310 && postfix == "es")	version = glu::GLSL_VERSION_310_ES;
	else if (versionNum == 130)						version = glu::GLSL_VERSION_130;
	else if (versionNum == 140)						version = glu::GLSL_VERSION_140;
	else if (versionNum == 150)						version = glu::GLSL_VERSION_150;
	else if (versionNum == 330)						version = glu::GLSL_VERSION_330;
	else if (versionNum == 400)						version = glu::GLSL_VERSION_400;
	else if (versionNum == 410)						version = glu::GLSL_VERSION_410;
	else if (versionNum == 420)						version = glu::GLSL_VERSION_420;
	else if (versionNum == 430)						version = glu::GLSL_VERSION_430;
	else
		parseError("Unknown GLSL version");
}

void ShaderParser::parsePipelineProgram (ShaderCase::PipelineProgram& program)
{
	deUint32							activeStages			= 0;
	vector<string>						vertexSources;
	vector<string>						fragmentSources;
	vector<string>						tessellationCtrlSources;
	vector<string>						tessellationEvalSources;
	vector<string>						geometrySources;
	vector<ShaderCase::CaseRequirement>	requirements;

	advanceToken(TOKEN_PIPELINE_PROGRAM);

	for (;;)
	{
		if (m_curToken == TOKEN_END)
			break;
		else if (m_curToken == TOKEN_ACTIVE_STAGES)
		{
			advanceToken();
			activeStages = parseShaderStageList();
		}
		else if (m_curToken == TOKEN_REQUIRE)
		{
			ShaderCase::CaseRequirement requirement;
			parseRequirement(requirement);
			requirements.push_back(requirement);
		}
		else if (m_curToken == TOKEN_VERTEX						||
				 m_curToken == TOKEN_FRAGMENT					||
				 m_curToken == TOKEN_TESSELLATION_CONTROL		||
				 m_curToken == TOKEN_TESSELLATION_EVALUATION	||
				 m_curToken == TOKEN_GEOMETRY)
		{
			const Token	token = m_curToken;
			string		source;

			advanceToken();
			assumeToken(TOKEN_SHADER_SOURCE);
			source = parseShaderSource(m_curTokenStr.c_str());
			advanceToken();

			switch (token)
			{
				case TOKEN_VERTEX:					vertexSources.push_back(source);			break;
				case TOKEN_FRAGMENT:				fragmentSources.push_back(source);			break;
				case TOKEN_TESSELLATION_CONTROL:	tessellationCtrlSources.push_back(source);	break;
				case TOKEN_TESSELLATION_EVALUATION:	tessellationEvalSources.push_back(source);	break;
				case TOKEN_GEOMETRY:				geometrySources.push_back(source);			break;
				default:
					parseError(DE_FALSE);
			}
		}
		else
			parseError(string("invalid pipeline program value: " + m_curTokenStr));
	}
	advanceToken(TOKEN_END);

	if (activeStages == 0)
		parseError("program pipeline object must have active stages");

	// return pipeline part
	program.activeStageBits = activeStages;
	program.requirements.swap(requirements);
	program.vertexSources.swap(vertexSources);
	program.fragmentSources.swap(fragmentSources);
	program.tessCtrlSources.swap(tessellationCtrlSources);
	program.tessEvalSources.swap(tessellationEvalSources);
	program.geometrySources.swap(geometrySources);
}

void ShaderParser::parseShaderCase (vector<tcu::TestNode*>& shaderNodeList)
{
	// Parse 'case'.
	PARSE_DBG(("  parseShaderCase()\n"));
	advanceToken(TOKEN_CASE);

	// Parse case name.
	string caseName = m_curTokenStr;
	advanceToken(); // \note [pyry] All token types are allowed here.

	// Setup case.
	GLSLVersion							version			= DEFAULT_GLSL_VERSION;
	ShaderCase::ExpectResult			expectResult	= ShaderCase::EXPECT_PASS;
	string								description;
	string								bothSource;
	vector<string>						vertexSources;
	vector<string>						fragmentSources;
	vector<string>						tessellationCtrlSources;
	vector<string>						tessellationEvalSources;
	vector<string>						geometrySources;
	vector<ShaderCase::ValueBlock>		valueBlockList;
	vector<ShaderCase::CaseRequirement>	requirements;
	vector<ShaderCase::PipelineProgram>	pipelinePrograms;

	for (;;)
	{
		if (m_curToken == TOKEN_END)
			break;
		else if (m_curToken == TOKEN_DESC)
		{
			advanceToken();
			assumeToken(TOKEN_STRING);
			description = parseStringLiteral(m_curTokenStr.c_str());
			advanceToken();
		}
		else if (m_curToken == TOKEN_EXPECT)
		{
			advanceToken();
			parseExpectResult(expectResult);
		}
		else if (m_curToken == TOKEN_VALUES)
		{
			ShaderCase::ValueBlock block;
			parseValueBlock(block);
			valueBlockList.push_back(block);
		}
		else if (m_curToken == TOKEN_BOTH						||
				 m_curToken == TOKEN_VERTEX						||
				 m_curToken == TOKEN_FRAGMENT					||
				 m_curToken == TOKEN_TESSELLATION_CONTROL		||
				 m_curToken == TOKEN_TESSELLATION_EVALUATION	||
				 m_curToken == TOKEN_GEOMETRY)
		{
			const Token	token = m_curToken;
			string		source;

			advanceToken();
			assumeToken(TOKEN_SHADER_SOURCE);
			source = parseShaderSource(m_curTokenStr.c_str());
			advanceToken();

			switch (token)
			{
				case TOKEN_VERTEX:					vertexSources.push_back(source);			break;
				case TOKEN_FRAGMENT:				fragmentSources.push_back(source);			break;
				case TOKEN_TESSELLATION_CONTROL:	tessellationCtrlSources.push_back(source);	break;
				case TOKEN_TESSELLATION_EVALUATION:	tessellationEvalSources.push_back(source);	break;
				case TOKEN_GEOMETRY:				geometrySources.push_back(source);			break;
				case TOKEN_BOTH:
				{
					if (!bothSource.empty())
						parseError("multiple 'both' blocks");
					bothSource = source;
					break;
				}

				default:
					parseError(DE_FALSE);
			}
		}
		else if (m_curToken == TOKEN_VERSION)
		{
			advanceToken();
			parseGLSLVersion(version);
		}
		else if (m_curToken == TOKEN_REQUIRE)
		{
			ShaderCase::CaseRequirement requirement;
			parseRequirement(requirement);
			requirements.push_back(requirement);
		}
		else if (m_curToken == TOKEN_PIPELINE_PROGRAM)
		{
			ShaderCase::PipelineProgram pipelineProgram;
			parsePipelineProgram(pipelineProgram);
			pipelinePrograms.push_back(pipelineProgram);
		}
		else
			parseError(string("unexpected token while parsing shader case: " + m_curTokenStr));
	}

	advanceToken(TOKEN_END); // case end

	if (!bothSource.empty())
	{
		if (!vertexSources.empty()				||
			!fragmentSources.empty()			||
			!tessellationCtrlSources.empty()	||
			!tessellationEvalSources.empty()	||
			!geometrySources.empty()			||
			!pipelinePrograms.empty())
		{
			parseError("'both' cannot be mixed with other shader stages");
		}

		// vertex
		{
			ShaderCase::ShaderCaseSpecification spec = ShaderCase::ShaderCaseSpecification::generateSharedSourceVertexCase(expectResult, version, valueBlockList, bothSource);
			spec.requirements = requirements;

			shaderNodeList.push_back(new ShaderCase(m_testCtx, m_renderCtx, m_contextInfo, (caseName + "_vertex").c_str(), description.c_str(), spec));
		}

		// fragment
		{
			ShaderCase::ShaderCaseSpecification spec = ShaderCase::ShaderCaseSpecification::generateSharedSourceFragmentCase(expectResult, version, valueBlockList, bothSource);
			spec.requirements = requirements;

			shaderNodeList.push_back(new ShaderCase(m_testCtx, m_renderCtx, m_contextInfo, (caseName + "_fragment").c_str(), description.c_str(), spec));
		}
	}
	else if (pipelinePrograms.empty())
	{
		ShaderCase::ShaderCaseSpecification spec;

		spec.expectResult	= expectResult;
		spec.caseType		= ShaderCase::CASETYPE_COMPLETE;
		spec.targetVersion	= version;
		spec.requirements.swap(requirements);
		spec.valueBlocks.swap(valueBlockList);
		spec.vertexSources.swap(vertexSources);
		spec.fragmentSources.swap(fragmentSources);
		spec.tessCtrlSources.swap(tessellationCtrlSources);
		spec.tessEvalSources.swap(tessellationEvalSources);
		spec.geometrySources.swap(geometrySources);

		shaderNodeList.push_back(new ShaderCase(m_testCtx, m_renderCtx, m_contextInfo, caseName.c_str(), description.c_str(), spec));
	}
	else
	{
		if (!vertexSources.empty()				||
			!fragmentSources.empty()			||
			!tessellationCtrlSources.empty()	||
			!tessellationEvalSources.empty()	||
			!geometrySources.empty())
		{
			parseError("pipeline programs cannot be mixed with complete programs");
		}

		// Pipeline case, multiple programs
		{
			ShaderCase::PipelineCaseSpecification spec;

			spec.expectResult	= expectResult;
			spec.caseType		= ShaderCase::CASETYPE_COMPLETE;
			spec.targetVersion	= version;
			spec.valueBlocks.swap(valueBlockList);
			spec.programs.swap(pipelinePrograms);

			shaderNodeList.push_back(new ShaderCase(m_testCtx, m_renderCtx, m_contextInfo, caseName.c_str(), description.c_str(), spec));
		}
	}
}

void ShaderParser::parseShaderGroup (vector<tcu::TestNode*>& shaderNodeList)
{
	// Parse 'case'.
	PARSE_DBG(("  parseShaderGroup()\n"));
	advanceToken(TOKEN_GROUP);

	// Parse case name.
	string name = m_curTokenStr;
	advanceToken(); // \note [pyry] We don't want to check token type here (for instance to allow "uniform") group.

	// Parse description.
	assumeToken(TOKEN_STRING);
	string description = parseStringLiteral(m_curTokenStr.c_str());
	advanceToken(TOKEN_STRING);

	std::vector<tcu::TestNode*> children;

	// Parse group children.
	for (;;)
	{
		if (m_curToken == TOKEN_END)
			break;
		else if (m_curToken == TOKEN_GROUP)
			parseShaderGroup(children);
		else if (m_curToken == TOKEN_CASE)
			parseShaderCase(children);
		else if (m_curToken == TOKEN_IMPORT)
			parseImport(children);
		else
			parseError(string("unexpected token while parsing shader group: " + m_curTokenStr));
	}

	advanceToken(TOKEN_END); // group end

	// Create group node.
	tcu::TestCaseGroup* groupNode = new tcu::TestCaseGroup(m_testCtx, name.c_str(), description.c_str(), children);
	shaderNodeList.push_back(groupNode);
}

void ShaderParser::parseImport (vector<tcu::TestNode*>& shaderNodeList)
{
	ShaderLibrary			subLibrary		(m_testCtx, m_renderCtx, m_contextInfo);
	vector<tcu::TestNode*>	importedCases;
	std::string				filename;

	if (!m_currentDir)
		parseError(string("cannot use import in inline shader source"));

	advanceToken(TOKEN_IMPORT);

	assumeToken(TOKEN_STRING);
	filename = m_currentDir + parseStringLiteral(m_curTokenStr.c_str());
	advanceToken(TOKEN_STRING);

	importedCases = subLibrary.loadShaderFile(filename.c_str());
	shaderNodeList.insert(shaderNodeList.end(), importedCases.begin(), importedCases.end());
}

vector<tcu::TestNode*> ShaderParser::parse (const char* input)
{
	// Initialize parser.
	m_input			= input;
	m_curPtr		= m_input.c_str();
	m_curToken		= TOKEN_INVALID;
	m_curTokenStr	= "";
	advanceToken();

	vector<tcu::TestNode*> nodeList;

	// Parse all cases.
	PARSE_DBG(("parse()\n"));
	for (;;)
	{
		if (m_curToken == TOKEN_CASE)
			parseShaderCase(nodeList);
		else if (m_curToken == TOKEN_GROUP)
			parseShaderGroup(nodeList);
		else if (m_curToken == TOKEN_IMPORT)
			parseImport(nodeList);
		else if (m_curToken == TOKEN_EOF)
			break;
		else
			parseError(string("invalid token encountered at main level: '") + m_curTokenStr + "'");
	}

	assumeToken(TOKEN_EOF);
//	printf("  parsed %d test cases.\n", caseList.size());
	return nodeList;
}

} // sl

static std::string getFileDirectory (const std::string& filePath)
{
	const std::string::size_type lastDelim = filePath.find_last_of('/');

	if (lastDelim == std::string::npos)
		return "";
	else
		return filePath.substr(0, lastDelim+1);
}

ShaderLibrary::ShaderLibrary (tcu::TestContext& testCtx, RenderContext& renderCtx, const glu::ContextInfo& contextInfo)
	: m_testCtx			(testCtx)
	, m_renderCtx		(renderCtx)
	, m_contextInfo		(contextInfo)
{
}

ShaderLibrary::~ShaderLibrary (void)
{
}

vector<tcu::TestNode*> ShaderLibrary::loadShaderFile (const char* fileName)
{
	tcu::Resource*		resource		= m_testCtx.getArchive().getResource(fileName);
	std::string			fileDirectory	= getFileDirectory(fileName);
	std::vector<char>	buf;

/*	printf("  loading '%s'\n", fileName);*/

	try
	{
		int size = resource->getSize();
		buf.resize(size + 1);
		resource->read((deUint8*)&buf[0], size);
		buf[size] = '\0';
	}
	catch (const std::exception&)
	{
		delete resource;
		throw;
	}

	delete resource;

	sl::ShaderParser parser(m_testCtx, m_renderCtx, m_contextInfo, fileDirectory.c_str());
	vector<tcu::TestNode*> nodes = parser.parse(&buf[0]);

	return nodes;
}

vector<tcu::TestNode*> ShaderLibrary::parseShader (const char* shaderSource)
{
	sl::ShaderParser parser(m_testCtx, m_renderCtx, m_contextInfo);
	vector<tcu::TestNode*> nodes = parser.parse(shaderSource);

	return nodes;
}

} // gls
} // deqp
