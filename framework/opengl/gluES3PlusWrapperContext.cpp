/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief OpenGL ES 3plus wrapper context.
 *//*--------------------------------------------------------------------*/

#include "gluES3PlusWrapperContext.hpp"
#include "gluRenderContext.hpp"
#include "gluRenderConfig.hpp"
#include "glwInitFunctions.hpp"
#include "glwFunctionLoader.hpp"
#include "gluContextFactory.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderUtil.hpp"
#include "deThreadLocal.hpp"
#include "deSTLUtil.hpp"
#include "deUniquePtr.hpp"
#include "glwEnums.hpp"

#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <map>

namespace glu
{

namespace es3plus
{

using std::vector;
using std::string;

class Context
{
public:
								Context			(const glu::RenderContext& ctx);
								~Context		(void);

	void						addExtension	(const char* name);

	const glw::Functions&		gl;			//!< GL 4.3 core context functions.

	// Wrapper state.
	string						vendor;
	string						version;
	string						renderer;
	string						shadingLanguageVersion;
	string						extensions;
	vector<string>				extensionList;
	bool						primitiveRestartEnabled;

	deUint32					defaultVAO;
	bool						defaultVAOBound;

	const glu::GLSLVersion		nativeGLSLVersion;
};

Context::Context (const glu::RenderContext& ctx)
	: gl						(ctx.getFunctions())
	, vendor					("drawElements")
	, version					("OpenGL ES 3.1")
	, renderer					((const char*)gl.getString(GL_RENDERER))
	, shadingLanguageVersion	("OpenGL ES GLSL ES 3.1")
	, primitiveRestartEnabled	(false)
	, defaultVAO				(0)
	, defaultVAOBound			(false)
	, nativeGLSLVersion			(glu::getContextTypeGLSLVersion(ctx.getType()))
{
	const de::UniquePtr<glu::ContextInfo> ctxInfo(glu::ContextInfo::create(ctx));

	gl.genVertexArrays(1, &defaultVAO);
	if (gl.getError() != GL_NO_ERROR || defaultVAO == 0)
		throw tcu::InternalError("Failed to allocate VAO for emulation");

	gl.bindVertexArray(defaultVAO);
	if (gl.getError() != GL_NO_ERROR)
		throw tcu::InternalError("Failed to bind default VAO");
	defaultVAOBound = true;

	gl.enable(GL_PROGRAM_POINT_SIZE);
	gl.getError(); // supress potential errors, feature is not critical

	gl.enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	gl.getError(); // suppress

	// Extensions
	addExtension("GL_OES_texture_stencil8");
	addExtension("GL_OES_sample_shading");
	addExtension("GL_OES_sample_variables");
	addExtension("GL_OES_shader_multisample_interpolation");
	addExtension("GL_OES_shader_image_atomic");
	addExtension("GL_OES_texture_storage_multisample_2d_array");

	// Enable only if base ctx supports these or compatible GL_NV_blend_equation_advanced ext
	if (ctxInfo->isExtensionSupported("GL_NV_blend_equation_advanced") ||
		ctxInfo->isExtensionSupported("GL_KHR_blend_equation_advanced"))
	{
		addExtension("GL_KHR_blend_equation_advanced");
	}
	if (ctxInfo->isExtensionSupported("GL_NV_blend_equation_advanced_coherent") ||
		ctxInfo->isExtensionSupported("GL_KHR_blend_equation_advanced_coherent"))
	{
		addExtension("GL_KHR_blend_equation_advanced_coherent");
	}

	addExtension("GL_EXT_shader_io_blocks");
	addExtension("GL_EXT_geometry_shader");
	addExtension("GL_EXT_geometry_point_size");
	addExtension("GL_EXT_tessellation_shader");
	addExtension("GL_EXT_tessellation_point_size");
	addExtension("GL_EXT_gpu_shader5");
	addExtension("GL_KHR_debug");
	addExtension("GL_EXT_texture_cube_map_array");
	addExtension("GL_EXT_shader_implicit_conversions");
	addExtension("GL_EXT_primitive_bounding_box");
	addExtension("GL_EXT_texture_sRGB_decode");
	addExtension("GL_EXT_texture_border_clamp");
	addExtension("GL_EXT_texture_buffer");
	addExtension("GL_EXT_draw_buffers_indexed");
}

Context::~Context (void)
{
	if (defaultVAO)
		gl.deleteVertexArrays(1, &defaultVAO);
}

void Context::addExtension (const char* name)
{
	if (!extensions.empty())
		extensions += " ";
	extensions += name;

	extensionList.push_back(name);
}

static de::ThreadLocal tls_context;

void setCurrentContext (Context* context)
{
	tls_context.set(context);
}

inline Context* getCurrentContext (void)
{
	return (Context*)tls_context.get();
}

static GLW_APICALL void GLW_APIENTRY getIntegerv (deUint32 pname, deInt32* params)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (pname == GL_NUM_EXTENSIONS && params)
			*params = (deInt32)context->extensionList.size();
		else
			context->gl.getIntegerv(pname, params);
	}
}

static GLW_APICALL const glw::GLubyte* GLW_APIENTRY getString (deUint32 name)
{
	Context* context = getCurrentContext();

	if (context)
	{
		switch (name)
		{
			case GL_VENDOR:						return (const glw::GLubyte*)context->vendor.c_str();
			case GL_VERSION:					return (const glw::GLubyte*)context->version.c_str();
			case GL_RENDERER:					return (const glw::GLubyte*)context->renderer.c_str();
			case GL_SHADING_LANGUAGE_VERSION:	return (const glw::GLubyte*)context->shadingLanguageVersion.c_str();
			case GL_EXTENSIONS:					return (const glw::GLubyte*)context->extensions.c_str();
			default:							return context->gl.getString(name);
		}
	}
	else
		return DE_NULL;
}

static GLW_APICALL const glw::GLubyte* GLW_APIENTRY getStringi (deUint32 name, deUint32 index)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (name == GL_EXTENSIONS)
		{
			if ((size_t)index < context->extensionList.size())
				return (const glw::GLubyte*)context->extensionList[index].c_str();
			else
				return context->gl.getStringi(name, ~0u);
		}
		else
			return context->gl.getStringi(name, index);
	}
	else
		return DE_NULL;
}

static GLW_APICALL void GLW_APIENTRY enable (deUint32 cap)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX)
		{
			context->primitiveRestartEnabled = true;
			// \todo [2013-09-30 pyry] Call to glPrimitiveRestartIndex() is required prior to all draw calls!
		}
		else
			context->gl.enable(cap);
	}
}

static GLW_APICALL void GLW_APIENTRY disable (deUint32 cap)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX)
			context->primitiveRestartEnabled = false;
		else
			context->gl.disable(cap);
	}
}

static GLW_APICALL void GLW_APIENTRY bindVertexArray (deUint32 array)
{
	Context* context = getCurrentContext();

	if (context)
	{
		context->gl.bindVertexArray(array == 0 ? context->defaultVAO : array);
		context->defaultVAOBound = (array == 0);
	}
}

static GLW_APICALL void GLW_APIENTRY hint (deUint32 target, deUint32 mode)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (target != GL_GENERATE_MIPMAP_HINT)
			context->gl.hint(target, mode);
		// \todo [2013-09-30 pyry] Verify mode.
	}
}

static void translateShaderSource (deUint32 shaderType, std::ostream& dst, const std::string& src, const std::vector<std::string>& filteredExtensions, GLSLVersion version)
{
	bool				foundVersion		= false;
	std::istringstream	istr				(src);
	std::string			line;
	int					srcLineNdx			= 1;
	bool				preprocessorSection	= true;

	while (std::getline(istr, line, '\n'))
	{
		if (preprocessorSection && !line.empty() && line[0] != '#')
		{
			preprocessorSection = false;

			// ARB_separate_shader_objects requires gl_PerVertex to be explicitly declared
			if (shaderType == GL_VERTEX_SHADER)
			{
				dst << "out gl_PerVertex {\n"
					<< "    vec4 gl_Position;\n"
					<< "    float gl_PointSize;\n"
					<< "    float gl_ClipDistance[];\n"
					<< "};\n"
					<< "#line " << (srcLineNdx + 1) << "\n";
			}
			else if (shaderType == GL_TESS_CONTROL_SHADER)
			{
				dst << "#extension GL_ARB_tessellation_shader : enable\n"
					<< "in gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "} gl_in[gl_MaxPatchVertices];\n"
					<< "out gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "} gl_out[];\n"
					<< "#line " << (srcLineNdx + 1) << "\n";
			}
			else if (shaderType == GL_TESS_EVALUATION_SHADER)
			{
				dst << "#extension GL_ARB_tessellation_shader : enable\n"
					<< "in gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "} gl_in[gl_MaxPatchVertices];\n"
					<< "out gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "};\n"
					<< "#line " << (srcLineNdx + 1) << "\n";
			}
			else if (shaderType == GL_GEOMETRY_SHADER)
			{
				dst << "in gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "} gl_in[];\n"
					<< "out gl_PerVertex {\n"
					<< "	highp vec4 gl_Position;\n"
					<< "	highp float gl_PointSize;\n"
					<< "};\n"
					<< "#line " << (srcLineNdx + 1) << "\n";
			}

			// GL_EXT_primitive_bounding_box tessellation no-op fallback
			if (shaderType == GL_TESS_CONTROL_SHADER)
			{
				dst << "#define gl_BoundingBoxEXT _dummy_unused_output_for_primitive_bbox\n"
					<< "patch out vec4 _dummy_unused_output_for_primitive_bbox[2];\n"
					<< "#line " << (srcLineNdx + 1) << "\n";
			}
		}

		if (line == "#version 310 es")
		{
			foundVersion = true;
			dst << glu::getGLSLVersionDeclaration(version) << "\n";
		}
		else if (line == "#version 300 es")
		{
			foundVersion = true;
			dst << "#version 330\n";
		}
		else if (line.substr(0, 10) == "precision ")
		{
			const size_t	precPos		= 10;
			const size_t	precEndPos	= line.find(' ', precPos);
			const size_t	endPos		= line.find(';');

			if (precEndPos != std::string::npos && endPos != std::string::npos && endPos > precEndPos+1)
			{
				const size_t		typePos		= precEndPos+1;
				const std::string	precision	= line.substr(precPos, precEndPos-precPos);
				const std::string	type		= line.substr(typePos, endPos-typePos);
				const bool			precOk		= precision == "lowp" || precision == "mediump" || precision == "highp";

				if (precOk &&
					(type == "image2D" || type == "uimage2D" || type == "iimage2D" ||
					 type == "imageCube" || type == "uimageCube" || type == "iimageCube" ||
					 type == "image3D" || type == "iimage3D" || type == "uimage3D" ||
					 type == "image2DArray" || type == "iimage2DArray" || type == "uimage2DArray" ||
					 type == "imageCubeArray" || type == "iimageCubeArray" || type == "uimageCubeArray"))
					dst << "// "; // Filter out statement
			}

			dst << line << "\n";
		}
		else if (line.substr(0, 11) == "#extension ")
		{
			const size_t	extNamePos		= 11;
			const size_t	extNameEndPos	= line.find_first_of(" :", extNamePos);
			const size_t	behaviorPos		= line.find_first_not_of(" :", extNameEndPos);

			if (extNameEndPos != std::string::npos && behaviorPos != std::string::npos)
			{
				const std::string	extName				= line.substr(extNamePos, extNameEndPos-extNamePos);
				const std::string	behavior			= line.substr(behaviorPos);
				const bool			filteredExtension	= de::contains(filteredExtensions.begin(), filteredExtensions.end(), extName);
				const bool			validBehavior		= behavior == "require" || behavior == "enable" || behavior == "warn" || behavior == "disable";

				if (filteredExtension && validBehavior)
					dst << "// "; // Filter out extension
			}
			dst << line << "\n";
		}
		else if (line.substr(0, 21) == "layout(blend_support_")
			dst << "// " << line << "\n";
		else
			dst << line << "\n";

		srcLineNdx += 1;
	}

	DE_ASSERT(foundVersion);
	DE_UNREF(foundVersion);
}

static std::string translateShaderSources (deUint32 shaderType, deInt32 count, const char* const* strings, const int* length, const std::vector<std::string>& filteredExtensions, GLSLVersion version)
{
	std::ostringstream	srcIn;
	std::ostringstream	srcOut;

	for (int ndx = 0; ndx < count; ndx++)
	{
		const int len = length && length[ndx] >= 0 ? length[ndx] : (int)strlen(strings[ndx]);
		srcIn << std::string(strings[ndx], strings[ndx] + len);
	}

	translateShaderSource(shaderType, srcOut, srcIn.str(), filteredExtensions, version);

	return srcOut.str();
}

static GLW_APICALL void GLW_APIENTRY shaderSource (deUint32 shader, deInt32 count, const char* const* strings, const int* length)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (count > 0 && strings)
		{
			deInt32				shaderType = GL_NONE;
			context->gl.getShaderiv(shader, GL_SHADER_TYPE, &shaderType);
			{
				const std::string	translatedSrc	= translateShaderSources(shaderType, count, strings, length, context->extensionList, context->nativeGLSLVersion);
				const char*			srcPtr			= translatedSrc.c_str();
				context->gl.shaderSource(shader, 1, &srcPtr, DE_NULL);
			}
		}
		else
			context->gl.shaderSource(shader, count, strings, length);
	}
}

static GLW_APICALL void GLW_APIENTRY bindFramebuffer (deUint32 target, deUint32 framebuffer)
{
	Context* context = getCurrentContext();

	if (context)
	{
		context->gl.bindFramebuffer(target, framebuffer);

		// Emulate ES behavior where sRGB conversion is only controlled by color buffer format.
		if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
			((framebuffer != 0) ? context->gl.enable : context->gl.disable)(GL_FRAMEBUFFER_SRGB);
	}
}

static GLW_APICALL void GLW_APIENTRY blendBarrierKHR (void)
{
	Context* context = getCurrentContext();

	if (context)
	{
		// \todo [2014-03-18 pyry] Use BlendBarrierNV() if supported
		context->gl.finish();
	}
}

static GLW_APICALL deUint32 GLW_APIENTRY createShaderProgramv (deUint32 type, deInt32 count, const char* const* strings)
{
	Context* context = getCurrentContext();

	if (context)
	{
		if (count > 0 && strings)
		{
			const std::string	translatedSrc	= translateShaderSources(type, count, strings, DE_NULL, context->extensionList, context->nativeGLSLVersion);
			const char*			srcPtr			= translatedSrc.c_str();
			return context->gl.createShaderProgramv(type, 1, &srcPtr);
		}
		else
			return context->gl.createShaderProgramv(type, count, strings);
	}
	return 0;
}

static GLW_APICALL void GLW_APIENTRY dummyPrimitiveBoundingBox (float minX, float minY, float minZ, float minW, float maxX, float maxY, float maxZ, float maxW)
{
	// dummy no-op. No-op is a valid implementation. States queries are not emulated.
	DE_UNREF(minX);
	DE_UNREF(minY);
	DE_UNREF(minZ);
	DE_UNREF(minW);
	DE_UNREF(maxX);
	DE_UNREF(maxY);
	DE_UNREF(maxZ);
	DE_UNREF(maxW);
}

static void initFunctions (glw::Functions* dst, const glw::Functions& src)
{
	// Functions directly passed to GL context.
#include "gluES3PlusWrapperFuncs.inl"

	// Wrapped functions.
	dst->bindVertexArray		= bindVertexArray;
	dst->disable				= disable;
	dst->enable					= enable;
	dst->getIntegerv			= getIntegerv;
	dst->getString				= getString;
	dst->getStringi				= getStringi;
	dst->hint					= hint;
	dst->shaderSource			= shaderSource;
	dst->createShaderProgramv	= createShaderProgramv;
	dst->bindFramebuffer		= bindFramebuffer;

	// Extension functions
	{
		using std::map;

		class ExtFuncLoader : public glw::FunctionLoader
		{
		public:
			ExtFuncLoader (const map<string, glw::GenericFuncType>& extFuncs)
				: m_extFuncs(extFuncs)
			{
			}

			glw::GenericFuncType get (const char* name) const
			{
				map<string, glw::GenericFuncType>::const_iterator pos = m_extFuncs.find(name);
				return pos != m_extFuncs.end() ? pos->second : DE_NULL;
			}

		private:
			const map<string, glw::GenericFuncType>& m_extFuncs;
		};

		map<string, glw::GenericFuncType>	extFuncMap;
		const ExtFuncLoader					extFuncLoader	(extFuncMap);

		// OES_sample_shading
		extFuncMap["glMinSampleShadingOES"]			= (glw::GenericFuncType)src.minSampleShading;

		// OES_texture_storage_multisample_2d_array
		extFuncMap["glTexStorage3DMultisampleOES"]	= (glw::GenericFuncType)src.texStorage3DMultisample;

		// KHR_blend_equation_advanced
		extFuncMap["glBlendBarrierKHR"]				= (glw::GenericFuncType)blendBarrierKHR;

		// EXT_tessellation_shader
		extFuncMap["glPatchParameteriEXT"]			= (glw::GenericFuncType)src.patchParameteri;

		// EXT_geometry_shader
		extFuncMap["glFramebufferTextureEXT"]		= (glw::GenericFuncType)src.framebufferTexture;

		// KHR_debug
		extFuncMap["glDebugMessageControlKHR"]		= (glw::GenericFuncType)src.debugMessageControl;
		extFuncMap["glDebugMessageInsertKHR"]		= (glw::GenericFuncType)src.debugMessageInsert;
		extFuncMap["glDebugMessageCallbackKHR"]		= (glw::GenericFuncType)src.debugMessageCallback;
		extFuncMap["glGetDebugMessageLogKHR"]		= (glw::GenericFuncType)src.getDebugMessageLog;
		extFuncMap["glGetPointervKHR"] 				= (glw::GenericFuncType)src.getPointerv;
		extFuncMap["glPushDebugGroupKHR"]			= (glw::GenericFuncType)src.pushDebugGroup;
		extFuncMap["glPopDebugGroupKHR"] 			= (glw::GenericFuncType)src.popDebugGroup;
		extFuncMap["glObjectLabelKHR"] 				= (glw::GenericFuncType)src.objectLabel;
		extFuncMap["glGetObjectLabelKHR"]			= (glw::GenericFuncType)src.getObjectLabel;
		extFuncMap["glObjectPtrLabelKHR"]			= (glw::GenericFuncType)src.objectPtrLabel;
		extFuncMap["glGetObjectPtrLabelKHR"]		= (glw::GenericFuncType)src.getObjectPtrLabel;

		// GL_EXT_primitive_bounding_box (dummy no-op)
		extFuncMap["glPrimitiveBoundingBoxEXT"]		= (glw::GenericFuncType)dummyPrimitiveBoundingBox;

		// GL_EXT_texture_border_clamp
		extFuncMap["glTexParameterIivEXT"]			= (glw::GenericFuncType)src.texParameterIiv;
		extFuncMap["glTexParameterIuivEXT"]			= (glw::GenericFuncType)src.texParameterIuiv;
		extFuncMap["glGetTexParameterIivEXT"]		= (glw::GenericFuncType)src.getTexParameterIiv;
		extFuncMap["glGetTexParameterIuivEXT"]		= (glw::GenericFuncType)src.getTexParameterIuiv;
		extFuncMap["glSamplerParameterIivEXT"]		= (glw::GenericFuncType)src.samplerParameterIiv;
		extFuncMap["glSamplerParameterIuivEXT"]		= (glw::GenericFuncType)src.samplerParameterIuiv;
		extFuncMap["glGetSamplerParameterIivEXT"]	= (glw::GenericFuncType)src.getSamplerParameterIiv;
		extFuncMap["glGetSamplerParameterIuivEXT"]	= (glw::GenericFuncType)src.getSamplerParameterIuiv;

		// GL_EXT_texture_buffer
		extFuncMap["glTexBufferEXT"]				= (glw::GenericFuncType)src.texBuffer;
		extFuncMap["glTexBufferRangeEXT"]			= (glw::GenericFuncType)src.texBufferRange;

		// GL_EXT_draw_buffers_indexed
		extFuncMap["glEnableiEXT"]					= (glw::GenericFuncType)src.enablei;
		extFuncMap["glDisableiEXT"]					= (glw::GenericFuncType)src.disablei;
		extFuncMap["glBlendEquationiEXT"]			= (glw::GenericFuncType)src.blendEquationi;
		extFuncMap["glBlendEquationSeparateiEXT"]	= (glw::GenericFuncType)src.blendEquationSeparatei;
		extFuncMap["glBlendFunciEXT"]				= (glw::GenericFuncType)src.blendFunci;
		extFuncMap["glBlendFuncSeparateiEXT"]		= (glw::GenericFuncType)src.blendFuncSeparatei;
		extFuncMap["glColorMaskiEXT"]				= (glw::GenericFuncType)src.colorMaski;
		extFuncMap["glIsEnablediEXT"]				= (glw::GenericFuncType)src.isEnabledi;

		{
			int	numExts	= 0;
			dst->getIntegerv(GL_NUM_EXTENSIONS, &numExts);

			if (numExts > 0)
			{
				vector<const char*> extStr(numExts);

				for (int ndx = 0; ndx < numExts; ndx++)
					extStr[ndx] = (const char*)dst->getStringi(GL_EXTENSIONS, ndx);

				glw::initExtensionsES(dst, &extFuncLoader, (int)extStr.size(), &extStr[0]);
			}
		}
	}
}

} // es3plus

ES3PlusWrapperContext::ES3PlusWrapperContext (const ContextFactory& factory, const RenderConfig& config, const tcu::CommandLine& cmdLine)
	: m_context		(DE_NULL)
	, m_wrapperCtx	(DE_NULL)
{
	// Flags that are valid for both core & es context. Currently only excludes CONTEXT_FORWARD_COMPATIBLE
	const ContextFlags validContextFlags = CONTEXT_ROBUST | CONTEXT_DEBUG;

	static const ContextType wrappableNativeTypes[] =
	{
		ContextType(ApiType::core(4,4), config.type.getFlags() & validContextFlags),	// !< higher in the list, preferred
		ContextType(ApiType::core(4,3), config.type.getFlags() & validContextFlags),
	};

	if (config.type.getAPI() != ApiType::es(3,1))
		throw tcu::NotSupportedError("Unsupported context type (ES3.1 wrapper supports only ES3.1)");

	// try to create any wrappable context

	for (int nativeCtxNdx = 0; nativeCtxNdx < DE_LENGTH_OF_ARRAY(wrappableNativeTypes); ++nativeCtxNdx)
	{
		glu::ContextType nativeContext = wrappableNativeTypes[nativeCtxNdx];

		try
		{
			glu::RenderConfig nativeConfig = config;
			nativeConfig.type = nativeContext;

			m_context		= factory.createContext(nativeConfig, cmdLine);
			m_wrapperCtx	= new es3plus::Context(*m_context);

			es3plus::setCurrentContext(m_wrapperCtx);
			es3plus::initFunctions(&m_functions, m_context->getFunctions());
			break;
		}
		catch (...)
		{
			es3plus::setCurrentContext(DE_NULL);

			delete m_wrapperCtx;
			delete m_context;

			m_wrapperCtx = DE_NULL;
			m_context = DE_NULL;

			// throw only if all tries failed (that is, this was the last potential target)
			if (nativeCtxNdx + 1 == DE_LENGTH_OF_ARRAY(wrappableNativeTypes))
				throw;
			else
				continue;
		}
	}
}

ES3PlusWrapperContext::~ES3PlusWrapperContext (void)
{
	delete m_wrapperCtx;
	delete m_context;
}

ContextType ES3PlusWrapperContext::getType (void) const
{
	return ContextType(ApiType::es(3,1), m_context->getType().getFlags());
}

} // glu
