/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 * \brief Subgroups Tests Utils
 */ /*--------------------------------------------------------------------*/

#include "glcSubgroupsTestsUtils.hpp"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderUtil.hpp"

using namespace deqp;
using namespace std;
using namespace glc;
using namespace glw;

namespace
{
// debug callback function
// To use:
//	  gl.enable(GL_DEBUG_OUTPUT);
//	  gl.debugMessageCallback(debugCallback, &context);
//
void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
					GLsizei length, const char * message, const void * userParam)
{
	glc::Context *context = (glc::Context *)userParam;

	tcu::TestLog& log	= context->getDeqpContext().getTestContext().getLog();

	log << tcu::TestLog::Message
		<< "DEBUG: source = " << source << ", type= " << type << ", id = " << id << ", severity = " << severity
		<< ", length = " << length << "\n"
		<< "DEBUG: `" << message << "`"
		<< tcu::TestLog::EndMessage;

}

// getFormatReadInfo
// returns the stride in bytes
deUint32 getFormatReadInfo(const subgroups::Format format, GLenum &readFormat, GLenum &readType)
{
	using namespace subgroups;
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			// fall-through
		case FORMAT_R32G32B32A32_SFLOAT:
			readFormat = GL_RGBA;
			readType = GL_FLOAT;
			return 4u;
		case FORMAT_R32G32_SFLOAT:
			readFormat = GL_RG;
			readType = GL_FLOAT;
			return 2u;
		case FORMAT_R32_UINT:
			readFormat = GL_RED_INTEGER;
			readType = GL_UNSIGNED_INT;
			return 1u;
		case FORMAT_R32G32B32A32_UINT:
			readFormat = GL_RGBA_INTEGER;
			readType = GL_UNSIGNED_INT;
			return 4u;
	}
}

deUint32 getMaxWidth ()
{
	return 1024u;
}

deUint32 getNextWidth (const deUint32 width)
{
	if (width < 128)
	{
		// This ensures we test every value up to 128 (the max subgroup size).
		return width + 1;
	}
	else
	{
		// And once we hit 128 we increment to only power of 2's to reduce testing time.
		return width * 2;
	}
}

deUint32 getFormatSizeInBytes(const subgroups::Format format)
{
	using namespace subgroups;
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			return 0;
		case FORMAT_R32_SINT:
		case FORMAT_R32_UINT:
			return sizeof(deInt32);
		case FORMAT_R32G32_SINT:
		case FORMAT_R32G32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case FORMAT_R32G32B32_SINT:
		case FORMAT_R32G32B32_UINT:
		case FORMAT_R32G32B32A32_SINT:
		case FORMAT_R32G32B32A32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
		case FORMAT_R32_SFLOAT:
			return 4;
		case FORMAT_R32G32_SFLOAT:
			return 8;
		case FORMAT_R32G32B32_SFLOAT:
			return 16;
		case FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case FORMAT_R64_SFLOAT:
			return 8;
		case FORMAT_R64G64_SFLOAT:
			return 16;
		case FORMAT_R64G64B64_SFLOAT:
			return 32;
		case FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		// The below formats are used to represent bool and bvec* types. These
		// types are passed to the shader as int and ivec* types, before the
		// calculations are done as booleans. We need a distinct type here so
		// that the shader generators can switch on it and generate the correct
		// shader source for testing.
		case FORMAT_R32_BOOL:
			return sizeof(deInt32);
		case FORMAT_R32G32_BOOL:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case FORMAT_R32G32B32_BOOL:
		case FORMAT_R32G32B32A32_BOOL:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
	}
}

deUint32 getElementSizeInBytes(
	const subgroups::Format format,
	const subgroups::SSBOData::InputDataLayoutType layout)
{
	deUint32 bytes = getFormatSizeInBytes(format);
	if (layout == subgroups::SSBOData::LayoutStd140)
		return bytes < 16 ? 16 : bytes;
	else
		return bytes;
}


de::MovePtr<glu::ShaderProgram> makeGraphicsPipeline(glc::Context&				context,
									  const subgroups::ShaderStageFlags			stages,
									  const GlslSource *						vshader,
									  const GlslSource *						fshader,
									  const GlslSource *						gshader,
									  const GlslSource *						tcshader,
									  const GlslSource *						teshader)
{
	tcu::TestLog&	log			= context.getDeqpContext().getTestContext().getLog();
	const bool		doShaderLog	= log.isShaderLoggingEnabled();
	DE_UNREF(stages);			// only used for asserts

	map<string, string> templateArgs;
	string				versionDecl(getGLSLVersionDeclaration(context.getGLSLVersion()));
	string				tessExtension =
		 context.getDeqpContext().getContextInfo().isExtensionSupported("GL_EXT_tessellation_shader") ?
						   "#extension GL_EXT_tessellation_shader : require" :
						   "";
	templateArgs.insert(pair<string, string>("VERSION_DECL", versionDecl));
	templateArgs.insert(pair<string, string>("TESS_EXTENSION", tessExtension));

	string vertSource, tescSource, teseSource, geomSource, fragSource;
	if (vshader)
	{
		DE_ASSERT(stages & subgroups::SHADER_STAGE_VERTEX_BIT);
		tcu::StringTemplate shaderTemplate(vshader->sources[glu::SHADERTYPE_VERTEX][0]);
		string shaderSource(shaderTemplate.specialize(templateArgs));
		if (doShaderLog)
		{
			log << tcu::TestLog::Message << "vertex shader:\n"
				<< shaderSource << "\n:end:" << tcu::TestLog::EndMessage;
		}
		vertSource = shaderSource;
	}
	if (tcshader)
	{
		DE_ASSERT(stages & subgroups::SHADER_STAGE_TESS_CONTROL_BIT);
		tcu::StringTemplate shaderTemplate(tcshader->sources[glu::SHADERTYPE_TESSELLATION_CONTROL][0]);
		string shaderSource(shaderTemplate.specialize(templateArgs));
		if (doShaderLog)
		{
			log << tcu::TestLog::Message << "tess control shader:\n"
				<< shaderSource << "\n:end:" << tcu::TestLog::EndMessage;
		}
		tescSource = shaderSource;
	}
	if (teshader)
	{
		DE_ASSERT(stages & subgroups::SHADER_STAGE_TESS_EVALUATION_BIT);
		tcu::StringTemplate shaderTemplate(teshader->sources[glu::SHADERTYPE_TESSELLATION_EVALUATION][0]);
		string shaderSource(shaderTemplate.specialize(templateArgs));
		if (doShaderLog)
		{
			log << tcu::TestLog::Message << "tess eval shader:\n"
				<< shaderSource << "\n:end:" << tcu::TestLog::EndMessage;
		}
		teseSource = shaderSource;
	}
	if (gshader)
	{
		DE_ASSERT(stages & subgroups::SHADER_STAGE_GEOMETRY_BIT);
		tcu::StringTemplate shaderTemplate(gshader->sources[glu::SHADERTYPE_GEOMETRY][0]);
		string shaderSource(shaderTemplate.specialize(templateArgs));
		if (doShaderLog)
		{
			log << tcu::TestLog::Message << "geometry shader:\n"
				<< shaderSource << "\n:end:" << tcu::TestLog::EndMessage;
		}
		geomSource = shaderSource;
	}
	if (fshader)
	{
		DE_ASSERT(stages & subgroups::SHADER_STAGE_FRAGMENT_BIT);
		tcu::StringTemplate shaderTemplate(fshader->sources[glu::SHADERTYPE_FRAGMENT][0]);
		string shaderSource(shaderTemplate.specialize(templateArgs));
		if (doShaderLog)
		{
			log << tcu::TestLog::Message << "fragment shader:\n"
				<< shaderSource << "\n:end:" << tcu::TestLog::EndMessage;
		}
		fragSource = shaderSource;
	}

	glu::ShaderProgram *program = DE_NULL;
	if(context.getShaderType() == SHADER_TYPE_GLSL)
	{
		glu::ProgramSources sources;
		if (vshader)
			sources << glu::VertexSource(vertSource);
		if (tcshader)
			sources << glu::TessellationControlSource(tescSource);
		if (teshader)
			sources << glu::TessellationEvaluationSource(teseSource);
		if (gshader)
			sources << glu::GeometrySource(geomSource);
		if (fshader)
			sources << glu::FragmentSource(fragSource);

		program = new glu::ShaderProgram(context.getDeqpContext().getRenderContext().getFunctions(), sources);
	} else {
		DE_ASSERT(context.getShaderType() == SHADER_TYPE_SPIRV);

		glu::ProgramBinaries binaries;
		if (vshader)
			binaries << spirvUtils::makeSpirV(log, glu::VertexSource(vertSource), spirvUtils::SPIRV_VERSION_1_3);
		if (tcshader)
			binaries << spirvUtils::makeSpirV(log, glu::TessellationControlSource(tescSource), spirvUtils::SPIRV_VERSION_1_3);
		if (teshader)
			binaries << spirvUtils::makeSpirV(log, glu::TessellationEvaluationSource(teseSource), spirvUtils::SPIRV_VERSION_1_3);
		if (gshader)
			binaries << spirvUtils::makeSpirV(log, glu::GeometrySource(geomSource), spirvUtils::SPIRV_VERSION_1_3);
		if (fshader)
			binaries << spirvUtils::makeSpirV(log, glu::FragmentSource(fragSource), spirvUtils::SPIRV_VERSION_1_3);

		program = new glu::ShaderProgram(context.getDeqpContext().getRenderContext().getFunctions(), binaries);
	}

	if (!program->isOk())
	{
		log << tcu::TestLog::Message << "Shader build failed.\n"
			<< "Vertex: " << (vshader ? program->getShaderInfo(glu::SHADERTYPE_VERTEX).infoLog : "n/a") << "\n"
			<< "Tess Cont: " << (tcshader ? program->getShaderInfo(glu::SHADERTYPE_TESSELLATION_CONTROL).infoLog : "n/a") << "\n"
			<< "Tess Eval: " << (teshader ? program->getShaderInfo(glu::SHADERTYPE_TESSELLATION_EVALUATION).infoLog : "n/a") << "\n"
			<< "Geometry: " << (gshader ? program->getShaderInfo(glu::SHADERTYPE_GEOMETRY).infoLog : "n/a") << "\n"
			<< "Fragment: " << (fshader ? program->getShaderInfo(glu::SHADERTYPE_FRAGMENT).infoLog : "n/a") << "\n"
			<< "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
	}
	return de::MovePtr<glu::ShaderProgram>(program);
}

de::MovePtr<glu::ShaderProgram> makeComputePipeline(glc::Context& context, const GlslSource &glslTemplate,
									 deUint32 localSizeX, deUint32 localSizeY, deUint32 localSizeZ)
{

	tcu::TestLog&	log			= context.getDeqpContext().getTestContext().getLog();
	const bool		doShaderLog	= log.isShaderLoggingEnabled();

	tcu::StringTemplate computeTemplate(glslTemplate.sources[glu::SHADERTYPE_COMPUTE][0]);

	map<string, string>		templateArgs;
	{
		stringstream localSize;
		localSize << "local_size_x = " << localSizeX;
		templateArgs.insert(pair<string, string>("LOCAL_SIZE_X", localSize.str()));
	}
	{
		stringstream localSize;
		localSize << "local_size_y = " << localSizeY;
		templateArgs.insert(pair<string, string>("LOCAL_SIZE_Y", localSize.str()));
	}
	{
		stringstream localSize;
		localSize << "local_size_z = " << localSizeZ;
		templateArgs.insert(pair<string, string>("LOCAL_SIZE_Z", localSize.str()));
	}
	string versionDecl(getGLSLVersionDeclaration(context.getGLSLVersion()));
	templateArgs.insert(pair<string, string>("VERSION_DECL", versionDecl));

	glu::ComputeSource cshader(glu::ComputeSource(computeTemplate.specialize(templateArgs)));

	if (doShaderLog)
	{
		log << tcu::TestLog::Message << "compute shader specialized source:\n"
			<< cshader.source << "\n:end:" << tcu::TestLog::EndMessage;
	}

	glu::ShaderProgram *program = DE_NULL;
	if(context.getShaderType() == SHADER_TYPE_GLSL)
	{
		glu::ProgramSources sources;
		sources << cshader;
		program = new glu::ShaderProgram(context.getDeqpContext().getRenderContext().getFunctions(), sources);
	} else {
		DE_ASSERT(context.getShaderType() == SHADER_TYPE_SPIRV);

		glu::ProgramBinaries binaries;
		binaries << spirvUtils::makeSpirV(log, cshader, spirvUtils::SPIRV_VERSION_1_3);

		program = new glu::ShaderProgram(context.getDeqpContext().getRenderContext().getFunctions(), binaries);
	}

	if (!program->isOk())
	{
		log << tcu::TestLog::Message << "Shader build failed.\n"
			<< "Compute: " << program->getShaderInfo(glu::SHADERTYPE_COMPUTE).infoLog << "\n"
			<< "Program: " << program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
	}
	return de::MovePtr<glu::ShaderProgram>(program);
}

struct Buffer;
struct Image;

struct BufferOrImage
{
	bool isImage() const
	{
		return m_isImage;
	}

	Buffer* getAsBuffer()
	{
		if (m_isImage) DE_FATAL("Trying to get a buffer as an image!");
		return reinterpret_cast<Buffer* >(this);
	}

	Image* getAsImage()
	{
		if (!m_isImage) DE_FATAL("Trying to get an image as a buffer!");
		return reinterpret_cast<Image*>(this);
	}

	virtual subgroups::DescriptorType getType() const
	{
		if (m_isImage)
		{
			return subgroups::DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		else
		{
			return subgroups::DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
	}

	GLuint getId()
	{
		return m_objectId;
	}

	virtual ~BufferOrImage() {}

protected:
	explicit BufferOrImage(glc::Context& context, bool image)
		: m_gl(context.getDeqpContext().getRenderContext().getFunctions())
		, m_isImage(image)
		, m_objectId(0) {}

	const glw::Functions &	m_gl;
	bool					m_isImage;
	GLuint					m_objectId;
};

struct Buffer : public BufferOrImage
{
	explicit Buffer(
		glc::Context& context, deUint64 sizeInBytes, GLenum target = GL_SHADER_STORAGE_BUFFER)
		: BufferOrImage		(context, false)
		, m_sizeInBytes		(sizeInBytes)
		, m_target			(target)
	{
		m_gl.genBuffers(1, &m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "genBuffers");
		m_gl.bindBuffer(m_target, m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "bindBuffer");
		m_gl.bufferData(m_target, m_sizeInBytes, NULL, GL_DYNAMIC_DRAW);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "bufferData");
		m_gl.bindBuffer(m_target, 0);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "bindBuffer(0)");
	}

	virtual ~Buffer()
	{
		if (m_objectId != 0)
		{
			m_gl.deleteBuffers(1, &m_objectId);
			GLU_EXPECT_NO_ERROR(m_gl.getError(), "glDeleteBuffers");
		}
	}

	virtual subgroups::DescriptorType getType() const
	{
		if (GL_UNIFORM_BUFFER == m_target)
		{
			return subgroups::DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		}
		return subgroups::DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	glw::GLvoid* mapBufferPtr() {
		glw::GLvoid *ptr;

		m_gl.bindBuffer(m_target, m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindBuffer");

		ptr = m_gl.mapBufferRange(m_target, 0, m_sizeInBytes, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glMapBuffer");

		m_gl.bindBuffer(m_target, 0);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindBuffer(0)");

		return ptr;
	}

	void unmapBufferPtr() {
		m_gl.bindBuffer(m_target, m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindBuffer");

		m_gl.unmapBuffer(m_target);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glUnmapBuffer");

		m_gl.bindBuffer(m_target, 0);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindBuffer(0)");
	}

	deUint64 getSize() const {
		return m_sizeInBytes;
	}

private:
	deUint64					m_sizeInBytes;
	const GLenum				m_target;
};

struct Image : public BufferOrImage
{
	explicit Image(glc::Context& context, deUint32 width, deUint32 height,
				   subgroups::Format format)
		: BufferOrImage(context, true)
	{
		m_gl.genTextures(1, &m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGenTextures");
		m_gl.bindTexture(GL_TEXTURE_2D, m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindTexture");
		m_gl.texStorage2D(GL_TEXTURE_2D, 1, format, width, height);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glTexStorage2D");

		m_gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glTexParameteri");
		m_gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glTexParameteri");
	}

	virtual ~Image()
	{
		if (m_objectId != 0)
		{
			m_gl.deleteTextures(1, &m_objectId);
			GLU_EXPECT_NO_ERROR(m_gl.getError(), "glDeleteTextures");
		}
	}

private:
};

struct Vao
{
	explicit Vao(glc::Context& context)
		: m_gl(context.getDeqpContext().getRenderContext().getFunctions())
		, m_objectId(0)
	{
		m_gl.genVertexArrays(1, &m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGenVertexArrays");
		m_gl.bindVertexArray(m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindVertexArray");
	}

	~Vao()
	{
		if (m_objectId != 0)
		{
			m_gl.deleteVertexArrays(1, &m_objectId);
			GLU_EXPECT_NO_ERROR(m_gl.getError(), "glDeleteVertexArrays");
		}
	}

private:
	const glw::Functions &	m_gl;
	GLuint					m_objectId;
};

struct Fbo
{
	explicit Fbo(glc::Context& context)
		: m_gl(context.getDeqpContext().getRenderContext().getFunctions())
		, m_objectId(0)
	{
		m_gl.genFramebuffers(1, &m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGenFramebuffers");
		m_gl.bindFramebuffer(GL_FRAMEBUFFER, m_objectId);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindFramebuffer");
	}

	~Fbo()
	{
		if (m_objectId != 0)
		{
			m_gl.deleteFramebuffers(1, &m_objectId);
			GLU_EXPECT_NO_ERROR(m_gl.getError(), "deleteFramebuffers");
		}
	}

	void bind2D(Image &img)
	{
		m_gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, img.getId(), 0);
		GLU_EXPECT_NO_ERROR(m_gl.getError(), "glFramebufferTexture2D");
	}

private:
	const glw::Functions &	m_gl;
	GLuint					m_objectId;

};
}

std::string glc::subgroups::getSharedMemoryBallotHelper()
{
	return	"shared uvec4 superSecretComputeShaderHelper[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
			"uvec4 sharedMemoryBallot(bool vote)\n"
			"{\n"
			"  uint groupOffset = gl_SubgroupID;\n"
			"  // One invocation in the group 0's the whole group's data\n"
			"  if (subgroupElect())\n"
			"  {\n"
			"    superSecretComputeShaderHelper[groupOffset] = uvec4(0);\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  if (vote)\n"
			"  {\n"
			"    highp uint invocationId = gl_SubgroupInvocationID % 32u;\n"
			"    highp uint bitToSet = 1u << invocationId;\n"
			"    switch (gl_SubgroupInvocationID / 32u)\n"
			"    {\n"
			"    case 0u: atomicOr(superSecretComputeShaderHelper[groupOffset].x, bitToSet); break;\n"
			"    case 1u: atomicOr(superSecretComputeShaderHelper[groupOffset].y, bitToSet); break;\n"
			"    case 2u: atomicOr(superSecretComputeShaderHelper[groupOffset].z, bitToSet); break;\n"
			"    case 3u: atomicOr(superSecretComputeShaderHelper[groupOffset].w, bitToSet); break;\n"
			"    }\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  return superSecretComputeShaderHelper[groupOffset];\n"
			"}\n";
}

deUint32 glc::subgroups::getSubgroupSize(Context& context)
{
	int subgroupSize = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SIZE_KHR);

	return subgroupSize;
}

deUint32 glc::subgroups::maxSupportedSubgroupSize() {
	return 128u;
}

std::string glc::subgroups::getShaderStageName(ShaderStageFlags stage)
{
	DE_ASSERT(stage & SHADER_STAGE_ALL_VALID);
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case SHADER_STAGE_COMPUTE_BIT:
			return "compute";
		case SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		case SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case SHADER_STAGE_GEOMETRY_BIT:
			return "geometry";
		case SHADER_STAGE_TESS_CONTROL_BIT:
			return "tess_control";
		case SHADER_STAGE_TESS_EVALUATION_BIT:
			return "tess_eval";
	}
}

std::string glc::subgroups::getSubgroupFeatureName(SubgroupFeatureFlags bit)
{
	DE_ASSERT(bit & SUBGROUP_FEATURE_ALL_VALID);
	switch (bit)
	{
		default:
			DE_FATAL("Unknown subgroup feature category!");
			return "";
		case SUBGROUP_FEATURE_BASIC_BIT:
			return "GL_SUBGROUP_FEATURE_BASIC_BIT_KHR";
		case SUBGROUP_FEATURE_VOTE_BIT:
			return "GL_SUBGROUP_FEATURE_VOTE_BIT_KHR";
		case SUBGROUP_FEATURE_ARITHMETIC_BIT:
			return "GL_SUBGROUP_FEATURE_ARITHMETIC_BIT_KHR";
		case SUBGROUP_FEATURE_BALLOT_BIT:
			return "GL_SUBGROUP_FEATURE_BALLOT_BIT_KHR";
		case SUBGROUP_FEATURE_SHUFFLE_BIT:
			return "GL_SUBGROUP_FEATURE_SHUFFLE_BIT_KHR";
		case SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT:
			return "GL_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT_KHR";
		case SUBGROUP_FEATURE_CLUSTERED_BIT:
			return "GL_SUBGROUP_FEATURE_CLUSTERED_BIT_KHR";
		case SUBGROUP_FEATURE_QUAD_BIT:
			return "GL_SUBGROUP_FEATURE_QUAD_BIT_KHR";
		case SUBGROUP_FEATURE_PARTITIONED_BIT_NV:
			return "GL_SUBGROUP_FEATURE_PARTITIONED_BIT_NV";
	}
}

void glc::subgroups::addNoSubgroupShader (SourceCollections& programCollection)
{
	{
		const std::string vertNoSubgroupGLSL =
			"${VERSION_DECL}\n"
			"void main (void)\n"
			"{\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"   float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		programCollection.add("vert_noSubgroup") << glu::VertexSource(vertNoSubgroupGLSL);
	}

	{
		const std::string tescNoSubgroupGLSL =
			"${VERSION_DECL}\n"
			"layout(vertices=1) out;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		programCollection.add("tesc_noSubgroup") << glu::TessellationControlSource(tescNoSubgroupGLSL);
	}

	{
		const std::string teseNoSubgroupGLSL =
			"${VERSION_DECL}\n"
			"layout(isolines) in;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";
		programCollection.add("tese_noSubgroup") << glu::TessellationEvaluationSource(teseNoSubgroupGLSL);
	}

}

std::string glc::subgroups::getVertShaderForStage(const ShaderStageFlags stage)
{
	DE_ASSERT(stage & SHADER_STAGE_ALL_VALID);
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case SHADER_STAGE_FRAGMENT_BIT:
			return
				"${VERSION_DECL}\n"
				"void main (void)\n"
				"{\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"   float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"}\n";
		case SHADER_STAGE_GEOMETRY_BIT:
			return
				"${VERSION_DECL}\n"
				"void main (void)\n"
				"{\n"
				"}\n";
		case SHADER_STAGE_TESS_CONTROL_BIT:
		case SHADER_STAGE_TESS_EVALUATION_BIT:
			return
				"${VERSION_DECL}\n"
				"void main (void)\n"
				"{\n"
				"}\n";
	}
}

bool glc::subgroups::isSubgroupSupported(Context& context)
{
	return context.getDeqpContext().getContextInfo().isExtensionSupported("GL_KHR_shader_subgroup");
}

bool glc::subgroups::areSubgroupOperationsSupportedForStage(
	Context& context, const ShaderStageFlags stage)
{
	DE_ASSERT(stage & SHADER_STAGE_ALL_VALID);
	int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

	return (stage & supportedStages) ? true : false;
}

bool glc::subgroups::areSubgroupOperationsRequiredForStage(
	const ShaderStageFlags stage)
{
	DE_ASSERT(stage & SHADER_STAGE_ALL_VALID);
	switch (stage)
	{
		default:
			return false;
		case SHADER_STAGE_COMPUTE_BIT:
			return true;
	}
}

bool glc::subgroups::isSubgroupFeatureSupportedForDevice(
	Context& context,
	const SubgroupFeatureFlags bit)
{
	DE_ASSERT(bit & SUBGROUP_FEATURE_ALL_VALID);

	int supportedOperations = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_FEATURES_KHR);

	return (bit & supportedOperations) ? true : false;
}

bool glc::subgroups::isFragmentSSBOSupportedForDevice(Context& context)
{
	int numFragmentSSBOs = context.getDeqpContext().getContextInfo().getInt(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS);

	return (numFragmentSSBOs > 0) ? true : false;
}

bool glc::subgroups::isVertexSSBOSupportedForDevice(Context& context)
{
	int numVertexSSBOs = context.getDeqpContext().getContextInfo().getInt(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);

	return (numVertexSSBOs > 0) ? true : false;
}

bool glc::subgroups::isImageSupportedForStageOnDevice(Context& context, const ShaderStageFlags stage)
{
	glw::GLint stageQuery;
	DE_ASSERT(stage & SHADER_STAGE_ALL_VALID);

	// image uniforms are optional in VTG stages
	switch (stage)
	{
		case SHADER_STAGE_FRAGMENT_BIT:
		case SHADER_STAGE_COMPUTE_BIT:
		default:
			return true;
		case SHADER_STAGE_VERTEX_BIT:
			stageQuery = GL_MAX_VERTEX_IMAGE_UNIFORMS;
			break;
		case SHADER_STAGE_TESS_CONTROL_BIT:
			stageQuery = GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS;
			break;
		case SHADER_STAGE_TESS_EVALUATION_BIT:
			stageQuery = GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS;
			break;
		case SHADER_STAGE_GEOMETRY_BIT:
			stageQuery = GL_MAX_GEOMETRY_IMAGE_UNIFORMS;
			break;
	}

	int numImages = context.getDeqpContext().getContextInfo().getInt(stageQuery);

	return (numImages > 0) ? true : false;
}

bool glc::subgroups::isDoubleSupportedForDevice(Context& context)
{
	glu::ContextType contextType = context.getDeqpContext().getRenderContext().getType();
	return (glu::contextSupports(contextType, glu::ApiType::core(4, 0)) ||
			context.getDeqpContext().getContextInfo().isExtensionSupported("GL_ARB_gpu_shader_fp64"));
}

bool glc::subgroups::isDoubleFormat(Format format)
{
	switch (format)
	{
		default:
			return false;
		case FORMAT_R64_SFLOAT:
		case FORMAT_R64G64_SFLOAT:
		case FORMAT_R64G64B64_SFLOAT:
		case FORMAT_R64G64B64A64_SFLOAT:
			return true;
	}
}

std::string glc::subgroups::getFormatNameForGLSL (Format format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			return "";
		case FORMAT_R32_SINT:
			return "int";
		case FORMAT_R32G32_SINT:
			return "ivec2";
		case FORMAT_R32G32B32_SINT:
			return "ivec3";
		case FORMAT_R32G32B32A32_SINT:
			return "ivec4";
		case FORMAT_R32_UINT:
			return "uint";
		case FORMAT_R32G32_UINT:
			return "uvec2";
		case FORMAT_R32G32B32_UINT:
			return "uvec3";
		case FORMAT_R32G32B32A32_UINT:
			return "uvec4";
		case FORMAT_R32_SFLOAT:
			return "float";
		case FORMAT_R32G32_SFLOAT:
			return "vec2";
		case FORMAT_R32G32B32_SFLOAT:
			return "vec3";
		case FORMAT_R32G32B32A32_SFLOAT:
			return "vec4";
		case FORMAT_R64_SFLOAT:
			return "double";
		case FORMAT_R64G64_SFLOAT:
			return "dvec2";
		case FORMAT_R64G64B64_SFLOAT:
			return "dvec3";
		case FORMAT_R64G64B64A64_SFLOAT:
			return "dvec4";
		case FORMAT_R32_BOOL:
			return "bool";
		case FORMAT_R32G32_BOOL:
			return "bvec2";
		case FORMAT_R32G32B32_BOOL:
			return "bvec3";
		case FORMAT_R32G32B32A32_BOOL:
			return "bvec4";
	}
}

void glc::subgroups::setVertexShaderFrameBuffer (SourceCollections& programCollection)
{
	programCollection.add("vert") << glu::VertexSource(
		"${VERSION_DECL}\n"
		"layout(location = 0) in highp vec4 in_position;\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = in_position;\n"
		"}\n");
}

void glc::subgroups::setFragmentShaderFrameBuffer (SourceCollections& programCollection)
{
	programCollection.add("fragment") << glu::FragmentSource(
		"${VERSION_DECL}\n"
		"precision highp int;\n"
		"layout(location = 0) in highp float in_color;\n"
		"layout(location = 0) out uint out_color;\n"
		"void main()\n"
		"{\n"
		"	out_color = uint(in_color);\n"
		"}\n");
}

void glc::subgroups::setTesCtrlShaderFrameBuffer (SourceCollections& programCollection)
{
	programCollection.add("tesc") << glu::TessellationControlSource(
		"${VERSION_DECL}\n"
		"#extension GL_KHR_shader_subgroup_basic: enable\n"
		"${TESS_EXTENSION}\n"
		"layout(vertices = 2) out;\n"
		"void main (void)\n"
		"{\n"
		"  if (gl_InvocationID == 0)\n"
		"  {\n"
		"    gl_TessLevelOuter[0] = 1.0f;\n"
		"    gl_TessLevelOuter[1] = 1.0f;\n"
		"  }\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n");
}

void glc::subgroups::setTesEvalShaderFrameBuffer (SourceCollections& programCollection)
{
	programCollection.add("tese") << glu::TessellationEvaluationSource(
		"${VERSION_DECL}\n"
		"#extension GL_KHR_shader_subgroup_ballot: enable\n"
		"${TESS_EXTENSION}\n"
		"layout(isolines, equal_spacing, ccw ) in;\n"
		"layout(location = 0) in float in_color[];\n"
		"layout(location = 0) out float out_color;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
		"  out_color = in_color[0];\n"
		"}\n");
}

void glc::subgroups::addGeometryShadersFromTemplate (const std::string& glslTemplate, SourceCollections& collection)
{
	tcu::StringTemplate geometryTemplate(glslTemplate);

	map<string, string>		linesParams;
	linesParams.insert(pair<string, string>("TOPOLOGY", "lines"));

	map<string, string>		pointsParams;
	pointsParams.insert(pair<string, string>("TOPOLOGY", "points"));

	collection.add("geometry_lines")	<< glu::GeometrySource("${VERSION_DECL}\n" + geometryTemplate.specialize(linesParams));
	collection.add("geometry_points")	<< glu::GeometrySource("${VERSION_DECL}\n" + geometryTemplate.specialize(pointsParams));
}

void initializeMemory(deqp::Context& context, glw::GLvoid *hostPtr, subgroups::SSBOData& data)
{
	using namespace subgroups;
	const Format format = data.format;
	const deUint64 size = data.numElements *
		(data.isImage ? getFormatSizeInBytes(format) : getElementSizeInBytes(format, data.layout));
	if (subgroups::SSBOData::InitializeNonZero == data.initializeType)
	{
		de::Random rnd(context.getTestContext().getCommandLine().getBaseSeed());
		switch (format)
		{
			default:
				DE_FATAL("Illegal buffer format");
				break;
			case FORMAT_R32_BOOL:
			case FORMAT_R32G32_BOOL:
			case FORMAT_R32G32B32_BOOL:
			case FORMAT_R32G32B32A32_BOOL:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(hostPtr);

				for (deUint64 k = 0; k < (size / sizeof(deUint32)); k++)
				{
					deUint32 r = rnd.getUint32();
					ptr[k] = (r & 1) ? r : 0;
				}
			}
			break;
			case FORMAT_R32_SINT:
			case FORMAT_R32G32_SINT:
			case FORMAT_R32G32B32_SINT:
			case FORMAT_R32G32B32A32_SINT:
			case FORMAT_R32_UINT:
			case FORMAT_R32G32_UINT:
			case FORMAT_R32G32B32_UINT:
			case FORMAT_R32G32B32A32_UINT:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(hostPtr);

				for (deUint64 k = 0; k < (size / sizeof(deUint32)); k++)
				{
					ptr[k] = rnd.getUint32();
				}
			}
			break;
			case FORMAT_R32_SFLOAT:
			case FORMAT_R32G32_SFLOAT:
			case FORMAT_R32G32B32_SFLOAT:
			case FORMAT_R32G32B32A32_SFLOAT:
			{
				float* ptr = reinterpret_cast<float*>(hostPtr);

				for (deUint64 k = 0; k < (size / sizeof(float)); k++)
				{
					ptr[k] = rnd.getFloat();
				}
			}
			break;
			case FORMAT_R64_SFLOAT:
			case FORMAT_R64G64_SFLOAT:
			case FORMAT_R64G64B64_SFLOAT:
			case FORMAT_R64G64B64A64_SFLOAT:
			{
				double* ptr = reinterpret_cast<double*>(hostPtr);

				for (deUint64 k = 0; k < (size / sizeof(double)); k++)
				{
					ptr[k] = rnd.getDouble();
				}
			}
			break;
		}
	}
	else if (subgroups::SSBOData::InitializeZero == data.initializeType)
	{
		deUint32* ptr = reinterpret_cast<deUint32*>(hostPtr);

		for (deUint64 k = 0; k < size / 4; k++)
		{
			ptr[k] = 0;
		}
	}

	if (subgroups::SSBOData::InitializeNone != data.initializeType)
	{
		// nothing to do for GL
	}
}

deUint32 getResultBinding (const glc::subgroups::ShaderStageFlags shaderStage)
{
	using namespace glc::subgroups;
	switch(shaderStage)
	{
		case SHADER_STAGE_VERTEX_BIT:
			return 0u;
		case SHADER_STAGE_TESS_CONTROL_BIT:
			return 1u;
		case SHADER_STAGE_TESS_EVALUATION_BIT:
			return 2u;
		case SHADER_STAGE_GEOMETRY_BIT:
			return 3u;
		default:
			DE_ASSERT(0);
			return -1;
	}
	DE_ASSERT(0);
	return -1;
}

tcu::TestStatus glc::subgroups::makeTessellationEvaluationFrameBufferTest(
	Context& context, Format format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const ShaderStageFlags shaderStage)
{
	tcu::TestLog& log	= context.getDeqpContext().getTestContext().getLog();
	const glw::Functions& gl = context.getDeqpContext().getRenderContext().getFunctions();

	const deUint32							maxWidth				= getMaxWidth();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);

	const GlslSource& vshader = context.getSourceCollection().get("vert");
	const GlslSource& tcshader = context.getSourceCollection().get("tesc");
	const GlslSource& teshader = context.getSourceCollection().get("tese");
	const GlslSource& fshader = context.getSourceCollection().get("fragment");

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
			// haven't implemented init for images yet
			DE_ASSERT(extraData[i].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			deUint64 size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, GL_UNIFORM_BUFFER));

			glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, extraData[i]);
			inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
		}
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
	{
		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << ndx << "](" << inputBuffers[ndx]->getType() << ", " << inputBuffers[ndx]->getId() << " ), "
			<< "stage = " << shaderStage << " , binding = " << extraData[ndx].binding << "\n"
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[ndx]->isImage())
		{
			gl.bindImageTexture(extraData[ndx].binding, inputBuffers[ndx]->getId(),
								0, GL_FALSE, 0, GL_READ_ONLY, extraData[ndx].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture()");
		} else
		{
			gl.bindBufferBase(inputBuffers[ndx]->getType(), extraData[ndx].binding, inputBuffers[ndx]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase()");
		}
	}

	de::MovePtr<glu::ShaderProgram> pipeline(
			makeGraphicsPipeline(context, (ShaderStageFlags)(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT | SHADER_STAGE_TESS_CONTROL_BIT | SHADER_STAGE_TESS_EVALUATION_BIT),
								 &vshader, &fshader, DE_NULL, &tcshader, &teshader));
	if (!pipeline->isOk())
	{
		return tcu::TestStatus::fail("tese graphics program build failed");
	}

	const deUint32							subgroupSize			= getSubgroupSize(context);
	const deUint64							vertexBufferSize		= 2ull * maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, GL_ARRAY_BUFFER);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format);

	{
		glw::GLvoid *			bufferPtr			= vertexBuffer.mapBufferPtr();
		std::vector<tcu::Vec4>	data				(2u * maxWidth, tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < data.size(); ndx+=2u)
		{
			data[ndx][0] = leftHandPosition;
			leftHandPosition += pixelSize;
			data[ndx+1][0] = leftHandPosition;
		}

		deMemcpy(bufferPtr, &data[0], data.size() * sizeof(tcu::Vec4));
		vertexBuffer.unmapBufferPtr();
	}

	Vao vao(context);
	Fbo fbo(context);
	fbo.bind2D(discardableImage);

	gl.viewport(0, 0, maxWidth, 1u);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport");

	const deUint64				imageResultSize		= getFormatSizeInBytes(format) * maxWidth;
	vector<glw::GLubyte>		imageBufferResult(imageResultSize);
	const deUint64				vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		{
			gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor");
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClear");

			gl.useProgram(pipeline->getProgram());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

			gl.enableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray");

			gl.bindBuffer(GL_ARRAY_BUFFER, vertexBuffer.getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");

			gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(tcu::Vec4), glu::BufferOffsetAsPointer(vertexBufferOffset));
			GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer");

			gl.patchParameteri(GL_PATCH_VERTICES, 2u);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glPatchParameter(PATCH_VERTICES)");

			gl.drawArrays(GL_PATCHES, 0, 2 * width);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays");

			gl.disableVertexAttribArray(0);

			GLenum readFormat;
			GLenum readType;
			getFormatReadInfo(format, readFormat, readType);

			gl.readPixels(0, 0, width, 1, readFormat, readType, (GLvoid*)&imageBufferResult[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");
		}

		{
			std::vector<const void*> datas;
			datas.push_back(&imageBufferResult[0]);
			if (!checkResult(datas, width/2u, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	} else
	{
		log	<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<<totalIterations << " values passed" << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("OK");
}

bool glc::subgroups::check(std::vector<const void*> datas,
	deUint32 width, deUint32 ref)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 n = 0; n < width; ++n)
	{
		if (data[n] != ref)
		{
			return false;
		}
	}

	return true;
}

bool glc::subgroups::checkCompute(std::vector<const void*> datas,
	const deUint32 numWorkgroups[3], const deUint32 localSize[3],
	deUint32 ref)
{
	const deUint32 globalSizeX = numWorkgroups[0] * localSize[0];
	const deUint32 globalSizeY = numWorkgroups[1] * localSize[1];
	const deUint32 globalSizeZ = numWorkgroups[2] * localSize[2];

	return check(datas, globalSizeX * globalSizeY * globalSizeZ, ref);
}


tcu::TestStatus glc::subgroups::makeGeometryFrameBufferTest(
	Context& context, Format format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	tcu::TestLog& log	= context.getDeqpContext().getTestContext().getLog();
	const glw::Functions& gl = context.getDeqpContext().getRenderContext().getFunctions();

	const deUint32							maxWidth				= getMaxWidth();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);

	const GlslSource& vshader = context.getSourceCollection().get("vert");
	const GlslSource& gshader = context.getSourceCollection().get("geometry");
	const GlslSource& fshader = context.getSourceCollection().get("fragment");

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
			// haven't implemented init for images yet
			DE_ASSERT(extraData[i].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			deUint64 size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, GL_UNIFORM_BUFFER));

			glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, extraData[i]);
			inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
		}
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
	{
		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << ndx << "](" << inputBuffers[ndx]->getType() << ", " << inputBuffers[ndx]->getId() << " ), "
			<< "GEOMETRY, binding = " << extraData[ndx].binding << "\n"
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[ndx]->isImage())
		{
			gl.bindImageTexture(extraData[ndx].binding, inputBuffers[ndx]->getId(),
								0, GL_FALSE, 0, GL_READ_ONLY, extraData[ndx].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture()");
		} else
		{
			gl.bindBufferBase(inputBuffers[ndx]->getType(), extraData[ndx].binding, inputBuffers[ndx]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase()");
		}
	}

	de::MovePtr<glu::ShaderProgram> pipeline(
			makeGraphicsPipeline(context, (ShaderStageFlags)(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT | SHADER_STAGE_GEOMETRY_BIT),
								 &vshader, &fshader, &gshader, DE_NULL, DE_NULL));
	if (!pipeline->isOk())
	{
		return tcu::TestStatus::fail("geom graphics program build failed");
	}

	const deUint32							subgroupSize			= getSubgroupSize(context);
	const deUint64							vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, GL_ARRAY_BUFFER);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format);

	{
		glw::GLvoid *			bufferPtr			= vertexBuffer.mapBufferPtr();
		std::vector<tcu::Vec4>	data				(maxWidth, tcu::Vec4(1.0f, 0.5f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(bufferPtr, &data[0], maxWidth * sizeof(tcu::Vec4));
		vertexBuffer.unmapBufferPtr();
	}

	Vao vao(context);
	Fbo fbo(context);
	fbo.bind2D(discardableImage);

	gl.viewport(0, 0, maxWidth, 1u);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport");

	const deUint64				imageResultSize		= getFormatSizeInBytes(format) * maxWidth;
	vector<glw::GLubyte>		imageBufferResult(imageResultSize);
	const deUint64				vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			if (inputBuffers[ndx]->isImage())
			{
				DE_ASSERT(extraData[ndx].initializeType == subgroups::SSBOData::InitializeNone);
			} else
			{
				glw::GLvoid *ptr = inputBuffers[ndx]->getAsBuffer()->mapBufferPtr();
				initializeMemory(context.getDeqpContext(), ptr, extraData[ndx]);
				inputBuffers[ndx]->getAsBuffer()->unmapBufferPtr();
			}
		}

		{
			gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor");
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClear");

			gl.useProgram(pipeline->getProgram());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

			gl.enableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray");

			gl.bindBuffer(GL_ARRAY_BUFFER, vertexBuffer.getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");

			gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(tcu::Vec4), glu::BufferOffsetAsPointer(vertexBufferOffset));
			GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer");

			gl.drawArrays(GL_POINTS, 0, width);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays");

			gl.disableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDisableVertexAttribArray");

			GLenum readFormat;
			GLenum readType;
			getFormatReadInfo(format, readFormat, readType);

			gl.readPixels(0, 0, width, 1, readFormat, readType, (GLvoid*)&imageBufferResult[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");
		}

		{
			std::vector<const void*> datas;
			datas.push_back(&imageBufferResult[0]);
			if (!checkResult(datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		log	<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	} else
	{
		log	<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<<totalIterations << " values passed" << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus glc::subgroups::allStages(
	Context& context, Format format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const ShaderStageFlags shaderStageTested)
{
	const deUint32					maxWidth			= getMaxWidth();
	vector<ShaderStageFlags>		stagesVector;
	ShaderStageFlags				shaderStageRequired	= (ShaderStageFlags)0ull;
	tcu::TestLog&					log					= context.getDeqpContext().getTestContext().getLog();
	const glw::Functions&			gl					= context.getDeqpContext().getRenderContext().getFunctions();

	if (shaderStageTested & SHADER_STAGE_VERTEX_BIT)
	{
		stagesVector.push_back(SHADER_STAGE_VERTEX_BIT);
	}
	if (shaderStageTested & SHADER_STAGE_TESS_CONTROL_BIT)
	{
		stagesVector.push_back(SHADER_STAGE_TESS_CONTROL_BIT);
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & SHADER_STAGE_TESS_EVALUATION_BIT) ? 0u : (deUint32)SHADER_STAGE_TESS_EVALUATION_BIT));
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & SHADER_STAGE_VERTEX_BIT) ? 0u : (deUint32)SHADER_STAGE_VERTEX_BIT));
	}
	if (shaderStageTested & SHADER_STAGE_TESS_EVALUATION_BIT)
	{
		stagesVector.push_back(SHADER_STAGE_TESS_EVALUATION_BIT);
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & SHADER_STAGE_VERTEX_BIT) ? 0u : (deUint32)SHADER_STAGE_VERTEX_BIT));
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & SHADER_STAGE_TESS_CONTROL_BIT) ? 0u : (deUint32)SHADER_STAGE_TESS_CONTROL_BIT));
	}
	if (shaderStageTested & SHADER_STAGE_GEOMETRY_BIT)
	{
		stagesVector.push_back(SHADER_STAGE_GEOMETRY_BIT);
		const ShaderStageFlags required = SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & required) ? 0u : (deUint32)required));
	}
	if (shaderStageTested & SHADER_STAGE_FRAGMENT_BIT)
	{
		const ShaderStageFlags required = SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired = (ShaderStageFlags)((deUint32)shaderStageRequired | ((deUint32)(shaderStageTested & required) ? 0u : (deUint32)required));
	}

	const deUint32	stagesCount	= static_cast<deUint32>(stagesVector.size());
	const string	vert		= (shaderStageRequired & SHADER_STAGE_VERTEX_BIT)			? "vert_noSubgroup"		: "vert";
	const string	tesc		= (shaderStageRequired & SHADER_STAGE_TESS_CONTROL_BIT)		? "tesc_noSubgroup"		: "tesc";
	const string	tese		= (shaderStageRequired & SHADER_STAGE_TESS_EVALUATION_BIT)	? "tese_noSubgroup"		: "tese";

	shaderStageRequired = (ShaderStageFlags)(shaderStageTested | shaderStageRequired);

	const GlslSource *vshader = &context.getSourceCollection().get(vert);
	const GlslSource *fshader = DE_NULL;
	const GlslSource *gshader = DE_NULL;
	const GlslSource *tcshader = DE_NULL;
	const GlslSource *teshader = DE_NULL;

	if (shaderStageRequired & SHADER_STAGE_TESS_CONTROL_BIT)
	{
		tcshader = &context.getSourceCollection().get(tesc);
		teshader = &context.getSourceCollection().get(tese);
	}
	if (shaderStageRequired & SHADER_STAGE_GEOMETRY_BIT)
	{
		if (shaderStageRequired & SHADER_STAGE_TESS_EVALUATION_BIT)
		{
			// tessellation shaders output line primitives
			gshader = &context.getSourceCollection().get("geometry_lines");
		}
		else
		{
			// otherwise points are processed by geometry shader
			gshader = &context.getSourceCollection().get("geometry_points");
		}
	}
	if (shaderStageRequired & SHADER_STAGE_FRAGMENT_BIT)
	{
		fshader = &context.getSourceCollection().get("fragment");
	}

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(stagesCount + extraDatasCount);

	// The implicit result SSBO we use to store our outputs from the shader
	for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
	{
		const deUint64 shaderSize = (stagesVector[ndx] == SHADER_STAGE_TESS_EVALUATION_BIT) ? maxWidth * 2 : maxWidth;
		const deUint64 size = getElementSizeInBytes(format, SSBOData::LayoutStd430) * shaderSize;
		inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));

		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << ndx << "](" << inputBuffers[ndx]->getType() << ", "
			<< inputBuffers[ndx]->getId() << ", " << size << "), "
			<< "inputstage[" << ndx << "] = " << stagesVector[ndx] << " binding = " << getResultBinding(stagesVector[ndx])
			<< tcu::TestLog::EndMessage;

		gl.bindBufferBase(inputBuffers[ndx]->getType(), getResultBinding(stagesVector[ndx]), inputBuffers[ndx]->getId());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase(ndx, inputBuffers[ndx])");
	}

	for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
	{
		const deUint32 datasNdx = ndx - stagesCount;
		if (extraDatas[datasNdx].isImage)
		{
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraDatas[datasNdx].numElements), 1, extraDatas[datasNdx].format));

			// haven't implemented init for images yet
			DE_ASSERT(extraDatas[datasNdx].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			const deUint64 size = getElementSizeInBytes(extraDatas[datasNdx].format, extraDatas[datasNdx].layout) * extraDatas[datasNdx].numElements;
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));

			glw::GLvoid *ptr = inputBuffers[ndx]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, extraDatas[datasNdx]);
			inputBuffers[ndx]->getAsBuffer()->unmapBufferPtr();
		}

		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << ndx << "](" << inputBuffers[ndx]->getType() << ", "
			<< inputBuffers[ndx]->getId() << ", " << extraDatas[datasNdx].numElements << " els), "
			<< "extrastage[" << datasNdx << "] = " << extraDatas[datasNdx].stages << " binding = " << extraDatas[datasNdx].binding
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[ndx]->isImage())
		{
			gl.bindImageTexture(extraDatas[datasNdx].binding, inputBuffers[ndx]->getId(),
								0, GL_FALSE, 0, GL_READ_WRITE, extraDatas[datasNdx].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture(extraDatas[datasNdx])");
		} else
		{
			gl.bindBufferBase(inputBuffers[ndx]->getType(), extraDatas[datasNdx].binding, inputBuffers[ndx]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase(extraDatas[datasNdx])");
		}
	}

	de::MovePtr<glu::ShaderProgram> pipeline(
			makeGraphicsPipeline(context, shaderStageRequired, vshader, fshader, gshader, tcshader, teshader));

	if (!pipeline->isOk())
	{
		return tcu::TestStatus::fail("allstages graphics program build failed");
	}

	{
		const deUint32					subgroupSize			= getSubgroupSize(context);
		unsigned						totalIterations			= 0u;
		unsigned						failedIterations		= 0u;
		Image							resultImage				(context, maxWidth, 1, format);
		const deUint64					imageResultSize			= getFormatSizeInBytes(format) * maxWidth;
		vector<glw::GLubyte>			imageBufferResult(imageResultSize);

		Vao vao(context);
		Fbo fbo(context);
		fbo.bind2D(resultImage);

		gl.viewport(0, 0, maxWidth, 1u);
		GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

		for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
		{
			for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
			{
				// re-init the data
				if (extraDatas[ndx - stagesCount].isImage)
				{
					// haven't implemented init for images yet
					DE_ASSERT(extraDatas[ndx - stagesCount].initializeType == subgroups::SSBOData::InitializeNone);
				} else
				{
					glw::GLvoid *ptr = inputBuffers[ndx]->getAsBuffer()->mapBufferPtr();
					initializeMemory(context.getDeqpContext(), ptr, extraDatas[ndx - stagesCount]);
					inputBuffers[ndx]->getAsBuffer()->unmapBufferPtr();
				}
			}

			totalIterations++;

			gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor");
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClear");

			gl.useProgram(pipeline->getProgram());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

			glw::GLenum drawType;
			if (shaderStageRequired & SHADER_STAGE_TESS_CONTROL_BIT)
			{
				drawType = GL_PATCHES;
				gl.patchParameteri(GL_PATCH_VERTICES, 1u);
				GLU_EXPECT_NO_ERROR(gl.getError(), "glPatchParameter(PATCH_VERTICES)");
			} else
			{
				drawType = GL_POINTS;
			}

			gl.drawArrays(drawType, 0, width);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays");

			GLenum readFormat;
			GLenum readType;
			getFormatReadInfo(format, readFormat, readType);

			gl.readPixels(0, 0, width, 1, readFormat, readType, (GLvoid*)&imageBufferResult[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

			for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
			{
				std::vector<const void*> datas;
				std::vector<Buffer *> buffersToUnmap;

				if (!inputBuffers[ndx]->isImage())
				{
					glw::GLvoid * resultData = inputBuffers[ndx]->getAsBuffer()->mapBufferPtr();
					buffersToUnmap.push_back(inputBuffers[ndx]->getAsBuffer());
					// we always have our result data first
					datas.push_back(resultData);
				}

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if ((stagesVector[ndx] & extraDatas[datasNdx].stages) && (!inputBuffers[index]->isImage()))
					{
						glw::GLvoid * resultData = inputBuffers[index]->getAsBuffer()->mapBufferPtr();
						buffersToUnmap.push_back(inputBuffers[index]->getAsBuffer());
						datas.push_back(resultData);
					}
				}

				if (!checkResult(datas, (stagesVector[ndx] == SHADER_STAGE_TESS_EVALUATION_BIT) ? width * 2 : width , subgroupSize))
					failedIterations++;

				while( !buffersToUnmap.empty() )
				{
					Buffer * buf = buffersToUnmap.back();
					buf->unmapBufferPtr();
					buffersToUnmap.pop_back();
				}
			}
			if (shaderStageTested & SHADER_STAGE_FRAGMENT_BIT)
			{
				std::vector<const void*> datas;
				std::vector<Buffer *> buffersToUnmap;

				// we always have our result data first
				datas.push_back(&imageBufferResult[0]);

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if (SHADER_STAGE_FRAGMENT_BIT & extraDatas[datasNdx].stages && (!inputBuffers[index]->isImage()))
					{
						glw::GLvoid * resultData = inputBuffers[index]->getAsBuffer()->mapBufferPtr();
						buffersToUnmap.push_back(inputBuffers[index]->getAsBuffer());
						// we always have our result data first
						datas.push_back(resultData);
					}
				}

				if (!checkResult(datas, width, subgroupSize))
					failedIterations++;

				while( !buffersToUnmap.empty() )
				{
					Buffer * buf = buffersToUnmap.back();
					buf->unmapBufferPtr();
					buffersToUnmap.pop_back();
				}
			}

		}

		if (0 < failedIterations)
		{
			log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
					<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Failed!");
		} else
		{
			log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
					<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		}
	}
	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus glc::subgroups::makeVertexFrameBufferTest(Context& context, Format format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	tcu::TestLog& log	= context.getDeqpContext().getTestContext().getLog();
	const glw::Functions& gl = context.getDeqpContext().getRenderContext().getFunctions();

	const deUint32							maxWidth				= getMaxWidth();
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);

	const GlslSource& vshader = context.getSourceCollection().get("vert");
	const GlslSource& fshader = context.getSourceCollection().get("fragment");

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));

			// haven't implemented init for images yet
			DE_ASSERT(extraData[i].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			deUint64 size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, GL_UNIFORM_BUFFER));

			glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, extraData[i]);
			inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
		}
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
	{
		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << ndx << "](" << inputBuffers[ndx]->getType() << ", " << inputBuffers[ndx]->getId() << " ), "
			<< "VERTEX, binding = " << extraData[ndx].binding << "\n"
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[ndx]->isImage())
		{
			gl.bindImageTexture(extraData[ndx].binding, inputBuffers[ndx]->getId(),
								0, GL_FALSE, 0, GL_READ_ONLY, extraData[ndx].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture()");
		} else
		{
			gl.bindBufferBase(inputBuffers[ndx]->getType(), extraData[ndx].binding, inputBuffers[ndx]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase()");
		}
	}

	de::MovePtr<glu::ShaderProgram> pipeline(
			makeGraphicsPipeline(context, (ShaderStageFlags)(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT),
								 &vshader, &fshader, DE_NULL, DE_NULL, DE_NULL));

	if (!pipeline->isOk())
	{
		return tcu::TestStatus::fail("vert graphics program build failed");
	}

	const deUint32							subgroupSize			= getSubgroupSize(context);

	const deUint64							vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, GL_ARRAY_BUFFER);

	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;

	Image									discardableImage		(context, maxWidth, 1u, format);

	{
		glw::GLvoid *			bufferPtr			= vertexBuffer.mapBufferPtr();
		std::vector<tcu::Vec4>	data				(maxWidth, tcu::Vec4(1.0f, 0.5f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(bufferPtr, &data[0], maxWidth * sizeof(tcu::Vec4));
		vertexBuffer.unmapBufferPtr();
	}

	Vao vao(context);
	Fbo fbo(context);
	fbo.bind2D(discardableImage);

	gl.viewport(0, 0, maxWidth, 1u);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport");

	const deUint64				imageResultSize		= getFormatSizeInBytes(format) * maxWidth;
	vector<glw::GLubyte>		imageBufferResult(imageResultSize);
	const deUint64				vertexBufferOffset	= 0u;

	for (deUint32 width = 1u; width < maxWidth; width = getNextWidth(width))
	{
		totalIterations++;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			if (inputBuffers[ndx]->isImage())
			{
				DE_ASSERT(extraData[ndx].initializeType == subgroups::SSBOData::InitializeNone);
			} else
			{
				glw::GLvoid *ptr = inputBuffers[ndx]->getAsBuffer()->mapBufferPtr();
				initializeMemory(context.getDeqpContext(), ptr, extraData[ndx]);
				inputBuffers[ndx]->getAsBuffer()->unmapBufferPtr();
			}
		}

		{
			gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor");
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClear");

			gl.useProgram(pipeline->getProgram());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

			gl.enableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray");

			gl.bindBuffer(GL_ARRAY_BUFFER, vertexBuffer.getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");

			gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(tcu::Vec4), glu::BufferOffsetAsPointer(vertexBufferOffset));
			GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer");

			gl.drawArrays(GL_POINTS, 0, width);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays");

			gl.disableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDisableVertexAttribArray");

			GLenum readFormat;
			GLenum readType;
			getFormatReadInfo(format, readFormat, readType);

			gl.readPixels(0, 0, width, 1, readFormat, readType, (GLvoid*)&imageBufferResult[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");
		}

		{
			std::vector<const void*> datas;
			datas.push_back(&imageBufferResult[0]);
			if (!checkResult(datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		log	<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
			<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	} else
	{
		log	<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<<totalIterations << " values passed" << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("OK");
}


tcu::TestStatus glc::subgroups::makeFragmentFrameBufferTest	(Context& context, Format format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width,
						deUint32 height, deUint32 subgroupSize))
{
	tcu::TestLog& log	= context.getDeqpContext().getTestContext().getLog();
	const glw::Functions& gl = context.getDeqpContext().getRenderContext().getFunctions();

	const GlslSource& vshader = context.getSourceCollection().get("vert");
	const GlslSource& fshader = context.getSourceCollection().get("fragment");

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount);

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));

			// haven't implemented init for images yet
			DE_ASSERT(extraDatas[i].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			deUint64 size =
				getElementSizeInBytes(extraDatas[i].format, extraDatas[i].layout) * extraDatas[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, GL_UNIFORM_BUFFER));

			glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, extraDatas[i]);
			inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
		}
	}

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << i << "](" << inputBuffers[i]->getType() << ", " << inputBuffers[i]->getId() << " ), "
			<< "FRAGMENT, binding = " << extraDatas[i].binding << "\n"
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[i]->isImage())
		{
			gl.bindImageTexture(extraDatas[i].binding, inputBuffers[i]->getId(),
								0, GL_FALSE, 0, GL_READ_ONLY, extraDatas[i].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture()");
		} else
		{
			gl.bindBufferBase(inputBuffers[i]->getType(), extraDatas[i].binding, inputBuffers[i]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase()");
		}
	}

	de::MovePtr<glu::ShaderProgram> pipeline(
			makeGraphicsPipeline(context, (ShaderStageFlags)(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT),
								 &vshader, &fshader, DE_NULL, DE_NULL, DE_NULL));

	if (!pipeline->isOk())
	{
		return tcu::TestStatus::fail("frag graphics program build failed");
	}

	const deUint32 subgroupSize = getSubgroupSize(context);

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	Vao vao(context);
	Fbo fbo(context);

	for (deUint32 width = 8; width <= subgroupSize; width *= 2)
	{
		for (deUint32 height = 8; height <= subgroupSize; height *= 2)
		{
			totalIterations++;

			// re-init the data
			for (deUint32 i = 0; i < extraDatasCount; i++)
			{
				if (inputBuffers[i]->isImage())
				{
					DE_ASSERT(extraDatas[i].initializeType == subgroups::SSBOData::InitializeNone);
				} else
				{
					glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
					initializeMemory(context.getDeqpContext(), ptr, extraDatas[i]);
					inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
				}
			}

			deUint64 formatSize = getFormatSizeInBytes(format);
			const deUint64 resultImageSizeInBytes =
				width * height * formatSize;

			Image resultImage(context, width, height, format);

			vector<glw::GLubyte>  resultBuffer(resultImageSizeInBytes);

			fbo.bind2D(resultImage);

			gl.viewport(0, 0, width, height);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport");

			gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClearColor");
			gl.clear(GL_COLOR_BUFFER_BIT);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glClear");

			gl.useProgram(pipeline->getProgram());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

			gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays");

			GLenum readFormat;
			GLenum readType;
			getFormatReadInfo(format, readFormat, readType);

			gl.readPixels(0, 0, width, height, readFormat, readType, (GLvoid*)&resultBuffer[0]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

			std::vector<const void*> datas;
			{
				// we always have our result data first
				datas.push_back(&resultBuffer[0]);
			}

			if (!checkResult(datas, width, height, subgroupSize))
			{
				failedIterations++;
			}
		}
	}

	if (0 < failedIterations)
	{
		log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	} else
	{
		log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<<totalIterations << " values passed" << tcu::TestLog::EndMessage;
	}
	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus glc::subgroups::makeComputeTest(
	Context& context, Format format, SSBOData* inputs, deUint32 inputsCount,
	bool (*checkResult)(std::vector<const void*> datas,
						const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						deUint32 subgroupSize))
{
	const glw::Functions& gl = context.getDeqpContext().getRenderContext().getFunctions();
	deUint64 elementSize = getFormatSizeInBytes(format);

	const deUint64 resultBufferSize = maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize();
	const deUint64 resultBufferSizeInBytes = resultBufferSize * elementSize;

	Buffer resultBuffer(
		context, resultBufferSizeInBytes);

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(inputsCount);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputs[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(inputs[i].numElements), 1, inputs[i].format));
			// haven't implemented init for images yet
			DE_ASSERT(inputs[i].initializeType == subgroups::SSBOData::InitializeNone);
		}
		else
		{
			deUint64 size =
				getElementSizeInBytes(inputs[i].format, inputs[i].layout) * inputs[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));

			glw::GLvoid *ptr = inputBuffers[i]->getAsBuffer()->mapBufferPtr();
			initializeMemory(context.getDeqpContext(), ptr, inputs[i]);
			inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
		}

	}

	tcu::TestLog& log	= context.getDeqpContext().getTestContext().getLog();
	log << tcu::TestLog::Message
		<< "binding resultbuffer(type=" << resultBuffer.getType()
		<< ", id=" << resultBuffer.getId() << ", binding=0), COMPUTE"
		<< tcu::TestLog::EndMessage;

	gl.bindBufferBase(resultBuffer.getType(), 0, resultBuffer.getId());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase(0, resultBuffer)");

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		log << tcu::TestLog::Message
			<< "binding inputBuffers[" << i << "](type=" << inputBuffers[i]->getType()
			<< ", id=" << inputBuffers[i]->getId() << ", binding="
			<< inputs[i].binding << "), 1, COMPUTE"
			<< tcu::TestLog::EndMessage;

		if (inputBuffers[i]->isImage())
		{
			gl.bindImageTexture(inputs[i].binding, inputBuffers[i]->getId(),
								0, GL_FALSE, 0, GL_READ_WRITE, inputs[i].format);
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindImageTexture(inputBuffer[i]");
		} else
		{
			gl.bindBufferBase(inputBuffers[i]->getType(), inputs[i].binding, inputBuffers[i]->getId());
			GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBufferBase(inputBuffer[i])");
		}
	}

	const GlslSource &cshader = context.getSourceCollection().get("comp");

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	const deUint32 subgroupSize = getSubgroupSize(context);

	const deUint32 numWorkgroups[3] = {4, 2, 2};

	const deUint32 localSizesToTestCount = 15;
	deUint32 localSizesToTest[localSizesToTestCount][3] =
	{
		{1, 1, 1},
		{32, 4, 1},
		{32, 1, 4},
		{1, 32, 4},
		{1, 4, 32},
		{4, 1, 32},
		{4, 32, 1},
		{subgroupSize, 1, 1},
		{1, subgroupSize, 1},
		{1, 1, subgroupSize},
		{3, 5, 7},
		{128, 1, 1},
		{1, 128, 1},
		{1, 1, 64},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};


	de::MovePtr<glu::ShaderProgram> lastPipeline(
		makeComputePipeline(context, cshader,
							localSizesToTest[0][0], localSizesToTest[0][1], localSizesToTest[0][2]));

	for (deUint32 index = 0; index < (localSizesToTestCount - 1); index++)
	{
		const deUint32 nextX = localSizesToTest[index + 1][0];
		const deUint32 nextY = localSizesToTest[index + 1][1];
		const deUint32 nextZ = localSizesToTest[index + 1][2];

		// we are running one test
		totalIterations++;

		if (!lastPipeline->isOk())
		{
			return tcu::TestStatus::fail("compute shaders build failed");
		}

		gl.useProgram(lastPipeline->getProgram());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram");

		gl.dispatchCompute(numWorkgroups[0], numWorkgroups[1], numWorkgroups[2]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDispatchCompute");

		de::MovePtr<glu::ShaderProgram> nextPipeline(
			makeComputePipeline(context, cshader, nextX, nextY, nextZ));

		std::vector<const void*> datas;

		{
			glw::GLvoid * resultData = resultBuffer.mapBufferPtr();

			// we always have our result data first
			datas.push_back(resultData);
		}

		for (deUint32 i = 0; i < inputsCount; i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				glw::GLvoid *resultData = inputBuffers[i]->getAsBuffer()->mapBufferPtr();

				// we always have our result data first
				datas.push_back(resultData);
			}
		}

		if (!checkResult(datas, numWorkgroups, localSizesToTest[index], subgroupSize))
		{
			failedIterations++;
		}

		resultBuffer.unmapBufferPtr();
		for (deUint32 i = 0; i < inputsCount; i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				inputBuffers[i]->getAsBuffer()->unmapBufferPtr();
			}
		}

		lastPipeline = nextPipeline;
	}

	if (0 < failedIterations)
	{
		log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	} else
	{
		log		<< tcu::TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("OK");
}
