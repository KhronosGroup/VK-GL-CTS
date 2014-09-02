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
 * \brief Texture level state query tests
 *//*--------------------------------------------------------------------*/

#include "es31fTextureLevelStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "tcuTestLog.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "gluContextInfo.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

enum VerifierType
{
	VERIFIER_INT = 0,
	VERIFIER_FLOAT
};

struct TextureGenerationSpec
{
	struct TextureLevelSpec
	{
		int			width;
		int			height;
		int			depth;
		int			level;
		glw::GLenum internalFormat;
		bool		compressed;
	};

	glw::GLenum						bindTarget;
	glw::GLenum						queryTarget;
	bool							immutable;
	bool							fixedSamplePos;	// !< fixed sample pos argument for multisample textures
	int								sampleCount;
	std::vector<TextureLevelSpec>	levels;
	std::string						description;
};

static bool textureTypeHasDepth (glw::GLenum textureBindTarget)
{
	switch (textureBindTarget)
	{
		case GL_TEXTURE_2D:						return false;
		case GL_TEXTURE_3D:						return true;
		case GL_TEXTURE_2D_ARRAY:				return true;
		case GL_TEXTURE_CUBE_MAP:				return false;
		case GL_TEXTURE_2D_MULTISAMPLE:			return false;
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:	return true;
		default:
			DE_ASSERT(DE_FALSE);
			return false;
	}
}

struct IntegerPrinter
{
	static std::string	getIntegerName	(int v)		{ return de::toString(v); }
	static std::string	getFloatName	(float v)	{ return de::toString(v); }
};

struct PixelFormatPrinter
{
	static std::string	getIntegerName	(int v)		{ return de::toString(glu::getPixelFormatStr(v));		}
	static std::string	getFloatName	(float v)	{ return de::toString(glu::getPixelFormatStr((int)v));	}
};

template <typename Printer>
static bool verifyTextureLevelParameterEqualWithPrinter (glu::CallLogWrapper& gl, glw::GLenum target, int level, glw::GLenum pname, int refValue, VerifierType type)
{
	gl.getLog() << tcu::TestLog::Message << "Verifying " << glu::getTextureLevelParameterStr(pname) << ", expecting " << Printer::getIntegerName(refValue) << tcu::TestLog::EndMessage;

	if (type == VERIFIER_INT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<int> result;

		gl.glGetTexLevelParameteriv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameteriv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		if (result == refValue)
			return true;

		gl.getLog() << tcu::TestLog::Message << "Error: Expected " << Printer::getIntegerName(refValue) << ", got " << Printer::getIntegerName(result) << tcu::TestLog::EndMessage;
		return false;
	}
	else if (type == VERIFIER_FLOAT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<float> result;

		gl.glGetTexLevelParameterfv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameterfv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		if (result == (float)refValue)
			return true;

		gl.getLog() << tcu::TestLog::Message << "Error: Expected " << Printer::getIntegerName(refValue) << ", got " << Printer::getFloatName(result) << tcu::TestLog::EndMessage;
		return false;
	}

	DE_ASSERT(DE_FALSE);
	return false;
}

static bool verifyTextureLevelParameterEqual (glu::CallLogWrapper& gl, glw::GLenum target, int level, glw::GLenum pname, int refValue, VerifierType type)
{
	return verifyTextureLevelParameterEqualWithPrinter<IntegerPrinter>(gl, target, level, pname, refValue, type);
}

static bool verifyTextureLevelParameterInternalFormatEqual (glu::CallLogWrapper& gl, glw::GLenum target, int level, glw::GLenum pname, int refValue, VerifierType type)
{
	return verifyTextureLevelParameterEqualWithPrinter<PixelFormatPrinter>(gl, target, level, pname, refValue, type);
}

static bool verifyTextureLevelParameterGreaterOrEqual (glu::CallLogWrapper& gl, glw::GLenum target, int level, glw::GLenum pname, int refValue, VerifierType type)
{
	gl.getLog() << tcu::TestLog::Message << "Verifying " << glu::getTextureLevelParameterStr(pname) << ", expecting " << refValue << " or greater" << tcu::TestLog::EndMessage;

	if (type == VERIFIER_INT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<int> result;

		gl.glGetTexLevelParameteriv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameteriv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		if (result >= refValue)
			return true;

		gl.getLog() << tcu::TestLog::Message << "Error: Expected " << refValue << " or larger, got " << result << tcu::TestLog::EndMessage;
		return false;
	}
	else if (type == VERIFIER_FLOAT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<float> result;

		gl.glGetTexLevelParameterfv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameterfv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		if (result >= (float)refValue)
			return true;

		gl.getLog() << tcu::TestLog::Message << "Error: Expected " << refValue << " or larger, got " << result << tcu::TestLog::EndMessage;
		return false;
	}

	DE_ASSERT(DE_FALSE);
	return false;
}

static bool verifyTextureLevelParameterInternalFormatAnyOf (glu::CallLogWrapper& gl, glw::GLenum target, int level, glw::GLenum pname, const int* refValues, int numRefValues, VerifierType type)
{
	// Log what we try to do
	{
		tcu::MessageBuilder msg(&gl.getLog());

		msg << "Verifying " << glu::getTextureLevelParameterStr(pname) << ", expecting any of {";
		for (int ndx = 0; ndx < numRefValues; ++ndx)
		{
			if (ndx != 0)
				msg << ", ";
			msg << glu::getPixelFormatStr(refValues[ndx]);
		}
		msg << "}";
		msg << tcu::TestLog::EndMessage;
	}

	// verify
	if (type == VERIFIER_INT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<int> result;

		gl.glGetTexLevelParameteriv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameteriv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		for (int ndx = 0; ndx < numRefValues; ++ndx)
			if (result == refValues[ndx])
				return true;

		gl.getLog() << tcu::TestLog::Message << "Error: got " << result << ", (" << glu::getPixelFormatStr(result) << ")" << tcu::TestLog::EndMessage;
		return false;
	}
	else if (type == VERIFIER_FLOAT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<float> result;

		gl.glGetTexLevelParameterfv(target, level, pname, &result);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "glGetTexLevelParameterfv");

		if (result.isUndefined())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: Get* did not write a value." << tcu::TestLog::EndMessage;
			return false;
		}
		else if (result.isMemoryContaminated())
		{
			gl.getLog() << tcu::TestLog::Message << "Error: detected illegal memory write." << tcu::TestLog::EndMessage;
			return false;
		}

		for (int ndx = 0; ndx < numRefValues; ++ndx)
			if (result == (float)refValues[ndx])
				return true;

		gl.getLog() << tcu::TestLog::Message << "Error: got " << result << ", (" << glu::getPixelFormatStr((int)result) << ")" << tcu::TestLog::EndMessage;
		return false;
	}

	DE_ASSERT(DE_FALSE);
	return false;

}

static void generateColorTextureGenerationGroup (std::vector<TextureGenerationSpec>& group, int max2DSamples, int max2DArraySamples, glw::GLenum internalFormat)
{
	// initial setups
	static const struct InitialSetup
	{
		glw::GLenum	bindTarget;
		glw::GLenum	queryTarget;
		bool		immutable;
		const char*	description;
	} initialSetups[] =
	{
		{ GL_TEXTURE_2D,					GL_TEXTURE_2D,					true,	"GL_TEXTURE_2D, initial values"						},
		{ GL_TEXTURE_3D,					GL_TEXTURE_3D,					true,	"GL_TEXTURE_3D, initial values"						},
		{ GL_TEXTURE_2D_ARRAY,				GL_TEXTURE_2D_ARRAY,			true,	"GL_TEXTURE_2D_ARRAY, initial values"				},
		{ GL_TEXTURE_CUBE_MAP,				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,	true,	"GL_TEXTURE_CUBE_MAP, initial values"				},
		{ GL_TEXTURE_2D_MULTISAMPLE,		GL_TEXTURE_2D_MULTISAMPLE,		true,	"GL_TEXTURE_2D_MULTISAMPLE, initial values"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	GL_TEXTURE_2D_MULTISAMPLE_ARRAY,true,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, initial values"	},
	};

	// multisample setups
	static const struct MultisampleSetup
	{
		glw::GLenum	bindTarget;
		int			sampleCount;
		const char*	description;
	} msSetups[] =
	{
		{ GL_TEXTURE_2D_MULTISAMPLE,		1,					"immutable GL_TEXTURE_2D_MULTISAMPLE, low sample count"			},
		{ GL_TEXTURE_2D_MULTISAMPLE,		max2DSamples,		"immutable GL_TEXTURE_2D_MULTISAMPLE, max sample count"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	1,					"immutable GL_TEXTURE_2D_MULTISAMPLE_ARRAY, low sample count"	},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	max2DArraySamples,	"immutable GL_TEXTURE_2D_MULTISAMPLE_ARRAY, max sample count"	},
	};

	// normal setups
	static const struct NormalSetup
	{
		glw::GLenum	bindTarget;
		glw::GLenum	queryTarget;
		bool		immutable;
		int			level;
		const char*	description;
	} normalSetups[] =
	{
		{ GL_TEXTURE_2D,					GL_TEXTURE_2D,					true,	0,	"immutable GL_TEXTURE_2D"					},
		{ GL_TEXTURE_3D,					GL_TEXTURE_3D,					true,	0,	"immutable GL_TEXTURE_3D"					},
		{ GL_TEXTURE_2D_ARRAY,				GL_TEXTURE_2D_ARRAY,			true,	0,	"immutable GL_TEXTURE_2D_ARRAY"				},
		{ GL_TEXTURE_CUBE_MAP,				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,	true,	0,	"immutable GL_TEXTURE_CUBE_MAP"				},
		{ GL_TEXTURE_2D,					GL_TEXTURE_2D,					false,	0,	"mutable GL_TEXTURE_2D"						},
		{ GL_TEXTURE_3D,					GL_TEXTURE_3D,					false,	0,	"mutable GL_TEXTURE_3D"						},
		{ GL_TEXTURE_2D_ARRAY,				GL_TEXTURE_2D_ARRAY,			false,	0,	"mutable GL_TEXTURE_2D_ARRAY"				},
		{ GL_TEXTURE_CUBE_MAP,				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,	false,	0,	"mutable GL_TEXTURE_CUBE_MAP"				},
		{ GL_TEXTURE_2D,					GL_TEXTURE_2D,					false,	3,	"mutable GL_TEXTURE_2D, mip level 3"		},
		{ GL_TEXTURE_3D,					GL_TEXTURE_3D,					false,	3,	"mutable GL_TEXTURE_3D, mip level 3"		},
		{ GL_TEXTURE_2D_ARRAY,				GL_TEXTURE_2D_ARRAY,			false,	3,	"mutable GL_TEXTURE_2D_ARRAY, mip level 3"	},
		{ GL_TEXTURE_CUBE_MAP,				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,	false,	3,	"mutable GL_TEXTURE_CUBE_MAP, mip level 3"	},
	};

	for (int initialSetupNdx = 0; initialSetupNdx < DE_LENGTH_OF_ARRAY(initialSetups); ++initialSetupNdx)
	{
		TextureGenerationSpec texGen;
		texGen.bindTarget		= initialSetups[initialSetupNdx].bindTarget;
		texGen.queryTarget		= initialSetups[initialSetupNdx].queryTarget;
		texGen.immutable		= initialSetups[initialSetupNdx].immutable;
		texGen.sampleCount		= 0;
		texGen.description		= initialSetups[initialSetupNdx].description;

		group.push_back(texGen);
	}

	for (int msSetupNdx = 0; msSetupNdx < DE_LENGTH_OF_ARRAY(msSetups); ++msSetupNdx)
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= msSetups[msSetupNdx].bindTarget;
		texGen.queryTarget		= msSetups[msSetupNdx].bindTarget;
		texGen.immutable		= true;
		texGen.sampleCount		= msSetups[msSetupNdx].sampleCount;
		texGen.fixedSamplePos	= false;
		texGen.description		= msSetups[msSetupNdx].description;

		level.width				= 32;
		level.height			= 32;
		level.depth				= (textureTypeHasDepth(texGen.bindTarget)) ? (8) : (0);
		level.level				= 0;
		level.internalFormat	= internalFormat;
		level.compressed		= false;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}

	for (int normalSetupNdx = 0; normalSetupNdx < DE_LENGTH_OF_ARRAY(normalSetups); ++normalSetupNdx)
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= normalSetups[normalSetupNdx].bindTarget;
		texGen.queryTarget		= normalSetups[normalSetupNdx].queryTarget;
		texGen.immutable		= normalSetups[normalSetupNdx].immutable;
		texGen.sampleCount		= 0;
		texGen.description		= normalSetups[normalSetupNdx].description;

		level.width				= 32;
		level.height			= 32;
		level.depth				= (textureTypeHasDepth(texGen.bindTarget)) ? (8) : (0);
		level.level				= normalSetups[normalSetupNdx].level;
		level.internalFormat	= internalFormat;
		level.compressed		= false;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}
}

static void generateColorMultisampleTextureGenerationGroup (std::vector<TextureGenerationSpec>& group, int max2DSamples, int max2DArraySamples, glw::GLenum internalFormat)
{
	// multisample setups
	static const struct MultisampleSetup
	{
		glw::GLenum	bindTarget;
		bool		initialized;
		int			sampleCount;
		bool		fixedSamples;
		const char*	description;
	} msSetups[] =
	{
		{ GL_TEXTURE_2D_MULTISAMPLE,		false,	0,					false,	"GL_TEXTURE_2D_MULTISAMPLE, initial values"					},
		{ GL_TEXTURE_2D_MULTISAMPLE,		true,	1,					false,	"GL_TEXTURE_2D_MULTISAMPLE, low sample count"				},
		{ GL_TEXTURE_2D_MULTISAMPLE,		true,	max2DSamples,		false,	"GL_TEXTURE_2D_MULTISAMPLE, max sample count"				},
		{ GL_TEXTURE_2D_MULTISAMPLE,		true,	max2DSamples,		true,	"GL_TEXTURE_2D_MULTISAMPLE, fixed sample positions"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	false,	0,					false,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, initial values"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	true,	1,					false,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, low sample count"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	true,	max2DArraySamples,	false,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, max sample count"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	true,	max2DArraySamples,	true,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, fixed sample positions"	},
	};

	for (int msSetupNdx = 0; msSetupNdx < DE_LENGTH_OF_ARRAY(msSetups); ++msSetupNdx)
	{
		TextureGenerationSpec texGen;

		texGen.bindTarget		= msSetups[msSetupNdx].bindTarget;
		texGen.queryTarget		= msSetups[msSetupNdx].bindTarget;
		texGen.immutable		= true;
		texGen.sampleCount		= msSetups[msSetupNdx].sampleCount;
		texGen.fixedSamplePos	= msSetups[msSetupNdx].fixedSamples;
		texGen.description		= msSetups[msSetupNdx].description;

		if (msSetups[msSetupNdx].initialized)
		{
			TextureGenerationSpec::TextureLevelSpec	level;
			level.width				= 32;
			level.height			= 32;
			level.depth				= (textureTypeHasDepth(texGen.bindTarget)) ? (8) : (0);
			level.level				= 0;
			level.internalFormat	= internalFormat;
			level.compressed		= false;

			texGen.levels.push_back(level);
		}

		group.push_back(texGen);
	}
}

static void generateInternalFormatTextureGenerationGroup (std::vector<TextureGenerationSpec>& group)
{
	// initial setups
	static const struct InitialSetup
	{
		glw::GLenum	bindTarget;
		glw::GLenum	queryTarget;
		bool		immutable;
		const char*	description;
	} initialSetups[] =
	{
		{ GL_TEXTURE_2D,					GL_TEXTURE_2D,					true,	"GL_TEXTURE_2D, initial values"						},
		{ GL_TEXTURE_3D,					GL_TEXTURE_3D,					true,	"GL_TEXTURE_3D, initial values"						},
		{ GL_TEXTURE_2D_ARRAY,				GL_TEXTURE_2D_ARRAY,			true,	"GL_TEXTURE_2D_ARRAY, initial values"				},
		{ GL_TEXTURE_CUBE_MAP,				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,	true,	"GL_TEXTURE_CUBE_MAP, initial values"				},
		{ GL_TEXTURE_2D_MULTISAMPLE,		GL_TEXTURE_2D_MULTISAMPLE,		true,	"GL_TEXTURE_2D_MULTISAMPLE, initial values"			},
		{ GL_TEXTURE_2D_MULTISAMPLE_ARRAY,	GL_TEXTURE_2D_MULTISAMPLE_ARRAY,true,	"GL_TEXTURE_2D_MULTISAMPLE_ARRAY, initial values"	},
	};

	// Renderable internal formats (subset)
	static const glw::GLenum renderableInternalFormats[] =
	{
		GL_R8, GL_RGB565, GL_RGB5_A1, GL_RGB10_A2UI, GL_SRGB8_ALPHA8, GL_RG32I,
		GL_RGBA16UI, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24,
		GL_DEPTH_COMPONENT32F, GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8
	};

	// Internal formats
	static const glw::GLenum internalFormats[] =
	{
		GL_R8, GL_R8_SNORM, GL_RG8, GL_RG8_SNORM, GL_RGB8, GL_RGB8_SNORM, GL_RGB565, GL_RGBA4, GL_RGB5_A1,
		GL_RGBA8, GL_RGBA8_SNORM, GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8, GL_SRGB8_ALPHA8, GL_R16F, GL_RG16F,
		GL_RGB16F, GL_RGBA16F, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F, GL_R11F_G11F_B10F, GL_RGB9_E5, GL_R8I,
		GL_R8UI, GL_R16I, GL_R16UI, GL_R32I, GL_R32UI, GL_RG8I, GL_RG8UI, GL_RG16I, GL_RG16UI, GL_RG32I, GL_RG32UI,
		GL_RGB8I, GL_RGB8UI, GL_RGB16I, GL_RGB16UI, GL_RGB32I, GL_RGB32UI, GL_RGBA8I, GL_RGBA8UI, GL_RGBA16I,
		GL_RGBA16UI, GL_RGBA32I, GL_RGBA32UI,

		GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16,
		GL_DEPTH32F_STENCIL8, GL_DEPTH24_STENCIL8
	};

	for (int initialSetupNdx = 0; initialSetupNdx < DE_LENGTH_OF_ARRAY(initialSetups); ++initialSetupNdx)
	{
		TextureGenerationSpec texGen;
		texGen.bindTarget		= initialSetups[initialSetupNdx].bindTarget;
		texGen.queryTarget		= initialSetups[initialSetupNdx].queryTarget;
		texGen.immutable		= initialSetups[initialSetupNdx].immutable;
		texGen.sampleCount		= 0;
		texGen.description		= initialSetups[initialSetupNdx].description;

		group.push_back(texGen);
	}

	// test some color/stencil/depth renderable with multisample texture2d
	for (int internalFormatNdx = 0; internalFormatNdx < DE_LENGTH_OF_ARRAY(renderableInternalFormats); ++internalFormatNdx)
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= GL_TEXTURE_2D_MULTISAMPLE;
		texGen.queryTarget		= GL_TEXTURE_2D_MULTISAMPLE;
		texGen.immutable		= true;
		texGen.sampleCount		= 1;
		texGen.fixedSamplePos	= false;
		texGen.description		= std::string() + "GL_TEXTURE_2D_MULTISAMPLE, internal format " + glu::getPixelFormatName(renderableInternalFormats[internalFormatNdx]);

		level.width				= 32;
		level.height			= 32;
		level.depth				= 0;
		level.level				= 0;
		level.internalFormat	= renderableInternalFormats[internalFormatNdx];
		level.compressed		= false;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}

	// test all with texture2d
	for (int internalFormatNdx = 0; internalFormatNdx < DE_LENGTH_OF_ARRAY(internalFormats); ++internalFormatNdx)
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= GL_TEXTURE_2D;
		texGen.queryTarget		= GL_TEXTURE_2D;
		texGen.immutable		= true;
		texGen.sampleCount		= 0;
		texGen.description		= std::string() + "GL_TEXTURE_2D, internal format " + glu::getPixelFormatName(internalFormats[internalFormatNdx]);

		level.width				= 32;
		level.height			= 32;
		level.depth				= 0;
		level.level				= 0;
		level.internalFormat	= internalFormats[internalFormatNdx];
		level.compressed		= false;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}

	// test rgba8 with mip level 3
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= GL_TEXTURE_2D;
		texGen.queryTarget		= GL_TEXTURE_2D;
		texGen.immutable		= false;
		texGen.sampleCount		= 0;
		texGen.description		= std::string() + "GL_TEXTURE_2D, internal format GL_RGBA8";

		level.width				= 32;
		level.height			= 32;
		level.depth				= 0;
		level.level				= 3;
		level.internalFormat	= GL_RGBA8;
		level.compressed		= false;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}
}

static void generateCompressedTextureGenerationGroup (std::vector<TextureGenerationSpec>& group)
{
	// initial ms
	{
		TextureGenerationSpec texGen;
		texGen.bindTarget	= GL_TEXTURE_2D_MULTISAMPLE;
		texGen.queryTarget	= GL_TEXTURE_2D_MULTISAMPLE;
		texGen.immutable	= true;
		texGen.sampleCount	= 0;
		texGen.description	= "GL_TEXTURE_2D_MULTISAMPLE, initial values";

		group.push_back(texGen);
	}

	// initial non-ms
	{
		TextureGenerationSpec texGen;
		texGen.bindTarget	= GL_TEXTURE_2D;
		texGen.queryTarget	= GL_TEXTURE_2D;
		texGen.immutable	= true;
		texGen.sampleCount	= 0;
		texGen.description	= "GL_TEXTURE_2D, initial values";

		group.push_back(texGen);
	}

	// compressed
	{
		TextureGenerationSpec					texGen;
		TextureGenerationSpec::TextureLevelSpec	level;

		texGen.bindTarget		= GL_TEXTURE_2D;
		texGen.queryTarget		= GL_TEXTURE_2D;
		texGen.immutable		= false;
		texGen.sampleCount		= 0;
		texGen.description		= "GL_TEXTURE_2D, compressed";

		level.width				= 32;
		level.height			= 32;
		level.depth				= 0;
		level.level				= 0;
		level.internalFormat	= GL_COMPRESSED_RGB8_ETC2;
		level.compressed		= true;

		texGen.levels.push_back(level);
		group.push_back(texGen);
	}
}

void applyTextureGenergationSpec (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
{
	DE_ASSERT(!(spec.immutable && spec.levels.size() > 1));		// !< immutable textures have only one level

	for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
	{
		const glu::TransferFormat transferFormat = (spec.levels[levelNdx].compressed) ? (glu::TransferFormat()) : (glu::getTransferFormat(glu::mapGLInternalFormat(spec.levels[levelNdx].internalFormat)));

		if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D)
			gl.glTexStorage2D(spec.bindTarget, 1, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height);
		else if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_3D)
			gl.glTexStorage3D(spec.bindTarget, 1, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, spec.levels[levelNdx].depth);
		else if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D_ARRAY)
			gl.glTexStorage3D(spec.bindTarget, 1, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, spec.levels[levelNdx].depth);
		else if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_CUBE_MAP)
			gl.glTexStorage2D(spec.bindTarget, 1, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height);
		else if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D_MULTISAMPLE)
			gl.glTexStorage2DMultisample(spec.bindTarget, spec.sampleCount, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, (spec.fixedSamplePos) ? (GL_TRUE) : (GL_FALSE));
		else if (spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
			gl.glTexStorage3DMultisample(spec.bindTarget, spec.sampleCount, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, spec.levels[levelNdx].depth, (spec.fixedSamplePos) ? (GL_TRUE) : (GL_FALSE));
		else if (!spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D)
			gl.glTexImage2D(spec.bindTarget, spec.levels[levelNdx].level, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, 0, transferFormat.format, transferFormat.dataType, DE_NULL);
		else if (!spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_3D)
			gl.glTexImage3D(spec.bindTarget, spec.levels[levelNdx].level, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, spec.levels[levelNdx].depth, 0, transferFormat.format, transferFormat.dataType, DE_NULL);
		else if (!spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D_ARRAY)
			gl.glTexImage3D(spec.bindTarget, spec.levels[levelNdx].level, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, spec.levels[levelNdx].depth, 0, transferFormat.format, transferFormat.dataType, DE_NULL);
		else if (!spec.immutable && !spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_CUBE_MAP)
			gl.glTexImage2D(spec.queryTarget, spec.levels[levelNdx].level, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, 0, transferFormat.format, transferFormat.dataType, DE_NULL);
		else if (!spec.immutable && spec.levels[levelNdx].compressed && spec.bindTarget == GL_TEXTURE_2D)
		{
			DE_ASSERT(spec.levels[levelNdx].width == 32);
			DE_ASSERT(spec.levels[levelNdx].height == 32);
			DE_ASSERT(spec.levels[levelNdx].internalFormat == GL_COMPRESSED_RGB8_ETC2);

			static const deUint8 buffer[64 * 8] = { 0 };
			gl.glCompressedTexImage2D(spec.bindTarget, spec.levels[levelNdx].level, spec.levels[levelNdx].internalFormat, spec.levels[levelNdx].width, spec.levels[levelNdx].height, 0, sizeof(buffer), buffer);
		}
		else
			DE_ASSERT(DE_FALSE);

		GLU_EXPECT_NO_ERROR(gl.glGetError(), "set level");
	}
}

class TextureLevelCase : public TestCase
{
public:
										TextureLevelCase		(Context& ctx, const char* name, const char* desc, VerifierType type);
										~TextureLevelCase		(void);

	void								init					(void);
	void								deinit					(void);
	IterateResult						iterate					(void);

protected:
	void								getFormatSamples		(glw::GLenum target, std::vector<int>& samples);
	bool								testConfig				(const TextureGenerationSpec& spec);
	virtual bool						checkTextureState		(glu::CallLogWrapper& gl, const TextureGenerationSpec& spec) = 0;
	virtual void						generateTestIterations	(std::vector<TextureGenerationSpec>& iterations) = 0;

	const VerifierType					m_type;
	const glw::GLenum					m_internalFormat;
	glw::GLuint							m_texture;

private:
	int									m_iteration;
	std::vector<TextureGenerationSpec>	m_iterations;
	bool								m_allIterationsOk;
	std::vector<int>					m_failedIterations;
};

TextureLevelCase::TextureLevelCase (Context& ctx, const char* name, const char* desc, VerifierType type)
	: TestCase			(ctx, name, desc)
	, m_type			(type)
	, m_internalFormat	(GL_RGBA8)
	, m_texture			(0)
	, m_iteration		(0)
	, m_allIterationsOk	(true)
{
}

TextureLevelCase::~TextureLevelCase (void)
{
	deinit();
}

void TextureLevelCase::init (void)
{
	generateTestIterations(m_iterations);
}

void TextureLevelCase::deinit (void)
{
	if (m_texture)
	{
		m_context.getRenderContext().getFunctions().deleteTextures(1, &m_texture);
		m_texture = 0;
	}
}

void TextureLevelCase::getFormatSamples (glw::GLenum target, std::vector<int>& samples)
{
	const glw::Functions	gl			= m_context.getRenderContext().getFunctions();
	int						sampleCount	= -1;

	// fake values for unsupported queries to simplify code. The extension will be checked later for each config anyway (in testConfig())
	if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY && !m_context.getContextInfo().isExtensionSupported("GL_OES_texture_storage_multisample_2d_array"))
	{
		samples.resize(1);
		samples[0] = 0;
		return;
	}

	gl.getInternalformativ(target, m_internalFormat, GL_NUM_SAMPLE_COUNTS, 1, &sampleCount);

	if (sampleCount < 0)
		throw tcu::TestError("internal format query failed");

	samples.resize(sampleCount);

	if (sampleCount > 0)
	{
		gl.getInternalformativ(target, m_internalFormat, GL_SAMPLES, sampleCount, &samples[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "get max samples");
	}
}

TextureLevelCase::IterateResult TextureLevelCase::iterate (void)
{
	const bool result = testConfig(m_iterations[m_iteration]);

	if (!result)
	{
		m_failedIterations.push_back(m_iteration);
		m_allIterationsOk = false;
	}

	if (++m_iteration < (int)m_iterations.size())
		return CONTINUE;

	if (m_allIterationsOk)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
	{
		tcu::MessageBuilder msg(&m_testCtx.getLog());

		msg << "Following iteration(s) failed: ";
		for (int ndx = 0; ndx < (int)m_failedIterations.size(); ++ndx)
		{
			if (ndx)
				msg << ", ";
			msg << (m_failedIterations[ndx] + 1);
		}
		msg << tcu::TestLog::EndMessage;

		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "One or more iterations failed");
	}
	return STOP;
}

bool TextureLevelCase::testConfig (const TextureGenerationSpec& spec)
{
	const tcu::ScopedLogSection section(m_testCtx.getLog(), "Iteration", std::string() + "Iteration " + de::toString(m_iteration+1) + "/" + de::toString((int)m_iterations.size()) + " - " + spec.description);
	glu::CallLogWrapper			gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	bool						result;

	// skip unsupported targets

	if (spec.bindTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY && !m_context.getContextInfo().isExtensionSupported("GL_OES_texture_storage_multisample_2d_array"))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Target binding requires GL_OES_texture_storage_multisample_2d_array extension, skipping." << tcu::TestLog::EndMessage;
		return true;
	}

	// test supported targets

	gl.enableLogging(true);

	gl.glGenTextures(1, &m_texture);
	gl.glBindTexture(spec.bindTarget, m_texture);
	GLU_EXPECT_NO_ERROR(gl.glGetError(), "gen tex");

	// Set the state
	applyTextureGenergationSpec(gl, spec);

	// Verify the state
	result = checkTextureState(gl, spec);

	gl.glDeleteTextures(1, &m_texture);
	m_texture = 0;

	return result;
}

/*--------------------------------------------------------------------*//*!
 * \brief Test all texture targets
 *//*--------------------------------------------------------------------*/
class TextureLevelCommonCase : public TextureLevelCase
{
public:
					TextureLevelCommonCase	(Context& ctx, const char* name, const char* desc, VerifierType type);

protected:
	virtual void	generateTestIterations	(std::vector<TextureGenerationSpec>& iterations);
};

TextureLevelCommonCase::TextureLevelCommonCase (Context& ctx, const char* name, const char* desc, VerifierType type)
	: TextureLevelCase(ctx, name, desc, type)
{
}

void TextureLevelCommonCase::generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
{
	std::vector<int> texture2DSamples;
	std::vector<int> texture2DArraySamples;

	getFormatSamples(GL_TEXTURE_2D_MULTISAMPLE, texture2DSamples);
	getFormatSamples(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, texture2DArraySamples);

	TCU_CHECK(!texture2DSamples.empty());
	TCU_CHECK(!texture2DArraySamples.empty());

	// gen iterations

	generateColorTextureGenerationGroup(iterations, texture2DSamples[0], texture2DArraySamples[0], m_internalFormat);
}

/*--------------------------------------------------------------------*//*!
 * \brief Test all multisample texture targets
 *//*--------------------------------------------------------------------*/
class TextureLevelMultisampleCase : public TextureLevelCase
{
public:
					TextureLevelMultisampleCase	(Context& ctx, const char* name, const char* desc, VerifierType type);

protected:
	virtual void	generateTestIterations		(std::vector<TextureGenerationSpec>& iterations);
};

TextureLevelMultisampleCase::TextureLevelMultisampleCase (Context& ctx, const char* name, const char* desc, VerifierType type)
	: TextureLevelCase(ctx, name, desc, type)
{
}

void TextureLevelMultisampleCase::generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
{
	std::vector<int> texture2DSamples;
	std::vector<int> texture2DArraySamples;

	getFormatSamples(GL_TEXTURE_2D_MULTISAMPLE, texture2DSamples);
	getFormatSamples(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, texture2DArraySamples);

	TCU_CHECK(!texture2DSamples.empty());
	TCU_CHECK(!texture2DArraySamples.empty());

	// gen iterations

	generateColorMultisampleTextureGenerationGroup(iterations, texture2DSamples[0], texture2DArraySamples[0], m_internalFormat);
}

class TextureLevelSampleCase : public TextureLevelMultisampleCase
{
public:
	TextureLevelSampleCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelMultisampleCase(ctx, name, desc, type)
	{
	}

private:
	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		const int queryLevel	= (spec.levels.empty()) ? (0) : (spec.levels[0].level);
		const int refValue		= (spec.levels.empty()) ? (0) : (spec.sampleCount);

		return verifyTextureLevelParameterGreaterOrEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_SAMPLES, refValue, m_type);
	}
};

class TextureLevelFixedSamplesCase : public TextureLevelMultisampleCase
{
public:
	TextureLevelFixedSamplesCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelMultisampleCase(ctx, name, desc, type)
	{
	}

private:
	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		const int queryLevel	= 0;
		const int refValue		= (spec.levels.empty()) ? (1) : ((spec.fixedSamplePos) ? (1) : (0));

		return verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS, refValue, m_type);
	}
};

class TextureLevelWidthCase : public TextureLevelCommonCase
{
public:
	TextureLevelWidthCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelCommonCase(ctx, name, desc, type)
	{
	}

private:
	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		const int	initialValue	= 0;
		bool		allOk			= true;

		if (spec.levels.empty())
		{
			const int queryLevel	= 0;
			const int refValue		= initialValue;

			allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_WIDTH, refValue, m_type);
		}
		else
		{
			for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
			{
				const int queryLevel	= spec.levels[levelNdx].level;
				const int refValue		= spec.levels[levelNdx].width;

				allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_WIDTH, refValue, m_type);
			}
		}

		return allOk;
	}
};

class TextureLevelHeightCase : public TextureLevelCommonCase
{
public:
	TextureLevelHeightCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelCommonCase(ctx, name, desc, type)
	{
	}

private:
	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		const int	initialValue	= 0;
		bool		allOk			= true;

		if (spec.levels.empty())
		{
			const int queryLevel	= 0;
			const int refValue		= initialValue;

			allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_HEIGHT, refValue, m_type);
		}
		else
		{
			for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
			{
				const int queryLevel	= spec.levels[levelNdx].level;
				const int refValue		= spec.levels[levelNdx].height;

				allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_HEIGHT, refValue, m_type);
			}
		}

		return allOk;
	}
};

class TextureLevelDepthCase : public TextureLevelCommonCase
{
public:
	TextureLevelDepthCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelCommonCase(ctx, name, desc, type)
	{
	}

private:

	void generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
	{
		std::vector<TextureGenerationSpec> allIterations;
		this->TextureLevelCommonCase::generateTestIterations(allIterations);

		// test only cases with depth
		for (int ndx = 0; ndx < (int)allIterations.size(); ++ndx)
			if (textureTypeHasDepth(allIterations[ndx].bindTarget))
				iterations.push_back(allIterations[ndx]);
	}

	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		const int	initialValue	= 0;
		bool		allOk			= true;

		if (spec.levels.empty())
		{
			const int queryLevel	= 0;
			const int refValue		= initialValue;

			allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_DEPTH, refValue, m_type);
		}
		else
		{
			for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
			{
				const int queryLevel	= spec.levels[levelNdx].level;
				const int refValue		= spec.levels[levelNdx].depth;

				allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_DEPTH, refValue, m_type);
			}
		}

		return allOk;
	}
};

class TextureLevelInternalFormatCase : public TextureLevelCase
{
public:
	TextureLevelInternalFormatCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelCase(ctx, name, desc, type)
	{
	}

private:
	void generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
	{
		generateInternalFormatTextureGenerationGroup(iterations);
	}

	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		bool allOk = true;

		if (spec.levels.empty())
		{
			const int queryLevel		= 0;
			const int initialValues[2]	= { GL_RGBA, GL_R8 };

			allOk &= verifyTextureLevelParameterInternalFormatAnyOf(gl, spec.queryTarget, queryLevel, GL_TEXTURE_INTERNAL_FORMAT, initialValues, DE_LENGTH_OF_ARRAY(initialValues), m_type);
		}
		else
		{
			for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
			{
				const int queryLevel	= spec.levels[levelNdx].level;
				const int refValue		= spec.levels[levelNdx].internalFormat;

				allOk &= verifyTextureLevelParameterInternalFormatEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_INTERNAL_FORMAT, refValue, m_type);
			}
		}

		return allOk;
	}
};

class TextureLevelSizeCase : public TextureLevelCase
{
public:
						TextureLevelSizeCase			(Context& ctx, const char* name, const char* desc, VerifierType type, glw::GLenum pname);

private:
	void				generateTestIterations			(std::vector<TextureGenerationSpec>& iterations);
	bool				checkTextureState				(glu::CallLogWrapper& gl, const TextureGenerationSpec& spec);
	int					getMinimumComponentResolution	(glw::GLenum internalFormat);

	const glw::GLenum	m_pname;
};

TextureLevelSizeCase::TextureLevelSizeCase (Context& ctx, const char* name, const char* desc, VerifierType type, glw::GLenum pname)
	: TextureLevelCase	(ctx, name, desc, type)
	, m_pname			(pname)
{
}

void TextureLevelSizeCase::generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
{
	generateInternalFormatTextureGenerationGroup(iterations);
}

bool TextureLevelSizeCase::checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
{
	bool allOk = true;

	if (spec.levels.empty())
	{
		allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, 0, m_pname, 0, m_type);
	}
	else
	{
		for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
		{
			const int queryLevel	= spec.levels[levelNdx].level;
			const int refValue		= getMinimumComponentResolution(spec.levels[levelNdx].internalFormat);

			allOk &= verifyTextureLevelParameterGreaterOrEqual(gl, spec.queryTarget, queryLevel, m_pname, refValue, m_type);
		}
	}

	return allOk;
}

int TextureLevelSizeCase::getMinimumComponentResolution (glw::GLenum internalFormat)
{
	const tcu::TextureFormat	format			= glu::mapGLInternalFormat(internalFormat);
	const tcu::IVec4			channelBitDepth	= tcu::getTextureFormatBitDepth(format);

	switch (m_pname)
	{
		case GL_TEXTURE_RED_SIZE:
			if (format.order == tcu::TextureFormat::R		||
				format.order == tcu::TextureFormat::RG		||
				format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelBitDepth[0];
			else
				return 0;

		case GL_TEXTURE_GREEN_SIZE:
			if (format.order == tcu::TextureFormat::RG		||
				format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelBitDepth[1];
			else
				return 0;

		case GL_TEXTURE_BLUE_SIZE:
			if (format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelBitDepth[2];
			else
				return 0;

		case GL_TEXTURE_ALPHA_SIZE:
			if (format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelBitDepth[3];
			else
				return 0;

		case GL_TEXTURE_DEPTH_SIZE:
			if (format.order == tcu::TextureFormat::D	||
				format.order == tcu::TextureFormat::DS)
				return channelBitDepth[0];
			else
				return 0;

		case GL_TEXTURE_STENCIL_SIZE:
			if (format.order == tcu::TextureFormat::DS)
				return channelBitDepth[3];
			else
				return 0;

		case GL_TEXTURE_SHARED_SIZE:
			if (internalFormat == GL_RGB9_E5)
				return 5;
			else
				return 0;
		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

class TextureLevelTypeCase : public TextureLevelCase
{
public:
						TextureLevelTypeCase			(Context& ctx, const char* name, const char* desc, VerifierType type, glw::GLenum pname);

private:
	void				generateTestIterations			(std::vector<TextureGenerationSpec>& iterations);
	bool				checkTextureState				(glu::CallLogWrapper& gl, const TextureGenerationSpec& spec);
	int					getComponentType				(glw::GLenum internalFormat);

	const glw::GLenum	m_pname;
};

TextureLevelTypeCase::TextureLevelTypeCase (Context& ctx, const char* name, const char* desc, VerifierType type, glw::GLenum pname)
	: TextureLevelCase	(ctx, name, desc, type)
	, m_pname			(pname)
{
}

void TextureLevelTypeCase::generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
{
	generateInternalFormatTextureGenerationGroup(iterations);
}

bool TextureLevelTypeCase::checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
{
	bool allOk = true;

	if (spec.levels.empty())
	{
		allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, 0, m_pname, GL_NONE, m_type);
	}
	else
	{
		for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
		{
			const int queryLevel	= spec.levels[levelNdx].level;
			const int refValue		= getComponentType(spec.levels[levelNdx].internalFormat);

			allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, m_pname, refValue, m_type);
		}
	}

	return allOk;
}

int TextureLevelTypeCase::getComponentType (glw::GLenum internalFormat)
{
	const tcu::TextureFormat		format			= glu::mapGLInternalFormat(internalFormat);
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);
	glw::GLenum						channelType		= GL_NONE;

	// depth-stencil special cases
	if (format.type == tcu::TextureFormat::UNSIGNED_INT_24_8)
	{
		if (m_pname == GL_TEXTURE_DEPTH_TYPE)
			return GL_UNSIGNED_NORMALIZED;
		else
			return GL_NONE;
	}
	else if (format.type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
	{
		if (m_pname == GL_TEXTURE_DEPTH_TYPE)
			return GL_FLOAT;
		else
			return GL_NONE;
	}
	else
	{
		switch (channelClass)
		{
			case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:		channelType = GL_SIGNED_NORMALIZED;		break;
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:		channelType = GL_UNSIGNED_NORMALIZED;	break;
			case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:			channelType = GL_INT;					break;
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:			channelType = GL_UNSIGNED_INT;			break;
			case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:			channelType = GL_FLOAT;					break;
			default:
				DE_ASSERT(DE_FALSE);
		}
	}

	switch (m_pname)
	{
		case GL_TEXTURE_RED_TYPE:
			if (format.order == tcu::TextureFormat::R		||
				format.order == tcu::TextureFormat::RG		||
				format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelType;
			else
				return GL_NONE;

		case GL_TEXTURE_GREEN_TYPE:
			if (format.order == tcu::TextureFormat::RG		||
				format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelType;
			else
				return GL_NONE;

		case GL_TEXTURE_BLUE_TYPE:
			if (format.order == tcu::TextureFormat::RGB		||
				format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelType;
			else
				return GL_NONE;

		case GL_TEXTURE_ALPHA_TYPE:
			if (format.order == tcu::TextureFormat::RGBA	||
				format.order == tcu::TextureFormat::BGRA	||
				format.order == tcu::TextureFormat::ARGB	||
				format.order == tcu::TextureFormat::sRGBA)
				return channelType;
			else
				return GL_NONE;

		case GL_TEXTURE_DEPTH_TYPE:
			if (format.order == tcu::TextureFormat::D	||
				format.order == tcu::TextureFormat::DS)
				return channelType;
			else
				return GL_NONE;

		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

class TextureLevelCompressedCase : public TextureLevelCase
{
public:
	TextureLevelCompressedCase (Context& ctx, const char* name, const char* desc, VerifierType type)
		: TextureLevelCase(ctx, name, desc, type)
	{
	}

private:
	void generateTestIterations (std::vector<TextureGenerationSpec>& iterations)
	{
		generateCompressedTextureGenerationGroup(iterations);
	}

	bool checkTextureState (glu::CallLogWrapper& gl, const TextureGenerationSpec& spec)
	{
		bool allOk = true;

		if (spec.levels.empty())
		{
			allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, 0, GL_TEXTURE_COMPRESSED, 0, m_type);
		}
		else
		{
			for (int levelNdx = 0; levelNdx < (int)spec.levels.size(); ++levelNdx)
			{
				const int queryLevel	= spec.levels[levelNdx].level;
				const int refValue		= (spec.levels[levelNdx].compressed) ? (1) : (0);

				allOk &= verifyTextureLevelParameterEqual(gl, spec.queryTarget, queryLevel, GL_TEXTURE_COMPRESSED, refValue, m_type);
			}
		}

		return allOk;
	}
};

} // anonymous

TextureLevelStateQueryTests::TextureLevelStateQueryTests (Context& context)
	: TestCaseGroup(context, "texture_level", "GetTexLevelParameter tests")
{
}

TextureLevelStateQueryTests::~TextureLevelStateQueryTests (void)
{
}

void TextureLevelStateQueryTests::init (void)
{
	tcu::TestCaseGroup* const integerGroup = new tcu::TestCaseGroup(m_testCtx, "integer", "use GetTexLevelParameteriv");
	tcu::TestCaseGroup* const floatGroup = new tcu::TestCaseGroup(m_testCtx, "float", "use GetTexLevelParameterfv");

	addChild(integerGroup);
	addChild(floatGroup);

	for (int groupNdx = 0; groupNdx < 2; ++groupNdx)
	{
		tcu::TestCaseGroup* const	group		= (groupNdx == 0) ? (integerGroup) : (floatGroup);
		const VerifierType			verifier	= (groupNdx == 0) ? (VERIFIER_INT) : (VERIFIER_FLOAT);


		group->addChild(new TextureLevelSampleCase			(m_context, "texture_samples",					"Verify TEXTURE_SAMPLES",					verifier));
		group->addChild(new TextureLevelFixedSamplesCase	(m_context, "texture_fixed_sample_locations",	"Verify TEXTURE_FIXED_SAMPLE_LOCATIONS",	verifier));
		group->addChild(new TextureLevelWidthCase			(m_context, "texture_width",					"Verify TEXTURE_WIDTH",						verifier));
		group->addChild(new TextureLevelHeightCase			(m_context, "texture_height",					"Verify TEXTURE_HEIGHT",					verifier));
		group->addChild(new TextureLevelDepthCase			(m_context, "texture_depth",					"Verify TEXTURE_DEPTH",						verifier));
		group->addChild(new TextureLevelInternalFormatCase	(m_context, "texture_internal_format",			"Verify TEXTURE_INTERNAL_FORMAT",			verifier));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_red_size",					"Verify TEXTURE_RED_SIZE",					verifier,	GL_TEXTURE_RED_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_green_size",				"Verify TEXTURE_GREEN_SIZE",				verifier,	GL_TEXTURE_GREEN_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_blue_size",				"Verify TEXTURE_BLUE_SIZE",					verifier,	GL_TEXTURE_BLUE_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_alpha_size",				"Verify TEXTURE_ALPHA_SIZE",				verifier,	GL_TEXTURE_ALPHA_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_depth_size",				"Verify TEXTURE_DEPTH_SIZE",				verifier,	GL_TEXTURE_DEPTH_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_stencil_size",				"Verify TEXTURE_STENCIL_SIZE",				verifier,	GL_TEXTURE_STENCIL_SIZE));
		group->addChild(new TextureLevelSizeCase			(m_context, "texture_shared_size",				"Verify TEXTURE_SHARED_SIZE",				verifier,	GL_TEXTURE_SHARED_SIZE));
		group->addChild(new TextureLevelTypeCase			(m_context, "texture_red_type",					"Verify TEXTURE_RED_TYPE",					verifier,	GL_TEXTURE_RED_TYPE));
		group->addChild(new TextureLevelTypeCase			(m_context, "texture_green_type",				"Verify TEXTURE_GREEN_TYPE",				verifier,	GL_TEXTURE_GREEN_TYPE));
		group->addChild(new TextureLevelTypeCase			(m_context, "texture_blue_type",				"Verify TEXTURE_BLUE_TYPE",					verifier,	GL_TEXTURE_BLUE_TYPE));
		group->addChild(new TextureLevelTypeCase			(m_context, "texture_alpha_type",				"Verify TEXTURE_ALPHA_TYPE",				verifier,	GL_TEXTURE_ALPHA_TYPE));
		group->addChild(new TextureLevelTypeCase			(m_context, "texture_depth_type",				"Verify TEXTURE_DEPTH_TYPE",				verifier,	GL_TEXTURE_DEPTH_TYPE));
		group->addChild(new TextureLevelCompressedCase		(m_context, "texture_compressed",				"Verify TEXTURE_COMPRESSED",				verifier));
	}
}

} // Functional
} // gles31
} // deqp
