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
 * \brief Utilities for framebuffer objects.
 *//*--------------------------------------------------------------------*/

#include "glsFboUtil.hpp"

#include "glwEnums.hpp"
#include "deUniquePtr.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"
#include "deStringUtil.hpp"
#include <sstream>

using namespace glw;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::NotSupportedError;
using glu::TransferFormat;
using glu::mapGLInternalFormat;
using glu::mapGLTransferFormat;
using glu::getPixelFormatName;
using glu::getTypeName;
using glu::getFramebufferTargetName;
using glu::getFramebufferAttachmentName;
using glu::getFramebufferAttachmentTypeName;
using glu::getTextureTargetName;
using glu::getTransferFormat;
using glu::ContextInfo;
using glu::ContextType;
using glu::RenderContext;
using de::UniquePtr;
using de::toString;
using std::set;
using std::vector;
using std::string;
using std::istringstream;
using std::istream_iterator;

namespace deqp
{
namespace gls
{

namespace FboUtil
{


void FormatDB::addFormat (ImageFormat format, FormatFlags newFlags)
{
	FormatFlags& flags = m_map[format];
	flags = FormatFlags(flags | newFlags);
}

// Not too fast at the moment, might consider indexing?
Formats FormatDB::getFormats (FormatFlags requirements) const
{
	Formats ret;
	for (FormatMap::const_iterator it = m_map.begin(); it != m_map.end(); it++)
	{
		if ((it->second & requirements) == requirements)
			ret.insert(it->first);
	}
	return ret;
}

FormatFlags FormatDB::getFormatInfo (ImageFormat format, FormatFlags fallback) const
{
	return lookupDefault(m_map, format, fallback);
}

void addFormats (FormatDB& db, FormatEntries stdFmts)
{
	for (const FormatEntry* it = stdFmts.begin(); it != stdFmts.end(); it++)
	{
		for (const FormatKey* it2 = it->second.begin(); it2 != it->second.end(); it2++)
			db.addFormat(formatKeyInfo(*it2), it->first);
	}
}

void addExtFormats (FormatDB& db, FormatExtEntries extFmts, const RenderContext* ctx)
{
	const UniquePtr<ContextInfo> ctxInfo(ctx != DE_NULL ? ContextInfo::create(*ctx) : DE_NULL);
	for (const FormatExtEntry* it = extFmts.begin(); it != extFmts.end(); it++)
	{
		bool supported = true;
		if (ctxInfo)
		{
			istringstream tokenStream(string(it->extensions));
			istream_iterator<string> tokens((tokenStream)), end;

			while (tokens != end)
			{
				if (!ctxInfo->isExtensionSupported(tokens->c_str()))
				{
					supported = false;
					break;
				}
				++tokens;
			}
		}
		if (supported)
			for (const FormatKey* i2 = it->formats.begin(); i2 != it->formats.end(); i2++)
				db.addFormat(formatKeyInfo(*i2), FormatFlags(it->flags));
	}
}

FormatFlags formatFlag (GLenum context)
{
	switch (context)
	{
		case GL_NONE:
			return FormatFlags(0);
		case GL_RENDERBUFFER:
			return RENDERBUFFER_VALID;
		case GL_TEXTURE:
			return TEXTURE_VALID;
		case GL_STENCIL_ATTACHMENT:
			return STENCIL_RENDERABLE;
		case GL_DEPTH_ATTACHMENT:
			return DEPTH_RENDERABLE;
		default:
			DE_ASSERT(context >= GL_COLOR_ATTACHMENT0 && context <= GL_COLOR_ATTACHMENT15);
			return COLOR_RENDERABLE;
	}
}

namespace config {

GLsizei	imageNumSamples	(const Image& img)
{
	if (const Renderbuffer* rbo = dynamic_cast<const Renderbuffer*>(&img))
		return rbo->numSamples;
	return 0;
}

static GLenum glTarget (const Image& img)
{
	if (dynamic_cast<const Renderbuffer*>(&img) != DE_NULL)
		return GL_RENDERBUFFER;
	if (dynamic_cast<const Texture2D*>(&img) != DE_NULL)
		return GL_TEXTURE_2D;
	if (dynamic_cast<const TextureCubeMap*>(&img) != DE_NULL)
		return GL_TEXTURE_CUBE_MAP;
	if (dynamic_cast<const Texture3D*>(&img) != DE_NULL)
		return GL_TEXTURE_3D;
	if (dynamic_cast<const Texture2DArray*>(&img) != DE_NULL)
		return GL_TEXTURE_2D_ARRAY;

	DE_ASSERT(!"Impossible image type");
	return GL_NONE;
}

static void glInitFlat (const TextureFlat& cfg, GLenum target, const glw::Functions& gl)
{
	const TransferFormat format = transferImageFormat(cfg.internalFormat);
	GLint w = cfg.width;
	GLint h = cfg.height;
	for (GLint level = 0; level < cfg.numLevels; level++)
	{
		gl.texImage2D(target, level, cfg.internalFormat.format, w, h, 0,
					  format.format, format.dataType, DE_NULL);
		w = de::max(1, w / 2);
		h = de::max(1, h / 2);
	}
}

static void glInitLayered (const TextureLayered& cfg,
						   GLint depth_divider, const glw::Functions& gl)
{
	const TransferFormat format = transferImageFormat(cfg.internalFormat);
	GLint w = cfg.width;
	GLint h = cfg.height;
	GLint depth = cfg.numLayers;
	for (GLint level = 0; level < cfg.numLevels; level++)
	{
		gl.texImage3D(glTarget(cfg), level, cfg.internalFormat.format, w, h, depth, 0,
					  format.format, format.dataType, DE_NULL);
		w = de::max(1, w / 2);
		h = de::max(1, h / 2);
		depth = de::max(1, depth / depth_divider);
	}
}

static void glInit (const Texture& cfg, const glw::Functions& gl)
{
	if (const Texture2D* t2d = dynamic_cast<const Texture2D*>(&cfg))
		glInitFlat(*t2d, glTarget(*t2d), gl);
	else if (const TextureCubeMap* tcm = dynamic_cast<const TextureCubeMap*>(&cfg))
	{
		// \todo [2013-12-05 lauri]
		// move this to glu or someplace sensible (this array is already
		// present in duplicates)
		static const GLenum s_cubeMapFaces[] =
			{
				GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X,
				GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
				GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
				GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
				GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
			};
		const Range<GLenum> range = GLS_ARRAY_RANGE(s_cubeMapFaces);
		for (const GLenum* it = range.begin(); it != range.end(); it++)
			glInitFlat(*tcm, *it, gl);
	}
	else if (const Texture3D* t3d = dynamic_cast<const Texture3D*>(&cfg))
		glInitLayered(*t3d, 2, gl);
	else if (const Texture2DArray* t2a = dynamic_cast<const Texture2DArray*>(&cfg))
		glInitLayered(*t2a, 1, gl);
}

static GLuint glCreate (const Image& cfg, const glw::Functions& gl)
{
	GLuint ret = 0;
	if (const Renderbuffer* const rbo = dynamic_cast<const Renderbuffer*>(&cfg))
	{
		gl.genRenderbuffers(1, &ret);
		gl.bindRenderbuffer(GL_RENDERBUFFER, ret);

		if (rbo->numSamples == 0)
			gl.renderbufferStorage(GL_RENDERBUFFER, rbo->internalFormat.format,
								   rbo->width, rbo->height);
		else
			gl.renderbufferStorageMultisample(
				GL_RENDERBUFFER, rbo->numSamples, rbo->internalFormat.format,
				rbo->width, rbo->height);

		gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	else if (const Texture* const tex = dynamic_cast<const Texture*>(&cfg))
	{
		gl.genTextures(1, &ret);
		gl.bindTexture(glTarget(*tex), ret);
		glInit(*tex, gl);
		gl.bindTexture(glTarget(*tex), 0);
	}
	else
		DE_ASSERT(!"Impossible image type");
	return ret;
}

static void glDelete (const Image& cfg, GLuint img, const glw::Functions& gl)
{
	if (dynamic_cast<const Renderbuffer*>(&cfg) != DE_NULL)
		gl.deleteRenderbuffers(1, &img);
	else if (dynamic_cast<const Texture*>(&cfg) != DE_NULL)
		gl.deleteTextures(1, &img);
	else
		DE_ASSERT(!"Impossible image type");
}

static void attachAttachment (const Attachment& att, GLenum attPoint,
							  const glw::Functions& gl)
{
	if (const RenderbufferAttachment* const rAtt =
		dynamic_cast<const RenderbufferAttachment*>(&att))
		gl.framebufferRenderbuffer(rAtt->target, attPoint,
								   rAtt->renderbufferTarget, rAtt->imageName);
	else if (const TextureFlatAttachment* const fAtt =
			 dynamic_cast<const TextureFlatAttachment*>(&att))
		gl.framebufferTexture2D(fAtt->target, attPoint,
								fAtt->texTarget, fAtt->imageName, fAtt->level);
	else if (const TextureLayerAttachment* const lAtt =
			 dynamic_cast<const TextureLayerAttachment*>(&att))
		gl.framebufferTextureLayer(lAtt->target, attPoint,
								   lAtt->imageName, lAtt->level, lAtt->layer);
	else
		DE_ASSERT(!"Impossible attachment type");
}

GLenum attachmentType (const Attachment& att)
{
	if (dynamic_cast<const RenderbufferAttachment*>(&att) != DE_NULL)
		return GL_RENDERBUFFER;
	else if (dynamic_cast<const TextureAttachment*>(&att) != DE_NULL)
		return GL_TEXTURE;

	DE_ASSERT(!"Impossible attachment type");
	return GL_NONE;
}

static GLsizei textureLayer (const TextureAttachment& tAtt)
{
	if (dynamic_cast<const TextureFlatAttachment*>(&tAtt) != DE_NULL)
		return 0;
	else if (const TextureLayerAttachment* const lAtt =
			 dynamic_cast<const TextureLayerAttachment*>(&tAtt))
		return lAtt->layer;

	DE_ASSERT(!"Impossible attachment type");
	return 0;
}

static void checkAttachmentCompleteness (Checker& cctx, const Attachment& attachment,
										 GLenum attPoint, const Image* image,
										 const FormatDB& db)
{
	// GLES2 4.4.5 / GLES3 4.4.4, "Framebuffer attachment completeness"

	if (const TextureAttachment* const texAtt =
		dynamic_cast<const TextureAttachment*>(&attachment))
		if (const TextureLayered* const ltex = dynamic_cast<const TextureLayered*>(image))
		{
			// GLES3: "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is
			// TEXTURE and the value of FRAMEBUFFER_ATTACHMENT_OBJECT_NAME names a
			// three-dimensional texture, then the value of
			// FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER must be smaller than the depth
			// of the texture.
			//
			// GLES3: "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is
			// TEXTURE and the value of FRAMEBUFFER_ATTACHMENT_OBJECT_NAME names a
			// two-dimensional array texture, then the value of
			// FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER must be smaller than the
			// number of layers in the texture.

			cctx.require(textureLayer(*texAtt) < ltex->numLayers,
						 GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
		}

	// "The width and height of image are non-zero."
	cctx.require(image->width > 0 && image->height > 0, GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);

	// Check for renderability
	FormatFlags flags = db.getFormatInfo(image->internalFormat, ANY_FORMAT);
	// If the format does not have the proper renderability flag, the
	// completeness check _must_ fail.
	cctx.require((flags & formatFlag(attPoint)) != 0, GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
	// If the format is only optionally renderable, the completeness check _can_ fail.
	cctx.canRequire((flags & REQUIRED_RENDERABLE) != 0,
					GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
}

} // namespace config

using namespace config;

void Checker::require (bool condition, GLenum error)
{
	if (!condition)
	{
		m_statusCodes.erase(GL_FRAMEBUFFER_COMPLETE);
		m_statusCodes.insert(error);
	}
}

void Checker::canRequire (bool condition, GLenum error)
{
	if (!condition)
		m_statusCodes.insert(error);
}

FboVerifier::FboVerifier (const FormatDB& formats, CheckerFactory& factory)
	: m_formats				(formats)
	, m_factory				(factory)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Return acceptable framebuffer status codes.
 *
 * This function examines the framebuffer configuration descriptor `fboConfig`
 * and returns the set of status codes that `glCheckFramebufferStatus` is
 * allowed to return on a conforming implementation when given a framebuffer
 * whose configuration adheres to `fboConfig`.
 *
 * The returned set is guaranteed to be non-empty, but it may contain multiple
 * INCOMPLETE statuses (if there are multiple errors in the spec), or or a mix
 * of COMPLETE and INCOMPLETE statuses (if supporting a FBO with this spec is
 * optional). Furthermore, the statuses may contain GL error codes, which
 * indicate that trying to create a framebuffer configuration like this could
 * have failed with an error (if one was checked for) even before
 * `glCheckFramebufferStatus` was ever called.
 *
 *//*--------------------------------------------------------------------*/
StatusCodes FboVerifier::validStatusCodes (const Framebuffer& fboConfig) const
{
	const AttachmentMap& atts = fboConfig.attachments;
	const UniquePtr<Checker> cctx(m_factory.createChecker());

	for (TextureMap::const_iterator it = fboConfig.textures.begin();
		 it != fboConfig.textures.end(); it++)
	{
		const FormatFlags flags =
			m_formats.getFormatInfo(it->second->internalFormat, ANY_FORMAT);
		cctx->require((flags & TEXTURE_VALID) != 0, GL_INVALID_ENUM);
		cctx->require((flags & TEXTURE_VALID) != 0, GL_INVALID_OPERATION);
		cctx->require((flags & TEXTURE_VALID) != 0, GL_INVALID_VALUE);
	}

	for (RboMap::const_iterator it = fboConfig.rbos.begin(); it != fboConfig.rbos.end(); it++)
	{
		const FormatFlags flags =
			m_formats.getFormatInfo(it->second->internalFormat, ANY_FORMAT);
		cctx->require((flags & RENDERBUFFER_VALID) != 0, GL_INVALID_ENUM);
	}

	// "There is at least one image attached to the framebuffer."
	// TODO: support XXX_framebuffer_no_attachments
	cctx->require(!atts.empty(), GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);

	for (AttachmentMap::const_iterator it = atts.begin(); it != atts.end(); it++)
	{
		const GLenum attPoint = it->first;
		const Attachment& att = *it->second;
		const Image* const image = fboConfig.getImage(attachmentType(att), att.imageName);
		checkAttachmentCompleteness(*cctx, att, attPoint, image, m_formats);
		cctx->check(it->first, *it->second, image);
	}

	return cctx->getStatusCodes();
}


void Framebuffer::attach (glw::GLenum attPoint, const Attachment* att)
{
	if (att == DE_NULL)
		attachments.erase(attPoint);
	else
		attachments[attPoint] = att;
}

const Image* Framebuffer::getImage (GLenum type, glw::GLuint imgName) const
{
	switch (type)
	{
		case GL_TEXTURE:
			return lookupDefault(textures, imgName, DE_NULL);
		case GL_RENDERBUFFER:
			return lookupDefault(rbos, imgName, DE_NULL);
		default:
			DE_ASSERT(!"Bad image type");
	}
	return DE_NULL; // shut up compiler warning
}

void Framebuffer::setTexture (glw::GLuint texName, const Texture& texCfg)
{
	textures[texName] = &texCfg;
}

void Framebuffer::setRbo (glw::GLuint rbName, const Renderbuffer& rbCfg)
{
	rbos[rbName] = &rbCfg;
}

static void logField (TestLog& log, const string& field, const string& value)
{
	log << TestLog::Message << field << ": " << value << TestLog::EndMessage;
}

static void logImage (const Image& img, TestLog& log, bool useType)
{
	const GLenum type = img.internalFormat.unsizedType;
	logField(log, "Internal format",	getPixelFormatName(img.internalFormat.format));
	if (useType && type != GL_NONE)
		logField(log, "Format type",	getTypeName(type));
	logField(log, "Width", 				toString(img.width));
	logField(log, "Height",				toString(img.height));
}

static void logRenderbuffer (const Renderbuffer& rbo, TestLog& log)
{
	logImage(rbo, log, false);
	logField(log, "Samples",			toString(rbo.numSamples));
}

static void logTexture (const Texture& tex, TestLog& log)
{
	logField(log, "Type",				glu::getTextureTargetName(glTarget(tex)));
	logImage(tex, log, true);
	logField(log, "Levels",				toString(tex.numLevels));
	if (const TextureLayered* const lTex = dynamic_cast<const TextureLayered*>(&tex))
		logField(log, "Layers",				toString(lTex->numLayers));
}

static void logAttachment (const Attachment& att, TestLog& log)
{
	logField(log, "Target",				getFramebufferTargetName(att.target));
	logField(log, "Type",				getFramebufferAttachmentTypeName(attachmentType(att)));
	logField(log, "Image Name",			toString(att.imageName));
	if (const RenderbufferAttachment* const rAtt
		= dynamic_cast<const RenderbufferAttachment*>(&att))
	{
		DE_UNREF(rAtt); // To shut up compiler during optimized builds.
		DE_ASSERT(rAtt->renderbufferTarget == GL_RENDERBUFFER);
		logField(log, "Renderbuffer Target",	"GL_RENDERBUFFER");
	}
	else if (const TextureAttachment* const tAtt = dynamic_cast<const TextureAttachment*>(&att))
	{
		logField(log, "Mipmap Level",		toString(tAtt->level));
		if (const TextureFlatAttachment* const fAtt =
			dynamic_cast<const TextureFlatAttachment*>(tAtt))
			logField(log, "Texture Target",		getTextureTargetName(fAtt->texTarget));
		else if (const TextureLayerAttachment* const lAtt =
			dynamic_cast<const TextureLayerAttachment*>(tAtt))
			logField(log, "Layer",				toString(lAtt->level));
	}
}

void logFramebufferConfig (const Framebuffer& cfg, TestLog& log)
{
	log << TestLog::Section("Framebuffer", "Framebuffer configuration");

	const string rboDesc = cfg.rbos.empty()
		? "No renderbuffers were created"
		: "Renderbuffers created";
	log << TestLog::Section("Renderbuffers", rboDesc);
	for (RboMap::const_iterator it = cfg.rbos.begin(); it != cfg.rbos.end(); ++it)
	{
		const string num = toString(it->first);
		log << TestLog::Section(num, "Renderbuffer " + num);
		logRenderbuffer(*it->second, log);
		log << TestLog::EndSection;
	}
	log << TestLog::EndSection; // Renderbuffers

	const string texDesc = cfg.textures.empty()
		? "No textures were created"
		: "Textures created";
	log << TestLog::Section("Textures", texDesc);
	for (TextureMap::const_iterator it = cfg.textures.begin();
		 it != cfg.textures.end(); ++it)
	{
		const string num = toString(it->first);
		log << TestLog::Section(num, "Texture " + num);
		logTexture(*it->second, log);
		log << TestLog::EndSection;
	}
	log << TestLog::EndSection; // Textures

	const string attDesc = cfg.attachments.empty()
		? "Framebuffer has no attachments"
		: "Framebuffer attachments";
	log << TestLog::Section("Attachments", attDesc);
	for (AttachmentMap::const_iterator it = cfg.attachments.begin();
		 it != cfg.attachments.end(); it++)
	{
		const string attPointName = getFramebufferAttachmentName(it->first);
		log << TestLog::Section(attPointName, "Attachment point " + attPointName);
		logAttachment(*it->second, log);
		log << TestLog::EndSection;
	}
	log << TestLog::EndSection; // Attachments

	log << TestLog::EndSection; // Framebuffer
}

FboBuilder::FboBuilder (GLuint fbo, GLenum target, const glw::Functions& gl)
	: m_error	(GL_NO_ERROR)
	, m_target	(target)
	, m_gl		(gl)
{
	m_gl.bindFramebuffer(m_target, fbo);
}

FboBuilder::~FboBuilder (void)
{
	for (TextureMap::const_iterator it = textures.begin(); it != textures.end(); it++)
	{
		glDelete(*it->second, it->first, m_gl);
	}
	for (RboMap::const_iterator it = rbos.begin(); it != rbos.end(); it++)
	{
		glDelete(*it->second, it->first, m_gl);
	}
	m_gl.bindFramebuffer(m_target, 0);
	for (Configs::const_iterator it = m_configs.begin(); it != m_configs.end(); it++)
	{
		delete *it;
	}
}

void FboBuilder::checkError (void)
{
	const GLenum error = m_gl.getError();
	if (error != GL_NO_ERROR && m_error == GL_NO_ERROR)
		m_error = error;
}

void FboBuilder::glAttach (GLenum attPoint, const Attachment* att)
{
	if (att == NULL)
		m_gl.framebufferRenderbuffer(m_target, attPoint, GL_RENDERBUFFER, 0);
	else
		attachAttachment(*att, attPoint, m_gl);
	checkError();
	attach(attPoint, att);
}

GLuint FboBuilder::glCreateTexture (const Texture& texCfg)
{
	const GLuint texName = glCreate(texCfg, m_gl);
	checkError();
	setTexture(texName, texCfg);
	return texName;
}

GLuint FboBuilder::glCreateRbo (const Renderbuffer& rbCfg)
{
	const GLuint rbName = glCreate(rbCfg, m_gl);
	checkError();
	setRbo(rbName, rbCfg);
	return rbName;
}

TransferFormat transferImageFormat (const ImageFormat& imgFormat)
{
	if (imgFormat.unsizedType == GL_NONE)
		return getTransferFormat(mapGLInternalFormat(imgFormat.format));
	else
		return TransferFormat(imgFormat.format, imgFormat.unsizedType);
}

} // FboUtil
} // gls
} // deqp
