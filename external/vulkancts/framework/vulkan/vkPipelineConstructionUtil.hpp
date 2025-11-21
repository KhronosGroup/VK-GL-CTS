#ifndef _VKPIPELINECONSTRUCTIONUTIL_HPP
#define _VKPIPELINECONSTRUCTIONUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Wrapper that can construct monolithic pipeline or use
          VK_EXT_graphics_pipeline_library for pipeline construction or use
          VK_EXT_shader_object for shader objects.
 *//*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "vkDefs.hpp"
#include "tcuDefs.hpp"
#include "deSharedPtr.hpp"
#include "vkPrograms.hpp"
#include "vkShaderObjectUtil.hpp"
#include <vector>
#include <stdexcept>

namespace vk
{

enum PipelineConstructionType
{
    PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC = 0, // Construct legacy - monolithic pipeline
    PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY, // Use VK_EXT_graphics_pipeline_library and construct pipeline out of several pipeline parts.
    PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, // Same as PIPELINE_CONSTRUCTION_TYPE_OPTIMISED_LIBRARY but with fast linking
    PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, // Use VK_EXT_shader_object unlinked shader objects from spirv
    PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY, // Use VK_EXT_shader_object unlinked shader objects from binary
    PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV,  // Use VK_EXT_shader_object linked shader objects from spirv
    PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY, // Use VK_EXT_shader_object linked shader objects from binary
};

bool isConstructionTypeLibrary(PipelineConstructionType pipelineConstructionType);
bool isConstructionTypeShaderObject(PipelineConstructionType pipelineConstructionType);
void checkPipelineConstructionRequirements(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                           PipelineConstructionType pipelineConstructionType);

// This exception may be raised in one of the intermediate steps when using shader module IDs instead of normal module objects.
class PipelineCompileRequiredError : public std::runtime_error
{
public:
    PipelineCompileRequiredError(const std::string &msg) : std::runtime_error(msg)
    {
    }
};

// PointerWrapper template is used to hide structures that should not be visible for Vulkan SC
template <typename T>
class PointerWrapper
{
public:
    PointerWrapper() : ptr(nullptr)
    {
    }
    PointerWrapper(T *p0) : ptr(p0)
    {
    }
    T *ptr;
};

template <typename T>
class ConstPointerWrapper
{
public:
    ConstPointerWrapper() : ptr(nullptr)
    {
    }
    ConstPointerWrapper(const T *p0) : ptr(p0)
    {
    }
    const T *ptr;
};

#ifndef CTS_USES_VULKANSC
typedef PointerWrapper<VkPipelineViewportDepthClipControlCreateInfoEXT>
    PipelineViewportDepthClipControlCreateInfoWrapper;
typedef PointerWrapper<VkPipelineRenderingCreateInfoKHR> PipelineRenderingCreateInfoWrapper;
typedef PointerWrapper<VkRenderingAttachmentLocationInfoKHR> RenderingAttachmentLocationInfoWrapper;
typedef PointerWrapper<VkRenderingInputAttachmentIndexInfoKHR> RenderingInputAttachmentIndexInfoWrapper;
typedef PointerWrapper<VkPipelineCreationFeedbackCreateInfoEXT> PipelineCreationFeedbackCreateInfoWrapper;
typedef ConstPointerWrapper<VkPipelineShaderStageModuleIdentifierCreateInfoEXT>
    PipelineShaderStageModuleIdentifierCreateInfoWrapper;
typedef PointerWrapper<VkPipelineRepresentativeFragmentTestStateCreateInfoNV>
    PipelineRepresentativeFragmentTestCreateInfoWrapper;
typedef PointerWrapper<VkPipelineBinaryInfoKHR> PipelineBinaryInfoWrapper;
typedef VkPipelineCreateFlags2KHR PipelineCreateFlags2;
typedef VkShaderCreateFlagsEXT ShaderCreateFlags;
typedef PointerWrapper<VkPipelineRobustnessCreateInfoEXT> PipelineRobustnessCreateInfoWrapper;
#else
typedef PointerWrapper<void> PipelineViewportDepthClipControlCreateInfoWrapper;
typedef PointerWrapper<void> PipelineRenderingCreateInfoWrapper;
typedef PointerWrapper<void> RenderingAttachmentLocationInfoWrapper;
typedef PointerWrapper<void> RenderingInputAttachmentIndexInfoWrapper;
typedef PointerWrapper<void> PipelineCreationFeedbackCreateInfoWrapper;
typedef ConstPointerWrapper<void> PipelineShaderStageModuleIdentifierCreateInfoWrapper;
typedef PointerWrapper<void> PipelineRepresentativeFragmentTestCreateInfoWrapper;
typedef PointerWrapper<void> PipelineBinaryInfoWrapper;
typedef uint64_t PipelineCreateFlags2;
typedef uint32_t ShaderCreateFlags;
typedef PointerWrapper<void> PipelineRobustnessCreateInfoWrapper;
#endif

PipelineCreateFlags2 translateCreateFlag(VkPipelineCreateFlags flagToTranslate);

class PipelineLayoutWrapper
{
public:
    PipelineLayoutWrapper() = default;
    PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                          const VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
                          const VkPushConstantRange *pushConstantRange    = nullptr);
    PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                          const std::vector<vk::Move<VkDescriptorSetLayout>> &descriptorSetLayout);
    PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                          uint32_t setLayoutCount, const VkDescriptorSetLayout *descriptorSetLayout);
    PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                          const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks * = nullptr);
    PipelineLayoutWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk,
                          const VkDevice device, const uint32_t setLayoutCount,
                          const VkDescriptorSetLayout *descriptorSetLayout, const uint32_t pushConstantRangeCount,
                          const VkPushConstantRange *pPushConstantRanges,
                          const VkPipelineLayoutCreateFlags flags = (VkPipelineLayoutCreateFlags)0u);
    PipelineLayoutWrapper(const PipelineLayoutWrapper &rhs) = delete;
    PipelineLayoutWrapper(PipelineLayoutWrapper &&rhs) noexcept;
    ~PipelineLayoutWrapper() = default;

    const VkPipelineLayout operator*(void) const
    {
        return *m_pipelineLayout;
    }
    const VkPipelineLayout get(void) const
    {
        return *m_pipelineLayout;
    }
    PipelineLayoutWrapper &operator=(const PipelineLayoutWrapper &rhs) = delete;
    PipelineLayoutWrapper &operator=(PipelineLayoutWrapper &&rhs);
    void destroy(void)
    {
        m_pipelineLayout = vk::Move<VkPipelineLayout>{};
    }

    uint32_t getSetLayoutCount(void) const
    {
        return m_setLayoutCount;
    }
    const VkDescriptorSetLayout *getSetLayouts(void) const
    {
        return m_setLayouts.data();
    }
    VkDescriptorSetLayout *getSetLayout(uint32_t i)
    {
        return &m_setLayouts[i];
    }
    uint32_t getPushConstantRangeCount(void) const
    {
        return m_pushConstantRangeCount;
    }
    const VkPushConstantRange *getPushConstantRanges(void) const
    {
        return m_pushConstantRanges.data();
    }

    void bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, uint32_t firstSet,
                            uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets,
                            uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets) const;

private:
    PipelineConstructionType m_pipelineConstructionType;
    const DeviceInterface *m_vk;
    VkDevice m_device;
    VkPipelineLayoutCreateFlags m_flags;
    uint32_t m_setLayoutCount;
    std::vector<VkDescriptorSetLayout> m_setLayouts;
    uint32_t m_pushConstantRangeCount;
    std::vector<VkPushConstantRange> m_pushConstantRanges;
    vk::Move<VkPipelineLayout> m_pipelineLayout;
};

class RenderPassWrapper
{
public:
    RenderPassWrapper() = default;
    RenderPassWrapper(const DeviceInterface &vk, VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo,
                      bool dynamicRendering);
#ifndef CTS_USES_VULKANSC
    RenderPassWrapper(const DeviceInterface &vk, VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                      const VkAttachmentFeedbackLoopInfoEXT *attachmentFeedbackLoopInfo);
#endif
    RenderPassWrapper(const DeviceInterface &vk, VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                      bool dynamicRendering);
    RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                      const VkRenderPassCreateInfo *pCreateInfo);
    RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk, VkDevice device,
                      const VkRenderPassCreateInfo2 *pCreateInfo);
    RenderPassWrapper(PipelineConstructionType pipelineConstructionType, const DeviceInterface &vk,
                      const VkDevice device, const VkFormat colorFormat = VK_FORMAT_UNDEFINED,
                      const VkFormat depthStencilFormat             = VK_FORMAT_UNDEFINED,
                      const VkAttachmentLoadOp loadOperation        = VK_ATTACHMENT_LOAD_OP_CLEAR,
                      const VkImageLayout finalLayoutColor          = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      const VkImageLayout finalLayoutDepthStencil   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      const VkImageLayout subpassLayoutColor        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      const VkImageLayout subpassLayoutDepthStencil = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                      const VkAllocationCallbacks *const allocationCallbacks = nullptr);

    RenderPassWrapper(RenderPassWrapper &&rhs) noexcept;
    RenderPassWrapper &operator=(RenderPassWrapper &&rhs) noexcept;

    // Clones the existing render pass by internally storing the same VkRenderPass handle. The new render pass wrapper
    // will not manage the lifetime of the VkRenderPass object, but it will allow you to create new framebuffers
    // sharing the same render pass.
    RenderPassWrapper clone() const;

    ~RenderPassWrapper() = default;

    const VkRenderPass operator*(void) const
    {
        return m_renderPass;
    }
    const VkRenderPass get(void) const
    {
        return m_renderPass;
    }
    const VkFramebuffer getFramebuffer(void) const
    {
        return m_framebuffer ? *m_framebuffer : VK_NULL_HANDLE;
    }
    void resetLayouts(void);

    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const uint32_t clearValueCount, const VkClearValue *clearValues,
               const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE, const void *pNext = nullptr) const;
    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const VkClearValue &clearValue, const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const tcu::Vec4 &clearColor, const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const tcu::Vec4 &clearColor, const float clearDepth, const uint32_t clearStencil,
               const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;
    void begin(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRect2D &renderArea,
               const tcu::UVec4 &clearColor, const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) const;

    void end(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const;
    void nextSubpass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                     const VkSubpassContents contents) const;

    void createFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkFramebufferCreateInfo *pCreateInfo,
                           const std::vector<vk::VkImage> &images);
    void createFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkFramebufferCreateInfo *pCreateInfo,
                           vk::VkImage colorImage, vk::VkImage depthStencilImage = VK_NULL_HANDLE);
    void createFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkImage colorImage,
                           const VkImageView colorAttachment, const uint32_t width, const uint32_t height,
                           const uint32_t layers = 1u);
    void createFramebuffer(const DeviceInterface &vk, const VkDevice device, const uint32_t attachmentCount,
                           const VkImage *imagesArray, const VkImageView *attachmentsArray, const uint32_t width,
                           const uint32_t height, const uint32_t layers = 1u);

private:
    void beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const;

    bool m_isDynamicRendering = false;
    vk::Move<vk::VkRenderPass> m_renderPassPtr;
    vk::VkRenderPass m_renderPass = VK_NULL_HANDLE;
    vk::Move<vk::VkFramebuffer> m_framebuffer;

#ifndef CTS_USES_VULKANSC
    vk::VkAttachmentFeedbackLoopInfoEXT m_attachmentFeedbackLoopInfo = {};

    struct Subpass
    {
        struct Attachment
        {
            uint32_t index                               = VK_ATTACHMENT_UNUSED;
            vk::VkRenderingAttachmentInfo attachmentInfo = {};
            vk::VkFormat format;
            vk::VkAttachmentLoadOp stencilLoadOp   = vk::VK_ATTACHMENT_LOAD_OP_LOAD;
            vk::VkAttachmentStoreOp stencilStoreOp = vk::VK_ATTACHMENT_STORE_OP_STORE;
        };
        mutable std::vector<Attachment> m_colorAttachments;
        mutable Attachment m_depthStencilAttachment;
        mutable std::vector<Attachment> m_resolveAttachments;
        mutable VkMultisampledRenderToSingleSampledInfoEXT m_msrtss    = {};
        mutable VkSubpassDescriptionDepthStencilResolve m_dsr          = {};
        mutable VkAttachmentReference2 m_depthStencilResolveAttachment = {};
    };
    struct SubpassDependency
    {
        SubpassDependency(const VkSubpassDependency &dependency);
        SubpassDependency(const VkSubpassDependency2 &dependency);

        uint32_t srcSubpass;
        uint32_t dstSubpass;
        VkPipelineStageFlags2 srcStageMask;
        VkPipelineStageFlags2 dstStageMask;
        VkAccessFlags2 srcAccessMask;
        VkAccessFlags2 dstAccessMask;
        VkDependencyFlags dependencyFlags;
        bool sync2;
    };
    std::vector<Subpass> m_subpasses;
    std::vector<SubpassDependency> m_dependencies;
    std::vector<vk::VkAttachmentDescription2> m_attachments;
    std::vector<vk::VkImage> m_images;
    std::vector<vk::VkImageView> m_imageViews;
    mutable std::vector<vk::VkClearValue> m_clearValues;
    mutable std::vector<vk::VkImageLayout> m_layouts;
    mutable uint32_t m_activeSubpass = 0;
    mutable vk::VkRenderingInfo m_renderingInfo;
    uint32_t m_layers = 1;
    std::vector<uint32_t> m_viewMasks;
    mutable bool m_secondaryCommandBuffers;

    // Returns true if any attachment was cleared.
    bool clearAttachments(const DeviceInterface &vk, const VkCommandBuffer commandBuffer) const;

    void updateLayout(VkImage updatedImage, VkImageLayout newLayout) const;
    void transitionLayouts(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const Subpass &subpass,
                           bool renderPassBegin) const;
    void insertDependencies(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, uint32_t subpassIdx) const;

public:
    void fillInheritanceRenderingInfo(uint32_t subpassIndex, std::vector<vk::VkFormat> *colorFormats,
                                      vk::VkCommandBufferInheritanceRenderingInfo *inheritanceRenderingInfo) const;

private:
#endif
};

class ShaderWrapper
{
public:
    ShaderWrapper();

    ShaderWrapper(const DeviceInterface &vk, VkDevice device, const vk::ProgramBinary &binary,
                  const vk::VkShaderModuleCreateFlags createFlags = 0u);

    ShaderWrapper(const ShaderWrapper &rhs) noexcept;

    ~ShaderWrapper() = default;

    ShaderWrapper &operator=(const ShaderWrapper &rhs) noexcept;

    bool isSet(void) const
    {
        return m_binary != nullptr;
    }

    vk::VkShaderModule getModule(void) const;

    size_t getCodeSize(void) const;
    void *getBinary(void) const;

    void createModule(void);
    void setLayoutAndSpecialization(const PipelineLayoutWrapper *layout,
                                    const VkSpecializationInfo *specializationInfo);

    const PipelineLayoutWrapper *getPipelineLayout(void) const
    {
        return m_layout;
    }
    const VkSpecializationInfo *getSpecializationInfo(void) const
    {
        return m_specializationInfo;
    }

#ifndef CTS_USES_VULKANSC
    vk::VkShaderEXT getShader(void) const
    {
        return m_shader ? *m_shader : VK_NULL_HANDLE;
    }
    void setShader(Move<VkShaderEXT> shader)
    {
        m_shader = shader;
    }

    void addFlags(const VkShaderCreateFlagsEXT flags)
    {
        m_shaderCreateFlags |= flags;
    }
    void getShaderBinary(void);
    size_t getShaderBinaryDataSize(void)
    {
        return m_binaryDataSize;
    }
    void *getShaderBinaryData(void)
    {
        return m_binaryData.data();
    }
#endif

private:
    const DeviceInterface *m_vk;
    VkDevice m_device;
    const vk::ProgramBinary *m_binary;
    vk::VkShaderModuleCreateFlags m_moduleCreateFlags;
    mutable vk::Move<vk::VkShaderModule> m_module;
    const PipelineLayoutWrapper *m_layout;
    const VkSpecializationInfo *m_specializationInfo;
#ifndef CTS_USES_VULKANSC
    vk::Move<vk::VkShaderEXT> m_shader;
    VkShaderCreateFlagsEXT m_shaderCreateFlags;
    size_t m_binaryDataSize;
    std::vector<uint8_t> m_binaryData;
#endif
};

// Class that can build monolithic pipeline or fully separated pipeline libraries
// depending on PipelineType specified in the constructor.
// Rarely needed configuration was extracted to setDefault*/disable* functions while common
// state setup is provided as arguments of four setup* functions - one for each state group.
class GraphicsPipelineWrapper
{
public:
    GraphicsPipelineWrapper(const InstanceInterface &vki, const DeviceInterface &vk, VkPhysicalDevice physicalDevice,
                            VkDevice device, const std::vector<std::string> &deviceExtensions,
                            const PipelineConstructionType pipelineConstructionType,
                            const VkPipelineCreateFlags flags = 0u);

    GraphicsPipelineWrapper(GraphicsPipelineWrapper &&) noexcept;

    ~GraphicsPipelineWrapper(void) = default;

    // By default pipelineLayout used for monotlithic pipeline is taken from layout specified
    // in setupPreRasterizationShaderState but when there are also descriptor sets needed for fragment
    // shader bindings then separate pipeline layout for monolithic pipeline must be provided
    GraphicsPipelineWrapper &setMonolithicPipelineLayout(const PipelineLayoutWrapper &layout);

    // By default dynamic state has to be specified before specifying other CreateInfo structures
    GraphicsPipelineWrapper &setDynamicState(const VkPipelineDynamicStateCreateInfo *dynamicState);

    // When this wrapper is used with functionality provided by VK_KHR_pipeline_binary
    // then we need to be able to set binaries for individual pipeline parts as well as for monolithic pipeline.
    GraphicsPipelineWrapper &setMonolithicPipelineBinaries(PipelineBinaryInfoWrapper binaries);

    // Specify the representative fragment test state.
    GraphicsPipelineWrapper &setRepresentativeFragmentTestState(
        PipelineRepresentativeFragmentTestCreateInfoWrapper representativeFragmentTestState);

    // Specify pipeline robustness state
    GraphicsPipelineWrapper &setPipelineRobustnessState(PipelineRobustnessCreateInfoWrapper pipelineRobustnessState);

    // Specifying how a pipeline is created using VkPipelineCreateFlags2CreateInfoKHR.
    GraphicsPipelineWrapper &setPipelineCreateFlags2(PipelineCreateFlags2 pipelineFlags2);

    // Specify how shaders should be created with explicit flags. Note we could try to unify this with
    // setPipelineCreateFlags2 but the equivalence is not direct and some flags do not map.
    GraphicsPipelineWrapper &setShaderCreateFlags(ShaderCreateFlags shaderFlags);

    // Specify topology that is used by default InputAssemblyState in vertex input state. This needs to be
    // specified only when there is no custom InputAssemblyState provided in setupVertexInputState and when
    // topology is diferent then VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST which is used by default.
    GraphicsPipelineWrapper &setDefaultTopology(const VkPrimitiveTopology topology);

    // Specify patch control points that is used by default TessellationState in pre-rasterization shader state.
    // This can to be specified only when there is no custom TessellationState provided in
    // setupPreRasterizationShaderState and when patchControlPoints is diferent then 3 which is used by default.
    // A value of std::numeric_limits<uint32_t>::max() forces the tessellation state to be null.
    GraphicsPipelineWrapper &setDefaultPatchControlPoints(const uint32_t patchControlPoints);

    // Specify tesellation domain origin, used by the tessellation state in pre-rasterization shader state.
    GraphicsPipelineWrapper &setDefaultTessellationDomainOrigin(const VkTessellationDomainOrigin domainOrigin,
                                                                bool forceExtStruct = false);

    // Enable discarding of primitives that is used by default RasterizationState in pre-rasterization shader state.
    // This can be specified only when there is no custom RasterizationState provided in setupPreRasterizationShaderState.
    GraphicsPipelineWrapper &setDefaultRasterizerDiscardEnable(const bool rasterizerDiscardEnable = true);

    // When some states are not provided then default structures can be used. This behaviour can be turned on by one of below methods.
    // Some tests require those states to be NULL so we can't assume using default versions.
    GraphicsPipelineWrapper &setDefaultRasterizationState(void);
    GraphicsPipelineWrapper &setDefaultDepthStencilState(void);
    GraphicsPipelineWrapper &setDefaultColorBlendState(void);
    GraphicsPipelineWrapper &setDefaultMultisampleState(void);
    GraphicsPipelineWrapper &setDefaultVertexInputState(const bool useDefaultVertexInputState);

    // Pre-rasterization shader state uses provieded viewports and scissors to create ViewportState. By default
    // number of viewports and scissors is same as number of items in vector but when vectors are empty then by
    // default count of viewports/scissors is set to 1. This can be changed by below functions.
    GraphicsPipelineWrapper &setDefaultViewportsCount(uint32_t viewportCount = 0u);
    GraphicsPipelineWrapper &setDefaultScissorsCount(uint32_t scissorCount = 0u);

    // Pre-rasterization shader state uses default ViewportState, this method extends the internal structure.
    GraphicsPipelineWrapper &setViewportStatePnext(const void *pNext);

#ifndef CTS_USES_VULKANSC
    GraphicsPipelineWrapper &setRenderingColorAttachmentsInfo(
        PipelineRenderingCreateInfoWrapper pipelineRenderingCreateInfo);
#endif

    // Pre-rasterization shader state uses provieded viewports and scissors to create ViewportState. When disableViewportState
    // is used then ViewportState won't be constructed and NULL will be used.
    GraphicsPipelineWrapper &disableViewportState(const bool disable = true);

    // Setup vertex input state. When VertexInputState or InputAssemblyState are not provided then default structures will be used.
    GraphicsPipelineWrapper &setupVertexInputState(
        const VkPipelineVertexInputStateCreateInfo *vertexInputState     = nullptr,
        const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyState = nullptr,
        const VkPipelineCache partPipelineCache                          = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback   = PipelineCreationFeedbackCreateInfoWrapper(),
        PipelineBinaryInfoWrapper partBinaries = PipelineBinaryInfoWrapper(), const bool useNullPtrs = false);

    // When disableShaderModules is used module attributes in VkPipelineShaderStageCreateInfo will be set to NULL.
    // This is needed for VK_KHR_pipeline_binary tests where we need to construct a pipeline from pipeline binaries without using modules.
    GraphicsPipelineWrapper &disableShaderModules(const bool disable = true);

    // Setup pre-rasterization shader state.
    GraphicsPipelineWrapper &setupPreRasterizationShaderState(
        const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper vertexShaderModule,
        const VkPipelineRasterizationStateCreateInfo *rasterizationState = nullptr,
        const ShaderWrapper tessellationControlShader                    = ShaderWrapper(),
        const ShaderWrapper tessellationEvalShader                       = ShaderWrapper(),
        const ShaderWrapper geometryShader = ShaderWrapper(), const VkSpecializationInfo *specializationInfo = nullptr,
        VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState = nullptr,
        PipelineRenderingCreateInfoWrapper rendering                   = PipelineRenderingCreateInfoWrapper(),
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

    GraphicsPipelineWrapper &setupPreRasterizationShaderState2(
        const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper vertexShaderModule,
        const VkPipelineRasterizationStateCreateInfo *rasterizationState          = nullptr,
        const ShaderWrapper tessellationControlShader                             = ShaderWrapper(),
        const ShaderWrapper tessellationEvalShader                                = ShaderWrapper(),
        const ShaderWrapper geometryShader                                        = ShaderWrapper(),
        const VkSpecializationInfo *vertSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *tescSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *teseSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *geomSpecializationInfo                        = nullptr,
        VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState = nullptr,
        PipelineRenderingCreateInfoWrapper rendering                   = PipelineRenderingCreateInfoWrapper(),
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper());

    // Note: VkPipelineShaderStageModuleIdentifierCreateInfoEXT::pIdentifier will not be copied. They need to continue to exist outside this wrapper.
    GraphicsPipelineWrapper &setupPreRasterizationShaderState3(
        const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper vertexShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper vertShaderModuleId =
            PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
        const VkPipelineRasterizationStateCreateInfo *rasterizationState = nullptr,
        const ShaderWrapper tessellationControlShader                    = ShaderWrapper(),
        PipelineShaderStageModuleIdentifierCreateInfoWrapper tescShaderModuleId =
            PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
        const ShaderWrapper tessellationEvalShader = ShaderWrapper(),
        PipelineShaderStageModuleIdentifierCreateInfoWrapper teseShaderModuleId =
            PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
        const ShaderWrapper geometryShader = ShaderWrapper(),
        PipelineShaderStageModuleIdentifierCreateInfoWrapper geomShaderModuleId =
            PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
        const VkSpecializationInfo *vertSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *tescSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *teseSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *geomSpecializationInfo                        = nullptr,
        VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState = nullptr,
        PipelineRenderingCreateInfoWrapper rendering                   = PipelineRenderingCreateInfoWrapper(),
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper(),
        PipelineBinaryInfoWrapper partBinaries                         = PipelineBinaryInfoWrapper());

#ifndef CTS_USES_VULKANSC
    // Setup pre-rasterization shader state, mesh shading version.
    GraphicsPipelineWrapper &setupPreRasterizationMeshShaderState(
        const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper taskShader, const ShaderWrapper meshShader,
        const VkPipelineRasterizationStateCreateInfo *rasterizationState          = nullptr,
        const VkSpecializationInfo *taskSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *meshSpecializationInfo                        = nullptr,
        VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState = nullptr,
        PipelineRenderingCreateInfoWrapper rendering                  = PipelineRenderingCreateInfoWrapper(),
        const VkPipelineCache partPipelineCache                       = VK_NULL_HANDLE,
        VkPipelineCreationFeedbackCreateInfoEXT *partCreationFeedback = nullptr);

    GraphicsPipelineWrapper &setupPreRasterizationMeshShaderState2(
        const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors,
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper taskShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper taskShaderModuleId,
        const ShaderWrapper meshShader, PipelineShaderStageModuleIdentifierCreateInfoWrapper meshShaderModuleId,
        const VkPipelineRasterizationStateCreateInfo *rasterizationState          = nullptr,
        const VkSpecializationInfo *taskSpecializationInfo                        = nullptr,
        const VkSpecializationInfo *meshSpecializationInfo                        = nullptr,
        VkPipelineFragmentShadingRateStateCreateInfoKHR *fragmentShadingRateState = nullptr,
        PipelineRenderingCreateInfoWrapper rendering                  = PipelineRenderingCreateInfoWrapper(),
        const VkPipelineCache partPipelineCache                       = VK_NULL_HANDLE,
        VkPipelineCreationFeedbackCreateInfoEXT *partCreationFeedback = nullptr);
#endif // CTS_USES_VULKANSC

    // Setup fragment shader state.
    GraphicsPipelineWrapper &setupFragmentShaderState(
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper fragmentShaderModule,
        const VkPipelineDepthStencilStateCreateInfo *depthStencilState = nullptr,
        const VkPipelineMultisampleStateCreateInfo *multisampleState   = nullptr,
        const VkSpecializationInfo *specializationInfo                 = nullptr,
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper(),
        RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfo =
            RenderingInputAttachmentIndexInfoWrapper());

    // Note: VkPipelineShaderStageModuleIdentifierCreateInfoEXT::pIdentifier will not be copied. They need to continue to exist outside this wrapper.
    GraphicsPipelineWrapper &setupFragmentShaderState2(
        const PipelineLayoutWrapper &layout, const VkRenderPass renderPass, const uint32_t subpass,
        const ShaderWrapper fragmentShaderModule,
        PipelineShaderStageModuleIdentifierCreateInfoWrapper fragmentShaderModuleId =
            PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
        const VkPipelineDepthStencilStateCreateInfo *depthStencilState = nullptr,
        const VkPipelineMultisampleStateCreateInfo *multisampleState   = nullptr,
        const VkSpecializationInfo *specializationInfo                 = nullptr,
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper(),
        RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfo =
            RenderingInputAttachmentIndexInfoWrapper(),
        PipelineBinaryInfoWrapper partBinaries = PipelineBinaryInfoWrapper());

    // Setup fragment output state.
    GraphicsPipelineWrapper &setupFragmentOutputState(
        const VkRenderPass renderPass, const uint32_t subpass = 0u,
        const VkPipelineColorBlendStateCreateInfo *colorBlendState     = nullptr,
        const VkPipelineMultisampleStateCreateInfo *multisampleState   = nullptr,
        const VkPipelineCache partPipelineCache                        = VK_NULL_HANDLE,
        PipelineCreationFeedbackCreateInfoWrapper partCreationFeedback = PipelineCreationFeedbackCreateInfoWrapper(),
        RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfo =
            RenderingAttachmentLocationInfoWrapper(),
        PipelineBinaryInfoWrapper partBinaries = PipelineBinaryInfoWrapper());

    // Build pipeline object out of provided state.
    void buildPipeline(
        const VkPipelineCache pipelineCache = VK_NULL_HANDLE, const VkPipeline basePipelineHandle = VK_NULL_HANDLE,
        const int32_t basePipelineIndex                            = 0,
        PipelineCreationFeedbackCreateInfoWrapper creationFeedback = PipelineCreationFeedbackCreateInfoWrapper(),
        void *pNext                                                = nullptr);
    // Create shader objects if used
#ifndef CTS_USES_VULKANSC
    vk::VkShaderStageFlags getNextStages(vk::VkShaderStageFlagBits shaderStage, bool tessellationShaders,
                                         bool geometryShaders, bool link);
    vk::VkShaderCreateInfoEXT makeShaderCreateInfo(VkShaderStageFlagBits stage, ShaderWrapper &shader, bool link,
                                                   bool binary, ShaderWrapper &other);
    void createShaders(bool linked, bool binary);
#endif

    // Bind pipeline or shader objects
    void bind(vk::VkCommandBuffer cmdBuffer) const;

    // Returns true when pipeline was build using buildPipeline method.
    bool wasBuild(void) const;
    // Returns true when pipeline or shader objects was built.
    bool wasPipelineOrShaderObjectBuild(void) const;

    // Get compleate pipeline. GraphicsPipelineWrapper preserves ovnership and will destroy pipeline in its destructor.
    vk::VkPipeline getPipeline(void) const;

    // Get partial pipeline. GraphicsPipelineWrapper preserves ovnership and will desroy pipeline in its destructor.
    vk::VkPipeline getPartialPipeline(uint32_t part) const;

    // Get compleate pipeline create info.
    const VkGraphicsPipelineCreateInfo &getPipelineCreateInfo(void) const;

    // Get partial pipeline create info.
    const VkGraphicsPipelineCreateInfo &getPartialPipelineCreateInfo(uint32_t part) const;

#ifndef CTS_USES_VULKANSC
    // Get particular shader. GraphicsPipelineWrapper preserves ovnership and will destroy shaders in its destructor.
    vk::VkShaderEXT getShader(VkShaderStageFlagBits stage) const;
#endif

    // Destroy compleate pipeline - pipeline parts are not destroyed.
    void destroyPipeline(void);

protected:
    // No default constructor - use parametrized constructor or emplace_back in case of vectors.
    GraphicsPipelineWrapper() = default;

    // Dynamic states that are only dynamic in shader objects
    bool isShaderObjectDynamic(vk::VkDynamicState dynamicState) const;
    void setShaderObjectDynamicStates(vk::VkCommandBuffer cmdBuffer) const;

    struct InternalData;

protected:
    static constexpr size_t kMaxPipelineParts = 4u;

    // Store partial pipelines when non monolithic construction was used.
    Move<VkPipeline> m_pipelineParts[kMaxPipelineParts];

    // Store monolithic pipeline or linked pipeline libraries.
    Move<VkPipeline> m_pipelineFinal;

    // Store internal data that is needed only for pipeline construction.
    de::SharedPtr<InternalData> m_internalData;
};

std::vector<VkDynamicState> getShaderObjectDynamicStatesFromExtensions(const std::vector<std::string> &extensions);

} // namespace vk

#endif // _VKPIPELINECONSTRUCTIONUTIL_HPP
