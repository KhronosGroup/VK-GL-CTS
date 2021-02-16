/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan Test Package
 *//*--------------------------------------------------------------------*/

#include "vktTestPackage.hpp"

#include "tcuPlatform.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"

#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkBinaryRegistry.hpp"
#include "vkShaderToSpirV.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkRenderDocUtil.hpp"

#include "deUniquePtr.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktApiTests.hpp"
#include "vktPipelineTests.hpp"
#include "vktBindingModelTests.hpp"
#include "vktSpvAsmTests.hpp"
#include "vktShaderLibrary.hpp"
#include "vktRenderPassTests.hpp"
#include "vktMemoryTests.hpp"
#include "vktShaderRenderBuiltinVarTests.hpp"
#include "vktShaderRenderDerivateTests.hpp"
#include "vktShaderRenderDiscardTests.hpp"
#include "vktShaderRenderIndexingTests.hpp"
#include "vktShaderRenderInvarianceTests.hpp"
#include "vktShaderRenderLimitTests.hpp"
#include "vktShaderRenderLoopTests.hpp"
#include "vktShaderRenderMatrixTests.hpp"
#include "vktShaderRenderOperatorTests.hpp"
#include "vktShaderRenderReturnTests.hpp"
#include "vktShaderRenderStructTests.hpp"
#include "vktShaderRenderSwitchTests.hpp"
#include "vktShaderRenderTextureFunctionTests.hpp"
#include "vktShaderRenderTextureGatherTests.hpp"
#include "vktShaderBuiltinTests.hpp"
#include "vktOpaqueTypeIndexingTests.hpp"
#include "vktAtomicOperationTests.hpp"
#include "vktUniformBlockTests.hpp"
#include "vktDynamicStateTests.hpp"
#include "vktSSBOLayoutTests.hpp"
#include "vktQueryPoolTests.hpp"
#include "vktDrawTests.hpp"
#include "vktComputeTests.hpp"
#include "vktConditionalTests.hpp"
#include "vktImageTests.hpp"
#include "vktInfoTests.hpp"
#include "vktWsiTests.hpp"
#include "vktSynchronizationTests.hpp"
#include "vktSparseResourcesTests.hpp"
#include "vktTessellationTests.hpp"
#include "vktRasterizationTests.hpp"
#include "vktClippingTests.hpp"
#include "vktFragmentOperationsTests.hpp"
#include "vktTextureTests.hpp"
#include "vktGeometryTests.hpp"
#include "vktRobustnessTests.hpp"
#include "vktMultiViewTests.hpp"
#include "vktSubgroupsTests.hpp"
#include "vktYCbCrTests.hpp"
#include "vktProtectedMemTests.hpp"
#include "vktDeviceGroupTests.hpp"
#include "vktMemoryModelTests.hpp"
#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberGlslTests.hpp"
#include "vktImagelessFramebufferTests.hpp"
#include "vktTransformFeedbackTests.hpp"
#include "vktDescriptorIndexingTests.hpp"
#include "vktImagelessFramebufferTests.hpp"
#include "vktFragmentShaderInterlockTests.hpp"
#include "vktShaderClockTests.hpp"
#include "vktModifiersTests.hpp"
#include "vktRayTracingTests.hpp"
#include "vktRayQueryTests.hpp"
#include "vktPostmortemTests.hpp"
#include "vktFragmentShadingRateTests.hpp"

#include <vector>
#include <sstream>

namespace // compilation
{

vk::ProgramBinary* compileProgram (const vk::GlslSource& source, glu::ShaderProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary* compileProgram (const vk::HlslSource& source, glu::ShaderProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	return vk::buildProgram(source, buildInfo, commandLine);
}

vk::ProgramBinary* compileProgram (const vk::SpirVAsmSource& source, vk::SpirVProgramInfo* buildInfo, const tcu::CommandLine& commandLine)
{
	return vk::assembleProgram(source, buildInfo, commandLine);
}

template <typename InfoType, typename IteratorType>
vk::ProgramBinary* buildProgram (const std::string&					casePath,
								 IteratorType						iter,
								 const vk::BinaryRegistryReader&	prebuiltBinRegistry,
								 tcu::TestLog&						log,
								 vk::BinaryCollection*				progCollection,
								 const tcu::CommandLine&			commandLine)
{
	const vk::ProgramIdentifier		progId		(casePath, iter.getName());
	const tcu::ScopedLogSection		progSection	(log, iter.getName(), "Program: " + iter.getName());
	de::MovePtr<vk::ProgramBinary>	binProg;
	InfoType						buildInfo;

	try
	{
		binProg	= de::MovePtr<vk::ProgramBinary>(compileProgram(iter.getProgram(), &buildInfo, commandLine));
		log << buildInfo;
	}
	catch (const tcu::NotSupportedError& err)
	{
		// Try to load from cache
		log << err << tcu::TestLog::Message << "Building from source not supported, loading stored binary instead" << tcu::TestLog::EndMessage;

		binProg = de::MovePtr<vk::ProgramBinary>(prebuiltBinRegistry.loadProgram(progId));

		log << iter.getProgram();
	}
	catch (const tcu::Exception&)
	{
		// Build failed for other reason
		log << buildInfo;
		throw;
	}

	TCU_CHECK_INTERNAL(binProg);

	{
		vk::ProgramBinary* const	returnBinary	= binProg.get();

		progCollection->add(progId.programName, binProg);

		return returnBinary;
	}
}

} // anonymous(compilation)

namespace vkt
{

using std::vector;
using de::UniquePtr;
using de::MovePtr;
using tcu::TestLog;

// TestCaseExecutor

class TestCaseExecutor : public tcu::TestCaseExecutor
{
public:
												TestCaseExecutor	(tcu::TestContext& testCtx);
												~TestCaseExecutor	(void);

	virtual void								init				(tcu::TestCase* testCase, const std::string& path);
	virtual void								deinit				(tcu::TestCase* testCase);

	virtual tcu::TestNode::IterateResult		iterate				(tcu::TestCase* testCase);

private:
	bool										spirvVersionSupported(vk::SpirvVersion);
	vk::BinaryCollection						m_progCollection;
	vk::BinaryRegistryReader					m_prebuiltBinRegistry;

	const UniquePtr<vk::Library>				m_library;
	Context										m_context;

	const UniquePtr<vk::RenderDocUtil>			m_renderDoc;
	vk::VkPhysicalDeviceProperties				m_deviceProperties;
	tcu::WaiverUtil								m_waiverMechanism;

	TestInstance*								m_instance;			//!< Current test case instance
};

static MovePtr<vk::Library> createLibrary (tcu::TestContext& testCtx)
{
	return MovePtr<vk::Library>(testCtx.getPlatform().getVulkanPlatform().createLibrary());
}

static vk::VkPhysicalDeviceProperties getPhysicalDeviceProperties(vkt::Context& context)
{
	const vk::InstanceInterface&	vki				= context.getInstanceInterface();
	const vk::VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	vk::VkPhysicalDeviceProperties	properties;
	vki.getPhysicalDeviceProperties(physicalDevice, &properties);
	return properties;
}

TestCaseExecutor::TestCaseExecutor (tcu::TestContext& testCtx)
	: m_prebuiltBinRegistry	(testCtx.getArchive(), "vulkan/prebuilt")
	, m_library				(createLibrary(testCtx))
	, m_context				(testCtx, m_library->getPlatformInterface(), m_progCollection)
	, m_renderDoc			(testCtx.getCommandLine().isRenderDocEnabled()
							 ? MovePtr<vk::RenderDocUtil>(new vk::RenderDocUtil())
							 : MovePtr<vk::RenderDocUtil>(DE_NULL))
	, m_deviceProperties	(getPhysicalDeviceProperties(m_context))
	, m_instance			(DE_NULL)
{
	tcu::SessionInfo sessionInfo(m_deviceProperties.vendorID,
								 m_deviceProperties.deviceID,
								 testCtx.getCommandLine().getInitialCmdLine());
	m_waiverMechanism.setup(testCtx.getCommandLine().getWaiverFileName(),
							"dEQP-VK",
							m_deviceProperties.vendorID,
							m_deviceProperties.deviceID,
							sessionInfo);
	testCtx.getLog().writeSessionInfo(sessionInfo.get());
}

TestCaseExecutor::~TestCaseExecutor (void)
{
	delete m_instance;
}

void TestCaseExecutor::init (tcu::TestCase* testCase, const std::string& casePath)
{
	TestCase*					vktCase						= dynamic_cast<TestCase*>(testCase);
	tcu::TestLog&				log							= m_context.getTestContext().getLog();
	const deUint32				usedVulkanVersion			= m_context.getUsedApiVersion();
	const vk::SpirvVersion		baselineSpirvVersion		= vk::getBaselineSpirvVersion(usedVulkanVersion);
	vk::ShaderBuildOptions		defaultGlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
	vk::ShaderBuildOptions		defaultHlslBuildOptions		(usedVulkanVersion, baselineSpirvVersion, 0u);
	vk::SpirVAsmBuildOptions	defaultSpirvAsmBuildOptions	(usedVulkanVersion, baselineSpirvVersion);
	vk::SourceCollections		sourceProgs					(usedVulkanVersion, defaultGlslBuildOptions, defaultHlslBuildOptions, defaultSpirvAsmBuildOptions);
	const bool					doShaderLog					= log.isShaderLoggingEnabled();
	const tcu::CommandLine&		commandLine					= m_context.getTestContext().getCommandLine();

	DE_UNREF(casePath); // \todo [2015-03-13 pyry] Use this to identify ProgramCollection storage path

	if (!vktCase)
		TCU_THROW(InternalError, "Test node not an instance of vkt::TestCase");

	if (m_waiverMechanism.isOnWaiverList(casePath))
		throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

	vktCase->checkSupport(m_context);

	vktCase->delayedInit();

	m_progCollection.clear();
	vktCase->initPrograms(sourceProgs);

	for (vk::GlslSourceCollection::Iterator progIter = sourceProgs.glslSources.begin(); progIter != sourceProgs.glslSources.end(); ++progIter)
	{
		if (!spirvVersionSupported(progIter.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		const vk::ProgramBinary* const binProg = buildProgram<glu::ShaderProgramInfo, vk::GlslSourceCollection::Iterator>(casePath, progIter, m_prebuiltBinRegistry, log, &m_progCollection, commandLine);

		if (doShaderLog)
		{
			try
			{
				std::ostringstream disasm;

				vk::disassembleProgram(*binProg, &disasm);

				log << vk::SpirVAsmSource(disasm.str());
			}
			catch (const tcu::NotSupportedError& err)
			{
				log << err;
			}
		}
	}

	for (vk::HlslSourceCollection::Iterator progIter = sourceProgs.hlslSources.begin(); progIter != sourceProgs.hlslSources.end(); ++progIter)
	{
		if (!spirvVersionSupported(progIter.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		const vk::ProgramBinary* const binProg = buildProgram<glu::ShaderProgramInfo, vk::HlslSourceCollection::Iterator>(casePath, progIter, m_prebuiltBinRegistry, log, &m_progCollection, commandLine);

		if (doShaderLog)
		{
			try
			{
				std::ostringstream disasm;

				vk::disassembleProgram(*binProg, &disasm);

				log << vk::SpirVAsmSource(disasm.str());
			}
			catch (const tcu::NotSupportedError& err)
			{
				log << err;
			}
		}
	}

	for (vk::SpirVAsmCollection::Iterator asmIterator = sourceProgs.spirvAsmSources.begin(); asmIterator != sourceProgs.spirvAsmSources.end(); ++asmIterator)
	{
		if (!spirvVersionSupported(asmIterator.getProgram().buildOptions.targetVersion))
			TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");

		buildProgram<vk::SpirVProgramInfo, vk::SpirVAsmCollection::Iterator>(casePath, asmIterator, m_prebuiltBinRegistry, log, &m_progCollection, commandLine);
	}

	if (m_renderDoc) m_renderDoc->startFrame(m_context.getInstance());

	DE_ASSERT(!m_instance);
	m_instance = vktCase->createInstance(m_context);
	m_context.resultSetOnValidation(false);
}

void TestCaseExecutor::deinit (tcu::TestCase*)
{
	delete m_instance;
	m_instance = DE_NULL;

	if (m_renderDoc) m_renderDoc->endFrame(m_context.getInstance());

	// Collect and report any debug messages
	if (m_context.hasDebugReportRecorder())
		collectAndReportDebugMessages(m_context.getDebugReportRecorder(), m_context);
}

tcu::TestNode::IterateResult TestCaseExecutor::iterate (tcu::TestCase*)
{
	DE_ASSERT(m_instance);

	const tcu::TestStatus	result	= m_instance->iterate();

	if (result.isComplete())
	{
		// Vulkan tests shouldn't set result directly except when using a debug report messenger to catch validation errors.
		DE_ASSERT(m_context.getTestContext().getTestResult() == QP_TEST_RESULT_LAST || m_context.resultSetOnValidation());

		// Override result if not set previously by a debug report messenger.
		if (!m_context.resultSetOnValidation())
			m_context.getTestContext().setTestResult(result.getCode(), result.getDescription().c_str());
		return tcu::TestNode::STOP;
	}
	else
		return tcu::TestNode::CONTINUE;
}

bool TestCaseExecutor::spirvVersionSupported (vk::SpirvVersion spirvVersion)
{
	if (spirvVersion <= vk::getMaxSpirvVersionForVulkan(m_context.getUsedApiVersion()))
		return true;

	if (spirvVersion <= vk::SPIRV_VERSION_1_4)
		return m_context.isDeviceFunctionalitySupported("VK_KHR_spirv_1_4");

	return false;
}

// GLSL shader tests

void createGlslTests (tcu::TestCaseGroup* glslTests)
{
	tcu::TestContext&	testCtx		= glslTests->getTestContext();

	// ShaderLibrary-based tests
	static const struct
	{
		const char*		name;
		const char*		description;
	} s_es310Tests[] =
	{
		{ "arrays",						"Arrays"					},
		{ "conditionals",				"Conditional statements"	},
		{ "constant_expressions",		"Constant expressions"		},
		{ "constants",					"Constants"					},
		{ "conversions",				"Type conversions"			},
		{ "functions",					"Functions"					},
		{ "linkage",					"Linking"					},
		{ "scoping",					"Scoping"					},
		{ "swizzles",					"Swizzles"					},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_es310Tests); ndx++)
		glslTests->addChild(createShaderLibraryGroup(testCtx,
													 s_es310Tests[ndx].name,
													 s_es310Tests[ndx].description,
													 std::string("vulkan/glsl/es310/") + s_es310Tests[ndx].name + ".test").release());

	static const struct
	{
		const char*		name;
		const char*		description;
	} s_440Tests[] =
	{
		{ "linkage",					"Linking"					},
	};

	de::MovePtr<tcu::TestCaseGroup> glsl440Tests = de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "440", ""));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_440Tests); ndx++)
		glsl440Tests->addChild(createShaderLibraryGroup(testCtx,
													 s_440Tests[ndx].name,
													 s_440Tests[ndx].description,
													 std::string("vulkan/glsl/440/") + s_440Tests[ndx].name + ".test").release());

	glslTests->addChild(glsl440Tests.release());

	// ShaderRenderCase-based tests
	glslTests->addChild(sr::createDerivateTests			(testCtx));
	glslTests->addChild(sr::createDiscardTests			(testCtx));
	glslTests->addChild(sr::createDemoteTests			(testCtx));
	glslTests->addChild(sr::createIndexingTests			(testCtx));
	glslTests->addChild(sr::createShaderInvarianceTests	(testCtx));
	glslTests->addChild(sr::createLimitTests			(testCtx));
	glslTests->addChild(sr::createLoopTests				(testCtx));
	glslTests->addChild(sr::createMatrixTests			(testCtx));
	glslTests->addChild(sr::createOperatorTests			(testCtx));
	glslTests->addChild(sr::createReturnTests			(testCtx));
	glslTests->addChild(sr::createStructTests			(testCtx));
	glslTests->addChild(sr::createSwitchTests			(testCtx));
	glslTests->addChild(sr::createTextureFunctionTests	(testCtx));
	glslTests->addChild(sr::createTextureGatherTests	(testCtx));
	glslTests->addChild(sr::createBuiltinVarTests		(testCtx));

	// ShaderExecutor-based tests
	glslTests->addChild(shaderexecutor::createBuiltinTests				(testCtx));
	glslTests->addChild(shaderexecutor::createOpaqueTypeIndexingTests	(testCtx));
	glslTests->addChild(shaderexecutor::createAtomicOperationTests		(testCtx));
	glslTests->addChild(shaderexecutor::createShaderClockTests			(testCtx));

	// Amber GLSL tests.
	glslTests->addChild(cts_amber::createCombinedOperationsGroup		(testCtx));
}

// TestPackage

BaseTestPackage::BaseTestPackage (tcu::TestContext& testCtx, const char* name, const char* desc)
	: tcu::TestPackage(testCtx, name, desc)
{
}

BaseTestPackage::~BaseTestPackage (void)
{
}

TestPackage::TestPackage (tcu::TestContext& testCtx)
	: BaseTestPackage(testCtx, "dEQP-VK", "dEQP Vulkan Tests")
{
}

TestPackage::~TestPackage (void)
{
}

ExperimentalTestPackage::ExperimentalTestPackage (tcu::TestContext& testCtx)
	: BaseTestPackage(testCtx, "dEQP-VK-experimental", "dEQP Vulkan Experimental Tests")
{
}

ExperimentalTestPackage::~ExperimentalTestPackage (void)
{
}

tcu::TestCaseExecutor* BaseTestPackage::createExecutor (void) const
{
	return new TestCaseExecutor(m_testCtx);
}

void TestPackage::init (void)
{
	addChild(createTestGroup					(m_testCtx, "info", "Build and Device Info Tests", createInfoTests));
	addChild(api::createTests					(m_testCtx));
	addChild(memory::createTests				(m_testCtx));
	addChild(pipeline::createTests				(m_testCtx));
	addChild(BindingModel::createTests			(m_testCtx));
	addChild(SpirVAssembly::createTests			(m_testCtx));
	addChild(createTestGroup					(m_testCtx, "glsl", "GLSL shader execution tests", createGlslTests));
	addChild(createRenderPassTests				(m_testCtx));
	addChild(createRenderPass2Tests				(m_testCtx));
	addChild(ubo::createTests					(m_testCtx));
	addChild(DynamicState::createTests			(m_testCtx));
	addChild(ssbo::createTests					(m_testCtx));
	addChild(QueryPool::createTests				(m_testCtx));
	addChild(Draw::createTests					(m_testCtx));
	addChild(compute::createTests				(m_testCtx));
	addChild(image::createTests					(m_testCtx));
	addChild(wsi::createTests					(m_testCtx));
	addChild(createSynchronizationTests			(m_testCtx));
	addChild(createSynchronization2Tests		(m_testCtx));
	addChild(sparse::createTests				(m_testCtx));
	addChild(tessellation::createTests			(m_testCtx));
	addChild(rasterization::createTests			(m_testCtx));
	addChild(clipping::createTests				(m_testCtx));
	addChild(FragmentOperations::createTests	(m_testCtx));
	addChild(texture::createTests				(m_testCtx));
	addChild(geometry::createTests				(m_testCtx));
	addChild(robustness::createTests			(m_testCtx));
	addChild(MultiView::createTests				(m_testCtx));
	addChild(subgroups::createTests				(m_testCtx));
	addChild(ycbcr::createTests					(m_testCtx));
	addChild(ProtectedMem::createTests			(m_testCtx));
	addChild(DeviceGroup::createTests			(m_testCtx));
	addChild(MemoryModel::createTests			(m_testCtx));
	addChild(conditional::createTests			(m_testCtx));
	addChild(cts_amber::createGraphicsFuzzTests	(m_testCtx));
	addChild(imageless::createTests				(m_testCtx));
	addChild(TransformFeedback::createTests		(m_testCtx));
	addChild(DescriptorIndexing::createTests	(m_testCtx));
	addChild(FragmentShaderInterlock::createTests(m_testCtx));
	addChild(modifiers::createTests				(m_testCtx));
	addChild(RayTracing::createTests			(m_testCtx));
	addChild(RayQuery::createTests				(m_testCtx));
	addChild(FragmentShadingRate::createTests	(m_testCtx));
}

void ExperimentalTestPackage::init (void)
{
	addChild(postmortem::createTests			(m_testCtx));
}

} // vkt
