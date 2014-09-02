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
 * \brief Texture buffer tests
 *//*--------------------------------------------------------------------*/

#include "es31fTextureBufferTests.hpp"

#include "glsTextureBufferCase.hpp"

#include "glwEnums.hpp"

#include "deStringUtil.hpp"

#include <string>

using std::string;
using namespace deqp::gls::TextureBufferCaseUtil;
using deqp::gls::TextureBufferCase;

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

string toTestName (RenderBits renderBits)
{
	struct
	{
		RenderBits	bit;
		const char*	str;
	} bitInfos[] =
	{
		{ RENDERBITS_AS_VERTEX_ARRAY,		"as_vertex_array"		},
		{ RENDERBITS_AS_INDEX_ARRAY,		"as_index_array"		},
		{ RENDERBITS_AS_VERTEX_TEXTURE,		"as_vertex_texture"		},
		{ RENDERBITS_AS_FRAGMENT_TEXTURE,	"as_fragment_texture"	}
	};

	std::ostringstream	stream;
	bool				first	= true;

	DE_ASSERT(renderBits != 0);

	for (int infoNdx = 0; infoNdx < DE_LENGTH_OF_ARRAY(bitInfos); infoNdx++)
	{
		if (renderBits & bitInfos[infoNdx].bit)
		{
			stream << (first ? "" : "_") << bitInfos[infoNdx].str;
			first = false;
		}
	}

	return stream.str();
}

string toTestName (ModifyBits modifyBits)
{
	struct
	{
		ModifyBits	bit;
		const char*	str;
	} bitInfos[] =
	{
		{ MODIFYBITS_BUFFERDATA,			"bufferdata"			},
		{ MODIFYBITS_BUFFERSUBDATA,			"buffersubdata"			},
		{ MODIFYBITS_MAPBUFFER_WRITE,		"mapbuffer_write"		},
		{ MODIFYBITS_MAPBUFFER_READWRITE,	"mapbuffer_readwrite"	}
	};

	std::ostringstream	stream;
	bool				first	= true;

	DE_ASSERT(modifyBits != 0);

	for (int infoNdx = 0; infoNdx < DE_LENGTH_OF_ARRAY(bitInfos); infoNdx++)
	{
		if (modifyBits & bitInfos[infoNdx].bit)
		{
			stream << (first ? "" : "_") << bitInfos[infoNdx].str;
			first = false;
		}
	}

	return stream.str();
}

RenderBits operator| (RenderBits a, RenderBits b)
{
	return (RenderBits)(deUint32(a) | deUint32(b));
}

} // anonymous

TestCaseGroup* createTextureBufferTests (Context& context)
{
	TestCaseGroup* const root = new TestCaseGroup(context, "texture_buffer", "Texture buffer syncronization tests");

	const size_t bufferSizes[] =
	{
		512,
		513,
		65536,
		65537,
		131071
	};

	const size_t rangeSizes[] =
	{
		512,
		513,
		65537,
		98304,
	};

	const size_t offsets[] =
	{
		1,
		7
	};

	const RenderBits renderTypeCombinations[] =
	{
		RENDERBITS_AS_VERTEX_ARRAY,
									  RENDERBITS_AS_INDEX_ARRAY,
		RENDERBITS_AS_VERTEX_ARRAY	| RENDERBITS_AS_INDEX_ARRAY,

																  RENDERBITS_AS_VERTEX_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	|							  RENDERBITS_AS_VERTEX_TEXTURE,
									  RENDERBITS_AS_INDEX_ARRAY	| RENDERBITS_AS_VERTEX_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	| RENDERBITS_AS_INDEX_ARRAY	| RENDERBITS_AS_VERTEX_TEXTURE,

																								  RENDERBITS_AS_FRAGMENT_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	|															  RENDERBITS_AS_FRAGMENT_TEXTURE,
									  RENDERBITS_AS_INDEX_ARRAY |								  RENDERBITS_AS_FRAGMENT_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	| RENDERBITS_AS_INDEX_ARRAY |								  RENDERBITS_AS_FRAGMENT_TEXTURE,
																  RENDERBITS_AS_VERTEX_TEXTURE	| RENDERBITS_AS_FRAGMENT_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	|							  RENDERBITS_AS_VERTEX_TEXTURE	| RENDERBITS_AS_FRAGMENT_TEXTURE,
									  RENDERBITS_AS_INDEX_ARRAY	| RENDERBITS_AS_VERTEX_TEXTURE	| RENDERBITS_AS_FRAGMENT_TEXTURE,
		RENDERBITS_AS_VERTEX_ARRAY	| RENDERBITS_AS_INDEX_ARRAY	| RENDERBITS_AS_VERTEX_TEXTURE	| RENDERBITS_AS_FRAGMENT_TEXTURE
	};

	const ModifyBits modifyTypes[] =
	{
		MODIFYBITS_BUFFERDATA,
		MODIFYBITS_BUFFERSUBDATA,
		MODIFYBITS_MAPBUFFER_WRITE,
		MODIFYBITS_MAPBUFFER_READWRITE
	};

	// Rendering test
	{
		TestCaseGroup* const renderGroup = new TestCaseGroup(context, "render", "Setup texture buffer with glBufferData and render data in different ways");
		root->addChild(renderGroup);

		for (int renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypeCombinations); renderTypeNdx++)
		{
			const RenderBits		renderType		= renderTypeCombinations[renderTypeNdx];
			TestCaseGroup* const	renderTypeGroup	= new TestCaseGroup(context, toTestName(renderType).c_str(), toTestName(renderType).c_str());

			renderGroup->addChild(renderTypeGroup);

			for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(bufferSizes); sizeNdx++)
			{
				const size_t size	= bufferSizes[sizeNdx];
				const string name	("buffer_size_" + de::toString(size));

				renderTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, size, 0, 0, RENDERBITS_NONE, MODIFYBITS_NONE, renderType, name.c_str(), name.c_str()));
			}

			for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(rangeSizes); sizeNdx++)
			{
				const size_t size		= rangeSizes[sizeNdx];
				const string name		("range_size_" + de::toString(size));
				const size_t bufferSize	= 131072;

				renderTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, bufferSize, 0, size, RENDERBITS_NONE, MODIFYBITS_NONE, renderType, name.c_str(), name.c_str()));
			}

			for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
			{
				const size_t offset		= offsets[offsetNdx];
				const size_t bufferSize	= 131072;
				const size_t size		= 65537;
				const string name		("offset_" + de::toString(offset) + "_alignments");

				renderTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, bufferSize, offset, size, RENDERBITS_NONE, MODIFYBITS_NONE, renderType, name.c_str(), name.c_str()));
			}
		}
	}

	// Modify tests
	{
		TestCaseGroup* const modifyGroup = new TestCaseGroup(context, "modify", "Modify texture buffer content in multiple ways");
		root->addChild(modifyGroup);

		for (int modifyNdx = 0; modifyNdx < DE_LENGTH_OF_ARRAY(modifyTypes); modifyNdx++)
		{
			const ModifyBits		modifyType		= modifyTypes[modifyNdx];
			TestCaseGroup* const	modifyTypeGroup	= new TestCaseGroup(context, toTestName(modifyType).c_str(), toTestName(modifyType).c_str());

			modifyGroup->addChild(modifyTypeGroup);

			for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(bufferSizes); sizeNdx++)
			{
				const size_t	size	= bufferSizes[sizeNdx];
				const string	name	("buffer_size_" + de::toString(size));

				modifyTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, size, 0, 0, RENDERBITS_NONE, modifyType, RENDERBITS_AS_FRAGMENT_TEXTURE, name.c_str(), name.c_str()));
			}

			for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(rangeSizes); sizeNdx++)
			{
				const size_t size		= rangeSizes[sizeNdx];
				const string name		("range_size_" + de::toString(size));
				const size_t bufferSize	= 131072;

				modifyTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, bufferSize, 0, size, RENDERBITS_NONE, modifyType, RENDERBITS_AS_FRAGMENT_TEXTURE, name.c_str(), name.c_str()));
			}

			for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
			{
				const size_t offset		= offsets[offsetNdx];
				const size_t bufferSize	= 131072;
				const size_t size		= 65537;
				const string name		("offset_" + de::toString(offset) + "_alignments");

				modifyTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, bufferSize, offset, size, RENDERBITS_NONE, modifyType, RENDERBITS_AS_FRAGMENT_TEXTURE, name.c_str(), name.c_str()));
			}
		}
	}

	// Modify-Render tests
	{
		TestCaseGroup* const modifyRenderGroup = new TestCaseGroup(context, "modify_render", "Modify texture buffer content in multiple ways and render in different ways");
		root->addChild(modifyRenderGroup);

		for (int modifyNdx = 0; modifyNdx < DE_LENGTH_OF_ARRAY(modifyTypes); modifyNdx++)
		{
			const ModifyBits		modifyType		= modifyTypes[modifyNdx];
			TestCaseGroup* const	modifyTypeGroup	= new TestCaseGroup(context, toTestName(modifyType).c_str(), toTestName(modifyType).c_str());

			modifyRenderGroup->addChild(modifyTypeGroup);

			for (int renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypeCombinations); renderTypeNdx++)
			{
				const RenderBits	renderType	= renderTypeCombinations[renderTypeNdx];
				const size_t		size		= 16*1024;
				const string		name		(toTestName(renderType));

				modifyTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, size, 0, 0, RENDERBITS_NONE, modifyType, renderType, name.c_str(), name.c_str()));
			}
		}
	}

	// Render-Modify tests
	{
		TestCaseGroup* const renderModifyGroup = new TestCaseGroup(context, "render_modify", "Render texture buffer and modify.");
		root->addChild(renderModifyGroup);

		for (int renderTypeNdx = 0; renderTypeNdx < DE_LENGTH_OF_ARRAY(renderTypeCombinations); renderTypeNdx++)
		{
			const RenderBits		renderType		= renderTypeCombinations[renderTypeNdx];
			TestCaseGroup* const	renderTypeGroup	= new TestCaseGroup(context, toTestName(renderType).c_str(), toTestName(renderType).c_str());

			renderModifyGroup->addChild(renderTypeGroup);

			for (int modifyNdx = 0; modifyNdx < DE_LENGTH_OF_ARRAY(modifyTypes); modifyNdx++)
			{
				const ModifyBits	modifyType	= modifyTypes[modifyNdx];
				const size_t		size		= 16*1024;
				const string		name		(toTestName(modifyType));

				renderTypeGroup->addChild(new TextureBufferCase(context.getTestContext(), context.getRenderContext(), GL_RGBA8, size, 0, 0, renderType, modifyType, RENDERBITS_AS_FRAGMENT_TEXTURE, name.c_str(), name.c_str()));
			}
		}
	}

	return root;
}

} // Functional
} // gles31
} // deqp
