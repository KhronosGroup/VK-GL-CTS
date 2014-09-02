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
 * \brief FBO test utilities.
 *//*--------------------------------------------------------------------*/

#include "es31fFboTestUtil.hpp"
#include "sglrContextUtil.hpp"
#include "sglrGLContext.hpp"
#include "sglrReferenceContext.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <limits>

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace FboTestUtil
{

using std::string;
using std::vector;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;

static rr::GenericVecType mapDataTypeToGenericVecType(glu::DataType type)
{
	switch (type)
	{
		case glu::TYPE_FLOAT_VEC4:	return rr::GENERICVECTYPE_FLOAT;
		case glu::TYPE_INT_VEC4:	return rr::GENERICVECTYPE_INT32;
		case glu::TYPE_UINT_VEC4:	return rr::GENERICVECTYPE_UINT32;
		default:
			DE_ASSERT(DE_FALSE);
			return rr::GENERICVECTYPE_LAST;
	}
}

template <typename T>
static tcu::Vector<T, 4> castVectorSaturate (const tcu::Vec4& in)
{
	return tcu::Vector<T, 4>((in.x() + 0.5f >= std::numeric_limits<T>::max()) ? (std::numeric_limits<T>::max()) : ((in.x() - 0.5f <= std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) : (T(in.x()))),
	                         (in.y() + 0.5f >= std::numeric_limits<T>::max()) ? (std::numeric_limits<T>::max()) : ((in.y() - 0.5f <= std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) : (T(in.y()))),
							 (in.z() + 0.5f >= std::numeric_limits<T>::max()) ? (std::numeric_limits<T>::max()) : ((in.z() - 0.5f <= std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) : (T(in.z()))),
							 (in.w() + 0.5f >= std::numeric_limits<T>::max()) ? (std::numeric_limits<T>::max()) : ((in.w() - 0.5f <= std::numeric_limits<T>::min()) ? (std::numeric_limits<T>::min()) : (T(in.w()))));
}

TextureCubeArrayShader::TextureCubeArrayShader (glu::DataType samplerType, glu::DataType outputType)
	: sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
							<< sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
							<< sglr::pdec::VertexAttribute("a_coord", rr::GENERICVECTYPE_FLOAT)
							<< sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
							<< sglr::pdec::FragmentOutput(mapDataTypeToGenericVecType(outputType))
							<< sglr::pdec::Uniform("u_coordMat", glu::TYPE_FLOAT_MAT3)
							<< sglr::pdec::Uniform("u_sampler0", samplerType)
							<< sglr::pdec::Uniform("u_scale", glu::TYPE_FLOAT_VEC4)
							<< sglr::pdec::Uniform("u_bias", glu::TYPE_FLOAT_VEC4)
							<< sglr::pdec::VertexSource(
									"#version 310 es\n"
									"#extension GL_EXT_texture_cube_map_array : require\n"
									"in highp vec4 a_position;\n"
									"in mediump vec2 a_coord;\n"
									"uniform mat3 u_coordMat;\n"
									"out highp vec3 v_coord;\n"
									"void main (void)\n"
									"{\n"
									"	gl_Position = a_position;\n"
									"	v_coord = u_coordMat * vec3(a_coord, 1.0);\n"
									"}\n")
							<< sglr::pdec::FragmentSource(
									string("") +
									"#version 310 es\n"
									"#extension GL_EXT_texture_cube_map_array : require\n"
									"uniform highp " + glu::getDataTypeName(samplerType) + " u_sampler0;\n"
									"uniform highp vec4 u_scale;\n"
									"uniform highp vec4 u_bias;\n"
									"uniform highp int u_layer;\n"
									"in highp vec3 v_coord;\n"
									"layout(location = 0) out highp " + glu::getDataTypeName(outputType) + " o_color;\n"
									"void main (void)\n"
									"{\n"
									"	o_color = " + glu::getDataTypeName(outputType) + "(vec4(texture(u_sampler0, vec4(v_coord, u_layer))) * u_scale + u_bias);\n"
									"}\n"))
	, m_texScale	(1.0f)
	, m_texBias		(0.0f)
	, m_layer		(0)
	, m_outputType	(outputType)
{
}

void TextureCubeArrayShader::setLayer (int layer)
{
	m_layer = layer;
}

void TextureCubeArrayShader::setFace (tcu::CubeFace face)
{
	static const float s_cubeTransforms[][3*3] =
	{
		// Face -X: (x, y, 1) -> (-1, -(2*y-1), +(2*x-1))
		{  0.0f,  0.0f, -1.0f,
		   0.0f, -2.0f,  1.0f,
		   2.0f,  0.0f, -1.0f },
		// Face +X: (x, y, 1) -> (+1, -(2*y-1), -(2*x-1))
		{  0.0f,  0.0f,  1.0f,
		   0.0f, -2.0f,  1.0f,
		  -2.0f,  0.0f,  1.0f },
		// Face -Y: (x, y, 1) -> (+(2*x-1), -1, -(2*y-1))
		{  2.0f,  0.0f, -1.0f,
		   0.0f,  0.0f, -1.0f,
		   0.0f, -2.0f,  1.0f },
		// Face +Y: (x, y, 1) -> (+(2*x-1), +1, +(2*y-1))
		{  2.0f,  0.0f, -1.0f,
		   0.0f,  0.0f,  1.0f,
		   0.0f,  2.0f, -1.0f },
		// Face -Z: (x, y, 1) -> (-(2*x-1), -(2*y-1), -1)
		{ -2.0f,  0.0f,  1.0f,
		   0.0f, -2.0f,  1.0f,
		   0.0f,  0.0f, -1.0f },
		// Face +Z: (x, y, 1) -> (+(2*x-1), -(2*y-1), +1)
		{  2.0f,  0.0f, -1.0f,
		   0.0f, -2.0f,  1.0f,
		   0.0f,  0.0f,  1.0f }
	};
	DE_ASSERT(de::inBounds<int>(face, 0, tcu::CUBEFACE_LAST));
	m_coordMat = tcu::Mat3(s_cubeTransforms[face]);
}

void TextureCubeArrayShader::setTexScaleBias (const Vec4& scale, const Vec4& bias)
{
	m_texScale	= scale;
	m_texBias	= bias;
}

void TextureCubeArrayShader::setUniforms (sglr::Context& gl, deUint32 program) const
{
	gl.useProgram(program);

	gl.uniform1i(gl.getUniformLocation(program, "u_sampler0"), 0);
	gl.uniformMatrix3fv(gl.getUniformLocation(program, "u_coordMat"), 1, GL_FALSE, m_coordMat.getColumnMajorData().getPtr());
	gl.uniform1i(gl.getUniformLocation(program, "u_layer"), m_layer);
	gl.uniform4fv(gl.getUniformLocation(program, "u_scale"), 1, m_texScale.getPtr());
	gl.uniform4fv(gl.getUniformLocation(program, "u_bias"), 1, m_texBias.getPtr());
}

void TextureCubeArrayShader::shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
{
	tcu::Mat3 texCoordMat = tcu::Mat3(m_uniforms[0].value.m3);

	for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
	{
		rr::VertexPacket&	packet	= *packets[packetNdx];
		const tcu::Vec2		a_coord = rr::readVertexAttribFloat(inputs[1], packet.instanceNdx, packet.vertexNdx).xy();
		const tcu::Vec3		v_coord = texCoordMat * tcu::Vec3(a_coord.x(), a_coord.y(), 1.0f);

		packet.position = rr::readVertexAttribFloat(inputs[0], packet.instanceNdx, packet.vertexNdx);
		packet.outputs[0] = tcu::Vec4(v_coord.x(), v_coord.y(), v_coord.z(), 0.0f);
	}
}

void TextureCubeArrayShader::shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
{
	const tcu::Vec4 texScale (m_uniforms[2].value.f4);
	const tcu::Vec4 texBias	 (m_uniforms[3].value.f4);

	tcu::Vec4 texCoords[4];
	tcu::Vec4 colors[4];

	for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
	{
		const sglr::rc::TextureCubeArray* tex = m_uniforms[1].sampler.texCubeArray;

		for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
		{
			const tcu::Vec4	coord = rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx);
			texCoords[fragNdx] = tcu::Vec4(coord.x(), coord.y(), coord.z(), (float)m_layer);
		}

		tex->sample4(colors, texCoords);

		for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
		{
			const tcu::Vec4		color	= colors[fragNdx] * texScale + texBias;
			const tcu::IVec4	icolor	= castVectorSaturate<deInt32>(color);
			const tcu::UVec4	uicolor	= castVectorSaturate<deUint32>(color);

			if (m_outputType == glu::TYPE_FLOAT_VEC4)		rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			else if (m_outputType == glu::TYPE_INT_VEC4)	rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, icolor);
			else if (m_outputType == glu::TYPE_UINT_VEC4)	rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, uicolor);
			else
				DE_ASSERT(DE_FALSE);
		}
	}
}

} // FboTestUtil
} // Functional
} // gles31
} // deqp
