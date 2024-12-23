/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Postmortem Tests Utils
 *//*--------------------------------------------------------------------*/

#include "vktPostmortemTestsUtils.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include <sstream>
#include <vector>

#define ARRAY_LENGTH(a_) std::extent<decltype(a_)>::value

namespace vkt
{
namespace postmortem
{
struct Header : vk::VkDeviceFaultVendorBinaryHeaderVersionOneEXT
{
    char applicationName[32];
    char engineName[32];
    Header()
    {
        headerSize    = sizeof(vk::VkDeviceFaultVendorBinaryHeaderVersionOneEXT);
        headerVersion = vk::VK_DEVICE_FAULT_VENDOR_BINARY_HEADER_VERSION_ONE_EXT;
        vendorID      = 0x9876;
        deviceID      = 0x5432;
        driverVersion = VK_MAKE_VERSION(3, 4, 5);
        deMemcpy(pipelineCacheUUID, this, sizeof(pipelineCacheUUID));
        applicationNameOffset = uint32_t(sizeof(vk::VkDeviceFaultVendorBinaryHeaderVersionOneEXT));
        applicationVersion    = VK_MAKE_API_VERSION(1, 7, 3, 11);
        engineNameOffset      = uint32_t(applicationNameOffset + sizeof(applicationName));

        strcpy(applicationName, "application.exe");
        strcpy(engineName, "driver.so.3.4.5");
    }
};

vk::VkResult getDeviceFaultInfoKHR(vk::VkDevice, vk::VkDeviceFaultCountsEXT *pFaultCounts,
                                   vk::VkDeviceFaultInfoEXT *pFaultInfo)
{
    static std::vector<vk::VkDeviceFaultAddressInfoEXT> addressInfos;
    static std::vector<vk::VkDeviceFaultVendorInfoEXT> vendorInfos;
    static vk::VkDeviceFaultAddressTypeEXT addressTypes[]{
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT,
        vk::VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT,
    };
    static vk::VkDeviceSize addressPrecisions[]{2, 4, 8, 16};
    static uint64_t vendorFaultCodes[]{0x11223344, 0x22334455, 0xAABBCCDD, 0xCCDDEEFF};
    static Header vendorBinaryData;

    if (nullptr == pFaultInfo)
    {
        if (nullptr == pFaultCounts)
            return vk::VK_ERROR_UNKNOWN;

        DE_ASSERT(pFaultCounts->sType == vk::VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);

        pFaultCounts->vendorBinarySize = sizeof(Header);
        pFaultCounts->vendorInfoCount  = 2;
        pFaultCounts->addressInfoCount = 2;
    }
    else
    {
        DE_ASSERT(pFaultCounts);
        DE_ASSERT(pFaultCounts->sType == vk::VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);
        DE_ASSERT(pFaultInfo->sType == vk::VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT);

        if (pFaultCounts->addressInfoCount && pFaultInfo->pAddressInfos)
        {
            vk::VkDeviceAddress deviceAddress = 1024;
            addressInfos.resize(pFaultCounts->addressInfoCount);
            for (uint32_t i = 0; i < pFaultCounts->addressInfoCount; ++i)
            {
                vk::VkDeviceFaultAddressInfoEXT &info = addressInfos[i];
                info.addressType                      = addressTypes[i % ARRAY_LENGTH(addressTypes)];
                info.addressPrecision                 = addressPrecisions[i % ARRAY_LENGTH(addressPrecisions)];
                info.reportedAddress                  = deviceAddress;
                deviceAddress <<= 1;

                pFaultInfo->pAddressInfos[i] = info;
            }
        }

        if (pFaultCounts->vendorInfoCount && pFaultInfo->pVendorInfos)
        {
            vendorInfos.resize(pFaultCounts->vendorInfoCount);
            for (uint32_t i = 0; i < pFaultCounts->vendorInfoCount; ++i)
            {
                vk::VkDeviceFaultVendorInfoEXT &info = vendorInfos[i];
                info.vendorFaultCode                 = vendorFaultCodes[i % ARRAY_LENGTH(vendorFaultCodes)];
                info.vendorFaultData                 = (i + 1) % ARRAY_LENGTH(vendorFaultCodes);
                deMemset(info.description, 0, sizeof(info.description));

                std::stringstream s;
                s << "VendorFaultDescription" << info.vendorFaultData;
                s.sync();
                const auto &str = s.str();
                deMemcpy(info.description, str.c_str(), str.length());

                pFaultInfo->pVendorInfos[i] = info;
            }
        }

        if (pFaultCounts->vendorBinarySize && pFaultInfo->pVendorBinaryData)
        {
            DE_ASSERT(pFaultCounts->vendorBinarySize >= sizeof(vk::VkDeviceFaultVendorBinaryHeaderVersionOneEXT));
            deMemcpy(pFaultInfo->pVendorBinaryData, &vendorBinaryData,
                     deMaxu32(sizeof(Header), uint32_t(pFaultCounts->vendorBinarySize)));
        }
    }

    return vk::VK_SUCCESS;
}

} // namespace postmortem
} // namespace vkt