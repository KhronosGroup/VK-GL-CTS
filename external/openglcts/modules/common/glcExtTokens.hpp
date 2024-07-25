#ifndef _GLCEXTTOKENS_HPP
#define _GLCEXTTOKENS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2015-2016 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "gluRenderContext.hpp"
#include "glwDefs.hpp"

namespace deqp
{

struct GLExtTokens
{
    void init(const glu::ContextType &contextType);

    glw::GLenum GEOMETRY_SHADER;
    glw::GLenum GEOMETRY_SHADER_BIT;
    glw::GLenum GEOMETRY_LINKED_VERTICES_OUT;
    glw::GLenum GEOMETRY_LINKED_INPUT_TYPE;
    glw::GLenum GEOMETRY_LINKED_OUTPUT_TYPE;
    glw::GLenum GEOMETRY_SHADER_INVOCATIONS;
    glw::GLenum MAX_GEOMETRY_TEXTURE_IMAGE_UNITS;
    glw::GLenum MAX_GEOMETRY_IMAGE_UNIFORMS;
    glw::GLenum MAX_GEOMETRY_SHADER_STORAGE_BLOCKS;
    glw::GLenum MAX_GEOMETRY_ATOMIC_COUNTERS;
    glw::GLenum LINE_STRIP_ADJACENCY;
    glw::GLenum LINES_ADJACENCY;
    glw::GLenum TRIANGLES_ADJACENCY;
    glw::GLenum TRIANGLE_STRIP_ADJACENCY;
    glw::GLenum FRAMEBUFFER_ATTACHMENT_LAYERED;
    glw::GLenum FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS;
    glw::GLenum LAYER_PROVOKING_VERTEX;
    glw::GLenum FIRST_VERTEX_CONVENTION;
    glw::GLenum LAST_VERTEX_CONVENTION;
    glw::GLenum UNDEFINED_VERTEX;
    glw::GLenum FRAMEBUFFER_DEFAULT_LAYERS;
    glw::GLenum MAX_FRAMEBUFFER_LAYERS;
    glw::GLenum MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS;
    glw::GLenum MAX_GEOMETRY_UNIFORM_COMPONENTS;
    glw::GLenum MAX_GEOMETRY_UNIFORM_BLOCKS;
    glw::GLenum MAX_GEOMETRY_INPUT_COMPONENTS;
    glw::GLenum MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS;
    glw::GLenum MAX_GEOMETRY_OUTPUT_COMPONENTS;
    glw::GLenum MAX_GEOMETRY_OUTPUT_VERTICES;
    glw::GLenum MAX_GEOMETRY_SHADER_INVOCATIONS;
    glw::GLenum MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS;
    glw::GLenum PRIMITIVES_GENERATED;
    glw::GLenum TEXTURE_BORDER_COLOR;
    glw::GLenum CLAMP_TO_BORDER;
    glw::GLenum PATCH_VERTICES;
    glw::GLenum TESS_CONTROL_SHADER;
    glw::GLenum TESS_EVALUATION_SHADER;
    glw::GLenum PATCHES;
    glw::GLenum MAX_PATCH_VERTICES;
    glw::GLenum MAX_TESS_GEN_LEVEL;
    glw::GLenum MAX_TESS_CONTROL_INPUT_COMPONENTS;
    glw::GLenum MAX_TESS_CONTROL_OUTPUT_COMPONENTS;
    glw::GLenum MAX_TESS_PATCH_COMPONENTS;
    glw::GLenum MAX_TESS_EVALUATION_INPUT_COMPONENTS;
    glw::GLenum MAX_TESS_EVALUATION_OUTPUT_COMPONENTS;
    glw::GLenum MAX_TESS_EVALUATION_ATOMIC_COUNTERS;
    glw::GLenum MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS;
    glw::GLenum MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS;
    glw::GLenum MAX_TESS_CONTROL_ATOMIC_COUNTERS;
    glw::GLenum MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS;
    glw::GLenum MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS;
    glw::GLenum MAX_TEXTURE_BUFFER_SIZE;
    glw::GLenum REFERENCED_BY_GEOMETRY_SHADER;
    glw::GLenum REFERENCED_BY_TESS_CONTROL_SHADER;
    glw::GLenum REFERENCED_BY_TESS_EVALUATION_SHADER;
    glw::GLenum TESS_CONTROL_OUTPUT_VERTICES;
    glw::GLenum TESS_GEN_MODE;
    glw::GLenum TESS_GEN_SPACING;
    glw::GLenum TESS_GEN_POINT_MODE;
    glw::GLenum TESS_GEN_VERTEX_ORDER;
    glw::GLenum TESS_CONTROL_SHADER_BIT;
    glw::GLenum TESS_EVALUATION_SHADER_BIT;
    glw::GLenum TEXTURE_BUFFER;
    glw::GLenum TEXTURE_BUFFER_SIZE;
    glw::GLenum TEXTURE_BINDING_BUFFER;
    glw::GLenum TEXTURE_BUFFER_BINDING;
    glw::GLenum TEXTURE_BUFFER_OFFSET;
    glw::GLenum TEXTURE_BUFFER_DATA_STORE_BINDING;
    glw::GLenum SAMPLER_BUFFER;
    glw::GLenum INT_SAMPLER_BUFFER;
    glw::GLenum UNSIGNED_INT_SAMPLER_BUFFER;
    glw::GLenum IMAGE_BUFFER;
    glw::GLenum INT_IMAGE_BUFFER;
    glw::GLenum UNSIGNED_INT_IMAGE_BUFFER;
    glw::GLenum TEXTURE_BUFFER_OFFSET_ALIGNMENT;
    glw::GLenum QUADS;
    glw::GLenum ISOLINES;
    glw::GLenum FRACTIONAL_EVEN;
    glw::GLenum FRACTIONAL_ODD;
    glw::GLenum COMPRESSED_RGBA_ASTC_4x4;
    glw::GLenum COMPRESSED_RGBA_ASTC_5x4;
    glw::GLenum COMPRESSED_RGBA_ASTC_5x5;
    glw::GLenum COMPRESSED_RGBA_ASTC_6x5;
    glw::GLenum COMPRESSED_RGBA_ASTC_6x6;
    glw::GLenum COMPRESSED_RGBA_ASTC_8x5;
    glw::GLenum COMPRESSED_RGBA_ASTC_8x6;
    glw::GLenum COMPRESSED_RGBA_ASTC_8x8;
    glw::GLenum COMPRESSED_RGBA_ASTC_10x5;
    glw::GLenum COMPRESSED_RGBA_ASTC_10x6;
    glw::GLenum COMPRESSED_RGBA_ASTC_10x8;
    glw::GLenum COMPRESSED_RGBA_ASTC_10x10;
    glw::GLenum COMPRESSED_RGBA_ASTC_12x10;
    glw::GLenum COMPRESSED_RGBA_ASTC_12x12;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_4x4;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_5x4;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_5x5;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_6x5;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_6x6;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x5;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x6;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x8;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x5;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x6;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x8;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x10;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_12x10;
    glw::GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_12x12;
    glw::GLenum MULTIPLY;
    glw::GLenum SCREEN;
    glw::GLenum OVERLAY;
    glw::GLenum DARKEN;
    glw::GLenum LIGHTEN;
    glw::GLenum COLORDODGE;
    glw::GLenum COLORBURN;
    glw::GLenum HARDLIGHT;
    glw::GLenum SOFTLIGHT;
    glw::GLenum DIFFERENCE;
    glw::GLenum EXCLUSION;
    glw::GLenum HSL_HUE;
    glw::GLenum HSL_SATURATION;
    glw::GLenum HSL_COLOR;
    glw::GLenum HSL_LUMINOSITY;
    glw::GLenum PRIMITIVE_BOUNDING_BOX;
};

} // namespace deqp

#endif // _GLCEXTTOKENS_HPP
