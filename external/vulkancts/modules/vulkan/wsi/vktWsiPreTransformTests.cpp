/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 Google Inc.
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
 * \brief VkSwapchainCreateInfoKHR::preTransform tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiPreTransformTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"
#include "tcuTestContext.hpp"
#include "tcuPlatform.hpp"

#include "vkQueryUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

#include <thread>
#include <chrono>
#include <filesystem>
#include <unordered_map>

namespace vkt
{
namespace wsi
{

namespace
{

using namespace vk;
using namespace vk::wsi;

using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;

using std::string;
using std::vector;

typedef vector<VkExtensionProperties> Extensions;
typedef SharedPtr<Unique<VkCommandBuffer>> CommandBufferSp;
typedef SharedPtr<Unique<VkFence>> FenceSp;
typedef SharedPtr<Unique<VkSemaphore>> SemaphoreSp;

struct TestParams
{
    Type wsiType;
    tcu::ScreenRotation rotation;
};

void checkAllSupported(const Extensions &supportedExtensions, const vector<string> &requiredExtensions)
{
    for (vector<string>::const_iterator requiredExtName = requiredExtensions.begin();
         requiredExtName != requiredExtensions.end(); ++requiredExtName)
    {
        if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(*requiredExtName)))
            TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
    }
}

CustomInstance createInstanceWithWsi(Context &context, const Extensions &supportedExtensions, Type wsiType,
                                     const vector<string> extraExtensions)
{
    vector<string> extensions = extraExtensions;

    extensions.push_back("VK_KHR_surface");
    extensions.push_back(getExtensionName(wsiType));
    if (isDisplaySurface(wsiType))
        extensions.push_back("VK_KHR_display");

    checkAllSupported(supportedExtensions, extensions);

    return vkt::createCustomInstanceWithExtensions(context, extensions, nullptr);
}

Move<VkDevice> createDeviceWithWsi(const PlatformInterface &vkp, uint32_t apiVersion, VkInstance instance,
                                   const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                   const Extensions &supportedExtensions, const vector<string> &additionalExtensions,
                                   const vector<uint32_t> &queueFamilyIndices)
{
    const float queuePriorities[] = {1.0f};
    vector<VkDeviceQueueCreateInfo> queueInfos;

    for (const auto familyIndex : queueFamilyIndices)
    {
        const VkDeviceQueueCreateInfo info = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,
            (VkDeviceQueueCreateFlags)0,
            familyIndex,
            DE_LENGTH_OF_ARRAY(queuePriorities),
            &queuePriorities[0],
        };

        queueInfos.push_back(info);
    }

    vector<string> extensions;
    extensions.push_back("VK_KHR_swapchain");
    extensions.insert(end(extensions), begin(additionalExtensions), end(additionalExtensions));

    for (const auto &extName : extensions)
    {
        if (!isCoreDeviceExtension(apiVersion, extName) &&
            !isExtensionStructSupported(supportedExtensions, RequiredExtension(extName)))
            TCU_THROW(NotSupportedError, extName + " is not supported");
    }

    for (const auto &ext : supportedExtensions)
    {
        if (strcmp(ext.extensionName, "VK_KHR_present_mode_fifo_latest_ready") == 0)
            extensions.push_back("VK_KHR_present_mode_fifo_latest_ready");
    }

    VkPhysicalDeviceFeatures features = {};

    // Convert from std::vector<std::string> to std::vector<const char*>.
    std::vector<const char *> extensionsChar;
    extensionsChar.reserve(extensions.size());
    std::transform(begin(extensions), end(extensions), std::back_inserter(extensionsChar),
                   [](const std::string &s) { return s.c_str(); });

    const VkDeviceCreateInfo deviceParams = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                             nullptr,
                                             (VkDeviceCreateFlags)0,
                                             static_cast<uint32_t>(queueInfos.size()),
                                             queueInfos.data(),
                                             0u,                                           // enabledLayerCount
                                             nullptr,                                      // ppEnabledLayerNames
                                             static_cast<uint32_t>(extensionsChar.size()), // enabledExtensionCount
                                             extensionsChar.data(),                        // ppEnabledExtensionNames
                                             &features};

    return createCustomDevice(vkp, instance, vki, physicalDevice, &deviceParams);
}

struct InstanceHelper
{
    const vector<VkExtensionProperties> supportedExtensions;
    const CustomInstance instance;
    const InstanceDriver &vki;

    InstanceHelper(Context &context, Type wsiType)
        : supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
        , instance(createInstanceWithWsi(context, supportedExtensions, wsiType, vector<string>()))
        , vki(instance.getDriver())
    {
    }

    InstanceHelper(Context &context, Type wsiType, const vector<string> &extensions)
        : supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
        , instance(createInstanceWithWsi(context, supportedExtensions, wsiType, extensions))
        , vki(instance.getDriver())
    {
    }
};

struct DeviceHelper
{
    const VkPhysicalDevice physicalDevice;
    const uint32_t queueFamilyIndex;
    const Unique<VkDevice> device;
    const DeviceDriver vkd;
    const VkQueue queue;

    DeviceHelper(Context &context, const InstanceInterface &vki, VkInstance instance, VkSurfaceKHR surface,
                 const vector<string> &additionalExtensions = vector<string>())
        : physicalDevice(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
        , queueFamilyIndex(chooseQueueFamilyIndex(vki, physicalDevice, surface))
        , device(createDeviceWithWsi(context.getPlatformInterface(), context.getUsedApiVersion(), instance, vki,
                                     physicalDevice, enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr),
                                     additionalExtensions, vector<uint32_t>{queueFamilyIndex}))
        , vkd(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion(),
              context.getTestContext().getCommandLine())
        , queue(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
    {
    }
};

VkSwapchainCreateInfoKHR getBasicSwapchainParameters(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                     VkSurfaceKHR surface, uint32_t desiredImageCount)
{
    const VkSurfaceCapabilitiesKHR capabilities = getPhysicalDeviceSurfaceCapabilities(vki, physicalDevice, surface);
    const vector<VkSurfaceFormatKHR> formats    = getPhysicalDeviceSurfaceFormats(vki, physicalDevice, surface);

    uint32_t formatIndex = 0;
    for (uint32_t i = 0; i < formats.size(); ++i)
    {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            formatIndex = i;
            break;
        }
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0)
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    const VkSwapchainCreateInfoKHR parameters = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        (VkSwapchainCreateFlagsKHR)0,
        surface,
        de::clamp(desiredImageCount, capabilities.minImageCount,
                  capabilities.maxImageCount > 0 ? capabilities.maxImageCount :
                                                   capabilities.minImageCount + desiredImageCount),
        formats[formatIndex].format,
        formats[formatIndex].colorSpace,
        capabilities.currentExtent,
        1u, // imageArrayLayers
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        capabilities.currentTransform, // VkSurfaceTransformFlagBitsKHR	preTransform
        compositeAlpha,
        VK_PRESENT_MODE_FIFO_KHR,
        VK_FALSE,      // clipped
        VK_NULL_HANDLE // oldSwapchain
    };

    return parameters;
}

vector<FenceSp> createFences(const DeviceInterface &vkd, const VkDevice device, size_t numFences,
                             bool isSignaled = true)
{
    vector<FenceSp> fences(numFences);

    for (size_t ndx = 0; ndx < numFences; ++ndx)
        fences[ndx] =
            FenceSp(new Unique<VkFence>(createFence(vkd, device, (isSignaled) ? vk::VK_FENCE_CREATE_SIGNALED_BIT : 0)));

    return fences;
}

vector<SemaphoreSp> createSemaphores(const DeviceInterface &vkd, const VkDevice device, size_t numSemaphores)
{
    vector<SemaphoreSp> semaphores(numSemaphores);

    for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
        semaphores[ndx] = SemaphoreSp(new Unique<VkSemaphore>(createSemaphore(vkd, device)));

    return semaphores;
}

vector<CommandBufferSp> allocateCommandBuffers(const DeviceInterface &vkd, const VkDevice device,
                                               const VkCommandPool commandPool, const VkCommandBufferLevel level,
                                               const size_t numCommandBuffers)
{
    vector<CommandBufferSp> buffers(numCommandBuffers);

    for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
        buffers[ndx] =
            CommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

    return buffers;
}

static bool fileReady(const std::filesystem::path &path)
{
    return std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
}

static bool waitForScreenshot(Context &context, const char *path, int timeoutMs)
{
    const int sleepMs = 200;
    int waited        = 0;

    while (waited < timeoutMs)
    {
        bool fileExists = fileReady(path);

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        waited += sleepMs;

        if (fileExists)
        {
            return true;
        }

        context.getTestContext().touchWatchdog();
    }

    return false;
}

static bool comparePixels(const uint8_t *result, const uint8_t *expected)
{
    for (int i = 0; i < 4; ++i)
    {
        if (result[i] != expected[i])
            return false;
    }
    return true;
}

static uint32_t findOrientation(const std::vector<uint8_t> &pixels, uint32_t width, uint32_t height)
{
    const uint8_t red[]   = {0, 0, 255, 255};
    const uint8_t green[] = {0, 255, 0, 255};
    const uint8_t blue[]  = {255, 0, 0, 255};
    const uint8_t white[] = {255, 255, 255, 255};

    const uint8_t *firstPixel  = &pixels[0];
    const uint8_t *secondPixel = &pixels[(width - 1) * 4];
    const uint8_t *thirdPixel  = &pixels[((height - 1) * width) * 4];
    const uint8_t *fourthPixel = &pixels[(height * width - 1) * 4];

    if (comparePixels(firstPixel, blue) && comparePixels(secondPixel, red) && comparePixels(thirdPixel, white) &&
        comparePixels(fourthPixel, green))
        return 0;
    if (comparePixels(firstPixel, red) && comparePixels(secondPixel, green) && comparePixels(thirdPixel, blue) &&
        comparePixels(fourthPixel, white))
        return 90;
    if (comparePixels(firstPixel, green) && comparePixels(secondPixel, white) && comparePixels(thirdPixel, red) &&
        comparePixels(fourthPixel, blue))
        return 180;
    if (comparePixels(firstPixel, white) && comparePixels(secondPixel, blue) && comparePixels(thirdPixel, green) &&
        comparePixels(fourthPixel, red))
        return 270;
    return -1;
}

tcu::TestStatus basicRenderTest(Context &context, TestParams params)
{
    const InstanceHelper instHelper(context, params.wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, params.wsiType, 1u);
    const Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, params.wsiType,
                                                     native.getDisplay(), native.getWindow(),
                                                     context.getTestContext().getCommandLine()));
    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface);
    const DeviceInterface &vkd = devHelper.vkd;
    const VkDevice device      = *devHelper.device;
    SimpleAllocator allocator(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
    tcu::TestLog &log = context.getTestContext().getLog();

    const std::string referenceFilename = "reference.data";
    const std::string referenceFullPath = "/sdcard/Android/data/com.drawelements.deqp/files/" + referenceFilename;
    std::remove(referenceFullPath.c_str());
    std::remove((referenceFullPath + ".tmp").c_str());

    const Unique<VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    const size_t maxQueuedFrames = 8u;

    // We need to keep hold of fences from vkAcquireNextImageKHR to actually
    // limit number of frames we allow to be queued.
    const vector<FenceSp> imageReadyFences(createFences(vkd, device, maxQueuedFrames));

    // We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to pass
    // the semaphore in same time as the fence we use to meter rendering.
    const vector<SemaphoreSp> imageReadySemaphores(createSemaphores(vkd, device, maxQueuedFrames + 1));

    // For rest we simply need maxQueuedFrames as we will wait for image
    // from frameNdx-maxQueuedFrames to become available to us, guaranteeing that
    // previous uses must have completed.
    const vector<SemaphoreSp> renderingCompleteSemaphores(createSemaphores(vkd, device, maxQueuedFrames));
    const vector<CommandBufferSp> commandBuffers(
        allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

    const auto &platform = context.getTestContext().getPlatform().getVulkanPlatform();
    platform.setCustomScreenOrientation(true);
    platform.rotateScreen(tcu::ScreenRotation::SCREENROTATION_0);

    bool fail = false;
    try
    {
        const uint32_t numFramesToRender = 1500;
        // Rotation while rendering to the first swapchain
        const uint32_t rotationFrame          = 30;
        const uint32_t screenshotAttemptFrame = 100;

        VkSurfaceTransformFlagBitsKHR referenceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        uint32_t referenceOrientation                    = 0;

        for (uint32_t swapchainNdx = 1; swapchainNdx < 3; ++swapchainNdx)
        {
            context.getTestContext().touchWatchdog();

            const VkSwapchainCreateInfoKHR swapchainInfo =
                getBasicSwapchainParameters(instHelper.vki, devHelper.physicalDevice, *surface, 2);
            if (swapchainNdx == 1)
            {
                referenceTransform = swapchainInfo.preTransform;
            }
            else if (referenceTransform == swapchainInfo.preTransform && params.rotation != tcu::SCREENROTATION_0)
            {
                platform.setCustomScreenOrientation(false);
                // Reset orientation to default
                platform.rotateScreen(tcu::ScreenRotation::SCREENROTATION_0);
                TCU_THROW(NotSupportedError, "Device orientation was not changed");
            }
            const Unique<VkSwapchainKHR> swapchain(createSwapchainKHR(vkd, device, &swapchainInfo));
            const vector<VkImage> swapchainImages = getSwapchainImages(vkd, device, *swapchain);
            const WsiTriangleRenderer renderer(
                vkd, device, allocator, context.getBinaryCollection(), false, swapchainImages, swapchainImages,
                swapchainInfo.imageFormat,
                tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

            for (uint32_t frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
            {
                const VkFence imageReadyFence         = **imageReadyFences[frameNdx % imageReadyFences.size()];
                const VkSemaphore imageReadySemaphore = **imageReadySemaphores[frameNdx % imageReadySemaphores.size()];
                uint32_t imageNdx                     = ~0u;
                const bool lastFrame                  = frameNdx == numFramesToRender - 1;

                VK_CHECK(
                    vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
                VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

                if (swapchainNdx == 1 && frameNdx == rotationFrame)
                {
                    log << tcu::TestLog::Message << "Taking reference screenshot, path: " << referenceFullPath
                        << tcu::TestLog::EndMessage;
                    platform.requestPixelCopy(referenceFilename.c_str());
                    if (!waitForScreenshot(context, referenceFullPath.c_str(), 60000))
                        TCU_FAIL("Reference screenshot not successful");

                    FILE *f = fopen(referenceFullPath.c_str(), "rb");
                    if (!f)
                        TCU_FAIL("Failed to open reference screenshot file");

                    uint32_t width, height;
                    size_t read1 = fread(&width, 4, 1, f);
                    size_t read2 = fread(&height, 4, 1, f);
                    if (read1 != 1 || read2 != 1)
                    {
                        fclose(f);
                        TCU_FAIL("Failed to read reference screenshot");
                    }
                    log << tcu::TestLog::Message << "Reference screenshot width=" << width << ", height=" << height
                        << tcu::TestLog::EndMessage;

                    std::vector<uint8_t> pixels(width * height * 4);
                    size_t readPixels = fread(pixels.data(), 1, pixels.size(), f);
                    fclose(f);
                    if (readPixels != pixels.size())
                        TCU_FAIL("Failed to read reference screenshot");

                    referenceOrientation = findOrientation(pixels, width, height);

                    log << tcu::TestLog::Message << "Rotating screen\n" << tcu::TestLog::EndMessage;
                    platform.rotateScreen(params.rotation);
                }
                else if (swapchainNdx == 2 && ((frameNdx - 1) % screenshotAttemptFrame == 0 || lastFrame))
                {
                    const std::string filename = "pixelcopy" + std::to_string(frameNdx) + ".data";
                    const std::string fullPath = "/sdcard/Android/data/com.drawelements.deqp/files/" + filename;
                    std::remove(fullPath.c_str());
                    std::remove((fullPath + ".tmp").c_str());

                    log << tcu::TestLog::Message << "Taking screenshot, path: " << fullPath << tcu::TestLog::EndMessage;

                    platform.requestPixelCopy(filename.c_str());

                    if (!waitForScreenshot(context, fullPath.c_str(), 60000))
                        TCU_FAIL("Screenshot not successful");

                    FILE *f = fopen(fullPath.c_str(), "rb");
                    if (!f)
                        TCU_FAIL("Failed to open screenshot file");

                    uint32_t width, height;
                    size_t read1 = fread(&width, 4, 1, f);
                    size_t read2 = fread(&height, 4, 1, f);
                    if (read1 != 1 || read2 != 1)
                    {
                        fclose(f);
                        TCU_FAIL("Failed to read screenshot");
                    }

                    log << tcu::TestLog::Message << "Screenshot width=" << width << ", height=" << height
                        << tcu::TestLog::EndMessage;
                    std::vector<uint8_t> pixels(width * height * 4);
                    size_t readPixels = fread(pixels.data(), 1, pixels.size(), f);
                    fclose(f);
                    if (readPixels != pixels.size())
                        TCU_FAIL("Failed to read screenshot");

                    uint32_t orientation = findOrientation(pixels, width, height);

                    bool match = true;
                    if (params.rotation == tcu::SCREENROTATION_0)
                    {
                        if (orientation != referenceOrientation)
                            match = false;
                    }
                    else if (params.rotation == tcu::SCREENROTATION_90)
                    {
                        if (orientation != (referenceOrientation + 90) % 360)
                            match = false;
                    }
                    else if (params.rotation == tcu::SCREENROTATION_180)
                    {
                        if (orientation != (referenceOrientation + 180) % 360)
                            match = false;
                    }
                    else if (params.rotation == tcu::SCREENROTATION_270)
                    {
                        if (orientation != (referenceOrientation + 270) % 360)
                            match = false;
                    }
                    if (match)
                    {
                        break;
                    }
                    else if (lastFrame)
                    {
                        fail = true;
                        log << tcu::TestLog::Message
                            << "Screenshot pixels did not match expected results.\nReference orientation was "
                            << referenceOrientation << " degrees, screenshot orientation was " << orientation
                            << " degrees." << tcu::TestLog::EndMessage;
                        break;
                    }
                }

                vkd.acquireNextImageKHR(device, *swapchain, std::numeric_limits<uint64_t>::max(), imageReadySemaphore,
                                        VK_NULL_HANDLE, &imageNdx);

                TCU_CHECK((size_t)imageNdx < swapchainImages.size());

                {
                    const VkSemaphore renderingCompleteSemaphore =
                        **renderingCompleteSemaphores[frameNdx % renderingCompleteSemaphores.size()];
                    const VkCommandBuffer commandBuffer     = **commandBuffers[frameNdx % commandBuffers.size()];
                    const VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    const VkSubmitInfo submitInfo           = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                               nullptr,
                                                               1u,
                                                               &imageReadySemaphore,
                                                               &waitDstStage,
                                                               1u,
                                                               &commandBuffer,
                                                               1u,
                                                               &renderingCompleteSemaphore};
                    const VkPresentInfoKHR presentInfo      = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                               nullptr,
                                                               1u,
                                                               &renderingCompleteSemaphore,
                                                               1u,
                                                               &*swapchain,
                                                               &imageNdx,
                                                               nullptr};

                    renderer.recordFrame(commandBuffer, imageNdx, frameNdx);
                    VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, imageReadyFence));
                    vkd.queuePresentKHR(devHelper.queue, &presentInfo);
                }

                if (swapchainNdx == 1)
                {
                    const VkSurfaceCapabilitiesKHR newCapabilities =
                        getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);
                    if (frameNdx < rotationFrame && referenceTransform != newCapabilities.currentTransform)
                    {
                        swapchainNdx = 0;
                        break;
                    }
                    else if (frameNdx > rotationFrame && (referenceTransform != newCapabilities.currentTransform ||
                                                          params.rotation == tcu::SCREENROTATION_0))
                    {
                        break;
                    }
                }
            }
            VK_CHECK(vkd.deviceWaitIdle(device));
        }

        platform.setCustomScreenOrientation(false);
        // Reset orientation to default
        platform.rotateScreen(tcu::ScreenRotation::SCREENROTATION_0);
    }
    catch (...)
    {
        // Make sure device is idle before destroying resources
        VK_CHECK(vkd.deviceWaitIdle(device));
        platform.setCustomScreenOrientation(false);
        // Reset orientation to default
        platform.rotateScreen(tcu::ScreenRotation::SCREENROTATION_0);
        throw;
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected screenshot results");

    return tcu::TestStatus::pass("Pass");
}

void getBasicRenderPrograms(SourceCollections &dst, TestParams)
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "layout (location=0) out vec2 uv;\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(pos * 4.0f - 1.0f, 0.0f, 1.0f);\n"
         << "    uv = pos;\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout (location=0) in vec2 uv;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    if (uv.y < 0.25f) {\n"
         << "        if (uv.x < 0.25f) {\n"
         << "            outColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n"
         << "        } else {\n "
         << "            outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
         << "        }\n"
         << "    } else {\n"
         << "        if (uv.x < 0.25f) {\n"
         << "            outColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n"
         << "        } else {\n "
         << "            outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
         << "        }\n"
         << "    }\n"
         << "}\n";

    dst.glslSources.add("tri-vert") << glu::VertexSource(vert.str());
    dst.glslSources.add("tri-frag") << glu::FragmentSource(frag.str());
}

} // namespace

void createPreTransformTests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    const struct
    {
        tcu::ScreenRotation rotation;
        const char *name;
    } rotationTests[] = {
        {tcu::SCREENROTATION_0, "rotation_0"},
        {tcu::SCREENROTATION_90, "rotation_90"},
        {tcu::SCREENROTATION_180, "rotation_180"},
        {tcu::SCREENROTATION_270, "rotation_270"},
    };

    for (const auto rotationTest : rotationTests)
    {
        TestParams params;
        params.wsiType  = wsiType;
        params.rotation = rotationTest.rotation;
        addFunctionCaseWithPrograms(testGroup, rotationTest.name, getBasicRenderPrograms, basicRenderTest, params);
    }
}

} // namespace wsi
} // namespace vkt
