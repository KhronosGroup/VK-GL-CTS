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

#define ENABLE_SLANG_COMPILATION 1
#if defined(ENABLE_SLANG_COMPILATION) && defined(_WIN32)
#define ENABLE_SLANG_LOGS 1

#ifdef ENABLE_SLANG_LOGS
#define SLANG_LOG(x) x
#else
#define SLANG_LOG(x) 
#endif
 //slang input files
#include <slang.h>
#include <slang-com-ptr.h>
#include <iostream>
#include <fstream>
#include <list>
#include <source/core/slang-list.h>
#include <tools/slang-test/test-context.h>
#include "vkSpirVAsm.hpp"
#endif
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
#if defined(ENABLE_SLANG_COMPILATION) && defined(_WIN32)
#include <windows.h>

#ifndef SLANG_RETURN_FAIL_ON_FALSE
#   define SLANG_RETURN_FAIL_ON_FALSE(x) if (!(x)) return SLANG_FAIL;
#endif

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


	class WinHandle
	{
	public:
		/// Detach the encapsulated handle. Returns the handle (which now must be externally handled) 
		HANDLE detach() { HANDLE handle = m_handle; m_handle = nullptr; return handle; }

		/// Return as a handle
		operator HANDLE() const { return m_handle; }

		/// Assign
		void operator=(HANDLE handle) { setNull(); m_handle = handle; }
		void operator=(WinHandle&& rhs) { HANDLE handle = m_handle; m_handle = rhs.m_handle; rhs.m_handle = handle; }

		/// Get ready for writing 
		SLANG_FORCE_INLINE HANDLE* writeRef() { setNull(); return &m_handle; }
		/// Get for read access
		SLANG_FORCE_INLINE const HANDLE* readRef() const { return &m_handle; }

		void setNull()
		{
			if (m_handle)
			{
				CloseHandle(m_handle);
				m_handle = nullptr;
			}
		}
		bool isNull() const { return m_handle == nullptr; }

		/// Ctor
		WinHandle(HANDLE handle = nullptr) :m_handle(handle) {}
		WinHandle(WinHandle&& rhs) :m_handle(rhs.m_handle) { rhs.m_handle = nullptr; }

		/// Dtor
		~WinHandle() { setNull(); }

	private:

		WinHandle(const WinHandle&) = delete;
		void operator=(const WinHandle& rhs) = delete;

		HANDLE m_handle;
	};

    class WinPipeStream 
    {
    public:
        typedef WinPipeStream ThisType;
    
        // Stream
        SlangResult read(void* buffer, size_t length, size_t& outReadBytes)
        {
			outReadBytes = 0;
            if (!_has(FileAccess::Read))
            {
                return SLANG_E_NOT_AVAILABLE;
            }
            
            if (m_streamHandle.isNull())
            {
                return SLANG_OK;
            }
            
            DWORD bytesRead = 0;
            
            // Check if there is any data, so won't block
            if (m_isPipe)
            {
                DWORD pipeBytesRead = 0;
                DWORD pipeTotalBytesAvailable = 0;
                DWORD pipeRemainingBytes = 0;
            
                // Works on anonymous pipes too
                // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe
            
                SLANG_RETURN_ON_FAIL(_updateState(PeekNamedPipe(m_streamHandle, nullptr, DWORD(0), &pipeBytesRead, &pipeTotalBytesAvailable, &pipeRemainingBytes)));
                // If there is nothing to read we are done
                // If we don't do this ReadFile will *block* if there is nothing available
                if (pipeTotalBytesAvailable == 0)
                {
                    return SLANG_OK;
                }
            
                SLANG_RETURN_ON_FAIL(_updateState(ReadFile(m_streamHandle, buffer, DWORD(length), &bytesRead, nullptr)));
            }
            else
            {
                SLANG_RETURN_ON_FAIL(_updateState(ReadFile(m_streamHandle, buffer, DWORD(length), &bytesRead, nullptr)));
            
                // If it's not a pipe, and there is nothing left, then we are done.
                if (length > 0 && bytesRead == 0)
                {
                    close();
                }
            }
            
            outReadBytes = size_t(bytesRead);
            return SLANG_OK;
        }

		SlangResult write(const void* buffer, size_t length)
		{

			if (!_has(FileAccess::Write))
            {
                return SLANG_E_NOT_AVAILABLE;
            }
            
            if (m_streamHandle.isNull())
            {
                // Writing to closed stream
                return SLANG_FAIL;
            }
            
            DWORD numWritten = 0;
            BOOL writeResult = WriteFile(m_streamHandle, buffer, DWORD(length), &numWritten, nullptr);
            
            if (!writeResult)
            {
                auto err = GetLastError();
            
                if (err == ERROR_BROKEN_PIPE)
                {
                    close();
                    return SLANG_FAIL;
                }
                    
                SLANG_UNUSED(err);
                return SLANG_FAIL;
            }
            
            if (numWritten != length)
            {
                return SLANG_FAIL;
            }
            return SLANG_OK;
        }

        bool isEnd() { return m_streamHandle.isNull(); }
        bool canRead() { return _has(FileAccess::Read) && !m_streamHandle.isNull(); }
        bool canWrite() { return _has(FileAccess::Write) && !m_streamHandle.isNull(); }
		void close()
		{
			if (!m_isOwned)
			{
				// If we don't own it just detach it 
				m_streamHandle.detach();
			}
			m_streamHandle.setNull();
		}
		SlangResult flush()
		{
			if ((Index(m_access) & Index(FileAccess::Write)) == 0 || m_streamHandle.isNull())
			{
				return SLANG_E_NOT_AVAILABLE;
			}

			// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
			if (!FlushFileBuffers(m_streamHandle))
			{
				auto err = GetLastError();
				SLANG_UNUSED(err);
			}
			return SLANG_OK;
		}
        WinPipeStream(HANDLE handle, FileAccess access, bool isOwned = true)
        {
			m_streamHandle = handle;
			m_access = access;
			m_isOwned = isOwned;

			// On Win32 a HANDLE has to be handled differently if it's a PIPE or FILE, so first determine
            // if it really is a pipe.
            // http://msdn.microsoft.com/en-us/library/aa364960(VS.85).aspx
            m_isPipe = GetFileType(handle) == FILE_TYPE_PIPE;
            
            if (m_isPipe)
            {
                // It might be handy to get information about the handle
                // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo
            
                DWORD flags, outBufferSize, inBufferSize, maxInstances;
                // It appears that by default windows pipe buffer size is 4k.
                if (GetNamedPipeInfo(handle, &flags, &outBufferSize, &inBufferSize, &maxInstances))
                {
                }
            }
        }
        ~WinPipeStream() { close(); }

    protected:
    
        bool _has(FileAccess access) const { return (Index(access) & Index(m_access)) != 0; }
    
        SlangResult _updateState(BOOL res)
        {
            if (res)
            {
                return SLANG_OK;
            }
            else
            {
                const auto err = GetLastError();
            
                if (err == ERROR_BROKEN_PIPE)
                {
                    m_streamHandle.setNull();
                    return SLANG_OK;
                }
            
                SLANG_UNUSED(err);
                return SLANG_FAIL;
            }
        }
    
        FileAccess m_access = FileAccess::None;
        WinHandle m_streamHandle;
        bool m_isOwned;
        bool m_isPipe;
    };

	class WinProcess 
	{
	public:

		// Process
		bool isTerminated()
		{
			return waitForTermination(0);
		}
		bool waitForTermination(int timeInMs)
		{
			if (m_processHandle.isNull())
			{
				return true;
			}

			const DWORD timeOutTime = (timeInMs < 0) ? INFINITE : DWORD(timeInMs);
			SLANG_LOG(std::cout << "#1 waitForTermination: start terminating process" << m_processHandle << "\n";)
			// wait for the process to exit
			// TODO: set a timeout as a safety measure...
			auto res = WaitForSingleObject(m_processHandle.writeRef(), timeOutTime);

			if (res == WAIT_TIMEOUT)
			{
				SLANG_LOG(std::cout << "#2 waitForTermination: Process FAILED TO terminated" << m_processHandle << "\n";)
				return false;
			}
			SLANG_LOG(std::cout << "#2 waitForTermination: Process terminated" << m_processHandle << "\n";)
			_hasTerminated();
			return true;
		}
		void terminate(int32_t returnCode)
		{
			if (!isTerminated())
			{
				// If it's not terminated, try terminating.
				// Might take time, so use isTerminated to check
				TerminateProcess(m_processHandle.writeRef(), UINT32(returnCode));
			}

		}
		void kill(int32_t returnCode)
		{
			if (!isTerminated())
			{
				// https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess
				TerminateProcess(m_processHandle.writeRef(), UINT32(returnCode));

				// Just assume it's done and set the return code
				m_returnValue = returnCode;
				m_processHandle.setNull();
			}
		}

		WinProcess(HANDLE handle, HANDLE *streams) :
			m_processHandle(handle)
		{
			for (Index i = 0; i < Index(StdStreamType::CountOf); ++i)
			{
				m_streams[i] = streams[i];
			}
		}
		HANDLE getStream(StdStreamType type) const { return m_streams[Index(type)]; }
		WinHandle m_processHandle;          ///< If not set the process has terminated

	protected:

		void _hasTerminated()
		{
			if (!m_processHandle.isNull())
			{
				// get exit code for process
				// https://docs.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess

				DWORD childExitCode = 0;
				if (GetExitCodeProcess(m_processHandle, &childExitCode))
				{
					m_returnValue = int32_t(childExitCode);
				}
				m_processHandle.setNull();
			}

		}
		int32_t m_returnValue = 0;                              ///< Value returned if process terminated
		HANDLE m_streams[Index(StdStreamType::CountOf)];   ///< Streams to communicate with the process

	};

	static WinProcess* m_process = nullptr;
	static WinPipeStream* m_readStream = nullptr;
	static WinPipeStream* m_writeStream = nullptr;
	static WinPipeStream* m_readErrStream = nullptr;
	HANDLE hProcessMgmtThread = NULL;
	const DWORD threadDiedWaitMS = 6000; //ms
	enum thread_state {
		ethread_state_start = 0,
		ethread_state_alive = 1,
		ethread_state_exit = 2
	};
	thread_state tstate = ethread_state_exit;
	HANDLE ghSemaphore = NULL;
	static HANDLE ghMutex = NULL;
	static bool g_hasProcess = false;
	static DWORD   dwThreadId;
	const DWORD sleepProcesMgmtThreads = 20;//20ms
	bool getMutexInfinite(bool sleepThread, int timeout_thresh=0);
	bool releaseMutex();

	SlangResult spawnThreadForTestServer();
	class SlangContext {
	public:
		Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
		bool globalSessionInit = false;
		std::string slangDllPath = ""; // By default this takes up the current directory. So keep the sldll and test
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
				char lpBuffer[128];
				DWORD ret = GetEnvironmentVariable("SLANG_DLL_PATH_OVERRIDE", lpBuffer, 128);
				if (ret > 0) {
					slangDllPath = lpBuffer;
				}
				if (!slangDllPath.empty()) {
					if (!SetDllDirectoryA(slangDllPath.c_str())) {
						SLANG_LOG(std::cout << "failed to set slang dll PATH\n";)
						return SLANG_FAIL;
					}
				}
				handle = LoadLibraryA("slang.dll");
				if (NULL == handle) {
					SLANG_LOG(std::cout << "failed to load slang.dll\n";)
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
		}
		static void _diagnosticCallback(
			char const* message,
			void*       /*userData*/)
		{
			printf("%s", message);
		}
		char * findSlangShaderStage(glu::ShaderType shaderType) {
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
				SLANG_LOG(std::cout << "unsupported shader stage:" << shaderType << "\n";)
				return "";
			}
			return "";
		};
		char* findSlangShaderExt(glu::ShaderType shaderType) {
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


		SlangResult CreateProcess(std::string &exename, std::string& cmdline, DWORD flags, WinProcess *&outProcess)
		{
			WinHandle childStdOutRead;
			WinHandle childStdErrRead;
			WinHandle childStdInWrite;

			WinHandle processHandle;
			{
				WinHandle childStdOutWrite;
				WinHandle childStdErrWrite;
				WinHandle childStdInRead;

				SECURITY_ATTRIBUTES securityAttributes;
				securityAttributes.nLength = sizeof(securityAttributes);
				securityAttributes.lpSecurityDescriptor = nullptr;
				securityAttributes.bInheritHandle = true;

				// 0 means use the 'system default'
				//const DWORD bufferSize = 64 * 1024;
				const DWORD bufferSize = 0;

				{
					WinHandle childStdOutReadTmp;
					WinHandle childStdErrReadTmp;
					WinHandle childStdInWriteTmp;
					// create stdout pipe for child process
					SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdOutReadTmp.writeRef(), childStdOutWrite.writeRef(), &securityAttributes, bufferSize));
					if ((flags & Process::Flag::DisableStdErrRedirection) == 0)
					{
						// create stderr pipe for child process
						SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdErrReadTmp.writeRef(), childStdErrWrite.writeRef(), &securityAttributes, bufferSize));
					}
					// create stdin pipe for child process        
					SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdInRead.writeRef(), childStdInWriteTmp.writeRef(), &securityAttributes, bufferSize));

					const HANDLE currentProcess = GetCurrentProcess();

					// https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle

					// create a non-inheritable duplicate of the stdout reader        
					SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdOutReadTmp, currentProcess, childStdOutRead.writeRef(), 0, FALSE, DUPLICATE_SAME_ACCESS));
					// create a non-inheritable duplicate of the stderr reader
					if (childStdErrReadTmp)
						SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdErrReadTmp, currentProcess, childStdErrRead.writeRef(), 0, FALSE, DUPLICATE_SAME_ACCESS));
					// create a non-inheritable duplicate of the stdin writer
					SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdInWriteTmp, currentProcess, childStdInWrite.writeRef(), 0, FALSE, DUPLICATE_SAME_ACCESS));
				}
				// TODO: switch to proper wide-character versions of these...
				STARTUPINFOW startupInfo;
				ZeroMemory(&startupInfo, sizeof(startupInfo));
				startupInfo.cb = sizeof(startupInfo);
				startupInfo.hStdError = childStdErrWrite;
				startupInfo.hStdOutput = childStdOutWrite;
				startupInfo.hStdInput = childStdInRead;
				startupInfo.dwFlags = STARTF_USESTDHANDLES;


				std::wstring wpath = std::wstring(exename.begin(), exename.end());
				LPCWSTR lppath = wpath.c_str();
				std::wstring wcmdline = std::wstring(cmdline.begin(), cmdline.end());
				LPCWSTR lpcmdline = wcmdline.c_str();

				// Now we can actually get around to starting a process
				PROCESS_INFORMATION processInfo;
				ZeroMemory(&processInfo, sizeof(processInfo));

				// https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags

				DWORD createFlags = CREATE_NO_WINDOW | CREATE_SUSPENDED;
				BOOL success = CreateProcessW(
					lppath,
					(LPWSTR)lpcmdline,
					nullptr,
					nullptr,
					true,
					createFlags,
					nullptr, // TODO: allow specifying environment variables?
					nullptr,
					&startupInfo,
					&processInfo);
				if (!success)
				{
					DWORD err = GetLastError();
					SLANG_UNUSED(err);
					return SLANG_FAIL;
				}
				ResumeThread(processInfo.hThread);

				// close handles we are now done with
				CloseHandle(processInfo.hThread);

				// Save the process handle
				processHandle = processInfo.hProcess;
			}
			HANDLE streams[Index(StdStreamType::CountOf)] = {NULL};

			if (childStdErrRead)
				streams[Index(StdStreamType::ErrorOut)] = childStdErrRead.detach();
			streams[Index(StdStreamType::Out)] = childStdOutRead.detach();//new WinPipeStream(childStdOutRead.detach(), FileAccess::Read);
			streams[Index(StdStreamType::In)] = childStdInWrite.detach();//new WinPipeStream(childStdInWrite.detach(), FileAccess::Write);
			outProcess = new WinProcess(processHandle.detach(), &streams[0]);

			return SLANG_OK;
		}
		// static const UnownedStringSlice TestServerProtocol::ExecuteCtsTestArgs::g_methodName = UnownedStringSlice::fromLiteral("unitTest");
		///* static */const StructRttiInfo TestServerProtocol::ExecuteCtsTestArgs::g_rttiInfo = _makeExecuteCtsTestArgsRtti();
		void createJSONCompileCommand(std::ostringstream &jsonCmd, std::string &filename, std::string &stage)
		{
			jsonCmd << "{\n"
					<< "    \"jsonrpc\" : \"2.0\", \n"
					<< "    \"method\" : \"tool\", \n"
					<< "    \"params\" : \n"
					<< "    [\n"
					<< "        \"slangc\", \n"
					<< "        [\n"
					<< "            \""+filename + "\", \n"
					<< "            \"-target\", \n"
					<< "            \"spirv\", \n"
					<< "            \"-stage\", \n"
					<< "            \""+stage + "\", \n"
					<< "            \"-entry\", \n"
					<< "            \"main\", \n"
					<< "            \"-allow-glsl\", \n"
					<< "            \"-matrix-layout-row-major\"\n"
					<< "        ]\n"
					<< "    ]\n"
					<< "}\n";
            return;
		}
        
		SlangResult sendCommand(std::string &filename, std::string &stage)
		{
			SlangResult res = SLANG_OK;
			std::ostringstream jsonCmd;
			createJSONCompileCommand(jsonCmd, filename, stage);
			std::string commandSize;
			commandSize = "Content-Length: " + std::to_string(jsonCmd.str().size())+"\r\n\r\n";
			if (!getMutexInfinite(false)) {
				SLANG_LOG(std::cout << "#5: Failed to acquire mutex\n";)
				return SLANG_FAIL;
			}
			if (!m_writeStream) {
				SLANG_LOG(std::cout << "read stream is NULL which means test-server has closed unexpectedly\n";)
				releaseMutex();
				return SLANG_FAIL;
			}
			res = m_writeStream->write(commandSize.c_str(), commandSize.size());
			if (res != SLANG_OK) {
				releaseMutex();
				SLANG_LOG(std::cout << "Failed to write the command size information\n";)
				return res;
			}
			res = m_writeStream->write(jsonCmd.str().c_str(), jsonCmd.str().size());
			if (res != SLANG_OK) {
				releaseMutex();
				SLANG_LOG(std::cout << "Failed to write the JSON command \n";)
				return res;
			}
			releaseMutex();

			return res;
		}

		enum class ReadState
		{
			Header,                     ///< Reading reader
			Content,                    ///< Reading content (ie header is read)
			Done,                       ///< The content is read
			Closed,                     ///< The read stream is closed - no further packets can be read
			Error,                      ///< In an error state - no further packets can be read
		};
#define MAX_TIMEOUT_ITER_COUNT 256
#define HEADER_BUFF_MAX_SIZE 1024
		SlangResult readResult(std::string &output) {
			DWORD sleepMS = 20;
			char content[HEADER_BUFF_MAX_SIZE];
			memset(content, 0, HEADER_BUFF_MAX_SIZE);
			ReadState state = ReadState::Header;
			int timeoutCount = 0;
			int bufferSize = -1;
			int currReadPointer = 0;
			int alreadyReadBuffer = 0;
			int bufferToBeRead = -1;
			bool skipsleep = false;
			while (state != ReadState::Done && timeoutCount <= MAX_TIMEOUT_ITER_COUNT)
			{
				skipsleep = false;
				if (!getMutexInfinite(false)) {
					SLANG_LOG(std::cout << "#6: Failed to acquire mutex\n";)
					return SLANG_FAIL;
				}
				if (!m_readStream) {
					SLANG_LOG(std::cout << "read stream is NULL which means test-server has closed unexpectedly\n";)
					releaseMutex();
					return SLANG_FAIL;
				}

				switch (state) {
				case ReadState::Header:
				{
					size_t contentSize = 0;
					//Read the header
					SlangResult readres = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
					releaseMutex();
					if (readres != SLANG_OK) {
						state = ReadState::Error;
						break;
					}
					if (contentSize > 0) {
						std::string contentstr;
						std::copy(content, content+ HEADER_BUFF_MAX_SIZE, std::back_inserter(contentstr));
						std::string pattern = "Content-Length: ";
						size_t pos_start = contentstr.find("Content-Length: ");
						if (pos_start == std::string::npos) {
							SLANG_LOG(std::cout << "failed to find the header pattern\n";)
							state = ReadState::Error;
							break;
						}
						pos_start = pattern.size();
						size_t pos_end = contentstr.find("\r");
						bufferSize = std::stoi(contentstr.substr(pos_start, pos_end- pos_start));
						size_t pos_json_start = contentstr.find("{");
						if (contentSize > pos_json_start) {
							//alreadyReadBuffer = contentSize - pos_json_start;
							output.append(contentstr.substr(pos_json_start, bufferSize));
							alreadyReadBuffer = output.size();
						}
					}
					if (bufferSize <= 0) {
						//std::cout << "failed to read the header: retry\n";
						//state = ReadState::Error;
						break;
					}
					state = ReadState::Content;
					skipsleep = true;

					break;
				}
				case ReadState::Content:
				{
					if (bufferSize - alreadyReadBuffer > 0) {
						int tobeRead = bufferSize - alreadyReadBuffer;
						char* readBuff = new char[bufferSize];
						size_t readStreamSize = 0;
						SlangResult res = m_readStream->read(readBuff, tobeRead, readStreamSize);
						releaseMutex();
						if (res != SLANG_OK) {
							state = ReadState::Error;
							break;
						}
						output.append(readBuff);
						alreadyReadBuffer += readStreamSize;
						if (alreadyReadBuffer == bufferSize) {
							state = ReadState::Done;
							skipsleep = true;
						}
					}
					else if (bufferSize == alreadyReadBuffer ) {
						skipsleep = true;
						state = ReadState::Done;
						break;
					}
					break;
				}
				case ReadState::Error:
					SLANG_LOG(std::cout << "Failed to read the results\n";)
					return SLANG_E_INTERNAL_FAIL;
				}
				if (!skipsleep) {
					Sleep(sleepMS);
					timeoutCount++;
				}
			}
			if (timeoutCount >= MAX_TIMEOUT_ITER_COUNT) {
				SLANG_LOG(std::cout << "Timer timed out\n";)
				// Kill the process and reset the thread state. 
				m_process->terminate(0);
				tstate = ethread_state_exit;
				g_hasProcess = false;

				SLANG_LOG(std::cout << "waiting for spwaned thread to be killed:" << hProcessMgmtThread << " threadID:" << dwThreadId<<"\n";)
				DWORD wres = WaitForSingleObject(hProcessMgmtThread, threadDiedWaitMS); // wait for the thread
				SLANG_LOG(std::cout << "Waited for thread id" << dwThreadId << "singleobject wait result =" << wres << "\n";)
				hProcessMgmtThread = NULL;
				return SLANG_E_TIME_OUT;
			}
			return SLANG_OK;
		}

		SlangResult KillProcessAndResetDS()
		{
			if (!m_process)
				return SLANG_OK;

			if (m_readErrStream) {
				delete m_readErrStream;
				m_readErrStream = nullptr;
			}
			if (m_readStream) {
				delete m_readStream;
				m_readStream = nullptr;
			}
			if (m_writeStream) {
				delete m_writeStream;
				m_writeStream = nullptr;
			}

			//m_process->terminate(0);
			delete m_process;
			m_process = NULL;
			return SLANG_OK;
		}
		// Sample custom data structure for threads to use.
		// This is passed by void pointer so it can be any data type
		// that can be passed using a single void pointer (LPVOID).

		SlangResult spawinAndWaitTestServer()
		{
			if (m_process)
				return SLANG_OK;

			SlangResult result = SLANG_OK;
			//WinProcess *process=nullptr;
			std::string exename = slangDllPath + "test-server.exe";
			std::string cmdline = slangDllPath + "test-server.exe";
			result = CreateProcess(exename, cmdline, Process::Flag::DisableStdErrRedirection, m_process);
			if (result != SLANG_OK) {
				SLANG_LOG(std::cout << "Failed to laucnh the test-server\n";)
				return SLANG_FAIL;
			}
			m_readStream = new WinPipeStream(m_process->getStream(StdStreamType::Out), FileAccess::Read);
			if (m_process->getStream(StdStreamType::ErrorOut))
				m_readErrStream = new WinPipeStream(m_process->getStream(StdStreamType::ErrorOut), FileAccess::Read);
			m_writeStream = new WinPipeStream(m_process->getStream(StdStreamType::In), FileAccess::Write);

			return SLANG_OK;
		}

		SlangResult parseSpirvAsm(string& output, std::vector<deUint32>* dst)
		{
			// get the SPIRV-txt
			//std::cout << output;
			size_t spirv_start_pos = output.find("; SPIR-V");
			if (spirv_start_pos == std::string::npos) {
				// Compilation failed to extract
				return SLANG_FAIL;
			}
			size_t spirv_end_pos = output.find("\", ", spirv_start_pos);
			std::string spvasm = output.substr(spirv_start_pos, spirv_end_pos - spirv_start_pos);
			size_t pos = spvasm.find("\\n");
			while (pos != std::string::npos) {
				spvasm.replace(pos, 2, "\n");
				//spvasm.erase(pos);
				pos = spvasm.find("\\n");
			};
			string pattern = "\\\"";
			pos = spvasm.find(pattern);
			while (pos != std::string::npos) {
				spvasm.replace(pos, 2, "\"");
				pos = spvasm.find(pattern);
			};
			//std::cout << "SPIRV ASM\n" << spvasm;

			// Now transform it into bitcode
			SpirVProgramInfo buildInfo;
			SpirVAsmSource program(spvasm);
			if (!assembleSpirV(&program, dst, &buildInfo, SPIRV_VERSION_1_0))
				return SLANG_FAIL;
			return SLANG_OK;
		}

		int setupSlangLikeSlangc(const std::vector<std::string>* sources, const ShaderBuildOptions& buildOptions, const ShaderLanguage shaderLanguage, std::vector<deUint32>* dst, glu::ShaderProgramInfo* buildInfo)
		{
			SlangResult result = SLANG_OK;
			bool enableServerMode = true;
			char lpBuffer[128];
			memset(lpBuffer, 0, 128);
			DWORD ret = GetEnvironmentVariable("DISABLE_CTS_SLANG_SERVER_MODE", lpBuffer, 128);
			if (ret > 0 && strcmp(lpBuffer, "1") == 0) {
				SLANG_LOG(std::cout << lpBuffer << "\n";)
				enableServerMode = false;
				SLANG_LOG(std::cout << "Disabled SLANG SERVER MODE\n";)
			}

			do {
				result = SetupSlangDLL();
				if (result != SLANG_OK) {
					SLANG_LOG(std::cout << "Failed to load SLANG DLL";)
					break;
				}
				SlangCompileRequest* compileRequest = nullptr;
				if (!enableServerMode) {
					getSlangFunctionHandles();
					if (!m_sfn.isInitialized()) {
						SLANG_LOG(std::cout << "Failed to get function pointers";)
						break;
					}
					result = m_sfn.pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
					if (result != SLANG_OK) {
						SLANG_LOG(std::cout << "Failed to create global session: " << std::hex << result << "\n";)
						break;
					}
					result = slangGlobalSession->createCompileRequest(&compileRequest);
					if (result != SLANG_OK) {
						SLANG_LOG(std::cout << "Failed to create CompileRequest: " << std::hex << result << "\n";)
						break;
					}
				}
				for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++) {
					if (!sources[shaderType].empty()) {
						const std::string& srcText = getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
						glu::ShaderType shaderstage = glu::ShaderType(shaderType);
						std::string slangShaderStage = findSlangShaderStage((glu::ShaderType)shaderType);
						if (slangShaderStage.empty()) {
							break;
						}
						const char* fileExt = findSlangShaderExt((glu::ShaderType)shaderType);
						if (!slangDllPath.empty())
							SetCurrentDirectory(slangDllPath.c_str());

						std::ofstream myfile;
						std::string temp_fname = "test.slang";
						temp_fname.append(fileExt);
						myfile.open(temp_fname);
						myfile << srcText.c_str();
						myfile.close();
						if (enableServerMode) {
							std::string output;
							//result = spawinAndWaitTestServer(); //this mechanism spawns the process in the same thread. This is not used in final implementation
							result = spawnThreadForTestServer();
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to spawn test sever: " << std::hex << result << "\n";)
								break;
							}
							result = sendCommand(temp_fname, slangShaderStage);
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to send command to test sever: " << std::hex << result << "\n";)
								break;
							}

							result = readResult(output);
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to read results from test sever: " << std::hex << result << "\n";)
								break;
							}

							//spv_binary binary;
							result = parseSpirvAsm(output, dst);
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to generate SPIRV ouptput from test-server results: " << std::hex << result << "\n";)
								break;
							}
							buildInfo->program.linkOk = true;
							//buildInfo->program.linkTimeUs = deGetMicroseconds() - linkStartTime;

						}
						else {
							compileRequest->addSearchPath(slangDllPath.c_str());
							compileRequest->setDiagnosticCallback(&_diagnosticCallback, nullptr);
							compileRequest->setCommandLineCompilerMode();
							const char* args[] = { "-target", "spirv", "-stage", slangShaderStage.c_str(), "-entry", "main", "-allow-glsl", "-matrix-layout-row-major", temp_fname.c_str(), "-o", "temp.spv" };
							int argCount = sizeof(args) / sizeof(char*);//8;
							result = compileRequest->processCommandLineArguments(args, argCount);
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to proces command line arguments: " << std::hex << result << "\n";)
								break;
							}
							const deUint64  compileStartTime = deGetMicroseconds();
							glu::ShaderInfo shaderBuildInfo;

							result = compileRequest->compile();
							if (result != SLANG_OK) {
								SLANG_LOG(std::cout << "Failed to compile: " << std::hex << result << "\n";)
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
							//spirvCode->release();
							compileRequest->release();

						}
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
					SLANG_LOG(std::cout << "failed to set slang dll PATH\n";)
					return SLANG_FAIL;
				}
				HMODULE handle = LoadLibraryA("slang.dll");
				if (NULL == handle) {
					SLANG_LOG(std::cout << "failed to load slang.dll\n";)
					return SLANG_FAIL;
				}
				typedef UINT(CALLBACK* LPFNDLLFUNC1)(SlangInt, slang::IGlobalSession**);
				LPFNDLLFUNC1 pfnslang_createGlobalSessionWithoutStdLib = (LPFNDLLFUNC1)GetProcAddress(handle,"slang_createGlobalSessionWithoutStdLib");
				if (!pfnslang_createGlobalSessionWithoutStdLib) {
					// handle the error
					SLANG_LOG(std::cout << "failed to get create global session method\n";)
					FreeLibrary(handle);
					return SLANG_FAIL;
				}
				;
				LPFNDLLFUNC1 pfnslang_createGlobalSession = (LPFNDLLFUNC1)GetProcAddress(handle, "slang_createGlobalSession");
				if (!pfnslang_createGlobalSession) {
					// handle the error
					SLANG_LOG(std::cout << "failed to get create global session method\n";)
					FreeLibrary(handle);
					return SLANG_FAIL;
				}

				//Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
				//result = slang::createGlobalSession(slangGlobalSession.writeRef());
				//result = slang_createGlobalSessionWithoutStdLib(SLANG_API_VERSION, slangGlobalSession.writeRef());
				result = pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
				if (result != SLANG_OK) {
					SLANG_LOG(std::cout << "Failed to create global session: " << std::hex << result << "\n";)
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
    					SLANG_LOG(std::cout << "Failed to create local session: " << std::hex << result << "\n";)
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
							SLANG_LOG(std::cout << "Failed to load the module\n";)
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
            			SLANG_LOG(std::cout << "Failed to find the entry point: " << std::hex << result << "\n";)
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
            				SLANG_LOG(std::cout << "Failed to create composite component type: " << std::hex << result << "\n";)
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
        						SLANG_LOG(std::cout << "Failed to link: " << std::hex << result << "\n";)
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
        						SLANG_LOG(std::cout << "Failed to generate SPIRV Code: " << std::hex << result << "\n";)
        						diagnoseIfNeeded(diagnosticsBlob);
        						return SLANG_FAIL;
        					}
        				}

						//copy the SPIRV
						uint32_t* buff32 = (uint32_t*)spirvCode->getBufferPointer();
						size_t size = spirvCode->getBufferSize();
						// print the buffer
						for (int i = 0; i < size / 4; i++) {
							//printf("%x ", buff32[i]);
							dst->push_back(buff32[i]);
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

	inline bool getMutexInfinite(bool sleepThread, int timeout_thresh) {
		//std::cout << "Inside infinited mutex thread id:" << GetCurrentThreadId() << "for mutex" << ghMutex << "\n";
		assert(ghMutex);
		while (true) { //&& timeout >= 0

			DWORD dwWaitResult;
			if (timeout_thresh > 0)
				dwWaitResult = WaitForSingleObject(ghMutex, timeout_thresh);
			else
				dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);
			if (dwWaitResult == WAIT_OBJECT_0) {
				return true;
			}
			if (dwWaitResult == WAIT_ABANDONED || dwWaitResult == WAIT_FAILED) {
				SLANG_LOG(std::cout << "getMutexInfinite: dWaitResult:" << dwWaitResult << std::endl;)
				return false;
			}
			if (sleepThread)
				Sleep(sleepProcesMgmtThreads); //sleep for 10ms for the other thread to make it work
		}
		return false;
	}

	inline DWORD getMutexState() {
		//int timeout = 50;
		SLANG_LOG(std::cout << "Inside infinited mutex thread id:" << GetCurrentThreadId() << "for mutex" << ghMutex << "\n";)
		assert(ghMutex);
		return WaitForSingleObject(ghMutex, 0);
		return false;
	}

	inline bool releaseMutex() {
		//std::cout << "Inside release mutex, thread id:" << GetCurrentThreadId() << "for mutex" << ghMutex << "\n";

		return ReleaseMutex(ghMutex);
	}

	SlangResult waitforSpawnthreadSignal(bool sleepThread)
	{
		assert(ghSemaphore);
		//int timeout = 50;
		while (true) { //&& timeout >= 0
			DWORD dwWaitResult = WaitForSingleObject(ghSemaphore, 0L);
			if (dwWaitResult == WAIT_OBJECT_0) {
				//semaphore got signaled we can continue
				return true;
			}
			if (sleepThread)
				Sleep(sleepProcesMgmtThreads); //sleep for 10ms for the other thread to make it work
			//timeout--;
		}
		return false;

	}

	DWORD WINAPI spawinAndWaitTestServerThread(LPVOID lpParam)
	{
		//std::cout << "Inside thread\n";
		do {
			tstate = ethread_state_start;
			ReleaseSemaphore(ghSemaphore, 1, NULL); // release the semahore
			SLANG_LOG(std::cout << "spawinAndWaitTestServerThread: #1 thread is active\n";)
;			if (!getMutexInfinite(false)) {
				SLANG_LOG(std::cout << "#1 spawinAndWaitTestServerThread: Failed to acquire mutex\n";)
				tstate = ethread_state_exit;
				return -1;
			}
			SLANG_LOG(std::cout << "#2 spawinAndWaitTestServerThread: Thread launching test-server \n";)
			if (g_slangContext.spawinAndWaitTestServer() == SLANG_OK) {
				SLANG_LOG(std::cout << "#3 spawinAndWaitTestServerThread: thread succeded to launch server \n";)
				tstate = ethread_state_alive;
				g_hasProcess = true;
				releaseMutex();
				HANDLE prochandle = HANDLE(m_process->m_processHandle);
				WaitForSingleObject(prochandle, INFINITE);
			}
			else {
				SLANG_LOG(std::cout << "#4 spawinAndWaitTestServerThread: thread failed to launch test-server \n";)
				tstate = ethread_state_exit;
				releaseMutex();
				return -1;
			}
			
			SLANG_LOG(std::cout << "#5 spawinAndWaitTestServerThread: thread state before is getting killed:" << tstate << " thread id:" << GetCurrentThreadId() << " threadHandle:" << GetCurrentThread() <<"\n";)
			if (getMutexState() == WAIT_TIMEOUT) {
				SLANG_LOG(std::cout << "#6 spawinAndWaitTestServerThread: spwaned process killed because it was hung, thread id : " << GetCurrentThreadId()<< "\n";)

				tstate = ethread_state_exit;
				releaseMutex();
				CloseHandle(ghMutex);
				ghMutex = NULL;
				g_hasProcess = false;
				return 0;
			}
			SLANG_LOG(std::cout << "#7 spawinAndWaitTestServerThread: thread state before is getting killed:" << tstate << " thread id:" << GetCurrentThreadId() << " threadHandle:" << GetCurrentThread() << "\n";)
			if (!getMutexInfinite(false)) {
				tstate = ethread_state_exit;
				SLANG_LOG(std::cout << "#8 spawinAndWaitTestServerThread: Failed to acquire mutex\n";)
				return -1;
			}
			SLANG_LOG(std::cout << "#9 spawinAndWaitTestServerThread: thread after taking mutex" << GetCurrentThreadId() << "\n";)
			CloseHandle(m_process->m_processHandle);
			g_slangContext.KillProcessAndResetDS();
			g_hasProcess = false;
			tstate = ethread_state_exit;
			releaseMutex();
			CloseHandle(ghMutex);
			ghMutex = NULL;
			SLANG_LOG(std::cout << "#10 spawinAndWaitTestServerThread: thread after exiting mutex" << GetCurrentThreadId() << "\n";)
		} while (false);
		return 0;
	}
	void flushtestserverpipes()
	{
		if (m_readStream) {
			char content[HEADER_BUFF_MAX_SIZE];
			size_t contentSize = 0;
			SlangResult res = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
			while (contentSize > 0 && res == SLANG_OK) {
				res = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
			}
		}
	}
	SlangResult spawnThreadForTestServer()
	{
		do {
			if (!ghMutex) {
				ghMutex = CreateMutex(
					NULL,              // default security attributes
					FALSE,             // initially not owned
					NULL);             // unnamed mutex
				if (ghMutex == NULL) {
					std::cout << "failed to create mutex for test-server\n";
					return -1;
				}
			}
			//std::cout << "#1: spawnThreadForTestServer: tstate:" << tstate << std::endl;
			if (tstate == ethread_state_exit) {
				ghSemaphore = CreateSemaphore(
					NULL,           // default security attributes
					0,  // initial count, this means the semaphor is not in signal state
					1,  // maximum count
					NULL);          // unnamed semaphore

				if (ghSemaphore == NULL)
				{
					printf("CreateSemaphore error: %d\n", GetLastError());
					return -1;
				}

				// This means process has exited, created a new thread
				hProcessMgmtThread = CreateThread(
					NULL,                   // default security attributes
					0,                      // use default stack size  
					spawinAndWaitTestServerThread,       // thread function name
					NULL,          // argument to thread function 
					0, // use default creation flags 
					&dwThreadId);   // returns the thread identifier
				if (hProcessMgmtThread == NULL)
				{
					printf("CreateThread error: %d\n", GetLastError());
					return -1;
				}

				if (!waitforSpawnthreadSignal(true)) {
					printf("Semaphore was never signalled error: %d\n", GetLastError());
					//TerminateThread(hProcessMgmtThread);
					CloseHandle(ghSemaphore);
					ghSemaphore = NULL;
					return -1;
				}
				CloseHandle(ghSemaphore);
				ghSemaphore = NULL;
			}
#if 0
			if (tstate == ethread_state_alive) {
				if (!getMutexInfinite(true)) {
					releaseMutex();
					std::cout << "#7: Failed to acquire mutex\n";
					return -1;
				}
				flushtestserverpipes();
				releaseMutex();
			}
#endif
			if (tstate == ethread_state_exit) {
				SLANG_LOG(std::cout << "#1 spawnThreadForTestServer: Failed to spawn server from the thread and the thread is dead\n";)
				WaitForSingleObject(hProcessMgmtThread, threadDiedWaitMS);
				return -1;
			}
			else {
				while (tstate != ethread_state_alive) {
					if (!getMutexInfinite(true)) {
						SLANG_LOG(std::cout << "#2 spawnThreadForTestServer: : Failed to acquire mutex\n";)
						return -1;
					}
					if (tstate == ethread_state_start) {
						// sever thread is in process of starting test-server
						// release the mutex and wait for updates
						releaseMutex(); // and poll again
					}
					else if (tstate == ethread_state_exit) {
						CloseHandle(ghMutex);
						ghMutex = NULL;
						hProcessMgmtThread = NULL;
						releaseMutex(); // and poll again
						std::cout << "#3 spawnThreadForTestServer: Worker thread failed to spawn the test-server and has exited\n";
						return -1;
					}
					else {
						// SUCCESS: test-sever is alive and ready to recieve commands
						assert(tstate == ethread_state_alive);
						// flush the files and clear them
						flushtestserverpipes();
						releaseMutex();
						break;
					}
				}
			}
		} while (false);
		return SLANG_OK;
	}

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
#if defined(ENABLE_SLANG_COMPILATION) && defined(_WIN32)
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
