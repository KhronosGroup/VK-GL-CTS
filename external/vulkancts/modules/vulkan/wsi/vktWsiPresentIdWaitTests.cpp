/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Tests for the present id and present wait extensions.
 *//*--------------------------------------------------------------------*/

#include "vktWsiPresentIdWaitTests.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"

#include "tcuTestContext.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deDefs.hpp"

#include <vector>
#include <string>
#include <set>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <utility>
#include <limits>

using std::vector;
using std::string;
using std::set;

namespace vkt
{
namespace wsi
{

namespace
{

// Handy time constants in nanoseconds.
constexpr deUint64 k10sec	= 10000000000ull;
constexpr deUint64 k1sec	=  1000000000ull;

// 100 milliseconds, way above 1/50 seconds for systems with 50Hz ticks.
// This should also take into account possible measure deviations due to the machine being loaded.
constexpr deUint64 kMargin	=   100000000ull;

using TimeoutRange = std::pair<deInt64, deInt64>;

// Calculate acceptable timeout range based on indicated timeout and taking into account kMargin.
TimeoutRange calcTimeoutRange (deUint64 timeout)
{
	constexpr auto kUnsignedMax	= std::numeric_limits<deUint64>::max();
	constexpr auto kSignedMax	= static_cast<deUint64>(std::numeric_limits<deInt64>::max());

	// Watch for over- and under-flows.
	deUint64 timeoutMin = ((timeout < kMargin) ? 0ull : (timeout - kMargin));
	deUint64 timeoutMax = ((kUnsignedMax - timeout < kMargin) ? kUnsignedMax : timeout + kMargin);

	// Make sure casting is safe.
	timeoutMin = de::min(kSignedMax, timeoutMin);
	timeoutMax = de::min(kSignedMax, timeoutMax);

	return TimeoutRange(static_cast<deInt64>(timeoutMin), static_cast<deInt64>(timeoutMax));
}

class PresentIdWaitInstance : public TestInstance
{
public:
								PresentIdWaitInstance	(Context& context, vk::wsi::Type wsiType) : TestInstance(context), m_wsiType(wsiType) {}
	virtual						~PresentIdWaitInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

	virtual tcu::TestStatus		run						(const vk::DeviceInterface&				vkd,
														 vk::VkDevice							device,
														 vk::VkQueue							queue,
														 vk::VkCommandPool						commandPool,
														 vk::VkSwapchainKHR						swapchain,
														 size_t									swapchainSize,
														 const vk::wsi::WsiTriangleRenderer&	renderer) = 0;

	// Subclasses will need to implement a static method like this one indicating which extensions they need.
	static vector<const char*>	requiredDeviceExts		(void) { return vector<const char*>(); }

	// Subclasses will also need to implement this nonstatic method returning the same information as above.
	virtual vector<const char*>	getRequiredDeviceExts	(void) = 0;

protected:
	vk::wsi::Type				m_wsiType;
};

vector<const char*> getRequiredInstanceExtensions (vk::wsi::Type wsiType)
{
	vector<const char*> extensions;
	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));
	if (isDisplaySurface(wsiType))
		extensions.push_back("VK_KHR_display");
	return extensions;
}

CustomInstance createInstanceWithWsi (Context&							context,
									  vk::wsi::Type						wsiType,
									  const vk::VkAllocationCallbacks*	pAllocator	= nullptr)
{
	const auto version				= context.getUsedApiVersion();
	const auto requiredExtensions	= getRequiredInstanceExtensions(wsiType);

	vector<string> requestedExtensions;
	for (const auto& extensionName : requiredExtensions)
	{
		if (!vk::isCoreInstanceExtension(version, extensionName))
			requestedExtensions.push_back(extensionName);
	}

	return vkt::createCustomInstanceWithExtensions(context, requestedExtensions, pAllocator);
}

struct InstanceHelper
{
	const vector<vk::VkExtensionProperties>	supportedExtensions;
	CustomInstance							instance;
	const vk::InstanceDriver&				vki;

	InstanceHelper (Context& context, vk::wsi::Type wsiType, const vk::VkAllocationCallbacks* pAllocator = nullptr)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
		, instance				(createInstanceWithWsi(context, wsiType, pAllocator))
		, vki					(instance.getDriver())
	{}
};

vector<const char*> getMandatoryDeviceExtensions ()
{
	vector<const char*> mandatoryExtensions;
	mandatoryExtensions.push_back("VK_KHR_swapchain");
	return mandatoryExtensions;
}

vk::Move<vk::VkDevice> createDeviceWithWsi (const vk::PlatformInterface&				vkp,
											vk::VkInstance								instance,
											const vk::InstanceInterface&				vki,
											vk::VkPhysicalDevice						physicalDevice,
											const vector<const char*>&					extraExtensions,
											const deUint32								queueFamilyIndex,
											bool										validationEnabled,
											const vk::VkAllocationCallbacks*			pAllocator = nullptr)
{
	const float							queuePriorities[]	= { 1.0f };
	const vk::VkDeviceQueueCreateInfo	queueInfos[]		=
	{
		{
			vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr,
			(vk::VkDeviceQueueCreateFlags)0,
			queueFamilyIndex,
			DE_LENGTH_OF_ARRAY(queuePriorities),
			&queuePriorities[0]
		}
	};
	vk::VkPhysicalDeviceFeatures		features;
	std::vector<const char*>			extensions			= extraExtensions;
	const auto							mandatoryExtensions	= getMandatoryDeviceExtensions();

	for (const auto& ext : mandatoryExtensions)
		extensions.push_back(ext);

	deMemset(&features, 0, sizeof(features));

	vk::VkPhysicalDeviceFeatures2		physicalDeviceFeatures2 { vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, DE_NULL, features };

	vk::VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures = { vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, DE_NULL, DE_TRUE };
	vk::VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures = { vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, DE_NULL, DE_TRUE };

	void* pNext = DE_NULL;
	for (size_t i = 0; i < extraExtensions.size(); ++i) {
		if (strcmp(extraExtensions[i], "VK_KHR_present_id") == 0)
		{
			presentIdFeatures.pNext = pNext;
			pNext = &presentIdFeatures;
		}
		else if (strcmp(extraExtensions[i], "VK_KHR_present_wait") == 0)
		{
			presentWaitFeatures.pNext = pNext;
			pNext = &presentWaitFeatures;
		}
	}
	physicalDeviceFeatures2.pNext = pNext;

	const vk::VkDeviceCreateInfo		deviceParams	=
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		pNext ? &physicalDeviceFeatures2 : DE_NULL,
		(vk::VkDeviceCreateFlags)0,
		DE_LENGTH_OF_ARRAY(queueInfos),
		&queueInfos[0],
		0u,											// enabledLayerCount
		nullptr,									// ppEnabledLayerNames
		static_cast<deUint32>(extensions.size()),	// enabledExtensionCount
		extensions.data(),							// ppEnabledExtensionNames
		pNext ? DE_NULL : &features
	};

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

struct DeviceHelper
{
	const vk::VkPhysicalDevice		physicalDevice;
	const deUint32					queueFamilyIndex;
	const vk::Unique<vk::VkDevice>	device;
	const vk::DeviceDriver			vkd;
	const vk::VkQueue				queue;

	DeviceHelper (Context&						context,
				  const vk::InstanceInterface&		vki,
				  vk::VkInstance					instance,
				  const vector<vk::VkSurfaceKHR>&	surfaces,
				  const vector<const char*>&		extraExtensions,
				  const vk::VkAllocationCallbacks*	pAllocator = nullptr)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex	(vk::wsi::chooseQueueFamilyIndex(vki, physicalDevice, surfaces))
		, device			(createDeviceWithWsi(context.getPlatformInterface(),
												 instance,
												 vki,
												 physicalDevice,
												 extraExtensions,
												 queueFamilyIndex,
												 context.getTestContext().getCommandLine().isValidationEnabled(),
												 pAllocator))
		, vkd				(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion())
		, queue				(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
	{
	}
};

vk::VkSwapchainCreateInfoKHR getBasicSwapchainParameters (vk::wsi::Type					wsiType,
														  const vk::InstanceInterface&	vki,
														  vk::VkPhysicalDevice			physicalDevice,
														  vk::VkSurfaceKHR				surface,
														  const tcu::UVec2&				desiredSize,
														  deUint32						desiredImageCount)
{
	const vk::VkSurfaceCapabilitiesKHR		capabilities		= vk::wsi::getPhysicalDeviceSurfaceCapabilities(vki,
																								   physicalDevice,
																								   surface);
	const vector<vk::VkSurfaceFormatKHR>	formats				= vk::wsi::getPhysicalDeviceSurfaceFormats(vki,
																							  physicalDevice,
																							  surface);
	const vk::wsi::PlatformProperties&		platformProperties	= vk::wsi::getPlatformProperties(wsiType);
	const vk::VkSurfaceTransformFlagBitsKHR transform			= (capabilities.supportedTransforms & vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const vk::VkSwapchainCreateInfoKHR		parameters			=
	{
		vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		(vk::VkSwapchainCreateFlagsKHR)0,
		surface,
		de::clamp(desiredImageCount, capabilities.minImageCount, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : capabilities.minImageCount + desiredImageCount),
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE
			? capabilities.currentExtent : vk::makeExtent2D(desiredSize.x(), desiredSize.y())),
		1u,									// imageArrayLayers
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		0u,
		nullptr,
		transform,
		vk::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		vk::VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(vk::VkSwapchainKHR)0				// oldSwapchain
	};

	return parameters;
}

using CommandBufferSp	= de::SharedPtr<vk::Unique<vk::VkCommandBuffer>>;
using FenceSp			= de::SharedPtr<vk::Unique<vk::VkFence>>;
using SemaphoreSp		= de::SharedPtr<vk::Unique<vk::VkSemaphore>>;

vector<FenceSp> createFences (const vk::DeviceInterface&	vkd,
							  const vk::VkDevice			device,
							  size_t						numFences)
{
	vector<FenceSp> fences(numFences);

	for (size_t ndx = 0; ndx < numFences; ++ndx)
		fences[ndx] = FenceSp(new vk::Unique<vk::VkFence>(createFence(vkd, device, vk::VK_FENCE_CREATE_SIGNALED_BIT)));

	return fences;
}

vector<SemaphoreSp> createSemaphores (const vk::DeviceInterface&	vkd,
									  const vk::VkDevice			device,
									  size_t						numSemaphores)
{
	vector<SemaphoreSp> semaphores(numSemaphores);

	for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
		semaphores[ndx] = SemaphoreSp(new vk::Unique<vk::VkSemaphore>(createSemaphore(vkd, device)));

	return semaphores;
}

vector<CommandBufferSp> allocateCommandBuffers (const vk::DeviceInterface&		vkd,
												const vk::VkDevice				device,
												const vk::VkCommandPool			commandPool,
												const vk::VkCommandBufferLevel	level,
												const size_t					numCommandBuffers)
{
	vector<CommandBufferSp>				buffers		(numCommandBuffers);

	for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
		buffers[ndx] = CommandBufferSp(new vk::Unique<vk::VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

	return buffers;
}

class FrameStreamObjects
{
public:
	struct FrameObjects
	{
		const vk::VkFence&			renderCompleteFence;
		const vk::VkSemaphore&		renderCompleteSemaphore;
		const vk::VkSemaphore&		imageAvailableSemaphore;
		const vk::VkCommandBuffer&	commandBuffer;
	};

	FrameStreamObjects (const vk::DeviceInterface& vkd, vk::VkDevice device, vk::VkCommandPool cmdPool, size_t maxQueuedFrames)
		: renderingCompleteFences		(createFences(vkd, device, maxQueuedFrames))
		, renderingCompleteSemaphores	(createSemaphores(vkd, device, maxQueuedFrames))
		, imageAvailableSemaphores		(createSemaphores(vkd, device, maxQueuedFrames))
		, commandBuffers				(allocateCommandBuffers(vkd, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames))
		, m_maxQueuedFrames				(maxQueuedFrames)
		, m_nextFrame					(0u)
	{}

	size_t frameNumber (void) const { DE_ASSERT(m_nextFrame > 0u); return m_nextFrame - 1u; }

	FrameObjects newFrame ()
	{
		const size_t mod = m_nextFrame % m_maxQueuedFrames;
		FrameObjects ret =
		{
			**renderingCompleteFences[mod],
			**renderingCompleteSemaphores[mod],
			**imageAvailableSemaphores[mod],
			**commandBuffers[mod],
		};
		++m_nextFrame;
		return ret;
	}

private:
	const vector<FenceSp>			renderingCompleteFences;
	const vector<SemaphoreSp>		renderingCompleteSemaphores;
	const vector<SemaphoreSp>		imageAvailableSemaphores;
	const vector<CommandBufferSp>	commandBuffers;

	const size_t	m_maxQueuedFrames;
	size_t			m_nextFrame;
};

tcu::TestStatus PresentIdWaitInstance::iterate (void)
{
	const tcu::UVec2						desiredSize					(256, 256);
	const InstanceHelper					instHelper					(m_context, m_wsiType);
	const NativeObjects						native						(m_context, instHelper.supportedExtensions, m_wsiType, 1u, tcu::just(desiredSize));
	const vk::Unique<vk::VkSurfaceKHR>		surface						(createSurface(instHelper.vki, instHelper.instance, m_wsiType, native.getDisplay(), native.getWindow(), m_context.getTestContext().getCommandLine()));
	const DeviceHelper						devHelper					(m_context, instHelper.vki, instHelper.instance, vector<vk::VkSurfaceKHR>(1u, surface.get()), getRequiredDeviceExts());
	const vk::DeviceInterface&				vkd							= devHelper.vkd;
	const vk::VkDevice						device						= *devHelper.device;
	vk::SimpleAllocator						allocator					(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
	const vk::VkSwapchainCreateInfoKHR		swapchainInfo				= getBasicSwapchainParameters(m_wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const vk::Unique<vk::VkSwapchainKHR>	swapchain					(vk::createSwapchainKHR(vkd, device, &swapchainInfo));
	const vector<vk::VkImage>				swapchainImages				= vk::wsi::getSwapchainImages(vkd, device, *swapchain);
	const vk::Unique<vk::VkCommandPool>		commandPool					(createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));
	const vk::wsi::WsiTriangleRenderer		renderer					(vkd,
																		 device,
																		 allocator,
																		 m_context.getBinaryCollection(),
																		 false,
																		 swapchainImages,
																		 swapchainImages,
																		 swapchainInfo.imageFormat,
																		 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	try
	{
		return run(vkd, device, devHelper.queue, commandPool.get(), swapchain.get(), swapchainImages.size(), renderer);
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	return tcu::TestStatus(QP_TEST_RESULT_INTERNAL_ERROR, "Reached unreachable code");
}

struct PresentParameters
{
	tcu::Maybe<deUint64>		presentId;
	tcu::Maybe<vk::VkResult>	expectedResult;
};

struct WaitParameters
{
	deUint64	presentId;
	deUint64	timeout; // Nanoseconds.
	bool		timeoutExpected;
};

// This structure represents a set of present operations to be run followed by a set of wait operations to be run after them.
// When running the present operations, the present id can be provided, together with an optional expected result to be checked.
// When runing the wait operations, the present id must be provided together with a timeout and an indication of whether the operation is expected to time out or not.
struct PresentAndWaitOps
{
	vector<PresentParameters>	presentOps;
	vector<WaitParameters>		waitOps;
};

// Parent class for VK_KHR_present_id and VK_KHR_present_wait simple tests.
class PresentIdWaitSimpleInstance : public PresentIdWaitInstance
{
public:
	PresentIdWaitSimpleInstance(Context& context, vk::wsi::Type wsiType, const vector<PresentAndWaitOps>& sequence)
		: PresentIdWaitInstance(context, wsiType), m_sequence(sequence)
	{}

	virtual ~PresentIdWaitSimpleInstance() {}

	virtual tcu::TestStatus		run						(const vk::DeviceInterface&				vkd,
														 vk::VkDevice							device,
														 vk::VkQueue							queue,
														 vk::VkCommandPool						commandPool,
														 vk::VkSwapchainKHR						swapchain,
														 size_t									swapchainSize,
														 const vk::wsi::WsiTriangleRenderer&	renderer);
protected:
	const vector<PresentAndWaitOps> m_sequence;
};

// Waits for the appropriate fences, acquires swapchain image, records frame and submits it to the given queue, signaling the appropriate frame semaphores.
// Returns the image index from the swapchain.
deUint32 recordAndSubmitFrame (FrameStreamObjects::FrameObjects& frameObjects, const vk::wsi::WsiTriangleRenderer& triangleRenderer, const vk::DeviceInterface& vkd, vk::VkDevice device, vk::VkSwapchainKHR swapchain, size_t swapchainSize, vk::VkQueue queue, size_t frameNumber, tcu::TestLog& testLog)
{
	// Wait and reset the render complete fence to avoid having too many submitted frames.
	VK_CHECK(vkd.waitForFences(device, 1u, &frameObjects.renderCompleteFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
	VK_CHECK(vkd.resetFences(device, 1, &frameObjects.renderCompleteFence));

	// Acquire swapchain image.
	deUint32 imageNdx = std::numeric_limits<deUint32>::max();
	const vk::VkResult acquireResult = vkd.acquireNextImageKHR(device,
																swapchain,
																std::numeric_limits<deUint64>::max(),
																frameObjects.imageAvailableSemaphore,
																(vk::VkFence)0,
																&imageNdx);

	if (acquireResult == vk::VK_SUBOPTIMAL_KHR)
		testLog << tcu::TestLog::Message << "Got " << acquireResult << " at frame " << frameNumber << tcu::TestLog::EndMessage;
	else
		VK_CHECK(acquireResult);
	TCU_CHECK(static_cast<size_t>(imageNdx) < swapchainSize);

	// Submit frame to the queue.
	const vk::VkPipelineStageFlags	waitDstStage	= vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const vk::VkSubmitInfo			submitInfo		=
	{
		vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		1u,
		&frameObjects.imageAvailableSemaphore,
		&waitDstStage,
		1u,
		&frameObjects.commandBuffer,
		1u,
		&frameObjects.renderCompleteSemaphore,
	};

	triangleRenderer.recordFrame(frameObjects.commandBuffer, imageNdx, static_cast<deUint32>(frameNumber));
	VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, frameObjects.renderCompleteFence));

	return imageNdx;
}

tcu::TestStatus PresentIdWaitSimpleInstance::run (const vk::DeviceInterface& vkd, vk::VkDevice device, vk::VkQueue queue, vk::VkCommandPool commandPool, vk::VkSwapchainKHR swapchain, size_t swapchainSize, const vk::wsi::WsiTriangleRenderer& renderer)
{
	const size_t		maxQueuedFrames		= swapchainSize*2;
	FrameStreamObjects	frameStreamObjects	(vkd, device, commandPool, maxQueuedFrames);

	for (const auto& step : m_sequence)
	{
		for (const auto& presentOp : step.presentOps)
		{
			// Get objects for the next frame.
			FrameStreamObjects::FrameObjects frameObjects = frameStreamObjects.newFrame();

			// Record and submit new frame.
			deUint32 imageNdx = recordAndSubmitFrame(frameObjects, renderer, vkd, device, swapchain, swapchainSize, queue, frameStreamObjects.frameNumber(), m_context.getTestContext().getLog());

			// Present rendered frame.
			const vk::VkPresentIdKHR		presentId		=
			{
				vk::VK_STRUCTURE_TYPE_PRESENT_ID_KHR,							// VkStructureType		sType;
				nullptr,														// const void*			pNext;
				(presentOp.presentId ? 1u : 0u),								// deUint32				swapchainCount;
				(presentOp.presentId ? &presentOp.presentId.get() : nullptr ),	// const deUint64*		pPresentIds;
			};

			const vk::VkPresentInfoKHR		presentInfo		=
			{
				vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				(presentOp.presentId ? &presentId : nullptr),
				1u,
				&frameObjects.renderCompleteSemaphore,
				1u,
				&swapchain,
				&imageNdx,
				nullptr,
			};

			vk::VkResult result = vkd.queuePresentKHR(queue, &presentInfo);

			if (presentOp.expectedResult)
			{
				const vk::VkResult expected = presentOp.expectedResult.get();
				if ((expected == vk::VK_SUCCESS && result != vk::VK_SUCCESS && result != vk::VK_SUBOPTIMAL_KHR) ||
					(expected != vk::VK_SUCCESS && result != expected))
				{
					std::ostringstream msg;
					msg << "Got " << result << " while expecting " << expected << " after presenting with ";
					if (presentOp.presentId)
						msg << "id " << presentOp.presentId.get();
					else
						msg << "no id";
					TCU_FAIL(msg.str());
				}
			}
		}

		// Wait operations.
		for (const auto& waitOp : step.waitOps)
		{
			auto			before		= std::chrono::high_resolution_clock::now();
			vk::VkResult	waitResult	= vkd.waitForPresentKHR(device, swapchain, waitOp.presentId, waitOp.timeout);
			auto			after		= std::chrono::high_resolution_clock::now();
			auto			diff		= std::chrono::nanoseconds(after - before).count();

			if (waitOp.timeoutExpected)
			{
				if (waitResult != vk::VK_TIMEOUT)
				{
					std::ostringstream msg;
					msg << "Got " << waitResult << " while expecting a timeout in vkWaitForPresentKHR call";
					TCU_FAIL(msg.str());
				}

				const auto timeoutRange = calcTimeoutRange(waitOp.timeout);

				if (diff < timeoutRange.first || diff > timeoutRange.second)
				{
					std::ostringstream msg;
					msg << "vkWaitForPresentKHR waited for " << diff << " nanoseconds with a timeout of " << waitOp.timeout << " nanoseconds";
					TCU_FAIL(msg.str());
				}
			}
			else if (waitResult != vk::VK_SUCCESS)
			{
				std::ostringstream msg;
				msg << "Got " << waitResult << " while expecting success in vkWaitForPresentKHR call";
				TCU_FAIL(msg.str());
			}
		}
	}

	// Wait until device is idle.
	VK_CHECK(vkd.deviceWaitIdle(device));

	return tcu::TestStatus::pass("Pass");
}

// Parent class for VK_KHR_present_id simple tests.
class PresentIdInstance : public PresentIdWaitSimpleInstance
{
public:
	PresentIdInstance(Context& context, vk::wsi::Type wsiType, const vector<PresentAndWaitOps>& sequence)
		: PresentIdWaitSimpleInstance(context, wsiType, sequence)
	{}

	virtual ~PresentIdInstance() {}

	static vector<const char*>	requiredDeviceExts (void)
	{
		vector<const char*> extensions;
		extensions.push_back("VK_KHR_present_id");
		return extensions;
	}

	virtual vector<const char*> getRequiredDeviceExts (void)
	{
		return requiredDeviceExts();
	}
};

// Parent class for VK_KHR_present_wait simple tests.
class PresentWaitInstance : public PresentIdWaitSimpleInstance
{
public:
	PresentWaitInstance(Context& context, vk::wsi::Type wsiType, const vector<PresentAndWaitOps>& sequence)
		: PresentIdWaitSimpleInstance(context, wsiType, sequence)
	{}

	virtual ~PresentWaitInstance() {}

	static vector<const char*>	requiredDeviceExts (void)
	{
		vector<const char*> extensions;
		extensions.push_back("VK_KHR_present_id");
		extensions.push_back("VK_KHR_present_wait");
		return extensions;
	}

	virtual vector<const char*>	getRequiredDeviceExts (void)
	{
		return requiredDeviceExts();
	}
};

class PresentIdZeroInstance : public PresentIdInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentIdZeroInstance (Context& context, vk::wsi::Type wsiType)
		: PresentIdInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentIdZeroInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(0), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
		},
	},
};

class PresentIdIncreasingInstance : public PresentIdInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentIdIncreasingInstance (Context& context, vk::wsi::Type wsiType)
		: PresentIdInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentIdIncreasingInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(1),							tcu::just(vk::VK_SUCCESS) },
			{ tcu::just(std::numeric_limits<deUint64>::max()),	tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
		},
	},
};

class PresentIdInterleavedInstance : public PresentIdInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentIdInterleavedInstance (Context& context, vk::wsi::Type wsiType)
		: PresentIdInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentIdInterleavedInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(0),							tcu::just(vk::VK_SUCCESS) },
			{ tcu::just<deUint64>(1),							tcu::just(vk::VK_SUCCESS) },
			{ tcu::Nothing,										tcu::just(vk::VK_SUCCESS) },
			{ tcu::just(std::numeric_limits<deUint64>::max()),	tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
		},
	},
};

class PresentWaitSingleFrameInstance : public PresentWaitInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentWaitSingleFrameInstance (Context& context, vk::wsi::Type wsiType)
		: PresentWaitInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentWaitSingleFrameInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(1), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ 1ull, k10sec, false },
		},
	},
};

class PresentWaitPastFrameInstance : public PresentWaitInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentWaitPastFrameInstance (Context& context, vk::wsi::Type wsiType)
		: PresentWaitInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentWaitPastFrameInstance::sequence =
{
	// Start with present id 1.
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(1), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ 1ull, k10sec, false },
			{ 1ull, 0ull,   false },
		},
	},
	// Then the maximum value. Both waiting for id 1 and the max id should work.
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just(std::numeric_limits<deUint64>::max()), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ 1ull,                                 0ull,   false },
			{ 1ull,                                 k10sec, false },
			{ std::numeric_limits<deUint64>::max(), k10sec, false },
			{ std::numeric_limits<deUint64>::max(), 0ull,   false },
		},
	},
	// Submit some frames without id after having used the maximum value. This should also work.
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::Nothing,				tcu::just(vk::VK_SUCCESS) },
			{ tcu::just<deUint64>(0),	tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
		},
	},
};

class PresentWaitNoFramesInstance : public PresentWaitInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentWaitNoFramesInstance (Context& context, vk::wsi::Type wsiType)
		: PresentWaitInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentWaitNoFramesInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
		},
		{	// waitOps vector
			{ 1ull, 0ull,  true },
			{ 1ull, k1sec, true },
		},
	},
};

class PresentWaitNoFrameIdInstance : public PresentWaitInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentWaitNoFrameIdInstance (Context& context, vk::wsi::Type wsiType)
		: PresentWaitInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentWaitNoFrameIdInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(0), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ 1ull, 0ull,  true },
			{ 1ull, k1sec, true },
		},
	},
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::Nothing, tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ 1ull, 0ull,  true },
			{ 1ull, k1sec, true },
		},
	},
};

class PresentWaitFutureFrameInstance : public PresentWaitInstance
{
public:
	static const vector<PresentAndWaitOps> sequence;

	PresentWaitFutureFrameInstance (Context& context, vk::wsi::Type wsiType)
		: PresentWaitInstance(context, wsiType, sequence)
	{}
};

const vector<PresentAndWaitOps> PresentWaitFutureFrameInstance::sequence =
{
	{ // PresentAndWaitOps
		{	// presentOps vector
			{ tcu::just<deUint64>(1), tcu::just(vk::VK_SUCCESS) },
		},
		{	// waitOps vector
			{ std::numeric_limits<deUint64>::max(), k1sec, true },
			{ std::numeric_limits<deUint64>::max(), 0ull,  true },
			{ 2ull,                                 0ull,  true },
			{ 2ull,                                 k1sec, true },
		},
	},
};

// Instance with two windows and surfaces to check present ids are not mixed up.
class PresentWaitDualInstance : public TestInstance
{
public:
								PresentWaitDualInstance		(Context& context, vk::wsi::Type wsiType) : TestInstance(context), m_wsiType(wsiType) {}
	virtual						~PresentWaitDualInstance	(void) {}

	virtual tcu::TestStatus		iterate						(void);

	static vector<const char*>	requiredDeviceExts			(void)
	{
		vector<const char*> extensions;
		extensions.push_back("VK_KHR_present_id");
		extensions.push_back("VK_KHR_present_wait");
		return extensions;
	}

	virtual vector<const char*>	getRequiredDeviceExts		(void)
	{
		return requiredDeviceExts();
	}

protected:
	vk::wsi::Type				m_wsiType;
};

struct IdAndWait
{
	deUint64	presentId;
	bool		wait;
};

struct DualIdAndWait
{
	IdAndWait idWait1;
	IdAndWait idWait2;
};

tcu::TestStatus PresentWaitDualInstance::iterate (void)
{
	const vk::wsi::PlatformProperties& platformProperties = getPlatformProperties(m_wsiType);
	if (2 > platformProperties.maxWindowsPerDisplay)
		TCU_THROW(NotSupportedError, "Creating 2 windows not supported");

	const tcu::UVec2						desiredSize					(256, 256);
	const InstanceHelper					instHelper					(m_context, m_wsiType);
	const NativeObjects						native						(m_context, instHelper.supportedExtensions, m_wsiType, 2u, tcu::just(desiredSize));
	const vk::Unique<vk::VkSurfaceKHR>		surface1					(createSurface(instHelper.vki, instHelper.instance, m_wsiType, native.getDisplay(), native.getWindow(0), m_context.getTestContext().getCommandLine()));
	const vk::Unique<vk::VkSurfaceKHR>		surface2					(createSurface(instHelper.vki, instHelper.instance, m_wsiType, native.getDisplay(), native.getWindow(1), m_context.getTestContext().getCommandLine()));
	const DeviceHelper						devHelper					(m_context, instHelper.vki, instHelper.instance, vector<vk::VkSurfaceKHR>{surface1.get(), surface2.get()}, getRequiredDeviceExts());
	const vk::DeviceInterface&				vkd							= devHelper.vkd;
	const vk::VkDevice						device						= *devHelper.device;
	vk::SimpleAllocator						allocator					(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
	const vk::VkSwapchainCreateInfoKHR		swapchainInfo1				= getBasicSwapchainParameters(m_wsiType, instHelper.vki, devHelper.physicalDevice, surface1.get(), desiredSize, 2);
	const vk::VkSwapchainCreateInfoKHR		swapchainInfo2				= getBasicSwapchainParameters(m_wsiType, instHelper.vki, devHelper.physicalDevice, surface2.get(), desiredSize, 2);
	const vk::Unique<vk::VkSwapchainKHR>	swapchain1					(vk::createSwapchainKHR(vkd, device, &swapchainInfo1));
	const vk::Unique<vk::VkSwapchainKHR>	swapchain2					(vk::createSwapchainKHR(vkd, device, &swapchainInfo2));
	const vector<vk::VkImage>				swapchainImages1			= vk::wsi::getSwapchainImages(vkd, device, swapchain1.get());
	const vector<vk::VkImage>				swapchainImages2			= vk::wsi::getSwapchainImages(vkd, device, swapchain2.get());
	const vk::Unique<vk::VkCommandPool>		commandPool					(createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));
	const vk::wsi::WsiTriangleRenderer		renderer1					(vkd,
																		 device,
																		 allocator,
																		 m_context.getBinaryCollection(),
																		 false,
																		 swapchainImages1,
																		 swapchainImages1,
																		 swapchainInfo1.imageFormat,
																		 tcu::UVec2(swapchainInfo1.imageExtent.width, swapchainInfo1.imageExtent.height));
	const vk::wsi::WsiTriangleRenderer		renderer2					(vkd,
																		 device,
																		 allocator,
																		 m_context.getBinaryCollection(),
																		 false,
																		 swapchainImages2,
																		 swapchainImages2,
																		 swapchainInfo2.imageFormat,
																		 tcu::UVec2(swapchainInfo2.imageExtent.width, swapchainInfo2.imageExtent.height));
	tcu::TestLog&							testLog						= m_context.getTestContext().getLog();

	try
	{
		const size_t		maxQueuedFrames		= swapchainImages1.size()*2;
		FrameStreamObjects	frameStreamObjects1	(vkd, device, commandPool.get(), maxQueuedFrames);
		FrameStreamObjects	frameStreamObjects2	(vkd, device, commandPool.get(), maxQueuedFrames);

		// Increasing ids for both swapchains, waiting on some to make sure we do not time out unexpectedly.
		const vector<DualIdAndWait> sequence =
		{
			{
				{ 1ull, false },
				{ 2ull, true  },
			},
			{
				{ 4ull, true  },
				{ 3ull, false },
			},
			{
				{ 5ull, true  },
				{ 6ull, true  },
			},
		};

		for (const auto& step : sequence)
		{
			// Get objects for the next frames.
			FrameStreamObjects::FrameObjects frameObjects1 = frameStreamObjects1.newFrame();
			FrameStreamObjects::FrameObjects frameObjects2 = frameStreamObjects2.newFrame();

			// Record and submit frame.
			deUint32 imageNdx1 = recordAndSubmitFrame(frameObjects1, renderer1, vkd, device, swapchain1.get(), swapchainImages1.size(), devHelper.queue, frameStreamObjects1.frameNumber(), testLog);
			deUint32 imageNdx2 = recordAndSubmitFrame(frameObjects2, renderer2, vkd, device, swapchain2.get(), swapchainImages2.size(), devHelper.queue, frameStreamObjects2.frameNumber(), testLog);

			// Present both images at the same time with their corresponding ids.
			const deUint64				presentIdsArr[] = { step.idWait1.presentId, step.idWait2.presentId };
			const vk::VkPresentIdKHR	presentId		=
			{
				vk::VK_STRUCTURE_TYPE_PRESENT_ID_KHR,							// VkStructureType		sType;
				nullptr,														// const void*			pNext;
				static_cast<deUint32>(DE_LENGTH_OF_ARRAY(presentIdsArr)),		// deUint32				swapchainCount;
				presentIdsArr,													// const deUint64*		pPresentIds;
			};

			const vk::VkSemaphore		semaphoreArr[]	= { frameObjects1.renderCompleteSemaphore, frameObjects2.renderCompleteSemaphore };
			const vk::VkSwapchainKHR	swapchainArr[]	= { swapchain1.get(), swapchain2.get() };
			const deUint32				imgIndexArr[]	= { imageNdx1, imageNdx2 };
			const vk::VkPresentInfoKHR	presentInfo		=
			{
				vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				&presentId,
				static_cast<deUint32>(DE_LENGTH_OF_ARRAY(semaphoreArr)),
				semaphoreArr,
				static_cast<deUint32>(DE_LENGTH_OF_ARRAY(swapchainArr)),
				swapchainArr,
				imgIndexArr,
				nullptr,
			};

			VK_CHECK(vkd.queuePresentKHR(devHelper.queue, &presentInfo));

			const IdAndWait* idWaitArr[] = { &step.idWait1, &step.idWait2 };
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(idWaitArr); ++i)
			{
				if (idWaitArr[i]->wait)
					VK_CHECK(vkd.waitForPresentKHR(device, swapchainArr[i], idWaitArr[i]->presentId, k10sec));
			}
		}

		// Wait until device is idle.
		VK_CHECK(vkd.deviceWaitIdle(device));

		return tcu::TestStatus::pass("Pass");
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	return tcu::TestStatus(QP_TEST_RESULT_INTERNAL_ERROR, "Reached unreachable code");
}

// Templated class for every instance type.
template <class T>	// T is the test instance class.
class PresentIdWaitCase : public TestCase
{
public:
							PresentIdWaitCase	(vk::wsi::Type wsiType, tcu::TestContext& ctx, const std::string& name, const std::string& description);
	virtual					~PresentIdWaitCase	(void) {}
	virtual void			initPrograms		(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
	virtual void			checkSupport		(Context& context) const;

protected:
	vk::wsi::Type			m_wsiType;
};

template <class T>
PresentIdWaitCase<T>::PresentIdWaitCase (vk::wsi::Type wsiType, tcu::TestContext& ctx, const std::string& name, const std::string& description)
	: TestCase(ctx, name, description), m_wsiType(wsiType)
{
}

template <class T>
void PresentIdWaitCase<T>::initPrograms (vk::SourceCollections& programCollection) const
{
	vk::wsi::WsiTriangleRenderer::getPrograms(programCollection);
}

template <class T>
TestInstance* PresentIdWaitCase<T>::createInstance (Context& context) const
{
	return new T(context, m_wsiType);
}

template <class T>
void PresentIdWaitCase<T>::checkSupport (Context& context) const
{
	// Check instance extension support.
	const auto instanceExtensions = getRequiredInstanceExtensions(m_wsiType);
	for (const auto& ext : instanceExtensions)
	{
		if (!context.isInstanceFunctionalitySupported(ext))
			TCU_THROW(NotSupportedError, ext + string(" is not supported"));
	}

	// Check device extension support.
	const auto& vki                 = context.getInstanceInterface();
	const auto  physDev             = context.getPhysicalDevice();
	const auto  supportedDeviceExts = vk::enumerateDeviceExtensionProperties(vki, physDev, nullptr);
	const auto  mandatoryDeviceExts = getMandatoryDeviceExtensions();

	auto checkedDeviceExts = T::requiredDeviceExts();
	for (const auto& ext : mandatoryDeviceExts)
		checkedDeviceExts.push_back(ext);

	for (const auto& ext : checkedDeviceExts)
	{
		if (!context.isDeviceFunctionalitySupported(ext))
			TCU_THROW(NotSupportedError, ext + string(" is not supported"));
	}
}

void createPresentIdTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	testGroup->addChild(new PresentIdWaitCase<PresentIdZeroInstance>		(wsiType, testGroup->getTestContext(), "zero",			"Use present id zero"));
	testGroup->addChild(new PresentIdWaitCase<PresentIdIncreasingInstance>	(wsiType, testGroup->getTestContext(), "increasing",	"Use increasing present ids"));
	testGroup->addChild(new PresentIdWaitCase<PresentIdInterleavedInstance>	(wsiType, testGroup->getTestContext(), "interleaved",	"Use increasing present ids interleaved with no ids"));
}

void createPresentWaitTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	testGroup->addChild(new PresentIdWaitCase<PresentWaitSingleFrameInstance>	(wsiType, testGroup->getTestContext(), "single_no_timeout",	"Present single frame with no expected timeout"));
	testGroup->addChild(new PresentIdWaitCase<PresentWaitPastFrameInstance>		(wsiType, testGroup->getTestContext(), "past_no_timeout",	"Wait for past frame with no expected timeout"));
	testGroup->addChild(new PresentIdWaitCase<PresentWaitNoFramesInstance>		(wsiType, testGroup->getTestContext(), "no_frames",			"Expect timeout before submitting any frame"));
	testGroup->addChild(new PresentIdWaitCase<PresentWaitNoFrameIdInstance>		(wsiType, testGroup->getTestContext(), "no_frame_id",		"Expect timeout after submitting frames with no id"));
	testGroup->addChild(new PresentIdWaitCase<PresentWaitFutureFrameInstance>	(wsiType, testGroup->getTestContext(), "future_frame",		"Expect timeout when waiting for a future frame"));
	testGroup->addChild(new PresentIdWaitCase<PresentWaitDualInstance>			(wsiType, testGroup->getTestContext(), "two_swapchains",	"Smoke test using two windows, surfaces and swapchains"));
}

} // anonymous

void createPresentIdWaitTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	de::MovePtr<tcu::TestCaseGroup>	idGroup		(new tcu::TestCaseGroup(testGroup->getTestContext(), "id",		"VK_KHR_present_id tests"));
	de::MovePtr<tcu::TestCaseGroup>	waitGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), "wait",	"VK_KHR_present_wait tests"));

	createPresentIdTests	(idGroup.get(),		wsiType);
	createPresentWaitTests	(waitGroup.get(),	wsiType);

	testGroup->addChild(idGroup.release());
	testGroup->addChild(waitGroup.release());
}

} // wsi
} // vkt

