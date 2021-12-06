/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
* \brief Cover for non-zero of memoryTypeBits from vkGetBufferMemoryRequirements*() tests.
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferMemoryRequirementsTests.hpp"
#include "vktApiBufferMemoryRequirementsTestsUtils.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "deFilePath.hpp"
#include "tcuTestLog.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

namespace vkt
{
namespace api
{
namespace
{

using namespace de;
using namespace vk;
using namespace tcu;

struct TestConfig;
struct InstanceConfig;

enum BufferFateFlagBits
{
	Transfer		= 0x01,
	Storage			= 0x02,
	Other			= 0x04,
	AccStructure	= 0x08,
	Video			= 0x10
};
typedef deUint32	BufferFateFlags;
typedef typename std::add_pointer<typename std::add_const<char>::type>::type	cstr;
typedef u::BitsSet<BufferFateFlags, BufferFateFlagBits, cstr> BufferFateBits;

const BufferFateBits	AvailableBufferFateBits
{
	std::make_tuple(Transfer,		"transfer_usage_bits"	),
	std::make_tuple(Storage,		"storage_usage_bits"	),
	std::make_tuple(Other,			"other_usage_bits"		),
	std::make_tuple(AccStructure,	"acc_struct_usage_bits"	),
	std::make_tuple(Video,			"video_usage_bits"		),
};

typedef u::BitsSet<VkBufferCreateFlags, VkBufferCreateFlagBits, cstr>				BufferCreateBits;
typedef u::BitsSet<VkBufferUsageFlags, VkBufferUsageFlagBits, BufferFateFlagBits>	BufferUsageBits;
typedef u::BitsSet<VkExternalMemoryHandleTypeFlags,
					VkExternalMemoryHandleTypeFlagBits, cstr, bool>					ExternalMemoryHandleBits;
typedef SharedPtr<BufferCreateBits>			BufferCreateBitsPtr;
typedef SharedPtr<BufferUsageBits>			BufferUsageBitsPtr;
typedef SharedPtr<ExternalMemoryHandleBits>	ExternalMemoryHandleBitsPtr;

struct TestConfig
{
	bool						useMethod2;
	SharedPtr<BufferCreateBits>	createBits;
	SharedPtr<BufferFateBits>	fateBits;
	bool						incExtMemTypeFlags;
	// Tests the buffer memory size requirement is less than or equal to the aligned size of the buffer.
	// Requires VK_KHR_maintenance4 extension.
	bool						testSizeRequirements;
};
struct InstanceConfig
{
	bool												useMethod2;
	SharedPtr<BufferCreateBits>							createBits;
	SharedPtr<BufferFateBits>							fateBits;
	SharedPtr<std::vector<BufferUsageBitsPtr>>			usageFlags;
	bool												incExtMemTypeFlags;
	SharedPtr<std::vector<ExternalMemoryHandleBitsPtr>>	extMemHandleFlags;
	bool												testSizeRequirements;

	InstanceConfig(const TestConfig& conf)
		: useMethod2			(conf.useMethod2)
		, createBits			(conf.createBits)
		, fateBits				(conf.fateBits)
		, usageFlags			(new std::vector<SharedPtr<BufferUsageBits>>)
		, incExtMemTypeFlags	(conf.incExtMemTypeFlags)
		, extMemHandleFlags		(new std::vector<SharedPtr<ExternalMemoryHandleBits>>)
		, testSizeRequirements	(conf.testSizeRequirements) {}
};

const BufferCreateBits	AvailableBufferCreateBits
{
	std::make_tuple(VkBufferCreateFlagBits(0),				"no_flags"			),
	std::make_tuple(VK_BUFFER_CREATE_PROTECTED_BIT,			"protected"			),
	std::make_tuple(VK_BUFFER_CREATE_SPARSE_BINDING_BIT,	"sparse_binding"	),
	std::make_tuple(VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,	"sparse_residency"	),
	std::make_tuple(VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,	"sparse_aliased"	),
};

const BufferUsageBits	AvailableBufferUsageBits
{
	std::make_tuple(VK_BUFFER_USAGE_TRANSFER_SRC_BIT										, Transfer		),
	std::make_tuple(VK_BUFFER_USAGE_TRANSFER_DST_BIT										, Transfer		),
	std::make_tuple(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT								, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT								, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT										, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT										, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_INDEX_BUFFER_BIT										, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT										, Storage		),
	std::make_tuple(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT										, Other			),
	std::make_tuple(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT								, Other			),
	std::make_tuple(VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR								, Video			),
	std::make_tuple(VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR								, Video			),
	std::make_tuple(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT						, Other			),
	std::make_tuple(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT				, Other			),
	std::make_tuple(VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT							, Other			),
	std::make_tuple(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR	, AccStructure	),
	std::make_tuple(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR					, AccStructure	),
	std::make_tuple(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR							, AccStructure	),
	std::make_tuple(VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR								, Video			),
	std::make_tuple(VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR								, Video			),
};

#define INTERNALTEST_EXTERNAL_MEMORY_HANDLE_TYPE_NO_BITS VkExternalMemoryHandleTypeFlagBits(0)
const ExternalMemoryHandleBits	AvailableExternalMemoryHandleBits
{
	std::make_tuple(INTERNALTEST_EXTERNAL_MEMORY_HANDLE_TYPE_NO_BITS					, "no_flags",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT						, "opaque_fd",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT						, "opaque_win32",		false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT					, "opaque_win32_kmt",	false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT					, "d3d11_tex",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT				, "d3d11_tex_kmt",		false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT						, "d3d12_heap",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT					, "d3d12_rsrc",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT						, "dma_buf",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID	, "android_hw",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT				, "host_alloc",			true  ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT	, "host_mapped",		true  ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA				, "zircon_vmo",			false ),
	std::make_tuple(VK_EXTERNAL_MEMORY_HANDLE_TYPE_RDMA_ADDRESS_BIT_NV					, "roma_addr",			false ),
};

template<class Flag, class Bit, class Str, class... Ignored>
std::string bitsToString (const u::BitsSet<Flag, Bit, Str, Ignored...>& bits,
						  const std::string& prefix = std::string())
{
	DE_ASSERT(!bits.empty());
	std::stringstream s;
	s << prefix;
	bool atLeastOne = false;
	for (const auto& bit : bits) {
		if (atLeastOne) s << '_';
		s << std::get<1>(bit);
		atLeastOne = true;
	}
	return s.str();
}

void updateBufferCreateFlags(std::vector<BufferCreateBits>& flags)
{
	const auto&	residencyBit	= AvailableBufferCreateBits.get(VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT);
	const auto&	aliasedBit		= AvailableBufferCreateBits.get(VK_BUFFER_CREATE_SPARSE_ALIASED_BIT);
	const auto&	bindingBit		= AvailableBufferCreateBits.get(VK_BUFFER_CREATE_SPARSE_BINDING_BIT);
	const auto&	protectedBit	= AvailableBufferCreateBits.get(VK_BUFFER_CREATE_PROTECTED_BIT);
	const auto&	noneBit			= AvailableBufferCreateBits.get(VkBufferCreateFlagBits(0));

	// VUID-VkBufferCreateInfo-flags-00918 { if sparse residency or sparse aliased include sparse binding }
	for (auto& bits : flags)
	{
		if (bits.contains(residencyBit) || bits.contains(aliasedBit))
			bits.insert(bindingBit);
	}

	// VUID-VkBufferCreateInfo-None-01888 { if sparse residency, sparse aliased or sparse binding then flags must not include protected }
	const typename BufferCreateBits::key_type disallowdBits[] { residencyBit, aliasedBit, bindingBit };
	for (auto i = flags.begin(); i != flags.end();)
	{
		auto& bits = *i;
		if (bits.contains(protectedBit))
		{
			for (const auto& disallowdBit : disallowdBits)
			{
				auto find = bits.find(disallowdBit);
				if (find != bits.end())
					bits.erase(find);
			}
		}
		i = bits.empty() ? flags.erase(i) : std::next(i);
	}

	// since 0 is a valid VkBufferCreateFlagBits flag then remove it flags where it exists along with other non-zero flags
	for (auto i = flags.begin(); i != flags.end(); ++i)
	{
		auto& bits = *i;
		auto find = bits.find(noneBit);
		if (find != bits.end() && bits.size() > 1)
		{
			bits.erase(find);
		}
	}

	// remove duplicates
	for (auto i = flags.begin(); i != flags.end(); ++i)
	{
		for (auto j = std::next(i); j != flags.end();)
			j = (*i == *j) ? flags.erase(j) : std::next(j);
	}
}

class BufferMemoryRequirementsInstance : public TestInstance
{
public:
							BufferMemoryRequirementsInstance	(Context&				context,
																 const InstanceConfig	config)
								: TestInstance	(context)
								, m_config		(config) {}

	virtual					~BufferMemoryRequirementsInstance	(void) override = default;
	virtual tcu::TestStatus	iterate								(void) override;

	void						getBufferMemoryRequirements		(VkMemoryRequirements&	result,
																 const DeviceInterface&	vkd,
																 VkDevice				device,
																 VkBuffer				buffer) const;
	void						getBufferMemoryRequirements2	(VkMemoryRequirements&	result,
																 const DeviceInterface&	vkd,
																 VkDevice				device,
																 VkBuffer				buffer) const;
	typedef void (BufferMemoryRequirementsInstance::* Method)	(VkMemoryRequirements&	result,
																 const DeviceInterface&	intf,
																 VkDevice				device,
																 VkBuffer				buffer) const;
	template<class T, class... AddArgs>
						void*	chainVkStructure				(void*					pNext,
																 const AddArgs&...		addArgs) const;
private:
	void						logFailedSubtests				(const std::vector<BufferCreateBitsPtr>&			failCreateBits,
																 const std::vector<BufferUsageBitsPtr>&				failUsageBits,
																 const std::vector<ExternalMemoryHandleBitsPtr>&	failExtMemHandleBits) const;
	const InstanceConfig	m_config;
};

class MemoryRequirementsTest : public TestCase
{
public:
							MemoryRequirementsTest	(TestContext&			testCtx,
													 const std::string&		name,
													 const TestConfig		testConfig)
								: TestCase		(testCtx, name, std::string())
								, m_testConfig	(testConfig)
								, m_instConfig	(testConfig) {}

	virtual					~MemoryRequirementsTest	(void) override = default;
	virtual void			checkSupport			(Context&				context) const override;
	virtual TestInstance*	createInstance			(Context&				context) const override
	{
		return new BufferMemoryRequirementsInstance(context, m_instConfig);
	}

private:
	const TestConfig	m_testConfig;
	InstanceConfig		m_instConfig;
};

struct Info
{
	enum Type {
		Create,
		Usage
	}					m_type;
	std::ostringstream	m_str;
	cstr				m_file;
	int					m_line;
	template<class Msg>	Info(Type type, const Msg& msg, cstr file, int line)
		: m_type(type), m_str(), m_file(file), m_line(line) { m_str << msg; }
	friend std::ostringstream& operator<<(std::ostringstream& str, const Info& info) {
		switch (info.m_type) {
		case Create:
			str << "  Info (Create buffer with " << info.m_str.str() << " not supported by device at "
				<< de::FilePath(info.m_file).getBaseName() << ":" << info.m_line << ")";
			break;
		case Usage:
			str << "  Info (Create buffer with " << info.m_str.str() << " not supported by device at "
				<< de::FilePath(info.m_file).getBaseName() << ":" << info.m_line << ")";
			break;
		}
		return str;
	}
};
#define INFOCREATE(msg_) Info(Info::Create, (msg_), __FILE__, __LINE__)
#define INFOUSAGE(msg_) Info(Info::Usage, (msg_), __FILE__, __LINE__)

#ifndef VK_KHR_VIDEO_QUEUE_EXTENSION_NAME
#define VK_KHR_VIDEO_QUEUE_EXTENSION_NAME "VK_KHR_video_queue"
#endif

#ifndef VK_EXT_VIDEO_ENCODE_H264_EXTENSION_NAME
#define VK_EXT_VIDEO_ENCODE_H264_EXTENSION_NAME "VK_EXT_video_encode_h264"
#endif

#ifndef VK_EXT_VIDEO_DECODE_H264_EXTENSION_NAME
#define VK_EXT_VIDEO_DECODE_H264_EXTENSION_NAME "VK_EXT_video_decode_h264"
#endif

VkVideoCodecOperationFlagsKHR readVideoCodecOperationFlagsKHR (const InstanceInterface& vki, const VkPhysicalDevice& device)
{
	uint32_t	queueFamilyPropertyCount = 0;
	vki.getPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropertyCount, nullptr);
	DE_ASSERT(queueFamilyPropertyCount);

	std::vector<VkVideoQueueFamilyProperties2KHR>	videoQueueFamilyProperties(
														queueFamilyPropertyCount,
														{
														   VK_STRUCTURE_TYPE_VIDEO_QUEUE_FAMILY_PROPERTIES_2_KHR,	// VkStructureType					sType
														   nullptr,													// void*							pNext
														   0														// VkVideoCodecOperationFlagsKHR	videoCodecOperations
														});
	std::vector<VkQueueFamilyProperties2>			queueFamilyProperties(
														queueFamilyPropertyCount,
														{
															VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,			// VkStructureType					sType
															nullptr,												// void*							pNext
															{}														// VkQueueFamilyProperties			queueFamilyProperties
														});
	for (auto begin = queueFamilyProperties.begin(), i = begin, end = queueFamilyProperties.end(); i != end; ++i)
	{
		i->pNext = &videoQueueFamilyProperties.data()[std::distance(begin, i)];
	}

	vki.getPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropertyCount, queueFamilyProperties.data());

	VkVideoCodecOperationFlagsKHR	codecOperationFlags = VK_VIDEO_CODEC_OPERATION_INVALID_BIT_KHR;
	for (const VkVideoQueueFamilyProperties2KHR& props : videoQueueFamilyProperties)
	{
		codecOperationFlags |= props.videoCodecOperations;
	}

	return codecOperationFlags;
}

void MemoryRequirementsTest::checkSupport (Context& context) const
{
	const InstanceInterface&						intf				= context.getInstanceInterface();
	const VkPhysicalDevice							physDevice			= context.getPhysicalDevice();

	if (m_testConfig.useMethod2)
		context.requireDeviceFunctionality(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

	VkPhysicalDeviceProtectedMemoryFeatures			protectedMemFeatures
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,	// VkStructureType	sType;
		nullptr,															// void*			pNext;
		VK_FALSE															// VkBool32			protectedMemory;
	};
	VkPhysicalDeviceFeatures2						extFeatures
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,					// VkStructureType			sType;
		&protectedMemFeatures,												// void*					pNext;
		{}																	// VkPhysicalDeviceFeatures	features;
	};
	intf.getPhysicalDeviceFeatures2(physDevice, &extFeatures);

	const VkPhysicalDeviceFeatures&	features					= extFeatures.features;
	const VkBool32&					protectedMemFeatureEnabled	= protectedMemFeatures.protectedMemory;

	// check the creating bits
	{
		std::ostringstream			str;
		bool		notSupported	= false;
		const auto& createBits		= *m_testConfig.createBits;

		if (createBits.contains(VK_BUFFER_CREATE_SPARSE_BINDING_BIT) && (VK_FALSE == features.sparseBinding))
		{
			str << INFOCREATE(getBufferCreateFlagsStr(VK_BUFFER_CREATE_SPARSE_BINDING_BIT));
			notSupported = true;
		}
		if (createBits.contains(VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) && (VK_FALSE == features.sparseResidencyBuffer))
		{
			if (notSupported) str << std::endl;
			str << INFOCREATE(getBufferCreateFlagsStr(VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT));
			notSupported = true;
		}
		if (createBits.contains(VK_BUFFER_CREATE_SPARSE_ALIASED_BIT) && (VK_FALSE == features.sparseResidencyAliased))
		{
			if (notSupported) str << std::endl;
			str << INFOCREATE(getBufferCreateFlagsStr(VK_BUFFER_CREATE_SPARSE_ALIASED_BIT));
			notSupported = true;
		}
		if (createBits.contains(VK_BUFFER_CREATE_PROTECTED_BIT) && (VK_FALSE == protectedMemFeatureEnabled))
		{
			if (notSupported) str << std::endl;
			str << INFOCREATE(getBufferCreateFlagsStr(VK_BUFFER_CREATE_PROTECTED_BIT));
			notSupported = true;
		}
		if (notSupported)
		{
			std::cout << str.str() << std::endl;
			TCU_THROW(NotSupportedError, "One or more create buffer flags not supported by device");
		}
	}

	// check the usage bits and build instance input
	{
		std::vector<BufferUsageBits>	usageFlags;
		for (const auto& bit : *m_testConfig.fateBits)
		{
			auto fate = m_testConfig.fateBits->extract(bit);
			std::vector<VkBufferUsageFlags>		usageHints;
			std::vector<BufferUsageBits>		usageFlagsTmp;
			u::combine(usageFlagsTmp, AvailableBufferUsageBits.select<1>(fate), usageHints);
			u::mergeFlags(usageFlags, usageFlagsTmp);
		}

		std::ostringstream str;
		std::array<bool, 7> msgs;
		bool notSupported	= false;
		int  entryCount		= 0;
		msgs.fill(false);

		for (auto i = usageFlags.begin(); i != usageFlags.end();)
		{
			notSupported = false;

			if (i->any({VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
					   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR})
					&& !context.isDeviceFunctionalitySupported("VK_KHR_acceleration_structure"))
			{
				if (!msgs[0])
				{
					if (entryCount++) str << std::endl;
					str << INFOUSAGE("VK_KHR_acceleration_structure not supported by device");
					msgs[0] = true;
				}
				notSupported = true;
			}

			if (i->contains(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
					&& !context.isBufferDeviceAddressSupported())
			{
				if (!msgs[1])
				{
					if (entryCount++) str << std::endl;
					str << INFOUSAGE("VK_EXT_buffer_device_address not supported by device");
					msgs[1] = true;
				}
				notSupported = true;
			}

			if (i->any({VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
						VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR, VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR}))
			{
				if (!context.isDeviceFunctionalitySupported(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME))
				{
					if (!msgs[2])
					{
						if (entryCount++) str << std::endl;
						str << INFOUSAGE("VK_EXT_video_queue not supported by device");
						msgs[2] = true;
					}
					notSupported = true;
				}
				else
				{
					const VkVideoCodecOperationFlagsKHR videoFlags = readVideoCodecOperationFlagsKHR(intf, physDevice);

					if (i->any({VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR}))
					{
						if (!context.isDeviceFunctionalitySupported(VK_EXT_VIDEO_ENCODE_H264_EXTENSION_NAME))
						{
							if (!msgs[3])
							{
								if (entryCount++) str << std::endl;
								str << INFOUSAGE("VK_EXT_video_encode_h264 not supported by device");
								msgs[3] = true;
							}
							notSupported = true;
						}
						if (!(videoFlags & VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT))
						{
							if (!msgs[4])
							{
								if (entryCount++) str << std::endl;
								str << INFOUSAGE("Could not find a queue that supports VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT on device");
								msgs[4] = true;
							}
							notSupported = true;
						}
					}
					if (i->any({VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR, VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR}))
					{
						if (!context.isDeviceFunctionalitySupported(VK_EXT_VIDEO_DECODE_H264_EXTENSION_NAME))
						{
							if (!msgs[5])
							{
								if (entryCount++) str << std::endl;
								str << INFOUSAGE("VK_EXT_video_decode_h264 not supported by device");
								msgs[5] = true;
							}
							notSupported = true;
						}
						if (!(videoFlags & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT))
						{
							if (!msgs[6])
							{
								if (entryCount++) str << std::endl;
								str << INFOUSAGE("Could not find a queue that supports VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT on device");
								msgs[6] = true;
							}
							notSupported = true;
						}
					}
				}
			}

			i = notSupported ? usageFlags.erase(i) : std::next(i);
		}

		// remove duplicates
		for (auto i = usageFlags.begin(); i != usageFlags.end(); ++i)
		{
			for (auto j = std::next(i); j != usageFlags.end();)
				j = (*i == *j) ? usageFlags.erase(j) : std::next(j);
		}

		if (usageFlags.empty())
		{
			std::cout << str.str() << std::endl;
			TCU_THROW(NotSupportedError, "One or more buffer usage flags not supported by device");
		}
		else
		{
			if (entryCount > 0)
			{
				std::cout << str.str() << std::endl;
			}
			DE_ASSERT(m_instConfig.usageFlags.get());
			m_instConfig.usageFlags->resize(usageFlags.size());
			std::transform(usageFlags.begin(), usageFlags.end(), m_instConfig.usageFlags->begin(),
						   [](BufferUsageBits& bits){ return BufferUsageBits::makeShared(std::move(bits)); });
		}
	}

	// check the external memory handle type bits and build instance input
	{
		std::vector<ExternalMemoryHandleBits>	extMemHandleFlags;
		if (m_testConfig.incExtMemTypeFlags)
			extMemHandleFlags.push_back({AvailableExternalMemoryHandleBits.get(INTERNALTEST_EXTERNAL_MEMORY_HANDLE_TYPE_NO_BITS)});
		else
		{
			std::vector<VkExternalMemoryHandleTypeFlags>	handleHints;
			std::vector<ExternalMemoryHandleBits>			handleFlagsTmp;
			u::combine(handleFlagsTmp, AvailableExternalMemoryHandleBits.select<2>(true), handleHints);
			u::mergeFlags(extMemHandleFlags, handleFlagsTmp);
		}

		DE_ASSERT(m_instConfig.extMemHandleFlags.get());
		m_instConfig.extMemHandleFlags->resize(extMemHandleFlags.size());
		std::transform(extMemHandleFlags.begin(), extMemHandleFlags.end(), m_instConfig.extMemHandleFlags->begin(),
					   [](ExternalMemoryHandleBits& bits){ return ExternalMemoryHandleBits::makeShared(std::move(bits)); });
	}

	if (m_testConfig.testSizeRequirements)
	{
		if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance4"))
			TCU_THROW(NotSupportedError, "VK_KHR_maintenance4 not supported");
	}
}

void BufferMemoryRequirementsInstance::logFailedSubtests (const std::vector<BufferCreateBitsPtr>&			failCreateBits,
														  const std::vector<BufferUsageBitsPtr>&			failUsageBits,
														  const std::vector<ExternalMemoryHandleBitsPtr>&	failExtMemHandleBits) const
{
	const deUint32	flagCount	= deUint32(failCreateBits.size());
	TestLog&		log			= m_context.getTestContext().getLog();
	deUint32		entries		= 0;

	DE_ASSERT(flagCount && flagCount == failUsageBits.size() && flagCount == failExtMemHandleBits.size());

	log << TestLog::Section("Failed", "Failed subtests");

	for (deUint32 i = 0; i < flagCount; ++i)
	{
		{
			log << TestLog::Section("VkBufferCreateFlags", "Buffer create flags");
			auto msg = log << TestLog::Message;
			entries = 0;
			for (const auto& createBit : *failCreateBits[i])
			{
				if (entries++) msg << " ";
				const VkBufferCreateFlags flags = BufferCreateBits::extract(createBit);
				if (flags == 0)
					msg << "0";
				else msg << getBufferCreateFlagsStr(flags);
			}
			msg << TestLog::EndMessage << TestLog::EndSection;
		}

		{
			log << TestLog::Section("VkBufferUsageFlags", "Buffer usage flags");
			auto msg = log << TestLog::Message;
			entries = 0;
			for (const auto& usageBit : *failUsageBits[i])
			{
				if (entries++) msg << " ";
				msg << getBufferUsageFlagsStr(BufferUsageBits::extract(usageBit));
			}
			msg << TestLog::EndMessage << TestLog::EndSection;
		}

		{
			log << TestLog::Section("VkExternalMemoryHandleTypeFlags", "External memory handle type flags");
			auto msg = log << TestLog::Message;
			entries = 0;
			for (const auto& extMemHandleTypeBit : *failExtMemHandleBits[i])
			{
				if (entries++) msg << " ";
				msg << getExternalMemoryHandleTypeFlagsStr(ExternalMemoryHandleBits::extract(extMemHandleTypeBit));
			}
			msg << TestLog::EndMessage << TestLog::EndSection;
		}
	}

	log << TestLog::EndSection;
}

void BufferMemoryRequirementsInstance::getBufferMemoryRequirements2	(VkMemoryRequirements&	result,
																	 const DeviceInterface&	vkd,
																	 VkDevice				device,
																	 VkBuffer				buffer) const
{
	VkMemoryDedicatedRequirements	dedicatedRequirements	=
	{
		VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,		// VkStructureType			sType;
		nullptr,												// const void*				pNext;
		VK_FALSE,												// VkBool32					prefersDedicatedAllocation
		VK_FALSE												// VkBool32					requiresDedicatedAllocation
	};

	VkMemoryRequirements2			desiredRequirements		=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,				// VkStructureType			sType
		&dedicatedRequirements,									// void*					pNext
		result													// VkMemoryRequirements		memoryRequirements
	};

	VkBufferMemoryRequirementsInfo2	requirementsInfo		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,	// VkStructureType			sType
		nullptr,												// const void*				pNext
		buffer													// VkBuffer					buffer
	};

	vkd.getBufferMemoryRequirements2(device, &requirementsInfo, &desiredRequirements);

	result = desiredRequirements.memoryRequirements;
}

void BufferMemoryRequirementsInstance::getBufferMemoryRequirements	(VkMemoryRequirements&	result,
																	 const DeviceInterface&	vkd,
																	 VkDevice				device,
																	 VkBuffer				buffer) const
{
	vkd.getBufferMemoryRequirements(device, buffer, &result);
}

template<> void*
BufferMemoryRequirementsInstance::chainVkStructure<VkExternalMemoryBufferCreateInfo> (void* pNext, const VkExternalMemoryHandleTypeFlags& handleTypes) const
{
	static VkExternalMemoryBufferCreateInfo	memInfo{};
	memInfo.sType		= VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	memInfo.pNext		= pNext;
	memInfo.handleTypes	= handleTypes;

	return &memInfo;
}

template<> void* BufferMemoryRequirementsInstance::chainVkStructure<VkVideoProfilesKHR> (void* pNext, const VkBufferUsageFlags& videoCodecUsage) const
{
	const bool encode = (videoCodecUsage & VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR) || (videoCodecUsage & VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR);
	const bool decode = (videoCodecUsage & VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR) || (videoCodecUsage & VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR);

	static VkVideoEncodeH264ProfileEXT	encodeProfile
	{
		VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_EXT,		// VkStructureType						sType;
		nullptr,												// const void*							pNext;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE						// StdVideoH264ProfileIdc				stdProfileIdc;
	};

	static VkVideoDecodeH264ProfileEXT	decodeProfile
	{
		VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_EXT,		// VkStructureType						sType;
		nullptr,												// const void*							pNext;
		STD_VIDEO_H264_PROFILE_IDC_BASELINE,					// StdVideoH264ProfileIdc				stdProfileIdc;
		VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_EXT		// VkVideoDecodeH264FieldLayoutFlagsEXT	fieldLayout;
	};

	static const VkVideoProfileKHR	videoProfiles[]
	{
		// encode profile
		{
			VK_STRUCTURE_TYPE_VIDEO_PROFILE_KHR,				// VkStructureType						sType;
			&encodeProfile,										// void*								pNext;
			VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT,		// VkVideoCodecOperationFlagBitsKHR		videoCodecOperation;
			VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,		// VkVideoChromaSubsamplingFlagsKHR		chromaSubsampling;
			VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,				// VkVideoComponentBitDepthFlagsKHR		lumaBitDepth;
			VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR				// VkVideoComponentBitDepthFlagsKHR		chromaBitDepth;
		},
		// decode profile
		{
			VK_STRUCTURE_TYPE_VIDEO_PROFILE_KHR,				// VkStructureType						sType;
			&decodeProfile,										// void*								pNext;
			VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_EXT,		// VkVideoCodecOperationFlagBitsKHR		videoCodecOperation;
			VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,		// VkVideoChromaSubsamplingFlagsKHR		chromaSubsampling;
			VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,				// VkVideoComponentBitDepthFlagsKHR		lumaBitDepth;
			VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR				// VkVideoComponentBitDepthFlagsKHR		chromaBitDepth;
		}
	};
	static VkVideoProfilesKHR	profiles;
	profiles.sType			= VK_STRUCTURE_TYPE_VIDEO_PROFILES_KHR;
	profiles.pNext			= pNext;
	if (encode && decode)
	{
		profiles.profileCount	= 2u;
		profiles.pProfiles		= videoProfiles;
	}
	else if (encode)
	{
		profiles.profileCount	= 1u;
		profiles.pProfiles		= &videoProfiles[0];
	}
	else
	{
		profiles.profileCount	= 1u;
		profiles.pProfiles		= &videoProfiles[1];
	}
	return &profiles;
}

TestStatus	BufferMemoryRequirementsInstance::iterate (void)
{
	const DeviceInterface&							vkd					= m_context.getDeviceInterface();
	const VkDevice									device				= m_context.getDevice();
	const deUint32									queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const Method									method				= m_config.useMethod2
																			? &BufferMemoryRequirementsInstance::getBufferMemoryRequirements2
																			: &BufferMemoryRequirementsInstance::getBufferMemoryRequirements;

	deUint32										passCount			= 0;
	deUint32										failCount			= 0;
	std::vector<BufferCreateBitsPtr>				failCreateBits;
	std::vector<BufferUsageBitsPtr>					failUsageBits;
	std::vector<ExternalMemoryHandleBitsPtr>		failExtMemHandleBits;

	DE_ASSERT(!m_config.createBits->empty());
	const VkBufferCreateFlags infoCreateFlags = *m_config.createBits;
	{
		DE_ASSERT(!m_config.usageFlags->empty());
		for (auto u = m_config.usageFlags->cbegin(); u != m_config.usageFlags->cend(); ++u)
		{
			const VkBufferUsageFlags infoUsageFlags = *(u->get());

			DE_ASSERT(!m_config.extMemHandleFlags->empty());
			for (auto m = m_config.extMemHandleFlags->cbegin(); m != m_config.extMemHandleFlags->cend(); ++m)
			{
				const VkExternalMemoryHandleTypeFlags handleFlags = *(m->get());

				void* pNext = nullptr;

				if (m_config.fateBits->contains(BufferFateFlagBits::Video))
				{
					pNext = chainVkStructure<VkVideoProfilesKHR>(pNext, infoUsageFlags);
				}
				if (m_config.incExtMemTypeFlags)
				{
					pNext = chainVkStructure<VkExternalMemoryBufferCreateInfo>(pNext, handleFlags);
				}
				VkBufferCreateInfo	createInfo
				{
					VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,					// VkStructureType					sType;
					pNext,													// const void*						pNext;
					infoCreateFlags,										// VkBufferCreateFlags				flags;
					4096u,													// VkDeviceSize						size;
					infoUsageFlags,											// VkBufferUsageFlags				usage;
					VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode					sharingMode;
					1u,														// uint32_t							queueFamilyIndexCount;
					&queueFamilyIndex,										// const uint32_t*					pQueueFamilyIndices;
				};

				if (m_config.testSizeRequirements)
				{
					VkPhysicalDeviceMaintenance4PropertiesKHR	maintenance4Properties		=
					{
						VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES_KHR,	// VkStructureType	sType;
						DE_NULL,														// void*			pNext;
						0u																// VkDeviceSize		maxBufferSize;
					};

					VkPhysicalDeviceProperties2					physicalDeviceProperties2	=
					{
						VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,	// VkStructureType				sType;
						&maintenance4Properties,						// void*						pNext;
						{},												// VkPhysicalDeviceProperties	properties;
					};

					m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &physicalDeviceProperties2);

					const VkDeviceSize							maxBufferSize				= maintenance4Properties.maxBufferSize;
					DE_ASSERT(maxBufferSize > 0);
					VkDeviceSize								N							= 0;

					while ((1ull << N) + 1 < maxBufferSize)
					{
						createInfo.size = (1ull << N) + 1;

						try
						{
							Move<VkBuffer> buffer = createBuffer(vkd, device, &createInfo);

							VkMemoryRequirements reqs{};
							(this->*method)(reqs, vkd, device, *buffer);

							if (reqs.size <= static_cast<VkDeviceSize>(deAlign64(static_cast<deInt64>(createInfo.size), static_cast<deInt64>(reqs.alignment))))
							{
								++passCount;
							} else
							{
								++failCount;
								failCreateBits.emplace_back(m_config.createBits);
								failUsageBits.emplace_back(*u);
								failExtMemHandleBits.emplace_back(*m);
							}

							N++;
						}
						catch (const vk::OutOfMemoryError&)
						{
							break;
						}
					}
				}
				else
				{
					Move<VkBuffer> buffer = createBuffer(vkd, device, &createInfo);

					VkMemoryRequirements reqs{};
					(this->*method)(reqs, vkd, device, *buffer);
					if (reqs.memoryTypeBits)
						++passCount;
					else
					{
						++failCount;
						failCreateBits.emplace_back(m_config.createBits);
						failUsageBits.emplace_back(*u);
						failExtMemHandleBits.emplace_back(*m);
					}
				}
			}
		}
	}

	if (failCount)
	{
		logFailedSubtests(failCreateBits, failUsageBits, failExtMemHandleBits);
		return TestStatus::fail(std::to_string(failCount));
	}

	return TestStatus::pass(std::to_string(passCount));
}

} // unnamed namespace

tcu::TestCaseGroup* createBufferMemoryRequirementsTests (tcu::TestContext& testCtx)
{
	cstr nilstr = "";

	struct
	{
		bool		include;
		cstr		name;
	} const extMemTypeFlags[] { { false, "ext_mem_flags_excluded" }, { true, "ext_mem_flags_included" } };

	struct
	{
		bool		method;
		cstr		name;
	} const methods[] { { false, "method1" }, { true, "method2" } };

	std::vector<SharedPtr<BufferCreateBits>>	createBitPtrs;
	{
		std::vector<VkBufferCreateFlags>		hints;
		std::vector<BufferCreateBits>			createFlags;
		u::combine(createFlags,	AvailableBufferCreateBits, hints);
		updateBufferCreateFlags(createFlags);
		createBitPtrs.resize(createFlags.size());
		std::transform(createFlags.begin(), createFlags.end(), createBitPtrs.begin(),
					   [](BufferCreateBits& bits) { return BufferCreateBits::makeShared(std::move(bits)); });
	}

	std::vector<SharedPtr<BufferFateBits>>	fateBitPtrs;
	{
		// An excerpt above has been disabled consciously for the sake of computational complexity.
		// Enabled block does the same things sequentially, it doesn't create cartesian product of combination of bits.
#if 0
		std::vector<BufferFateFlags>	hints;
		std::vector<BufferFateBits>		bufferFateFlags;
		u::combine(bufferFateFlags, AvailableBufferFateBits, hints);
		fateBitPtrs.resize(bufferFateFlags.size());
		std::transform(bufferFateFlags.begin(), bufferFateFlags.end(), fateBitPtrs.begin(),
					   [](BufferFateBits& bits) { return BufferFateBits::makeShared(std::move(bits)); });
#else
		fateBitPtrs.resize(AvailableBufferFateBits.size());
		std::transform(AvailableBufferFateBits.begin(), AvailableBufferFateBits.end(), fateBitPtrs.begin(),
					   [](const typename BufferFateBits::value_type& bit) { return BufferFateBits::makeShared(bit); });
#endif
	}

	auto groupRoot = new TestCaseGroup(testCtx, "buffer_memory_requirements", "vkGetBufferMemoryRequirements*(...) routines tests.");
	for (const auto& createBits : createBitPtrs)
	{
		auto groupCreate = new TestCaseGroup(testCtx, bitsToString(*createBits, "create_").c_str(), nilstr);
		for (const auto& extMemTypeFlag : extMemTypeFlags)
		{
			auto groupExtMemTypeFlags = new TestCaseGroup(testCtx, extMemTypeFlag.name, nilstr);
			for (const auto& method : methods)
			{
				auto groupMethod = new TestCaseGroup(testCtx, method.name, nilstr);
				for (const auto& fateBits : fateBitPtrs)
				{
					for (const auto testSizeReq : {false, true})
					{
						TestConfig	config;
						config.fateBits				= fateBits;
						config.incExtMemTypeFlags	= extMemTypeFlag.include;
						config.createBits			= createBits;
						config.useMethod2			= method.method;
						config.testSizeRequirements	= testSizeReq;
						groupMethod->addChild(new MemoryRequirementsTest(testCtx, ((testSizeReq ? "size_req_" : "") + bitsToString(*fateBits)).c_str(), config));
					}
				}
				groupExtMemTypeFlags->addChild(groupMethod);
			}
			groupCreate->addChild(groupExtMemTypeFlags);
		}
		groupRoot->addChild(groupCreate);
	}

	return groupRoot;
}
} // api
} // vkt
