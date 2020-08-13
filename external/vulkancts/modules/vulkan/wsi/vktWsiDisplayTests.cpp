/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief Vulkan coverage tests for extensions VK_KHR_display,
 *        VK_KHR_get_display_properties2
 *//*--------------------------------------------------------------------*/

#include "vktWsiDisplayTests.hpp"

#include "vktTestCase.hpp"
#include "vkStrUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRef.hpp"
#include "vkQueryUtil.hpp"
#include "vkWsiUtil.hpp"

#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"
#include "tcuResultCollector.hpp"

#include "deMemory.h"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"

#include <set>
#include <map>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace vkt
{
namespace wsi
{
using namespace vk;
using std::vector;
using std::map;
using std::set;
using std::string;

#ifndef TCU_FAIL_STR
	#define TCU_FAIL_STR(MSG) TCU_FAIL(string(MSG).c_str())
#endif

enum DisplayIndexTest
{
	DISPLAY_TEST_INDEX_START,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_SUPPORTED_DISPLAY,
	DISPLAY_TEST_INDEX_GET_DISPLAY_MODE,
	DISPLAY_TEST_INDEX_CREATE_DISPLAY_MODE,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES,
	DISPLAY_TEST_INDEX_CREATE_DISPLAY_PLANE_SURFACE,
	DISPLAY_TEST_INDEX_SURFACE_COUNTERS,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES2,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES2,
	DISPLAY_TEST_INDEX_GET_DISPLAY_MODE2,
	DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES2,
	DISPLAY_TEST_INDEX_LAST
};

template <typename Type>
class BinaryCompare
{
public:
	bool operator() (const Type& a, const Type& b) const
	{
		return deMemCmp(&a, &b, sizeof(Type)) < 0;
	}
};

typedef std::set<vk::VkDisplayKHR, BinaryCompare<vk::VkDisplayKHR> >	DisplaySet;
typedef std::vector<vk::VkDisplayKHR>									DisplayVector;
typedef std::vector<vk::VkDisplayModePropertiesKHR>						DisplayModePropertiesVector;
typedef std::vector<vk::VkDisplayModeProperties2KHR>					DisplayModeProperties2Vector;

const deUint32 DEUINT32_MAX = std::numeric_limits<deUint32>::max();

const deUint32 RECOGNIZED_SURFACE_TRANSFORM_FLAGS =
														  vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR
														| vk::VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR;

const deUint32 RECOGNIZED_DISPLAY_PLANE_ALPHA_FLAGS =
														  VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR
														| VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR
														| VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR
														| VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
enum DisplayMaxTestedConsts
{
	MAX_TESTED_DISPLAY_COUNT	= 16,
	MAX_TESTED_PLANE_COUNT		= 16,
};

/*--------------------------------------------------------------------*//*!
 * \brief Return Vulkan result name or code as std::string.
 *
 * \param result Vulkan code to convert to string
 * \return Vulkun result code name or code number as std::string
 *//*--------------------------------------------------------------------*/
std::string getResultAsString (vk::VkResult result)
{
	const char* resultAsChar = vk::getResultName(result);

	if (resultAsChar != DE_NULL)
		return std::string(resultAsChar);
	else
		return de::toString(result);
}

/*--------------------------------------------------------------------*//*!
 * \brief Moves test index to next test skipping middle tests.
 *
 * Gets first 3 tests and last 3 tests on long sequences.
 * After test number 2 moves index value to endIndex - 3.
 * Shortens the number of tests executed by skipping middle tests.
 *
 * Example:
 * for (i=0; i<endIndex; nextTestNumber(i, endIndex))
 * with endIndex = 4 generates 0,1,2,3
 * with endIndex = 9 generates 0,1,2,6,7,8
 *
 * \param index    Current iterator value
 * \param endIndex First number out of iteration sequence
 * \return new iterator value
 *//*--------------------------------------------------------------------*/
deUint32 nextTestNumber (deUint32 index, deUint32 endIndex)
{
	deUint32 result;

	if (endIndex > 6 && index == 2)
		result = endIndex - 3;
	else
		result = index + 1;

	return result;
}

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan VK_KHR_display extensions coverage tests
 *//*--------------------------------------------------------------------*/
class DisplayCoverageTestInstance : public TestInstance
{
public:
								DisplayCoverageTestInstance						(Context& context, const DisplayIndexTest testId);
private:
	typedef void				(DisplayCoverageTestInstance::*EachSurfaceFunctionPtr)
																				(VkSurfaceKHR& surface, VkDisplayModePropertiesKHR& modeProperties);

	bool						getDisplays										(DisplayVector& displays);
	bool						getDisplaysForPlane								(deUint32 plane, DisplayVector& displays);
	bool						getDisplayModeProperties						(VkDisplayKHR display, DisplayModePropertiesVector& modeProperties);

	bool						getDisplays2									(DisplayVector& displays);
	bool						getDisplayModeProperties2						(VkDisplayKHR display, DisplayModeProperties2Vector& modeProperties);

	void						validateDisplayProperties						(	tcu::ResultCollector&					results,
																					const VkDisplayPropertiesKHR&			toValidate,
																					const VkDisplayPropertiesKHR&			nonUpdated);

	void						validateDisplayPlaneProperties					(	tcu::ResultCollector&					results,
																					const VkDisplayPlanePropertiesKHR&		toValidate,
																					const VkDisplayPlanePropertiesKHR&		nonUpdated,
																					DisplaySet&								displaySet);

	void						validateDisplayPlaneCapabilities				(	tcu::ResultCollector&					results,
																					const VkDisplayPlaneCapabilitiesKHR&	toValidate,
																					const VkDisplayPlaneCapabilitiesKHR&	nonUpdated);

	void						validateDisplayModeProperties					(	tcu::ResultCollector&					results,
																					const VkDisplayModePropertiesKHR&		toValidate,
																					const VkDisplayModePropertiesKHR&		nonUpdated);

	// VK_KHR_display extension tests
	tcu::TestStatus				testGetPhysicalDeviceDisplayPropertiesKHR		(void);
	tcu::TestStatus				testGetPhysicalDeviceDisplayPlanePropertiesKHR	(void);
	tcu::TestStatus				testGetDisplayPlaneSupportedDisplaysKHR			(void);
	tcu::TestStatus				testGetDisplayModePropertiesKHR					(void);
	tcu::TestStatus				testCreateDisplayModeKHR						(void);
	tcu::TestStatus				testGetDisplayPlaneCapabilitiesKHR				(void);

	enum SurfaceTestKind
	{
		SURFACE_CREATE = 0,
		SURFACE_COUNTERS,
		SURFACE_TEST_KIND_MAX_ENUM
	};

	tcu::TestStatus				testDisplaySurface								(SurfaceTestKind testKind);

	// VK_KHR_get_display_properties2 extension tests
	tcu::TestStatus				testGetPhysicalDeviceDisplayProperties2KHR		(void);
	tcu::TestStatus				testGetPhysicalDeviceDisplayPlaneProperties2KHR	(void);
	tcu::TestStatus				testGetDisplayModeProperties2KHR				(void);
	tcu::TestStatus				testGetDisplayPlaneCapabilities2KHR				(void);

	tcu::TestStatus				iterate											(void);

	void						testCreateSharedSwapchainsKHRforSurface			(VkSurfaceKHR& surface, VkDisplayModePropertiesKHR& modeProperties);

	const InstanceInterface&	m_vki;
	const DeviceInterface&		m_vkd;
	tcu::TestLog&				m_log;
	const VkPhysicalDevice		m_physicalDevice;
	const DisplayIndexTest		m_testId;
};


/*--------------------------------------------------------------------*//*!
 * \brief DisplayCoverageTestInstance constructor
 *
 * Initializes DisplayCoverageTestInstance object
 *
 * \param context    Context object
 * \param parameters Test parameters structure
 *//*--------------------------------------------------------------------*/
DisplayCoverageTestInstance::DisplayCoverageTestInstance (Context& context, const DisplayIndexTest testId)
	: TestInstance		(context)
	, m_vki				(m_context.getInstanceInterface())
	, m_vkd				(m_context.getDeviceInterface())
	, m_log				(m_context.getTestContext().getLog())
	, m_physicalDevice	(m_context.getPhysicalDevice())
	, m_testId			(testId)
{
	const std::string extensionName("VK_KHR_display");

	if(!de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), extensionName))
		TCU_THROW(NotSupportedError, std::string(extensionName + " is not supported").c_str());

	switch (m_testId)
	{
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES2:
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES2:
		case DISPLAY_TEST_INDEX_GET_DISPLAY_MODE2:
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES2:
		{
			const std::string extensionNameAddition("VK_KHR_get_display_properties2");

			if(!de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), extensionNameAddition))
				TCU_THROW(NotSupportedError, std::string(extensionNameAddition + " is not supported").c_str());

			break;
		}

		default:
		{
			break;
		}
	}
}

/*--------------------------------------------------------------------*//*!
 * \brief Step forward test execution
 *
 * \return true if application should call iterate() again and false
 *         if test execution session is complete.
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::iterate (void)
{
	switch (m_testId)
	{
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES:					return testGetPhysicalDeviceDisplayPropertiesKHR();			break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES:						return testGetPhysicalDeviceDisplayPlanePropertiesKHR();	break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_SUPPORTED_DISPLAY:	return testGetDisplayPlaneSupportedDisplaysKHR();			break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_MODE:						return testGetDisplayModePropertiesKHR();					break;
		case DISPLAY_TEST_INDEX_CREATE_DISPLAY_MODE:					return testCreateDisplayModeKHR();							break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES:			return testGetDisplayPlaneCapabilitiesKHR();				break;
		case DISPLAY_TEST_INDEX_CREATE_DISPLAY_PLANE_SURFACE:			return testDisplaySurface(SURFACE_CREATE);					break;
		case DISPLAY_TEST_INDEX_SURFACE_COUNTERS:						return testDisplaySurface(SURFACE_COUNTERS);				break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES2:				return testGetPhysicalDeviceDisplayProperties2KHR();		break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES2:					return testGetPhysicalDeviceDisplayPlaneProperties2KHR();	break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_MODE2:						return testGetDisplayModeProperties2KHR();					break;
		case DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES2:		return testGetDisplayPlaneCapabilities2KHR();				break;
		default:
		{
			DE_FATAL("Impossible");
		}
	}

	TCU_FAIL("Invalid test identifier");
}

/*--------------------------------------------------------------------*//*!
 * \brief Fills vector with available displays. Clears passed vector at start.
 *
 * \param displays The vector filled with display handles
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool DisplayCoverageTestInstance::getDisplays (DisplayVector& displays)
{
	deUint32							countReported	=	0u;
	deUint32							countRetrieved	=	0u;
	std::vector<VkDisplayPropertiesKHR>	displaysProps;
	VkResult							result;

	displays.clear();

	result = m_vki.getPhysicalDeviceDisplayPropertiesKHR(	m_physicalDevice,	// VkPhysicalDevice			physicalDevice
															&countReported,		// uint32_t*				pPropertyCount
															DE_NULL);			// VkDisplayPropertiesKHR*	pProperties

	if (result != VK_SUCCESS)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetPhysicalDeviceDisplayPropertiesKHR failed with " << getResultAsString(result)
				<< " reported items count " << countReported
				<< tcu::TestLog::EndMessage;

		return false;
	}

	if (!countReported)
		TCU_THROW(NotSupportedError, "No displays reported");

	displaysProps.resize(countReported);

	countRetrieved = countReported;

	result = m_vki.getPhysicalDeviceDisplayPropertiesKHR(	m_physicalDevice,	// VkPhysicalDevice			physicalDevice
															&countRetrieved,	// uint32_t*				pPropertyCount
															&displaysProps[0]);	// VkDisplayPropertiesKHR*	pProperties

	if (result != VK_SUCCESS || countRetrieved > countReported)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetPhysicalDeviceDisplayPropertiesKHR failed with " << getResultAsString(result)
				<< " reported items count " << countReported
				<< " retrieved items count " << countRetrieved
				<< tcu::TestLog::EndMessage;

		return false;
	}

	displays.reserve(countRetrieved);

	for (deUint32	displayIndex = 0;
					displayIndex < countRetrieved;
					displayIndex++)
	{
		const VkDisplayKHR display = displaysProps[displayIndex].display;

		if (display == DE_NULL)
		{
			displays.clear();

			return false;
		}

		displays.push_back(display);
	}

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fills vector with available displays for plane specified.
 *
 * Clears passed vector at start and on error.
 *
 * \param plane		The plane to get displays for
 * \param displays	The vector filled with display handles
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool DisplayCoverageTestInstance::getDisplaysForPlane(deUint32 plane, DisplayVector& displays)
{
	deUint32	countReported	=	0u;
	deUint32	countRetrieved	=	0u;
	VkResult	result;

	displays.clear();

	result = m_vki.getDisplayPlaneSupportedDisplaysKHR(	m_physicalDevice,	// VkPhysicalDevice	physicalDevice
														plane,				// uint32_t			planeIndex
														&countReported,		// uint32_t*		pDisplayCount
														DE_NULL);			// VkDisplayKHR*	pDisplays

	if (result != VK_SUCCESS)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayPlaneSupportedDisplaysKHR failed with " << getResultAsString(result)
				<< " for plane " << plane
				<< " reported items count " << countReported
				<< tcu::TestLog::EndMessage;

		return false;
	}

	displays.resize(countReported);

	countRetrieved = countReported;

	result = m_vki.getDisplayPlaneSupportedDisplaysKHR(	m_physicalDevice,	// VkPhysicalDevice	physicalDevice
														plane,				// uint32_t			planeIndex
														&countRetrieved,	// uint32_t*		pDisplayCount
														&displays[0]);		// VkDisplayKHR*	pDisplays

	if (result != VK_SUCCESS || countRetrieved > countReported)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayPlaneSupportedDisplaysKHR failed with " << getResultAsString(result)
				<< " for plane " << plane
				<< " reported items count " << countReported
				<< " retrieved items count " << countRetrieved
				<< tcu::TestLog::EndMessage;

		displays.clear();

		return false;
	}

	if (countRetrieved < countReported)
		displays.resize(countRetrieved);

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fills vector with available modes properties for display specified.
 *
 * Clears passed vector at start and on error.
 *
 * \param display	The display to get modes for
 * \param modes		The vector filled with display mode properties structures
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool DisplayCoverageTestInstance::getDisplayModeProperties(VkDisplayKHR display, DisplayModePropertiesVector& modeProperties)
{
	deUint32	countReported	=	0u;
	deUint32	countRetrieved	=	0u;
	VkResult	result;

	modeProperties.clear();

	result = m_vki.getDisplayModePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
												display,				// VkDisplayKHR					display
												&countReported,			// uint32_t*					pPropertyCount
												DE_NULL);				// VkDisplayModePropertiesKHR*	pProperties

	if (result != VK_SUCCESS)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayModePropertiesKHR failed with " << getResultAsString(result)
				<< " for display " << display
				<< " reported items count " << countReported
				<< tcu::TestLog::EndMessage;

		return false;
	}

	modeProperties.resize(countReported);

	countRetrieved = countReported;

	result = m_vki.getDisplayModePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
												display,				// VkDisplayKHR					display
												&countRetrieved,		// uint32_t*					pPropertyCount
												&modeProperties[0]);	// VkDisplayModePropertiesKHR*	pProperties

	if (result != VK_SUCCESS || countRetrieved > countReported)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayModePropertiesKHR failed with " << getResultAsString(result)
				<< " for display " << display
				<< " reported items count " << countReported
				<< " retrieved items count " << countReported
				<< tcu::TestLog::EndMessage;

		modeProperties.clear();

		return false;
	}

	if (countRetrieved < countReported)
		modeProperties.resize(countRetrieved);

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fills vector with available displays. Clears passed vector at start.
 *
 * Uses VK_KHR_get_display_properties2 extension API.
 * Clears passed vector at start.
 *
 * \param displays The vector filled with display handles
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool DisplayCoverageTestInstance::getDisplays2 (DisplayVector& displays)
{
	deUint32								countReported		=	0u;
	deUint32								countRetrieved		=	0u;
	const VkDisplayPropertiesKHR			displayProperties	=	{
																		DE_NULL,										// VkDisplayKHR					display
																		DE_NULL,										// const char*					displayName
																		{0, 0},											// VkExtent2D					physicalDimensions
																		{0, 0},											// VkExtent2D					physicalResolution
																		0,												// VkSurfaceTransformFlagsKHR	supportedTransforms
																		VK_FALSE,										// VkBool32						planeReorderPossible
																		VK_FALSE										// VkBool32						persistentContent
																	};
	const VkDisplayProperties2KHR			displayProperties2	=	{
																		VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR,		// VkStructureType			sType
																		DE_NULL,										// void*					pNext
																		displayProperties								// VkDisplayPropertiesKHR	displayProperties
																	};

	std::vector<VkDisplayProperties2KHR>	displaysProps;
	VkResult								result;

	displays.clear();

	result = m_vki.getPhysicalDeviceDisplayProperties2KHR(	m_physicalDevice,	// VkPhysicalDevice			physicalDevice
															&countReported,		// uint32_t*				pPropertyCount
															DE_NULL);			// VkDisplayProperties2KHR*	pProperties

	if (result != VK_SUCCESS)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetPhysicalDeviceDisplayProperties2KHR failed with " << getResultAsString(result)
				<< " reported items count " << countReported
				<< tcu::TestLog::EndMessage;

		return false;
	}

	if (!countReported)
		TCU_THROW(NotSupportedError, "No displays reported");

	displaysProps.resize(countReported, displayProperties2);

	countRetrieved = countReported;

	result = m_vki.getPhysicalDeviceDisplayProperties2KHR(	m_physicalDevice,	// VkPhysicalDevice			physicalDevice
															&countRetrieved,	// uint32_t*				pPropertyCount
															&displaysProps[0]);	// VkDisplayPropertiesKHR*	pProperties

	if (result != VK_SUCCESS || countRetrieved > countReported)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetPhysicalDeviceDisplayProperties2KHR failed with " << getResultAsString(result)
				<< " reported items count " << countReported
				<< " retrieved items count " << countRetrieved
				<< tcu::TestLog::EndMessage;

		return false;
	}

	displays.reserve(countRetrieved);

	for (deUint32	displayIndex = 0;
					displayIndex < countRetrieved;
					displayIndex++)
	{
		const VkDisplayKHR display = displaysProps[displayIndex].displayProperties.display;

		if (display == DE_NULL)
		{
			displays.clear();

			return false;
		}

		displays.push_back(display);
	}

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Fills vector with available modes properties for display specified.
 *
 * Uses VK_KHR_get_display_properties2 extension API.
 * Clears passed vector at start and on error.
 *
 * \param display	The display to get modes for
 * \param modes		The vector filled with display mode properties structures
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool DisplayCoverageTestInstance::getDisplayModeProperties2 (VkDisplayKHR display, DisplayModeProperties2Vector& modeProperties)
{
	deUint32							countReported			=	0u;
	deUint32							countRetrieved			=	0u;
	const VkDisplayModePropertiesKHR	displayModeProperties	=	{
																		DE_NULL,											// VkDisplayModeKHR				displayMode
																		{													// VkDisplayModeParametersKHR	parameters
																			{0, 0},											// VkExtent2D					visibleRegion
																			0												// uint32_t						refreshRate
																		}
																	};
	const VkDisplayModeProperties2KHR	displayModeProperties2	=	{
																		VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR,	// VkStructureType				sType
																		DE_NULL,											// void*						pNext
																		displayModeProperties								// VkDisplayModePropertiesKHR	displayModeProperties
																	};
	VkResult							result;

	modeProperties.clear();

	result = m_vki.getDisplayModeProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
													display,				// VkDisplayKHR					display
													&countReported,			// uint32_t*					pPropertyCount
													DE_NULL);				// VkDisplayModeProperties2KHR*	pProperties

	if (result != VK_SUCCESS)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayModeProperties2KHR failed with " << getResultAsString(result)
				<< " for display " << display
				<< " reported items count " << countReported
				<< tcu::TestLog::EndMessage;

		return false;
	}

	modeProperties.resize(countReported, displayModeProperties2);

	countRetrieved = countReported;

	result = m_vki.getDisplayModeProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
													display,				// VkDisplayKHR					display
													&countRetrieved,		// uint32_t*					pPropertyCount
													&modeProperties[0]);	// VkDisplayModeProperties2KHR*	pProperties

	if (result != VK_SUCCESS || countRetrieved > countReported)
	{
		m_log	<< tcu::TestLog::Message
				<< "vkGetDisplayModeProperties2KHR failed with " << getResultAsString(result)
				<< " for display " << display
				<< " reported items count " << countReported
				<< " retrieved items count " << countReported
				<< tcu::TestLog::EndMessage;

		modeProperties.clear();

		return false;
	}

	if (countRetrieved < countReported)
		modeProperties.resize(countRetrieved);

	return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Validate display properties and report failures
 *        into results collector
 *
 * \param results		Results collector
 * \param toValidate	Display properties to validate
 * \param nonUpdated	Display properties to compare with
 *//*--------------------------------------------------------------------*/
void DisplayCoverageTestInstance::validateDisplayProperties (	tcu::ResultCollector&			results,
																const VkDisplayPropertiesKHR&	toValidate,
																const VkDisplayPropertiesKHR&	nonUpdated)
{
	results.check(	toValidate.display != nonUpdated.display,
					"Invalid display handle");

	results.check(	toValidate.planeReorderPossible == VK_TRUE || toValidate.planeReorderPossible == VK_FALSE,
					"planeReorderPossible neither VK_TRUE, nor VK_FALSE");

	results.check(	toValidate.persistentContent == VK_TRUE || toValidate.persistentContent == VK_FALSE,
					"persistentContent neither VK_TRUE, nor VK_FALSE");

	results.check(	(toValidate.supportedTransforms & nonUpdated.supportedTransforms) == 0,
					"supportedTransforms contains unrecognized flags");

	// Outside specification, but resolution 0x0 pixels will break many applications
	results.check(	toValidate.physicalResolution.height != 0,
					"physicalResolution.height cannot be zero");

	// Outside specification, but resolution 0x0 pixels will break many applications
	results.check(	toValidate.physicalResolution.width != 0,
					"physicalResolution.width cannot be zero");
}

/*--------------------------------------------------------------------*//*!
 * \brief Validates display plane properties and report failures
 *        into results collector
 *
 * \param results		Results collector
 * \param toValidate	Display plane properties to validate
 * \param nonUpdated	Display plane properties to compare with
 * \param displaySet	Set of valid display handles
 *//*--------------------------------------------------------------------*/
void DisplayCoverageTestInstance::validateDisplayPlaneProperties (	tcu::ResultCollector&				results,
																	const VkDisplayPlanePropertiesKHR&	toValidate,
																	const VkDisplayPlanePropertiesKHR&	nonUpdated,
																	DisplaySet&							displaySet)
{
	const VkDisplayKHR currentDisplay = toValidate.currentDisplay;

	results.check(	toValidate.currentStackIndex < nonUpdated.currentStackIndex,
					"CurrentStackIndex must be less than the number of planes reported " + de::toString(nonUpdated.currentStackIndex));

	results.check(	currentDisplay == DE_NULL || de::contains(displaySet, currentDisplay),
					"Plane bound to invalid handle " + de::toString(toValidate.currentDisplay));
}

/*--------------------------------------------------------------------*//*!
 * \brief Validate display plane capabilities and report failures
 *        into results collector
 *
 * \param results		Results collector
 * \param toValidate	Display plane capabilities to validate
 * \param nonUpdated	Display plane capabilities to compare with
 *//*--------------------------------------------------------------------*/
void DisplayCoverageTestInstance::validateDisplayPlaneCapabilities (	tcu::ResultCollector&					results,
																		const VkDisplayPlaneCapabilitiesKHR&	toValidate,
																		const VkDisplayPlaneCapabilitiesKHR&	nonUpdated)
{
	results.check(	(toValidate.supportedAlpha & nonUpdated.supportedAlpha) == 0,
					"supportedAlpha contains unrecognized value");

	results.check(	toValidate.minSrcPosition.x >= 0,
					"minSrcPosition.x >= 0");

	results.check(	toValidate.minSrcPosition.y >= 0,
					"minSrcPosition.y >= 0");

	results.check(	toValidate.maxSrcPosition.x >= 0,
					"maxSrcPosition.x >= 0");

	results.check(	toValidate.maxSrcPosition.y >= 0,
					"maxSrcPosition.y >= 0");

	results.check(	toValidate.minSrcPosition.x <= toValidate.maxSrcPosition.x,
					"minSrcPosition.x <= maxSrcPosition.x");

	results.check(	toValidate.minSrcPosition.y <= toValidate.maxSrcPosition.y,
					"minSrcPosition.y <= maxSrcPosition.y");

	results.check(	toValidate.minDstPosition.x <= toValidate.maxDstPosition.x,
					"minDstPosition.x <= maxDstPosition.x");

	results.check(	toValidate.minDstPosition.y <= toValidate.maxDstPosition.y,
					"minDstPosition.y <= maxDstPosition.y");

	results.check(	toValidate.minSrcExtent.width <= toValidate.maxSrcExtent.width,
					"minSrcExtent.width <= maxSrcExtent.width");

	results.check(	toValidate.minSrcExtent.height <= toValidate.maxSrcExtent.height,
					"minSrcExtent.height <= maxSrcExtent.height");

	results.check(	toValidate.minDstExtent.width <= toValidate.maxDstExtent.width,
					"minDstExtent.width <= maxDstExtent.width");

	results.check(	toValidate.minDstExtent.height <= toValidate.maxDstExtent.height,
					"minDstExtent.height <= maxDstExtent.height");
}

/*--------------------------------------------------------------------*//*!
 * \brief Validate display mode properties and report failures
 *        into results collector
 *
 * \param results		Results collector
 * \param toValidate	Display mode properties to validate
 * \param nonUpdated	Display mode properties to compare with
 *//*--------------------------------------------------------------------*/
void DisplayCoverageTestInstance::validateDisplayModeProperties (	tcu::ResultCollector&				results,
																	const VkDisplayModePropertiesKHR&	toValidate,
																	const VkDisplayModePropertiesKHR&	nonUpdated)
{
	results.check(	toValidate.displayMode != nonUpdated.displayMode,
					"Invalid mode display handle reported");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display enumeration coverage test
 *
 * Throws ResourceError exception in case no displays available.
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetPhysicalDeviceDisplayPropertiesKHR (void)
{
	deUint32				displayCountReported	=	0u;
	deUint32				displayCountToTest		=	0u;
	tcu::ResultCollector	results						(m_log);
	VkResult				result;

	result = m_vki.getPhysicalDeviceDisplayPropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice			physicalDevice
															&displayCountReported,	// uint32_t*				pPropertyCount
															DE_NULL);				// VkDisplayPropertiesKHR*	pProperties

	if (   result != VK_SUCCESS
		&& result != VK_INCOMPLETE
		&& result != VK_ERROR_OUT_OF_HOST_MEMORY
		&& result != VK_ERROR_OUT_OF_DEVICE_MEMORY
		)
	{
		TCU_FAIL_STR(string("Invalid result ") + getResultAsString(result));
	}

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (displayCountReported == 0)
		TCU_THROW(NotSupportedError, std::string("Cannot perform test: no displays found").c_str());

	displayCountToTest = displayCountReported;
	if (displayCountReported > MAX_TESTED_DISPLAY_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of displays reported is too high " << displayCountReported
				<< ". Test is limited to " << MAX_TESTED_DISPLAY_COUNT
				<< tcu::TestLog::EndMessage;

		displayCountToTest = MAX_TESTED_DISPLAY_COUNT;
	}

	// Test the call correctly writes data in various size arrays
	for (deUint32	displayCountRequested = 0;
					displayCountRequested < displayCountToTest + 2;
					displayCountRequested++)
	{
		const deUint32						displayCountExpected	=	std::min(displayCountRequested, displayCountReported);
		const VkDisplayPropertiesKHR		invalidDisplayProps		=	{	// Most values are set to fail the test to make sure driver updates these
																			DE_NULL,								// VkDisplayKHR					display;
																			DE_NULL,								// const char*					displayName;
																			{0, 0},									// VkExtent2D					physicalDimensions;
																			{0, 0},									// VkExtent2D					physicalResolution;
																			~RECOGNIZED_SURFACE_TRANSFORM_FLAGS,	// VkSurfaceTransformFlagsKHR	supportedTransforms;
																			(vk::VkBool32)(VK_TRUE + 1),			// VkBool32						planeReorderPossible;
																			(vk::VkBool32)(VK_TRUE + 1)				// VkBool32						persistentContent;
																		};
		const VkDisplayKHR					canaryDisplay			=	static_cast<VkDisplayKHR>(0xABCDEF11);
		const deUint32						canaryItemCount			=	1;
		std::vector<VkDisplayPropertiesKHR>	displaysProps				(displayCountRequested + canaryItemCount, invalidDisplayProps);
		deUint32							displayCountRetrieved	=	displayCountRequested;
		DisplaySet							displaySet;

		displaysProps[displayCountExpected].display = canaryDisplay;

		result = m_vki.getPhysicalDeviceDisplayPropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice			physicalDevice
																&displayCountRetrieved,	// uint32_t*				pPropertyCount
																&displaysProps[0]);		// VkDisplayPropertiesKHR*	pProperties

		// Check amount of data written equals to expected
		if (displayCountRetrieved != displayCountExpected)
			TCU_FAIL_STR(	string("displayCountRetrieved != displayCountExpected, ") +
							de::toString(displayCountRetrieved) + " != " + de::toString(displayCountExpected));

		if (displayCountRequested >= displayCountReported)
		{
			if (result != VK_SUCCESS)
				TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
		}
		else
		{
			if (result != VK_INCOMPLETE)
				TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
		}

		// Check the driver has written something
		for (deUint32	displayIndex = 0;
						displayIndex < displayCountRetrieved;
						displayIndex++)
		{
			displaySet.insert(displaysProps[displayIndex].display);

			results.check(	displaysProps[displayIndex].display != invalidDisplayProps.display,
							"Invalid display handle for display number " + de::toString(displayIndex));

			results.check(	displaysProps[displayIndex].planeReorderPossible == VK_TRUE || displaysProps[displayIndex].planeReorderPossible == VK_FALSE,
							"planeReorderPossible neither VK_TRUE, nor VK_FALSE");

			results.check(	displaysProps[displayIndex].persistentContent == VK_TRUE || displaysProps[displayIndex].persistentContent == VK_FALSE,
							"persistentContent neither VK_TRUE, nor VK_FALSE");

			results.check(	(displaysProps[displayIndex].supportedTransforms & invalidDisplayProps.supportedTransforms) == 0,
							"supportedTransforms contains unrecognized flags");

			// Outside specification, but resolution 0x0 pixels will break many applications
			results.check(	displaysProps[displayIndex].physicalResolution.height != 0,
							"physicalResolution.height cannot be zero");

			// Outside specification, but resolution 0x0 pixels will break many applications
			results.check(	displaysProps[displayIndex].physicalResolution.width != 0,
							"physicalResolution.width cannot be zero");

			if (results.getResult() != QP_TEST_RESULT_PASS)
			{
				m_log	<< tcu::TestLog::Message
						<< "Error detected " << results.getMessage()
						<< " for display " << displayIndex << " with properties " << displaysProps[displayIndex]
						<< " invalid display properties are " << invalidDisplayProps
						<< tcu::TestLog::EndMessage;

				TCU_FAIL_STR(results.getMessage());
			}
		}

		// Check the driver has not written more than requested
		if (displaysProps[displayCountExpected].display != canaryDisplay)
			TCU_FAIL("Memory damage detected: driver has written more than expected");

		// Check display handle uniqueness
		if (displaySet.size() != displayCountRetrieved)
			TCU_FAIL("Display handle duplication detected");
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Plane enumeration coverage test
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetPhysicalDeviceDisplayPlanePropertiesKHR (void)
{
	DisplayVector			displaysVector;
	DisplaySet				displaySet;
	deUint32				planeCountReported	=	0u;
	deUint32				planeCountTested	=	0u;
	tcu::ResultCollector	results					(m_log);
	VkResult				result;

	// Create a list of displays available
	if (!getDisplays(displaysVector))
		TCU_FAIL("Failed to retrieve displays");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	displaySet = DisplaySet(displaysVector.begin(), displaysVector.end());

	// Get planes to test
	result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																&planeCountReported,	// uint32_t*					pPropertyCount
																DE_NULL);				// VkDisplayPlanePropertiesKHR*	pProperties

	if (   result != VK_SUCCESS
		&& result != VK_INCOMPLETE
		&& result != VK_ERROR_OUT_OF_HOST_MEMORY
		&& result != VK_ERROR_OUT_OF_DEVICE_MEMORY
		)
	{
		TCU_FAIL_STR(string("Invalid result ") + getResultAsString(result));
	}

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
		TCU_THROW(ResourceError, "Cannot perform test: no planes found");

	planeCountTested = planeCountReported;
	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountTested = MAX_TESTED_PLANE_COUNT;
	}

	// Test the call correctly writes data in various size arrays
	for (deUint32	planeCountRequested = 0;
					planeCountRequested < planeCountTested + 2;
					planeCountRequested++)
	{
		const deUint32								planeCountExpected	=	std::min(planeCountRequested, planeCountReported);
		const VkDisplayPlanePropertiesKHR			invalidPlaneProps	=	{	// Most values are set to fail the test to make sure driver updates these
																				DE_NULL,		// VkDisplayKHR	currentDisplay
																				DEUINT32_MAX	// deUint32		currentStackIndex
																			};
		const VkDisplayKHR							canaryDisplay		=	static_cast<VkDisplayKHR>(0xABCDEF11);
		const deUint32								canaryItemCount		=	1;
		std::vector<VkDisplayPlanePropertiesKHR>	planeProps				(planeCountRequested + canaryItemCount, invalidPlaneProps);
		deUint32									planeCountRetrieved	=	planeCountRequested;

		planeProps[planeCountExpected].currentDisplay = canaryDisplay;

		result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																	&planeCountRetrieved,	// uint32_t*					pPropertyCount
																	&planeProps[0]);		// VkDisplayPlanePropertiesKHR*	pProperties

		// Check amount of data written equals to expected
		if (planeCountRetrieved != planeCountExpected)
			TCU_FAIL_STR(	string("planeCountRetrieved != planeCountExpected, ") +
							de::toString(planeCountRetrieved) + " != " + de::toString(planeCountExpected));

		if (planeCountRequested >= planeCountReported)
		{
			if (result != VK_SUCCESS)
				TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
		}
		else
		{
			if (result != VK_INCOMPLETE)
				TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
		}

		// Check the driver has written something
		for (deUint32	planeIndex = 0;
						planeIndex < planeCountRetrieved;
						planeIndex++)
		{
			const VkDisplayKHR currentDisplay = planeProps[planeIndex].currentDisplay;

			results.check(	planeProps[planeIndex].currentStackIndex < planeCountReported,
							"CurrentStackIndex must be less than the number of planes reported " + de::toString(planeCountReported));

			results.check(	currentDisplay == DE_NULL || de::contains(displaySet, currentDisplay),
							"Plane bound to invalid handle " + de::toString(currentDisplay));

			if (results.getResult() != QP_TEST_RESULT_PASS)
			{
				m_log	<< tcu::TestLog::Message
						<< "Error detected " << results.getMessage()
						<< " for plane " << planeIndex << " with properties " << planeProps[planeIndex]
						<< tcu::TestLog::EndMessage;

				TCU_FAIL_STR(results.getMessage());
			}
		}

		// Check the driver has not written more than requested
		if (planeProps[planeCountExpected].currentDisplay != canaryDisplay)
			TCU_FAIL("Memory damage detected: driver has written more than expected");
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display plane support coverage test
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetDisplayPlaneSupportedDisplaysKHR (void)
{
	deUint32		planeCountReported	=	0u;
	deUint32		planeCountTested	=	0u;
	VkResult		result;
	DisplayVector	displaysVector;
	DisplaySet		displaySet;

	if (!getDisplays(displaysVector))
		TCU_FAIL("Failed to retrieve displays");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	displaySet = DisplaySet(displaysVector.begin(), displaysVector.end());

	result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																&planeCountReported,	// uint32_t*					pPropertyCount
																DE_NULL);				// VkDisplayPlanePropertiesKHR*	pProperties

	if (   result != VK_SUCCESS
		&& result != VK_INCOMPLETE
		&& result != VK_ERROR_OUT_OF_HOST_MEMORY
		&& result != VK_ERROR_OUT_OF_DEVICE_MEMORY
		)
	{
		TCU_FAIL_STR(string("Invalid result ") + getResultAsString(result));
	}

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
		TCU_THROW(ResourceError, "Cannot perform test: no planes supported");

	planeCountTested = planeCountReported;
	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountTested = MAX_TESTED_PLANE_COUNT;
	}

	for (deUint32	planeIndex = 0;
					planeIndex < planeCountTested;
					planeIndex++)
	{
		deUint32 displayCountReported = 0u;

		result = m_vki.getDisplayPlaneSupportedDisplaysKHR(	m_physicalDevice,		// VkPhysicalDevice	physicalDevice
															planeIndex,				// uint32_t			planeIndex
															&displayCountReported,	// uint32_t*		pDisplayCount
															DE_NULL);				// VkDisplayKHR*	pDisplays

		if (result != VK_SUCCESS)
			TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

		// Test the call correctly writes data in various size arrays
		for (deUint32	displayCountRequested = 0;
						displayCountRequested < displayCountReported + 2;
						displayCountRequested++)
		{
			const deUint32				displayCountExpected	=	std::min(displayCountRequested, displayCountReported);
			const VkDisplayKHR			nullDisplay				=	DE_NULL;
			const VkDisplayKHR			canaryDisplay			=	static_cast<VkDisplayKHR>(0xABCDEF11);
			const deUint32				canaryItemCount			=	1;
			std::vector<VkDisplayKHR>	displaysForPlane			(displayCountRequested + canaryItemCount, nullDisplay);
			deUint32					displayCountRetrieved	=	displayCountRequested;

			displaysForPlane[displayCountExpected] = canaryDisplay;

			result = m_vki.getDisplayPlaneSupportedDisplaysKHR(	m_physicalDevice,		// VkPhysicalDevice	physicalDevice
																planeIndex,				// uint32_t			planeIndex
																&displayCountRetrieved,	// uint32_t*		pDisplayCount
																&displaysForPlane[0]);	// VkDisplayKHR*	pDisplays

			// Check amount of data written equals to expected
			if (displayCountRetrieved != displayCountExpected)
				TCU_FAIL_STR(	string("displayCountRetrieved != displayCountExpected, ") +
								de::toString(displayCountRetrieved) + " != " + de::toString(displayCountExpected));

			if (displayCountRequested >= displayCountReported)
			{
				if (result != VK_SUCCESS)
					TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
			}
			else
			{
				if (result != VK_INCOMPLETE)
					TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
			}

			// Check the driver has written something
			for (deUint32	displayIndex = 0;
							displayIndex < displayCountExpected;
							displayIndex++)
			{
				const VkDisplayKHR display = displaysForPlane[displayIndex];

				if (display != nullDisplay)
				{
					if (!de::contains(displaySet, display))
					{
						TCU_FAIL_STR("Invalid display handle " + de::toString(display));
					}
				}
			}

			// Check the driver has not written more than requested
			if (displaysForPlane[displayCountExpected] != canaryDisplay)
				TCU_FAIL("Memory damage detected: driver has written more than expected");
		}
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display mode properties coverage test
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetDisplayModePropertiesKHR (void)
{
	VkResult		result;
	DisplayVector	displaysVector;

	if (!getDisplays(displaysVector))
		TCU_FAIL("Failed to retrieve displays list");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	for (DisplayVector::iterator	it =  displaysVector.begin();
									it != displaysVector.end();
									it++)
	{
		VkDisplayKHR	display				= *it;
		deUint32		modesCountReported	= 0u;

		result = m_vki.getDisplayModePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
													display,				// VkDisplayKHR					display
													&modesCountReported,	// uint32_t*					pPropertyCount
													DE_NULL);				// VkDisplayModePropertiesKHR*	pProperties

		// Test the call correctly writes data in various size arrays
		for (deUint32	modesCountRequested = 0;
						modesCountRequested < modesCountReported + 2;
						modesCountRequested = nextTestNumber(modesCountRequested, modesCountReported + 2))
		{
			const deUint32							modesCountExpected	=	std::min(modesCountRequested, modesCountReported);
			const VkDisplayModeKHR					nullDisplayMode		=	DE_NULL;
			const VkDisplayModePropertiesKHR		nullMode			=	{
																				nullDisplayMode,	// VkDisplayModeKHR				displayMode
																				{					// VkDisplayModeParametersKHR	parameters
																					{0, 0},			// VkExtent2D					visibleRegion
																					0				// uint32_t						refreshRate
																				}
																			};
			const VkDisplayModeKHR					canaryDisplayMode	=	static_cast<VkDisplayModeKHR>(0xABCDEF11);
			const deUint32							canaryItemCount		=	1;
			std::vector<VkDisplayModePropertiesKHR>	modesForDisplay			(modesCountRequested + canaryItemCount, nullMode);
			deUint32								modesCountRetrieved	=	modesCountRequested;

			modesForDisplay[modesCountExpected].displayMode = canaryDisplayMode;

			result = m_vki.getDisplayModePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
														display,				// VkDisplayKHR					display
														&modesCountRetrieved,	// uint32_t*					pPropertyCount
														&modesForDisplay[0]);	// VkDisplayModePropertiesKHR*	pProperties

			// Check amount of data written equals to expected
			if (modesCountRetrieved != modesCountExpected)
				TCU_FAIL_STR(	string("modesCountRetrieved != modesCountExpected, ") +
								de::toString(modesCountRetrieved) + " != " + de::toString(modesCountExpected));

			if (modesCountRequested >= modesCountReported)
			{
				if (result != VK_SUCCESS)
					TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
			}
			else
			{
				if (result != VK_INCOMPLETE)
					TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
			}

			// Check the driver has written something
			for (deUint32	modeIndex = 0;
							modeIndex < modesCountExpected;
							modeIndex++)
			{
				const VkDisplayModePropertiesKHR theModeProperties = modesForDisplay[modeIndex];

				if (theModeProperties.displayMode == nullMode.displayMode)
					TCU_FAIL_STR("Invalid mode display handle reported for display " + de::toString(display));
			}

			// Check the driver has not written more than requested
			if (modesForDisplay[modesCountExpected].displayMode != canaryDisplayMode)
				TCU_FAIL("Memory damage detected: driver has written more than expected");
		}
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Create display mode coverage test
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus	DisplayCoverageTestInstance::testCreateDisplayModeKHR (void)
{
	DisplayVector	displaysVector;
	VkResult		result;

	if (!getDisplays(displaysVector))
		TCU_FAIL("Failed to retrieve displays");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	for (DisplayVector::iterator	it =  displaysVector.begin();
									it != displaysVector.end();
									it++)
	{
		const VkDisplayKHR						display				=	*it;
		DisplayModePropertiesVector::size_type	builtinModesCount	=	0u;
		VkDisplayModePropertiesKHR				validModeProperties;
		VkDisplayModeKHR						mode				=	DE_NULL;
		DisplayModePropertiesVector				modes;
		VkDisplayModeCreateInfoKHR				createInfo			=	{
																			VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR,	// VkStructureType				sType
																			DE_NULL,										// const void*					pNext
																			0,												// VkDisplayModeCreateFlagsKHR	flags
																			{												// VkDisplayModeParametersKHR	parameters
																				{0, 0},										// VkExtent2D					visibleRegion
																				0											// uint32_t						refreshRate
																			}
																		};

		if (!getDisplayModeProperties(display, modes))
			TCU_FAIL("Failed to retrieve display mode properties");

		if (modes.size() < 1)
			TCU_FAIL("At least one mode expected to be returned");

		// Builtin mode count should not be updated with a new mode. Get initial builtin mode count
		builtinModesCount = modes.size();

		// Assume first available builtin mode as a valid mode sample
		validModeProperties = modes[0];

		// Do negative test by making one of parameters unacceptable
		for (deUint32	testIndex = 0;
						testIndex < 3;
						testIndex++)
		{
			VkDisplayModeCreateInfoKHR	createInfoFail		(createInfo);
			VkDisplayModeKHR			modeFail		=	DE_NULL;

			createInfoFail.parameters = validModeProperties.parameters;

			switch (testIndex)
			{
				case 0:		createInfoFail.parameters.refreshRate			= 0;	break;
				case 1:		createInfoFail.parameters.visibleRegion.width	= 0;	break;
				case 2:		createInfoFail.parameters.visibleRegion.height	= 0;	break;
				default:	DE_FATAL("Impossible");									break;
			}

			result = m_vki.createDisplayModeKHR(	m_physicalDevice,	// VkPhysicalDevice						physicalDevice
													display,			// VkDisplayKHR							display
													&createInfoFail,	// const VkDisplayModeCreateInfoKHR*	pCreateInfo
													DE_NULL,			// const VkAllocationCallbacks*			pAllocator
													&modeFail);			// VkDisplayModeKHR*					pMode

			if (result != VK_ERROR_INITIALIZATION_FAILED)
				TCU_FAIL_STR(string("Expected VK_ERROR_INITIALIZATION_FAILED. Have ") + getResultAsString(result));

			if (modeFail != DE_NULL)
				TCU_FAIL("Mode should be kept invalid on fail");
		}

		// At last create valid display mode
		createInfo.parameters = validModeProperties.parameters;

		result = m_vki.createDisplayModeKHR(	m_physicalDevice,	// VkPhysicalDevice						physicalDevice
												display,			// VkDisplayKHR							display
												&createInfo,		// const VkDisplayModeCreateInfoKHR*	pCreateInfo
												DE_NULL,			// const VkAllocationCallbacks*			pAllocator
												&mode);				// VkDisplayModeKHR*					pMode

		if (result != VK_SUCCESS)
			TCU_FAIL_STR("Expected VK_SUCCESS. Have " + getResultAsString(result));

		if (mode == DE_NULL)
			TCU_FAIL("Valid handle expected");

		// Builtin mode count should not be updated with a new mode
		modes.clear();

		if (!getDisplayModeProperties(display, modes))
			TCU_FAIL("Failed to retrieve display mode properties");

		if (builtinModesCount != modes.size())
			TCU_FAIL_STR(	string("Mode count has changed from ") + de::toString(builtinModesCount) +
							string(" to ") + de::toString(modes.size()));
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display-plane capabilities coverage test
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus	DisplayCoverageTestInstance::testGetDisplayPlaneCapabilitiesKHR (void)
{
	deUint32	planeCountReported	=	0u;
	VkResult	result;

	result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																&planeCountReported,	// uint32_t*					pPropertyCount
																DE_NULL);				// VkDisplayPlanePropertiesKHR*	pProperties

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
	{
		DisplayVector	displaysVector;

		// If we don't have any displays then it's alright to have no planes, as
		// per the Vulkan Spec:
		//		Devices must support at least one plane on each display
		if (!getDisplays(displaysVector))
			TCU_FAIL("Failed to retrieve displays");

		if (displaysVector.empty())
			TCU_THROW(NotSupportedError, "No display planes reported");

		TCU_FAIL("No planes defined");
	}

	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountReported = MAX_TESTED_PLANE_COUNT;
	}

	for (deUint32	planeIndex = 0;
					planeIndex < planeCountReported;
					planeIndex++)
	{
		std::vector<VkDisplayKHR> displaysForPlane;

		if (!getDisplaysForPlane(planeIndex, displaysForPlane))
			TCU_FAIL_STR("Failed to retrieve displays list for plane " + de::toString(planeIndex));

		if (displaysForPlane.empty())
			continue;

		// Check the driver has written something
		for (deUint32	displayIndex = 0;
						displayIndex < displaysForPlane.size();
						displayIndex++)
		{
			const VkDisplayKHR						display						=	displaysForPlane[displayIndex];
			std::vector<VkDisplayModePropertiesKHR>	modesPropertiesForDisplay;

			if (!getDisplayModeProperties(display, modesPropertiesForDisplay))
				TCU_FAIL("Failed to retrieve display mode properties");

			for (deUint32	modeIndex = 0;
							modeIndex < modesPropertiesForDisplay.size();
							modeIndex++)
			{
				const VkDisplayModeKHR			theDisplayMode			=	modesPropertiesForDisplay[modeIndex].displayMode;
				const deUint32					unrecognizedAlphaFlags	=	~RECOGNIZED_DISPLAY_PLANE_ALPHA_FLAGS;
				VkDisplayPlaneCapabilitiesKHR	planeCapabilities		=	{
																				unrecognizedAlphaFlags,	// VkDisplayPlaneAlphaFlagsKHR	supportedAlpha;
																				{ -1, -1 },				// VkOffset2D					minSrcPosition;
																				{ -1, -1 },				// VkOffset2D					maxSrcPosition;
																				{ 1, 1 },				// VkExtent2D					minSrcExtent;
																				{ 0, 0 },				// VkExtent2D					maxSrcExtent;
																				{ 1, 1 },				// VkOffset2D					minDstPosition;
																				{ 0, 0 },				// VkOffset2D					maxDstPosition;
																				{ 1, 1 },				// VkExtent2D					minDstExtent;
																				{ 0, 0 },				// VkExtent2D					maxDstExtent;
																			};
				tcu::ResultCollector			results						(m_log);

				result = m_vki.getDisplayPlaneCapabilitiesKHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																theDisplayMode,			// VkDisplayModeKHR					mode
																planeIndex,				// uint32_t							planeIndex
																&planeCapabilities);	// VkDisplayPlaneCapabilitiesKHR*	pCapabilities

				results.check(	result == VK_SUCCESS,
								string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

				results.check(	(planeCapabilities.supportedAlpha & unrecognizedAlphaFlags) == 0,
								"supportedAlpha contains unrecognized value");

				results.check(	planeCapabilities.minSrcPosition.x >= 0,
								"minSrcPosition.x >= 0");

				results.check(	planeCapabilities.minSrcPosition.y >= 0,
								"minSrcPosition.y >= 0");

				results.check(	planeCapabilities.maxSrcPosition.x >= 0,
								"maxSrcPosition.x >= 0");

				results.check(	planeCapabilities.maxSrcPosition.y >= 0,
								"maxSrcPosition.y >= 0");

				results.check(	planeCapabilities.minSrcPosition.x <= planeCapabilities.maxSrcPosition.x,
								"minSrcPosition.x <= maxSrcPosition.x");

				results.check(	planeCapabilities.minSrcPosition.y <= planeCapabilities.maxSrcPosition.y,
								"minSrcPosition.y <= maxSrcPosition.y");

				results.check(	planeCapabilities.minDstPosition.x <= planeCapabilities.maxDstPosition.x,
								"minDstPosition.x <= maxDstPosition.x");

				results.check(	planeCapabilities.minDstPosition.y <= planeCapabilities.maxDstPosition.y,
								"minDstPosition.y <= maxDstPosition.y");

				results.check(	planeCapabilities.minSrcExtent.width <= planeCapabilities.maxSrcExtent.width,
								"minSrcExtent.width <= maxSrcExtent.width");

				results.check(	planeCapabilities.minSrcExtent.height <= planeCapabilities.maxSrcExtent.height,
								"minSrcExtent.height <= maxSrcExtent.height");

				results.check(	planeCapabilities.minDstExtent.width <= planeCapabilities.maxDstExtent.width,
								"minDstExtent.width <= maxDstExtent.width");

				results.check(	planeCapabilities.minDstExtent.height <= planeCapabilities.maxDstExtent.height,
								"minDstExtent.height <= maxDstExtent.height");

				if (results.getResult() != QP_TEST_RESULT_PASS)
				{
					m_log	<< tcu::TestLog::Message
							<< "Error detected " << results.getMessage()
							<< " for plane's " << planeIndex
							<< " display " << displayIndex
							<< " and mode " << modeIndex
							<< " with capabilities " << planeCapabilities
							<< tcu::TestLog::EndMessage;

					TCU_FAIL_STR(results.getMessage());
				}

			}
		}
	}

	return tcu::TestStatus::pass("pass");
}

namespace
{
	struct SurfaceCountersError : public std::runtime_error
	{
		SurfaceCountersError(const std::string& what_) : std::runtime_error(what_) {}
	};
}

/*--------------------------------------------------------------------*//*!
 * \brief Test display surface creation or counters.
 *
 * In the counter variant, it needs VK_EXT_display_surface_counter
 * and checks the available surface counters.
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus	DisplayCoverageTestInstance::testDisplaySurface (SurfaceTestKind testKind)
{
	deUint32									planeCountReported	=	0u;
	deUint32									planeCountTested	=	0u;
	deUint32									planeCountRetrieved	=	0u;
	std::vector<VkDisplayPlanePropertiesKHR>	planeProperties;
	bool										testPerformed		=	false;
	DisplayVector								displaysVector;
	VkResult									result;
	std::string									surfaceCountersErr;

	DE_ASSERT(testKind >= 0 && testKind < SURFACE_TEST_KIND_MAX_ENUM);

	// Check the needed extension.
	if (testKind == SURFACE_COUNTERS && (!isInstanceExtensionSupported(m_context.getUsedApiVersion(), m_context.getInstanceExtensions(), "VK_EXT_display_surface_counter")))
		TCU_THROW(NotSupportedError, "VK_EXT_display_surface_counter not supported");

	// Get displays
	if (!getDisplays(displaysVector))
		TCU_FAIL("Failed to retrieve displays");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	// Get planes
	result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																&planeCountReported,	// uint32_t*					pPropertyCount
																DE_NULL);				// VkDisplayPlanePropertiesKHR*	pProperties

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
		TCU_FAIL("No planes defined");

	planeCountTested = planeCountReported;
	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountTested = MAX_TESTED_PLANE_COUNT;
	}

	planeProperties.resize(planeCountTested);
	planeCountRetrieved = planeCountTested;

	result = m_vki.getPhysicalDeviceDisplayPlanePropertiesKHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
																&planeCountRetrieved,	// uint32_t*					pPropertyCount
																&planeProperties[0]);	// VkDisplayPlanePropertiesKHR*	pProperties

	if (result != VK_SUCCESS && result != VK_INCOMPLETE )
		TCU_FAIL_STR(string("Expected VK_SUCCESS or VK_INCOMPLETE expected. Have ") + getResultAsString(result));

	if (planeCountRetrieved != planeCountTested)
		TCU_FAIL_STR(	string("Number of planes requested (") + de::toString(planeCountTested) +
						") does not match retrieved (" + de::toString(planeCountRetrieved) + ")");

	// Iterate through displays-modes
	for (DisplayVector::iterator	it =  displaysVector.begin();
									it != displaysVector.end();
									it++)
	{
		const VkDisplayKHR						display						=	*it;
		std::vector<VkDisplayModePropertiesKHR>	modesPropertiesForDisplay;

		if (!getDisplayModeProperties(display, modesPropertiesForDisplay))
			TCU_FAIL("Failed to retrieve display mode properties");

		for (deUint32	modeIndex = 0;
						modeIndex < modesPropertiesForDisplay.size();
						modeIndex++)
		{
			const VkDisplayModeKHR				displayMode		=	modesPropertiesForDisplay[modeIndex].displayMode;
			const VkDisplayModePropertiesKHR&	modeProperties	=	modesPropertiesForDisplay[modeIndex];

			for (deUint32	planeIndex = 0;
							planeIndex < planeCountTested;
							planeIndex++)
			{
				std::vector<VkDisplayKHR>	displaysForPlane;

				if (!getDisplaysForPlane(planeIndex, displaysForPlane))
					TCU_FAIL_STR("Failed to retrieve displays list for plane " + de::toString(planeIndex));

				if (displaysForPlane.empty())
					continue;

				// Iterate through displays supported by the plane
				for (deUint32	displayIndex = 0;
								displayIndex < displaysForPlane.size();
								displayIndex++)
				{
					const VkDisplayKHR				planeDisplay		=	displaysForPlane[displayIndex];
					VkDisplayPlaneCapabilitiesKHR	planeCapabilities;
					bool							fullDisplayPlane;

					if (display == planeDisplay)
					{
						deMemset(&planeCapabilities, 0, sizeof(planeCapabilities));

						result = m_vki.getDisplayPlaneCapabilitiesKHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																		displayMode,			// VkDisplayModeKHR					mode
																		planeIndex,				// uint32_t							planeIndex
																		&planeCapabilities);	// VkDisplayPlaneCapabilitiesKHR*	pCapabilities

						if (result != VK_SUCCESS)
							TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

						fullDisplayPlane	=	   planeCapabilities.minDstExtent.height == modeProperties.parameters.visibleRegion.height
												&& planeCapabilities.minDstExtent.width  == modeProperties.parameters.visibleRegion.width;

						if (fullDisplayPlane && (planeCapabilities.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR) != 0)
						{
							const VkDisplayPlaneAlphaFlagBitsKHR	alphaMode	=	VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
							const VkInstance						instance	=	m_context.getInstance();
							const VkDisplaySurfaceCreateInfoKHR		createInfo	=	{
																						VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,	// VkStructureType					sType
																						DE_NULL,											// const void*						pNext
																						0,													// VkDisplaySurfaceCreateFlagsKHR	flags
																						displayMode,										// VkDisplayModeKHR					displayMode
																						planeIndex,											// uint32_t							planeIndex
																						planeProperties[planeIndex].currentStackIndex,		// uint32_t							planeStackIndex
																						VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,				// VkSurfaceTransformFlagBitsKHR	transform
																						1.0f,												// float							globalAlpha
																						alphaMode,											// VkDisplayPlaneAlphaFlagBitsKHR	alphaMode
																						{													// VkExtent2D						imageExtent
																							planeCapabilities.minDstExtent.width,
																							planeCapabilities.minDstExtent.height
																						}
																					};
							VkSurfaceKHR							surface		=	DE_NULL;

							result = m_vki.createDisplayPlaneSurfaceKHR(	instance,		// VkInstance							instance
																			&createInfo,	// const VkDisplaySurfaceCreateInfoKHR*	pCreateInfo
																			DE_NULL,		// const VkAllocationCallbacks*			pAllocator
																			&surface);		// VkSurfaceKHR*						pSurface

							if (result != VK_SUCCESS)
								TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

							if (surface == DE_NULL)
								TCU_FAIL("Invalid surface handle returned");

							if (testKind == SURFACE_COUNTERS)
							{
								// Check surface counters.
								try
								{
									const vk::VkSurfaceCapabilities2EXT	capsExt = vk::wsi::getPhysicalDeviceSurfaceCapabilities2EXT	(m_vki, m_physicalDevice, surface);
									const vk::VkSurfaceCapabilitiesKHR	capsKhr = vk::wsi::getPhysicalDeviceSurfaceCapabilities		(m_vki, m_physicalDevice, surface);

									if (!vk::wsi::sameSurfaceCapabilities(capsKhr, capsExt))
									{
										throw SurfaceCountersError("KHR and EXT surface capabilities do not match");
									}

									for (deUint32 i = 0; i < sizeof(capsExt.supportedSurfaceCounters) * 8; ++i)
									{
										deUint32 mask = (1<<i);
										if (capsExt.supportedSurfaceCounters & mask)
										{
											if (mask != static_cast<deUint32>(VK_SURFACE_COUNTER_VBLANK_EXT))
											{
												std::ostringstream msg;
												msg << "Invalid bit set in supportedSurfaceCounters: 0x" << std::hex << mask;
												throw SurfaceCountersError(msg.str());
											}
										}
									}
								}
								catch(const SurfaceCountersError& err)
								{
									surfaceCountersErr = err.what();
								}
							}

							m_vki.destroySurfaceKHR(	instance,	// VkInstance							instance
														surface,	// VkSurfaceKHR*						pSurface
														DE_NULL);	// const VkAllocationCallbacks*			pAllocator

							testPerformed = true;
						}
					}
				}
			}
		}
	}

	if (!testPerformed)
		TCU_THROW(NotSupportedError, "Cannot find suitable parameters for the test");

	return ((surfaceCountersErr.empty()) ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail(surfaceCountersErr));
}

/*--------------------------------------------------------------------*//*!
 * \brief Display enumeration coverage test using VK_KHR_get_display_properties2
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetPhysicalDeviceDisplayProperties2KHR (void)
{
	deUint32				displayCountReported	=	0u;
	deUint32				displayCountToTest		=	0u;
	tcu::ResultCollector	results						(m_log);
	VkResult				result;

	result = m_vki.getPhysicalDeviceDisplayProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice			physicalDevice
															&displayCountReported,	// uint32_t*				pPropertyCount
															DE_NULL);				// VkDisplayProperties2KHR*	pProperties

	if (   result != VK_SUCCESS
		&& result != VK_INCOMPLETE
		&& result != VK_ERROR_OUT_OF_HOST_MEMORY
		&& result != VK_ERROR_OUT_OF_DEVICE_MEMORY
		)
	{
		TCU_FAIL_STR(string("Invalid result ") + getResultAsString(result));
	}

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (displayCountReported == 0)
		TCU_THROW(NotSupportedError, std::string("Cannot perform test: no displays found").c_str());

	displayCountToTest = displayCountReported;
	if (displayCountReported > MAX_TESTED_DISPLAY_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of displays reported is too high " << displayCountReported
				<< ". Test is limited to " << MAX_TESTED_DISPLAY_COUNT
				<< tcu::TestLog::EndMessage;

		displayCountToTest = MAX_TESTED_DISPLAY_COUNT;
	}

	// Test the call correctly writes data in various size arrays
	for (deUint32	displayCountRequested = 0;
					displayCountRequested < displayCountToTest + 2;
					displayCountRequested++)
	{
		const deUint32							displayCountExpected			=	std::min(displayCountRequested, displayCountReported);
		const VkDisplayPropertiesKHR			nonUpdatedDisplayProperties		=	{	// Most values are set to fail the test to make sure driver updates them
																						DE_NULL,								// VkDisplayKHR					display
																						DE_NULL,								// const char*					displayName
																						{0, 0},									// VkExtent2D					physicalDimensions
																						{0, 0},									// VkExtent2D					physicalResolution
																						~RECOGNIZED_SURFACE_TRANSFORM_FLAGS,	// VkSurfaceTransformFlagsKHR	supportedTransforms
																						(vk::VkBool32)(VK_TRUE + 1),			// VkBool32						planeReorderPossible
																						(vk::VkBool32)(VK_TRUE + 1)				// VkBool32						persistentContent
																					};
		const VkStructureType					queryStructureType				=	VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR;
		const VkDisplayProperties2KHR			nonUpdatedDisplayProperties2	=	{
																						queryStructureType,			// VkStructureType			sType
																						DE_NULL,					// void*					pNext
																						nonUpdatedDisplayProperties	// VkDisplayPropertiesKHR	displayProperties
																					};
		const VkDisplayKHR						canaryDisplay					=	static_cast<VkDisplayKHR>(0xABCDEF11);
		const deUint32							canaryItemCount					=	1;
		std::vector<VkDisplayProperties2KHR>	displaysProps2						(displayCountRequested + canaryItemCount, nonUpdatedDisplayProperties2);
		deUint32								displayCountRetrieved			=	displayCountRequested;
		DisplaySet								displaySet;

		displaysProps2[displayCountExpected].displayProperties.display = canaryDisplay;

		result = m_vki.getPhysicalDeviceDisplayProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice			physicalDevice
																&displayCountRetrieved,	// uint32_t*				pPropertyCount
																&displaysProps2[0]);	// VkDisplayProperties2KHR*	pProperties

		// Check amount of data written equals to expected
		if (displayCountRetrieved != displayCountExpected)
			TCU_FAIL_STR(	string("displayCountRetrieved != displayCountExpected, ") +
							de::toString(displayCountRetrieved) + " != " + de::toString(displayCountExpected));

		if (displayCountRequested >= displayCountReported)
		{
			if (result != VK_SUCCESS)
				TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
		}
		else
		{
			if (result != VK_INCOMPLETE)
				TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
		}

		// Check the driver has written something
		for (deUint32	displayIndex = 0;
						displayIndex < displayCountRetrieved;
						displayIndex++)
		{
			const VkDisplayProperties2KHR&	properties2 = displaysProps2[displayIndex];
			const VkDisplayPropertiesKHR&	properties	= properties2.displayProperties;

			displaySet.insert(properties.display);

			results.check(	properties2.sType == queryStructureType,
							"sType has changed to " + de::toString(properties2.sType));

			results.check(	properties2.pNext == DE_NULL,
							"pNext has changed to " + de::toString(properties2.pNext));

			validateDisplayProperties(results, properties, nonUpdatedDisplayProperties);

			if (results.getResult() != QP_TEST_RESULT_PASS)
			{
				m_log	<< tcu::TestLog::Message
						<< "Error detected " << results.getMessage()
						<< " for display " << displayIndex << " with properties " << properties2
						<< " non updated display properties are " << nonUpdatedDisplayProperties2
						<< tcu::TestLog::EndMessage;

				TCU_FAIL_STR(results.getMessage());
			}
		}

		// Check the driver has not written more than requested
		if (displaysProps2[displayCountExpected].displayProperties.display != canaryDisplay)
			TCU_FAIL("Memory damage detected: driver has written more than expected");

		// Check display handle uniqueness
		if (displaySet.size() != displayCountRetrieved)
			TCU_FAIL("Display handle duplication detected");
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Plane enumeration coverage test using VK_KHR_get_display_properties2
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetPhysicalDeviceDisplayPlaneProperties2KHR (void)
{
	DisplayVector			displaysVector;
	DisplaySet				displaySet;
	deUint32				planeCountReported	=	0u;
	deUint32				planeCountTested	=	0u;
	tcu::ResultCollector	results					(m_log);
	VkResult				result;

	// Create a list of displays available
	if (!getDisplays2(displaysVector))
		TCU_FAIL("Failed to retrieve displays");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	displaySet = DisplaySet(displaysVector.begin(), displaysVector.end());

	// Get planes to test
	result = m_vki.getPhysicalDeviceDisplayPlaneProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																&planeCountReported,	// uint32_t*						pPropertyCount
																DE_NULL);				// VkDisplayPlaneProperties2KHR*	pProperties

	if (   result != VK_SUCCESS
		&& result != VK_INCOMPLETE
		&& result != VK_ERROR_OUT_OF_HOST_MEMORY
		&& result != VK_ERROR_OUT_OF_DEVICE_MEMORY
		)
	{
		TCU_FAIL_STR(string("Invalid result ") + getResultAsString(result));
	}

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
		TCU_THROW(ResourceError, "Cannot perform test: no planes found");

	planeCountTested = planeCountReported;
	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountTested = MAX_TESTED_PLANE_COUNT;
	}

	// Test the call correctly writes data in various size arrays
	for (deUint32	planeCountRequested = 0;
					planeCountRequested < planeCountTested + 2;
					planeCountRequested++)
	{
		const deUint32								planeCountExpected			=	std::min(planeCountRequested, planeCountReported);
		const VkDisplayPlanePropertiesKHR			nonUpdatedPlaneProperties	=	{	// Most values are set to fail the test to make sure driver updates them
																						DE_NULL,			// VkDisplayKHR	currentDisplay
																						planeCountReported	// deUint32		currentStackIndex
																					};
		const VkStructureType						queryStructureType			=	VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR;
		const VkDisplayPlaneProperties2KHR			nonUpdatedPlaneProperties2	=	{
																						queryStructureType,			// VkStructureType				sType
																						DE_NULL,					// void*						pNext
																						nonUpdatedPlaneProperties	// VkDisplayPlanePropertiesKHR	displayPlaneProperties
																					};
		const VkDisplayKHR							canaryDisplay				=	static_cast<VkDisplayKHR>(0xABCDEF11);
		const deUint32								canaryItemCount				=	1;
		std::vector<VkDisplayPlaneProperties2KHR>	planeProps2						(planeCountRequested + canaryItemCount, nonUpdatedPlaneProperties2);
		deUint32									planeCountRetrieved			=	planeCountRequested;

		planeProps2[planeCountExpected].displayPlaneProperties.currentDisplay = canaryDisplay;

		result = m_vki.getPhysicalDeviceDisplayPlaneProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																	&planeCountRetrieved,	// uint32_t*						pPropertyCount
																	&planeProps2[0]);		// VkDisplayPlaneProperties2KHR*	pProperties

		// Check amount of data written equals to expected
		if (planeCountRetrieved != planeCountExpected)
			TCU_FAIL_STR(	string("planeCountRetrieved != planeCountExpected, ") +
							de::toString(planeCountRetrieved) + " != " + de::toString(planeCountExpected));

		if (planeCountRequested >= planeCountReported)
		{
			if (result != VK_SUCCESS)
				TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
		}
		else
		{
			if (result != VK_INCOMPLETE)
				TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
		}

		// Check the driver has written something
		for (deUint32	planeIndex = 0;
						planeIndex < planeCountRetrieved;
						planeIndex++)
		{
			const VkDisplayPlaneProperties2KHR&	properties2		= planeProps2[planeIndex];
			const VkDisplayPlanePropertiesKHR&	properties		= properties2.displayPlaneProperties;

			results.check(	properties2.sType == queryStructureType,
							"sType has changed to " + de::toString(properties2.sType));

			results.check(	properties2.pNext == DE_NULL,
							"pNext has changed to " + de::toString(properties2.pNext));

			validateDisplayPlaneProperties(results, properties, nonUpdatedPlaneProperties, displaySet);

			if (results.getResult() != QP_TEST_RESULT_PASS)
			{
				m_log	<< tcu::TestLog::Message
						<< "Error detected " << results.getMessage()
						<< " for plane " << planeIndex << " with properties " << properties2
						<< tcu::TestLog::EndMessage;

				TCU_FAIL_STR(results.getMessage());
			}
		}

		// Check the driver has not written more than requested
		if (planeProps2[planeCountExpected].displayPlaneProperties.currentDisplay != canaryDisplay)
			TCU_FAIL("Memory damage detected: driver has written more than expected");
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display-plane capabilities coverage test using VK_KHR_get_display_properties2
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus	DisplayCoverageTestInstance::testGetDisplayPlaneCapabilities2KHR (void)
{
	deUint32	planeCountReported	=	0u;
	VkResult	result;

	result = m_vki.getPhysicalDeviceDisplayPlaneProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																&planeCountReported,	// uint32_t*						pPropertyCount
																DE_NULL);				// VkDisplayPlaneProperties2KHR*	pProperties

	if (result != VK_SUCCESS)
		TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

	if (planeCountReported == 0)
		TCU_THROW(NotSupportedError, "No display plane reported");

	if (planeCountReported > MAX_TESTED_PLANE_COUNT)
	{
		m_log	<< tcu::TestLog::Message
				<< "Number of planes reported is too high " << planeCountReported
				<< ". Test is limited to " << MAX_TESTED_PLANE_COUNT
				<< tcu::TestLog::EndMessage;

		planeCountReported = MAX_TESTED_PLANE_COUNT;
	}

	for (deUint32	planeIndex = 0;
					planeIndex < planeCountReported;
					planeIndex++)
	{
		std::vector<VkDisplayKHR> displaysForPlane;

		if (!getDisplaysForPlane(planeIndex, displaysForPlane))
			TCU_FAIL_STR("Failed to retrieve displays list for plane " + de::toString(planeIndex));

		if (displaysForPlane.empty())
			continue;

		// Check the driver has written something
		for (deUint32	displayIndex = 0;
						displayIndex < displaysForPlane.size();
						displayIndex++)
		{
			const VkDisplayKHR							display						=	displaysForPlane[displayIndex];
			std::vector<VkDisplayModeProperties2KHR>	modesPropertiesForDisplay;

			if (!getDisplayModeProperties2(display, modesPropertiesForDisplay))
				TCU_FAIL("Failed to retrieve display mode properties");

			for (deUint32	modeIndex = 0;
							modeIndex < modesPropertiesForDisplay.size();
							modeIndex++)
			{
				const VkDisplayModeKHR			displayMode				=	modesPropertiesForDisplay[modeIndex].displayModeProperties.displayMode;
				const deUint32					unrecognizedAlphaFlags	=	~RECOGNIZED_DISPLAY_PLANE_ALPHA_FLAGS;
				const VkDisplayPlaneInfo2KHR	planeInfo2				=	{
																				VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR, // VkStructureType	sType
																				DE_NULL,									// const void*		pNext
																				displayMode,								// VkDisplayModeKHR	mode
																				planeIndex									// uint32_t			planeIndex
																			};
				VkDisplayPlaneCapabilitiesKHR	planeCapabilities		=	{
																				unrecognizedAlphaFlags,	// VkDisplayPlaneAlphaFlagsKHR	supportedAlpha
																				{ -1, -1 },				// VkOffset2D					minSrcPosition
																				{ -1, -1 },				// VkOffset2D					maxSrcPosition
																				{ 1, 1 },				// VkExtent2D					minSrcExtent
																				{ 0, 0 },				// VkExtent2D					maxSrcExtent
																				{ 1, 1 },				// VkOffset2D					minDstPosition
																				{ 0, 0 },				// VkOffset2D					maxDstPosition
																				{ 1, 1 },				// VkExtent2D					minDstExtent
																				{ 0, 0 },				// VkExtent2D					maxDstExtent
																			};
				const VkStructureType			queryStructureType		=	VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR;
				VkDisplayPlaneCapabilities2KHR	planeCapabilities2		=	{
																				queryStructureType,	// VkStructureType					sType
																				DE_NULL,			// void*							pNext
																				planeCapabilities	// VkDisplayPlaneCapabilitiesKHR	capabilities
																			};
				tcu::ResultCollector			results						(m_log);

				result = m_vki.getDisplayPlaneCapabilities2KHR(	m_physicalDevice,		// VkPhysicalDevice					physicalDevice
																&planeInfo2,			// const VkDisplayPlaneInfo2KHR*	pDisplayPlaneInfo
																&planeCapabilities2);	// VkDisplayPlaneCapabilities2KHR*	pCapabilities

				results.check(	result == VK_SUCCESS,
								string("Expected VK_SUCCESS. Have ") + getResultAsString(result));

				results.check(	planeCapabilities2.sType == queryStructureType,
								"sType has changed to " + de::toString(planeCapabilities2.sType));

				results.check(	planeCapabilities2.pNext == DE_NULL,
								"pNext has changed to " + de::toString(planeCapabilities2.pNext));

				// Validate results returned by driver in planeCapabilities2 using non-updated planeCapabilities
				validateDisplayPlaneCapabilities(results, planeCapabilities2.capabilities, planeCapabilities);

				if (results.getResult() != QP_TEST_RESULT_PASS)
				{
					m_log	<< tcu::TestLog::Message
							<< "Error detected " << results.getMessage()
							<< " for plane's " << planeIndex
							<< " display " << displayIndex
							<< " and mode " << modeIndex
							<< " with capabilities " << planeCapabilities2
							<< tcu::TestLog::EndMessage;

					TCU_FAIL_STR(results.getMessage());
				}
			}
		}
	}

	return tcu::TestStatus::pass("pass");
}

/*--------------------------------------------------------------------*//*!
 * \brief Display mode properties coverage test using VK_KHR_get_display_properties2
 *
 * Throws an exception on fail.
 *
 * \return tcu::TestStatus::pass on success
 *//*--------------------------------------------------------------------*/
tcu::TestStatus DisplayCoverageTestInstance::testGetDisplayModeProperties2KHR (void)
{
	VkResult				result;
	tcu::ResultCollector	results			(m_log);
	DisplayVector			displaysVector;

	if (!getDisplays2(displaysVector))
		TCU_FAIL("Failed to retrieve displays list");

	if (displaysVector.empty())
		TCU_THROW(NotSupportedError, "No displays reported");

	for (DisplayVector::iterator	it =  displaysVector.begin();
									it != displaysVector.end();
									it++)
	{
		VkDisplayKHR	display				= *it;
		deUint32		modesCountReported	= 0u;

		result = m_vki.getDisplayModeProperties2KHR(	m_physicalDevice,		// VkPhysicalDevice				physicalDevice
														display,				// VkDisplayKHR					display
														&modesCountReported,	// uint32_t*					pPropertyCount
														DE_NULL);				// VkDisplayModeProperties2KHR*	pProperties

		// Test the call correctly writes data in various size arrays
		for (deUint32	modesCountRequested = 0;
						modesCountRequested < modesCountReported + 2;
						modesCountRequested = nextTestNumber(modesCountRequested, modesCountReported + 2))
		{
			const deUint32								modesCountExpected			=	std::min(modesCountRequested, modesCountReported);
			const VkDisplayModeKHR						nullDisplayMode				=	DE_NULL;
			const VkDisplayModePropertiesKHR			nonUpdatedModeProperties	=	{
																							nullDisplayMode,	// VkDisplayModeKHR				displayMode
																							{					// VkDisplayModeParametersKHR	parameters
																								{0, 0},			// VkExtent2D					visibleRegion
																								0				// uint32_t						refreshRate
																							}
																						};
			const VkStructureType						queryStructureType			=	VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR;
			const VkDisplayModeProperties2KHR			nonUpdatedModeProperties2	=	{
																							queryStructureType,			// VkStructureType				sType
																							DE_NULL,					// void*						pNext
																							nonUpdatedModeProperties	// VkDisplayModePropertiesKHR	displayModeProperties
																						};
			const VkDisplayModeKHR						canaryDisplayMode			=	static_cast<VkDisplayModeKHR>(0xABCDEF11);
			const deUint32								canaryItemCount				=	1;
			std::vector<VkDisplayModeProperties2KHR>	modesProperties2				(modesCountRequested + canaryItemCount, nonUpdatedModeProperties2);
			deUint32									modesCountRetrieved			=	modesCountRequested;

			modesProperties2[modesCountExpected].displayModeProperties.displayMode = canaryDisplayMode;

			result = m_vki.getDisplayModeProperties2KHR(m_physicalDevice,		// VkPhysicalDevice				physicalDevice
														display,				// VkDisplayKHR					display
														&modesCountRetrieved,	// uint32_t*					pPropertyCount
														&modesProperties2[0]);	// VkDisplayModeProperties2KHR*	pProperties

			// Check amount of data written equals to expected
			if (modesCountRetrieved != modesCountExpected)
				TCU_FAIL_STR(	string("modesCountRetrieved != modesCountExpected, ") +
								de::toString(modesCountRetrieved) + " != " + de::toString(modesCountExpected));

			if (modesCountRequested >= modesCountReported)
			{
				if (result != VK_SUCCESS)
					TCU_FAIL_STR(string("Expected VK_SUCCESS. Have ") + getResultAsString(result));
			}
			else
			{
				if (result != VK_INCOMPLETE)
					TCU_FAIL_STR(string("Expected VK_INCOMPLETE. Have ") + getResultAsString(result));
			}

			// Check the driver has written something
			for (deUint32	modeIndex = 0;
							modeIndex < modesCountExpected;
							modeIndex++)
			{
				const VkDisplayModeProperties2KHR&	properties2	= modesProperties2[modeIndex];
				const VkDisplayModePropertiesKHR&	properties	= properties2.displayModeProperties;

				results.check(	properties2.sType == queryStructureType,
								"sType has changed to " + de::toString(properties2.sType));

				results.check(	properties2.pNext == DE_NULL,
								"pNext has changed to " + de::toString(properties2.pNext));

				validateDisplayModeProperties(results, properties, nonUpdatedModeProperties);

				if (results.getResult() != QP_TEST_RESULT_PASS)
				{
					m_log	<< tcu::TestLog::Message
							<< "Error detected " << results.getMessage()
							<< " for mode " << modeIndex << " with properties " << properties2
							<< " non updated mode properties are " << nonUpdatedModeProperties2
							<< tcu::TestLog::EndMessage;

					TCU_FAIL_STR(results.getMessage());
				}
			}

			// Check the driver has not written more than requested
			if (modesProperties2[modesCountExpected].displayModeProperties.displayMode != canaryDisplayMode)
				TCU_FAIL("Memory damage detected: driver has written more than expected");
		}
	}

	return tcu::TestStatus::pass("pass");
}


/*--------------------------------------------------------------------*//*!
 * \brief Display coverage tests case class
 *//*--------------------------------------------------------------------*/
class DisplayCoverageTestsCase : public vkt::TestCase
{
public:
	DisplayCoverageTestsCase (tcu::TestContext &context, const char *name, const char *description, const DisplayIndexTest testId)
		: TestCase	(context, name, description)
		, m_testId	(testId)
	{
	}
private:
	const DisplayIndexTest	m_testId;

	vkt::TestInstance*	createInstance	(vkt::Context& context) const
	{
		return new DisplayCoverageTestInstance(context, m_testId);
	}
};


/*--------------------------------------------------------------------*//*!
 * \brief Adds a test into group
 *//*--------------------------------------------------------------------*/
static void addTest (tcu::TestCaseGroup* group, const DisplayIndexTest testId, const char* name, const char* description)
{
	tcu::TestContext&	testCtx	= group->getTestContext();

	group->addChild(new DisplayCoverageTestsCase(testCtx, name, description, testId));
}

/*--------------------------------------------------------------------*//*!
 * \brief Adds VK_KHR_display and VK_KHR_display_swapchain extension tests into group
 *//*--------------------------------------------------------------------*/
void createDisplayCoverageTests (tcu::TestCaseGroup* group)
{
	// VK_KHR_display extension tests
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES,				"get_display_properties",				"Display enumeration coverage test");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES,					"get_display_plane_properties",			"Planes enumeration coverage test");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_SUPPORTED_DISPLAY,	"get_display_plane_supported_displays", "Display plane support coverage test");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_MODE,						"get_display_mode_properties",			"Display mode properties coverage test");
	addTest(group, DISPLAY_TEST_INDEX_CREATE_DISPLAY_MODE,					"create_display_mode",					"Create display mode coverage test");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES,		"get_display_plane_capabilities",		"Display-plane capabilities coverage test");
	addTest(group, DISPLAY_TEST_INDEX_CREATE_DISPLAY_PLANE_SURFACE,			"create_display_plane_surface",			"Create display plane surface coverage test");
	addTest(group, DISPLAY_TEST_INDEX_SURFACE_COUNTERS,						"surface_counters",						"Display plane surface counters test");

	// VK_KHR_get_display_properties2 extension tests
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PROPERTIES2,				"get_display_properties2",				"Display enumeration coverage test using VK_KHR_get_display_properties2");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PLANES2,					"get_display_plane_properties2",		"Planes enumeration coverage test using VK_KHR_get_display_properties2");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_MODE2,					"get_display_mode_properties2",			"Display mode properties coverage test using VK_KHR_get_display_properties2");
	addTest(group, DISPLAY_TEST_INDEX_GET_DISPLAY_PLANE_CAPABILITIES2,		"get_display_plane_capabilities2",		"Display-plane capabilities coverage test using VK_KHR_get_display_properties2");
}

} //wsi
} //vkt

