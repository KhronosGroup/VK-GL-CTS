/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
struct VkExtent2D
{
	deUint32	width;
	deUint32	height;
};

struct VkExtent3D
{
	deUint32	width;
	deUint32	height;
	deUint32	depth;
};

struct VkOffset2D
{
	deInt32	x;
	deInt32	y;
};

struct VkOffset3D
{
	deInt32	x;
	deInt32	y;
	deInt32	z;
};

struct VkRect2D
{
	VkOffset2D	offset;
	VkExtent2D	extent;
};

struct VkBaseInStructure
{
	VkStructureType					sType;
	const struct VkBaseInStructure*	pNext;
};

struct VkBaseOutStructure
{
	VkStructureType				sType;
	struct VkBaseOutStructure*	pNext;
};

struct VkBufferMemoryBarrier
{
	VkStructureType	sType;
	const void*		pNext;
	VkAccessFlags	srcAccessMask;
	VkAccessFlags	dstAccessMask;
	deUint32		srcQueueFamilyIndex;
	deUint32		dstQueueFamilyIndex;
	VkBuffer		buffer;
	VkDeviceSize	offset;
	VkDeviceSize	size;
};

struct VkDispatchIndirectCommand
{
	deUint32	x;
	deUint32	y;
	deUint32	z;
};

struct VkDrawIndexedIndirectCommand
{
	deUint32	indexCount;
	deUint32	instanceCount;
	deUint32	firstIndex;
	deInt32		vertexOffset;
	deUint32	firstInstance;
};

struct VkDrawIndirectCommand
{
	deUint32	vertexCount;
	deUint32	instanceCount;
	deUint32	firstVertex;
	deUint32	firstInstance;
};

struct VkImageSubresourceRange
{
	VkImageAspectFlags	aspectMask;
	deUint32			baseMipLevel;
	deUint32			levelCount;
	deUint32			baseArrayLayer;
	deUint32			layerCount;
};

struct VkImageMemoryBarrier
{
	VkStructureType			sType;
	const void*				pNext;
	VkAccessFlags			srcAccessMask;
	VkAccessFlags			dstAccessMask;
	VkImageLayout			oldLayout;
	VkImageLayout			newLayout;
	deUint32				srcQueueFamilyIndex;
	deUint32				dstQueueFamilyIndex;
	VkImage					image;
	VkImageSubresourceRange	subresourceRange;
};

struct VkMemoryBarrier
{
	VkStructureType	sType;
	const void*		pNext;
	VkAccessFlags	srcAccessMask;
	VkAccessFlags	dstAccessMask;
};

struct VkAllocationCallbacks
{
	void*									pUserData;
	PFN_vkAllocationFunction				pfnAllocation;
	PFN_vkReallocationFunction				pfnReallocation;
	PFN_vkFreeFunction						pfnFree;
	PFN_vkInternalAllocationNotification	pfnInternalAllocation;
	PFN_vkInternalFreeNotification			pfnInternalFree;
};

struct VkApplicationInfo
{
	VkStructureType	sType;
	const void*		pNext;
	const char*		pApplicationName;
	deUint32		applicationVersion;
	const char*		pEngineName;
	deUint32		engineVersion;
	deUint32		apiVersion;
};

struct VkFormatProperties
{
	VkFormatFeatureFlags	linearTilingFeatures;
	VkFormatFeatureFlags	optimalTilingFeatures;
	VkFormatFeatureFlags	bufferFeatures;
};

struct VkImageFormatProperties
{
	VkExtent3D			maxExtent;
	deUint32			maxMipLevels;
	deUint32			maxArrayLayers;
	VkSampleCountFlags	sampleCounts;
	VkDeviceSize		maxResourceSize;
};

struct VkInstanceCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkInstanceCreateFlags		flags;
	const VkApplicationInfo*	pApplicationInfo;
	deUint32					enabledLayerCount;
	const char* const*			ppEnabledLayerNames;
	deUint32					enabledExtensionCount;
	const char* const*			ppEnabledExtensionNames;
};

struct VkMemoryHeap
{
	VkDeviceSize		size;
	VkMemoryHeapFlags	flags;
};

struct VkMemoryType
{
	VkMemoryPropertyFlags	propertyFlags;
	deUint32				heapIndex;
};

struct VkPhysicalDeviceFeatures
{
	VkBool32	robustBufferAccess;
	VkBool32	fullDrawIndexUint32;
	VkBool32	imageCubeArray;
	VkBool32	independentBlend;
	VkBool32	geometryShader;
	VkBool32	tessellationShader;
	VkBool32	sampleRateShading;
	VkBool32	dualSrcBlend;
	VkBool32	logicOp;
	VkBool32	multiDrawIndirect;
	VkBool32	drawIndirectFirstInstance;
	VkBool32	depthClamp;
	VkBool32	depthBiasClamp;
	VkBool32	fillModeNonSolid;
	VkBool32	depthBounds;
	VkBool32	wideLines;
	VkBool32	largePoints;
	VkBool32	alphaToOne;
	VkBool32	multiViewport;
	VkBool32	samplerAnisotropy;
	VkBool32	textureCompressionETC2;
	VkBool32	textureCompressionASTC_LDR;
	VkBool32	textureCompressionBC;
	VkBool32	occlusionQueryPrecise;
	VkBool32	pipelineStatisticsQuery;
	VkBool32	vertexPipelineStoresAndAtomics;
	VkBool32	fragmentStoresAndAtomics;
	VkBool32	shaderTessellationAndGeometryPointSize;
	VkBool32	shaderImageGatherExtended;
	VkBool32	shaderStorageImageExtendedFormats;
	VkBool32	shaderStorageImageMultisample;
	VkBool32	shaderStorageImageReadWithoutFormat;
	VkBool32	shaderStorageImageWriteWithoutFormat;
	VkBool32	shaderUniformBufferArrayDynamicIndexing;
	VkBool32	shaderSampledImageArrayDynamicIndexing;
	VkBool32	shaderStorageBufferArrayDynamicIndexing;
	VkBool32	shaderStorageImageArrayDynamicIndexing;
	VkBool32	shaderClipDistance;
	VkBool32	shaderCullDistance;
	VkBool32	shaderFloat64;
	VkBool32	shaderInt64;
	VkBool32	shaderInt16;
	VkBool32	shaderResourceResidency;
	VkBool32	shaderResourceMinLod;
	VkBool32	sparseBinding;
	VkBool32	sparseResidencyBuffer;
	VkBool32	sparseResidencyImage2D;
	VkBool32	sparseResidencyImage3D;
	VkBool32	sparseResidency2Samples;
	VkBool32	sparseResidency4Samples;
	VkBool32	sparseResidency8Samples;
	VkBool32	sparseResidency16Samples;
	VkBool32	sparseResidencyAliased;
	VkBool32	variableMultisampleRate;
	VkBool32	inheritedQueries;
};

struct VkPhysicalDeviceLimits
{
	deUint32			maxImageDimension1D;
	deUint32			maxImageDimension2D;
	deUint32			maxImageDimension3D;
	deUint32			maxImageDimensionCube;
	deUint32			maxImageArrayLayers;
	deUint32			maxTexelBufferElements;
	deUint32			maxUniformBufferRange;
	deUint32			maxStorageBufferRange;
	deUint32			maxPushConstantsSize;
	deUint32			maxMemoryAllocationCount;
	deUint32			maxSamplerAllocationCount;
	VkDeviceSize		bufferImageGranularity;
	VkDeviceSize		sparseAddressSpaceSize;
	deUint32			maxBoundDescriptorSets;
	deUint32			maxPerStageDescriptorSamplers;
	deUint32			maxPerStageDescriptorUniformBuffers;
	deUint32			maxPerStageDescriptorStorageBuffers;
	deUint32			maxPerStageDescriptorSampledImages;
	deUint32			maxPerStageDescriptorStorageImages;
	deUint32			maxPerStageDescriptorInputAttachments;
	deUint32			maxPerStageResources;
	deUint32			maxDescriptorSetSamplers;
	deUint32			maxDescriptorSetUniformBuffers;
	deUint32			maxDescriptorSetUniformBuffersDynamic;
	deUint32			maxDescriptorSetStorageBuffers;
	deUint32			maxDescriptorSetStorageBuffersDynamic;
	deUint32			maxDescriptorSetSampledImages;
	deUint32			maxDescriptorSetStorageImages;
	deUint32			maxDescriptorSetInputAttachments;
	deUint32			maxVertexInputAttributes;
	deUint32			maxVertexInputBindings;
	deUint32			maxVertexInputAttributeOffset;
	deUint32			maxVertexInputBindingStride;
	deUint32			maxVertexOutputComponents;
	deUint32			maxTessellationGenerationLevel;
	deUint32			maxTessellationPatchSize;
	deUint32			maxTessellationControlPerVertexInputComponents;
	deUint32			maxTessellationControlPerVertexOutputComponents;
	deUint32			maxTessellationControlPerPatchOutputComponents;
	deUint32			maxTessellationControlTotalOutputComponents;
	deUint32			maxTessellationEvaluationInputComponents;
	deUint32			maxTessellationEvaluationOutputComponents;
	deUint32			maxGeometryShaderInvocations;
	deUint32			maxGeometryInputComponents;
	deUint32			maxGeometryOutputComponents;
	deUint32			maxGeometryOutputVertices;
	deUint32			maxGeometryTotalOutputComponents;
	deUint32			maxFragmentInputComponents;
	deUint32			maxFragmentOutputAttachments;
	deUint32			maxFragmentDualSrcAttachments;
	deUint32			maxFragmentCombinedOutputResources;
	deUint32			maxComputeSharedMemorySize;
	deUint32			maxComputeWorkGroupCount[3];
	deUint32			maxComputeWorkGroupInvocations;
	deUint32			maxComputeWorkGroupSize[3];
	deUint32			subPixelPrecisionBits;
	deUint32			subTexelPrecisionBits;
	deUint32			mipmapPrecisionBits;
	deUint32			maxDrawIndexedIndexValue;
	deUint32			maxDrawIndirectCount;
	float				maxSamplerLodBias;
	float				maxSamplerAnisotropy;
	deUint32			maxViewports;
	deUint32			maxViewportDimensions[2];
	float				viewportBoundsRange[2];
	deUint32			viewportSubPixelBits;
	deUintptr			minMemoryMapAlignment;
	VkDeviceSize		minTexelBufferOffsetAlignment;
	VkDeviceSize		minUniformBufferOffsetAlignment;
	VkDeviceSize		minStorageBufferOffsetAlignment;
	deInt32				minTexelOffset;
	deUint32			maxTexelOffset;
	deInt32				minTexelGatherOffset;
	deUint32			maxTexelGatherOffset;
	float				minInterpolationOffset;
	float				maxInterpolationOffset;
	deUint32			subPixelInterpolationOffsetBits;
	deUint32			maxFramebufferWidth;
	deUint32			maxFramebufferHeight;
	deUint32			maxFramebufferLayers;
	VkSampleCountFlags	framebufferColorSampleCounts;
	VkSampleCountFlags	framebufferDepthSampleCounts;
	VkSampleCountFlags	framebufferStencilSampleCounts;
	VkSampleCountFlags	framebufferNoAttachmentsSampleCounts;
	deUint32			maxColorAttachments;
	VkSampleCountFlags	sampledImageColorSampleCounts;
	VkSampleCountFlags	sampledImageIntegerSampleCounts;
	VkSampleCountFlags	sampledImageDepthSampleCounts;
	VkSampleCountFlags	sampledImageStencilSampleCounts;
	VkSampleCountFlags	storageImageSampleCounts;
	deUint32			maxSampleMaskWords;
	VkBool32			timestampComputeAndGraphics;
	float				timestampPeriod;
	deUint32			maxClipDistances;
	deUint32			maxCullDistances;
	deUint32			maxCombinedClipAndCullDistances;
	deUint32			discreteQueuePriorities;
	float				pointSizeRange[2];
	float				lineWidthRange[2];
	float				pointSizeGranularity;
	float				lineWidthGranularity;
	VkBool32			strictLines;
	VkBool32			standardSampleLocations;
	VkDeviceSize		optimalBufferCopyOffsetAlignment;
	VkDeviceSize		optimalBufferCopyRowPitchAlignment;
	VkDeviceSize		nonCoherentAtomSize;
};

struct VkPhysicalDeviceMemoryProperties
{
	deUint32		memoryTypeCount;
	VkMemoryType	memoryTypes[VK_MAX_MEMORY_TYPES];
	deUint32		memoryHeapCount;
	VkMemoryHeap	memoryHeaps[VK_MAX_MEMORY_HEAPS];
};

struct VkPhysicalDeviceSparseProperties
{
	VkBool32	residencyStandard2DBlockShape;
	VkBool32	residencyStandard2DMultisampleBlockShape;
	VkBool32	residencyStandard3DBlockShape;
	VkBool32	residencyAlignedMipSize;
	VkBool32	residencyNonResidentStrict;
};

struct VkPhysicalDeviceProperties
{
	deUint32							apiVersion;
	deUint32							driverVersion;
	deUint32							vendorID;
	deUint32							deviceID;
	VkPhysicalDeviceType				deviceType;
	char								deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
	deUint8								pipelineCacheUUID[VK_UUID_SIZE];
	VkPhysicalDeviceLimits				limits;
	VkPhysicalDeviceSparseProperties	sparseProperties;
};

struct VkQueueFamilyProperties
{
	VkQueueFlags	queueFlags;
	deUint32		queueCount;
	deUint32		timestampValidBits;
	VkExtent3D		minImageTransferGranularity;
};

struct VkDeviceQueueCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkDeviceQueueCreateFlags	flags;
	deUint32					queueFamilyIndex;
	deUint32					queueCount;
	const float*				pQueuePriorities;
};

struct VkDeviceCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkDeviceCreateFlags				flags;
	deUint32						queueCreateInfoCount;
	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
	deUint32						enabledLayerCount;
	const char* const*				ppEnabledLayerNames;
	deUint32						enabledExtensionCount;
	const char* const*				ppEnabledExtensionNames;
	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
};

struct VkExtensionProperties
{
	char		extensionName[VK_MAX_EXTENSION_NAME_SIZE];
	deUint32	specVersion;
};

struct VkLayerProperties
{
	char		layerName[VK_MAX_EXTENSION_NAME_SIZE];
	deUint32	specVersion;
	deUint32	implementationVersion;
	char		description[VK_MAX_DESCRIPTION_SIZE];
};

struct VkSubmitInfo
{
	VkStructureType				sType;
	const void*					pNext;
	deUint32					waitSemaphoreCount;
	const VkSemaphore*			pWaitSemaphores;
	const VkPipelineStageFlags*	pWaitDstStageMask;
	deUint32					commandBufferCount;
	const VkCommandBuffer*		pCommandBuffers;
	deUint32					signalSemaphoreCount;
	const VkSemaphore*			pSignalSemaphores;
};

struct VkMappedMemoryRange
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	memory;
	VkDeviceSize	offset;
	VkDeviceSize	size;
};

struct VkMemoryAllocateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceSize	allocationSize;
	deUint32		memoryTypeIndex;
};

struct VkMemoryRequirements
{
	VkDeviceSize	size;
	VkDeviceSize	alignment;
	deUint32		memoryTypeBits;
};

struct VkSparseMemoryBind
{
	VkDeviceSize			resourceOffset;
	VkDeviceSize			size;
	VkDeviceMemory			memory;
	VkDeviceSize			memoryOffset;
	VkSparseMemoryBindFlags	flags;
};

struct VkSparseBufferMemoryBindInfo
{
	VkBuffer					buffer;
	deUint32					bindCount;
	const VkSparseMemoryBind*	pBinds;
};

struct VkSparseImageOpaqueMemoryBindInfo
{
	VkImage						image;
	deUint32					bindCount;
	const VkSparseMemoryBind*	pBinds;
};

struct VkImageSubresource
{
	VkImageAspectFlags	aspectMask;
	deUint32			mipLevel;
	deUint32			arrayLayer;
};

struct VkSparseImageMemoryBind
{
	VkImageSubresource		subresource;
	VkOffset3D				offset;
	VkExtent3D				extent;
	VkDeviceMemory			memory;
	VkDeviceSize			memoryOffset;
	VkSparseMemoryBindFlags	flags;
};

struct VkSparseImageMemoryBindInfo
{
	VkImage							image;
	deUint32						bindCount;
	const VkSparseImageMemoryBind*	pBinds;
};

struct VkBindSparseInfo
{
	VkStructureType								sType;
	const void*									pNext;
	deUint32									waitSemaphoreCount;
	const VkSemaphore*							pWaitSemaphores;
	deUint32									bufferBindCount;
	const VkSparseBufferMemoryBindInfo*			pBufferBinds;
	deUint32									imageOpaqueBindCount;
	const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
	deUint32									imageBindCount;
	const VkSparseImageMemoryBindInfo*			pImageBinds;
	deUint32									signalSemaphoreCount;
	const VkSemaphore*							pSignalSemaphores;
};

struct VkSparseImageFormatProperties
{
	VkImageAspectFlags			aspectMask;
	VkExtent3D					imageGranularity;
	VkSparseImageFormatFlags	flags;
};

struct VkSparseImageMemoryRequirements
{
	VkSparseImageFormatProperties	formatProperties;
	deUint32						imageMipTailFirstLod;
	VkDeviceSize					imageMipTailSize;
	VkDeviceSize					imageMipTailOffset;
	VkDeviceSize					imageMipTailStride;
};

struct VkFenceCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkFenceCreateFlags	flags;
};

struct VkSemaphoreCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkSemaphoreCreateFlags	flags;
};

struct VkEventCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkEventCreateFlags	flags;
};

struct VkQueryPoolCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkQueryPoolCreateFlags			flags;
	VkQueryType						queryType;
	deUint32						queryCount;
	VkQueryPipelineStatisticFlags	pipelineStatistics;
};

struct VkBufferCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkBufferCreateFlags	flags;
	VkDeviceSize		size;
	VkBufferUsageFlags	usage;
	VkSharingMode		sharingMode;
	deUint32			queueFamilyIndexCount;
	const deUint32*		pQueueFamilyIndices;
};

struct VkBufferViewCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkBufferViewCreateFlags	flags;
	VkBuffer				buffer;
	VkFormat				format;
	VkDeviceSize			offset;
	VkDeviceSize			range;
};

struct VkImageCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImageCreateFlags		flags;
	VkImageType				imageType;
	VkFormat				format;
	VkExtent3D				extent;
	deUint32				mipLevels;
	deUint32				arrayLayers;
	VkSampleCountFlagBits	samples;
	VkImageTiling			tiling;
	VkImageUsageFlags		usage;
	VkSharingMode			sharingMode;
	deUint32				queueFamilyIndexCount;
	const deUint32*			pQueueFamilyIndices;
	VkImageLayout			initialLayout;
};

struct VkSubresourceLayout
{
	VkDeviceSize	offset;
	VkDeviceSize	size;
	VkDeviceSize	rowPitch;
	VkDeviceSize	arrayPitch;
	VkDeviceSize	depthPitch;
};

struct VkComponentMapping
{
	VkComponentSwizzle	r;
	VkComponentSwizzle	g;
	VkComponentSwizzle	b;
	VkComponentSwizzle	a;
};

struct VkImageViewCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImageViewCreateFlags	flags;
	VkImage					image;
	VkImageViewType			viewType;
	VkFormat				format;
	VkComponentMapping		components;
	VkImageSubresourceRange	subresourceRange;
};

struct VkShaderModuleCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkShaderModuleCreateFlags	flags;
	deUintptr					codeSize;
	const deUint32*				pCode;
};

struct VkPipelineCacheCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkPipelineCacheCreateFlags	flags;
	deUintptr					initialDataSize;
	const void*					pInitialData;
};

struct VkSpecializationMapEntry
{
	deUint32	constantID;
	deUint32	offset;
	deUintptr	size;
};

struct VkSpecializationInfo
{
	deUint32						mapEntryCount;
	const VkSpecializationMapEntry*	pMapEntries;
	deUintptr						dataSize;
	const void*						pData;
};

struct VkPipelineShaderStageCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkPipelineShaderStageCreateFlags	flags;
	VkShaderStageFlagBits				stage;
	VkShaderModule						module;
	const char*							pName;
	const VkSpecializationInfo*			pSpecializationInfo;
};

struct VkComputePipelineCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkPipelineCreateFlags			flags;
	VkPipelineShaderStageCreateInfo	stage;
	VkPipelineLayout				layout;
	VkPipeline						basePipelineHandle;
	deInt32							basePipelineIndex;
};

struct VkVertexInputBindingDescription
{
	deUint32			binding;
	deUint32			stride;
	VkVertexInputRate	inputRate;
};

struct VkVertexInputAttributeDescription
{
	deUint32	location;
	deUint32	binding;
	VkFormat	format;
	deUint32	offset;
};

struct VkPipelineVertexInputStateCreateInfo
{
	VkStructureType								sType;
	const void*									pNext;
	VkPipelineVertexInputStateCreateFlags		flags;
	deUint32									vertexBindingDescriptionCount;
	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
	deUint32									vertexAttributeDescriptionCount;
	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
};

struct VkPipelineInputAssemblyStateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineInputAssemblyStateCreateFlags	flags;
	VkPrimitiveTopology						topology;
	VkBool32								primitiveRestartEnable;
};

struct VkPipelineTessellationStateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineTessellationStateCreateFlags	flags;
	deUint32								patchControlPoints;
};

struct VkViewport
{
	float	x;
	float	y;
	float	width;
	float	height;
	float	minDepth;
	float	maxDepth;
};

struct VkPipelineViewportStateCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkPipelineViewportStateCreateFlags	flags;
	deUint32							viewportCount;
	const VkViewport*					pViewports;
	deUint32							scissorCount;
	const VkRect2D*						pScissors;
};

struct VkPipelineRasterizationStateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineRasterizationStateCreateFlags	flags;
	VkBool32								depthClampEnable;
	VkBool32								rasterizerDiscardEnable;
	VkPolygonMode							polygonMode;
	VkCullModeFlags							cullMode;
	VkFrontFace								frontFace;
	VkBool32								depthBiasEnable;
	float									depthBiasConstantFactor;
	float									depthBiasClamp;
	float									depthBiasSlopeFactor;
	float									lineWidth;
};

struct VkPipelineMultisampleStateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineMultisampleStateCreateFlags	flags;
	VkSampleCountFlagBits					rasterizationSamples;
	VkBool32								sampleShadingEnable;
	float									minSampleShading;
	const VkSampleMask*						pSampleMask;
	VkBool32								alphaToCoverageEnable;
	VkBool32								alphaToOneEnable;
};

struct VkStencilOpState
{
	VkStencilOp	failOp;
	VkStencilOp	passOp;
	VkStencilOp	depthFailOp;
	VkCompareOp	compareOp;
	deUint32	compareMask;
	deUint32	writeMask;
	deUint32	reference;
};

struct VkPipelineDepthStencilStateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineDepthStencilStateCreateFlags	flags;
	VkBool32								depthTestEnable;
	VkBool32								depthWriteEnable;
	VkCompareOp								depthCompareOp;
	VkBool32								depthBoundsTestEnable;
	VkBool32								stencilTestEnable;
	VkStencilOpState						front;
	VkStencilOpState						back;
	float									minDepthBounds;
	float									maxDepthBounds;
};

struct VkPipelineColorBlendAttachmentState
{
	VkBool32				blendEnable;
	VkBlendFactor			srcColorBlendFactor;
	VkBlendFactor			dstColorBlendFactor;
	VkBlendOp				colorBlendOp;
	VkBlendFactor			srcAlphaBlendFactor;
	VkBlendFactor			dstAlphaBlendFactor;
	VkBlendOp				alphaBlendOp;
	VkColorComponentFlags	colorWriteMask;
};

struct VkPipelineColorBlendStateCreateInfo
{
	VkStructureType								sType;
	const void*									pNext;
	VkPipelineColorBlendStateCreateFlags		flags;
	VkBool32									logicOpEnable;
	VkLogicOp									logicOp;
	deUint32									attachmentCount;
	const VkPipelineColorBlendAttachmentState*	pAttachments;
	float										blendConstants[4];
};

struct VkPipelineDynamicStateCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkPipelineDynamicStateCreateFlags	flags;
	deUint32							dynamicStateCount;
	const VkDynamicState*				pDynamicStates;
};

struct VkGraphicsPipelineCreateInfo
{
	VkStructureType									sType;
	const void*										pNext;
	VkPipelineCreateFlags							flags;
	deUint32										stageCount;
	const VkPipelineShaderStageCreateInfo*			pStages;
	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
	const VkPipelineViewportStateCreateInfo*		pViewportState;
	const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
	VkPipelineLayout								layout;
	VkRenderPass									renderPass;
	deUint32										subpass;
	VkPipeline										basePipelineHandle;
	deInt32											basePipelineIndex;
};

struct VkPushConstantRange
{
	VkShaderStageFlags	stageFlags;
	deUint32			offset;
	deUint32			size;
};

struct VkPipelineLayoutCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkPipelineLayoutCreateFlags		flags;
	deUint32						setLayoutCount;
	const VkDescriptorSetLayout*	pSetLayouts;
	deUint32						pushConstantRangeCount;
	const VkPushConstantRange*		pPushConstantRanges;
};

struct VkSamplerCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkSamplerCreateFlags	flags;
	VkFilter				magFilter;
	VkFilter				minFilter;
	VkSamplerMipmapMode		mipmapMode;
	VkSamplerAddressMode	addressModeU;
	VkSamplerAddressMode	addressModeV;
	VkSamplerAddressMode	addressModeW;
	float					mipLodBias;
	VkBool32				anisotropyEnable;
	float					maxAnisotropy;
	VkBool32				compareEnable;
	VkCompareOp				compareOp;
	float					minLod;
	float					maxLod;
	VkBorderColor			borderColor;
	VkBool32				unnormalizedCoordinates;
};

struct VkCopyDescriptorSet
{
	VkStructureType	sType;
	const void*		pNext;
	VkDescriptorSet	srcSet;
	deUint32		srcBinding;
	deUint32		srcArrayElement;
	VkDescriptorSet	dstSet;
	deUint32		dstBinding;
	deUint32		dstArrayElement;
	deUint32		descriptorCount;
};

struct VkDescriptorBufferInfo
{
	VkBuffer		buffer;
	VkDeviceSize	offset;
	VkDeviceSize	range;
};

struct VkDescriptorImageInfo
{
	VkSampler		sampler;
	VkImageView		imageView;
	VkImageLayout	imageLayout;
};

struct VkDescriptorPoolSize
{
	VkDescriptorType	type;
	deUint32			descriptorCount;
};

struct VkDescriptorPoolCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkDescriptorPoolCreateFlags	flags;
	deUint32					maxSets;
	deUint32					poolSizeCount;
	const VkDescriptorPoolSize*	pPoolSizes;
};

struct VkDescriptorSetAllocateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkDescriptorPool				descriptorPool;
	deUint32						descriptorSetCount;
	const VkDescriptorSetLayout*	pSetLayouts;
};

struct VkDescriptorSetLayoutBinding
{
	deUint32			binding;
	VkDescriptorType	descriptorType;
	deUint32			descriptorCount;
	VkShaderStageFlags	stageFlags;
	const VkSampler*	pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkDescriptorSetLayoutCreateFlags	flags;
	deUint32							bindingCount;
	const VkDescriptorSetLayoutBinding*	pBindings;
};

struct VkWriteDescriptorSet
{
	VkStructureType					sType;
	const void*						pNext;
	VkDescriptorSet					dstSet;
	deUint32						dstBinding;
	deUint32						dstArrayElement;
	deUint32						descriptorCount;
	VkDescriptorType				descriptorType;
	const VkDescriptorImageInfo*	pImageInfo;
	const VkDescriptorBufferInfo*	pBufferInfo;
	const VkBufferView*				pTexelBufferView;
};

struct VkAttachmentDescription
{
	VkAttachmentDescriptionFlags	flags;
	VkFormat						format;
	VkSampleCountFlagBits			samples;
	VkAttachmentLoadOp				loadOp;
	VkAttachmentStoreOp				storeOp;
	VkAttachmentLoadOp				stencilLoadOp;
	VkAttachmentStoreOp				stencilStoreOp;
	VkImageLayout					initialLayout;
	VkImageLayout					finalLayout;
};

struct VkAttachmentReference
{
	deUint32		attachment;
	VkImageLayout	layout;
};

struct VkFramebufferCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkFramebufferCreateFlags	flags;
	VkRenderPass				renderPass;
	deUint32					attachmentCount;
	const VkImageView*			pAttachments;
	deUint32					width;
	deUint32					height;
	deUint32					layers;
};

struct VkSubpassDescription
{
	VkSubpassDescriptionFlags		flags;
	VkPipelineBindPoint				pipelineBindPoint;
	deUint32						inputAttachmentCount;
	const VkAttachmentReference*	pInputAttachments;
	deUint32						colorAttachmentCount;
	const VkAttachmentReference*	pColorAttachments;
	const VkAttachmentReference*	pResolveAttachments;
	const VkAttachmentReference*	pDepthStencilAttachment;
	deUint32						preserveAttachmentCount;
	const deUint32*					pPreserveAttachments;
};

struct VkSubpassDependency
{
	deUint32				srcSubpass;
	deUint32				dstSubpass;
	VkPipelineStageFlags	srcStageMask;
	VkPipelineStageFlags	dstStageMask;
	VkAccessFlags			srcAccessMask;
	VkAccessFlags			dstAccessMask;
	VkDependencyFlags		dependencyFlags;
};

struct VkRenderPassCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkRenderPassCreateFlags			flags;
	deUint32						attachmentCount;
	const VkAttachmentDescription*	pAttachments;
	deUint32						subpassCount;
	const VkSubpassDescription*		pSubpasses;
	deUint32						dependencyCount;
	const VkSubpassDependency*		pDependencies;
};

struct VkCommandPoolCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkCommandPoolCreateFlags	flags;
	deUint32					queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkCommandPool			commandPool;
	VkCommandBufferLevel	level;
	deUint32				commandBufferCount;
};

struct VkCommandBufferInheritanceInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkRenderPass					renderPass;
	deUint32						subpass;
	VkFramebuffer					framebuffer;
	VkBool32						occlusionQueryEnable;
	VkQueryControlFlags				queryFlags;
	VkQueryPipelineStatisticFlags	pipelineStatistics;
};

struct VkCommandBufferBeginInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkCommandBufferUsageFlags				flags;
	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
};

struct VkBufferCopy
{
	VkDeviceSize	srcOffset;
	VkDeviceSize	dstOffset;
	VkDeviceSize	size;
};

struct VkImageSubresourceLayers
{
	VkImageAspectFlags	aspectMask;
	deUint32			mipLevel;
	deUint32			baseArrayLayer;
	deUint32			layerCount;
};

struct VkBufferImageCopy
{
	VkDeviceSize				bufferOffset;
	deUint32					bufferRowLength;
	deUint32					bufferImageHeight;
	VkImageSubresourceLayers	imageSubresource;
	VkOffset3D					imageOffset;
	VkExtent3D					imageExtent;
};

union VkClearColorValue
{
	float		float32[4];
	deInt32		int32[4];
	deUint32	uint32[4];
};

struct VkClearDepthStencilValue
{
	float		depth;
	deUint32	stencil;
};

union VkClearValue
{
	VkClearColorValue			color;
	VkClearDepthStencilValue	depthStencil;
};

struct VkClearAttachment
{
	VkImageAspectFlags	aspectMask;
	deUint32			colorAttachment;
	VkClearValue		clearValue;
};

struct VkClearRect
{
	VkRect2D	rect;
	deUint32	baseArrayLayer;
	deUint32	layerCount;
};

struct VkImageBlit
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffsets[2];
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffsets[2];
};

struct VkImageCopy
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkImageResolve
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkRenderPassBeginInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkRenderPass		renderPass;
	VkFramebuffer		framebuffer;
	VkRect2D			renderArea;
	deUint32			clearValueCount;
	const VkClearValue*	pClearValues;
};

struct VkPhysicalDeviceSubgroupProperties
{
	VkStructureType			sType;
	void*					pNext;
	deUint32				subgroupSize;
	VkShaderStageFlags		supportedStages;
	VkSubgroupFeatureFlags	supportedOperations;
	VkBool32				quadOperationsInAllStages;
};

struct VkBindBufferMemoryInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkBuffer		buffer;
	VkDeviceMemory	memory;
	VkDeviceSize	memoryOffset;
};

struct VkBindImageMemoryInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkImage			image;
	VkDeviceMemory	memory;
	VkDeviceSize	memoryOffset;
};

struct VkPhysicalDevice16BitStorageFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		storageBuffer16BitAccess;
	VkBool32		uniformAndStorageBuffer16BitAccess;
	VkBool32		storagePushConstant16;
	VkBool32		storageInputOutput16;
};

struct VkMemoryDedicatedRequirements
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		prefersDedicatedAllocation;
	VkBool32		requiresDedicatedAllocation;
};

struct VkMemoryDedicatedAllocateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkImage			image;
	VkBuffer		buffer;
};

struct VkMemoryAllocateFlagsInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkMemoryAllocateFlags	flags;
	deUint32				deviceMask;
};

struct VkDeviceGroupRenderPassBeginInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		deviceMask;
	deUint32		deviceRenderAreaCount;
	const VkRect2D*	pDeviceRenderAreas;
};

struct VkDeviceGroupCommandBufferBeginInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		deviceMask;
};

struct VkDeviceGroupSubmitInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		waitSemaphoreCount;
	const deUint32*	pWaitSemaphoreDeviceIndices;
	deUint32		commandBufferCount;
	const deUint32*	pCommandBufferDeviceMasks;
	deUint32		signalSemaphoreCount;
	const deUint32*	pSignalSemaphoreDeviceIndices;
};

struct VkDeviceGroupBindSparseInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		resourceDeviceIndex;
	deUint32		memoryDeviceIndex;
};

struct VkBindBufferMemoryDeviceGroupInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		deviceIndexCount;
	const deUint32*	pDeviceIndices;
};

struct VkBindImageMemoryDeviceGroupInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		deviceIndexCount;
	const deUint32*	pDeviceIndices;
	deUint32		splitInstanceBindRegionCount;
	const VkRect2D*	pSplitInstanceBindRegions;
};

struct VkPhysicalDeviceGroupProperties
{
	VkStructureType		sType;
	void*				pNext;
	deUint32			physicalDeviceCount;
	VkPhysicalDevice	physicalDevices[VK_MAX_DEVICE_GROUP_SIZE];
	VkBool32			subsetAllocation;
};

struct VkDeviceGroupDeviceCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				physicalDeviceCount;
	const VkPhysicalDevice*	pPhysicalDevices;
};

struct VkBufferMemoryRequirementsInfo2
{
	VkStructureType	sType;
	const void*		pNext;
	VkBuffer		buffer;
};

struct VkImageMemoryRequirementsInfo2
{
	VkStructureType	sType;
	const void*		pNext;
	VkImage			image;
};

struct VkImageSparseMemoryRequirementsInfo2
{
	VkStructureType	sType;
	const void*		pNext;
	VkImage			image;
};

struct VkMemoryRequirements2
{
	VkStructureType			sType;
	void*					pNext;
	VkMemoryRequirements	memoryRequirements;
};

struct VkSparseImageMemoryRequirements2
{
	VkStructureType					sType;
	void*							pNext;
	VkSparseImageMemoryRequirements	memoryRequirements;
};

struct VkPhysicalDeviceFeatures2
{
	VkStructureType				sType;
	void*						pNext;
	VkPhysicalDeviceFeatures	features;
};

struct VkPhysicalDeviceProperties2
{
	VkStructureType				sType;
	void*						pNext;
	VkPhysicalDeviceProperties	properties;
};

struct VkFormatProperties2
{
	VkStructureType		sType;
	void*				pNext;
	VkFormatProperties	formatProperties;
};

struct VkImageFormatProperties2
{
	VkStructureType			sType;
	void*					pNext;
	VkImageFormatProperties	imageFormatProperties;
};

struct VkPhysicalDeviceImageFormatInfo2
{
	VkStructureType		sType;
	const void*			pNext;
	VkFormat			format;
	VkImageType			type;
	VkImageTiling		tiling;
	VkImageUsageFlags	usage;
	VkImageCreateFlags	flags;
};

struct VkQueueFamilyProperties2
{
	VkStructureType			sType;
	void*					pNext;
	VkQueueFamilyProperties	queueFamilyProperties;
};

struct VkPhysicalDeviceMemoryProperties2
{
	VkStructureType						sType;
	void*								pNext;
	VkPhysicalDeviceMemoryProperties	memoryProperties;
};

struct VkSparseImageFormatProperties2
{
	VkStructureType					sType;
	void*							pNext;
	VkSparseImageFormatProperties	properties;
};

struct VkPhysicalDeviceSparseImageFormatInfo2
{
	VkStructureType			sType;
	const void*				pNext;
	VkFormat				format;
	VkImageType				type;
	VkSampleCountFlagBits	samples;
	VkImageUsageFlags		usage;
	VkImageTiling			tiling;
};

struct VkPhysicalDevicePointClippingProperties
{
	VkStructureType			sType;
	void*					pNext;
	VkPointClippingBehavior	pointClippingBehavior;
};

struct VkInputAttachmentAspectReference
{
	deUint32			subpass;
	deUint32			inputAttachmentIndex;
	VkImageAspectFlags	aspectMask;
};

struct VkRenderPassInputAttachmentAspectCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								aspectReferenceCount;
	const VkInputAttachmentAspectReference*	pAspectReferences;
};

struct VkImageViewUsageCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkImageUsageFlags	usage;
};

struct VkPipelineTessellationDomainOriginStateCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkTessellationDomainOrigin	domainOrigin;
};

struct VkRenderPassMultiviewCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		subpassCount;
	const deUint32*	pViewMasks;
	deUint32		dependencyCount;
	const deInt32*	pViewOffsets;
	deUint32		correlationMaskCount;
	const deUint32*	pCorrelationMasks;
};

struct VkPhysicalDeviceMultiviewFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		multiview;
	VkBool32		multiviewGeometryShader;
	VkBool32		multiviewTessellationShader;
};

struct VkPhysicalDeviceMultiviewProperties
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxMultiviewViewCount;
	deUint32		maxMultiviewInstanceIndex;
};

struct VkPhysicalDeviceVariablePointersFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		variablePointersStorageBuffer;
	VkBool32		variablePointers;
};

struct VkPhysicalDeviceProtectedMemoryFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		protectedMemory;
};

struct VkPhysicalDeviceProtectedMemoryProperties
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		protectedNoFault;
};

struct VkDeviceQueueInfo2
{
	VkStructureType				sType;
	const void*					pNext;
	VkDeviceQueueCreateFlags	flags;
	deUint32					queueFamilyIndex;
	deUint32					queueIndex;
};

struct VkProtectedSubmitInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		protectedSubmit;
};

struct VkSamplerYcbcrConversionCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkFormat						format;
	VkSamplerYcbcrModelConversion	ycbcrModel;
	VkSamplerYcbcrRange				ycbcrRange;
	VkComponentMapping				components;
	VkChromaLocation				xChromaOffset;
	VkChromaLocation				yChromaOffset;
	VkFilter						chromaFilter;
	VkBool32						forceExplicitReconstruction;
};

struct VkSamplerYcbcrConversionInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkSamplerYcbcrConversion	conversion;
};

struct VkBindImagePlaneMemoryInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImageAspectFlagBits	planeAspect;
};

struct VkImagePlaneMemoryRequirementsInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImageAspectFlagBits	planeAspect;
};

struct VkPhysicalDeviceSamplerYcbcrConversionFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		samplerYcbcrConversion;
};

struct VkSamplerYcbcrConversionImageFormatProperties
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		combinedImageSamplerDescriptorCount;
};

struct VkDescriptorUpdateTemplateEntry
{
	deUint32			dstBinding;
	deUint32			dstArrayElement;
	deUint32			descriptorCount;
	VkDescriptorType	descriptorType;
	deUintptr			offset;
	deUintptr			stride;
};

struct VkDescriptorUpdateTemplateCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkDescriptorUpdateTemplateCreateFlags	flags;
	deUint32								descriptorUpdateEntryCount;
	const VkDescriptorUpdateTemplateEntry*	pDescriptorUpdateEntries;
	VkDescriptorUpdateTemplateType			templateType;
	VkDescriptorSetLayout					descriptorSetLayout;
	VkPipelineBindPoint						pipelineBindPoint;
	VkPipelineLayout						pipelineLayout;
	deUint32								set;
};

struct VkExternalMemoryProperties
{
	VkExternalMemoryFeatureFlags	externalMemoryFeatures;
	VkExternalMemoryHandleTypeFlags	exportFromImportedHandleTypes;
	VkExternalMemoryHandleTypeFlags	compatibleHandleTypes;
};

struct VkPhysicalDeviceExternalImageFormatInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagBits	handleType;
};

struct VkExternalImageFormatProperties
{
	VkStructureType				sType;
	void*						pNext;
	VkExternalMemoryProperties	externalMemoryProperties;
};

struct VkPhysicalDeviceExternalBufferInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkBufferCreateFlags					flags;
	VkBufferUsageFlags					usage;
	VkExternalMemoryHandleTypeFlagBits	handleType;
};

struct VkExternalBufferProperties
{
	VkStructureType				sType;
	void*						pNext;
	VkExternalMemoryProperties	externalMemoryProperties;
};

struct VkPhysicalDeviceIDProperties
{
	VkStructureType	sType;
	void*			pNext;
	deUint8			deviceUUID[VK_UUID_SIZE];
	deUint8			driverUUID[VK_UUID_SIZE];
	deUint8			deviceLUID[VK_LUID_SIZE];
	deUint32		deviceNodeMask;
	VkBool32		deviceLUIDValid;
};

struct VkExternalMemoryImageCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkExternalMemoryHandleTypeFlags	handleTypes;
};

struct VkExternalMemoryBufferCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkExternalMemoryHandleTypeFlags	handleTypes;
};

struct VkExportMemoryAllocateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkExternalMemoryHandleTypeFlags	handleTypes;
};

struct VkPhysicalDeviceExternalFenceInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalFenceHandleTypeFlagBits	handleType;
};

struct VkExternalFenceProperties
{
	VkStructureType					sType;
	void*							pNext;
	VkExternalFenceHandleTypeFlags	exportFromImportedHandleTypes;
	VkExternalFenceHandleTypeFlags	compatibleHandleTypes;
	VkExternalFenceFeatureFlags		externalFenceFeatures;
};

struct VkExportFenceCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkExternalFenceHandleTypeFlags	handleTypes;
};

struct VkExportSemaphoreCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalSemaphoreHandleTypeFlags	handleTypes;
};

struct VkPhysicalDeviceExternalSemaphoreInfo
{
	VkStructureType							sType;
	const void*								pNext;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
};

struct VkExternalSemaphoreProperties
{
	VkStructureType						sType;
	void*								pNext;
	VkExternalSemaphoreHandleTypeFlags	exportFromImportedHandleTypes;
	VkExternalSemaphoreHandleTypeFlags	compatibleHandleTypes;
	VkExternalSemaphoreFeatureFlags		externalSemaphoreFeatures;
};

struct VkPhysicalDeviceMaintenance3Properties
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxPerSetDescriptors;
	VkDeviceSize	maxMemoryAllocationSize;
};

struct VkDescriptorSetLayoutSupport
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		supported;
};

struct VkPhysicalDeviceShaderDrawParametersFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderDrawParameters;
};

struct VkPhysicalDeviceVulkan11Features
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		storageBuffer16BitAccess;
	VkBool32		uniformAndStorageBuffer16BitAccess;
	VkBool32		storagePushConstant16;
	VkBool32		storageInputOutput16;
	VkBool32		multiview;
	VkBool32		multiviewGeometryShader;
	VkBool32		multiviewTessellationShader;
	VkBool32		variablePointersStorageBuffer;
	VkBool32		variablePointers;
	VkBool32		protectedMemory;
	VkBool32		samplerYcbcrConversion;
	VkBool32		shaderDrawParameters;
};

struct VkPhysicalDeviceVulkan11Properties
{
	VkStructureType			sType;
	void*					pNext;
	deUint8					deviceUUID[VK_UUID_SIZE];
	deUint8					driverUUID[VK_UUID_SIZE];
	deUint8					deviceLUID[VK_LUID_SIZE];
	deUint32				deviceNodeMask;
	VkBool32				deviceLUIDValid;
	deUint32				subgroupSize;
	VkShaderStageFlags		subgroupSupportedStages;
	VkSubgroupFeatureFlags	subgroupSupportedOperations;
	VkBool32				subgroupQuadOperationsInAllStages;
	VkPointClippingBehavior	pointClippingBehavior;
	deUint32				maxMultiviewViewCount;
	deUint32				maxMultiviewInstanceIndex;
	VkBool32				protectedNoFault;
	deUint32				maxPerSetDescriptors;
	VkDeviceSize			maxMemoryAllocationSize;
};

struct VkPhysicalDeviceVulkan12Features
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		samplerMirrorClampToEdge;
	VkBool32		drawIndirectCount;
	VkBool32		storageBuffer8BitAccess;
	VkBool32		uniformAndStorageBuffer8BitAccess;
	VkBool32		storagePushConstant8;
	VkBool32		shaderBufferInt64Atomics;
	VkBool32		shaderSharedInt64Atomics;
	VkBool32		shaderFloat16;
	VkBool32		shaderInt8;
	VkBool32		descriptorIndexing;
	VkBool32		shaderInputAttachmentArrayDynamicIndexing;
	VkBool32		shaderUniformTexelBufferArrayDynamicIndexing;
	VkBool32		shaderStorageTexelBufferArrayDynamicIndexing;
	VkBool32		shaderUniformBufferArrayNonUniformIndexing;
	VkBool32		shaderSampledImageArrayNonUniformIndexing;
	VkBool32		shaderStorageBufferArrayNonUniformIndexing;
	VkBool32		shaderStorageImageArrayNonUniformIndexing;
	VkBool32		shaderInputAttachmentArrayNonUniformIndexing;
	VkBool32		shaderUniformTexelBufferArrayNonUniformIndexing;
	VkBool32		shaderStorageTexelBufferArrayNonUniformIndexing;
	VkBool32		descriptorBindingUniformBufferUpdateAfterBind;
	VkBool32		descriptorBindingSampledImageUpdateAfterBind;
	VkBool32		descriptorBindingStorageImageUpdateAfterBind;
	VkBool32		descriptorBindingStorageBufferUpdateAfterBind;
	VkBool32		descriptorBindingUniformTexelBufferUpdateAfterBind;
	VkBool32		descriptorBindingStorageTexelBufferUpdateAfterBind;
	VkBool32		descriptorBindingUpdateUnusedWhilePending;
	VkBool32		descriptorBindingPartiallyBound;
	VkBool32		descriptorBindingVariableDescriptorCount;
	VkBool32		runtimeDescriptorArray;
	VkBool32		samplerFilterMinmax;
	VkBool32		scalarBlockLayout;
	VkBool32		imagelessFramebuffer;
	VkBool32		uniformBufferStandardLayout;
	VkBool32		shaderSubgroupExtendedTypes;
	VkBool32		separateDepthStencilLayouts;
	VkBool32		hostQueryReset;
	VkBool32		timelineSemaphore;
	VkBool32		bufferDeviceAddress;
	VkBool32		bufferDeviceAddressCaptureReplay;
	VkBool32		bufferDeviceAddressMultiDevice;
	VkBool32		vulkanMemoryModel;
	VkBool32		vulkanMemoryModelDeviceScope;
	VkBool32		vulkanMemoryModelAvailabilityVisibilityChains;
	VkBool32		shaderOutputViewportIndex;
	VkBool32		shaderOutputLayer;
	VkBool32		subgroupBroadcastDynamicId;
};

struct VkConformanceVersion
{
	deUint8	major;
	deUint8	minor;
	deUint8	subminor;
	deUint8	patch;
};

struct VkPhysicalDeviceVulkan12Properties
{
	VkStructureType						sType;
	void*								pNext;
	VkDriverId							driverID;
	char								driverName[VK_MAX_DRIVER_NAME_SIZE];
	char								driverInfo[VK_MAX_DRIVER_INFO_SIZE];
	VkConformanceVersion				conformanceVersion;
	VkShaderFloatControlsIndependence	denormBehaviorIndependence;
	VkShaderFloatControlsIndependence	roundingModeIndependence;
	VkBool32							shaderSignedZeroInfNanPreserveFloat16;
	VkBool32							shaderSignedZeroInfNanPreserveFloat32;
	VkBool32							shaderSignedZeroInfNanPreserveFloat64;
	VkBool32							shaderDenormPreserveFloat16;
	VkBool32							shaderDenormPreserveFloat32;
	VkBool32							shaderDenormPreserveFloat64;
	VkBool32							shaderDenormFlushToZeroFloat16;
	VkBool32							shaderDenormFlushToZeroFloat32;
	VkBool32							shaderDenormFlushToZeroFloat64;
	VkBool32							shaderRoundingModeRTEFloat16;
	VkBool32							shaderRoundingModeRTEFloat32;
	VkBool32							shaderRoundingModeRTEFloat64;
	VkBool32							shaderRoundingModeRTZFloat16;
	VkBool32							shaderRoundingModeRTZFloat32;
	VkBool32							shaderRoundingModeRTZFloat64;
	deUint32							maxUpdateAfterBindDescriptorsInAllPools;
	VkBool32							shaderUniformBufferArrayNonUniformIndexingNative;
	VkBool32							shaderSampledImageArrayNonUniformIndexingNative;
	VkBool32							shaderStorageBufferArrayNonUniformIndexingNative;
	VkBool32							shaderStorageImageArrayNonUniformIndexingNative;
	VkBool32							shaderInputAttachmentArrayNonUniformIndexingNative;
	VkBool32							robustBufferAccessUpdateAfterBind;
	VkBool32							quadDivergentImplicitLod;
	deUint32							maxPerStageDescriptorUpdateAfterBindSamplers;
	deUint32							maxPerStageDescriptorUpdateAfterBindUniformBuffers;
	deUint32							maxPerStageDescriptorUpdateAfterBindStorageBuffers;
	deUint32							maxPerStageDescriptorUpdateAfterBindSampledImages;
	deUint32							maxPerStageDescriptorUpdateAfterBindStorageImages;
	deUint32							maxPerStageDescriptorUpdateAfterBindInputAttachments;
	deUint32							maxPerStageUpdateAfterBindResources;
	deUint32							maxDescriptorSetUpdateAfterBindSamplers;
	deUint32							maxDescriptorSetUpdateAfterBindUniformBuffers;
	deUint32							maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
	deUint32							maxDescriptorSetUpdateAfterBindStorageBuffers;
	deUint32							maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
	deUint32							maxDescriptorSetUpdateAfterBindSampledImages;
	deUint32							maxDescriptorSetUpdateAfterBindStorageImages;
	deUint32							maxDescriptorSetUpdateAfterBindInputAttachments;
	VkResolveModeFlags					supportedDepthResolveModes;
	VkResolveModeFlags					supportedStencilResolveModes;
	VkBool32							independentResolveNone;
	VkBool32							independentResolve;
	VkBool32							filterMinmaxSingleComponentFormats;
	VkBool32							filterMinmaxImageComponentMapping;
	deUint64							maxTimelineSemaphoreValueDifference;
	VkSampleCountFlags					framebufferIntegerColorSampleCounts;
};

struct VkImageFormatListCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		viewFormatCount;
	const VkFormat*	pViewFormats;
};

struct VkAttachmentDescription2
{
	VkStructureType					sType;
	const void*						pNext;
	VkAttachmentDescriptionFlags	flags;
	VkFormat						format;
	VkSampleCountFlagBits			samples;
	VkAttachmentLoadOp				loadOp;
	VkAttachmentStoreOp				storeOp;
	VkAttachmentLoadOp				stencilLoadOp;
	VkAttachmentStoreOp				stencilStoreOp;
	VkImageLayout					initialLayout;
	VkImageLayout					finalLayout;
};

struct VkAttachmentReference2
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			attachment;
	VkImageLayout		layout;
	VkImageAspectFlags	aspectMask;
};

struct VkSubpassDescription2
{
	VkStructureType					sType;
	const void*						pNext;
	VkSubpassDescriptionFlags		flags;
	VkPipelineBindPoint				pipelineBindPoint;
	deUint32						viewMask;
	deUint32						inputAttachmentCount;
	const VkAttachmentReference2*	pInputAttachments;
	deUint32						colorAttachmentCount;
	const VkAttachmentReference2*	pColorAttachments;
	const VkAttachmentReference2*	pResolveAttachments;
	const VkAttachmentReference2*	pDepthStencilAttachment;
	deUint32						preserveAttachmentCount;
	const deUint32*					pPreserveAttachments;
};

struct VkSubpassDependency2
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				srcSubpass;
	deUint32				dstSubpass;
	VkPipelineStageFlags	srcStageMask;
	VkPipelineStageFlags	dstStageMask;
	VkAccessFlags			srcAccessMask;
	VkAccessFlags			dstAccessMask;
	VkDependencyFlags		dependencyFlags;
	deInt32					viewOffset;
};

struct VkRenderPassCreateInfo2
{
	VkStructureType					sType;
	const void*						pNext;
	VkRenderPassCreateFlags			flags;
	deUint32						attachmentCount;
	const VkAttachmentDescription2*	pAttachments;
	deUint32						subpassCount;
	const VkSubpassDescription2*	pSubpasses;
	deUint32						dependencyCount;
	const VkSubpassDependency2*		pDependencies;
	deUint32						correlatedViewMaskCount;
	const deUint32*					pCorrelatedViewMasks;
};

struct VkSubpassBeginInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkSubpassContents	contents;
};

struct VkSubpassEndInfo
{
	VkStructureType	sType;
	const void*		pNext;
};

struct VkPhysicalDevice8BitStorageFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		storageBuffer8BitAccess;
	VkBool32		uniformAndStorageBuffer8BitAccess;
	VkBool32		storagePushConstant8;
};

struct VkPhysicalDeviceDriverProperties
{
	VkStructureType			sType;
	void*					pNext;
	VkDriverId				driverID;
	char					driverName[VK_MAX_DRIVER_NAME_SIZE];
	char					driverInfo[VK_MAX_DRIVER_INFO_SIZE];
	VkConformanceVersion	conformanceVersion;
};

struct VkPhysicalDeviceShaderAtomicInt64Features
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderBufferInt64Atomics;
	VkBool32		shaderSharedInt64Atomics;
};

struct VkPhysicalDeviceShaderFloat16Int8Features
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderFloat16;
	VkBool32		shaderInt8;
};

struct VkPhysicalDeviceFloatControlsProperties
{
	VkStructureType						sType;
	void*								pNext;
	VkShaderFloatControlsIndependence	denormBehaviorIndependence;
	VkShaderFloatControlsIndependence	roundingModeIndependence;
	VkBool32							shaderSignedZeroInfNanPreserveFloat16;
	VkBool32							shaderSignedZeroInfNanPreserveFloat32;
	VkBool32							shaderSignedZeroInfNanPreserveFloat64;
	VkBool32							shaderDenormPreserveFloat16;
	VkBool32							shaderDenormPreserveFloat32;
	VkBool32							shaderDenormPreserveFloat64;
	VkBool32							shaderDenormFlushToZeroFloat16;
	VkBool32							shaderDenormFlushToZeroFloat32;
	VkBool32							shaderDenormFlushToZeroFloat64;
	VkBool32							shaderRoundingModeRTEFloat16;
	VkBool32							shaderRoundingModeRTEFloat32;
	VkBool32							shaderRoundingModeRTEFloat64;
	VkBool32							shaderRoundingModeRTZFloat16;
	VkBool32							shaderRoundingModeRTZFloat32;
	VkBool32							shaderRoundingModeRTZFloat64;
};

struct VkDescriptorSetLayoutBindingFlagsCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						bindingCount;
	const VkDescriptorBindingFlags*	pBindingFlags;
};

struct VkPhysicalDeviceDescriptorIndexingFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderInputAttachmentArrayDynamicIndexing;
	VkBool32		shaderUniformTexelBufferArrayDynamicIndexing;
	VkBool32		shaderStorageTexelBufferArrayDynamicIndexing;
	VkBool32		shaderUniformBufferArrayNonUniformIndexing;
	VkBool32		shaderSampledImageArrayNonUniformIndexing;
	VkBool32		shaderStorageBufferArrayNonUniformIndexing;
	VkBool32		shaderStorageImageArrayNonUniformIndexing;
	VkBool32		shaderInputAttachmentArrayNonUniformIndexing;
	VkBool32		shaderUniformTexelBufferArrayNonUniformIndexing;
	VkBool32		shaderStorageTexelBufferArrayNonUniformIndexing;
	VkBool32		descriptorBindingUniformBufferUpdateAfterBind;
	VkBool32		descriptorBindingSampledImageUpdateAfterBind;
	VkBool32		descriptorBindingStorageImageUpdateAfterBind;
	VkBool32		descriptorBindingStorageBufferUpdateAfterBind;
	VkBool32		descriptorBindingUniformTexelBufferUpdateAfterBind;
	VkBool32		descriptorBindingStorageTexelBufferUpdateAfterBind;
	VkBool32		descriptorBindingUpdateUnusedWhilePending;
	VkBool32		descriptorBindingPartiallyBound;
	VkBool32		descriptorBindingVariableDescriptorCount;
	VkBool32		runtimeDescriptorArray;
};

struct VkPhysicalDeviceDescriptorIndexingProperties
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxUpdateAfterBindDescriptorsInAllPools;
	VkBool32		shaderUniformBufferArrayNonUniformIndexingNative;
	VkBool32		shaderSampledImageArrayNonUniformIndexingNative;
	VkBool32		shaderStorageBufferArrayNonUniformIndexingNative;
	VkBool32		shaderStorageImageArrayNonUniformIndexingNative;
	VkBool32		shaderInputAttachmentArrayNonUniformIndexingNative;
	VkBool32		robustBufferAccessUpdateAfterBind;
	VkBool32		quadDivergentImplicitLod;
	deUint32		maxPerStageDescriptorUpdateAfterBindSamplers;
	deUint32		maxPerStageDescriptorUpdateAfterBindUniformBuffers;
	deUint32		maxPerStageDescriptorUpdateAfterBindStorageBuffers;
	deUint32		maxPerStageDescriptorUpdateAfterBindSampledImages;
	deUint32		maxPerStageDescriptorUpdateAfterBindStorageImages;
	deUint32		maxPerStageDescriptorUpdateAfterBindInputAttachments;
	deUint32		maxPerStageUpdateAfterBindResources;
	deUint32		maxDescriptorSetUpdateAfterBindSamplers;
	deUint32		maxDescriptorSetUpdateAfterBindUniformBuffers;
	deUint32		maxDescriptorSetUpdateAfterBindUniformBuffersDynamic;
	deUint32		maxDescriptorSetUpdateAfterBindStorageBuffers;
	deUint32		maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
	deUint32		maxDescriptorSetUpdateAfterBindSampledImages;
	deUint32		maxDescriptorSetUpdateAfterBindStorageImages;
	deUint32		maxDescriptorSetUpdateAfterBindInputAttachments;
};

struct VkDescriptorSetVariableDescriptorCountAllocateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		descriptorSetCount;
	const deUint32*	pDescriptorCounts;
};

struct VkDescriptorSetVariableDescriptorCountLayoutSupport
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxVariableDescriptorCount;
};

struct VkSubpassDescriptionDepthStencilResolve
{
	VkStructureType					sType;
	const void*						pNext;
	VkResolveModeFlagBits			depthResolveMode;
	VkResolveModeFlagBits			stencilResolveMode;
	const VkAttachmentReference2*	pDepthStencilResolveAttachment;
};

struct VkPhysicalDeviceDepthStencilResolveProperties
{
	VkStructureType		sType;
	void*				pNext;
	VkResolveModeFlags	supportedDepthResolveModes;
	VkResolveModeFlags	supportedStencilResolveModes;
	VkBool32			independentResolveNone;
	VkBool32			independentResolve;
};

struct VkPhysicalDeviceScalarBlockLayoutFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		scalarBlockLayout;
};

struct VkImageStencilUsageCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkImageUsageFlags	stencilUsage;
};

struct VkSamplerReductionModeCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkSamplerReductionMode	reductionMode;
};

struct VkPhysicalDeviceSamplerFilterMinmaxProperties
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		filterMinmaxSingleComponentFormats;
	VkBool32		filterMinmaxImageComponentMapping;
};

struct VkPhysicalDeviceVulkanMemoryModelFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		vulkanMemoryModel;
	VkBool32		vulkanMemoryModelDeviceScope;
	VkBool32		vulkanMemoryModelAvailabilityVisibilityChains;
};

struct VkPhysicalDeviceImagelessFramebufferFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		imagelessFramebuffer;
};

struct VkFramebufferAttachmentImageInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkImageCreateFlags	flags;
	VkImageUsageFlags	usage;
	deUint32			width;
	deUint32			height;
	deUint32			layerCount;
	deUint32			viewFormatCount;
	const VkFormat*		pViewFormats;
};

struct VkFramebufferAttachmentsCreateInfo
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								attachmentImageInfoCount;
	const VkFramebufferAttachmentImageInfo*	pAttachmentImageInfos;
};

struct VkRenderPassAttachmentBeginInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			attachmentCount;
	const VkImageView*	pAttachments;
};

struct VkPhysicalDeviceUniformBufferStandardLayoutFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		uniformBufferStandardLayout;
};

struct VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderSubgroupExtendedTypes;
};

struct VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		separateDepthStencilLayouts;
};

struct VkAttachmentReferenceStencilLayout
{
	VkStructureType	sType;
	void*			pNext;
	VkImageLayout	stencilLayout;
};

struct VkAttachmentDescriptionStencilLayout
{
	VkStructureType	sType;
	void*			pNext;
	VkImageLayout	stencilInitialLayout;
	VkImageLayout	stencilFinalLayout;
};

struct VkPhysicalDeviceHostQueryResetFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		hostQueryReset;
};

struct VkPhysicalDeviceTimelineSemaphoreFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		timelineSemaphore;
};

struct VkPhysicalDeviceTimelineSemaphoreProperties
{
	VkStructureType	sType;
	void*			pNext;
	deUint64		maxTimelineSemaphoreValueDifference;
};

struct VkSemaphoreTypeCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkSemaphoreType	semaphoreType;
	deUint64		initialValue;
};

struct VkTimelineSemaphoreSubmitInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		waitSemaphoreValueCount;
	const deUint64*	pWaitSemaphoreValues;
	deUint32		signalSemaphoreValueCount;
	const deUint64*	pSignalSemaphoreValues;
};

struct VkSemaphoreWaitInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkSemaphoreWaitFlags	flags;
	deUint32				semaphoreCount;
	const VkSemaphore*		pSemaphores;
	const deUint64*			pValues;
};

struct VkSemaphoreSignalInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkSemaphore		semaphore;
	deUint64		value;
};

struct VkPhysicalDeviceBufferDeviceAddressFeatures
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		bufferDeviceAddress;
	VkBool32		bufferDeviceAddressCaptureReplay;
	VkBool32		bufferDeviceAddressMultiDevice;
};

struct VkBufferDeviceAddressInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkBuffer		buffer;
};

struct VkBufferOpaqueCaptureAddressCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint64		opaqueCaptureAddress;
};

struct VkMemoryOpaqueCaptureAddressAllocateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint64		opaqueCaptureAddress;
};

struct VkDeviceMemoryOpaqueCaptureAddressInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	memory;
};

struct VkSurfaceCapabilitiesKHR
{
	deUint32						minImageCount;
	deUint32						maxImageCount;
	VkExtent2D						currentExtent;
	VkExtent2D						minImageExtent;
	VkExtent2D						maxImageExtent;
	deUint32						maxImageArrayLayers;
	VkSurfaceTransformFlagsKHR		supportedTransforms;
	VkSurfaceTransformFlagBitsKHR	currentTransform;
	VkCompositeAlphaFlagsKHR		supportedCompositeAlpha;
	VkImageUsageFlags				supportedUsageFlags;
};

struct VkSurfaceFormatKHR
{
	VkFormat		format;
	VkColorSpaceKHR	colorSpace;
};

struct VkSwapchainCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkSwapchainCreateFlagsKHR		flags;
	VkSurfaceKHR					surface;
	deUint32						minImageCount;
	VkFormat						imageFormat;
	VkColorSpaceKHR					imageColorSpace;
	VkExtent2D						imageExtent;
	deUint32						imageArrayLayers;
	VkImageUsageFlags				imageUsage;
	VkSharingMode					imageSharingMode;
	deUint32						queueFamilyIndexCount;
	const deUint32*					pQueueFamilyIndices;
	VkSurfaceTransformFlagBitsKHR	preTransform;
	VkCompositeAlphaFlagBitsKHR		compositeAlpha;
	VkPresentModeKHR				presentMode;
	VkBool32						clipped;
	VkSwapchainKHR					oldSwapchain;
};

struct VkPresentInfoKHR
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				waitSemaphoreCount;
	const VkSemaphore*		pWaitSemaphores;
	deUint32				swapchainCount;
	const VkSwapchainKHR*	pSwapchains;
	const deUint32*			pImageIndices;
	VkResult*				pResults;
};

struct VkImageSwapchainCreateInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkSwapchainKHR	swapchain;
};

struct VkBindImageMemorySwapchainInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkSwapchainKHR	swapchain;
	deUint32		imageIndex;
};

struct VkAcquireNextImageInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkSwapchainKHR	swapchain;
	deUint64		timeout;
	VkSemaphore		semaphore;
	VkFence			fence;
	deUint32		deviceMask;
};

struct VkDeviceGroupPresentCapabilitiesKHR
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							presentMask[VK_MAX_DEVICE_GROUP_SIZE];
	VkDeviceGroupPresentModeFlagsKHR	modes;
};

struct VkDeviceGroupPresentInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							swapchainCount;
	const deUint32*						pDeviceMasks;
	VkDeviceGroupPresentModeFlagBitsKHR	mode;
};

struct VkDeviceGroupSwapchainCreateInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceGroupPresentModeFlagsKHR	modes;
};

struct VkDisplayModeParametersKHR
{
	VkExtent2D	visibleRegion;
	deUint32	refreshRate;
};

struct VkDisplayModeCreateInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkDisplayModeCreateFlagsKHR	flags;
	VkDisplayModeParametersKHR	parameters;
};

struct VkDisplayModePropertiesKHR
{
	VkDisplayModeKHR			displayMode;
	VkDisplayModeParametersKHR	parameters;
};

struct VkDisplayPlaneCapabilitiesKHR
{
	VkDisplayPlaneAlphaFlagsKHR	supportedAlpha;
	VkOffset2D					minSrcPosition;
	VkOffset2D					maxSrcPosition;
	VkExtent2D					minSrcExtent;
	VkExtent2D					maxSrcExtent;
	VkOffset2D					minDstPosition;
	VkOffset2D					maxDstPosition;
	VkExtent2D					minDstExtent;
	VkExtent2D					maxDstExtent;
};

struct VkDisplayPlanePropertiesKHR
{
	VkDisplayKHR	currentDisplay;
	deUint32		currentStackIndex;
};

struct VkDisplayPropertiesKHR
{
	VkDisplayKHR				display;
	const char*					displayName;
	VkExtent2D					physicalDimensions;
	VkExtent2D					physicalResolution;
	VkSurfaceTransformFlagsKHR	supportedTransforms;
	VkBool32					planeReorderPossible;
	VkBool32					persistentContent;
};

struct VkDisplaySurfaceCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkDisplaySurfaceCreateFlagsKHR	flags;
	VkDisplayModeKHR				displayMode;
	deUint32						planeIndex;
	deUint32						planeStackIndex;
	VkSurfaceTransformFlagBitsKHR	transform;
	float							globalAlpha;
	VkDisplayPlaneAlphaFlagBitsKHR	alphaMode;
	VkExtent2D						imageExtent;
};

struct VkDisplayPresentInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkRect2D		srcRect;
	VkRect2D		dstRect;
	VkBool32		persistent;
};

struct VkImportMemoryFdInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagBits	handleType;
	int									fd;
};

struct VkMemoryFdPropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		memoryTypeBits;
};

struct VkMemoryGetFdInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceMemory						memory;
	VkExternalMemoryHandleTypeFlagBits	handleType;
};

struct VkImportSemaphoreFdInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkSemaphoreImportFlags					flags;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
	int										fd;
};

struct VkSemaphoreGetFdInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
};

struct VkPhysicalDevicePushDescriptorPropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxPushDescriptors;
};

struct VkRectLayerKHR
{
	VkOffset2D	offset;
	VkExtent2D	extent;
	deUint32	layer;
};

struct VkPresentRegionKHR
{
	deUint32				rectangleCount;
	const VkRectLayerKHR*	pRectangles;
};

struct VkPresentRegionsKHR
{
	VkStructureType				sType;
	const void*					pNext;
	deUint32					swapchainCount;
	const VkPresentRegionKHR*	pRegions;
};

struct VkSharedPresentSurfaceCapabilitiesKHR
{
	VkStructureType		sType;
	void*				pNext;
	VkImageUsageFlags	sharedPresentSupportedUsageFlags;
};

struct VkImportFenceFdInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkFence								fence;
	VkFenceImportFlags					flags;
	VkExternalFenceHandleTypeFlagBits	handleType;
	int									fd;
};

struct VkFenceGetFdInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkFence								fence;
	VkExternalFenceHandleTypeFlagBits	handleType;
};

struct VkPhysicalDevicePerformanceQueryFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		performanceCounterQueryPools;
	VkBool32		performanceCounterMultipleQueryPools;
};

struct VkPhysicalDevicePerformanceQueryPropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		allowCommandBufferQueryCopies;
};

struct VkPerformanceCounterKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkPerformanceCounterUnitKHR		unit;
	VkPerformanceCounterScopeKHR	scope;
	VkPerformanceCounterStorageKHR	storage;
	deUint8							uuid[VK_UUID_SIZE];
};

struct VkPerformanceCounterDescriptionKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkPerformanceCounterDescriptionFlagsKHR	flags;
	char									name[VK_MAX_DESCRIPTION_SIZE];
	char									category[VK_MAX_DESCRIPTION_SIZE];
	char									description[VK_MAX_DESCRIPTION_SIZE];
};

struct VkQueryPoolPerformanceCreateInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		queueFamilyIndex;
	deUint32		counterIndexCount;
	const deUint32*	pCounterIndices;
};

union VkPerformanceCounterResultKHR
{
	deInt32		int32;
	deInt64		int64;
	deUint32	uint32;
	deUint64	uint64;
	float		float32;
	double		float64;
};

struct VkAcquireProfilingLockInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkAcquireProfilingLockFlagsKHR	flags;
	deUint64						timeout;
};

struct VkPerformanceQuerySubmitInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		counterPassIndex;
};

struct VkPhysicalDeviceSurfaceInfo2KHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkSurfaceKHR	surface;
};

struct VkSurfaceCapabilities2KHR
{
	VkStructureType				sType;
	void*						pNext;
	VkSurfaceCapabilitiesKHR	surfaceCapabilities;
};

struct VkSurfaceFormat2KHR
{
	VkStructureType		sType;
	void*				pNext;
	VkSurfaceFormatKHR	surfaceFormat;
};

struct VkDisplayProperties2KHR
{
	VkStructureType			sType;
	void*					pNext;
	VkDisplayPropertiesKHR	displayProperties;
};

struct VkDisplayPlaneProperties2KHR
{
	VkStructureType				sType;
	void*						pNext;
	VkDisplayPlanePropertiesKHR	displayPlaneProperties;
};

struct VkDisplayModeProperties2KHR
{
	VkStructureType				sType;
	void*						pNext;
	VkDisplayModePropertiesKHR	displayModeProperties;
};

struct VkDisplayPlaneInfo2KHR
{
	VkStructureType		sType;
	const void*			pNext;
	VkDisplayModeKHR	mode;
	deUint32			planeIndex;
};

struct VkDisplayPlaneCapabilities2KHR
{
	VkStructureType					sType;
	void*							pNext;
	VkDisplayPlaneCapabilitiesKHR	capabilities;
};

struct VkPhysicalDeviceShaderClockFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderSubgroupClock;
	VkBool32		shaderDeviceClock;
};

struct VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderTerminateInvocation;
};

struct VkFragmentShadingRateAttachmentInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	const VkAttachmentReference2*	pFragmentShadingRateAttachment;
	VkExtent2D						shadingRateAttachmentTexelSize;
};

struct VkPipelineFragmentShadingRateStateCreateInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkExtent2D							fragmentSize;
	VkFragmentShadingRateCombinerOpKHR	combinerOps[2];
};

struct VkPhysicalDeviceFragmentShadingRateFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		pipelineFragmentShadingRate;
	VkBool32		primitiveFragmentShadingRate;
	VkBool32		attachmentFragmentShadingRate;
};

struct VkPhysicalDeviceFragmentShadingRatePropertiesKHR
{
	VkStructureType			sType;
	void*					pNext;
	VkExtent2D				minFragmentShadingRateAttachmentTexelSize;
	VkExtent2D				maxFragmentShadingRateAttachmentTexelSize;
	deUint32				maxFragmentShadingRateAttachmentTexelSizeAspectRatio;
	VkBool32				primitiveFragmentShadingRateWithMultipleViewports;
	VkBool32				layeredShadingRateAttachments;
	VkBool32				fragmentShadingRateNonTrivialCombinerOps;
	VkExtent2D				maxFragmentSize;
	deUint32				maxFragmentSizeAspectRatio;
	deUint32				maxFragmentShadingRateCoverageSamples;
	VkSampleCountFlagBits	maxFragmentShadingRateRasterizationSamples;
	VkBool32				fragmentShadingRateWithShaderDepthStencilWrites;
	VkBool32				fragmentShadingRateWithSampleMask;
	VkBool32				fragmentShadingRateWithShaderSampleMask;
	VkBool32				fragmentShadingRateWithConservativeRasterization;
	VkBool32				fragmentShadingRateWithFragmentShaderInterlock;
	VkBool32				fragmentShadingRateWithCustomSampleLocations;
	VkBool32				fragmentShadingRateStrictMultiplyCombiner;
};

struct VkPhysicalDeviceFragmentShadingRateKHR
{
	VkStructureType		sType;
	void*				pNext;
	VkSampleCountFlags	sampleCounts;
	VkExtent2D			fragmentSize;
};

struct VkSurfaceProtectedCapabilitiesKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		supportsProtected;
};

struct VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		pipelineExecutableInfo;
};

struct VkPipelineInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkPipeline		pipeline;
};

struct VkPipelineExecutablePropertiesKHR
{
	VkStructureType		sType;
	void*				pNext;
	VkShaderStageFlags	stages;
	char				name[VK_MAX_DESCRIPTION_SIZE];
	char				description[VK_MAX_DESCRIPTION_SIZE];
	deUint32			subgroupSize;
};

struct VkPipelineExecutableInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkPipeline		pipeline;
	deUint32		executableIndex;
};

union VkPipelineExecutableStatisticValueKHR
{
	VkBool32	b32;
	deInt64		i64;
	deUint64	u64;
	double		f64;
};

struct VkPipelineExecutableStatisticKHR
{
	VkStructureType							sType;
	void*									pNext;
	char									name[VK_MAX_DESCRIPTION_SIZE];
	char									description[VK_MAX_DESCRIPTION_SIZE];
	VkPipelineExecutableStatisticFormatKHR	format;
	VkPipelineExecutableStatisticValueKHR	value;
};

struct VkPipelineExecutableInternalRepresentationKHR
{
	VkStructureType	sType;
	void*			pNext;
	char			name[VK_MAX_DESCRIPTION_SIZE];
	char			description[VK_MAX_DESCRIPTION_SIZE];
	VkBool32		isText;
	deUintptr		dataSize;
	void*			pData;
};

struct VkPipelineLibraryCreateInfoKHR
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			libraryCount;
	const VkPipeline*	pLibraries;
};

struct VkMemoryBarrier2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkPipelineStageFlags2KHR	srcStageMask;
	VkAccessFlags2KHR			srcAccessMask;
	VkPipelineStageFlags2KHR	dstStageMask;
	VkAccessFlags2KHR			dstAccessMask;
};

struct VkBufferMemoryBarrier2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkPipelineStageFlags2KHR	srcStageMask;
	VkAccessFlags2KHR			srcAccessMask;
	VkPipelineStageFlags2KHR	dstStageMask;
	VkAccessFlags2KHR			dstAccessMask;
	deUint32					srcQueueFamilyIndex;
	deUint32					dstQueueFamilyIndex;
	VkBuffer					buffer;
	VkDeviceSize				offset;
	VkDeviceSize				size;
};

struct VkImageMemoryBarrier2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkPipelineStageFlags2KHR	srcStageMask;
	VkAccessFlags2KHR			srcAccessMask;
	VkPipelineStageFlags2KHR	dstStageMask;
	VkAccessFlags2KHR			dstAccessMask;
	VkImageLayout				oldLayout;
	VkImageLayout				newLayout;
	deUint32					srcQueueFamilyIndex;
	deUint32					dstQueueFamilyIndex;
	VkImage						image;
	VkImageSubresourceRange		subresourceRange;
};

struct VkDependencyInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkDependencyFlags					dependencyFlags;
	deUint32							memoryBarrierCount;
	const VkMemoryBarrier2KHR*			pMemoryBarriers;
	deUint32							bufferMemoryBarrierCount;
	const VkBufferMemoryBarrier2KHR*	pBufferMemoryBarriers;
	deUint32							imageMemoryBarrierCount;
	const VkImageMemoryBarrier2KHR*		pImageMemoryBarriers;
};

struct VkSemaphoreSubmitInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkSemaphore					semaphore;
	deUint64					value;
	VkPipelineStageFlags2KHR	stageMask;
	deUint32					deviceIndex;
};

struct VkCommandBufferSubmitInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkCommandBuffer	commandBuffer;
	deUint32		deviceMask;
};

struct VkSubmitInfo2KHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkSubmitFlagsKHR					flags;
	deUint32							waitSemaphoreInfoCount;
	const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos;
	deUint32							commandBufferInfoCount;
	const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos;
	deUint32							signalSemaphoreInfoCount;
	const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos;
};

struct VkPhysicalDeviceSynchronization2FeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		synchronization2;
};

struct VkQueueFamilyCheckpointProperties2NV
{
	VkStructureType				sType;
	void*						pNext;
	VkPipelineStageFlags2KHR	checkpointExecutionStageMask;
};

struct VkCheckpointData2NV
{
	VkStructureType				sType;
	void*						pNext;
	VkPipelineStageFlags2KHR	stage;
	void*						pCheckpointMarker;
};

struct VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderZeroInitializeWorkgroupMemory;
};

struct VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		workgroupMemoryExplicitLayout;
	VkBool32		workgroupMemoryExplicitLayoutScalarBlockLayout;
	VkBool32		workgroupMemoryExplicitLayout8BitAccess;
	VkBool32		workgroupMemoryExplicitLayout16BitAccess;
};

struct VkBufferCopy2KHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceSize	srcOffset;
	VkDeviceSize	dstOffset;
	VkDeviceSize	size;
};

struct VkCopyBufferInfo2KHR
{
	VkStructureType			sType;
	const void*				pNext;
	VkBuffer				srcBuffer;
	VkBuffer				dstBuffer;
	deUint32				regionCount;
	const VkBufferCopy2KHR*	pRegions;
};

struct VkImageCopy2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkCopyImageInfo2KHR
{
	VkStructureType			sType;
	const void*				pNext;
	VkImage					srcImage;
	VkImageLayout			srcImageLayout;
	VkImage					dstImage;
	VkImageLayout			dstImageLayout;
	deUint32				regionCount;
	const VkImageCopy2KHR*	pRegions;
};

struct VkBufferImageCopy2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkDeviceSize				bufferOffset;
	deUint32					bufferRowLength;
	deUint32					bufferImageHeight;
	VkImageSubresourceLayers	imageSubresource;
	VkOffset3D					imageOffset;
	VkExtent3D					imageExtent;
};

struct VkCopyBufferToImageInfo2KHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkBuffer						srcBuffer;
	VkImage							dstImage;
	VkImageLayout					dstImageLayout;
	deUint32						regionCount;
	const VkBufferImageCopy2KHR*	pRegions;
};

struct VkCopyImageToBufferInfo2KHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkImage							srcImage;
	VkImageLayout					srcImageLayout;
	VkBuffer						dstBuffer;
	deUint32						regionCount;
	const VkBufferImageCopy2KHR*	pRegions;
};

struct VkImageBlit2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffsets[2];
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffsets[2];
};

struct VkBlitImageInfo2KHR
{
	VkStructureType			sType;
	const void*				pNext;
	VkImage					srcImage;
	VkImageLayout			srcImageLayout;
	VkImage					dstImage;
	VkImageLayout			dstImageLayout;
	deUint32				regionCount;
	const VkImageBlit2KHR*	pRegions;
	VkFilter				filter;
};

struct VkImageResolve2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkResolveImageInfo2KHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkImage						srcImage;
	VkImageLayout				srcImageLayout;
	VkImage						dstImage;
	VkImageLayout				dstImageLayout;
	deUint32					regionCount;
	const VkImageResolve2KHR*	pRegions;
};

struct VkDebugReportCallbackCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkDebugReportFlagsEXT			flags;
	PFN_vkDebugReportCallbackEXT	pfnCallback;
	void*							pUserData;
};

struct VkPipelineRasterizationStateRasterizationOrderAMD
{
	VkStructureType			sType;
	const void*				pNext;
	VkRasterizationOrderAMD	rasterizationOrder;
};

struct VkDebugMarkerObjectNameInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkDebugReportObjectTypeEXT	objectType;
	deUint64					object;
	const char*					pObjectName;
};

struct VkDebugMarkerObjectTagInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkDebugReportObjectTypeEXT	objectType;
	deUint64					object;
	deUint64					tagName;
	deUintptr					tagSize;
	const void*					pTag;
};

struct VkDebugMarkerMarkerInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	const char*		pMarkerName;
	float			color[4];
};

struct VkDedicatedAllocationImageCreateInfoNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		dedicatedAllocation;
};

struct VkDedicatedAllocationBufferCreateInfoNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		dedicatedAllocation;
};

struct VkDedicatedAllocationMemoryAllocateInfoNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkImage			image;
	VkBuffer		buffer;
};

struct VkPhysicalDeviceTransformFeedbackFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		transformFeedback;
	VkBool32		geometryStreams;
};

struct VkPhysicalDeviceTransformFeedbackPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxTransformFeedbackStreams;
	deUint32		maxTransformFeedbackBuffers;
	VkDeviceSize	maxTransformFeedbackBufferSize;
	deUint32		maxTransformFeedbackStreamDataSize;
	deUint32		maxTransformFeedbackBufferDataSize;
	deUint32		maxTransformFeedbackBufferDataStride;
	VkBool32		transformFeedbackQueries;
	VkBool32		transformFeedbackStreamsLinesTriangles;
	VkBool32		transformFeedbackRasterizationStreamSelect;
	VkBool32		transformFeedbackDraw;
};

struct VkPipelineRasterizationStateStreamCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	VkPipelineRasterizationStateStreamCreateFlagsEXT	flags;
	deUint32											rasterizationStream;
};

struct VkImageViewHandleInfoNVX
{
	VkStructureType		sType;
	const void*			pNext;
	VkImageView			imageView;
	VkDescriptorType	descriptorType;
	VkSampler			sampler;
};

struct VkImageViewAddressPropertiesNVX
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceAddress	deviceAddress;
	VkDeviceSize	size;
};

struct VkTextureLODGatherFormatPropertiesAMD
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		supportsTextureGatherLODBiasAMD;
};

struct VkShaderResourceUsageAMD
{
	deUint32	numUsedVgprs;
	deUint32	numUsedSgprs;
	deUint32	ldsSizePerLocalWorkGroup;
	deUintptr	ldsUsageSizeInBytes;
	deUintptr	scratchMemUsageInBytes;
};

struct VkShaderStatisticsInfoAMD
{
	VkShaderStageFlags			shaderStageMask;
	VkShaderResourceUsageAMD	resourceUsage;
	deUint32					numPhysicalVgprs;
	deUint32					numPhysicalSgprs;
	deUint32					numAvailableVgprs;
	deUint32					numAvailableSgprs;
	deUint32					computeWorkGroupSize[3];
};

struct VkPhysicalDeviceCornerSampledImageFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		cornerSampledImage;
};

struct VkExternalImageFormatPropertiesNV
{
	VkImageFormatProperties				imageFormatProperties;
	VkExternalMemoryFeatureFlagsNV		externalMemoryFeatures;
	VkExternalMemoryHandleTypeFlagsNV	exportFromImportedHandleTypes;
	VkExternalMemoryHandleTypeFlagsNV	compatibleHandleTypes;
};

struct VkExternalMemoryImageCreateInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagsNV	handleTypes;
};

struct VkExportMemoryAllocateInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagsNV	handleTypes;
};

struct VkValidationFlagsEXT
{
	VkStructureType				sType;
	const void*					pNext;
	deUint32					disabledValidationCheckCount;
	const VkValidationCheckEXT*	pDisabledValidationChecks;
};

struct VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		textureCompressionASTC_HDR;
};

struct VkImageViewASTCDecodeModeEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkFormat		decodeMode;
};

struct VkPhysicalDeviceASTCDecodeFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		decodeModeSharedExponent;
};

struct VkConditionalRenderingBeginInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkBuffer						buffer;
	VkDeviceSize					offset;
	VkConditionalRenderingFlagsEXT	flags;
};

struct VkPhysicalDeviceConditionalRenderingFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		conditionalRendering;
	VkBool32		inheritedConditionalRendering;
};

struct VkCommandBufferInheritanceConditionalRenderingInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		conditionalRenderingEnable;
};

struct VkViewportWScalingNV
{
	float	xcoeff;
	float	ycoeff;
};

struct VkPipelineViewportWScalingStateCreateInfoNV
{
	VkStructureType				sType;
	const void*					pNext;
	VkBool32					viewportWScalingEnable;
	deUint32					viewportCount;
	const VkViewportWScalingNV*	pViewportWScalings;
};

struct VkSurfaceCapabilities2EXT
{
	VkStructureType					sType;
	void*							pNext;
	deUint32						minImageCount;
	deUint32						maxImageCount;
	VkExtent2D						currentExtent;
	VkExtent2D						minImageExtent;
	VkExtent2D						maxImageExtent;
	deUint32						maxImageArrayLayers;
	VkSurfaceTransformFlagsKHR		supportedTransforms;
	VkSurfaceTransformFlagBitsKHR	currentTransform;
	VkCompositeAlphaFlagsKHR		supportedCompositeAlpha;
	VkImageUsageFlags				supportedUsageFlags;
	VkSurfaceCounterFlagsEXT		supportedSurfaceCounters;
};

struct VkDisplayPowerInfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	VkDisplayPowerStateEXT	powerState;
};

struct VkDeviceEventInfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	VkDeviceEventTypeEXT	deviceEvent;
};

struct VkDisplayEventInfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	VkDisplayEventTypeEXT	displayEvent;
};

struct VkSwapchainCounterCreateInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkSurfaceCounterFlagsEXT	surfaceCounters;
};

struct VkRefreshCycleDurationGOOGLE
{
	deUint64	refreshDuration;
};

struct VkPastPresentationTimingGOOGLE
{
	deUint32	presentID;
	deUint64	desiredPresentTime;
	deUint64	actualPresentTime;
	deUint64	earliestPresentTime;
	deUint64	presentMargin;
};

struct VkPresentTimeGOOGLE
{
	deUint32	presentID;
	deUint64	desiredPresentTime;
};

struct VkPresentTimesInfoGOOGLE
{
	VkStructureType				sType;
	const void*					pNext;
	deUint32					swapchainCount;
	const VkPresentTimeGOOGLE*	pTimes;
};

struct VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		perViewPositionAllComponents;
};

struct VkViewportSwizzleNV
{
	VkViewportCoordinateSwizzleNV	x;
	VkViewportCoordinateSwizzleNV	y;
	VkViewportCoordinateSwizzleNV	z;
	VkViewportCoordinateSwizzleNV	w;
};

struct VkPipelineViewportSwizzleStateCreateInfoNV
{
	VkStructureType								sType;
	const void*									pNext;
	VkPipelineViewportSwizzleStateCreateFlagsNV	flags;
	deUint32									viewportCount;
	const VkViewportSwizzleNV*					pViewportSwizzles;
};

struct VkPhysicalDeviceDiscardRectanglePropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxDiscardRectangles;
};

struct VkPipelineDiscardRectangleStateCreateInfoEXT
{
	VkStructureType									sType;
	const void*										pNext;
	VkPipelineDiscardRectangleStateCreateFlagsEXT	flags;
	VkDiscardRectangleModeEXT						discardRectangleMode;
	deUint32										discardRectangleCount;
	const VkRect2D*									pDiscardRectangles;
};

struct VkPhysicalDeviceConservativeRasterizationPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	float			primitiveOverestimationSize;
	float			maxExtraPrimitiveOverestimationSize;
	float			extraPrimitiveOverestimationSizeGranularity;
	VkBool32		primitiveUnderestimation;
	VkBool32		conservativePointAndLineRasterization;
	VkBool32		degenerateTrianglesRasterized;
	VkBool32		degenerateLinesRasterized;
	VkBool32		fullyCoveredFragmentShaderInputVariable;
	VkBool32		conservativeRasterizationPostDepthCoverage;
};

struct VkPipelineRasterizationConservativeStateCreateInfoEXT
{
	VkStructureType											sType;
	const void*												pNext;
	VkPipelineRasterizationConservativeStateCreateFlagsEXT	flags;
	VkConservativeRasterizationModeEXT						conservativeRasterizationMode;
	float													extraPrimitiveOverestimationSize;
};

struct VkPhysicalDeviceDepthClipEnableFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		depthClipEnable;
};

struct VkPipelineRasterizationDepthClipStateCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	VkPipelineRasterizationDepthClipStateCreateFlagsEXT	flags;
	VkBool32											depthClipEnable;
};

struct VkXYColorEXT
{
	float	x;
	float	y;
};

struct VkHdrMetadataEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkXYColorEXT	displayPrimaryRed;
	VkXYColorEXT	displayPrimaryGreen;
	VkXYColorEXT	displayPrimaryBlue;
	VkXYColorEXT	whitePoint;
	float			maxLuminance;
	float			minLuminance;
	float			maxContentLightLevel;
	float			maxFrameAverageLightLevel;
};

struct VkDebugUtilsLabelEXT
{
	VkStructureType	sType;
	const void*		pNext;
	const char*		pLabelName;
	float			color[4];
};

struct VkDebugUtilsObjectNameInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkObjectType	objectType;
	deUint64		objectHandle;
	const char*		pObjectName;
};

struct VkDebugUtilsMessengerCallbackDataEXT
{
	VkStructureType								sType;
	const void*									pNext;
	VkDebugUtilsMessengerCallbackDataFlagsEXT	flags;
	const char*									pMessageIdName;
	deInt32										messageIdNumber;
	const char*									pMessage;
	deUint32									queueLabelCount;
	const VkDebugUtilsLabelEXT*					pQueueLabels;
	deUint32									cmdBufLabelCount;
	const VkDebugUtilsLabelEXT*					pCmdBufLabels;
	deUint32									objectCount;
	const VkDebugUtilsObjectNameInfoEXT*		pObjects;
};

struct VkDebugUtilsMessengerCreateInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	VkDebugUtilsMessengerCreateFlagsEXT		flags;
	VkDebugUtilsMessageSeverityFlagsEXT		messageSeverity;
	VkDebugUtilsMessageTypeFlagsEXT			messageType;
	PFN_vkDebugUtilsMessengerCallbackEXT	pfnUserCallback;
	void*									pUserData;
};

struct VkDebugUtilsObjectTagInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkObjectType	objectType;
	deUint64		objectHandle;
	deUint64		tagName;
	deUintptr		tagSize;
	const void*		pTag;
};

struct VkPhysicalDeviceInlineUniformBlockFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		inlineUniformBlock;
	VkBool32		descriptorBindingInlineUniformBlockUpdateAfterBind;
};

struct VkPhysicalDeviceInlineUniformBlockPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxInlineUniformBlockSize;
	deUint32		maxPerStageDescriptorInlineUniformBlocks;
	deUint32		maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks;
	deUint32		maxDescriptorSetInlineUniformBlocks;
	deUint32		maxDescriptorSetUpdateAfterBindInlineUniformBlocks;
};

struct VkWriteDescriptorSetInlineUniformBlockEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		dataSize;
	const void*		pData;
};

struct VkDescriptorPoolInlineUniformBlockCreateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		maxInlineUniformBlockBindings;
};

struct VkSampleLocationEXT
{
	float	x;
	float	y;
};

struct VkSampleLocationsInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkSampleCountFlagBits		sampleLocationsPerPixel;
	VkExtent2D					sampleLocationGridSize;
	deUint32					sampleLocationsCount;
	const VkSampleLocationEXT*	pSampleLocations;
};

struct VkAttachmentSampleLocationsEXT
{
	deUint32					attachmentIndex;
	VkSampleLocationsInfoEXT	sampleLocationsInfo;
};

struct VkSubpassSampleLocationsEXT
{
	deUint32					subpassIndex;
	VkSampleLocationsInfoEXT	sampleLocationsInfo;
};

struct VkRenderPassSampleLocationsBeginInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								attachmentInitialSampleLocationsCount;
	const VkAttachmentSampleLocationsEXT*	pAttachmentInitialSampleLocations;
	deUint32								postSubpassSampleLocationsCount;
	const VkSubpassSampleLocationsEXT*		pPostSubpassSampleLocations;
};

struct VkPipelineSampleLocationsStateCreateInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkBool32					sampleLocationsEnable;
	VkSampleLocationsInfoEXT	sampleLocationsInfo;
};

struct VkPhysicalDeviceSampleLocationsPropertiesEXT
{
	VkStructureType		sType;
	void*				pNext;
	VkSampleCountFlags	sampleLocationSampleCounts;
	VkExtent2D			maxSampleLocationGridSize;
	float				sampleLocationCoordinateRange[2];
	deUint32			sampleLocationSubPixelBits;
	VkBool32			variableSampleLocations;
};

struct VkMultisamplePropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkExtent2D		maxSampleLocationGridSize;
};

struct VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		advancedBlendCoherentOperations;
};

struct VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		advancedBlendMaxColorAttachments;
	VkBool32		advancedBlendIndependentBlend;
	VkBool32		advancedBlendNonPremultipliedSrcColor;
	VkBool32		advancedBlendNonPremultipliedDstColor;
	VkBool32		advancedBlendCorrelatedOverlap;
	VkBool32		advancedBlendAllOperations;
};

struct VkPipelineColorBlendAdvancedStateCreateInfoEXT
{
	VkStructureType		sType;
	const void*			pNext;
	VkBool32			srcPremultiplied;
	VkBool32			dstPremultiplied;
	VkBlendOverlapEXT	blendOverlap;
};

struct VkPipelineCoverageToColorStateCreateInfoNV
{
	VkStructureType								sType;
	const void*									pNext;
	VkPipelineCoverageToColorStateCreateFlagsNV	flags;
	VkBool32									coverageToColorEnable;
	deUint32									coverageToColorLocation;
};

struct VkPipelineCoverageModulationStateCreateInfoNV
{
	VkStructureType									sType;
	const void*										pNext;
	VkPipelineCoverageModulationStateCreateFlagsNV	flags;
	VkCoverageModulationModeNV						coverageModulationMode;
	VkBool32										coverageModulationTableEnable;
	deUint32										coverageModulationTableCount;
	const float*									pCoverageModulationTable;
};

struct VkPhysicalDeviceShaderSMBuiltinsPropertiesNV
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		shaderSMCount;
	deUint32		shaderWarpsPerSM;
};

struct VkPhysicalDeviceShaderSMBuiltinsFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderSMBuiltins;
};

struct VkDrmFormatModifierPropertiesEXT
{
	deUint64				drmFormatModifier;
	deUint32				drmFormatModifierPlaneCount;
	VkFormatFeatureFlags	drmFormatModifierTilingFeatures;
};

struct VkDrmFormatModifierPropertiesListEXT
{
	VkStructureType						sType;
	void*								pNext;
	deUint32							drmFormatModifierCount;
	VkDrmFormatModifierPropertiesEXT*	pDrmFormatModifierProperties;
};

struct VkPhysicalDeviceImageDrmFormatModifierInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint64		drmFormatModifier;
	VkSharingMode	sharingMode;
	deUint32		queueFamilyIndexCount;
	const deUint32*	pQueueFamilyIndices;
};

struct VkImageDrmFormatModifierListCreateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		drmFormatModifierCount;
	const deUint64*	pDrmFormatModifiers;
};

struct VkImageDrmFormatModifierExplicitCreateInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	deUint64					drmFormatModifier;
	deUint32					drmFormatModifierPlaneCount;
	const VkSubresourceLayout*	pPlaneLayouts;
};

struct VkImageDrmFormatModifierPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint64		drmFormatModifier;
};

struct VkValidationCacheCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkValidationCacheCreateFlagsEXT	flags;
	deUintptr						initialDataSize;
	const void*						pInitialData;
};

struct VkShaderModuleValidationCacheCreateInfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	VkValidationCacheEXT	validationCache;
};

struct VkShadingRatePaletteNV
{
	deUint32							shadingRatePaletteEntryCount;
	const VkShadingRatePaletteEntryNV*	pShadingRatePaletteEntries;
};

struct VkPipelineViewportShadingRateImageStateCreateInfoNV
{
	VkStructureType					sType;
	const void*						pNext;
	VkBool32						shadingRateImageEnable;
	deUint32						viewportCount;
	const VkShadingRatePaletteNV*	pShadingRatePalettes;
};

struct VkPhysicalDeviceShadingRateImageFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shadingRateImage;
	VkBool32		shadingRateCoarseSampleOrder;
};

struct VkPhysicalDeviceShadingRateImagePropertiesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkExtent2D		shadingRateTexelSize;
	deUint32		shadingRatePaletteSize;
	deUint32		shadingRateMaxCoarseSamples;
};

struct VkCoarseSampleLocationNV
{
	deUint32	pixelX;
	deUint32	pixelY;
	deUint32	sample;
};

struct VkCoarseSampleOrderCustomNV
{
	VkShadingRatePaletteEntryNV		shadingRate;
	deUint32						sampleCount;
	deUint32						sampleLocationCount;
	const VkCoarseSampleLocationNV*	pSampleLocations;
};

struct VkPipelineViewportCoarseSampleOrderStateCreateInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkCoarseSampleOrderTypeNV			sampleOrderType;
	deUint32							customSampleOrderCount;
	const VkCoarseSampleOrderCustomNV*	pCustomSampleOrders;
};

struct VkRayTracingShaderGroupCreateInfoNV
{
	VkStructureType					sType;
	const void*						pNext;
	VkRayTracingShaderGroupTypeKHR	type;
	deUint32						generalShader;
	deUint32						closestHitShader;
	deUint32						anyHitShader;
	deUint32						intersectionShader;
};

struct VkRayTracingPipelineCreateInfoNV
{
	VkStructureType								sType;
	const void*									pNext;
	VkPipelineCreateFlags						flags;
	deUint32									stageCount;
	const VkPipelineShaderStageCreateInfo*		pStages;
	deUint32									groupCount;
	const VkRayTracingShaderGroupCreateInfoNV*	pGroups;
	deUint32									maxRecursionDepth;
	VkPipelineLayout							layout;
	VkPipeline									basePipelineHandle;
	deInt32										basePipelineIndex;
};

struct VkGeometryTrianglesNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkBuffer		vertexData;
	VkDeviceSize	vertexOffset;
	deUint32		vertexCount;
	VkDeviceSize	vertexStride;
	VkFormat		vertexFormat;
	VkBuffer		indexData;
	VkDeviceSize	indexOffset;
	deUint32		indexCount;
	VkIndexType		indexType;
	VkBuffer		transformData;
	VkDeviceSize	transformOffset;
};

struct VkGeometryAABBNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkBuffer		aabbData;
	deUint32		numAABBs;
	deUint32		stride;
	VkDeviceSize	offset;
};

struct VkGeometryDataNV
{
	VkGeometryTrianglesNV	triangles;
	VkGeometryAABBNV		aabbs;
};

struct VkGeometryNV
{
	VkStructureType		sType;
	const void*			pNext;
	VkGeometryTypeKHR	geometryType;
	VkGeometryDataNV	geometry;
	VkGeometryFlagsKHR	flags;
};

struct VkAccelerationStructureInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkAccelerationStructureTypeNV		type;
	VkBuildAccelerationStructureFlagsNV	flags;
	deUint32							instanceCount;
	deUint32							geometryCount;
	const VkGeometryNV*					pGeometries;
};

struct VkAccelerationStructureCreateInfoNV
{
	VkStructureType					sType;
	const void*						pNext;
	VkDeviceSize					compactedSize;
	VkAccelerationStructureInfoNV	info;
};

struct VkBindAccelerationStructureMemoryInfoNV
{
	VkStructureType				sType;
	const void*					pNext;
	VkAccelerationStructureNV	accelerationStructure;
	VkDeviceMemory				memory;
	VkDeviceSize				memoryOffset;
	deUint32					deviceIndexCount;
	const deUint32*				pDeviceIndices;
};

struct VkWriteDescriptorSetAccelerationStructureNV
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							accelerationStructureCount;
	const VkAccelerationStructureNV*	pAccelerationStructures;
};

struct VkAccelerationStructureMemoryRequirementsInfoNV
{
	VkStructureType									sType;
	const void*										pNext;
	VkAccelerationStructureMemoryRequirementsTypeNV	type;
	VkAccelerationStructureNV						accelerationStructure;
};

struct VkPhysicalDeviceRayTracingPropertiesNV
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		shaderGroupHandleSize;
	deUint32		maxRecursionDepth;
	deUint32		maxShaderGroupStride;
	deUint32		shaderGroupBaseAlignment;
	deUint64		maxGeometryCount;
	deUint64		maxInstanceCount;
	deUint64		maxTriangleCount;
	deUint32		maxDescriptorSetAccelerationStructures;
};

struct VkTransformMatrixKHR
{
	float	matrix[3][4];
};

struct VkAabbPositionsKHR
{
	float	minX;
	float	minY;
	float	minZ;
	float	maxX;
	float	maxY;
	float	maxZ;
};

struct VkAccelerationStructureInstanceKHR
{
	VkTransformMatrixKHR		transform;
	deUint32					instanceCustomIndex:24;
	deUint32					mask:8;
	deUint32					instanceShaderBindingTableRecordOffset:24;
	VkGeometryInstanceFlagsKHR	flags:8;
	deUint64					accelerationStructureReference;
};

struct VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		representativeFragmentTest;
};

struct VkPipelineRepresentativeFragmentTestStateCreateInfoNV
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		representativeFragmentTestEnable;
};

struct VkPhysicalDeviceImageViewImageFormatInfoEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkImageViewType	imageViewType;
};

struct VkFilterCubicImageViewImageFormatPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		filterCubic;
	VkBool32		filterCubicMinmax;
};

struct VkDeviceQueueGlobalPriorityCreateInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkQueueGlobalPriorityEXT	globalPriority;
};

struct VkImportMemoryHostPointerInfoEXT
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagBits	handleType;
	void*								pHostPointer;
};

struct VkMemoryHostPointerPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		memoryTypeBits;
};

struct VkPhysicalDeviceExternalMemoryHostPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceSize	minImportedHostPointerAlignment;
};

struct VkPipelineCompilerControlCreateInfoAMD
{
	VkStructureType						sType;
	const void*							pNext;
	VkPipelineCompilerControlFlagsAMD	compilerControlFlags;
};

struct VkCalibratedTimestampInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkTimeDomainEXT	timeDomain;
};

struct VkPhysicalDeviceShaderCorePropertiesAMD
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		shaderEngineCount;
	deUint32		shaderArraysPerEngineCount;
	deUint32		computeUnitsPerShaderArray;
	deUint32		simdPerComputeUnit;
	deUint32		wavefrontsPerSimd;
	deUint32		wavefrontSize;
	deUint32		sgprsPerSimd;
	deUint32		minSgprAllocation;
	deUint32		maxSgprAllocation;
	deUint32		sgprAllocationGranularity;
	deUint32		vgprsPerSimd;
	deUint32		minVgprAllocation;
	deUint32		maxVgprAllocation;
	deUint32		vgprAllocationGranularity;
};

struct VkDeviceMemoryOverallocationCreateInfoAMD
{
	VkStructureType						sType;
	const void*							pNext;
	VkMemoryOverallocationBehaviorAMD	overallocationBehavior;
};

struct VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxVertexAttribDivisor;
};

struct VkVertexInputBindingDivisorDescriptionEXT
{
	deUint32	binding;
	deUint32	divisor;
};

struct VkPipelineVertexInputDivisorStateCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	deUint32											vertexBindingDivisorCount;
	const VkVertexInputBindingDivisorDescriptionEXT*	pVertexBindingDivisors;
};

struct VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		vertexAttributeInstanceRateDivisor;
	VkBool32		vertexAttributeInstanceRateZeroDivisor;
};

struct VkPipelineCreationFeedbackEXT
{
	VkPipelineCreationFeedbackFlagsEXT	flags;
	deUint64							duration;
};

struct VkPipelineCreationFeedbackCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkPipelineCreationFeedbackEXT*	pPipelineCreationFeedback;
	deUint32						pipelineStageCreationFeedbackCount;
	VkPipelineCreationFeedbackEXT*	pPipelineStageCreationFeedbacks;
};

struct VkPhysicalDeviceComputeShaderDerivativesFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		computeDerivativeGroupQuads;
	VkBool32		computeDerivativeGroupLinear;
};

struct VkPhysicalDeviceMeshShaderFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		taskShader;
	VkBool32		meshShader;
};

struct VkPhysicalDeviceMeshShaderPropertiesNV
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxDrawMeshTasksCount;
	deUint32		maxTaskWorkGroupInvocations;
	deUint32		maxTaskWorkGroupSize[3];
	deUint32		maxTaskTotalMemorySize;
	deUint32		maxTaskOutputCount;
	deUint32		maxMeshWorkGroupInvocations;
	deUint32		maxMeshWorkGroupSize[3];
	deUint32		maxMeshTotalMemorySize;
	deUint32		maxMeshOutputVertices;
	deUint32		maxMeshOutputPrimitives;
	deUint32		maxMeshMultiviewViewCount;
	deUint32		meshOutputPerVertexGranularity;
	deUint32		meshOutputPerPrimitiveGranularity;
};

struct VkDrawMeshTasksIndirectCommandNV
{
	deUint32	taskCount;
	deUint32	firstTask;
};

struct VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fragmentShaderBarycentric;
};

struct VkPhysicalDeviceShaderImageFootprintFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		imageFootprint;
};

struct VkPipelineViewportExclusiveScissorStateCreateInfoNV
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		exclusiveScissorCount;
	const VkRect2D*	pExclusiveScissors;
};

struct VkPhysicalDeviceExclusiveScissorFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		exclusiveScissor;
};

struct VkQueueFamilyCheckpointPropertiesNV
{
	VkStructureType			sType;
	void*					pNext;
	VkPipelineStageFlags	checkpointExecutionStageMask;
};

struct VkCheckpointDataNV
{
	VkStructureType			sType;
	void*					pNext;
	VkPipelineStageFlagBits	stage;
	void*					pCheckpointMarker;
};

struct VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderIntegerFunctions2;
};

union VkPerformanceValueDataINTEL
{
	deUint32	value32;
	deUint64	value64;
	float		valueFloat;
	VkBool32	valueBool;
	const char*	valueString;
};

struct VkPerformanceValueINTEL
{
	VkPerformanceValueTypeINTEL	type;
	VkPerformanceValueDataINTEL	data;
};

struct VkInitializePerformanceApiInfoINTEL
{
	VkStructureType	sType;
	const void*		pNext;
	void*			pUserData;
};

struct VkQueryPoolPerformanceQueryCreateInfoINTEL
{
	VkStructureType					sType;
	const void*						pNext;
	VkQueryPoolSamplingModeINTEL	performanceCountersSampling;
};

struct VkPerformanceMarkerInfoINTEL
{
	VkStructureType	sType;
	const void*		pNext;
	deUint64		marker;
};

struct VkPerformanceStreamMarkerInfoINTEL
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		marker;
};

struct VkPerformanceOverrideInfoINTEL
{
	VkStructureType					sType;
	const void*						pNext;
	VkPerformanceOverrideTypeINTEL	type;
	VkBool32						enable;
	deUint64						parameter;
};

struct VkPerformanceConfigurationAcquireInfoINTEL
{
	VkStructureType						sType;
	const void*							pNext;
	VkPerformanceConfigurationTypeINTEL	type;
};

struct VkPhysicalDevicePCIBusInfoPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		pciDomain;
	deUint32		pciBus;
	deUint32		pciDevice;
	deUint32		pciFunction;
};

struct VkDisplayNativeHdrSurfaceCapabilitiesAMD
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		localDimmingSupport;
};

struct VkSwapchainDisplayNativeHdrCreateInfoAMD
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		localDimmingEnable;
};

struct VkPhysicalDeviceFragmentDensityMapFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fragmentDensityMap;
	VkBool32		fragmentDensityMapDynamic;
	VkBool32		fragmentDensityMapNonSubsampledImages;
};

struct VkPhysicalDeviceFragmentDensityMapPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkExtent2D		minFragmentDensityTexelSize;
	VkExtent2D		maxFragmentDensityTexelSize;
	VkBool32		fragmentDensityInvocations;
};

struct VkRenderPassFragmentDensityMapCreateInfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	VkAttachmentReference	fragmentDensityMapAttachment;
};

struct VkPhysicalDeviceSubgroupSizeControlFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		subgroupSizeControl;
	VkBool32		computeFullSubgroups;
};

struct VkPhysicalDeviceSubgroupSizeControlPropertiesEXT
{
	VkStructureType		sType;
	void*				pNext;
	deUint32			minSubgroupSize;
	deUint32			maxSubgroupSize;
	deUint32			maxComputeWorkgroupSubgroups;
	VkShaderStageFlags	requiredSubgroupSizeStages;
};

struct VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		requiredSubgroupSize;
};

struct VkPhysicalDeviceShaderCoreProperties2AMD
{
	VkStructureType					sType;
	void*							pNext;
	VkShaderCorePropertiesFlagsAMD	shaderCoreFeatures;
	deUint32						activeComputeUnitCount;
};

struct VkPhysicalDeviceCoherentMemoryFeaturesAMD
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		deviceCoherentMemory;
};

struct VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderImageInt64Atomics;
	VkBool32		sparseImageInt64Atomics;
};

struct VkPhysicalDeviceMemoryBudgetPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceSize	heapBudget[VK_MAX_MEMORY_HEAPS];
	VkDeviceSize	heapUsage[VK_MAX_MEMORY_HEAPS];
};

struct VkPhysicalDeviceMemoryPriorityFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		memoryPriority;
};

struct VkMemoryPriorityAllocateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	float			priority;
};

struct VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		dedicatedAllocationImageAliasing;
};

struct VkPhysicalDeviceBufferDeviceAddressFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		bufferDeviceAddress;
	VkBool32		bufferDeviceAddressCaptureReplay;
	VkBool32		bufferDeviceAddressMultiDevice;
};

struct VkBufferDeviceAddressCreateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceAddress	deviceAddress;
};

struct VkPhysicalDeviceToolPropertiesEXT
{
	VkStructureType			sType;
	void*					pNext;
	char					name[VK_MAX_EXTENSION_NAME_SIZE];
	char					version[VK_MAX_EXTENSION_NAME_SIZE];
	VkToolPurposeFlagsEXT	purposes;
	char					description[VK_MAX_DESCRIPTION_SIZE];
	char					layer[VK_MAX_EXTENSION_NAME_SIZE];
};

struct VkValidationFeaturesEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								enabledValidationFeatureCount;
	const VkValidationFeatureEnableEXT*		pEnabledValidationFeatures;
	deUint32								disabledValidationFeatureCount;
	const VkValidationFeatureDisableEXT*	pDisabledValidationFeatures;
};

struct VkCooperativeMatrixPropertiesNV
{
	VkStructureType		sType;
	void*				pNext;
	deUint32			MSize;
	deUint32			NSize;
	deUint32			KSize;
	VkComponentTypeNV	AType;
	VkComponentTypeNV	BType;
	VkComponentTypeNV	CType;
	VkComponentTypeNV	DType;
	VkScopeNV			scope;
};

struct VkPhysicalDeviceCooperativeMatrixFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		cooperativeMatrix;
	VkBool32		cooperativeMatrixRobustBufferAccess;
};

struct VkPhysicalDeviceCooperativeMatrixPropertiesNV
{
	VkStructureType		sType;
	void*				pNext;
	VkShaderStageFlags	cooperativeMatrixSupportedStages;
};

struct VkPhysicalDeviceCoverageReductionModeFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		coverageReductionMode;
};

struct VkPipelineCoverageReductionStateCreateInfoNV
{
	VkStructureType									sType;
	const void*										pNext;
	VkPipelineCoverageReductionStateCreateFlagsNV	flags;
	VkCoverageReductionModeNV						coverageReductionMode;
};

struct VkFramebufferMixedSamplesCombinationNV
{
	VkStructureType				sType;
	void*						pNext;
	VkCoverageReductionModeNV	coverageReductionMode;
	VkSampleCountFlagBits		rasterizationSamples;
	VkSampleCountFlags			depthStencilSamples;
	VkSampleCountFlags			colorSamples;
};

struct VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fragmentShaderSampleInterlock;
	VkBool32		fragmentShaderPixelInterlock;
	VkBool32		fragmentShaderShadingRateInterlock;
};

struct VkPhysicalDeviceYcbcrImageArraysFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		ycbcrImageArrays;
};

struct VkHeadlessSurfaceCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkHeadlessSurfaceCreateFlagsEXT	flags;
};

struct VkPhysicalDeviceLineRasterizationFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		rectangularLines;
	VkBool32		bresenhamLines;
	VkBool32		smoothLines;
	VkBool32		stippledRectangularLines;
	VkBool32		stippledBresenhamLines;
	VkBool32		stippledSmoothLines;
};

struct VkPhysicalDeviceLineRasterizationPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		lineSubPixelPrecisionBits;
};

struct VkPipelineRasterizationLineStateCreateInfoEXT
{
	VkStructureType				sType;
	const void*					pNext;
	VkLineRasterizationModeEXT	lineRasterizationMode;
	VkBool32					stippledLineEnable;
	deUint32					lineStippleFactor;
	deUint16					lineStipplePattern;
};

struct VkPhysicalDeviceShaderAtomicFloatFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderBufferFloat32Atomics;
	VkBool32		shaderBufferFloat32AtomicAdd;
	VkBool32		shaderBufferFloat64Atomics;
	VkBool32		shaderBufferFloat64AtomicAdd;
	VkBool32		shaderSharedFloat32Atomics;
	VkBool32		shaderSharedFloat32AtomicAdd;
	VkBool32		shaderSharedFloat64Atomics;
	VkBool32		shaderSharedFloat64AtomicAdd;
	VkBool32		shaderImageFloat32Atomics;
	VkBool32		shaderImageFloat32AtomicAdd;
	VkBool32		sparseImageFloat32Atomics;
	VkBool32		sparseImageFloat32AtomicAdd;
};

struct VkPhysicalDeviceIndexTypeUint8FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		indexTypeUint8;
};

struct VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		extendedDynamicState;
};

struct VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderDemoteToHelperInvocation;
};

struct VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxGraphicsShaderGroupCount;
	deUint32		maxIndirectSequenceCount;
	deUint32		maxIndirectCommandsTokenCount;
	deUint32		maxIndirectCommandsStreamCount;
	deUint32		maxIndirectCommandsTokenOffset;
	deUint32		maxIndirectCommandsStreamStride;
	deUint32		minSequencesCountBufferOffsetAlignment;
	deUint32		minSequencesIndexBufferOffsetAlignment;
	deUint32		minIndirectCommandsBufferOffsetAlignment;
};

struct VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		deviceGeneratedCommands;
};

struct VkGraphicsShaderGroupCreateInfoNV
{
	VkStructureType									sType;
	const void*										pNext;
	deUint32										stageCount;
	const VkPipelineShaderStageCreateInfo*			pStages;
	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
};

struct VkGraphicsPipelineShaderGroupsCreateInfoNV
{
	VkStructureType								sType;
	const void*									pNext;
	deUint32									groupCount;
	const VkGraphicsShaderGroupCreateInfoNV*	pGroups;
	deUint32									pipelineCount;
	const VkPipeline*							pPipelines;
};

struct VkBindShaderGroupIndirectCommandNV
{
	deUint32	groupIndex;
};

struct VkBindIndexBufferIndirectCommandNV
{
	VkDeviceAddress	bufferAddress;
	deUint32		size;
	VkIndexType		indexType;
};

struct VkBindVertexBufferIndirectCommandNV
{
	VkDeviceAddress	bufferAddress;
	deUint32		size;
	deUint32		stride;
};

struct VkSetStateFlagsIndirectCommandNV
{
	deUint32	data;
};

struct VkIndirectCommandsStreamNV
{
	VkBuffer		buffer;
	VkDeviceSize	offset;
};

struct VkIndirectCommandsLayoutTokenNV
{
	VkStructureType					sType;
	const void*						pNext;
	VkIndirectCommandsTokenTypeNV	tokenType;
	deUint32						stream;
	deUint32						offset;
	deUint32						vertexBindingUnit;
	VkBool32						vertexDynamicStride;
	VkPipelineLayout				pushconstantPipelineLayout;
	VkShaderStageFlags				pushconstantShaderStageFlags;
	deUint32						pushconstantOffset;
	deUint32						pushconstantSize;
	VkIndirectStateFlagsNV			indirectStateFlags;
	deUint32						indexTypeCount;
	const VkIndexType*				pIndexTypes;
	const deUint32*					pIndexTypeValues;
};

struct VkIndirectCommandsLayoutCreateInfoNV
{
	VkStructureType							sType;
	const void*								pNext;
	VkIndirectCommandsLayoutUsageFlagsNV	flags;
	VkPipelineBindPoint						pipelineBindPoint;
	deUint32								tokenCount;
	const VkIndirectCommandsLayoutTokenNV*	pTokens;
	deUint32								streamCount;
	const deUint32*							pStreamStrides;
};

struct VkGeneratedCommandsInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkPipelineBindPoint					pipelineBindPoint;
	VkPipeline							pipeline;
	VkIndirectCommandsLayoutNV			indirectCommandsLayout;
	deUint32							streamCount;
	const VkIndirectCommandsStreamNV*	pStreams;
	deUint32							sequencesCount;
	VkBuffer							preprocessBuffer;
	VkDeviceSize						preprocessOffset;
	VkDeviceSize						preprocessSize;
	VkBuffer							sequencesCountBuffer;
	VkDeviceSize						sequencesCountOffset;
	VkBuffer							sequencesIndexBuffer;
	VkDeviceSize						sequencesIndexOffset;
};

struct VkGeneratedCommandsMemoryRequirementsInfoNV
{
	VkStructureType				sType;
	const void*					pNext;
	VkPipelineBindPoint			pipelineBindPoint;
	VkPipeline					pipeline;
	VkIndirectCommandsLayoutNV	indirectCommandsLayout;
	deUint32					maxSequencesCount;
};

struct VkPhysicalDeviceInheritedViewportScissorFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		inheritedViewportScissor2D;
};

struct VkCommandBufferInheritanceViewportScissorInfoNV
{
	VkStructureType		sType;
	const void*			pNext;
	VkBool32			viewportScissor2D;
	deUint32			viewportDepthCount;
	const VkViewport*	pViewportDepths;
};

struct VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		texelBufferAlignment;
};

struct VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceSize	storageTexelBufferOffsetAlignmentBytes;
	VkBool32		storageTexelBufferOffsetSingleTexelAlignment;
	VkDeviceSize	uniformTexelBufferOffsetAlignmentBytes;
	VkBool32		uniformTexelBufferOffsetSingleTexelAlignment;
};

struct VkRenderPassTransformBeginInfoQCOM
{
	VkStructureType					sType;
	void*							pNext;
	VkSurfaceTransformFlagBitsKHR	transform;
};

struct VkCommandBufferInheritanceRenderPassTransformInfoQCOM
{
	VkStructureType					sType;
	void*							pNext;
	VkSurfaceTransformFlagBitsKHR	transform;
	VkRect2D						renderArea;
};

struct VkPhysicalDeviceDeviceMemoryReportFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		deviceMemoryReport;
};

struct VkDeviceMemoryReportCallbackDataEXT
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceMemoryReportFlagsEXT		flags;
	VkDeviceMemoryReportEventTypeEXT	type;
	deUint64							memoryObjectId;
	VkDeviceSize						size;
	VkObjectType						objectType;
	deUint64							objectHandle;
	deUint32							heapIndex;
};

struct VkDeviceDeviceMemoryReportCreateInfoEXT
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceMemoryReportFlagsEXT		flags;
	PFN_vkDeviceMemoryReportCallbackEXT	pfnUserCallback;
	void*								pUserData;
};

struct VkPhysicalDeviceRobustness2FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		robustBufferAccess2;
	VkBool32		robustImageAccess2;
	VkBool32		nullDescriptor;
};

struct VkPhysicalDeviceRobustness2PropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceSize	robustStorageBufferAccessSizeAlignment;
	VkDeviceSize	robustUniformBufferAccessSizeAlignment;
};

struct VkSamplerCustomBorderColorCreateInfoEXT
{
	VkStructureType		sType;
	const void*			pNext;
	VkClearColorValue	customBorderColor;
	VkFormat			format;
};

struct VkPhysicalDeviceCustomBorderColorPropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		maxCustomBorderColorSamplers;
};

struct VkPhysicalDeviceCustomBorderColorFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		customBorderColors;
	VkBool32		customBorderColorWithoutFormat;
};

struct VkPhysicalDevicePrivateDataFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		privateData;
};

struct VkDevicePrivateDataCreateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		privateDataSlotRequestCount;
};

struct VkPrivateDataSlotCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkPrivateDataSlotCreateFlagsEXT	flags;
};

struct VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		pipelineCreationCacheControl;
};

struct VkPhysicalDeviceDiagnosticsConfigFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		diagnosticsConfig;
};

struct VkDeviceDiagnosticsConfigCreateInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceDiagnosticsConfigFlagsNV	flags;
};

struct VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fragmentShadingRateEnums;
	VkBool32		supersampleFragmentShadingRates;
	VkBool32		noInvocationFragmentShadingRates;
};

struct VkPhysicalDeviceFragmentShadingRateEnumsPropertiesNV
{
	VkStructureType			sType;
	void*					pNext;
	VkSampleCountFlagBits	maxFragmentShadingRateInvocationCount;
};

struct VkPipelineFragmentShadingRateEnumStateCreateInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkFragmentShadingRateTypeNV			shadingRateType;
	VkFragmentShadingRateNV				shadingRate;
	VkFragmentShadingRateCombinerOpKHR	combinerOps[2];
};

struct VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		ycbcr2plane444Formats;
};

struct VkPhysicalDeviceFragmentDensityMap2FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fragmentDensityMapDeferred;
};

struct VkPhysicalDeviceFragmentDensityMap2PropertiesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		subsampledLoads;
	VkBool32		subsampledCoarseReconstructionEarlyAccess;
	deUint32		maxSubsampledArrayLayers;
	deUint32		maxDescriptorSetSubsampledSamplers;
};

struct VkCopyCommandTransformInfoQCOM
{
	VkStructureType					sType;
	const void*						pNext;
	VkSurfaceTransformFlagBitsKHR	transform;
};

struct VkPhysicalDeviceImageRobustnessFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		robustImageAccess;
};

struct VkPhysicalDevice4444FormatsFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		formatA4R4G4B4;
	VkBool32		formatA4B4G4R4;
};

struct VkPhysicalDeviceMutableDescriptorTypeFeaturesVALVE
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		mutableDescriptorType;
};

struct VkMutableDescriptorTypeListVALVE
{
	deUint32				descriptorTypeCount;
	const VkDescriptorType*	pDescriptorTypes;
};

struct VkMutableDescriptorTypeCreateInfoVALVE
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								mutableDescriptorTypeListCount;
	const VkMutableDescriptorTypeListVALVE*	pMutableDescriptorTypeLists;
};

struct VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		vertexInputDynamicState;
};

struct VkVertexInputBindingDescription2EXT
{
	VkStructureType		sType;
	void*				pNext;
	deUint32			binding;
	deUint32			stride;
	VkVertexInputRate	inputRate;
	deUint32			divisor;
};

struct VkVertexInputAttributeDescription2EXT
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		location;
	deUint32		binding;
	VkFormat		format;
	deUint32		offset;
};

struct VkPhysicalDeviceExtendedDynamicState2FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		extendedDynamicState2;
	VkBool32		extendedDynamicState2LogicOp;
	VkBool32		extendedDynamicState2PatchControlPoints;
};

struct VkPhysicalDeviceColorWriteEnableFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		colorWriteEnable;
};

struct VkPipelineColorWriteCreateInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		attachmentCount;
	const VkBool32*	pColorWriteEnables;
};

union VkDeviceOrHostAddressKHR
{
	VkDeviceAddress	deviceAddress;
	void*			hostAddress;
};

union VkDeviceOrHostAddressConstKHR
{
	VkDeviceAddress	deviceAddress;
	const void*		hostAddress;
};

struct VkAccelerationStructureBuildRangeInfoKHR
{
	deUint32	primitiveCount;
	deUint32	primitiveOffset;
	deUint32	firstVertex;
	deUint32	transformOffset;
};

struct VkAccelerationStructureGeometryTrianglesDataKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkFormat						vertexFormat;
	VkDeviceOrHostAddressConstKHR	vertexData;
	VkDeviceSize					vertexStride;
	deUint32						maxVertex;
	VkIndexType						indexType;
	VkDeviceOrHostAddressConstKHR	indexData;
	VkDeviceOrHostAddressConstKHR	transformData;
};

struct VkAccelerationStructureGeometryAabbsDataKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkDeviceOrHostAddressConstKHR	data;
	VkDeviceSize					stride;
};

struct VkAccelerationStructureGeometryInstancesDataKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkBool32						arrayOfPointers;
	VkDeviceOrHostAddressConstKHR	data;
};

union VkAccelerationStructureGeometryDataKHR
{
	VkAccelerationStructureGeometryTrianglesDataKHR	triangles;
	VkAccelerationStructureGeometryAabbsDataKHR		aabbs;
	VkAccelerationStructureGeometryInstancesDataKHR	instances;
};

struct VkAccelerationStructureGeometryKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkGeometryTypeKHR						geometryType;
	VkAccelerationStructureGeometryDataKHR	geometry;
	VkGeometryFlagsKHR						flags;
};

struct VkAccelerationStructureBuildGeometryInfoKHR
{
	VkStructureType										sType;
	const void*											pNext;
	VkAccelerationStructureTypeKHR						type;
	VkBuildAccelerationStructureFlagsKHR				flags;
	VkBuildAccelerationStructureModeKHR					mode;
	VkAccelerationStructureKHR							srcAccelerationStructure;
	VkAccelerationStructureKHR							dstAccelerationStructure;
	deUint32											geometryCount;
	const VkAccelerationStructureGeometryKHR*			pGeometries;
	const VkAccelerationStructureGeometryKHR* const*	ppGeometries;
	VkDeviceOrHostAddressKHR							scratchData;
};

struct VkAccelerationStructureCreateInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkAccelerationStructureCreateFlagsKHR	createFlags;
	VkBuffer								buffer;
	VkDeviceSize							offset;
	VkDeviceSize							size;
	VkAccelerationStructureTypeKHR			type;
	VkDeviceAddress							deviceAddress;
};

struct VkWriteDescriptorSetAccelerationStructureKHR
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							accelerationStructureCount;
	const VkAccelerationStructureKHR*	pAccelerationStructures;
};

struct VkPhysicalDeviceAccelerationStructureFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		accelerationStructure;
	VkBool32		accelerationStructureCaptureReplay;
	VkBool32		accelerationStructureIndirectBuild;
	VkBool32		accelerationStructureHostCommands;
	VkBool32		descriptorBindingAccelerationStructureUpdateAfterBind;
};

struct VkPhysicalDeviceAccelerationStructurePropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint64		maxGeometryCount;
	deUint64		maxInstanceCount;
	deUint64		maxPrimitiveCount;
	deUint32		maxPerStageDescriptorAccelerationStructures;
	deUint32		maxPerStageDescriptorUpdateAfterBindAccelerationStructures;
	deUint32		maxDescriptorSetAccelerationStructures;
	deUint32		maxDescriptorSetUpdateAfterBindAccelerationStructures;
	deUint32		minAccelerationStructureScratchOffsetAlignment;
};

struct VkAccelerationStructureDeviceAddressInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkAccelerationStructureKHR	accelerationStructure;
};

struct VkAccelerationStructureVersionInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	const deUint8*	pVersionData;
};

struct VkCopyAccelerationStructureToMemoryInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkAccelerationStructureKHR			src;
	VkDeviceOrHostAddressKHR			dst;
	VkCopyAccelerationStructureModeKHR	mode;
};

struct VkCopyMemoryToAccelerationStructureInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceOrHostAddressConstKHR		src;
	VkAccelerationStructureKHR			dst;
	VkCopyAccelerationStructureModeKHR	mode;
};

struct VkCopyAccelerationStructureInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkAccelerationStructureKHR			src;
	VkAccelerationStructureKHR			dst;
	VkCopyAccelerationStructureModeKHR	mode;
};

struct VkAccelerationStructureBuildSizesInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceSize	accelerationStructureSize;
	VkDeviceSize	updateScratchSize;
	VkDeviceSize	buildScratchSize;
};

struct VkRayTracingShaderGroupCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkRayTracingShaderGroupTypeKHR	type;
	deUint32						generalShader;
	deUint32						closestHitShader;
	deUint32						anyHitShader;
	deUint32						intersectionShader;
	const void*						pShaderGroupCaptureReplayHandle;
};

struct VkRayTracingPipelineInterfaceCreateInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		maxPipelineRayPayloadSize;
	deUint32		maxPipelineRayHitAttributeSize;
};

struct VkRayTracingPipelineCreateInfoKHR
{
	VkStructureType										sType;
	const void*											pNext;
	VkPipelineCreateFlags								flags;
	deUint32											stageCount;
	const VkPipelineShaderStageCreateInfo*				pStages;
	deUint32											groupCount;
	const VkRayTracingShaderGroupCreateInfoKHR*			pGroups;
	deUint32											maxPipelineRayRecursionDepth;
	const VkPipelineLibraryCreateInfoKHR*				pLibraryInfo;
	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryInterface;
	const VkPipelineDynamicStateCreateInfo*				pDynamicState;
	VkPipelineLayout									layout;
	VkPipeline											basePipelineHandle;
	deInt32												basePipelineIndex;
};

struct VkPhysicalDeviceRayTracingPipelineFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		rayTracingPipeline;
	VkBool32		rayTracingPipelineShaderGroupHandleCaptureReplay;
	VkBool32		rayTracingPipelineShaderGroupHandleCaptureReplayMixed;
	VkBool32		rayTracingPipelineTraceRaysIndirect;
	VkBool32		rayTraversalPrimitiveCulling;
};

struct VkPhysicalDeviceRayTracingPipelinePropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		shaderGroupHandleSize;
	deUint32		maxRayRecursionDepth;
	deUint32		maxShaderGroupStride;
	deUint32		shaderGroupBaseAlignment;
	deUint32		shaderGroupHandleCaptureReplaySize;
	deUint32		maxRayDispatchInvocationCount;
	deUint32		shaderGroupHandleAlignment;
	deUint32		maxRayHitAttributeSize;
};

struct VkStridedDeviceAddressRegionKHR
{
	VkDeviceAddress	deviceAddress;
	VkDeviceSize	stride;
	VkDeviceSize	size;
};

struct VkTraceRaysIndirectCommandKHR
{
	deUint32	width;
	deUint32	height;
	deUint32	depth;
};

struct VkPhysicalDeviceRayQueryFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		rayQuery;
};

struct VkAndroidSurfaceCreateInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkAndroidSurfaceCreateFlagsKHR		flags;
	struct pt::AndroidNativeWindowPtr	window;
};

struct VkAndroidHardwareBufferUsageANDROID
{
	VkStructureType	sType;
	void*			pNext;
	deUint64		androidHardwareBufferUsage;
};

struct VkAndroidHardwareBufferPropertiesANDROID
{
	VkStructureType	sType;
	void*			pNext;
	VkDeviceSize	allocationSize;
	deUint32		memoryTypeBits;
};

struct VkAndroidHardwareBufferFormatPropertiesANDROID
{
	VkStructureType					sType;
	void*							pNext;
	VkFormat						format;
	deUint64						externalFormat;
	VkFormatFeatureFlags			formatFeatures;
	VkComponentMapping				samplerYcbcrConversionComponents;
	VkSamplerYcbcrModelConversion	suggestedYcbcrModel;
	VkSamplerYcbcrRange				suggestedYcbcrRange;
	VkChromaLocation				suggestedXChromaOffset;
	VkChromaLocation				suggestedYChromaOffset;
};

struct VkImportAndroidHardwareBufferInfoANDROID
{
	VkStructureType						sType;
	const void*							pNext;
	struct pt::AndroidHardwareBufferPtr	buffer;
};

struct VkMemoryGetAndroidHardwareBufferInfoANDROID
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	memory;
};

struct VkExternalFormatANDROID
{
	VkStructureType	sType;
	void*			pNext;
	deUint64		externalFormat;
};

struct VkVideoQueueFamilyProperties2KHR
{
	VkStructureType					sType;
	void*							pNext;
	VkVideoCodecOperationFlagsKHR	videoCodecOperations;
};

struct VkVideoProfileKHR
{
	VkStructureType						sType;
	void*								pNext;
	VkVideoCodecOperationFlagBitsKHR	videoCodecOperation;
	VkVideoChromaSubsamplingFlagsKHR	chromaSubsampling;
	VkVideoComponentBitDepthFlagsKHR	lumaBitDepth;
	VkVideoComponentBitDepthFlagsKHR	chromaBitDepth;
};

struct VkVideoProfilesKHR
{
	VkStructureType				sType;
	void*						pNext;
	deUint32					profileCount;
	const VkVideoProfileKHR*	pProfiles;
};

struct VkVideoCapabilitiesKHR
{
	VkStructureType				sType;
	void*						pNext;
	VkVideoCapabilitiesFlagsKHR	capabilityFlags;
	VkDeviceSize				minBitstreamBufferOffsetAlignment;
	VkDeviceSize				minBitstreamBufferSizeAlignment;
	VkExtent2D					videoPictureExtentGranularity;
	VkExtent2D					minExtent;
	VkExtent2D					maxExtent;
	deUint32					maxReferencePicturesSlotsCount;
	deUint32					maxReferencePicturesActiveCount;
};

struct VkPhysicalDeviceVideoFormatInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkImageUsageFlags			imageUsage;
	const VkVideoProfilesKHR*	pVideoProfiles;
};

struct VkVideoFormatPropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkFormat		format;
};

struct VkVideoPictureResourceKHR
{
	VkStructureType	sType;
	const void*		pNext;
	VkOffset2D		codedOffset;
	VkExtent2D		codedExtent;
	deUint32		baseArrayLayer;
	VkImageView		imageViewBinding;
};

struct VkVideoReferenceSlotKHR
{
	VkStructureType						sType;
	const void*							pNext;
	deInt8								slotIndex;
	const VkVideoPictureResourceKHR*	pPictureResource;
};

struct VkVideoGetMemoryPropertiesKHR
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				memoryBindIndex;
	VkMemoryRequirements2*	pMemoryRequirements;
};

struct VkVideoBindMemoryKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		memoryBindIndex;
	VkDeviceMemory	memory;
	VkDeviceSize	memoryOffset;
	VkDeviceSize	memorySize;
};

struct VkVideoSessionCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						queueFamilyIndex;
	VkVideoSessionCreateFlagsKHR	flags;
	const VkVideoProfileKHR*		pVideoProfile;
	VkFormat						pictureFormat;
	VkExtent2D						maxCodedExtent;
	VkFormat						referencePicturesFormat;
	deUint32						maxReferencePicturesSlotsCount;
	deUint32						maxReferencePicturesActiveCount;
};

struct VkVideoSessionParametersCreateInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkVideoSessionParametersKHR	videoSessionParametersTemplate;
	VkVideoSessionKHR			videoSession;
};

struct VkVideoSessionParametersUpdateInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		updateSequenceCount;
};

struct VkVideoBeginCodingInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkVideoBeginCodingFlagsKHR			flags;
	VkVideoCodingQualityPresetFlagsKHR	codecQualityPreset;
	VkVideoSessionKHR					videoSession;
	VkVideoSessionParametersKHR			videoSessionParameters;
	deUint32							referenceSlotCount;
	const VkVideoReferenceSlotKHR*		pReferenceSlots;
};

struct VkVideoEndCodingInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkVideoEndCodingFlagsKHR	flags;
};

struct VkVideoCodingControlInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoCodingControlFlagsKHR	flags;
};

struct VkVideoDecodeInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoDecodeFlagsKHR			flags;
	VkOffset2D						codedOffset;
	VkExtent2D						codedExtent;
	VkBuffer						srcBuffer;
	VkDeviceSize					srcBufferOffset;
	VkDeviceSize					srcBufferRange;
	VkVideoPictureResourceKHR		dstPictureResource;
	const VkVideoReferenceSlotKHR*	pSetupReferenceSlot;
	deUint32						referenceSlotCount;
	const VkVideoReferenceSlotKHR*	pReferenceSlots;
};

struct VkPhysicalDevicePortabilitySubsetFeaturesKHR
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		constantAlphaColorBlendFactors;
	VkBool32		events;
	VkBool32		imageViewFormatReinterpretation;
	VkBool32		imageViewFormatSwizzle;
	VkBool32		imageView2DOn3DImage;
	VkBool32		multisampleArrayImage;
	VkBool32		mutableComparisonSamplers;
	VkBool32		pointPolygons;
	VkBool32		samplerMipLodBias;
	VkBool32		separateStencilMaskRef;
	VkBool32		shaderSampleRateInterpolationFunctions;
	VkBool32		tessellationIsolines;
	VkBool32		tessellationPointMode;
	VkBool32		triangleFans;
	VkBool32		vertexAttributeAccessBeyondStride;
};

struct VkPhysicalDevicePortabilitySubsetPropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		minVertexInputBindingStrideAlignment;
};

struct VkVideoEncodeInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoEncodeFlagsKHR			flags;
	deUint32						qualityLevel;
	VkExtent2D						codedExtent;
	VkBuffer						dstBitstreamBuffer;
	VkDeviceSize					dstBitstreamBufferOffset;
	VkDeviceSize					dstBitstreamBufferMaxRange;
	VkVideoPictureResourceKHR		srcPictureResource;
	const VkVideoReferenceSlotKHR*	pSetupReferenceSlot;
	deUint32						referenceSlotCount;
	const VkVideoReferenceSlotKHR*	pReferenceSlots;
};

struct VkVideoEncodeRateControlInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkVideoEncodeRateControlFlagsKHR		flags;
	VkVideoEncodeRateControlModeFlagBitsKHR	rateControlMode;
	deUint32								averageBitrate;
	deUint16								peakToAverageBitrateRatio;
	deUint16								frameRateNumerator;
	deUint16								frameRateDenominator;
	deUint32								virtualBufferSizeInMs;
};

struct StdVideoH264SpsVuiFlags
{
	deUint32	aspect_ratio_info_present_flag:1;
	deUint32	overscan_info_present_flag:1;
	deUint32	overscan_appropriate_flag:1;
	deUint32	video_signal_type_present_flag:1;
	deUint32	video_full_range_flag:1;
	deUint32	color_description_present_flag:1;
	deUint32	chroma_loc_info_present_flag:1;
	deUint32	timing_info_present_flag:1;
	deUint32	fixed_frame_rate_flag:1;
	deUint32	bitstream_restriction_flag:1;
	deUint32	nal_hrd_parameters_present_flag:1;
	deUint32	vcl_hrd_parameters_present_flag:1;
};

struct StdVideoH264HrdParameters
{
	deUint8		cpb_cnt_minus1;
	deUint8		bit_rate_scale;
	deUint8		cpb_size_scale;
	deUint32	bit_rate_value_minus1[32];
	deUint32	cpb_size_value_minus1[32];
	deUint8		cbr_flag[32];
	deUint32	initial_cpb_removal_delay_length_minus1;
	deUint32	cpb_removal_delay_length_minus1;
	deUint32	dpb_output_delay_length_minus1;
	deUint32	time_offset_length;
};

struct StdVideoH264SequenceParameterSetVui
{
	StdVideoH264AspectRatioIdc	aspect_ratio_idc;
	deUint16					sar_width;
	deUint16					sar_height;
	deUint8						video_format;
	deUint8						color_primaries;
	deUint8						transfer_characteristics;
	deUint8						matrix_coefficients;
	deUint32					num_units_in_tick;
	deUint32					time_scale;
	StdVideoH264HrdParameters	hrd_parameters;
	deUint8						num_reorder_frames;
	deUint8						max_dec_frame_buffering;
	StdVideoH264SpsVuiFlags		flags;
};

struct StdVideoH264SpsFlags
{
	deUint32	constraint_set0_flag:1;
	deUint32	constraint_set1_flag:1;
	deUint32	constraint_set2_flag:1;
	deUint32	constraint_set3_flag:1;
	deUint32	constraint_set4_flag:1;
	deUint32	constraint_set5_flag:1;
	deUint32	direct_8x8_inference_flag:1;
	deUint32	mb_adaptive_frame_field_flag:1;
	deUint32	frame_mbs_only_flag:1;
	deUint32	delta_pic_order_always_zero_flag:1;
	deUint32	residual_colour_transform_flag:1;
	deUint32	gaps_in_frame_num_value_allowed_flag:1;
	deUint32	first_picture_after_seek_flag:1;
	deUint32	qpprime_y_zero_transform_bypass_flag:1;
	deUint32	frame_cropping_flag:1;
	deUint32	scaling_matrix_present_flag:1;
	deUint32	vui_parameters_present_flag:1;
};

struct StdVideoH264ScalingLists
{
	deUint8	scaling_list_present_mask;
	deUint8	use_default_scaling_matrix_mask;
	deUint8	ScalingList4x4[6][16];
	deUint8	ScalingList8x8[2][64];
};

struct StdVideoH264SequenceParameterSet
{
	StdVideoH264ProfileIdc					profile_idc;
	StdVideoH264Level						level_idc;
	deUint8									seq_parameter_set_id;
	StdVideoH264ChromaFormatIdc				chroma_format_idc;
	deUint8									bit_depth_luma_minus8;
	deUint8									bit_depth_chroma_minus8;
	deUint8									log2_max_frame_num_minus4;
	StdVideoH264PocType						pic_order_cnt_type;
	deUint8									log2_max_pic_order_cnt_lsb_minus4;
	deInt32									offset_for_non_ref_pic;
	deInt32									offset_for_top_to_bottom_field;
	deUint8									num_ref_frames_in_pic_order_cnt_cycle;
	deUint8									max_num_ref_frames;
	deUint32								pic_width_in_mbs_minus1;
	deUint32								pic_height_in_map_units_minus1;
	deUint32								frame_crop_left_offset;
	deUint32								frame_crop_right_offset;
	deUint32								frame_crop_top_offset;
	deUint32								frame_crop_bottom_offset;
	StdVideoH264SpsFlags					flags;
	deInt32									offset_for_ref_frame[255];
	StdVideoH264ScalingLists*				pScalingLists;
	StdVideoH264SequenceParameterSetVui*	pSequenceParameterSetVui;
};

struct StdVideoH264PpsFlags
{
	deUint32	transform_8x8_mode_flag:1;
	deUint32	redundant_pic_cnt_present_flag:1;
	deUint32	constrained_intra_pred_flag:1;
	deUint32	deblocking_filter_control_present_flag:1;
	deUint32	weighted_bipred_idc_flag:1;
	deUint32	weighted_pred_flag:1;
	deUint32	pic_order_present_flag:1;
	deUint32	entropy_coding_mode_flag:1;
	deUint32	scaling_matrix_present_flag:1;
};

struct StdVideoH264PictureParameterSet
{
	deUint8							seq_parameter_set_id;
	deUint8							pic_parameter_set_id;
	deUint8							num_ref_idx_l0_default_active_minus1;
	deUint8							num_ref_idx_l1_default_active_minus1;
	StdVideoH264WeightedBiPredIdc	weighted_bipred_idc;
	deInt8							pic_init_qp_minus26;
	deInt8							pic_init_qs_minus26;
	deInt8							chroma_qp_index_offset;
	deInt8							second_chroma_qp_index_offset;
	StdVideoH264PpsFlags			flags;
	StdVideoH264ScalingLists*		pScalingLists;
};

struct StdVideoEncodeH264SliceHeaderFlags
{
	deUint32	idr_flag:1;
	deUint32	is_reference_flag:1;
	deUint32	num_ref_idx_active_override_flag:1;
	deUint32	no_output_of_prior_pics_flag:1;
	deUint32	long_term_reference_flag:1;
	deUint32	adaptive_ref_pic_marking_mode_flag:1;
	deUint32	no_prior_references_available_flag:1;
};

struct StdVideoEncodeH264PictureInfoFlags
{
	deUint32	idr_flag:1;
	deUint32	is_reference_flag:1;
	deUint32	long_term_reference_flag:1;
};

struct StdVideoEncodeH264RefMgmtFlags
{
	deUint32	ref_pic_list_modification_l0_flag:1;
	deUint32	ref_pic_list_modification_l1_flag:1;
};

struct StdVideoEncodeH264RefListModEntry
{
	StdVideoH264ModificationOfPicNumsIdc	modification_of_pic_nums_idc;
	deUint16								abs_diff_pic_num_minus1;
	deUint16								long_term_pic_num;
};

struct StdVideoEncodeH264RefPicMarkingEntry
{
	StdVideoH264MemMgmtControlOp	operation;
	deUint16						difference_of_pic_nums_minus1;
	deUint16						long_term_pic_num;
	deUint16						long_term_frame_idx;
	deUint16						max_long_term_frame_idx_plus1;
};

struct StdVideoEncodeH264RefMemMgmtCtrlOperations
{
	StdVideoEncodeH264RefMgmtFlags			flags;
	deUint8									refList0ModOpCount;
	StdVideoEncodeH264RefListModEntry*		pRefList0ModOperations;
	deUint8									refList1ModOpCount;
	StdVideoEncodeH264RefListModEntry*		pRefList1ModOperations;
	deUint8									refPicMarkingOpCount;
	StdVideoEncodeH264RefPicMarkingEntry*	pRefPicMarkingOperations;
};

struct StdVideoEncodeH264PictureInfo
{
	StdVideoEncodeH264PictureInfoFlags	flags;
	StdVideoH264PictureType				pictureType;
	deUint32							frameNum;
	deUint32							pictureOrderCount;
	deUint16							long_term_pic_num;
	deUint16							long_term_frame_idx;
};

struct StdVideoEncodeH264SliceHeader
{
	StdVideoEncodeH264SliceHeaderFlags			flags;
	StdVideoH264SliceType						slice_type;
	deUint8										seq_parameter_set_id;
	deUint8										pic_parameter_set_id;
	deUint16									idr_pic_id;
	deUint8										num_ref_idx_l0_active_minus1;
	deUint8										num_ref_idx_l1_active_minus1;
	StdVideoH264CabacInitIdc					cabac_init_idc;
	StdVideoH264DisableDeblockingFilterIdc		disable_deblocking_filter_idc;
	deInt8										slice_alpha_c0_offset_div2;
	deInt8										slice_beta_offset_div2;
	StdVideoEncodeH264RefMemMgmtCtrlOperations*	pMemMgmtCtrlOperations;
};

struct VkVideoEncodeH264CapabilitiesEXT
{
	VkStructureType							sType;
	const void*								pNext;
	VkVideoEncodeH264CapabilitiesFlagsEXT	flags;
	VkVideoEncodeH264InputModeFlagsEXT		inputModeFlags;
	VkVideoEncodeH264OutputModeFlagsEXT		outputModeFlags;
	VkExtent2D								minPictureSizeInMbs;
	VkExtent2D								maxPictureSizeInMbs;
	VkExtent2D								inputImageDataAlignment;
	deUint8									maxNumL0ReferenceForP;
	deUint8									maxNumL0ReferenceForB;
	deUint8									maxNumL1Reference;
	deUint8									qualityLevelCount;
	VkExtensionProperties					stdExtensionVersion;
};

struct VkVideoEncodeH264SessionCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoEncodeH264CreateFlagsEXT	flags;
	VkExtent2D						maxPictureSizeInMbs;
	const VkExtensionProperties*	pStdExtensionVersion;
};

struct VkVideoEncodeH264SessionParametersAddInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								spsStdCount;
	const StdVideoH264SequenceParameterSet*	pSpsStd;
	deUint32								ppsStdCount;
	const StdVideoH264PictureParameterSet*	pPpsStd;
};

struct VkVideoEncodeH264SessionParametersCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	deUint32											maxSpsStdCount;
	deUint32											maxPpsStdCount;
	const VkVideoEncodeH264SessionParametersAddInfoEXT*	pParametersAddInfo;
};

struct VkVideoEncodeH264DpbSlotInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deInt8									slotIndex;
	const StdVideoEncodeH264PictureInfo*	pStdPictureInfo;
};

struct VkVideoEncodeH264NaluSliceEXT
{
	VkStructureType							sType;
	const void*								pNext;
	const StdVideoEncodeH264SliceHeader*	pSliceHeaderStd;
	deUint32								mbCount;
	deUint8									refFinalList0EntryCount;
	const VkVideoEncodeH264DpbSlotInfoEXT*	pRefFinalList0Entries;
	deUint8									refFinalList1EntryCount;
	const VkVideoEncodeH264DpbSlotInfoEXT*	pRefFinalList1Entries;
	deUint32								precedingNaluBytes;
	deUint8									minQp;
	deUint8									maxQp;
};

struct VkVideoEncodeH264VclFrameInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint8									refDefaultFinalList0EntryCount;
	const VkVideoEncodeH264DpbSlotInfoEXT*	pRefDefaultFinalList0Entries;
	deUint8									refDefaultFinalList1EntryCount;
	const VkVideoEncodeH264DpbSlotInfoEXT*	pRefDefaultFinalList1Entries;
	deUint32								naluSliceEntryCount;
	const VkVideoEncodeH264NaluSliceEXT*	pNaluSliceEntries;
	const VkVideoEncodeH264DpbSlotInfoEXT*	pCurrentPictureInfo;
};

struct VkVideoEncodeH264EmitPictureParametersEXT
{
	VkStructureType	sType;
	const void*		pNext;
	deUint8			spsId;
	VkBool32		emitSpsEnable;
	deUint32		ppsIdEntryCount;
	const deUint8*	ppsIdEntries;
};

struct VkVideoEncodeH264ProfileEXT
{
	VkStructureType			sType;
	const void*				pNext;
	StdVideoH264ProfileIdc	stdProfileIdc;
};

struct StdVideoDecodeH264PictureInfoFlags
{
	deUint32	field_pic_flag:1;
	deUint32	is_intra:1;
	deUint32	bottom_field_flag:1;
	deUint32	is_reference:1;
	deUint32	complementary_field_pair:1;
};

struct StdVideoDecodeH264PictureInfo
{
	deUint8								seq_parameter_set_id;
	deUint8								pic_parameter_set_id;
	deUint16							reserved;
	deUint16							frame_num;
	deUint16							idr_pic_id;
	deInt32								PicOrderCnt[2];
	StdVideoDecodeH264PictureInfoFlags	flags;
};

struct StdVideoDecodeH264ReferenceInfoFlags
{
	deUint32	top_field_flag:1;
	deUint32	bottom_field_flag:1;
	deUint32	is_long_term:1;
	deUint32	is_non_existing:1;
};

struct StdVideoDecodeH264ReferenceInfo
{
	deUint16								FrameNum;
	deUint16								reserved;
	deInt32									PicOrderCnt[2];
	StdVideoDecodeH264ReferenceInfoFlags	flags;
};

struct StdVideoDecodeH264MvcElementFlags
{
	deUint32	non_idr:1;
	deUint32	anchor_pic:1;
	deUint32	inter_view:1;
};

struct StdVideoDecodeH264MvcElement
{
	StdVideoDecodeH264MvcElementFlags	flags;
	deUint16							viewOrderIndex;
	deUint16							viewId;
	deUint16							temporalId;
	deUint16							priorityId;
	deUint16							numOfAnchorRefsInL0;
	deUint16							viewIdOfAnchorRefsInL0[15];
	deUint16							numOfAnchorRefsInL1;
	deUint16							viewIdOfAnchorRefsInL1[15];
	deUint16							numOfNonAnchorRefsInL0;
	deUint16							viewIdOfNonAnchorRefsInL0[15];
	deUint16							numOfNonAnchorRefsInL1;
	deUint16							viewIdOfNonAnchorRefsInL1[15];
};

struct StdVideoDecodeH264Mvc
{
	deUint32						viewId0;
	deUint32						mvcElementCount;
	StdVideoDecodeH264MvcElement*	pMvcElements;
};

struct VkVideoDecodeH264ProfileEXT
{
	VkStructureType							sType;
	const void*								pNext;
	StdVideoH264ProfileIdc					stdProfileIdc;
	VkVideoDecodeH264FieldLayoutFlagsEXT	fieldLayout;
};

struct VkVideoDecodeH264CapabilitiesEXT
{
	VkStructureType			sType;
	void*					pNext;
	deUint32				maxLevel;
	VkOffset2D				fieldOffsetGranularity;
	VkExtensionProperties	stdExtensionVersion;
};

struct VkVideoDecodeH264SessionCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoDecodeH264CreateFlagsEXT	flags;
	const VkExtensionProperties*	pStdExtensionVersion;
};

struct VkVideoDecodeH264SessionParametersAddInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								spsStdCount;
	const StdVideoH264SequenceParameterSet*	pSpsStd;
	deUint32								ppsStdCount;
	const StdVideoH264PictureParameterSet*	pPpsStd;
};

struct VkVideoDecodeH264SessionParametersCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	deUint32											maxSpsStdCount;
	deUint32											maxPpsStdCount;
	const VkVideoDecodeH264SessionParametersAddInfoEXT*	pParametersAddInfo;
};

struct VkVideoDecodeH264PictureInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	const StdVideoDecodeH264PictureInfo*	pStdPictureInfo;
	deUint32								slicesCount;
	const deUint32*							pSlicesDataOffsets;
};

struct VkVideoDecodeH264MvcEXT
{
	VkStructureType					sType;
	const void*						pNext;
	const StdVideoDecodeH264Mvc*	pStdMvc;
};

struct VkVideoDecodeH264DpbSlotInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	const StdVideoDecodeH264ReferenceInfo*	pStdReferenceInfo;
};

struct StdVideoH265DecPicBufMgr
{
	deUint32	max_latency_increase_plus1[7];
	deUint8		max_dec_pic_buffering_minus1[7];
	deUint8		max_num_reorder_pics[7];
};

struct StdVideoH265SubLayerHrdParameters
{
	deUint32	bit_rate_value_minus1[32];
	deUint32	cpb_size_value_minus1[32];
	deUint32	cpb_size_du_value_minus1[32];
	deUint32	bit_rate_du_value_minus1[32];
	deUint32	cbr_flag;
};

struct StdVideoH265HrdFlags
{
	deUint32	nal_hrd_parameters_present_flag:1;
	deUint32	vcl_hrd_parameters_present_flag:1;
	deUint32	sub_pic_hrd_params_present_flag:1;
	deUint32	sub_pic_cpb_params_in_pic_timing_sei_flag:1;
	deUint8		fixed_pic_rate_general_flag;
	deUint8		fixed_pic_rate_within_cvs_flag;
	deUint8		low_delay_hrd_flag;
};

struct StdVideoH265HrdParameters
{
	deUint8								tick_divisor_minus2;
	deUint8								du_cpb_removal_delay_increment_length_minus1;
	deUint8								dpb_output_delay_du_length_minus1;
	deUint8								bit_rate_scale;
	deUint8								cpb_size_scale;
	deUint8								cpb_size_du_scale;
	deUint8								initial_cpb_removal_delay_length_minus1;
	deUint8								au_cpb_removal_delay_length_minus1;
	deUint8								dpb_output_delay_length_minus1;
	deUint8								cpb_cnt_minus1[7];
	deUint16							elemental_duration_in_tc_minus1[7];
	StdVideoH265SubLayerHrdParameters*	SubLayerHrdParametersNal[7];
	StdVideoH265SubLayerHrdParameters*	SubLayerHrdParametersVcl[7];
	StdVideoH265HrdFlags				flags;
};

struct StdVideoH265VpsFlags
{
	deUint32	vps_temporal_id_nesting_flag:1;
	deUint32	vps_sub_layer_ordering_info_present_flag:1;
	deUint32	vps_timing_info_present_flag:1;
	deUint32	vps_poc_proportional_to_timing_flag:1;
};

struct StdVideoH265VideoParameterSet
{
	deUint8						vps_video_parameter_set_id;
	deUint8						vps_max_sub_layers_minus1;
	deUint32					vps_num_units_in_tick;
	deUint32					vps_time_scale;
	deUint32					vps_num_ticks_poc_diff_one_minus1;
	StdVideoH265DecPicBufMgr*	pDecPicBufMgr;
	StdVideoH265HrdParameters*	hrd_parameters;
	StdVideoH265VpsFlags		flags;
};

struct StdVideoH265ScalingLists
{
	deUint8	ScalingList4x4[6][16];
	deUint8	ScalingList8x8[6][64];
	deUint8	ScalingList16x16[6][64];
	deUint8	ScalingList32x32[2][64];
	deUint8	ScalingListDCCoef16x16[6];
	deUint8	ScalingListDCCoef32x32[2];
};

struct StdVideoH265SpsVuiFlags
{
	deUint32	aspect_ratio_info_present_flag:1;
	deUint32	overscan_info_present_flag:1;
	deUint32	overscan_appropriate_flag:1;
	deUint32	video_signal_type_present_flag:1;
	deUint32	video_full_range_flag:1;
	deUint32	colour_description_present_flag:1;
	deUint32	chroma_loc_info_present_flag:1;
	deUint32	neutral_chroma_indication_flag:1;
	deUint32	field_seq_flag:1;
	deUint32	frame_field_info_present_flag:1;
	deUint32	default_display_window_flag:1;
	deUint32	vui_timing_info_present_flag:1;
	deUint32	vui_poc_proportional_to_timing_flag:1;
	deUint32	vui_hrd_parameters_present_flag:1;
	deUint32	bitstream_restriction_flag:1;
	deUint32	tiles_fixed_structure_flag:1;
	deUint32	motion_vectors_over_pic_boundaries_flag:1;
	deUint32	restricted_ref_pic_lists_flag:1;
};

struct StdVideoH265SequenceParameterSetVui
{
	deUint8						aspect_ratio_idc;
	deUint16					sar_width;
	deUint16					sar_height;
	deUint8						video_format;
	deUint8						colour_primaries;
	deUint8						transfer_characteristics;
	deUint8						matrix_coeffs;
	deUint8						chroma_sample_loc_type_top_field;
	deUint8						chroma_sample_loc_type_bottom_field;
	deUint16					def_disp_win_left_offset;
	deUint16					def_disp_win_right_offset;
	deUint16					def_disp_win_top_offset;
	deUint16					def_disp_win_bottom_offset;
	deUint32					vui_num_units_in_tick;
	deUint32					vui_time_scale;
	deUint32					vui_num_ticks_poc_diff_one_minus1;
	StdVideoH265HrdParameters*	hrd_parameters;
	deUint16					min_spatial_segmentation_idc;
	deUint8						max_bytes_per_pic_denom;
	deUint8						max_bits_per_min_cu_denom;
	deUint8						log2_max_mv_length_horizontal;
	deUint8						log2_max_mv_length_vertical;
	StdVideoH265SpsVuiFlags		flags;
};

struct StdVideoH265PredictorPaletteEntries
{
	deUint16	PredictorPaletteEntries[3][128];
};

struct StdVideoH265SpsFlags
{
	deUint32	sps_temporal_id_nesting_flag:1;
	deUint32	separate_colour_plane_flag:1;
	deUint32	scaling_list_enabled_flag:1;
	deUint32	sps_scaling_list_data_present_flag:1;
	deUint32	amp_enabled_flag:1;
	deUint32	sample_adaptive_offset_enabled_flag:1;
	deUint32	pcm_enabled_flag:1;
	deUint32	pcm_loop_filter_disabled_flag:1;
	deUint32	long_term_ref_pics_present_flag:1;
	deUint32	sps_temporal_mvp_enabled_flag:1;
	deUint32	strong_intra_smoothing_enabled_flag:1;
	deUint32	vui_parameters_present_flag:1;
	deUint32	sps_extension_present_flag:1;
	deUint32	sps_range_extension_flag:1;
	deUint32	transform_skip_rotation_enabled_flag:1;
	deUint32	transform_skip_context_enabled_flag:1;
	deUint32	implicit_rdpcm_enabled_flag:1;
	deUint32	explicit_rdpcm_enabled_flag:1;
	deUint32	extended_precision_processing_flag:1;
	deUint32	intra_smoothing_disabled_flag:1;
	deUint32	high_precision_offsets_enabled_flag:1;
	deUint32	persistent_rice_adaptation_enabled_flag:1;
	deUint32	cabac_bypass_alignment_enabled_flag:1;
	deUint32	sps_curr_pic_ref_enabled_flag:1;
	deUint32	palette_mode_enabled_flag:1;
	deUint32	sps_palette_predictor_initializer_present_flag:1;
	deUint32	intra_boundary_filtering_disabled_flag:1;
};

struct StdVideoH265SequenceParameterSet
{
	StdVideoH265ProfileIdc					profile_idc;
	StdVideoH265Level						level_idc;
	deUint32								pic_width_in_luma_samples;
	deUint32								pic_height_in_luma_samples;
	deUint8									sps_video_parameter_set_id;
	deUint8									sps_max_sub_layers_minus1;
	deUint8									sps_seq_parameter_set_id;
	deUint8									chroma_format_idc;
	deUint8									bit_depth_luma_minus8;
	deUint8									bit_depth_chroma_minus8;
	deUint8									log2_max_pic_order_cnt_lsb_minus4;
	deUint8									sps_max_dec_pic_buffering_minus1;
	deUint8									log2_min_luma_coding_block_size_minus3;
	deUint8									log2_diff_max_min_luma_coding_block_size;
	deUint8									log2_min_luma_transform_block_size_minus2;
	deUint8									log2_diff_max_min_luma_transform_block_size;
	deUint8									max_transform_hierarchy_depth_inter;
	deUint8									max_transform_hierarchy_depth_intra;
	deUint8									num_short_term_ref_pic_sets;
	deUint8									num_long_term_ref_pics_sps;
	deUint8									pcm_sample_bit_depth_luma_minus1;
	deUint8									pcm_sample_bit_depth_chroma_minus1;
	deUint8									log2_min_pcm_luma_coding_block_size_minus3;
	deUint8									log2_diff_max_min_pcm_luma_coding_block_size;
	deUint32								conf_win_left_offset;
	deUint32								conf_win_right_offset;
	deUint32								conf_win_top_offset;
	deUint32								conf_win_bottom_offset;
	StdVideoH265DecPicBufMgr*				pDecPicBufMgr;
	StdVideoH265SpsFlags					flags;
	StdVideoH265ScalingLists*				pScalingLists;
	StdVideoH265SequenceParameterSetVui*	pSequenceParameterSetVui;
	deUint8									palette_max_size;
	deUint8									delta_palette_max_predictor_size;
	deUint8									motion_vector_resolution_control_idc;
	deUint8									sps_num_palette_predictor_initializer_minus1;
	StdVideoH265PredictorPaletteEntries*	pPredictorPaletteEntries;
};

struct StdVideoH265PpsFlags
{
	deUint32	dependent_slice_segments_enabled_flag:1;
	deUint32	output_flag_present_flag:1;
	deUint32	sign_data_hiding_enabled_flag:1;
	deUint32	cabac_init_present_flag:1;
	deUint32	constrained_intra_pred_flag:1;
	deUint32	transform_skip_enabled_flag:1;
	deUint32	cu_qp_delta_enabled_flag:1;
	deUint32	pps_slice_chroma_qp_offsets_present_flag:1;
	deUint32	weighted_pred_flag:1;
	deUint32	weighted_bipred_flag:1;
	deUint32	transquant_bypass_enabled_flag:1;
	deUint32	tiles_enabled_flag:1;
	deUint32	entropy_coding_sync_enabled_flag:1;
	deUint32	uniform_spacing_flag:1;
	deUint32	loop_filter_across_tiles_enabled_flag:1;
	deUint32	pps_loop_filter_across_slices_enabled_flag:1;
	deUint32	deblocking_filter_control_present_flag:1;
	deUint32	deblocking_filter_override_enabled_flag:1;
	deUint32	pps_deblocking_filter_disabled_flag:1;
	deUint32	pps_scaling_list_data_present_flag:1;
	deUint32	lists_modification_present_flag:1;
	deUint32	slice_segment_header_extension_present_flag:1;
	deUint32	pps_extension_present_flag:1;
	deUint32	cross_component_prediction_enabled_flag:1;
	deUint32	chroma_qp_offset_list_enabled_flag:1;
	deUint32	pps_curr_pic_ref_enabled_flag:1;
	deUint32	residual_adaptive_colour_transform_enabled_flag:1;
	deUint32	pps_slice_act_qp_offsets_present_flag:1;
	deUint32	pps_palette_predictor_initializer_present_flag:1;
	deUint32	monochrome_palette_flag:1;
	deUint32	pps_range_extension_flag:1;
};

struct StdVideoH265PictureParameterSet
{
	deUint8									pps_pic_parameter_set_id;
	deUint8									pps_seq_parameter_set_id;
	deUint8									num_extra_slice_header_bits;
	deUint8									num_ref_idx_l0_default_active_minus1;
	deUint8									num_ref_idx_l1_default_active_minus1;
	deInt8									init_qp_minus26;
	deUint8									diff_cu_qp_delta_depth;
	deInt8									pps_cb_qp_offset;
	deInt8									pps_cr_qp_offset;
	deUint8									num_tile_columns_minus1;
	deUint8									num_tile_rows_minus1;
	deUint16								column_width_minus1[19];
	deUint16								row_height_minus1[21];
	deInt8									pps_beta_offset_div2;
	deInt8									pps_tc_offset_div2;
	deUint8									log2_parallel_merge_level_minus2;
	StdVideoH265PpsFlags					flags;
	StdVideoH265ScalingLists*				pScalingLists;
	deUint8									log2_max_transform_skip_block_size_minus2;
	deUint8									diff_cu_chroma_qp_offset_depth;
	deUint8									chroma_qp_offset_list_len_minus1;
	deInt8									cb_qp_offset_list[6];
	deInt8									cr_qp_offset_list[6];
	deUint8									log2_sao_offset_scale_luma;
	deUint8									log2_sao_offset_scale_chroma;
	deInt8									pps_act_y_qp_offset_plus5;
	deInt8									pps_act_cb_qp_offset_plus5;
	deInt8									pps_act_cr_qp_offset_plus5;
	deUint8									pps_num_palette_predictor_initializer;
	deUint8									luma_bit_depth_entry_minus8;
	deUint8									chroma_bit_depth_entry_minus8;
	StdVideoH265PredictorPaletteEntries*	pPredictorPaletteEntries;
};

struct StdVideoDecodeH265PictureInfoFlags
{
	deUint32	IrapPicFlag:1;
	deUint32	IdrPicFlag:1;
	deUint32	IsReference:1;
	deUint32	short_term_ref_pic_set_sps_flag:1;
};

struct StdVideoDecodeH265PictureInfo
{
	deUint8								vps_video_parameter_set_id;
	deUint8								sps_seq_parameter_set_id;
	deUint8								pps_pic_parameter_set_id;
	deUint8								num_short_term_ref_pic_sets;
	deInt32								PicOrderCntVal;
	deUint16							NumBitsForSTRefPicSetInSlice;
	deUint8								NumDeltaPocsOfRefRpsIdx;
	deUint8								RefPicSetStCurrBefore[8];
	deUint8								RefPicSetStCurrAfter[8];
	deUint8								RefPicSetLtCurr[8];
	StdVideoDecodeH265PictureInfoFlags	flags;
};

struct StdVideoDecodeH265ReferenceInfoFlags
{
	deUint32	is_long_term:1;
	deUint32	is_non_existing:1;
};

struct StdVideoDecodeH265ReferenceInfo
{
	deInt32									PicOrderCntVal;
	StdVideoDecodeH265ReferenceInfoFlags	flags;
};

struct VkVideoDecodeH265ProfileEXT
{
	VkStructureType			sType;
	const void*				pNext;
	StdVideoH265ProfileIdc	stdProfileIdc;
};

struct VkVideoDecodeH265CapabilitiesEXT
{
	VkStructureType			sType;
	void*					pNext;
	deUint32				maxLevel;
	VkExtensionProperties	stdExtensionVersion;
};

struct VkVideoDecodeH265SessionCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkVideoDecodeH265CreateFlagsEXT	flags;
	const VkExtensionProperties*	pStdExtensionVersion;
};

struct VkVideoDecodeH265SessionParametersAddInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	deUint32								spsStdCount;
	const StdVideoH265SequenceParameterSet*	pSpsStd;
	deUint32								ppsStdCount;
	const StdVideoH265PictureParameterSet*	pPpsStd;
};

struct VkVideoDecodeH265SessionParametersCreateInfoEXT
{
	VkStructureType										sType;
	const void*											pNext;
	deUint32											maxSpsStdCount;
	deUint32											maxPpsStdCount;
	const VkVideoDecodeH265SessionParametersAddInfoEXT*	pParametersAddInfo;
};

struct VkVideoDecodeH265PictureInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	StdVideoDecodeH265PictureInfo*	pStdPictureInfo;
	deUint32						slicesCount;
	const deUint32*					pSlicesDataOffsets;
};

struct VkVideoDecodeH265DpbSlotInfoEXT
{
	VkStructureType							sType;
	const void*								pNext;
	const StdVideoDecodeH265ReferenceInfo*	pStdReferenceInfo;
};

struct VkImagePipeSurfaceCreateInfoFUCHSIA
{
	VkStructureType							sType;
	const void*								pNext;
	VkImagePipeSurfaceCreateFlagsFUCHSIA	flags;
	pt::zx_handle_t							imagePipeHandle;
};

struct VkImportMemoryZirconHandleInfoFUCHSIA
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagBits	handleType;
	pt::zx_handle_t						handle;
};

struct VkMemoryZirconHandlePropertiesFUCHSIA
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		memoryTypeBits;
};

struct VkMemoryGetZirconHandleInfoFUCHSIA
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceMemory						memory;
	VkExternalMemoryHandleTypeFlagBits	handleType;
};

struct VkImportSemaphoreZirconHandleInfoFUCHSIA
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkSemaphoreImportFlags					flags;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
	pt::zx_handle_t							zirconHandle;
};

struct VkSemaphoreGetZirconHandleInfoFUCHSIA
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
};

struct VkStreamDescriptorSurfaceCreateInfoGGP
{
	VkStructureType							sType;
	const void*								pNext;
	VkStreamDescriptorSurfaceCreateFlagsGGP	flags;
	pt::GgpStreamDescriptor					streamDescriptor;
};

struct VkPresentFrameTokenGGP
{
	VkStructureType		sType;
	const void*			pNext;
	pt::GgpFrameToken	frameToken;
};

struct VkIOSSurfaceCreateInfoMVK
{
	VkStructureType				sType;
	const void*					pNext;
	VkIOSSurfaceCreateFlagsMVK	flags;
	const void*					pView;
};

struct VkMacOSSurfaceCreateInfoMVK
{
	VkStructureType					sType;
	const void*						pNext;
	VkMacOSSurfaceCreateFlagsMVK	flags;
	const void*						pView;
};

struct VkMetalSurfaceCreateInfoEXT
{
	VkStructureType					sType;
	const void*						pNext;
	VkMetalSurfaceCreateFlagsEXT	flags;
	const pt::CAMetalLayer*			pLayer;
};

struct VkViSurfaceCreateInfoNN
{
	VkStructureType				sType;
	const void*					pNext;
	VkViSurfaceCreateFlagsNN	flags;
	void*						window;
};

struct VkWaylandSurfaceCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkWaylandSurfaceCreateFlagsKHR	flags;
	pt::WaylandDisplayPtr			display;
	pt::WaylandSurfacePtr			surface;
};

struct VkWin32SurfaceCreateInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	VkWin32SurfaceCreateFlagsKHR	flags;
	pt::Win32InstanceHandle			hinstance;
	pt::Win32WindowHandle			hwnd;
};

struct VkImportMemoryWin32HandleInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagBits	handleType;
	pt::Win32Handle						handle;
	pt::Win32LPCWSTR					name;
};

struct VkExportMemoryWin32HandleInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	pt::Win32SecurityAttributesPtr	pAttributes;
	deUint32						dwAccess;
	pt::Win32LPCWSTR				name;
};

struct VkMemoryWin32HandlePropertiesKHR
{
	VkStructureType	sType;
	void*			pNext;
	deUint32		memoryTypeBits;
};

struct VkMemoryGetWin32HandleInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkDeviceMemory						memory;
	VkExternalMemoryHandleTypeFlagBits	handleType;
};

struct VkWin32KeyedMutexAcquireReleaseInfoKHR
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				acquireCount;
	const VkDeviceMemory*	pAcquireSyncs;
	const deUint64*			pAcquireKeys;
	const deUint32*			pAcquireTimeouts;
	deUint32				releaseCount;
	const VkDeviceMemory*	pReleaseSyncs;
	const deUint64*			pReleaseKeys;
};

struct VkImportSemaphoreWin32HandleInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkSemaphoreImportFlags					flags;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
	pt::Win32Handle							handle;
	pt::Win32LPCWSTR						name;
};

struct VkExportSemaphoreWin32HandleInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	pt::Win32SecurityAttributesPtr	pAttributes;
	deUint32						dwAccess;
	pt::Win32LPCWSTR				name;
};

struct VkD3D12FenceSubmitInfoKHR
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		waitSemaphoreValuesCount;
	const deUint64*	pWaitSemaphoreValues;
	deUint32		signalSemaphoreValuesCount;
	const deUint64*	pSignalSemaphoreValues;
};

struct VkSemaphoreGetWin32HandleInfoKHR
{
	VkStructureType							sType;
	const void*								pNext;
	VkSemaphore								semaphore;
	VkExternalSemaphoreHandleTypeFlagBits	handleType;
};

struct VkImportFenceWin32HandleInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkFence								fence;
	VkFenceImportFlags					flags;
	VkExternalFenceHandleTypeFlagBits	handleType;
	pt::Win32Handle						handle;
	pt::Win32LPCWSTR					name;
};

struct VkExportFenceWin32HandleInfoKHR
{
	VkStructureType					sType;
	const void*						pNext;
	pt::Win32SecurityAttributesPtr	pAttributes;
	deUint32						dwAccess;
	pt::Win32LPCWSTR				name;
};

struct VkFenceGetWin32HandleInfoKHR
{
	VkStructureType						sType;
	const void*							pNext;
	VkFence								fence;
	VkExternalFenceHandleTypeFlagBits	handleType;
};

struct VkImportMemoryWin32HandleInfoNV
{
	VkStructureType						sType;
	const void*							pNext;
	VkExternalMemoryHandleTypeFlagsNV	handleType;
	pt::Win32Handle						handle;
};

struct VkExportMemoryWin32HandleInfoNV
{
	VkStructureType					sType;
	const void*						pNext;
	pt::Win32SecurityAttributesPtr	pAttributes;
	deUint32						dwAccess;
};

struct VkWin32KeyedMutexAcquireReleaseInfoNV
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				acquireCount;
	const VkDeviceMemory*	pAcquireSyncs;
	const deUint64*			pAcquireKeys;
	const deUint32*			pAcquireTimeoutMilliseconds;
	deUint32				releaseCount;
	const VkDeviceMemory*	pReleaseSyncs;
	const deUint64*			pReleaseKeys;
};

struct VkSurfaceFullScreenExclusiveInfoEXT
{
	VkStructureType				sType;
	void*						pNext;
	VkFullScreenExclusiveEXT	fullScreenExclusive;
};

struct VkSurfaceCapabilitiesFullScreenExclusiveEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		fullScreenExclusiveSupported;
};

struct VkSurfaceFullScreenExclusiveWin32InfoEXT
{
	VkStructureType			sType;
	const void*				pNext;
	pt::Win32MonitorHandle	hmonitor;
};

struct VkXcbSurfaceCreateInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkXcbSurfaceCreateFlagsKHR	flags;
	pt::XcbConnectionPtr		connection;
	pt::XcbWindow				window;
};

struct VkXlibSurfaceCreateInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkXlibSurfaceCreateFlagsKHR	flags;
	pt::XlibDisplayPtr			dpy;
	pt::XlibWindow				window;
};

typedef VkBindBufferMemoryInfo VkBindBufferMemoryInfoKHR;

typedef VkBindImageMemoryInfo VkBindImageMemoryInfoKHR;

typedef VkPhysicalDevice16BitStorageFeatures VkPhysicalDevice16BitStorageFeaturesKHR;

typedef VkMemoryDedicatedRequirements VkMemoryDedicatedRequirementsKHR;

typedef VkMemoryDedicatedAllocateInfo VkMemoryDedicatedAllocateInfoKHR;

typedef VkMemoryAllocateFlagsInfo VkMemoryAllocateFlagsInfoKHR;

typedef VkDeviceGroupRenderPassBeginInfo VkDeviceGroupRenderPassBeginInfoKHR;

typedef VkDeviceGroupCommandBufferBeginInfo VkDeviceGroupCommandBufferBeginInfoKHR;

typedef VkDeviceGroupSubmitInfo VkDeviceGroupSubmitInfoKHR;

typedef VkDeviceGroupBindSparseInfo VkDeviceGroupBindSparseInfoKHR;

typedef VkBindBufferMemoryDeviceGroupInfo VkBindBufferMemoryDeviceGroupInfoKHR;

typedef VkBindImageMemoryDeviceGroupInfo VkBindImageMemoryDeviceGroupInfoKHR;

typedef VkPhysicalDeviceGroupProperties VkPhysicalDeviceGroupPropertiesKHR;

typedef VkDeviceGroupDeviceCreateInfo VkDeviceGroupDeviceCreateInfoKHR;

typedef VkBufferMemoryRequirementsInfo2 VkBufferMemoryRequirementsInfo2KHR;

typedef VkImageMemoryRequirementsInfo2 VkImageMemoryRequirementsInfo2KHR;

typedef VkImageSparseMemoryRequirementsInfo2 VkImageSparseMemoryRequirementsInfo2KHR;

typedef VkMemoryRequirements2 VkMemoryRequirements2KHR;

typedef VkSparseImageMemoryRequirements2 VkSparseImageMemoryRequirements2KHR;

typedef VkPhysicalDeviceFeatures2 VkPhysicalDeviceFeatures2KHR;

typedef VkPhysicalDeviceProperties2 VkPhysicalDeviceProperties2KHR;

typedef VkFormatProperties2 VkFormatProperties2KHR;

typedef VkImageFormatProperties2 VkImageFormatProperties2KHR;

typedef VkPhysicalDeviceImageFormatInfo2 VkPhysicalDeviceImageFormatInfo2KHR;

typedef VkQueueFamilyProperties2 VkQueueFamilyProperties2KHR;

typedef VkPhysicalDeviceMemoryProperties2 VkPhysicalDeviceMemoryProperties2KHR;

typedef VkSparseImageFormatProperties2 VkSparseImageFormatProperties2KHR;

typedef VkPhysicalDeviceSparseImageFormatInfo2 VkPhysicalDeviceSparseImageFormatInfo2KHR;

typedef VkPhysicalDevicePointClippingProperties VkPhysicalDevicePointClippingPropertiesKHR;

typedef VkInputAttachmentAspectReference VkInputAttachmentAspectReferenceKHR;

typedef VkRenderPassInputAttachmentAspectCreateInfo VkRenderPassInputAttachmentAspectCreateInfoKHR;

typedef VkImageViewUsageCreateInfo VkImageViewUsageCreateInfoKHR;

typedef VkPipelineTessellationDomainOriginStateCreateInfo VkPipelineTessellationDomainOriginStateCreateInfoKHR;

typedef VkRenderPassMultiviewCreateInfo VkRenderPassMultiviewCreateInfoKHR;

typedef VkPhysicalDeviceMultiviewFeatures VkPhysicalDeviceMultiviewFeaturesKHR;

typedef VkPhysicalDeviceMultiviewProperties VkPhysicalDeviceMultiviewPropertiesKHR;

typedef VkSamplerYcbcrConversionCreateInfo VkSamplerYcbcrConversionCreateInfoKHR;

typedef VkSamplerYcbcrConversionInfo VkSamplerYcbcrConversionInfoKHR;

typedef VkBindImagePlaneMemoryInfo VkBindImagePlaneMemoryInfoKHR;

typedef VkImagePlaneMemoryRequirementsInfo VkImagePlaneMemoryRequirementsInfoKHR;

typedef VkPhysicalDeviceSamplerYcbcrConversionFeatures VkPhysicalDeviceSamplerYcbcrConversionFeaturesKHR;

typedef VkSamplerYcbcrConversionImageFormatProperties VkSamplerYcbcrConversionImageFormatPropertiesKHR;

typedef VkDescriptorUpdateTemplateEntry VkDescriptorUpdateTemplateEntryKHR;

typedef VkDescriptorUpdateTemplateCreateInfo VkDescriptorUpdateTemplateCreateInfoKHR;

typedef VkExternalMemoryProperties VkExternalMemoryPropertiesKHR;

typedef VkPhysicalDeviceExternalImageFormatInfo VkPhysicalDeviceExternalImageFormatInfoKHR;

typedef VkExternalImageFormatProperties VkExternalImageFormatPropertiesKHR;

typedef VkPhysicalDeviceExternalBufferInfo VkPhysicalDeviceExternalBufferInfoKHR;

typedef VkExternalBufferProperties VkExternalBufferPropertiesKHR;

typedef VkPhysicalDeviceIDProperties VkPhysicalDeviceIDPropertiesKHR;

typedef VkExternalMemoryImageCreateInfo VkExternalMemoryImageCreateInfoKHR;

typedef VkExternalMemoryBufferCreateInfo VkExternalMemoryBufferCreateInfoKHR;

typedef VkExportMemoryAllocateInfo VkExportMemoryAllocateInfoKHR;

typedef VkPhysicalDeviceExternalFenceInfo VkPhysicalDeviceExternalFenceInfoKHR;

typedef VkExternalFenceProperties VkExternalFencePropertiesKHR;

typedef VkExportFenceCreateInfo VkExportFenceCreateInfoKHR;

typedef VkExportSemaphoreCreateInfo VkExportSemaphoreCreateInfoKHR;

typedef VkPhysicalDeviceExternalSemaphoreInfo VkPhysicalDeviceExternalSemaphoreInfoKHR;

typedef VkExternalSemaphoreProperties VkExternalSemaphorePropertiesKHR;

typedef VkPhysicalDeviceMaintenance3Properties VkPhysicalDeviceMaintenance3PropertiesKHR;

typedef VkDescriptorSetLayoutSupport VkDescriptorSetLayoutSupportKHR;

typedef VkPhysicalDeviceShaderDrawParametersFeatures VkPhysicalDeviceShaderDrawParameterFeatures;

typedef VkConformanceVersion VkConformanceVersionKHR;

typedef VkImageFormatListCreateInfo VkImageFormatListCreateInfoKHR;

typedef VkAttachmentDescription2 VkAttachmentDescription2KHR;

typedef VkAttachmentReference2 VkAttachmentReference2KHR;

typedef VkSubpassDescription2 VkSubpassDescription2KHR;

typedef VkSubpassDependency2 VkSubpassDependency2KHR;

typedef VkRenderPassCreateInfo2 VkRenderPassCreateInfo2KHR;

typedef VkSubpassBeginInfo VkSubpassBeginInfoKHR;

typedef VkSubpassEndInfo VkSubpassEndInfoKHR;

typedef VkPhysicalDevice8BitStorageFeatures VkPhysicalDevice8BitStorageFeaturesKHR;

typedef VkPhysicalDeviceDriverProperties VkPhysicalDeviceDriverPropertiesKHR;

typedef VkPhysicalDeviceShaderAtomicInt64Features VkPhysicalDeviceShaderAtomicInt64FeaturesKHR;

typedef VkPhysicalDeviceFloatControlsProperties VkPhysicalDeviceFloatControlsPropertiesKHR;

typedef VkDescriptorSetLayoutBindingFlagsCreateInfo VkDescriptorSetLayoutBindingFlagsCreateInfoEXT;

typedef VkPhysicalDeviceDescriptorIndexingFeatures VkPhysicalDeviceDescriptorIndexingFeaturesEXT;

typedef VkPhysicalDeviceDescriptorIndexingProperties VkPhysicalDeviceDescriptorIndexingPropertiesEXT;

typedef VkDescriptorSetVariableDescriptorCountAllocateInfo VkDescriptorSetVariableDescriptorCountAllocateInfoEXT;

typedef VkDescriptorSetVariableDescriptorCountLayoutSupport VkDescriptorSetVariableDescriptorCountLayoutSupportEXT;

typedef VkSubpassDescriptionDepthStencilResolve VkSubpassDescriptionDepthStencilResolveKHR;

typedef VkPhysicalDeviceDepthStencilResolveProperties VkPhysicalDeviceDepthStencilResolvePropertiesKHR;

typedef VkPhysicalDeviceScalarBlockLayoutFeatures VkPhysicalDeviceScalarBlockLayoutFeaturesEXT;

typedef VkImageStencilUsageCreateInfo VkImageStencilUsageCreateInfoEXT;

typedef VkSamplerReductionModeCreateInfo VkSamplerReductionModeCreateInfoEXT;

typedef VkPhysicalDeviceSamplerFilterMinmaxProperties VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT;

typedef VkPhysicalDeviceVulkanMemoryModelFeatures VkPhysicalDeviceVulkanMemoryModelFeaturesKHR;

typedef VkPhysicalDeviceImagelessFramebufferFeatures VkPhysicalDeviceImagelessFramebufferFeaturesKHR;

typedef VkFramebufferAttachmentImageInfo VkFramebufferAttachmentImageInfoKHR;

typedef VkFramebufferAttachmentsCreateInfo VkFramebufferAttachmentsCreateInfoKHR;

typedef VkRenderPassAttachmentBeginInfo VkRenderPassAttachmentBeginInfoKHR;

typedef VkPhysicalDeviceUniformBufferStandardLayoutFeatures VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR;

typedef VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR;

typedef VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR;

typedef VkAttachmentReferenceStencilLayout VkAttachmentReferenceStencilLayoutKHR;

typedef VkAttachmentDescriptionStencilLayout VkAttachmentDescriptionStencilLayoutKHR;

typedef VkPhysicalDeviceHostQueryResetFeatures VkPhysicalDeviceHostQueryResetFeaturesEXT;

typedef VkPhysicalDeviceTimelineSemaphoreFeatures VkPhysicalDeviceTimelineSemaphoreFeaturesKHR;

typedef VkPhysicalDeviceTimelineSemaphoreProperties VkPhysicalDeviceTimelineSemaphorePropertiesKHR;

typedef VkSemaphoreTypeCreateInfo VkSemaphoreTypeCreateInfoKHR;

typedef VkTimelineSemaphoreSubmitInfo VkTimelineSemaphoreSubmitInfoKHR;

typedef VkSemaphoreWaitInfo VkSemaphoreWaitInfoKHR;

typedef VkSemaphoreSignalInfo VkSemaphoreSignalInfoKHR;

typedef VkPhysicalDeviceBufferDeviceAddressFeatures VkPhysicalDeviceBufferDeviceAddressFeaturesKHR;

typedef VkBufferOpaqueCaptureAddressCreateInfo VkBufferOpaqueCaptureAddressCreateInfoKHR;

typedef VkMemoryOpaqueCaptureAddressAllocateInfo VkMemoryOpaqueCaptureAddressAllocateInfoKHR;

typedef VkDeviceMemoryOpaqueCaptureAddressInfo VkDeviceMemoryOpaqueCaptureAddressInfoKHR;

typedef VkTransformMatrixKHR VkTransformMatrixNV;

typedef VkAabbPositionsKHR VkAabbPositionsNV;

typedef VkAccelerationStructureInstanceKHR VkAccelerationStructureInstanceNV;

typedef VkQueryPoolPerformanceQueryCreateInfoINTEL VkQueryPoolCreateInfoINTEL;

typedef VkPhysicalDeviceBufferDeviceAddressFeaturesEXT VkPhysicalDeviceBufferAddressFeaturesEXT;

