/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Video Session NV Utils
 *//*--------------------------------------------------------------------*/

#include "vktVideoSessionNvUtils.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "tcuPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deMemory.h"

#include "extNvidiaVideoParserIf.hpp"

namespace vkt
{
namespace video
{
#if DE_OS == DE_OS_WIN32
	const char* CreateVulkanVideoDecodeParserFuncName = "?CreateVulkanVideoDecodeParser@@YA_NPEAPEAVVulkanVideoDecodeParser@@W4VkVideoCodecOperationFlagBitsKHR@@PEBUVkExtensionProperties@@P6AXPEBDZZH@Z";
#else
	const char* CreateVulkanVideoDecodeParserFuncName = "_Z29CreateVulkanVideoDecodeParserPP23VulkanVideoDecodeParser32VkVideoCodecOperationFlagBitsKHRPK21VkExtensionPropertiesPFvPKczEi";
#endif

typedef void (*NvidiaParserLogFuncType)(const char* format, ...);
typedef bool (*CreateVulkanVideoDecodeParserFunc)(NvidiaVulkanVideoDecodeParser** ppobj, vk::VkVideoCodecOperationFlagBitsKHR eCompression, const vk::VkExtensionProperties* extensionProperty, NvidiaParserLogFuncType pParserLogFunc, int logLevel);

void NvidiaParserLogFunc (const char* format, ...)
{
	DE_UNREF(format);
}

class ClsVulkanVideoDecodeParser : public IfcVulkanVideoDecodeParser
{
public:
										ClsVulkanVideoDecodeParser	(NvidiaVulkanVideoDecodeParser*		vulkanVideoDecodeParser);
	virtual								~ClsVulkanVideoDecodeParser	();

public:
	// IfcVulkanVideoDecodeParser commands
	virtual bool						initialize					(NvidiaVulkanParserVideoDecodeClient*	nvidiaVulkanParserVideoDecodeClient);
	virtual bool						deinitialize				(void);
	virtual bool						parseByteStream				(deUint8* pData, deInt64 size);

protected:
	NvidiaVulkanVideoDecodeParser*		m_vulkanVideoDecodeParser;
};

ClsVulkanVideoDecodeParser::ClsVulkanVideoDecodeParser	(NvidiaVulkanVideoDecodeParser*		vulkanVideoDecodeParser)
	: m_vulkanVideoDecodeParser				(vulkanVideoDecodeParser)
{
}

ClsVulkanVideoDecodeParser::~ClsVulkanVideoDecodeParser()
{
	if (m_vulkanVideoDecodeParser != DE_NULL)
	{
		m_vulkanVideoDecodeParser->Deinitialize();

		m_vulkanVideoDecodeParser->Release();

		m_vulkanVideoDecodeParser = DE_NULL;
	}
}

bool ClsVulkanVideoDecodeParser::initialize (NvidiaVulkanParserVideoDecodeClient* nvidiaVulkanParserVideoDecodeClient)
{
	DE_ASSERT(m_vulkanVideoDecodeParser != DE_NULL);

	NvidiaVulkanParserInitDecodeParameters	parameters =
	{
		NV_VULKAN_VIDEO_PARSER_API_VERSION,
		nvidiaVulkanParserVideoDecodeClient,
		0,
		0,
		DE_NULL,
		true
	};

	if (m_vulkanVideoDecodeParser->Initialize(&parameters) != vk::VK_SUCCESS)
	{
		TCU_THROW(InternalError, "ClsVulkanVideoDecodeParser->Initialize failed");
	}

	return true;
}

bool ClsVulkanVideoDecodeParser::deinitialize (void)
{
	bool result = true;

	if (m_vulkanVideoDecodeParser != DE_NULL)
	{
		result = m_vulkanVideoDecodeParser->Deinitialize();

		m_vulkanVideoDecodeParser = DE_NULL;
	}

	return result;
}

bool ClsVulkanVideoDecodeParser::parseByteStream (deUint8* pData, deInt64 size)
{
	DE_ASSERT(m_vulkanVideoDecodeParser != DE_NULL);

	int32_t								parsed	= 0;
	NvidiaVulkanParserBitstreamPacket	pkt;

	deMemset(&pkt, 0, sizeof(pkt));

	pkt.nDataLength = static_cast<int32_t>(size);
	pkt.pByteStream	= pData;
	pkt.bEOS		= (pData == DE_NULL || size == 0);

	bool result = m_vulkanVideoDecodeParser->ParseByteStream(&pkt, &parsed);

	return result && (parsed > 0);
}


class NvFunctions: public IfcNvFunctions
{
public:
										NvFunctions							(const vk::Platform&				platform);
	virtual	IfcVulkanVideoDecodeParser*	createIfcVulkanVideoDecodeParser	(VkVideoCodecOperationFlagBitsKHR	codecOperation, const VkExtensionProperties* stdExtensionVersion);

private:
	de::MovePtr<vk::Library>			m_library;
	CreateVulkanVideoDecodeParserFunc	m_createVulkanVideoDecodeParserFunc;
};

NvFunctions::NvFunctions (const vk::Platform& platform)
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
	: m_library	(de::MovePtr<vk::Library>(platform.createLibrary(vk::Platform::LIBRARY_TYPE_VULKAN_VIDEO_DECODE_PARSER, DE_NULL)))
#else
	: m_library	(de::MovePtr<vk::Library>(platform.createLibrary()))
#endif
{
	const tcu::FunctionLibrary& funcsLibrary = m_library->getFunctionLibrary();

	m_createVulkanVideoDecodeParserFunc = reinterpret_cast<CreateVulkanVideoDecodeParserFunc>(funcsLibrary.getFunction(CreateVulkanVideoDecodeParserFuncName));

	if (m_createVulkanVideoDecodeParserFunc == DE_NULL)
		TCU_THROW(InternalError, string("Function not found in library: ") + CreateVulkanVideoDecodeParserFuncName);
}

IfcVulkanVideoDecodeParser* NvFunctions::createIfcVulkanVideoDecodeParser (VkVideoCodecOperationFlagBitsKHR codecOperation, const VkExtensionProperties* stdExtensionVersion)
{
	DE_ASSERT(m_createVulkanVideoDecodeParserFunc != DE_NULL);

	NvidiaVulkanVideoDecodeParser* pobj = DE_NULL;

	if (!m_createVulkanVideoDecodeParserFunc(&pobj, codecOperation, stdExtensionVersion, NvidiaParserLogFunc, 0) || pobj == DE_NULL)
	{
		return DE_NULL;
	}

	return new ClsVulkanVideoDecodeParser(pobj);
}

de::MovePtr<IfcNvFunctions> createIfcNvFunctions (const vk::Platform& platform)
{
	return de::MovePtr<IfcNvFunctions>(new NvFunctions(platform));
}

}	// video
}	// vkt
