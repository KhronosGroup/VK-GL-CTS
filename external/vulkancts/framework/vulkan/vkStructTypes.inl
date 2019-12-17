/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
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

struct VkAllocationCallbacks
{
	void*									pUserData;
	PFN_vkAllocationFunction				pfnAllocation;
	PFN_vkReallocationFunction				pfnReallocation;
	PFN_vkFreeFunction						pfnFree;
	PFN_vkInternalAllocationNotification	pfnInternalAllocation;
	PFN_vkInternalFreeNotification			pfnInternalFree;
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

struct VkFormatProperties
{
	VkFormatFeatureFlags	linearTilingFeatures;
	VkFormatFeatureFlags	optimalTilingFeatures;
	VkFormatFeatureFlags	bufferFeatures;
};

struct VkExtent3D
{
	deUint32	width;
	deUint32	height;
	deUint32	depth;
};

struct VkImageFormatProperties
{
	VkExtent3D			maxExtent;
	deUint32			maxMipLevels;
	deUint32			maxArrayLayers;
	VkSampleCountFlags	sampleCounts;
	VkDeviceSize		maxResourceSize;
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

struct VkMemoryType
{
	VkMemoryPropertyFlags	propertyFlags;
	deUint32				heapIndex;
};

struct VkMemoryHeap
{
	VkDeviceSize		size;
	VkMemoryHeapFlags	flags;
};

struct VkPhysicalDeviceMemoryProperties
{
	deUint32		memoryTypeCount;
	VkMemoryType	memoryTypes[VK_MAX_MEMORY_TYPES];
	deUint32		memoryHeapCount;
	VkMemoryHeap	memoryHeaps[VK_MAX_MEMORY_HEAPS];
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

struct VkMemoryAllocateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceSize	allocationSize;
	deUint32		memoryTypeIndex;
};

struct VkMappedMemoryRange
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	memory;
	VkDeviceSize	offset;
	VkDeviceSize	size;
};

struct VkMemoryRequirements
{
	VkDeviceSize	size;
	VkDeviceSize	alignment;
	deUint32		memoryTypeBits;
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

struct VkOffset3D
{
	deInt32	x;
	deInt32	y;
	deInt32	z;
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

struct VkImageSubresourceRange
{
	VkImageAspectFlags	aspectMask;
	deUint32			baseMipLevel;
	deUint32			levelCount;
	deUint32			baseArrayLayer;
	deUint32			layerCount;
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

struct VkOffset2D
{
	deInt32	x;
	deInt32	y;
};

struct VkExtent2D
{
	deUint32	width;
	deUint32	height;
};

struct VkRect2D
{
	VkOffset2D	offset;
	VkExtent2D	extent;
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

struct VkDescriptorImageInfo
{
	VkSampler		sampler;
	VkImageView		imageView;
	VkImageLayout	imageLayout;
};

struct VkDescriptorBufferInfo
{
	VkBuffer		buffer;
	VkDeviceSize	offset;
	VkDeviceSize	range;
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

struct VkImageCopy
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkImageBlit
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffsets[2];
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffsets[2];
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

struct VkImageResolve
{
	VkImageSubresourceLayers	srcSubresource;
	VkOffset3D					srcOffset;
	VkImageSubresourceLayers	dstSubresource;
	VkOffset3D					dstOffset;
	VkExtent3D					extent;
};

struct VkMemoryBarrier
{
	VkStructureType	sType;
	const void*		pNext;
	VkAccessFlags	srcAccessMask;
	VkAccessFlags	dstAccessMask;
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

struct VkBaseOutStructure
{
	VkStructureType				sType;
	struct VkBaseOutStructure*	pNext;
};

struct VkBaseInStructure
{
	VkStructureType					sType;
	const struct VkBaseInStructure*	pNext;
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

struct VkDisplayModeParametersKHR
{
	VkExtent2D	visibleRegion;
	deUint32	refreshRate;
};

struct VkDisplayModePropertiesKHR
{
	VkDisplayModeKHR			displayMode;
	VkDisplayModeParametersKHR	parameters;
};

struct VkDisplayModeCreateInfoKHR
{
	VkStructureType				sType;
	const void*					pNext;
	VkDisplayModeCreateFlagsKHR	flags;
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

struct VkDeviceGeneratedCommandsFeaturesNVX
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		computeBindingPointSupport;
};

struct VkDeviceGeneratedCommandsLimitsNVX
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		maxIndirectCommandsLayoutTokenCount;
	deUint32		maxObjectEntryCounts;
	deUint32		minSequenceCountBufferOffsetAlignment;
	deUint32		minSequenceIndexBufferOffsetAlignment;
	deUint32		minCommandsTokenBufferOffsetAlignment;
};

struct VkIndirectCommandsTokenNVX
{
	VkIndirectCommandsTokenTypeNVX	tokenType;
	VkBuffer						buffer;
	VkDeviceSize					offset;
};

struct VkIndirectCommandsLayoutTokenNVX
{
	VkIndirectCommandsTokenTypeNVX	tokenType;
	deUint32						bindingUnit;
	deUint32						dynamicCount;
	deUint32						divisor;
};

struct VkIndirectCommandsLayoutCreateInfoNVX
{
	VkStructureType							sType;
	const void*								pNext;
	VkPipelineBindPoint						pipelineBindPoint;
	VkIndirectCommandsLayoutUsageFlagsNVX	flags;
	deUint32								tokenCount;
	const VkIndirectCommandsLayoutTokenNVX*	pTokens;
};

struct VkCmdProcessCommandsInfoNVX
{
	VkStructureType						sType;
	const void*							pNext;
	VkObjectTableNVX					objectTable;
	VkIndirectCommandsLayoutNVX			indirectCommandsLayout;
	deUint32							indirectCommandsTokenCount;
	const VkIndirectCommandsTokenNVX*	pIndirectCommandsTokens;
	deUint32							maxSequencesCount;
	VkCommandBuffer						targetCommandBuffer;
	VkBuffer							sequencesCountBuffer;
	VkDeviceSize						sequencesCountOffset;
	VkBuffer							sequencesIndexBuffer;
	VkDeviceSize						sequencesIndexOffset;
};

struct VkCmdReserveSpaceForCommandsInfoNVX
{
	VkStructureType				sType;
	const void*					pNext;
	VkObjectTableNVX			objectTable;
	VkIndirectCommandsLayoutNVX	indirectCommandsLayout;
	deUint32					maxSequencesCount;
};

struct VkObjectTableCreateInfoNVX
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							objectCount;
	const VkObjectEntryTypeNVX*			pObjectEntryTypes;
	const deUint32*						pObjectEntryCounts;
	const VkObjectEntryUsageFlagsNVX*	pObjectEntryUsageFlags;
	deUint32							maxUniformBuffersPerDescriptor;
	deUint32							maxStorageBuffersPerDescriptor;
	deUint32							maxStorageImagesPerDescriptor;
	deUint32							maxSampledImagesPerDescriptor;
	deUint32							maxPipelineLayouts;
};

struct VkObjectTableEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
};

struct VkObjectTablePipelineEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
	VkPipeline					pipeline;
};

struct VkObjectTableDescriptorSetEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
	VkPipelineLayout			pipelineLayout;
	VkDescriptorSet				descriptorSet;
};

struct VkObjectTableVertexBufferEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
	VkBuffer					buffer;
};

struct VkObjectTableIndexBufferEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
	VkBuffer					buffer;
	VkIndexType					indexType;
};

struct VkObjectTablePushConstantEntryNVX
{
	VkObjectEntryTypeNVX		type;
	VkObjectEntryUsageFlagsNVX	flags;
	VkPipelineLayout			pipelineLayout;
	VkShaderStageFlags			stageFlags;
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

struct VkDebugUtilsObjectNameInfoEXT
{
	VkStructureType	sType;
	const void*		pNext;
	VkObjectType	objectType;
	deUint64		objectHandle;
	const char*		pObjectName;
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

struct VkDebugUtilsLabelEXT
{
	VkStructureType	sType;
	const void*		pNext;
	const char*		pLabelName;
	float			color[4];
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
	VkRayTracingShaderGroupTypeNV	type;
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
	VkGeometryTypeNV	geometryType;
	VkGeometryDataNV	geometry;
	VkGeometryFlagsNV	flags;
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

struct VkQueryPoolCreateInfoINTEL
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

struct VkPhysicalDeviceIndexTypeUint8FeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		indexTypeUint8;
};

struct VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT
{
	VkStructureType	sType;
	void*			pNext;
	VkBool32		shaderDemoteToHelperInvocation;
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

struct VkImagePipeSurfaceCreateInfoFUCHSIA
{
	VkStructureType							sType;
	const void*								pNext;
	VkImagePipeSurfaceCreateFlagsFUCHSIA	flags;
	pt::zx_handle_t							imagePipeHandle;
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

typedef VkPhysicalDeviceBufferDeviceAddressFeaturesEXT VkPhysicalDeviceBufferAddressFeaturesEXT;

