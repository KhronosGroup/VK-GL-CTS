#if defined(ENABLE_SLANG_COMPILATION) && defined(_WIN32)

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "vkShaderToSpirV_slang.h"
#include "gluShaderProgram.hpp"
#include "vkPrograms.hpp"
#include "vkSpirVAsm.hpp"
#include "deClock.h"

#include <iostream>
#include <ostream>

#define ENABLE_SLANG_LOGS 1

#if defined(ENABLE_SLANG_LOGS)
#define SLANG_LOG std::cout << "SLANG: "
#else
// Define a null stream that ignores any input
class NullStream : public std::ostream
{
public:
    NullStream() : std::ostream(nullptr)
    {
    }
};

// Global instance of NullStream
inline NullStream SLANG_LOG;
#endif

// Include public Slang headers
#include <slang.h>
#include <slang-com-ptr.h>

// Additional includes
#include <core/slang-list.h>
#include <slang-test/test-context.h>

#ifndef SLANG_RETURN_FAIL_ON_FALSE
#define SLANG_RETURN_FAIL_ON_FALSE(x) \
    if (!(x))                         \
        return SLANG_FAIL;
#endif

namespace Slang
{

using namespace vk;

class SlangBlob : public ISlangBlob
{
public:
    SlangBlob(std::string in)
    {
        inputString = in;
    }
    SLANG_NO_THROW void const *SLANG_MCALL getBufferPointer()
    {
        return inputString.c_str();
    }
    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize()
    {
        return inputString.size();
    }
    // ISlangUnknown
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &guid, void **outObject)
    {
        return SLANG_OK;
    };
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef()
    {
        return 1;
    }
    SLANG_NO_THROW uint32_t SLANG_MCALL release()
    {
        return 1;
    }

protected:
    std::string inputString;
};
typedef void(CALLBACK *PFNSPSETDIAGONSTICCB)(SlangCompileRequest *, SlangDiagnosticCallback, void const *);
typedef void(CALLBACK *PFNSPSETCOMMANDLINECOMPILERMODE)(SlangCompileRequest *);
typedef SlangResult(CALLBACK *PFNSPPROCESSCOMMANDLINEARG)(SlangCompileRequest *, char const *, int);
typedef SlangResult(CALLBACK *PFNSPCOMPILE)(SlangCompileRequest *request);
typedef SlangResult(CALLBACK *PFNCREATEGLOBALSESSION)(SlangInt, slang::IGlobalSession **);
typedef SlangResult(CALLBACK *PFNCREATEGLOBALSESSION2)(const SlangGlobalSessionDesc *, slang::IGlobalSession **);

typedef struct _slangLibFuncs
{
    PFNSPSETDIAGONSTICCB pfnspSetDiagnosticCallback             = nullptr;
    PFNSPPROCESSCOMMANDLINEARG pfnspProcessCommandLineArguments = nullptr;
    PFNSPCOMPILE pfnspCompile                                   = nullptr;
    PFNCREATEGLOBALSESSION pfnslang_createGlobalSession         = nullptr;
    PFNCREATEGLOBALSESSION2 pfnslang_createGlobalSession2       = nullptr;

    bool isInitialized()
    {
        return (pfnspSetDiagnosticCallback && pfnspProcessCommandLineArguments && pfnspCompile &&
                pfnslang_createGlobalSession);
    }
} slangLibFuncs;

class WinHandle
{
public:
    /// Detach the encapsulated handle. Returns the handle (which now must be
    /// externally handled)
    HANDLE detach()
    {
        HANDLE handle = m_handle;
        m_handle      = nullptr;
        return handle;
    }

    /// Return as a handle
    operator HANDLE() const
    {
        return m_handle;
    }

    /// Assign
    void operator=(HANDLE handle)
    {
        setNull();
        m_handle = handle;
    }
    void operator=(WinHandle &&rhs)
    {
        HANDLE handle = m_handle;
        m_handle      = rhs.m_handle;
        rhs.m_handle  = handle;
    }

    /// Get ready for writing
    SLANG_FORCE_INLINE HANDLE *writeRef()
    {
        setNull();
        return &m_handle;
    }
    /// Get for read access
    SLANG_FORCE_INLINE const HANDLE *readRef() const
    {
        return &m_handle;
    }

    void setNull()
    {
        if (m_handle)
        {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
    bool isNull() const
    {
        return m_handle == nullptr;
    }

    /// Ctor
    WinHandle(HANDLE handle = nullptr) : m_handle(handle)
    {
    }
    WinHandle(WinHandle &&rhs) : m_handle(rhs.m_handle)
    {
        rhs.m_handle = nullptr;
    }

    /// Dtor
    ~WinHandle()
    {
        setNull();
    }

private:
    WinHandle(const WinHandle &)         = delete;
    void operator=(const WinHandle &rhs) = delete;

    HANDLE m_handle;
};

class WinPipeStream
{
public:
    typedef WinPipeStream ThisType;

    // Stream
    SlangResult read(void *buffer, size_t length, size_t &outReadBytes)
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
            DWORD pipeBytesRead           = 0;
            DWORD pipeTotalBytesAvailable = 0;
            DWORD pipeRemainingBytes      = 0;

            // Works on anonymous pipes too
            // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe

            SLANG_RETURN_ON_FAIL(_updateState(PeekNamedPipe(m_streamHandle, nullptr, DWORD(0), &pipeBytesRead,
                                                            &pipeTotalBytesAvailable, &pipeRemainingBytes)));
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

    SlangResult write(const void *buffer, size_t length)
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

    bool isEnd()
    {
        return m_streamHandle.isNull();
    }
    bool canRead()
    {
        return _has(FileAccess::Read) && !m_streamHandle.isNull();
    }
    bool canWrite()
    {
        return _has(FileAccess::Write) && !m_streamHandle.isNull();
    }
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
        m_access       = access;
        m_isOwned      = isOwned;

        // On Win32 a HANDLE has to be handled differently if it's a PIPE or FILE,
        // so first determine
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
    ~WinPipeStream()
    {
        close();
    }

protected:
    bool _has(FileAccess access) const
    {
        return (Index(access) & Index(m_access)) != 0;
    }

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
        SLANG_LOG << "#1 waitForTermination: start terminating process" << m_processHandle << std::endl;
        // wait for the process to exit
        // TODO: set a timeout as a safety measure...
        auto res = WaitForSingleObject(m_processHandle.writeRef(), timeOutTime);

        if (res == WAIT_TIMEOUT)
        {
            SLANG_LOG << "#2 waitForTermination: Process FAILED TO terminated" << m_processHandle << std::endl;
            return false;
        }
        SLANG_LOG << "#2 waitForTermination: Process terminated" << m_processHandle << std::endl;
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

    WinProcess(HANDLE handle, HANDLE *streams) : m_processHandle(handle)
    {
        for (Index i = 0; i < Index(StdStreamType::CountOf); ++i)
        {
            m_streams[i] = streams[i];
        }
    }
    HANDLE getStream(StdStreamType type) const
    {
        return m_streams[Index(type)];
    }
    WinHandle m_processHandle; ///< If not set the process has terminated

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
    int32_t m_returnValue = 0;                       ///< Value returned if process terminated
    HANDLE m_streams[Index(StdStreamType::CountOf)]; ///< Streams to communicate with the process
};

static WinProcess *m_process          = nullptr;
static WinPipeStream *m_readStream    = nullptr;
static WinPipeStream *m_writeStream   = nullptr;
static WinPipeStream *m_readErrStream = nullptr;
HANDLE hProcessMgmtThread             = NULL;
const DWORD threadDiedWaitMS          = 6000; // ms
enum thread_state
{
    ethread_state_start = 0,
    ethread_state_alive = 1,
    ethread_state_exit  = 2
};
thread_state tstate      = ethread_state_exit;
HANDLE ghSemaphore       = NULL;
static HANDLE ghMutex    = NULL;
static bool g_hasProcess = false;
static DWORD dwThreadId;
const DWORD sleepProcesMgmtThreads = 20; // 20ms
bool getMutexInfinite(bool sleepThread, int timeout_thresh = 0);
bool releaseMutex();

SlangResult spawnThreadForTestServer();
class SlangContext
{
public:
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    bool globalSessionInit   = false;
    std::string slangDllPath = ""; // By default this takes up the current
    // directory. So keep the sldll and test
    bool loadDLL   = false;
    HMODULE handle = nullptr;
    slangLibFuncs m_sfn;

    template <typename... TArgs>
    inline void reportError(const char *format, TArgs... args)
    {
        printf(format, args...);
    }

    inline void diagnoseIfNeeded(slang::IBlob *diagnosticsBlob)
    {
        if (diagnosticsBlob != nullptr)
        {
            reportError("%s", (const char *)diagnosticsBlob->getBufferPointer());
        }
    }
    int SetupSlangDLL()
    {
        if (!handle)
        {
            char lpBuffer[128];
            DWORD ret = GetEnvironmentVariable("SLANG_DLL_PATH_OVERRIDE", lpBuffer, 128);
            if (ret > 0)
            {
                slangDllPath = lpBuffer;
            }
            if (!slangDllPath.empty())
            {
                if (!SetDllDirectoryA(slangDllPath.c_str()))
                {
                    SLANG_LOG << "failed to set slang dll PATH" << std::endl;
                    return SLANG_FAIL;
                }
            }
            handle = LoadLibraryA("slang.dll");
            if (NULL == handle)
            {
                SLANG_LOG << "failed to load slang.dll" << std::endl;
                return SLANG_FAIL;
            }
        }
        return SLANG_OK;
    }

    void getSlangFunctionHandles()
    {
        m_sfn.pfnslang_createGlobalSession =
            (PFNCREATEGLOBALSESSION)GetProcAddress(handle, "slang_createGlobalSession");
        m_sfn.pfnslang_createGlobalSession2 =
            (PFNCREATEGLOBALSESSION2)GetProcAddress(handle, "slang_createGlobalSession2");
        m_sfn.pfnspCompile               = (PFNSPCOMPILE)GetProcAddress(handle, "spCompile");
        m_sfn.pfnspSetDiagnosticCallback = (PFNSPSETDIAGONSTICCB)GetProcAddress(handle, "spSetDiagnosticCallback");
        m_sfn.pfnspProcessCommandLineArguments =
            (PFNSPPROCESSCOMMANDLINEARG)GetProcAddress(handle, "spProcessCommandLineArguments");

        if (nullptr == m_sfn.pfnslang_createGlobalSession)
            SLANG_LOG << "failed to get slang_createGlobalSession" << std::endl;
        if (nullptr == m_sfn.pfnslang_createGlobalSession2)
            SLANG_LOG << "failed to get slang_createGlobalSession2" << std::endl;
        if (nullptr == m_sfn.pfnspCompile)
            SLANG_LOG << "failed to get spCompile" << std::endl;
        if (nullptr == m_sfn.pfnspSetDiagnosticCallback)
            SLANG_LOG << "failed to get spSetDiagnosticCallback" << std::endl;
        if (nullptr == m_sfn.pfnspProcessCommandLineArguments)
            SLANG_LOG << "failed to get spProcessCommandLineArguments" << std::endl;
    }
    static void _diagnosticCallback(char const *message, void * /*userData*/)
    {
        printf("%s", message);
    }
    char *findSlangShaderStage(glu::ShaderType shaderType)
    {
        switch (shaderType)
        {
        case glu::SHADERTYPE_VERTEX:
            return "vertex";
        case glu::SHADERTYPE_FRAGMENT:
            return "fragment";
        case glu::SHADERTYPE_GEOMETRY:
            return "geometry";
        case glu::SHADERTYPE_COMPUTE:
            return "compute";
        default:
            SLANG_LOG << "unsupported shader stage:" << shaderType << std::endl;
            return "";
        }
        return "";
    };
    char *findSlangShaderExt(glu::ShaderType shaderType)
    {
        switch (shaderType)
        {
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

    SlangResult CreateProcess(std::string &exename, std::string &cmdline, DWORD flags, WinProcess *&outProcess)
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
            securityAttributes.nLength              = sizeof(securityAttributes);
            securityAttributes.lpSecurityDescriptor = nullptr;
            securityAttributes.bInheritHandle       = true;

            // 0 means use the 'system default'
            // const DWORD bufferSize = 64 * 1024;
            const DWORD bufferSize = 0;

            {
                WinHandle childStdOutReadTmp;
                WinHandle childStdErrReadTmp;
                WinHandle childStdInWriteTmp;
                // create stdout pipe for child process
                SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdOutReadTmp.writeRef(), childStdOutWrite.writeRef(),
                                                      &securityAttributes, bufferSize));
                if ((flags & Process::Flag::DisableStdErrRedirection) == 0)
                {
                    // create stderr pipe for child process
                    SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdErrReadTmp.writeRef(), childStdErrWrite.writeRef(),
                                                          &securityAttributes, bufferSize));
                }
                // create stdin pipe for child process
                SLANG_RETURN_FAIL_ON_FALSE(CreatePipe(childStdInRead.writeRef(), childStdInWriteTmp.writeRef(),
                                                      &securityAttributes, bufferSize));

                const HANDLE currentProcess = GetCurrentProcess();

                // https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle

                // create a non-inheritable duplicate of the stdout reader
                SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdOutReadTmp, currentProcess,
                                                           childStdOutRead.writeRef(), 0, FALSE,
                                                           DUPLICATE_SAME_ACCESS));
                // create a non-inheritable duplicate of the stderr reader
                if (childStdErrReadTmp)
                    SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdErrReadTmp, currentProcess,
                                                               childStdErrRead.writeRef(), 0, FALSE,
                                                               DUPLICATE_SAME_ACCESS));
                // create a non-inheritable duplicate of the stdin writer
                SLANG_RETURN_FAIL_ON_FALSE(DuplicateHandle(currentProcess, childStdInWriteTmp, currentProcess,
                                                           childStdInWrite.writeRef(), 0, FALSE,
                                                           DUPLICATE_SAME_ACCESS));
            }
            // TODO: switch to proper wide-character versions of these...
            STARTUPINFOW startupInfo;
            ZeroMemory(&startupInfo, sizeof(startupInfo));
            startupInfo.cb         = sizeof(startupInfo);
            startupInfo.hStdError  = childStdErrWrite;
            startupInfo.hStdOutput = childStdOutWrite;
            startupInfo.hStdInput  = childStdInRead;
            startupInfo.dwFlags    = STARTF_USESTDHANDLES;

            std::wstring wpath    = std::wstring(exename.begin(), exename.end());
            LPCWSTR lppath        = wpath.c_str();
            std::wstring wcmdline = std::wstring(cmdline.begin(), cmdline.end());
            LPCWSTR lpcmdline     = wcmdline.c_str();

            // Now we can actually get around to starting a process
            PROCESS_INFORMATION processInfo;
            ZeroMemory(&processInfo, sizeof(processInfo));

            // https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags

            DWORD createFlags = CREATE_NO_WINDOW | CREATE_SUSPENDED;
            BOOL success      = CreateProcessW(lppath, (LPWSTR)lpcmdline, nullptr, nullptr, true, createFlags,
                                               nullptr, // TODO: allow specifying environment variables?
                                               nullptr, &startupInfo, &processInfo);
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
        streams[Index(StdStreamType::Out)] = childStdOutRead.detach(); // new WinPipeStream(childStdOutRead.detach(),
        // FileAccess::Read);
        streams[Index(StdStreamType::In)] = childStdInWrite.detach(); // new WinPipeStream(childStdInWrite.detach(),
        // FileAccess::Write);
        outProcess = new WinProcess(processHandle.detach(), &streams[0]);

        return SLANG_OK;
    }
    // static const UnownedStringSlice
    // TestServerProtocol::ExecuteCtsTestArgs::g_methodName =
    // UnownedStringSlice::fromLiteral("unitTest");
    ///* static */const StructRttiInfo
    ///TestServerProtocol::ExecuteCtsTestArgs::g_rttiInfo =
    ///_makeExecuteCtsTestArgsRtti();
    void createJSONCompileCommand(std::ostringstream &jsonCmd, std::string &filename, std::string &stage)
    {
        jsonCmd << "{\n"
                << "    \"jsonrpc\" : \"2.0\", \n"
                << "    \"method\" : \"tool\", \n"
                << "    \"params\" : \n"
                << "    [\n"
                << "        \"slangc\", \n"
                << "        [\n"
                << "            \"" + filename + "\", \n"
                << "            \"-target\", \n"
                << "            \"spirv\", \n"
                << "            \"-stage\", \n"
                << "            \"" + stage + "\", \n"
                << "            \"-entry\", \n"
                << "            \"main\", \n"
                << "            \"-allow-glsl\", \n"
                << "            \"-matrix-layout-row-major\"\n"
                << "        ]\n"
                << "    ]\n"
                << "}" << std::endl;
        return;
    }

    SlangResult sendCommand(std::string &filename, std::string &stage)
    {
        SlangResult res = SLANG_OK;
        std::ostringstream jsonCmd;
        createJSONCompileCommand(jsonCmd, filename, stage);
        std::string commandSize;
        commandSize = "Content-Length: " + std::to_string(jsonCmd.str().size()) + "\r\n\r\n";
        if (!getMutexInfinite(false))
        {
            SLANG_LOG << "#5: Failed to acquire mutex" << std::endl;
            return SLANG_FAIL;
        }
        if (!m_writeStream)
        {
            SLANG_LOG << "read stream is NULL which means test-server has "
                         "closed unexpectedly"
                      << std::endl;
            releaseMutex();
            return SLANG_FAIL;
        }
        res = m_writeStream->write(commandSize.c_str(), commandSize.size());
        if (res != SLANG_OK)
        {
            releaseMutex();
            SLANG_LOG << "Failed to write the command size information" << std::endl;
            return res;
        }
        res = m_writeStream->write(jsonCmd.str().c_str(), jsonCmd.str().size());
        if (res != SLANG_OK)
        {
            releaseMutex();
            SLANG_LOG << "Failed to write the JSON command " << std::endl;
            return res;
        }
        releaseMutex();

        return res;
    }

    enum class ReadState
    {
        Header,  ///< Reading reader
        Content, ///< Reading content (ie header is read)
        Done,    ///< The content is read
        Closed,  ///< The read stream is closed - no further packets can be read
        Error,   ///< In an error state - no further packets can be read
    };
#define MAX_TIMEOUT_ITER_COUNT 256
#define HEADER_BUFF_MAX_SIZE 1024
    SlangResult readResult(std::string &output)
    {
        DWORD sleepMS = 20;
        char content[HEADER_BUFF_MAX_SIZE];
        memset(content, 0, HEADER_BUFF_MAX_SIZE);
        ReadState state       = ReadState::Header;
        int timeoutCount      = 0;
        int bufferSize        = -1;
        int currReadPointer   = 0;
        int alreadyReadBuffer = 0;
        int bufferToBeRead    = -1;
        bool skipsleep        = false;
        while (state != ReadState::Done && timeoutCount <= MAX_TIMEOUT_ITER_COUNT)
        {
            skipsleep = false;
            if (!getMutexInfinite(false))
            {
                SLANG_LOG << "#6: Failed to acquire mutex" << std::endl;
                return SLANG_FAIL;
            }
            if (!m_readStream)
            {
                SLANG_LOG << "read stream is NULL which means test-server "
                             "has closed unexpectedly"
                          << std::endl;
                releaseMutex();
                return SLANG_FAIL;
            }

            switch (state)
            {
            case ReadState::Header:
            {
                size_t contentSize = 0;
                // Read the header
                SlangResult readres = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
                releaseMutex();
                if (readres != SLANG_OK)
                {
                    state = ReadState::Error;
                    break;
                }
                if (contentSize > 0)
                {
                    std::string contentstr;
                    std::copy(content, content + HEADER_BUFF_MAX_SIZE, std::back_inserter(contentstr));
                    std::string pattern = "Content-Length: ";
                    size_t pos_start    = contentstr.find("Content-Length: ");
                    if (pos_start == std::string::npos)
                    {
                        SLANG_LOG << "failed to find the header pattern" << std::endl;
                        state = ReadState::Error;
                        break;
                    }
                    pos_start             = pattern.size();
                    size_t pos_end        = contentstr.find("\r");
                    bufferSize            = std::stoi(contentstr.substr(pos_start, pos_end - pos_start));
                    size_t pos_json_start = contentstr.find("{");
                    if (contentSize > pos_json_start)
                    {
                        // alreadyReadBuffer = contentSize - pos_json_start;
                        output.append(contentstr.substr(pos_json_start, bufferSize));
                        alreadyReadBuffer = (int)output.size();
                    }
                }
                if (bufferSize <= 0)
                {
                    // std::cout << "failed to read the header: retry" << std::endl;
                    // state = ReadState::Error;
                    break;
                }
                state     = ReadState::Content;
                skipsleep = true;

                break;
            }
            case ReadState::Content:
            {
                if (bufferSize - alreadyReadBuffer > 0)
                {
                    int tobeRead          = bufferSize - alreadyReadBuffer;
                    char *readBuff        = new char[bufferSize];
                    size_t readStreamSize = 0;
                    SlangResult res       = m_readStream->read(readBuff, tobeRead, readStreamSize);
                    releaseMutex();
                    if (res != SLANG_OK)
                    {
                        state = ReadState::Error;
                        break;
                    }
                    output.append(readBuff);
                    alreadyReadBuffer += (int)readStreamSize;
                    if (alreadyReadBuffer == bufferSize)
                    {
                        state     = ReadState::Done;
                        skipsleep = true;
                    }
                }
                else if (bufferSize == alreadyReadBuffer)
                {
                    skipsleep = true;
                    state     = ReadState::Done;
                    break;
                }
                break;
            }
            case ReadState::Error:
                SLANG_LOG << "Failed to read the results" << std::endl;
                return SLANG_E_INTERNAL_FAIL;
            }
            if (!skipsleep)
            {
                Sleep(sleepMS);
                timeoutCount++;
            }
        }
        if (timeoutCount >= MAX_TIMEOUT_ITER_COUNT)
        {
            SLANG_LOG << "Timer timed out" << std::endl;
            // Kill the process and reset the thread state.
            m_process->terminate(0);
            tstate       = ethread_state_exit;
            g_hasProcess = false;

            SLANG_LOG << "waiting for spwaned thread to be killed:" << hProcessMgmtThread << " threadID:" << dwThreadId
                      << std::endl;
            DWORD wres = WaitForSingleObject(hProcessMgmtThread,
                                             threadDiedWaitMS); // wait for the thread
            SLANG_LOG << "Waited for thread id" << dwThreadId << "singleobject wait result =" << wres << std::endl;
            hProcessMgmtThread = NULL;
            return SLANG_E_TIME_OUT;
        }
        return SLANG_OK;
    }

    SlangResult KillProcessAndResetDS()
    {
        if (!m_process)
            return SLANG_OK;

        if (m_readErrStream)
        {
            delete m_readErrStream;
            m_readErrStream = nullptr;
        }
        if (m_readStream)
        {
            delete m_readStream;
            m_readStream = nullptr;
        }
        if (m_writeStream)
        {
            delete m_writeStream;
            m_writeStream = nullptr;
        }

        // m_process->terminate(0);
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
        // WinProcess *process=nullptr;
        std::string exename = slangDllPath + "test-server.exe";
        std::string cmdline = slangDllPath + "test-server.exe";
        result              = CreateProcess(exename, cmdline, Process::Flag::DisableStdErrRedirection, m_process);
        if (result != SLANG_OK)
        {
            SLANG_LOG << "Failed to laucnh the test-server" << std::endl;
            return SLANG_FAIL;
        }
        m_readStream = new WinPipeStream(m_process->getStream(StdStreamType::Out), FileAccess::Read);
        if (m_process->getStream(StdStreamType::ErrorOut))
            m_readErrStream = new WinPipeStream(m_process->getStream(StdStreamType::ErrorOut), FileAccess::Read);
        m_writeStream = new WinPipeStream(m_process->getStream(StdStreamType::In), FileAccess::Write);

        return SLANG_OK;
    }

    SlangResult parseSpirvAsm(std::string &output, std::vector<uint32_t> *dst)
    {
        // get the SPIRV-txt
        // std::cout << output;
        size_t spirv_start_pos = output.find("; SPIR-V");
        if (spirv_start_pos == std::string::npos)
        {
            // Compilation failed to extract
            return SLANG_FAIL;
        }
        size_t spirv_end_pos = output.find("\", ", spirv_start_pos);
        std::string spvasm   = output.substr(spirv_start_pos, spirv_end_pos - spirv_start_pos);
        size_t pos           = spvasm.find("\\n");
        while (pos != std::string::npos)
        {
            spvasm.replace(pos, 2, "\n");
            // spvasm.erase(pos);
            pos = spvasm.find("\\n");
        };
        std::string pattern = "\\\"";
        pos                 = spvasm.find(pattern);
        while (pos != std::string::npos)
        {
            spvasm.replace(pos, 2, "\"");
            pos = spvasm.find(pattern);
        };
        // std::cout << "SPIRV ASM\n" << spvasm;

        // Now transform it into bitcode
        SpirVProgramInfo buildInfo;
        SpirVAsmSource program(spvasm);
        if (!assembleSpirV(&program, dst, &buildInfo, SPIRV_VERSION_1_5))
            return SLANG_FAIL;
        return SLANG_OK;
    }

    int setupSlangLikeSlangc(const std::vector<std::string> *sources, const ShaderBuildOptions &buildOptions,
                             const ShaderLanguage shaderLanguage, std::vector<uint32_t> *dst,
                             glu::ShaderProgramInfo *buildInfo)
    {
        SlangResult result    = SLANG_OK;
        bool enableServerMode = true;
        char lpBuffer[128];
        memset(lpBuffer, 0, 128);
        DWORD ret = GetEnvironmentVariable("DISABLE_CTS_SLANG_SERVER_MODE", lpBuffer, 128);
        if (ret > 0 && strcmp(lpBuffer, "1") == 0)
        {
            static bool s_needToPrintOnce = true;
            if (s_needToPrintOnce)
            {
                SLANG_LOG << "Disabled SLANG SERVER MODE: " << lpBuffer << std::endl;
                s_needToPrintOnce = false;
            }
            enableServerMode = false;
        }

        do
        {
            result = SetupSlangDLL();
            if (result != SLANG_OK)
            {
                SLANG_LOG << "Failed to load SLANG DLL";
                break;
            }
            SlangCompileRequest *compileRequest = nullptr;
            if (!enableServerMode)
            {
                getSlangFunctionHandles();
                if (!m_sfn.isInitialized())
                {
                    SLANG_LOG << "Failed to get function pointers";
                    break;
                }

                if (m_sfn.pfnslang_createGlobalSession2)
                {
                    SlangGlobalSessionDesc desc;
                    desc.enableGLSL = true;
                    result          = m_sfn.pfnslang_createGlobalSession2(&desc, slangGlobalSession.writeRef());
                    if (result != SLANG_OK)
                    {
                        SLANG_LOG << "Failed to create global session: " << std::hex << result << std::endl;
                        break;
                    }
                }
                else
                {
                    result = m_sfn.pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
                    if (result != SLANG_OK)
                    {
                        SLANG_LOG << "Failed to create global session: " << std::hex << result << std::endl;
                        break;
                    }
                }

                result = slangGlobalSession->createCompileRequest(&compileRequest);
                if (result != SLANG_OK)
                {
                    SLANG_LOG << "Failed to create CompileRequest: " << std::hex << result << std::endl;
                    break;
                }
            }
            for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
            {
                if (!sources[shaderType].empty())
                {
                    const std::string &srcText =
                        Slang::getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
                    glu::ShaderType shaderstage  = glu::ShaderType(shaderType);
                    std::string slangShaderStage = findSlangShaderStage((glu::ShaderType)shaderType);
                    if (slangShaderStage.empty())
                    {
                        break;
                    }
                    const char *fileExt = findSlangShaderExt((glu::ShaderType)shaderType);
                    if (!slangDllPath.empty())
                        SetCurrentDirectory(slangDllPath.c_str());

                    std::ofstream myfile;
                    std::string temp_fname = "test.slang";
                    temp_fname.append(fileExt);
                    myfile.open(temp_fname);
                    myfile << srcText.c_str();
                    myfile.close();
                    if (enableServerMode)
                    {
                        std::string output;
                        // result = spawinAndWaitTestServer(); //this mechanism spawns the
                        // process in the same thread. This is not used in final
                        // implementation
                        result = spawnThreadForTestServer();
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to spawn test sever: " << std::hex << result << std::endl;
                            break;
                        }
                        result = sendCommand(temp_fname, slangShaderStage);
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to send command to test sever: " << std::hex << result << std::endl;
                            break;
                        }

                        result = readResult(output);
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to read results from test sever: " << std::hex << result << std::endl;
                            break;
                        }

                        // spv_binary binary;
                        result = parseSpirvAsm(output, dst);
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to generate SPIRV ouptput from "
                                         "test-server results: "
                                      << std::hex << result << std::endl;
                            break;
                        }
                        buildInfo->program.linkOk = true;
                        // buildInfo->program.linkTimeUs = deGetMicroseconds() -
                        // linkStartTime;
                    }
                    else
                    {
                        compileRequest->addSearchPath(slangDllPath.c_str());
                        compileRequest->setDiagnosticCallback(&_diagnosticCallback, nullptr);
                        compileRequest->setCommandLineCompilerMode();
                        const char *args[] = {"-target",          "spirv", "-stage",      slangShaderStage.c_str(),
                                              "-entry",           "main",  "-allow-glsl", "-matrix-layout-row-major",
                                              temp_fname.c_str(), "-o",    "temp.spv"};
                        int argCount       = sizeof(args) / sizeof(char *); // 8;
                        result             = compileRequest->processCommandLineArguments(args, argCount);
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to proces command line arguments: " << std::hex << result << std::endl;
                            break;
                        }
                        const uint64_t compileStartTime = deGetMicroseconds();
                        glu::ShaderInfo shaderBuildInfo;

                        result = compileRequest->compile();
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to compile: " << std::hex << result << std::endl;
                            break;
                        }
                        shaderBuildInfo.type    = (glu::ShaderType)shaderType;
                        shaderBuildInfo.source  = srcText;
                        shaderBuildInfo.infoLog = ""; // shader.getInfoLog(); // \todo [2015-07-13 pyry] Include
                        // debug log?
                        shaderBuildInfo.compileTimeUs = deGetMicroseconds() - compileStartTime;
                        shaderBuildInfo.compileOk     = (result == SLANG_OK);
                        buildInfo->shaders.push_back(shaderBuildInfo);

                        const uint64_t linkStartTime = deGetMicroseconds();

                        Slang::ComPtr<ISlangBlob> spirvCode;
                        compileRequest->getEntryPointCodeBlob(0, 0, spirvCode.writeRef());

                        // copy the SPIRV
                        uint32_t *buff32 = (uint32_t *)spirvCode->getBufferPointer();
                        size_t size      = spirvCode->getBufferSize();
                        // print the buffer
                        for (int i = 0; i < size / 4; i++)
                        {
                            // printf("%x ", buff32[i]);
                            dst->push_back(buff32[i]);
                        }
                        buildInfo->program.infoLog = ""; // glslangProgram.getInfoLog(); // \todo [2015-11-05 scygan]
                        // Include debug log?
                        buildInfo->program.linkOk     = true;
                        buildInfo->program.linkTimeUs = deGetMicroseconds() - linkStartTime;
                        // spirvCode->release();
                        compileRequest->release();
                    }
                }
            }
        } while (false);
        return result;
    }

    // SLANG ISession Interface to generate SIPRV
    int setupSlang(const std::vector<std::string> *sources, const ShaderBuildOptions &buildOptions,
                   const ShaderLanguage shaderLanguage, std::vector<uint32_t> *dst, glu::ShaderProgramInfo *buildInfo)
    {
        SlangResult result = SLANG_OK;

        if (!globalSessionInit)
        {
            // load DLL
            if (!SetDllDirectoryA(slangDllPath.c_str()))
            {
                SLANG_LOG << "failed to set slang dll PATH" << std::endl;
                return SLANG_FAIL;
            }
            HMODULE handle = LoadLibraryA("slang.dll");
            if (NULL == handle)
            {
                SLANG_LOG << "failed to load slang.dll" << std::endl;
                return SLANG_FAIL;
            }

            typedef UINT(CALLBACK * LPFNDLLFUNC1)(SlangInt, slang::IGlobalSession **);
            typedef UINT(CALLBACK * LPFNDLLFUNC2)(const SlangGlobalSessionDesc *, slang::IGlobalSession **);

            LPFNDLLFUNC1 pfnslang_createGlobalSession =
                (LPFNDLLFUNC1)GetProcAddress(handle, "slang_createGlobalSession");
            if (!pfnslang_createGlobalSession)
            {
                // handle the error
                SLANG_LOG << "failed to get create global session method" << std::endl;
                FreeLibrary(handle);
                return SLANG_FAIL;
            }

            LPFNDLLFUNC2 pfnslang_createGlobalSession2 =
                (LPFNDLLFUNC2)GetProcAddress(handle, "slang_createGlobalSession2");
            if (!pfnslang_createGlobalSession2)
            {
                // handle the error
                SLANG_LOG << "failed to get create global session method 2" << std::endl;
            }

            if (pfnslang_createGlobalSession2)
            {
                SlangGlobalSessionDesc desc;
                desc.enableGLSL = true;
                result          = pfnslang_createGlobalSession2(&desc, slangGlobalSession.writeRef());
                if (result != SLANG_OK)
                {
                    SLANG_LOG << "Failed to create global session 2: " << std::hex << result << std::endl;
                    return result;
                }
            }
            else
            {
                result = pfnslang_createGlobalSession(SLANG_API_VERSION, slangGlobalSession.writeRef());
                if (result != SLANG_OK)
                {
                    SLANG_LOG << "Failed to create global session: " << std::hex << result << std::endl;
                    return result;
                }
            }

            globalSessionInit = true;
        }
        // Next we create a compilation session to generate SPIRV code from Slang
        // source.
        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc   = {};
        targetDesc.format              = SLANG_SPIRV;
        targetDesc.profile             = slangGlobalSession->findProfile("glsl440");
        targetDesc.flags               = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

        sessionDesc.targets     = &targetDesc;
        sessionDesc.targetCount = 1;

        for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
        {
            if (!sources[shaderType].empty())
            {
                const std::string &srcText =
                    Slang::getShaderStageSource(sources, buildOptions, (glu::ShaderType)shaderType);
                glu::ShaderType shaderstage = glu::ShaderType(shaderType);

                Slang::ComPtr<slang::ISession> session;
                result = slangGlobalSession->createSession(sessionDesc, session.writeRef());
                if (result != SLANG_OK)
                {
                    SLANG_LOG << "Failed to create local session: " << std::hex << result << std::endl;
                    // return SLANG_FAIL;
                    break;
                }
                // session->setAllowGLSLInput(true);
                slang::IModule *slangModule = nullptr;
                {
                    // write the file onto the disk temporarily
                    // SetCurrentDirectory(slangDllPath);
                    std::ofstream myfile;
                    myfile.open("test.slang");
                    myfile << srcText.c_str();
                    myfile.close();

                    char *path = ".\\";
                    Slang::ComPtr<slang::IBlob> diagnosticBlob;
                    SlangBlob blobSource = SlangBlob(srcText);
                    // slangModule = session->loadModuleFromSource("cts-test", path,
                    // &blobSource, diagnosticBlob.writeRef());
                    slangModule = session->loadModule("test", diagnosticBlob.writeRef());
                    if (!slangModule)
                    {
                        SLANG_LOG << "Failed to load the module" << std::endl;
                        diagnoseIfNeeded(diagnosticBlob);
                        result = SLANG_FAIL;
                    }
                }
                // ERROR: loadModule fails to find the entry point as it is looking for
                // "[shader("compute")]" to identify the entry point Hence switching to
                // slangc mechanism for the same
                const uint64_t compileStartTime = deGetMicroseconds();
                glu::ShaderInfo shaderBuildInfo;
                if (result == SLANG_FAIL)
                {
                    shaderBuildInfo.type    = (glu::ShaderType)shaderType;
                    shaderBuildInfo.source  = srcText;
                    shaderBuildInfo.infoLog = ""; // shader.getInfoLog(); // \todo
                    // [2015-07-13 pyry] Include debug log?
                    shaderBuildInfo.compileOk = false;
                    return SLANG_FAIL;
                }

                ComPtr<slang::IEntryPoint> entryPoint;
                result = slangModule->findEntryPointByName("main", entryPoint.writeRef());
                if (result != SLANG_OK)
                {
                    SLANG_LOG << "Failed to find the entry point: " << std::hex << result << std::endl;
                    // return SLANG_FAIL;
                    // break;
                }
                slang::IComponentType *componentTypes[] = {slangModule, entryPoint};
                ComPtr<slang::IComponentType> composedProgram;
                if (result == SLANG_OK)
                {
                    ComPtr<slang::IBlob> diagnosticsBlob;
                    SlangResult result = session->createCompositeComponentType(
                        &componentTypes[0], 2, composedProgram.writeRef(), diagnosticsBlob.writeRef());
                    if (result != SLANG_OK)
                    {
                        SLANG_LOG << "Failed to create composite component type: " << std::hex << result << std::endl;
                        diagnoseIfNeeded(diagnosticsBlob);
                        // return SLANG_FAIL;
                        // break;
                    }
                }
                shaderBuildInfo.type    = (glu::ShaderType)shaderType;
                shaderBuildInfo.source  = srcText;
                shaderBuildInfo.infoLog = ""; // shader.getInfoLog(); // \todo
                // [2015-07-13 pyry] Include debug log?
                shaderBuildInfo.compileTimeUs = deGetMicroseconds() - compileStartTime;
                shaderBuildInfo.compileOk     = (result == SLANG_OK);
                buildInfo->shaders.push_back(shaderBuildInfo);

                if (buildInfo->shaders[0].compileOk)
                {
                    const uint64_t linkStartTime = deGetMicroseconds();
                    // Link it against all depdendencies
                    ComPtr<slang::IComponentType> linkedProgram;
                    {
                        ComPtr<slang::IBlob> diagnosticsBlob;
                        result = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to link: " << std::hex << result << std::endl;
                            diagnoseIfNeeded(diagnosticsBlob);
                        }
                    }

                    buildInfo->program.infoLog = ""; // glslangProgram.getInfoLog(); // \todo [2015-11-05 scygan]
                    // Include debug log?
                    buildInfo->program.linkOk     = (result != 0);
                    buildInfo->program.linkTimeUs = deGetMicroseconds() - linkStartTime;
                }
                // Now we can call `composedProgram->getEntryPointCode()` to retrieve
                // the
                if (buildInfo->program.linkOk)
                {
                    // compiled SPIRV code that we will use to create a vulkan compute
                    // pipeline. This will trigger the final Slang compilation and spirv
                    // code generation.
                    ComPtr<slang::IBlob> spirvCode;
                    {
                        ComPtr<slang::IBlob> diagnosticsBlob;
                        result =
                            composedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnosticsBlob.writeRef());
                        if (result != SLANG_OK)
                        {
                            SLANG_LOG << "Failed to generate SPIRV Code: " << std::hex << result << std::endl;
                            diagnoseIfNeeded(diagnosticsBlob);
                            return SLANG_FAIL;
                        }
                    }

                    // copy the SPIRV
                    uint32_t *buff32 = (uint32_t *)spirvCode->getBufferPointer();
                    size_t size      = spirvCode->getBufferSize();
                    // print the buffer
                    for (int i = 0; i < size / 4; i++)
                    {
                        // printf("%x ", buff32[i]);
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

inline bool getMutexInfinite(bool sleepThread, int timeout_thresh)
{
    // std::cout << "Inside infinited mutex thread id:" << GetCurrentThreadId() <<
    // "for mutex" << ghMutex << std::endl;
    assert(ghMutex);
    while (true)
    { //&& timeout >= 0

        DWORD dwWaitResult;
        if (timeout_thresh > 0)
            dwWaitResult = WaitForSingleObject(ghMutex, timeout_thresh);
        else
            dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);
        if (dwWaitResult == WAIT_OBJECT_0)
        {
            return true;
        }
        if (dwWaitResult == WAIT_ABANDONED || dwWaitResult == WAIT_FAILED)
        {
            SLANG_LOG << "getMutexInfinite: dWaitResult:" << dwWaitResult << std::endl;
            return false;
        }
        if (sleepThread)
            Sleep(sleepProcesMgmtThreads); // sleep for 10ms for the other thread to
                                           // make it work
    }
    return false;
}

inline DWORD getMutexState()
{
    // int timeout = 50;
    SLANG_LOG << "Inside infinited mutex thread id:" << GetCurrentThreadId() << "for mutex" << ghMutex << std::endl;
    assert(ghMutex);
    return WaitForSingleObject(ghMutex, 0);
    return false;
}

inline bool releaseMutex()
{
    // std::cout << "Inside release mutex, thread id:" << GetCurrentThreadId() <<
    // "for mutex" << ghMutex << std::endl;

    return ReleaseMutex(ghMutex);
}

SlangResult waitforSpawnthreadSignal(bool sleepThread)
{
    assert(ghSemaphore);
    // int timeout = 50;
    while (true)
    { //&& timeout >= 0
        DWORD dwWaitResult = WaitForSingleObject(ghSemaphore, 0L);
        if (dwWaitResult == WAIT_OBJECT_0)
        {
            // semaphore got signaled we can continue
            return true;
        }
        if (sleepThread)
            Sleep(sleepProcesMgmtThreads);
        // sleep for 10ms for the other thread to
        // make it work
        // timeout--;
    }
    return false;
}

DWORD WINAPI spawinAndWaitTestServerThread(LPVOID lpParam)
{
    // std::cout << "Inside thread" << std::endl;
    do
    {
        tstate = ethread_state_start;
        ReleaseSemaphore(ghSemaphore, 1, NULL); // release the semahore
        SLANG_LOG << "spawinAndWaitTestServerThread: #1 thread is active" << std::endl;
        if (!getMutexInfinite(false))
        {
            SLANG_LOG << "#1 spawinAndWaitTestServerThread: Failed to acquire mutex" << std::endl;
            tstate = ethread_state_exit;
            return -1;
        }
        SLANG_LOG << "#2 spawinAndWaitTestServerThread: Thread launching "
                     "test-server "
                  << std::endl;
        if (g_slangContext.spawinAndWaitTestServer() == SLANG_OK)
        {
            SLANG_LOG << "#3 spawinAndWaitTestServerThread: thread "
                         "succeded to launch server "
                      << std::endl;
            tstate       = ethread_state_alive;
            g_hasProcess = true;
            releaseMutex();
            HANDLE prochandle = HANDLE(m_process->m_processHandle);
            WaitForSingleObject(prochandle, INFINITE);
        }
        else
        {
            SLANG_LOG << "#4 spawinAndWaitTestServerThread: thread failed "
                         "to launch test-server "
                      << std::endl;
            tstate = ethread_state_exit;
            releaseMutex();
            return -1;
        }

        SLANG_LOG << "#5 spawinAndWaitTestServerThread: thread state "
                     "before is getting killed:"
                  << tstate << " thread id:" << GetCurrentThreadId() << " threadHandle:" << GetCurrentThread()
                  << std::endl;
        if (getMutexState() == WAIT_TIMEOUT)
        {
            SLANG_LOG << "#6 spawinAndWaitTestServerThread: spwaned "
                         "process killed because it was hung, thread id : "
                      << GetCurrentThreadId() << std::endl;

            tstate = ethread_state_exit;
            releaseMutex();
            CloseHandle(ghMutex);
            ghMutex      = NULL;
            g_hasProcess = false;
            return 0;
        }
        SLANG_LOG << "#7 spawinAndWaitTestServerThread: thread state "
                     "before is getting killed:"
                  << tstate << " thread id:" << GetCurrentThreadId() << " threadHandle:" << GetCurrentThread()
                  << std::endl;
        if (!getMutexInfinite(false))
        {
            tstate = ethread_state_exit;
            SLANG_LOG << "#8 spawinAndWaitTestServerThread: Failed to acquire mutex" << std::endl;
            return -1;
        }
        SLANG_LOG << "#9 spawinAndWaitTestServerThread: thread after taking mutex" << GetCurrentThreadId() << std::endl;
        CloseHandle(m_process->m_processHandle);
        g_slangContext.KillProcessAndResetDS();
        g_hasProcess = false;
        tstate       = ethread_state_exit;
        releaseMutex();
        CloseHandle(ghMutex);
        ghMutex = NULL;
        SLANG_LOG << "#10 spawinAndWaitTestServerThread: thread after exiting mutex" << GetCurrentThreadId()
                  << std::endl;
    } while (false);
    return 0;
}

void flushtestserverpipes()
{
    if (m_readStream)
    {
        char content[HEADER_BUFF_MAX_SIZE];
        size_t contentSize = 0;
        SlangResult res    = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
        while (contentSize > 0 && res == SLANG_OK)
        {
            res = m_readStream->read(content, HEADER_BUFF_MAX_SIZE, contentSize);
        }
    }
}

SlangResult spawnThreadForTestServer()
{
    do
    {
        if (!ghMutex)
        {
            ghMutex = CreateMutex(NULL,  // default security attributes
                                  FALSE, // initially not owned
                                  NULL); // unnamed mutex
            if (ghMutex == NULL)
            {
                SLANG_LOG << "failed to create mutex for test-server" << std::endl;
                return -1;
            }
        }
        // std::cout << "#1: spawnThreadForTestServer: tstate:" << tstate <<
        // std::endl;
        if (tstate == ethread_state_exit)
        {
            ghSemaphore = CreateSemaphore(NULL,  // default security attributes
                                          0,     // initial count, this means the semaphor is not in signal state
                                          1,     // maximum count
                                          NULL); // unnamed semaphore

            if (ghSemaphore == NULL)
            {
                printf("CreateSemaphore error: %d\n", GetLastError());
                return -1;
            }

            // This means process has exited, created a new thread
            hProcessMgmtThread = CreateThread(NULL,                          // default security attributes
                                              0,                             // use default stack size
                                              spawinAndWaitTestServerThread, // thread function name
                                              NULL,                          // argument to thread function
                                              0,                             // use default creation flags
                                              &dwThreadId);                  // returns the thread identifier
            if (hProcessMgmtThread == NULL)
            {
                printf("CreateThread error: %d\n", GetLastError());
                return -1;
            }

            if (!waitforSpawnthreadSignal(true))
            {
                printf("Semaphore was never signalled error: %d\n", GetLastError());
                // TerminateThread(hProcessMgmtThread);
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
				SLANG_LOG << "#7: Failed to acquire mutex" << std::endl;
				return -1;
			}
			flushtestserverpipes();
			releaseMutex();
		}
#endif
        if (tstate == ethread_state_exit)
        {
            SLANG_LOG << "#1 spawnThreadForTestServer: Failed to spawn "
                         "server from the thread and the thread is dead"
                      << std::endl;
            WaitForSingleObject(hProcessMgmtThread, threadDiedWaitMS);
            return -1;
        }
        else
        {
            while (tstate != ethread_state_alive)
            {
                if (!getMutexInfinite(true))
                {
                    SLANG_LOG << "#2 spawnThreadForTestServer: : Failed to "
                                 "acquire mutex"
                              << std::endl;
                    return -1;
                }
                if (tstate == ethread_state_start)
                {
                    // sever thread is in process of starting test-server
                    // release the mutex and wait for updates
                    releaseMutex(); // and poll again
                }
                else if (tstate == ethread_state_exit)
                {
                    CloseHandle(ghMutex);
                    ghMutex            = NULL;
                    hProcessMgmtThread = NULL;
                    releaseMutex(); // and poll again
                    SLANG_LOG << "#3 spawnThreadForTestServer: Worker thread failed to "
                                 "spawn the test-server and has exited"
                              << std::endl;
                    return -1;
                }
                else
                {
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

bool compileShaderToSpirV(const std::vector<std::string> *sources, const ShaderBuildOptions &buildOptions,
                          const ShaderLanguage shaderLanguage, std::vector<uint32_t> *dst,
                          glu::ShaderProgramInfo *buildInfo)
{
    return SLANG_OK ==
           Slang::g_slangContext.setupSlangLikeSlangc(sources, buildOptions, shaderLanguage, dst, buildInfo);
}

} // namespace Slang

#endif // #if defined(ENABLE_SLANG_COMPILATION) && defined(_WIN32)
