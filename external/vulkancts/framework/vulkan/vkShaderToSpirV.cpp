/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 * \brief Shading language (GLSL/HLSL) to SPIR-V.
 *//*--------------------------------------------------------------------*/

#include "vkShaderToSpirV.hpp"
#include "deArrayUtil.hpp"
#include "deSingleton.h"
#include "deMemory.h"
#include "deClock.h"
#include "qpDebugOut.h"

#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/disassemble.h"
#include "SPIRV/SPVRemapper.h"
#include "SPIRV/doc.h"
#include "glslang/Include/InfoSink.h"
#include "glslang/Include/ShHandle.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/Public/ShaderLang.h"

 //slang input files
 //#include "D:/githubnv//VK-GL-CTS/external/slang/slang.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <iostream>
#include <fstream>

namespace vk
{

	using std::string;
	using std::vector;

	namespace
	{

		EShLanguage getGlslangStage(glu::ShaderType type)
		{
			static const EShLanguage stageMap[] =
			{
				EShLangVertex,
				EShLangFragment,
				EShLangGeometry,
				EShLangTessControl,
				EShLangTessEvaluation,
				EShLangCompute,
				EShLangRayGen,
				EShLangAnyHit,
				EShLangClosestHit,
				EShLangMiss,
				EShLangIntersect,
				EShLangCallable,
				EShLangTaskNV,
				EShLangMeshNV,
			};
			return de::getSizedArrayElement<glu::SHADERTYPE_LAST>(stageMap, type);
		}

		static volatile deSingletonState	s_glslangInitState = DE_SINGLETON_STATE_NOT_INITIALIZED;

		void initGlslang(void*)
		{
			// Main compiler
			glslang::InitializeProcess();

			// SPIR-V disassembly
			spv::Parameterize();
		}

		void prepareGlslang(void)
		{
			deInitSingleton(&s_glslangInitState, initGlslang, DE_NULL);
		}

		// \todo [2015-06-19 pyry] Specialize these per GLSL version

		// Fail compilation if more members are added to TLimits or TBuiltInResource
		struct LimitsSizeHelper_s { bool m0, m1, m2, m3, m4, m5, m6, m7, m8; };
		struct BuiltInResourceSizeHelper_s { int m[102]; LimitsSizeHelper_s l; };

		DE_STATIC_ASSERT(sizeof(TLimits) == sizeof(LimitsSizeHelper_s));
		DE_STATIC_ASSERT(sizeof(TBuiltInResource) == sizeof(BuiltInResourceSizeHelper_s));

		void getDefaultLimits(TLimits* limits)
		{
			limits->nonInductiveForLoops = true;
			limits->whileLoops = true;
			limits->doWhileLoops = true;
			limits->generalUniformIndexing = true;
			limits->generalAttributeMatrixVectorIndexing = true;
			limits->generalVaryingIndexing = true;
			limits->generalSamplerIndexing = true;
			limits->generalVariableIndexing = true;
			limits->generalConstantMatrixVectorIndexing = true;
		}

		void getDefaultBuiltInResources(TBuiltInResource* builtin)
		{
			getDefaultLimits(&builtin->limits);

			builtin->maxLights = 32;
			builtin->maxClipPlanes = 6;
			builtin->maxTextureUnits = 32;
			builtin->maxTextureCoords = 32;
			builtin->maxVertexAttribs = 64;
			builtin->maxVertexUniformComponents = 4096;
			builtin->maxVaryingFloats = 64;
			builtin->maxVertexTextureImageUnits = 32;
			builtin->maxCombinedTextureImageUnits = 80;
			builtin->maxTextureImageUnits = 32;
			builtin->maxFragmentUniformComponents = 4096;
			builtin->maxDrawBuffers = 32;
			builtin->maxVertexUniformVectors = 128;
			builtin->maxVaryingVectors = 8;
			builtin->maxFragmentUniformVectors = 16;
			builtin->maxVertexOutputVectors = 16;
			builtin->maxFragmentInputVectors = 15;
			builtin->minProgramTexelOffset = -8;
			builtin->maxProgramTexelOffset = 7;
			builtin->maxClipDistances = 8;
			builtin->maxComputeWorkGroupCountX = 65535;
			builtin->maxComputeWorkGroupCountY = 65535;
			builtin->maxComputeWorkGroupCountZ = 65535;
			builtin->maxComputeWorkGroupSizeX = 1024;
			builtin->maxComputeWorkGroupSizeY = 1024;
			builtin->maxComputeWorkGroupSizeZ = 64;
			builtin->maxComputeUniformComponents = 1024;
			builtin->maxComputeTextureImageUnits = 16;
			builtin->maxComputeImageUniforms = 8;
			builtin->maxComputeAtomicCounters = 8;
			builtin->maxComputeAtomicCounterBuffers = 1;
			builtin->maxVaryingComponents = 60;
			builtin->maxVertexOutputComponents = 64;
			builtin->maxGeometryInputComponents = 64;
			builtin->maxGeometryOutputComponents = 128;
			builtin->maxFragmentInputComponents = 128;
			builtin->maxImageUnits = 8;
			builtin->maxCombinedImageUnitsAndFragmentOutputs = 8;
			builtin->maxCombinedShaderOutputResources = 8;
			builtin->maxImageSamples = 0;
			builtin->maxVertexImageUniforms = 0;
			builtin->maxTessControlImageUniforms = 0;
			builtin->maxTessEvaluationImageUniforms = 0;
			builtin->maxGeometryImageUniforms = 0;
			builtin->maxFragmentImageUniforms = 8;
			builtin->maxCombinedImageUniforms = 8;
			builtin->maxGeometryTextureImageUnits = 16;
			builtin->maxGeometryOutputVertices = 256;
			builtin->maxGeometryTotalOutputComponents = 1024;
			builtin->maxGeometryUniformComponents = 1024;
			builtin->maxGeometryVaryingComponents = 64;
			builtin->maxTessControlInputComponents = 128;
			builtin->maxTessControlOutputComponents = 128;
			builtin->maxTessControlTextureImageUnits = 16;
			builtin->maxTessControlUniformComponents = 1024;
			builtin->maxTessControlTotalOutputComponents = 4096;
			builtin->maxTessEvaluationInputComponents = 128;
			builtin->maxTessEvaluationOutputComponents = 128;
			builtin->maxTessEvaluationTextureImageUnits = 16;
			builtin->maxTessEvaluationUniformComponents = 1024;
			builtin->maxTessPatchComponents = 120;
			builtin->maxPatchVertices = 32;
			builtin->maxTessGenLevel = 64;
			builtin->maxViewports = 16;
			builtin->maxVertexAtomicCounters = 0;
			builtin->maxTessControlAtomicCounters = 0;
			builtin->maxTessEvaluationAtomicCounters = 0;
			builtin->maxGeometryAtomicCounters = 0;
			builtin->maxFragmentAtomicCounters = 8;
			builtin->maxCombinedAtomicCounters = 8;
			builtin->maxAtomicCounterBindings = 1;
			builtin->maxVertexAtomicCounterBuffers = 0;
			builtin->maxTessControlAtomicCounterBuffers = 0;
			builtin->maxTessEvaluationAtomicCounterBuffers = 0;
			builtin->maxGeometryAtomicCounterBuffers = 0;
			builtin->maxFragmentAtomicCounterBuffers = 1;
			builtin->maxCombinedAtomicCounterBuffers = 1;
			builtin->maxAtomicCounterBufferSize = 16384;
			builtin->maxTransformFeedbackBuffers = 8;
			builtin->maxTransformFeedbackInterleavedComponents = 16382;
			builtin->maxCullDistances = 8;
			builtin->maxCombinedClipAndCullDistances = 8;
			builtin->maxSamples = 4;
			builtin->maxMeshOutputVerticesNV = 2048;
			builtin->maxMeshOutputPrimitivesNV = 2048;
			builtin->maxMeshWorkGroupSizeX_NV = 256;
			builtin->maxMeshWorkGroupSizeY_NV = 1;
			builtin->maxMeshWorkGroupSizeZ_NV = 1;
			builtin->maxTaskWorkGroupSizeX_NV = 1024;
			builtin->maxTaskWorkGroupSizeY_NV = 1;
			builtin->maxTaskWorkGroupSizeZ_NV = 1;
			builtin->maxMeshViewCountNV = 4;
			builtin->maxMeshOutputVerticesEXT = 2048;
			builtin->maxMeshOutputPrimitivesEXT = 2048;
			builtin->maxMeshWorkGroupSizeX_EXT = 256;
			builtin->maxMeshWorkGroupSizeY_EXT = 256;
			builtin->maxMeshWorkGroupSizeZ_EXT = 256;
			builtin->maxTaskWorkGroupSizeX_EXT = 256;
			builtin->maxTaskWorkGroupSizeY_EXT = 256;
			builtin->maxTaskWorkGroupSizeZ_EXT = 256;
			builtin->maxMeshViewCountEXT = 4;
			builtin->maxDualSourceDrawBuffersEXT = 1;
		};

		int getNumShaderStages(const std::vector<std::string>* sources)
		{
			int numShaderStages = 0;

			for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; ++shaderType)
			{
				if (!sources[shaderType].empty())
					numShaderStages += 1;
			}

			return numShaderStages;
		}

		std::string getShaderStageSource(const std::vector<std::string>* sources, const ShaderBuildOptions buildOptions, glu::ShaderType shaderType)
		{
			if (sources[shaderType].size() != 1)
				TCU_THROW(InternalError, "Linking multiple compilation units is not supported");

			if ((buildOptions.flags & ShaderBuildOptions::FLAG_USE_STORAGE_BUFFER_STORAGE_CLASS) != 0)
			{
				// Hack to inject #pragma right after first #version statement
				std::string src = sources[shaderType][0];
				size_t		injectPos = 0;

				if (de::beginsWith(src, "#version"))
					injectPos = src.find('\n') + 1;

				src.insert(injectPos, "#pragma use_storage_buffer\n");

				return src;
			}
			else
				return sources[shaderType][0];
		}

		EShMessages getCompileFlags(const ShaderBuildOptions& buildOpts, const ShaderLanguage shaderLanguage)
		{
			EShMessages		flags = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

			if ((buildOpts.flags & ShaderBuildOptions::FLAG_ALLOW_RELAXED_OFFSETS) != 0)
				flags = (EShMessages)(flags | EShMsgHlslOffsets);

			if (shaderLanguage == SHADER_LANGUAGE_HLSL)
				flags = (EShMessages)(flags | EShMsgReadHlsl);

			return flags;
		}

	} // anonymous
#ifdef _WIN32
#include <windows.h>
	using namespace Slang;
	class SlangBlob : public ISlangBlob {
	public:
		SlangBlob(std::string in) { inputString = in; }
		SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() { return inputString.c_str(); }
		SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() { return inputString.size(); }
		// ISlangUnknown
		SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& guid, void** outObject)
		{
			return SLANG_OK;
		};
		SLANG_NO_THROW uint32_t SLANG_MCALL addRef() { return 1; }
		SLANG_NO_THROW uint32_t SLANG_MCALL release() { return 1; }
	protected:
		std::string inputString;
	};
	typedef void (CALLBACK* PFNSPSETDIAGONSTICCB)(SlangCompileRequest*, SlangDiagnosticCallback, void const*);
	typedef void (CALLBACK* PFNSPSETCOMMANDLINECOMPILERMODE)(SlangCompileRequest*);
	typedef SlangResult(CALLBACK* PFNSPPROCESSCOMMANDLINEARG)(SlangCompileRequest*, char const*, int);
	typedef SlangResult(CALLBACK* PFNSPCOMPILE)(SlangCompileRequest* request);
	typedef SlangResult(CALLBACK* PFNCREATEGLOBALSESSION)(SlangInt, slang::IGlobalSession**);

	typedef struct _slangLibFuncs {
		PFNSPSETDIAGONSTICCB pfnspSetDiagnosticCallback = nullptr;
		PFNSPPROCESSCOMMANDLINEARG pfnspProcessCommandLineArguments = nullptr;
		PFNSPCOMPILE pfnspCompile = nullptr;
		PFNCREATEGLOBALSESSION pfnslang_createGlobalSession = nullptr;
		bool isInitialized() {
			return (pfnspSetDiagnosticCallback && pfnspProcessCommandLineArguments && pfnspCompile && pfnslang_createGlobalSession);
		}
	} slangLibFuncs;

	class SlangContext {
	public:
		Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
		bool globalSessionInit = false;
		std::string slangDllPath = "D:\\githubnv\\VK-GL-CTS\\external\\slang"; //This will be transformed to a environment variable SLANG_DLL_PATH
		bool loadDLL = false;
		HMODULE handle = nullptr;
		slangLibFuncs m_sfn;

		template<typename ... TArgs> inline void reportError(const char* format, TArgs... args)
		{
			printf(format, args...);
		}

		inline void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob)
		{
			if (diagnosticsBlob != nullptr)
			{
				reportError("%s", (const char*)diagnosticsBlob->getBufferPointer());
			}
		}
		int SetupSlangDLL()
		{
			if (!handle) {
				//if (!SetDllDirectoryA(slangDllPath.c_str())) {
				//	std::cout << "failed to set slang dll PATH\n";
				//	return SLANG_FAIL;
				//}
				handle = LoadLibraryA("slang.dll");
				if (NULL == handle) {
					std::cout << "failed to load slang.dll\n";
					return SLANG_FAIL;
				}
			}
			return SLANG_OK;
		}

		void getSlangFunctionHandles() {
			m_sfn.pfnslang_createGlobalSession = (PFNCREATEGLOBALSESSION)GetProcAddress(handle, "slang_createGlobalSession");
			m_sfn.pfnspCompile = (PFNSPCOMPILE)GetProcAddress(handle, "spCompile");
			m_sfn.pfnspSetDiagnosticCallback = (PFNSPSETDIAGONSTICCB)GetProcAddress(handle, "spSetDiagnosticCallback");
			m_sfn.pfnspProcessCommandLineArguments = (PFNSPPROCESSCOMMANDLINEARG)GetProcAddress(handle, "spProcessCommandLineArguments");
			//m_sfn.pfnspSetCommandLineCompilerMode = (PFNSPSETCOMMANDLINECOMPILERMODE)GetProcAddress(handle, "spSetCommandLineCompilerMode");
		}
		static void _diagnosticCallback(
			char const* message,
			void*       /*userData*/)
		{
			printf("%s", message);
		}

		int setupSlangLikeSlangc(const std::vector<std::string>* sources, const ShaderBuildOptions& buildOptions, const ShaderLanguage shaderLanguage, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
		{
			SlangResult result = SLANG_OK;

			do {
				result = SetupSlangDLL();
				if (result != SLANG_OK) {
					std::cout << "Failed to load SLANG DLL";
					break;
				}
				getSlangFunctionHandles();
				if (!m_sfn.isInitialized()) {
					std::cout << "Failed to get function pointers";
					break;
				}
				result = m_sfn.pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
				if (result != SLANG_OK) {
					std::cout << "Failed to create global session: " << std::hex << result << "\n";
					break;
				}
				SlangCompileRequest* compileRequest = nullptr;
				result = slangGlobalSession->createCompileRequest(&compileRequest);
				if (result != SLANG_OK) {
					std::cout << "Failed to create CompileRequest: " << std::hex << result << "\n";
					break;
				}
				auto findSlangShaderStage = [](glu::ShaderType shaderType) {
					switch (shaderType) {
						case glu::SHADERTYPE_VERTEX:
							return "vertex";
						case glu::SHADERTYPE_FRAGMENT:
							return "fragment";
						case glu::SHADERTYPE_GEOMETRY:
							return "geometry";
						case glu::SHADERTYPE_COMPUTE:
							return "compute";
						default:
							std::cout << "unsupported shader stage";
							return "unknown";
					}
					return "unknown";
				};
				auto findSlangShaderExt = [](glu::ShaderType shaderType) {
					switch (shaderType) {
					case glu::SHADERTYPE_VERTEX:
						return ".vert";
					case glu::SHADERTYPE_FRAGMENT:
						return ".frag";
					case glu::SHADERTYPE_GEOMETRY:
						return ".geom";
					case glu::SHADERTYPE_COMPUTE:
						return ".comp";
					default:
						std::cout << "unsupported shader stage";
						return "";
					}
					return "";
				};

				for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++) {
					if (!sources[shaderType].empty()) {
						const std::string& srcText = getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
						glu::ShaderType shaderstage = glu::ShaderType(shaderType);
						const char* slangShaderStage = findSlangShaderStage((glu::ShaderType)shaderType);
						const char* fileExt = findSlangShaderExt((glu::ShaderType)shaderType);

						SetCurrentDirectory(slangDllPath.c_str());
						std::ofstream myfile;
						std::string temp_fname = "test.slang";
						temp_fname.append(fileExt);
						myfile.open(temp_fname);
						myfile << srcText.c_str();
						myfile.close();

						compileRequest->addSearchPath(slangDllPath.c_str());
						compileRequest->setDiagnosticCallback(&_diagnosticCallback, nullptr);
						compileRequest->setCommandLineCompilerMode();
						const char* args[] = { "-target", "spirv", "-stage", slangShaderStage, "-entry", "main", "-allow-glsl", temp_fname.c_str(), "-o", "temp.spv"};
						int argCount = sizeof(args) / sizeof(char*);//8;
						result = compileRequest->processCommandLineArguments(args, argCount);
						if (result != SLANG_OK) {
							std::cout << "Failed to proces command line arguments: " << std::hex << result << "\n";
							break;
						}
						const deUint64  compileStartTime = deGetMicroseconds();
						glu::ShaderInfo shaderBuildInfo;

						result = compileRequest->compile();
						if (result != SLANG_OK) {
							std::cout << "Failed to compile: " << std::hex << result << "\n";
							break;

						}
						shaderBuildInfo.type = (glu::ShaderType)shaderType;
						shaderBuildInfo.source = srcText;
						shaderBuildInfo.infoLog = "";//shader.getInfoLog(); // \todo [2015-07-13 pyry] Include debug log?
						shaderBuildInfo.compileTimeUs = deGetMicroseconds() - compileStartTime;
						shaderBuildInfo.compileOk = (result == SLANG_OK);
						buildInfo->shaders.push_back(shaderBuildInfo);

						const deUint64  linkStartTime = deGetMicroseconds();

						Slang::ComPtr<ISlangBlob> spirvCode;
						compileRequest->getEntryPointCodeBlob(0, 0, spirvCode.writeRef());


						//copy the SPIRV
						uint32_t* buff32 = (uint32_t*)spirvCode->getBufferPointer();
						size_t size = spirvCode->getBufferSize();
						// print the buffer
						for (int i = 0; i < size / 4; i++) {
							//printf("%x ", buff32[i]);
							dst->push_back(buff32[i]);
						}
						buildInfo->program.infoLog = "";//glslangProgram.getInfoLog(); // \todo [2015-11-05 scygan] Include debug log?
						buildInfo->program.linkOk = true;
						buildInfo->program.linkTimeUs = deGetMicroseconds() - linkStartTime;

					}
				}
			} while (false);
			return result;
		}

		// SLANG ISession Interface to generate SIPRV 
		int setupSlang(const std::vector<std::string>* sources, const ShaderBuildOptions& buildOptions, const ShaderLanguage shaderLanguage, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
		{
			SlangResult result = SLANG_OK;

			if (!globalSessionInit) {
				// load DLL
				if (!SetDllDirectoryA(slangDllPath.c_str())) {
					std::cout << "failed to set slang dll PATH\n";
					return SLANG_FAIL;
				}
				HMODULE handle = LoadLibraryA("slang.dll");
				if (NULL == handle) {
					std::cout << "failed to load slang.dll\n";
					return SLANG_FAIL;
				}
				typedef UINT(CALLBACK* LPFNDLLFUNC1)(SlangInt, slang::IGlobalSession**);
				LPFNDLLFUNC1 pfnslang_createGlobalSessionWithoutStdLib = (LPFNDLLFUNC1)GetProcAddress(handle,"slang_createGlobalSessionWithoutStdLib");
				if (!pfnslang_createGlobalSessionWithoutStdLib) {
					// handle the error
					std::cout << "failed to get create global session method\n";
					FreeLibrary(handle);
					return SLANG_FAIL;
				}
				;
				LPFNDLLFUNC1 pfnslang_createGlobalSession = (LPFNDLLFUNC1)GetProcAddress(handle, "slang_createGlobalSession");
				if (!pfnslang_createGlobalSession) {
					// handle the error
					std::cout << "failed to get create global session method\n";
					FreeLibrary(handle);
					return SLANG_FAIL;
				}

				//Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
				//result = slang::createGlobalSession(slangGlobalSession.writeRef());
				//result = slang_createGlobalSessionWithoutStdLib(SLANG_API_VERSION, slangGlobalSession.writeRef());
				result = pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
				if (result != SLANG_OK) {
					std::cout << "Failed to create global session: " << std::hex << result << "\n";
					return result;
				}
				globalSessionInit = true;
			}
			// Next we create a compilation session to generate SPIRV code from Slang source.
			slang::SessionDesc sessionDesc = {};
			slang::TargetDesc targetDesc = {};
			targetDesc.format = SLANG_SPIRV;
			targetDesc.profile = slangGlobalSession->findProfile("glsl440");
			targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

			sessionDesc.targets = &targetDesc;
			sessionDesc.targetCount = 1;

			for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
			{
			    if (!sources[shaderType].empty()) {
    				const std::string& srcText = getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
    				glu::ShaderType shaderstage = glu::ShaderType(shaderType);

    				Slang::ComPtr<slang::ISession> session;
    				result = slangGlobalSession->createSession(sessionDesc, session.writeRef());
    				if (result != SLANG_OK) {
    					std::cout << "Failed to create local session: " << std::hex << result << "\n";
    					//return SLANG_FAIL;
    					break;
    				}
    				//session->setAllowGLSLInput(true);
    				slang::IModule* slangModule = nullptr;
    				{
						//write the file onto the disk temporarily
						//SetCurrentDirectory(slangDllPath);
						std::ofstream myfile;
						myfile.open("test.slang");
						myfile << srcText.c_str();
						myfile.close();

						char* path = ".\\";
    					Slang::ComPtr<slang::IBlob> diagnosticBlob;
    					SlangBlob blobSource = SlangBlob(srcText);
    					//slangModule = session->loadModuleFromSource("cts-test", path, &blobSource, diagnosticBlob.writeRef());
						slangModule = session->loadModule("test", diagnosticBlob.writeRef());
						if (!slangModule) {
							std::cout << "Failed to load the module\n";
							diagnoseIfNeeded(diagnosticBlob);
							result = SLANG_FAIL;
						}
    				}
					// ERROR: loadModule fails to find the entry point as it is looking for "[shader("compute")]" to identify the entry point
					// Hence switching to slangc mechanism for the same
                    const deUint64  compileStartTime    = deGetMicroseconds();
                    glu::ShaderInfo shaderBuildInfo;
					if (result == SLANG_FAIL) {
						shaderBuildInfo.type = (glu::ShaderType)shaderType;
						shaderBuildInfo.source = srcText;
						shaderBuildInfo.infoLog = "";//shader.getInfoLog(); // \todo [2015-07-13 pyry] Include debug log?
						shaderBuildInfo.compileOk = false;
						return SLANG_FAIL;
					}

            		ComPtr<slang::IEntryPoint> entryPoint;
            		result = slangModule->findEntryPointByName("main", entryPoint.writeRef());
            		if (result != SLANG_OK) {
            			std::cout << "Failed to find the entry point: " << std::hex << result << "\n";
            			//return SLANG_FAIL;
            			//break;
            		}
            		slang::IComponentType* componentTypes[] = { slangModule , entryPoint };
            		ComPtr<slang::IComponentType> composedProgram;
            		if(result == SLANG_OK) {
            			ComPtr<slang::IBlob> diagnosticsBlob;
            			SlangResult result = session->createCompositeComponentType(
            				&componentTypes[0],
            				2,
            				composedProgram.writeRef(),
            				diagnosticsBlob.writeRef());
            			if (result != SLANG_OK) {
            				std::cout << "Failed to create composite component type: " << std::hex << result << "\n";
            				diagnoseIfNeeded(diagnosticsBlob);
            				//return SLANG_FAIL;
            				//break;
            			}
            		}
                    shaderBuildInfo.type            = (glu::ShaderType)shaderType;
                    shaderBuildInfo.source          = srcText;
                    shaderBuildInfo.infoLog         = "";//shader.getInfoLog(); // \todo [2015-07-13 pyry] Include debug log?
                    shaderBuildInfo.compileTimeUs   = deGetMicroseconds()-compileStartTime;
                    shaderBuildInfo.compileOk       = (result == SLANG_OK);
                    buildInfo->shaders.push_back(shaderBuildInfo);
                    
                    if (buildInfo->shaders[0].compileOk) {
                        const deUint64  linkStartTime   = deGetMicroseconds();
        				// Link it against all depdendencies
        				ComPtr<slang::IComponentType> linkedProgram;
        				{
        					ComPtr<slang::IBlob> diagnosticsBlob;
        					result = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
        					if (result != SLANG_OK) {
        						std::cout << "Failed to link: " << std::hex << result << "\n";
        						diagnoseIfNeeded(diagnosticsBlob);
        					}
        				}
                        
                        buildInfo->program.infoLog      = "";//glslangProgram.getInfoLog(); // \todo [2015-11-05 scygan] Include debug log?
                        buildInfo->program.linkOk       = (result != 0);
                        buildInfo->program.linkTimeUs   = deGetMicroseconds()-linkStartTime;
                    }
    				// Now we can call `composedProgram->getEntryPointCode()` to retrieve the
                    if (buildInfo->program.linkOk) {
        				// compiled SPIRV code that we will use to create a vulkan compute pipeline.
        				// This will trigger the final Slang compilation and spirv code generation.
        				ComPtr<slang::IBlob> spirvCode;
        				{
        					ComPtr<slang::IBlob> diagnosticsBlob;
        					result = composedProgram->getEntryPointCode(
        						0, 0, spirvCode.writeRef(), diagnosticsBlob.writeRef());
        					if (result != SLANG_OK) {
        						std::cout << "Failed to generate SPIRV Code: " << std::hex << result << "\n";
        						diagnoseIfNeeded(diagnosticsBlob);
        						return SLANG_FAIL;
        					}
        				}

						//copy the SPIRV
						uint8_t *buff = (uint8_t *)spirvCode->getBufferPointer();
                        for(deUint32 i=0; i < spirvCode->getBufferSize(); i=i+4) {
							char c[4];
							//int d = *(int *)(buff + i);
							memcpy(c, (buff + i), sizeof(4));
							int d = std::stoi(c, 0, 10);
							//for (deUint32 j = 0; j < 4; j++) {
							//	c[j] = std::stoi( buff[i * 4 + j];

							//}
							//deUint32 data = std::stoi(buff[i], 0, 10);
							dst->push_back(d);

                        }
                    }
                    return result;
                }

			}
			TCU_THROW(InternalError, "Can't compile empty program");
            return SLANG_FAIL;
		}
	};
	static SlangContext g_slangContext;
#endif
	bool compileShaderToSpirV(const std::vector<std::string>* sources, const ShaderBuildOptions& buildOptions, const ShaderLanguage shaderLanguage, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
	{
		TBuiltInResource	builtinRes = {};
		const EShMessages	compileFlags = getCompileFlags(buildOptions, shaderLanguage);

		if (buildOptions.targetVersion >= SPIRV_VERSION_LAST)
			TCU_THROW(InternalError, "Unsupported SPIR-V target version");

		if (getNumShaderStages(sources) > 1)
			TCU_THROW(InternalError, "Linking multiple shader stages into a single SPIR-V binary is not supported");

		prepareGlslang();
		getDefaultBuiltInResources(&builtinRes);
#ifdef _WIN32
		bool enableSlang = true;
		char lpBuffer[128];
		DWORD ret = GetEnvironmentVariable("DISABLE_CTS_SLANG", lpBuffer, 128);
		if (ret > 0 && strcmp(lpBuffer, "1") == 0) {
			enableSlang = false;
			//std::cout << "Disabled SLANG\n";
		}
		if (enableSlang) {
			//int result = g_slangContext.setupSlang(sources, buildOptions, shaderLanguage, dst, buildInfo);
			g_slangContext.setupSlangLikeSlangc(sources, buildOptions, shaderLanguage, dst, buildInfo);
			return buildInfo->program.linkOk;
		}

#endif
	// \note Compiles only first found shader
	for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
	{
		if (!sources[shaderType].empty())
		{
			const std::string&		srcText				= getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
			const char*				srcPtrs[]			= { srcText.c_str() };
			const EShLanguage		shaderStage			= getGlslangStage(glu::ShaderType(shaderType));
			glslang::TShader		shader				(shaderStage);
			glslang::TProgram		glslangProgram;

			shader.setStrings(srcPtrs, DE_LENGTH_OF_ARRAY(srcPtrs));

			switch ( buildOptions.targetVersion )
			{
			case SPIRV_VERSION_1_0:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10000);
				break;
			case SPIRV_VERSION_1_1:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10100);
				break;
			case SPIRV_VERSION_1_2:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10200);
				break;
			case SPIRV_VERSION_1_3:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10300);
				break;
			case SPIRV_VERSION_1_4:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10400);
				break;
			case SPIRV_VERSION_1_5:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10500);
				break;
			case SPIRV_VERSION_1_6:
				shader.setEnvTarget(glslang::EshTargetSpv, (glslang::EShTargetLanguageVersion)0x10600);
				break;
			default:
				TCU_THROW(InternalError, "Unsupported SPIR-V target version");
			}

			glslangProgram.addShader(&shader);

			if (shaderLanguage == SHADER_LANGUAGE_HLSL)
			{
				// Entry point assumed to be named main.
				shader.setEntryPoint("main");
			}

			{
				const deUint64	compileStartTime	= deGetMicroseconds();
				const int		compileRes			= shader.parse(&builtinRes, 110, false, compileFlags);
				glu::ShaderInfo	shaderBuildInfo;

				shaderBuildInfo.type			= (glu::ShaderType)shaderType;
				shaderBuildInfo.source			= srcText;
				shaderBuildInfo.infoLog			= shader.getInfoLog(); // \todo [2015-07-13 pyry] Include debug log?
				shaderBuildInfo.compileTimeUs	= deGetMicroseconds()-compileStartTime;
				shaderBuildInfo.compileOk		= (compileRes != 0);

				buildInfo->shaders.push_back(shaderBuildInfo);
			}

			DE_ASSERT(buildInfo->shaders.size() == 1);
			if (buildInfo->shaders[0].compileOk)
			{
				const deUint64	linkStartTime	= deGetMicroseconds();
				const int		linkRes			= glslangProgram.link(compileFlags);

				buildInfo->program.infoLog		= glslangProgram.getInfoLog(); // \todo [2015-11-05 scygan] Include debug log?
				buildInfo->program.linkOk		= (linkRes != 0);
				buildInfo->program.linkTimeUs	= deGetMicroseconds()-linkStartTime;
			}

			if (buildInfo->program.linkOk)
			{
				const glslang::TIntermediate* const	intermediate	= glslangProgram.getIntermediate(shaderStage);
				glslang::GlslangToSpv(*intermediate, *dst);
			}
			bool printdst = false;
			if (printdst) {
				for (int i=0; i < dst->size(); i++) {
					std::cout << std::hex << (*dst)[i] << " ";
				}
			}
			return buildInfo->program.linkOk;
		}
	}

	TCU_THROW(InternalError, "Can't compile empty program");
}

bool compileGlslToSpirV (const GlslSource& program, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
{
	return compileShaderToSpirV(program.sources, program.buildOptions, program.shaderLanguage, dst, buildInfo);
}

bool compileHlslToSpirV (const HlslSource& program, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
{
	return compileShaderToSpirV(program.sources, program.buildOptions, program.shaderLanguage, dst, buildInfo);
}

void stripSpirVDebugInfo (const size_t numSrcInstrs, const deUint32* srcInstrs, std::vector<deUint32>* dst)
{
	spv::spirvbin_t remapper;
	std::vector<std::string> whiteListStrings;

	// glslang operates in-place
	dst->resize(numSrcInstrs);
	std::copy(srcInstrs, srcInstrs+numSrcInstrs, dst->begin());
	remapper.remap(*dst, whiteListStrings, spv::spirvbin_base_t::STRIP);
}

} // vk
