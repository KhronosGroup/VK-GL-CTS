#ifndef _ES31FFBOTESTUTIL_HPP
#define _ES31FFBOTESTUTIL_HPP
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

#include "tcuDefs.hpp"
#include "sglrContext.hpp"
#include "gluShaderUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuMatrix.hpp"
#include "tcuRenderTarget.hpp"

#include <vector>

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace FboTestUtil
{

// \todo [2012-04-29 pyry] Clean up and name as SglrUtil

// Helper class for constructing DataType vectors.
struct DataTypes
{
	std::vector<glu::DataType> vec;
	DataTypes& operator<< (glu::DataType type) { vec.push_back(type); return *this; }
};

// Shaders.

class TextureCubeArrayShader : public sglr::ShaderProgram
{
public:
						TextureCubeArrayShader	(glu::DataType samplerType, glu::DataType outputType);
						~TextureCubeArrayShader	(void) {}

	void				setLayer				(int layer);
	void				setFace					(tcu::CubeFace face);
	void				setTexScaleBias			(const tcu::Vec4& scale, const tcu::Vec4& bias);

	void				setUniforms				(sglr::Context& context, deUint32 program) const;

	void				shadeVertices			(const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const;
	void				shadeFragments			(rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const;

private:
	tcu::Vec4			m_texScale;
	tcu::Vec4			m_texBias;
	int					m_layer;
	tcu::Mat3			m_coordMat;

	const glu::DataType	m_outputType;
};

} // FboTestUtil
} // Functional
} // gles31
} // deqp

#endif // _ES31FFBOTESTUTIL_HPP
