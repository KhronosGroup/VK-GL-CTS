/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
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
 * \brief Function table initialization.
 *//*--------------------------------------------------------------------*/

#include "glwInitFunctions.hpp"
#include "deSTLUtil.hpp"

#include <string>
#include <set>

namespace glw
{

// \todo [2014-03-19 pyry] Replace this with more generic system based on upstream XML spec desc.

void initES20 (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitES20.inl"
}

void initES30 (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitES30.inl"
}

void initES31 (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitES31.inl"
}

void initGL30Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL30.inl"
}

void initGL31Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL31.inl"
}

void initGL32Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL32.inl"
}

void initGL33Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL33.inl"
}

void initGL40Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL40.inl"
}

void initGL41Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL41.inl"
}

void initGL42Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL42.inl"
}

void initGL43Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL43.inl"
}

void initGL44Core (Functions* gl, const FunctionLoader* loader)
{
#include "glwInitGL44.inl"
}

void initExtensionsShared (Functions* gl, const FunctionLoader* loader, const std::set<std::string>& extensions)
{
	if (de::contains(extensions, "GL_KHR_blend_equation_advanced"))
	{
		gl->blendBarrierKHR		= (glBlendBarrierKHRFunc)		loader->get("glBlendBarrierKHR");
	}
}

void initExtensionsGL (Functions* gl, const FunctionLoader* loader, int numExtensions, const char* const* extensions)
{
	using std::string;
	using std::set;

	const set<string> extSet(extensions, extensions+numExtensions);

	initExtensionsShared(gl, loader, extSet);

	if (de::contains(extSet, "GL_KHR_debug"))
	{
		/*
			From the spec:
				NOTE: when implemented in an OpenGL ES context, all entry points defined
				by this extension must have a "KHR" suffix. When implemented in an
				OpenGL context, all entry points must have NO suffix, as shown below.
		*/
		gl->debugMessageControl = (glDebugMessageControlFunc)	loader->get("glDebugMessageControl");
		gl->debugMessageInsert	= (glDebugMessageInsertFunc)	loader->get("glDebugMessageInsert");
		gl->debugMessageCallback= (glDebugMessageCallbackFunc)	loader->get("glDebugMessageCallback");
		gl->getDebugMessageLog	= (glGetDebugMessageLogFunc)	loader->get("glGetDebugMessageLog");
		gl->getPointerv			= (glGetPointervFunc)			loader->get("glGetPointerv");
		gl->pushDebugGroup		= (glPushDebugGroupFunc)		loader->get("glPushDebugGroup");
		gl->popDebugGroup		= (glPopDebugGroupFunc)			loader->get("glPopDebugGroup");
		gl->objectLabel			= (glObjectLabelFunc)			loader->get("glObjectLabel");
		gl->getObjectLabel		= (glGetObjectLabelFunc)		loader->get("glGetObjectLabel");
		gl->objectPtrLabel		= (glObjectPtrLabelFunc)		loader->get("glObjectPtrLabel");
		gl->getObjectPtrLabel	= (glGetObjectPtrLabelFunc)		loader->get("glGetObjectPtrLabel");
	}
}

void initExtensionsES (Functions* gl, const FunctionLoader* loader, int numExtensions, const char* const* extensions)
{
	using std::string;
	using std::set;

	const set<string> extSet(extensions, extensions+numExtensions);

	initExtensionsShared(gl, loader, extSet);

	if (de::contains(extSet, "GL_OES_sample_shading"))
	{
		gl->minSampleShading		= (glMinSampleShadingFunc)			loader->get("glMinSampleShadingOES");
	}

	if (de::contains(extSet, "GL_OES_texture_storage_multisample_2d_array"))
	{
		gl->texStorage3DMultisample	= (glTexStorage3DMultisampleFunc)	loader->get("glTexStorage3DMultisampleOES");
	}

	if (de::contains(extSet, "GL_KHR_debug"))
	{
		/*
			From the spec:
				NOTE: when implemented in an OpenGL ES context, all entry points defined
				by this extension must have a "KHR" suffix. When implemented in an
				OpenGL context, all entry points must have NO suffix, as shown below.
		*/
		gl->debugMessageControl		= (glDebugMessageControlFunc)		loader->get("glDebugMessageControlKHR");
		gl->debugMessageInsert		= (glDebugMessageInsertFunc)		loader->get("glDebugMessageInsertKHR");
		gl->debugMessageCallback	= (glDebugMessageCallbackFunc)		loader->get("glDebugMessageCallbackKHR");
		gl->getDebugMessageLog		= (glGetDebugMessageLogFunc)		loader->get("glGetDebugMessageLogKHR");
		gl->getPointerv				= (glGetPointervFunc)				loader->get("glGetPointervKHR");
		gl->pushDebugGroup			= (glPushDebugGroupFunc)			loader->get("glPushDebugGroupKHR");
		gl->popDebugGroup			= (glPopDebugGroupFunc)				loader->get("glPopDebugGroupKHR");
		gl->objectLabel				= (glObjectLabelFunc)				loader->get("glObjectLabelKHR");
		gl->getObjectLabel			= (glGetObjectLabelFunc)			loader->get("glGetObjectLabelKHR");
		gl->objectPtrLabel			= (glObjectPtrLabelFunc)			loader->get("glObjectPtrLabelKHR");
		gl->getObjectPtrLabel		= (glGetObjectPtrLabelFunc)			loader->get("glGetObjectPtrLabelKHR");
	}

	if (de::contains(extSet, "GL_EXT_tessellation_shader"))
	{
		gl->patchParameteri			= (glPatchParameteriFunc)			loader->get("glPatchParameteriEXT");
	}

	if (de::contains(extSet, "GL_EXT_geometry_shader"))
	{
		gl->framebufferTexture		= (glFramebufferTextureFunc)		loader->get("glFramebufferTextureEXT");
	}

	if (de::contains(extSet, "GL_EXT_texture_buffer"))
	{
		gl->texBuffer				= (glTexBufferFunc)					loader->get("glTexBufferEXT");
		gl->texBufferRange			= (glTexBufferRangeFunc)			loader->get("glTexBufferRangeEXT");
	}
}

} // glw
