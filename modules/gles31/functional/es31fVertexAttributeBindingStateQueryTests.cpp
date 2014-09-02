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
 * \brief Vertex attribute binding state query tests.
 *//*--------------------------------------------------------------------*/

#include "es31fVertexAttributeBindingStateQueryTests.hpp"
#include "tcuTestLog.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"
#include "gluObjectWrapper.hpp"
#include "gluStrUtil.hpp"
#include "glsStateQueryUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deRandom.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

class AttributeBindingCase : public TestCase
{
public:
					AttributeBindingCase	(Context& context, const char* name, const char* desc);
	IterateResult	iterate					(void);
};

AttributeBindingCase::AttributeBindingCase (Context& context, const char* name, const char* desc)
	: TestCase(context, name, desc)
{
}

AttributeBindingCase::IterateResult AttributeBindingCase::iterate (void)
{
	glu::CallLogWrapper gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao			(m_context.getRenderContext());
	glw::GLenum			error		= 0;
	glw::GLint			maxAttrs	= -1;
	bool				allOk		= true;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttrs);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int attr = 0; attr < de::max(16, maxAttrs); ++attr)
		{
			glw::GLint bindingState = -1;

			gl.glGetVertexAttribiv(attr, GL_VERTEX_ATTRIB_BINDING, &bindingState);
			error = gl.glGetError();

			if (error != GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
				allOk = false;
			}
			else if (bindingState != attr)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << attr << ", got " << bindingState << tcu::TestLog::EndMessage;
				allOk = false;
			}
		}
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());
		glw::GLint					bindingState	= -1;

		// set to value A in vao1
		gl.glVertexAttribBinding(1, 4);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glVertexAttribBinding(1, 7);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		gl.glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_BINDING, &bindingState);
		error = gl.glGetError();

		if (error != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
			allOk = false;
		}
		else if (bindingState != 4)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected 4, got " << bindingState << tcu::TestLog::EndMessage;
			allOk = false;
		}
	}

	// random values
	{
		const tcu::ScopedLogSection	section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0xabc);
		const int					numRandomTests	= 10;

		for (int randomTestNdx = 0; randomTestNdx < numRandomTests; ++randomTestNdx)
		{
			// switch random va to random binding
			const int	va				= rnd.getInt(0, de::max(16, maxAttrs)-1);
			const int	binding			= rnd.getInt(0, 16);
			glw::GLint	bindingState	= -1;

			gl.glVertexAttribBinding(va, binding);
			gl.glGetVertexAttribiv(va, GL_VERTEX_ATTRIB_BINDING, &bindingState);
			error = gl.glGetError();

			if (error != GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
				allOk = false;
			}
			else if (bindingState != binding)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << binding << ", got " << bindingState << tcu::TestLog::EndMessage;
				allOk = false;
			}
		}
	}

	if (allOk)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	return STOP;
}

class AttributeRelativeOffsetCase : public TestCase
{
public:
					AttributeRelativeOffsetCase	(Context& context, const char* name, const char* desc);
	IterateResult	iterate						(void);
};

AttributeRelativeOffsetCase::AttributeRelativeOffsetCase (Context& context, const char* name, const char* desc)
	: TestCase(context, name, desc)
{
}

AttributeRelativeOffsetCase::IterateResult AttributeRelativeOffsetCase::iterate (void)
{
	glu::CallLogWrapper gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao			(m_context.getRenderContext());
	glw::GLenum			error		= 0;
	glw::GLint			maxAttrs	= -1;
	bool				allOk		= true;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttrs);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int attr = 0; attr < de::max(16, maxAttrs); ++attr)
		{
			glw::GLint relOffsetState = -1;

			gl.glGetVertexAttribiv(attr, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &relOffsetState);
			error = gl.glGetError();

			if (error != GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
				allOk = false;
			}
			else if (relOffsetState != 0)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected 0, got " << relOffsetState << tcu::TestLog::EndMessage;
				allOk = false;
			}
		}
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());
		glw::GLint					relOffsetState	= -1;

		// set to value A in vao1
		gl.glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 9);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 21);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		gl.glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &relOffsetState);
		error = gl.glGetError();

		if (error != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
			allOk = false;
		}
		else if (relOffsetState != 9)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected 9, got " << relOffsetState << tcu::TestLog::EndMessage;
			allOk = false;
		}
	}

	// random values
	{
		const tcu::ScopedLogSection	section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0xabc);
		const int					numRandomTests	= 10;

		for (int randomTestNdx = 0; randomTestNdx < numRandomTests; ++randomTestNdx)
		{
			const int	va				= rnd.getInt(0, de::max(16, maxAttrs)-1);
			const int	offset			= rnd.getInt(0, 2047);
			glw::GLint	relOffsetState	= -1;

			gl.glVertexAttribFormat(va, 4, GL_FLOAT, GL_FALSE, offset);
			gl.glGetVertexAttribiv(va, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &relOffsetState);
			error = gl.glGetError();

			if (error != GL_NO_ERROR)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got error " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
				allOk = false;
			}
			else if (relOffsetState != offset)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << offset << ", got " << relOffsetState << tcu::TestLog::EndMessage;
				allOk = false;
			}
		}
	}

	if (allOk)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
	return STOP;
}

class IndexedCase : public TestCase
{
public:
	enum VerifierType
	{
		VERIFIER_INT,
		VERIFIER_INT64,

		VERIFIER_LAST
	};

						IndexedCase			(Context& context, const char* name, const char* desc, VerifierType verifier);

	IterateResult		iterate				(void);
	void				verifyValue			(glu::CallLogWrapper& gl, glw::GLenum name, int index, int expected);

	virtual void		test				(void) = 0;
private:
	const VerifierType	m_verifier;
};

IndexedCase::IndexedCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
	DE_ASSERT(verifier < VERIFIER_LAST);
}

IndexedCase::IterateResult IndexedCase::iterate (void)
{
	// default value
	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

	test();

	return STOP;
}

void IndexedCase::verifyValue (glu::CallLogWrapper& gl, glw::GLenum name, int index, int expected)
{
	if (m_verifier == VERIFIER_INT)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint>	value;
		glw::GLenum													error = 0;

		gl.glGetIntegeri_v(name, index, &value);
		error = gl.glGetError();

		if (error != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got unexpected error: " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
			if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected error");
		}
		else if (!value.verifyValidity(m_testCtx))
		{
			// verifyValidity sets error
		}
		else
		{
			if (value != expected)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << expected << ", got " << value << tcu::TestLog::EndMessage;
				if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected value");
			}
		}
	}
	else if (m_verifier == VERIFIER_INT64)
	{
		gls::StateQueryUtil::StateQueryMemoryWriteGuard<glw::GLint64>	value;
		glw::GLenum														error = 0;

		gl.glGetInteger64i_v(name, index, &value);
		error = gl.glGetError();

		if (error != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Got unexpected error: " << glu::getErrorStr(error) << tcu::TestLog::EndMessage;
			if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected error");
		}
		else if (!value.verifyValidity(m_testCtx))
		{
			// verifyValidity sets error
		}
		else
		{
			if (value != expected)
			{
				m_testCtx.getLog() << tcu::TestLog::Message << "// ERROR: Expected " << expected << ", got " << value << tcu::TestLog::EndMessage;
				if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected value");
			}
		}
	}
	else
		DE_ASSERT(false);
}

class VertexBindingDivisorCase : public IndexedCase
{
public:
			VertexBindingDivisorCase	(Context& context, const char* name, const char* desc, VerifierType verifier);
	void	test						(void);
};

VertexBindingDivisorCase::VertexBindingDivisorCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: IndexedCase(context, name, desc, verifier)
{
}

void VertexBindingDivisorCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glw::GLint			reportedMaxBindings	= -1;
	glw::GLint			maxBindings;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &reportedMaxBindings);
	maxBindings = de::max(16, reportedMaxBindings);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int binding = 0; binding < maxBindings; ++binding)
			verifyValue(gl, GL_VERTEX_BINDING_DIVISOR, binding, 0);
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());

		// set to value A in vao1
		gl.glVertexBindingDivisor(1, 4);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glVertexBindingDivisor(1, 9);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		verifyValue(gl, GL_VERTEX_BINDING_DIVISOR, 1, 4);
	}

	// random values
	{
		const tcu::ScopedLogSection	section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0xabc);
		const int					numRandomTests	= 10;

		for (int randomTestNdx = 0; randomTestNdx < numRandomTests; ++randomTestNdx)
		{
			const int	binding			= rnd.getInt(0, maxBindings-1);
			const int	divisor			= rnd.getInt(0, 2047);

			gl.glVertexBindingDivisor(binding, divisor);
			verifyValue(gl, GL_VERTEX_BINDING_DIVISOR, binding, divisor);
		}
	}
}

class VertexBindingOffsetCase : public IndexedCase
{
public:
			VertexBindingOffsetCase		(Context& context, const char* name, const char* desc, VerifierType verifier);
	void	test						(void);
};

VertexBindingOffsetCase::VertexBindingOffsetCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: IndexedCase(context, name, desc, verifier)
{
}

void VertexBindingOffsetCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());
	glw::GLint			reportedMaxBindings	= -1;
	glw::GLint			maxBindings;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &reportedMaxBindings);
	maxBindings = de::max(16, reportedMaxBindings);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int binding = 0; binding < maxBindings; ++binding)
			verifyValue(gl, GL_VERTEX_BINDING_OFFSET, binding, 0);
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());

		// set to value A in vao1
		gl.glBindVertexBuffer(1, *buffer, 4, 32);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glBindVertexBuffer(1, *buffer, 13, 32);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		verifyValue(gl, GL_VERTEX_BINDING_OFFSET, 1, 4);
	}

	// random values
	{
		const tcu::ScopedLogSection	section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0xabc);
		const int					numRandomTests	= 10;

		for (int randomTestNdx = 0; randomTestNdx < numRandomTests; ++randomTestNdx)
		{
			const int	binding			= rnd.getInt(0, maxBindings-1);
			const int	offset			= rnd.getInt(0, 4000);

			gl.glBindVertexBuffer(binding, *buffer, offset, 32);
			verifyValue(gl, GL_VERTEX_BINDING_OFFSET, binding, offset);
		}
	}
}

class VertexBindingStrideCase : public IndexedCase
{
public:
			VertexBindingStrideCase		(Context& context, const char* name, const char* desc, VerifierType verifier);
	void	test						(void);
};

VertexBindingStrideCase::VertexBindingStrideCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: IndexedCase(context, name, desc, verifier)
{
}

void VertexBindingStrideCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());
	glw::GLint			reportedMaxBindings	= -1;
	glw::GLint			maxBindings;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &reportedMaxBindings);
	maxBindings = de::max(16, reportedMaxBindings);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int binding = 0; binding < maxBindings; ++binding)
			verifyValue(gl, GL_VERTEX_BINDING_STRIDE, binding, 16);
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());

		// set to value A in vao1
		gl.glBindVertexBuffer(1, *buffer, 0, 32);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glBindVertexBuffer(1, *buffer, 0, 64);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		verifyValue(gl, GL_VERTEX_BINDING_STRIDE, 1, 32);
	}

	// random values
	{
		const tcu::ScopedLogSection	section			(m_testCtx.getLog(), "random", "Random values");
		de::Random					rnd				(0xabc);
		const int					numRandomTests	= 10;

		for (int randomTestNdx = 0; randomTestNdx < numRandomTests; ++randomTestNdx)
		{
			const int	binding			= rnd.getInt(0, maxBindings-1);
			const int	stride			= rnd.getInt(0, 2048);

			gl.glBindVertexBuffer(binding, *buffer, 0, stride);
			verifyValue(gl, GL_VERTEX_BINDING_STRIDE, binding, stride);
		}
	}
}

class VertexBindingBufferCase : public IndexedCase
{
public:
			VertexBindingBufferCase		(Context& context, const char* name, const char* desc, VerifierType verifier);
	void	test						(void);
};

VertexBindingBufferCase::VertexBindingBufferCase (Context& context, const char* name, const char* desc, VerifierType verifier)
	: IndexedCase(context, name, desc, verifier)
{
}

void VertexBindingBufferCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());
	glw::GLint			reportedMaxBindings	= -1;
	glw::GLint			maxBindings;

	gl.enableLogging(true);

	gl.glBindVertexArray(*vao);
	gl.glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &reportedMaxBindings);
	maxBindings = de::max(16, reportedMaxBindings);

	// initial
	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "initial", "Initial values");

		for (int binding = 0; binding < maxBindings; ++binding)
			verifyValue(gl, GL_VERTEX_BINDING_BUFFER, binding, 0);
	}

	// is part of vao
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "vao", "VAO state");
		glu::VertexArray			otherVao		(m_context.getRenderContext());
		glu::Buffer					otherBuffer		(m_context.getRenderContext());

		// set to value A in vao1
		gl.glBindVertexBuffer(1, *buffer, 0, 32);

		// set to value B in vao2
		gl.glBindVertexArray(*otherVao);
		gl.glBindVertexBuffer(1, *otherBuffer, 0, 32);

		// check value is still ok in original vao
		gl.glBindVertexArray(*vao);
		verifyValue(gl, GL_VERTEX_BINDING_BUFFER, 1, *buffer);
	}

	// Is detached in delete from active vao and not from deactive
	{
		const tcu::ScopedLogSection section			(m_testCtx.getLog(), "autoUnbind", "Unbind on delete");
		glu::VertexArray			otherVao		(m_context.getRenderContext());
		glw::GLuint					otherBuffer		= -1;

		gl.glGenBuffers(1, &otherBuffer);

		// set in vao1 and vao2
		gl.glBindVertexBuffer(1, otherBuffer, 0, 32);
		gl.glBindVertexArray(*otherVao);
		gl.glBindVertexBuffer(1, otherBuffer, 0, 32);

		// delete buffer. This unbinds it from active (vao2) but not from unactive
		gl.glDeleteBuffers(1, &otherBuffer);
		verifyValue(gl, GL_VERTEX_BINDING_BUFFER, 1, 0);
		gl.glBindVertexArray(*vao);
		verifyValue(gl, GL_VERTEX_BINDING_BUFFER, 1, otherBuffer);
	}
}

class MixedVertexBindingDivisorCase : public IndexedCase
{
public:
			MixedVertexBindingDivisorCase	(Context& context, const char* name, const char* desc);
	void	test							(void);
};

MixedVertexBindingDivisorCase::MixedVertexBindingDivisorCase (Context& context, const char* name, const char* desc)
	: IndexedCase(context, name, desc, VERIFIER_INT)
{
}

void MixedVertexBindingDivisorCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());

	gl.enableLogging(true);

	gl.glVertexAttribDivisor(1, 4);
	verifyValue(gl, GL_VERTEX_BINDING_DIVISOR, 1, 4);
}

class MixedVertexBindingOffsetCase : public IndexedCase
{
public:
			MixedVertexBindingOffsetCase	(Context& context, const char* name, const char* desc);
	void	test							(void);
};

MixedVertexBindingOffsetCase::MixedVertexBindingOffsetCase (Context& context, const char* name, const char* desc)
	: IndexedCase(context, name, desc, VERIFIER_INT)
{
}

void MixedVertexBindingOffsetCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());

	gl.enableLogging(true);

	gl.glBindBuffer(GL_ARRAY_BUFFER, *buffer);
	gl.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (const deUint8*)DE_NULL + 12);

	verifyValue(gl, GL_VERTEX_BINDING_OFFSET, 1, 12);
}

class MixedVertexBindingStrideCase : public IndexedCase
{
public:
			MixedVertexBindingStrideCase	(Context& context, const char* name, const char* desc);
	void	test							(void);
};

MixedVertexBindingStrideCase::MixedVertexBindingStrideCase (Context& context, const char* name, const char* desc)
	: IndexedCase(context, name, desc, VERIFIER_INT)
{
}

void MixedVertexBindingStrideCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());

	gl.enableLogging(true);

	gl.glBindBuffer(GL_ARRAY_BUFFER, *buffer);
	gl.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12, 0);
	verifyValue(gl, GL_VERTEX_BINDING_STRIDE, 1, 12);

	// test effectiveStride
	gl.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
	verifyValue(gl, GL_VERTEX_BINDING_STRIDE, 1, 16);
}

class MixedVertexBindingBufferCase : public IndexedCase
{
public:
			MixedVertexBindingBufferCase	(Context& context, const char* name, const char* desc);
	void	test							(void);
};

MixedVertexBindingBufferCase::MixedVertexBindingBufferCase (Context& context, const char* name, const char* desc)
	: IndexedCase(context, name, desc, VERIFIER_INT)
{
}

void MixedVertexBindingBufferCase::test (void)
{
	glu::CallLogWrapper gl					(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	glu::VertexArray	vao					(m_context.getRenderContext());
	glu::Buffer			buffer				(m_context.getRenderContext());

	gl.enableLogging(true);

	gl.glBindBuffer(GL_ARRAY_BUFFER, *buffer);
	gl.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
	verifyValue(gl, GL_VERTEX_BINDING_BUFFER, 1, *buffer);
}

} // anonymous

VertexAttributeBindingStateQueryTests::VertexAttributeBindingStateQueryTests (Context& context)
	: TestCaseGroup(context, "vertex_attribute_binding", "Query vertex attribute binding state.")
{
}

VertexAttributeBindingStateQueryTests::~VertexAttributeBindingStateQueryTests (void)
{
}

void VertexAttributeBindingStateQueryTests::init (void)
{
	tcu::TestCaseGroup* const attributeGroup	= new TestCaseGroup(m_context, "vertex_attrib", "Vertex attribute state");
	tcu::TestCaseGroup* const indexedGroup		= new TestCaseGroup(m_context, "indexed", "Indexed state");

	addChild(attributeGroup);
	addChild(indexedGroup);

	// .vertex_attrib
	{
		attributeGroup->addChild(new AttributeBindingCase		(m_context,	"vertex_attrib_binding",			"Test VERTEX_ATTRIB_BINDING"));
		attributeGroup->addChild(new AttributeRelativeOffsetCase(m_context,	"vertex_attrib_relative_offset",	"Test VERTEX_ATTRIB_RELATIVE_OFFSET"));
	}

	// .indexed (and 64)
	{
		static const struct Verifier
		{
			const char*					name;
			IndexedCase::VerifierType	type;
		} verifiers[] =
		{
			{ "getintegeri",	IndexedCase::VERIFIER_INT	},
			{ "getintegeri64",	IndexedCase::VERIFIER_INT64	},
		};

		// states

		for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(verifiers); ++verifierNdx)
		{
			indexedGroup->addChild(new VertexBindingDivisorCase	(m_context, (std::string("vertex_binding_divisor_") + verifiers[verifierNdx].name).c_str(),	"Test VERTEX_BINDING_DIVISOR",	verifiers[verifierNdx].type));
			indexedGroup->addChild(new VertexBindingOffsetCase	(m_context, (std::string("vertex_binding_offset_") + verifiers[verifierNdx].name).c_str(),	"Test VERTEX_BINDING_OFFSET",	verifiers[verifierNdx].type));
			indexedGroup->addChild(new VertexBindingStrideCase	(m_context, (std::string("vertex_binding_stride_") + verifiers[verifierNdx].name).c_str(),	"Test VERTEX_BINDING_STRIDE",	verifiers[verifierNdx].type));
			indexedGroup->addChild(new VertexBindingBufferCase	(m_context, (std::string("vertex_binding_buffer_") + verifiers[verifierNdx].name).c_str(),	"Test VERTEX_BINDING_BUFFER",	verifiers[verifierNdx].type));
		}

		// mixed apis

		indexedGroup->addChild(new MixedVertexBindingDivisorCase(m_context, "vertex_binding_divisor_mixed",	"Test VERTEX_BINDING_DIVISOR"));
		indexedGroup->addChild(new MixedVertexBindingOffsetCase	(m_context, "vertex_binding_offset_mixed",	"Test VERTEX_BINDING_OFFSET"));
		indexedGroup->addChild(new MixedVertexBindingStrideCase	(m_context, "vertex_binding_stride_mixed",	"Test VERTEX_BINDING_STRIDE"));
		indexedGroup->addChild(new MixedVertexBindingBufferCase	(m_context, "vertex_binding_buffer_mixed",	"Test VERTEX_BINDING_BUFFER"));
	}
}

} // Functional
} // gles31
} // deqp
