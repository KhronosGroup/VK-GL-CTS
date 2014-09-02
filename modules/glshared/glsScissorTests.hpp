#ifndef _GLSSCISSORTESTS_HPP
#define _GLSSCISSORTESTS_HPP
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
 * \brief Common parts for ES2/3 scissor tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuVectorType.hpp"
#include "tcuVector.hpp"
#include "sglrGLContext.hpp"

namespace glu
{
class RenderContext;
} // glu

namespace sglr
{
class Context;
} // sglr

namespace deqp
{
namespace gls
{
namespace Functional
{

// Wrapper class, provides iterator & reporting logic
class ScissorCase : public tcu::TestCase
{
public:
	enum PrimitiveType
	{
		POINT = 0,
		LINE,
		TRIANGLE,
		QUAD,

		PRIMITIVETYPE_LAST
	};

								ScissorCase				(glu::RenderContext& context, tcu::TestContext& testContext, const tcu::Vec4& scissorArea, const char* name, const char* description);
	virtual						~ScissorCase			(void) {}

	virtual IterateResult		iterate					(void);

	// Areas are of the form (x,y,widht,height) in the range [0,1]. Vertex counts 1-3 result in single point/line/tri, higher ones result in the indicated number of quads in pseudorandom locations.
	static tcu::TestNode*		createPrimitiveTest		(glu::RenderContext&	context,
														 tcu::TestContext&		testContext,
														 const tcu::Vec4&		scissorArea,
														 const tcu::Vec4&		renderArea,
														 PrimitiveType			type,
														 int					primitiveCount,
														 const char*			name,
														 const char*			description);
	static tcu::TestNode*		createClearTest			(glu::RenderContext&	context,
														 tcu::TestContext&		testContext,
														 const tcu::Vec4&		scissorArea,
														 deUint32				clearMode,
														 const char*			name,
														 const char*			description);

protected:
	virtual void				render					(sglr::Context& context, const tcu::IVec4& viewport) = 0;

	glu::RenderContext&			m_renderContext;
	const tcu::Vec4				m_scissorArea;
};

class ScissorTestShader : public sglr::ShaderProgram
{
public:
								ScissorTestShader	(void);

	void						setColor			(sglr::Context& ctx, deUint32 programID, const tcu::Vec4& color);

private:
	void						shadeVertices		(const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const;
	void						shadeFragments		(rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const;

	const sglr::UniformSlot&	u_color;
};

} // Functional
} // gls
} // deqp

#endif // _GLSSCISSORTESTS_HPP
