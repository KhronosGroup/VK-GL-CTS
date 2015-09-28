/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

inline VkAllocCallbacks makeAllocCallbacks (void* pUserData, PFN_vkAllocFunction pfnAlloc, PFN_vkFreeFunction pfnFree)
{
	VkAllocCallbacks res;
	res.pUserData	= pUserData;
	res.pfnAlloc	= pfnAlloc;
	res.pfnFree		= pfnFree;
	return res;
}

inline VkExtent3D makeExtent3D (deInt32 width, deInt32 height, deInt32 depth)
{
	VkExtent3D res;
	res.width	= width;
	res.height	= height;
	res.depth	= depth;
	return res;
}

inline VkMemoryRequirements makeMemoryRequirements (VkDeviceSize size, VkDeviceSize alignment, deUint32 memoryTypeBits)
{
	VkMemoryRequirements res;
	res.size			= size;
	res.alignment		= alignment;
	res.memoryTypeBits	= memoryTypeBits;
	return res;
}

inline VkSparseMemoryBindInfo makeSparseMemoryBindInfo (VkDeviceSize rangeOffset, VkDeviceSize rangeSize, VkDeviceSize memOffset, VkDeviceMemory mem, VkSparseMemoryBindFlags flags)
{
	VkSparseMemoryBindInfo res;
	res.rangeOffset	= rangeOffset;
	res.rangeSize	= rangeSize;
	res.memOffset	= memOffset;
	res.mem			= mem;
	res.flags		= flags;
	return res;
}

inline VkImageSubresource makeImageSubresource (VkImageAspect aspect, deUint32 mipLevel, deUint32 arrayLayer)
{
	VkImageSubresource res;
	res.aspect		= aspect;
	res.mipLevel	= mipLevel;
	res.arrayLayer	= arrayLayer;
	return res;
}

inline VkOffset3D makeOffset3D (deInt32 x, deInt32 y, deInt32 z)
{
	VkOffset3D res;
	res.x	= x;
	res.y	= y;
	res.z	= z;
	return res;
}

inline VkSubresourceLayout makeSubresourceLayout (VkDeviceSize offset, VkDeviceSize size, VkDeviceSize rowPitch, VkDeviceSize depthPitch)
{
	VkSubresourceLayout res;
	res.offset		= offset;
	res.size		= size;
	res.rowPitch	= rowPitch;
	res.depthPitch	= depthPitch;
	return res;
}

inline VkChannelMapping makeChannelMapping (VkChannelSwizzle r, VkChannelSwizzle g, VkChannelSwizzle b, VkChannelSwizzle a)
{
	VkChannelMapping res;
	res.r	= r;
	res.g	= g;
	res.b	= b;
	res.a	= a;
	return res;
}

inline VkImageSubresourceRange makeImageSubresourceRange (VkImageAspectFlags aspectMask, deUint32 baseMipLevel, deUint32 mipLevels, deUint32 baseArrayLayer, deUint32 arraySize)
{
	VkImageSubresourceRange res;
	res.aspectMask		= aspectMask;
	res.baseMipLevel	= baseMipLevel;
	res.mipLevels		= mipLevels;
	res.baseArrayLayer	= baseArrayLayer;
	res.arraySize		= arraySize;
	return res;
}

inline VkSpecializationMapEntry makeSpecializationMapEntry (deUint32 constantId, deUintptr size, deUint32 offset)
{
	VkSpecializationMapEntry res;
	res.constantId	= constantId;
	res.size		= size;
	res.offset		= offset;
	return res;
}

inline VkSpecializationInfo makeSpecializationInfo (deUint32 mapEntryCount, const VkSpecializationMapEntry* pMap, deUintptr dataSize, const void* pData)
{
	VkSpecializationInfo res;
	res.mapEntryCount	= mapEntryCount;
	res.pMap			= pMap;
	res.dataSize		= dataSize;
	res.pData			= pData;
	return res;
}

inline VkVertexInputBindingDescription makeVertexInputBindingDescription (deUint32 binding, deUint32 strideInBytes, VkVertexInputStepRate stepRate)
{
	VkVertexInputBindingDescription res;
	res.binding			= binding;
	res.strideInBytes	= strideInBytes;
	res.stepRate		= stepRate;
	return res;
}

inline VkVertexInputAttributeDescription makeVertexInputAttributeDescription (deUint32 location, deUint32 binding, VkFormat format, deUint32 offsetInBytes)
{
	VkVertexInputAttributeDescription res;
	res.location		= location;
	res.binding			= binding;
	res.format			= format;
	res.offsetInBytes	= offsetInBytes;
	return res;
}

inline VkViewport makeViewport (float originX, float originY, float width, float height, float minDepth, float maxDepth)
{
	VkViewport res;
	res.originX		= originX;
	res.originY		= originY;
	res.width		= width;
	res.height		= height;
	res.minDepth	= minDepth;
	res.maxDepth	= maxDepth;
	return res;
}

inline VkOffset2D makeOffset2D (deInt32 x, deInt32 y)
{
	VkOffset2D res;
	res.x	= x;
	res.y	= y;
	return res;
}

inline VkExtent2D makeExtent2D (deInt32 width, deInt32 height)
{
	VkExtent2D res;
	res.width	= width;
	res.height	= height;
	return res;
}

inline VkStencilOpState makeStencilOpState (VkStencilOp stencilFailOp, VkStencilOp stencilPassOp, VkStencilOp stencilDepthFailOp, VkCompareOp stencilCompareOp, deUint32 stencilCompareMask, deUint32 stencilWriteMask, deUint32 stencilReference)
{
	VkStencilOpState res;
	res.stencilFailOp		= stencilFailOp;
	res.stencilPassOp		= stencilPassOp;
	res.stencilDepthFailOp	= stencilDepthFailOp;
	res.stencilCompareOp	= stencilCompareOp;
	res.stencilCompareMask	= stencilCompareMask;
	res.stencilWriteMask	= stencilWriteMask;
	res.stencilReference	= stencilReference;
	return res;
}

inline VkPipelineColorBlendAttachmentState makePipelineColorBlendAttachmentState (VkBool32 blendEnable, VkBlend srcBlendColor, VkBlend destBlendColor, VkBlendOp blendOpColor, VkBlend srcBlendAlpha, VkBlend destBlendAlpha, VkBlendOp blendOpAlpha, VkChannelFlags channelWriteMask)
{
	VkPipelineColorBlendAttachmentState res;
	res.blendEnable			= blendEnable;
	res.srcBlendColor		= srcBlendColor;
	res.destBlendColor		= destBlendColor;
	res.blendOpColor		= blendOpColor;
	res.srcBlendAlpha		= srcBlendAlpha;
	res.destBlendAlpha		= destBlendAlpha;
	res.blendOpAlpha		= blendOpAlpha;
	res.channelWriteMask	= channelWriteMask;
	return res;
}

inline VkPushConstantRange makePushConstantRange (VkShaderStageFlags stageFlags, deUint32 start, deUint32 length)
{
	VkPushConstantRange res;
	res.stageFlags	= stageFlags;
	res.start		= start;
	res.length		= length;
	return res;
}

inline VkDescriptorSetLayoutBinding makeDescriptorSetLayoutBinding (VkDescriptorType descriptorType, deUint32 arraySize, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSamplers)
{
	VkDescriptorSetLayoutBinding res;
	res.descriptorType		= descriptorType;
	res.arraySize			= arraySize;
	res.stageFlags			= stageFlags;
	res.pImmutableSamplers	= pImmutableSamplers;
	return res;
}

inline VkDescriptorTypeCount makeDescriptorTypeCount (VkDescriptorType type, deUint32 count)
{
	VkDescriptorTypeCount res;
	res.type	= type;
	res.count	= count;
	return res;
}

inline VkDescriptorBufferInfo makeDescriptorBufferInfo (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	VkDescriptorBufferInfo res;
	res.buffer	= buffer;
	res.offset	= offset;
	res.range	= range;
	return res;
}

inline VkAttachmentReference makeAttachmentReference (deUint32 attachment, VkImageLayout layout)
{
	VkAttachmentReference res;
	res.attachment	= attachment;
	res.layout		= layout;
	return res;
}

inline VkBufferCopy makeBufferCopy (VkDeviceSize srcOffset, VkDeviceSize destOffset, VkDeviceSize copySize)
{
	VkBufferCopy res;
	res.srcOffset	= srcOffset;
	res.destOffset	= destOffset;
	res.copySize	= copySize;
	return res;
}

inline VkImageSubresourceCopy makeImageSubresourceCopy (VkImageAspect aspect, deUint32 mipLevel, deUint32 arrayLayer, deUint32 arraySize)
{
	VkImageSubresourceCopy res;
	res.aspect		= aspect;
	res.mipLevel	= mipLevel;
	res.arrayLayer	= arrayLayer;
	res.arraySize	= arraySize;
	return res;
}

inline VkClearDepthStencilValue makeClearDepthStencilValue (float depth, deUint32 stencil)
{
	VkClearDepthStencilValue res;
	res.depth	= depth;
	res.stencil	= stencil;
	return res;
}

inline VkDispatchIndirectCmd makeDispatchIndirectCmd (deUint32 x, deUint32 y, deUint32 z)
{
	VkDispatchIndirectCmd res;
	res.x	= x;
	res.y	= y;
	res.z	= z;
	return res;
}

inline VkDrawIndexedIndirectCmd makeDrawIndexedIndirectCmd (deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance)
{
	VkDrawIndexedIndirectCmd res;
	res.indexCount		= indexCount;
	res.instanceCount	= instanceCount;
	res.firstIndex		= firstIndex;
	res.vertexOffset	= vertexOffset;
	res.firstInstance	= firstInstance;
	return res;
}

inline VkDrawIndirectCmd makeDrawIndirectCmd (deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance)
{
	VkDrawIndirectCmd res;
	res.vertexCount		= vertexCount;
	res.instanceCount	= instanceCount;
	res.firstVertex		= firstVertex;
	res.firstInstance	= firstInstance;
	return res;
}
