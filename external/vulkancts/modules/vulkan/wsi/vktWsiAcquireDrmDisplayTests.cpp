/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Vulkan coverage tests for extension VK_EXT_acquire_drm_display
 *//*--------------------------------------------------------------------*/

#include "vktWsiAcquireDrmDisplayTests.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktTestCase.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuDefs.hpp"
#include "tcuLibDrm.hpp"

#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
#include <fcntl.h>
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

#include <string>
#include <vector>

#if (DE_OS != DE_OS_WIN32)
#include <unistd.h>
#endif

namespace vkt
{
namespace wsi
{
using namespace vk;
using std::string;
using std::vector;
#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
using tcu::LibDrm;
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

#define INVALID_PTR 0xFFFFFFFF

enum DrmTestIndex
{
	DRM_TEST_INDEX_START,
	DRM_TEST_INDEX_GET_DRM_DISPLAY,
	DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_FD,
	DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_CONNECTOR_ID,
	DRM_TEST_INDEX_GET_DRM_DISPLAY_NOT_MASTER,
	DRM_TEST_INDEX_GET_DRM_DISPLAY_UNOWNED_CONNECTOR_ID,
	DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY,
	DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_INVALID_FD,
	DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_NOT_MASTER,
	DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_UNOWNED_CONNECTOR_ID,
	DRM_TEST_INDEX_RELEASE_DISPLAY,
	DRM_TEST_INDEX_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan VK_EXT_acquire_drm_display extension tests
 *//*--------------------------------------------------------------------*/
class AcquireDrmDisplayTestInstance : public TestInstance
{
public:
								AcquireDrmDisplayTestInstance				(Context& context, const DrmTestIndex testId);
	tcu::TestStatus				iterate										(void) override;

private:
	CustomInstance				createInstanceWithAcquireDrmDisplay			(void);

#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
	LibDrm::FdPtr				getDrmFdPtr									(void);
	deUint32					getConnectedConnectorId						(int fd, deUint32 connectorId = 0);
	deUint32					getValidCrtcId								(int fd, deUint32 connectorId);
	bool						isDrmMaster									(int fd);

	// VK_EXT_acquire_drm_display extension tests
	tcu::TestStatus				testGetDrmDisplayEXT						(void);
	tcu::TestStatus				testGetDrmDisplayEXTInvalidFd				(void);
	tcu::TestStatus				testGetDrmDisplayEXTInvalidConnectorId		(void);
	tcu::TestStatus				testGetDrmDisplayEXTNotMaster				(void);
	tcu::TestStatus				testGetDrmDisplayEXTUnownedConnectorId		(void);
	tcu::TestStatus				testAcquireDrmDisplayEXT					(void);
	tcu::TestStatus				testAcquireDrmDisplayEXTInvalidFd			(void);
	tcu::TestStatus				testAcquireDrmDisplayEXTNotMaster			(void);
	tcu::TestStatus				testAcquireDrmDisplayEXTUnownedConnectorId	(void);
	tcu::TestStatus				testReleaseDisplayEXT						(void);

	const LibDrm				m_libDrm;
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

	const CustomInstance		m_instance;
	const InstanceInterface&	m_vki;
	const VkPhysicalDevice		m_physDevice;
	const DrmTestIndex			m_testId;
};

/*--------------------------------------------------------------------*//*!
 * \brief AcquireDrmDisplayTestInstance constructor
 *
 * Initializes AcquireDrmDisplayTestInstance object
 *
 * \param context	Context object
 * \param testId    Enum for which test to run
 *//*--------------------------------------------------------------------*/
AcquireDrmDisplayTestInstance::AcquireDrmDisplayTestInstance (Context& context, const DrmTestIndex testId)
	: TestInstance	(context)
	, m_instance	(createInstanceWithAcquireDrmDisplay())
	, m_vki			(m_instance.getDriver())
	, m_physDevice	(vk::chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_testId		(testId)
{
	DE_UNREF(m_testId);
}

/*--------------------------------------------------------------------*//*!
 * \brief Step forward test execution
 *
 * \return true if application should call iterate() again and false
 *		 if test execution session is complete.
 *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::iterate (void)
{
#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
	switch (m_testId)
	{
		case DRM_TEST_INDEX_GET_DRM_DISPLAY:							return testGetDrmDisplayEXT();
		case DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_FD:					return testGetDrmDisplayEXTInvalidFd();
		case DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_CONNECTOR_ID:		return testGetDrmDisplayEXTInvalidConnectorId();
		case DRM_TEST_INDEX_GET_DRM_DISPLAY_NOT_MASTER:					return testGetDrmDisplayEXTNotMaster();
		case DRM_TEST_INDEX_GET_DRM_DISPLAY_UNOWNED_CONNECTOR_ID:		return testGetDrmDisplayEXTUnownedConnectorId();
		case DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY:						return testAcquireDrmDisplayEXT();
		case DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_INVALID_FD:				return testAcquireDrmDisplayEXTInvalidFd();
		case DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_NOT_MASTER:				return testAcquireDrmDisplayEXTNotMaster();
		case DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_UNOWNED_CONNECTOR_ID:	return testAcquireDrmDisplayEXTUnownedConnectorId();
		case DRM_TEST_INDEX_RELEASE_DISPLAY:							return testReleaseDisplayEXT();
		default:
		{
			DE_FATAL("Impossible");
		}
	}

	TCU_FAIL("Invalid test identifier");
#else
	TCU_THROW(NotSupportedError, "Drm not supported.");
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Create a instance with the VK_EXT_acquire_drm_display extension.
//  *
//  * \return The created instance
//  *//*--------------------------------------------------------------------*/
CustomInstance AcquireDrmDisplayTestInstance::createInstanceWithAcquireDrmDisplay (void)
{
	vector<VkExtensionProperties> supportedExtensions = enumerateInstanceExtensionProperties(m_context.getPlatformInterface(), DE_NULL);
	vector<string> requiredExtensions = {
		"VK_EXT_acquire_drm_display",
		"VK_EXT_direct_mode_display",
	};

	for (const auto& extension : requiredExtensions)
		if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(extension)))
			TCU_THROW(NotSupportedError, "Instance extension not supported.");

	return createCustomInstanceWithExtensions(m_context, requiredExtensions);
}

#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
// /*--------------------------------------------------------------------*//*!
//  * \brief Open a fd for the Drm corresponding to the Vulkan instance
//  *
//  * \return A LibDrm::FdPtr with the int fd.
//  *//*--------------------------------------------------------------------*/
LibDrm::FdPtr AcquireDrmDisplayTestInstance::getDrmFdPtr (void)
{
	VkPhysicalDeviceProperties2			deviceProperties2;
	VkPhysicalDeviceDrmPropertiesEXT	deviceDrmProperties;

	deMemset(&deviceDrmProperties, 0, sizeof(deviceDrmProperties));
	deviceDrmProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
	deviceDrmProperties.pNext = DE_NULL;

	deMemset(&deviceProperties2, 0, sizeof(deviceProperties2));
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &deviceDrmProperties;

	m_vki.getPhysicalDeviceProperties2(m_physDevice, &deviceProperties2);

	if (!deviceDrmProperties.hasPrimary)
		TCU_THROW(NotSupportedError, "No DRM primary device.");

	int numDrmDevices;
	drmDevicePtr* drmDevices = m_libDrm.getDevices(&numDrmDevices);
	const char* drmNode = m_libDrm.findDeviceNode(drmDevices, numDrmDevices, deviceDrmProperties.primaryMajor, deviceDrmProperties.primaryMinor);

	if (!drmNode)
		TCU_THROW(NotSupportedError, "No DRM node.");

	return m_libDrm.openFd(drmNode);
}

/*--------------------------------------------------------------------*//*!
 * \brief Get a connected Drm connector
 *
 * \param fd			A Drm fd to query
 * \param connectorId	If nonzero, a connector to be different from
 * \return The connectorId of the connected Drm connector
 *//*--------------------------------------------------------------------*/
deUint32 AcquireDrmDisplayTestInstance::getConnectedConnectorId (int fd, deUint32 connectorId)
{
	LibDrm::ResPtr res = m_libDrm.getResources(fd);
	if (!res)
		TCU_THROW(NotSupportedError, "Could not get DRM resources.");

	for (int i = 0; i < res->count_connectors; ++i)
	{
		if (connectorId && connectorId == res->connectors[i])
			continue;
		LibDrm::ConnectorPtr conn = m_libDrm.getConnector(fd, res->connectors[i]);

		if (conn && conn->connection == DRM_MODE_CONNECTED) {
			return res->connectors[i];
		}
	}
	return 0;
}

/*--------------------------------------------------------------------*//*!
 * \brief Get a valid Drm crtc for the connector
 *
 * \param fd			A Drm fd to query
 * \param connectorId	The connector that the crtc is valid for
 * \return The crtcId of the valid Drm crtc
 *//*--------------------------------------------------------------------*/
deUint32 AcquireDrmDisplayTestInstance::getValidCrtcId (int fd, deUint32 connectorId)
{
	LibDrm::ResPtr res = m_libDrm.getResources(fd);
	LibDrm::ConnectorPtr conn = m_libDrm.getConnector(fd, connectorId);
	if (!res || !conn)
		TCU_THROW(NotSupportedError, "Could not get DRM resources or connector.");

	for (int i = 0; i < conn->count_encoders; ++i)
	{
		LibDrm::EncoderPtr enc = m_libDrm.getEncoder(fd, conn->encoders[i]);
		if (!enc)
			continue;

		for (int j = 0; j < res->count_crtcs; ++j)
			if (enc->possible_crtcs & (1 << j))
				return res->crtcs[j];
	}
	return 0;
}

/*--------------------------------------------------------------------*//*!
 * \brief Checks if we have Drm master permissions
 *
 * \param fd A Drm fd to query
 * \return True if we have Drm master
 *//*--------------------------------------------------------------------*/
bool AcquireDrmDisplayTestInstance::isDrmMaster (int fd)
{
	/*
	 * Call a drm API that requires master permissions, but with an invalid
	 * value. If we are master it should return -EINVAL, but if we are not
	 * it should return -EACCESS.
	 */
	return m_libDrm.authMagic(fd, 0) != -EACCES;
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests successfully getting a connected Drm display
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testGetDrmDisplayEXT (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error with an invalid Drm fd
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testGetDrmDisplayEXTInvalidFd (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	int invalidFd = open("/", O_RDONLY | O_PATH);
	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, invalidFd, connectorId, &display);
	close(invalidFd);
	if (result != VK_ERROR_UNKNOWN)
		TCU_FAIL("vkGetDrmDisplayEXT failed to return error.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error with an invalid connector id
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testGetDrmDisplayEXTInvalidConnectorId (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	deUint32 invalidConnectorId = connectorId + 1234;
	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, invalidConnectorId, &display);
	if (result != VK_ERROR_UNKNOWN)
		TCU_FAIL("vkGetDrmDisplayEXT failed to return error.");

	if (display != DE_NULL)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display to null.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests successfully getting without master
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testGetDrmDisplayEXTNotMaster (void)
{
	LibDrm::FdPtr masterFdPtr = getDrmFdPtr();
	LibDrm::FdPtr notMasterFdPtr = getDrmFdPtr();
	if (!masterFdPtr || !notMasterFdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	if (*masterFdPtr == *notMasterFdPtr)
		TCU_THROW(NotSupportedError, "Did not open 2 different fd.");
	int fd = *notMasterFdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error with an unowned connector id
//  *
//  * This needs to be run with drm master permissions.
//  * No other drm client can be running, such as X or Wayland.
//  * Then, to run with drm master, either:
//  *   Add your user to the "video" linux group.
//  *   Log in to the virtual tty.
//  *   Run as root.
//  * This also requires 2 physically connected displays.
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testGetDrmDisplayEXTUnownedConnectorId (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	if (!isDrmMaster(fd))
		TCU_THROW(NotSupportedError, "Does not have drm master permissions.");

	deUint32 connectorId		= getConnectedConnectorId(fd);
	deUint32 otherConnectorId	= getConnectedConnectorId(fd, connectorId);
	deUint32 crtcId				= getValidCrtcId(fd, connectorId);
	if (!connectorId || !crtcId || !otherConnectorId || connectorId == otherConnectorId)
		TCU_THROW(NotSupportedError, "Could not find 2 DRM connectors or a crtc.");

	// Lease the first connector, but try to get the other connector.
	deUint32		objects[2] = {connectorId, crtcId};
	LibDrm::FdPtr	leaseFdPtr = m_libDrm.createLease(fd, objects, 2, O_CLOEXEC);
	if (!leaseFdPtr)
		TCU_THROW(NotSupportedError, "Could not lease DRM.");
	int leaseFd = *leaseFdPtr;

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, leaseFd, otherConnectorId, &display);
	if (result != VK_ERROR_UNKNOWN)
		TCU_FAIL("vkGetDrmDisplayEXT failed to return error.");

	if (display != DE_NULL)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display to null.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests successfully acquiring a connected Drm display
//  *
//  * This needs to be run with drm master permissions.
//  * No other drm client can be running, such as X or Wayland.
//  * Then, to run with drm master, either:
//  *   Add your user to the "video" linux group.
//  *   Log in to the virtual tty.
//  *   Run as root.
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testAcquireDrmDisplayEXT (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	if (!isDrmMaster(fd))
		TCU_THROW(NotSupportedError, "Does not have drm master permissions.");

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	result = m_vki.acquireDrmDisplayEXT(m_physDevice, fd, display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkAcquireDrmDisplayEXT failed.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error with an invalid drm fd
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testAcquireDrmDisplayEXTInvalidFd (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	int invalidFd = open("/", O_RDONLY | O_PATH);
	result = m_vki.acquireDrmDisplayEXT(m_physDevice, invalidFd, display);
	close(invalidFd);
	if (result != VK_ERROR_UNKNOWN)
		TCU_FAIL("vkAcquireDrmDisplayEXT failed to return error.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error without master
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testAcquireDrmDisplayEXTNotMaster (void)
{
	LibDrm::FdPtr masterFdPtr = getDrmFdPtr();
	LibDrm::FdPtr notMasterFdPtr = getDrmFdPtr();
	if (!masterFdPtr || !notMasterFdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	if (*masterFdPtr == *notMasterFdPtr)
		TCU_THROW(NotSupportedError, "Did not open 2 different fd.");
	int fd = *notMasterFdPtr;

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	result = m_vki.acquireDrmDisplayEXT(m_physDevice, fd, display);
	if (result != VK_ERROR_INITIALIZATION_FAILED)
		TCU_FAIL("vkAcquireDrmDisplayEXT failed to return error.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests getting an error with an unowned connector id
//  *
//  * This needs to be run with drm master permissions.
//  * No other drm client can be running, such as X or Wayland.
//  * Then, to run with drm master, either:
//  *   Add your user to the "video" linux group.
//  *   Log in to the virtual tty.
//  *   Run as root.
//  * This also requires 2 physically connected displays.
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testAcquireDrmDisplayEXTUnownedConnectorId (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	if (!isDrmMaster(fd))
		TCU_THROW(NotSupportedError, "Does not have drm master permissions.");

	deUint32 connectorId		= getConnectedConnectorId(fd);
	deUint32 otherConnectorId	= getConnectedConnectorId(fd, connectorId);
	deUint32 crtcId				= getValidCrtcId(fd, connectorId);
	if (!connectorId || !crtcId || !otherConnectorId || connectorId == otherConnectorId)
		TCU_THROW(NotSupportedError, "Could not find 2 DRM connectors or a crtc.");

	// Lease the first connector, but try to get and acquire the other connector.
	deUint32		objects[2] = {connectorId, crtcId};
	LibDrm::FdPtr	leaseFdPtr = m_libDrm.createLease(fd, objects, 2, O_CLOEXEC);
	if (!leaseFdPtr)
		TCU_THROW(NotSupportedError, "Could not lease DRM.");
	int leaseFd = *leaseFdPtr;

	// We know that this would fail with leaseFd, so use the original master fd.
	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, otherConnectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	result = m_vki.acquireDrmDisplayEXT(m_physDevice, leaseFd, display);
	if (result != VK_ERROR_INITIALIZATION_FAILED)
		TCU_FAIL("vkAcquireDrmDisplayEXT failed to return error.");

	return tcu::TestStatus::pass("pass");
}

// /*--------------------------------------------------------------------*//*!
//  * \brief Tests successfully releasing an acquired Drm display
//  *
//  * This needs to be run with drm master permissions.
//  * No other drm client can be running, such as X or Wayland.
//  * Then, to run with drm master, either:
//  *   Add your user to the "video" linux group.
//  *   Log in to the virtual tty.
//  *   Run as root.
//  *
//  * Throws an exception on fail.
//  *
//  * \return tcu::TestStatus::pass on success
//  *//*--------------------------------------------------------------------*/
tcu::TestStatus AcquireDrmDisplayTestInstance::testReleaseDisplayEXT (void)
{
	LibDrm::FdPtr fdPtr = getDrmFdPtr();
	if (!fdPtr)
		TCU_THROW(NotSupportedError, "Could not open DRM.");
	int fd = *fdPtr;

	if (!isDrmMaster(fd))
		TCU_THROW(NotSupportedError, "Does not have drm master permissions.");

	deUint32 connectorId = getConnectedConnectorId(fd);
	if (!connectorId)
		TCU_THROW(NotSupportedError, "Could not find a DRM connector.");

	VkDisplayKHR display = INVALID_PTR;
	VkResult result = m_vki.getDrmDisplayEXT(m_physDevice, fd, connectorId, &display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkGetDrmDisplayEXT failed.");

	if (display == DE_NULL || display == INVALID_PTR)
		TCU_FAIL("vkGetDrmDisplayEXT did not set display.");

	result = m_vki.acquireDrmDisplayEXT(m_physDevice, fd, display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkAcquireDrmDisplayEXT failed.");

	result = m_vki.releaseDisplayEXT(m_physDevice, display);
	if (result != VK_SUCCESS)
		TCU_FAIL("vkReleaseDisplayEXT failed.");

	return tcu::TestStatus::pass("pass");
}
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

/*--------------------------------------------------------------------*//*!
 * \brief Acquire Drm display tests case class
 *//*--------------------------------------------------------------------*/
class AcquireDrmDisplayTestsCase : public vkt::TestCase
{
public:
	AcquireDrmDisplayTestsCase (tcu::TestContext &context, const char *name, const char *description, const DrmTestIndex testId)
		: TestCase	(context, name, description)
		, m_testId	(testId)
	{
	}
private:
	const DrmTestIndex	m_testId;

	vkt::TestInstance*	createInstance	(vkt::Context& context) const
	{
		return new AcquireDrmDisplayTestInstance(context, m_testId);
	}
};


/*--------------------------------------------------------------------*//*!
 * \brief Adds a test into group
 *//*--------------------------------------------------------------------*/
static void addTest (tcu::TestCaseGroup* group, const DrmTestIndex testId, const char* name, const char* description)
{
	tcu::TestContext&	testCtx	= group->getTestContext();

	group->addChild(new AcquireDrmDisplayTestsCase(testCtx, name, description, testId));
}

/*--------------------------------------------------------------------*//*!
 * \brief Adds VK_EXT_acquire_drm_display extension tests into group
 *//*--------------------------------------------------------------------*/
void createAcquireDrmDisplayTests (tcu::TestCaseGroup* group)
{
	// VK_EXT_acquire_drm_display extension tests
	addTest(group, DRM_TEST_INDEX_GET_DRM_DISPLAY,							"get_drm_display",							"Get Drm display test");
	addTest(group, DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_FD,				"get_drm_display_invalid_fd",				"Get Drm display with invalid fd test");
	addTest(group, DRM_TEST_INDEX_GET_DRM_DISPLAY_INVALID_CONNECTOR_ID,		"get_drm_display_invalid_connector_id",		"Get Drm display with invalid connector id test");
	addTest(group, DRM_TEST_INDEX_GET_DRM_DISPLAY_NOT_MASTER,				"get_drm_display_not_master",				"Get Drm display with not master test");
	addTest(group, DRM_TEST_INDEX_GET_DRM_DISPLAY_UNOWNED_CONNECTOR_ID,		"get_drm_display_unowned_connector_id",		"Get Drm display with unowned connector id test");
	addTest(group, DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY,						"acquire_drm_display",						"Acquire Drm display test");
	addTest(group, DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_INVALID_FD,			"acquire_drm_display_invalid_fd",			"Acquire Drm display with invalid fd test");
	addTest(group, DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_NOT_MASTER,			"acquire_drm_display_not_master",			"Acquire Drm display with not master test");
	addTest(group, DRM_TEST_INDEX_ACQUIRE_DRM_DISPLAY_UNOWNED_CONNECTOR_ID,	"acquire_drm_display_unowned_connector_id",	"Acquire Drm display with unowned connector id test");

	// VK_EXT_direct_mode_display extension tests
	addTest(group, DRM_TEST_INDEX_RELEASE_DISPLAY,						"release_display",						"Release Drm display test");
}

} //wsi
} //vkt

