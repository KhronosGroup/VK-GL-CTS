/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
struct VkApplicationInfo
{
	VkStructureType	sType;
	const void*		pNext;
	const char*		pAppName;
	deUint32		appVersion;
	const char*		pEngineName;
	deUint32		engineVersion;
	deUint32		apiVersion;
};

struct VkAllocCallbacks
{
	void*				pUserData;
	PFN_vkAllocFunction	pfnAlloc;
	PFN_vkFreeFunction	pfnFree;
};

struct VkInstanceCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	const VkApplicationInfo*	pAppInfo;
	const VkAllocCallbacks*		pAllocCb;
	deUint32					layerCount;
	const char*const*			ppEnabledLayerNames;
	deUint32					extensionCount;
	const char*const*			ppEnabledExtensionNames;
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
	VkBool32	dualSourceBlend;
	VkBool32	logicOp;
	VkBool32	instancedDrawIndirect;
	VkBool32	depthClip;
	VkBool32	depthBiasClamp;
	VkBool32	fillModeNonSolid;
	VkBool32	depthBounds;
	VkBool32	wideLines;
	VkBool32	largePoints;
	VkBool32	textureCompressionETC2;
	VkBool32	textureCompressionASTC_LDR;
	VkBool32	textureCompressionBC;
	VkBool32	pipelineStatisticsQuery;
	VkBool32	vertexSideEffects;
	VkBool32	tessellationSideEffects;
	VkBool32	geometrySideEffects;
	VkBool32	fragmentSideEffects;
	VkBool32	shaderTessellationPointSize;
	VkBool32	shaderGeometryPointSize;
	VkBool32	shaderTextureGatherExtended;
	VkBool32	shaderStorageImageExtendedFormats;
	VkBool32	shaderStorageImageMultisample;
	VkBool32	shaderStorageBufferArrayConstantIndexing;
	VkBool32	shaderStorageImageArrayConstantIndexing;
	VkBool32	shaderUniformBufferArrayDynamicIndexing;
	VkBool32	shaderSampledImageArrayDynamicIndexing;
	VkBool32	shaderStorageBufferArrayDynamicIndexing;
	VkBool32	shaderStorageImageArrayDynamicIndexing;
	VkBool32	shaderClipDistance;
	VkBool32	shaderCullDistance;
	VkBool32	shaderFloat64;
	VkBool32	shaderInt64;
	VkBool32	shaderFloat16;
	VkBool32	shaderInt16;
	VkBool32	shaderResourceResidency;
	VkBool32	shaderResourceMinLOD;
	VkBool32	sparse;
	VkBool32	sparseResidencyBuffer;
	VkBool32	sparseResidencyImage2D;
	VkBool32	sparseResidencyImage3D;
	VkBool32	sparseResidency2Samples;
	VkBool32	sparseResidency4Samples;
	VkBool32	sparseResidency8Samples;
	VkBool32	sparseResidency16Samples;
	VkBool32	sparseResidencyStandard2DBlockShape;
	VkBool32	sparseResidencyStandard2DMSBlockShape;
	VkBool32	sparseResidencyStandard3DBlockShape;
	VkBool32	sparseResidencyAlignedMipSize;
	VkBool32	sparseResidencyNonResident;
	VkBool32	sparseResidencyNonResidentStrict;
	VkBool32	sparseResidencyAliased;
};

struct VkFormatProperties
{
	VkFormatFeatureFlags	linearTilingFeatures;
	VkFormatFeatureFlags	optimalTilingFeatures;
};

struct VkImageFormatProperties
{
	deUint64	maxResourceSize;
	deUint32	maxSamples;
};

struct VkPhysicalDeviceLimits
{
	deUint32		maxImageDimension1D;
	deUint32		maxImageDimension2D;
	deUint32		maxImageDimension3D;
	deUint32		maxImageDimensionCube;
	deUint32		maxImageArrayLayers;
	deUint32		maxTexelBufferSize;
	deUint32		maxUniformBufferSize;
	deUint32		maxStorageBufferSize;
	deUint32		maxPushConstantsSize;
	deUint32		maxMemoryAllocationCount;
	VkDeviceSize	bufferImageGranularity;
	deUint32		maxBoundDescriptorSets;
	deUint32		maxDescriptorSets;
	deUint32		maxPerStageDescriptorSamplers;
	deUint32		maxPerStageDescriptorUniformBuffers;
	deUint32		maxPerStageDescriptorStorageBuffers;
	deUint32		maxPerStageDescriptorSampledImages;
	deUint32		maxPerStageDescriptorStorageImages;
	deUint32		maxDescriptorSetSamplers;
	deUint32		maxDescriptorSetUniformBuffers;
	deUint32		maxDescriptorSetStorageBuffers;
	deUint32		maxDescriptorSetSampledImages;
	deUint32		maxDescriptorSetStorageImages;
	deUint32		maxVertexInputAttributes;
	deUint32		maxVertexInputAttributeOffset;
	deUint32		maxVertexInputBindingStride;
	deUint32		maxVertexOutputComponents;
	deUint32		maxTessGenLevel;
	deUint32		maxTessPatchSize;
	deUint32		maxTessControlPerVertexInputComponents;
	deUint32		maxTessControlPerVertexOutputComponents;
	deUint32		maxTessControlPerPatchOutputComponents;
	deUint32		maxTessControlTotalOutputComponents;
	deUint32		maxTessEvaluationInputComponents;
	deUint32		maxTessEvaluationOutputComponents;
	deUint32		maxGeometryShaderInvocations;
	deUint32		maxGeometryInputComponents;
	deUint32		maxGeometryOutputComponents;
	deUint32		maxGeometryOutputVertices;
	deUint32		maxGeometryTotalOutputComponents;
	deUint32		maxFragmentInputComponents;
	deUint32		maxFragmentOutputBuffers;
	deUint32		maxFragmentDualSourceBuffers;
	deUint32		maxFragmentCombinedOutputResources;
	deUint32		maxComputeSharedMemorySize;
	deUint32		maxComputeWorkGroupCount[3];
	deUint32		maxComputeWorkGroupInvocations;
	deUint32		maxComputeWorkGroupSize[3];
	deUint32		subPixelPrecisionBits;
	deUint32		subTexelPrecisionBits;
	deUint32		mipmapPrecisionBits;
	deUint32		maxDrawIndexedIndexValue;
	deUint32		maxDrawIndirectInstanceCount;
	VkBool32		primitiveRestartForPatches;
	float			maxSamplerLodBias;
	float			maxSamplerAnisotropy;
	deUint32		maxViewports;
	deUint32		maxDynamicViewportStates;
	deUint32		maxViewportDimensions[2];
	float			viewportBoundsRange[2];
	deUint32		viewportSubPixelBits;
	deUint32		minMemoryMapAlignment;
	deUint32		minTexelBufferOffsetAlignment;
	deUint32		minUniformBufferOffsetAlignment;
	deUint32		minStorageBufferOffsetAlignment;
	deUint32		minTexelOffset;
	deUint32		maxTexelOffset;
	deUint32		minTexelGatherOffset;
	deUint32		maxTexelGatherOffset;
	float			minInterpolationOffset;
	float			maxInterpolationOffset;
	deUint32		subPixelInterpolationOffsetBits;
	deUint32		maxFramebufferWidth;
	deUint32		maxFramebufferHeight;
	deUint32		maxFramebufferLayers;
	deUint32		maxFramebufferColorSamples;
	deUint32		maxFramebufferDepthSamples;
	deUint32		maxFramebufferStencilSamples;
	deUint32		maxColorAttachments;
	deUint32		maxSampledImageColorSamples;
	deUint32		maxSampledImageDepthSamples;
	deUint32		maxSampledImageIntegerSamples;
	deUint32		maxStorageImageSamples;
	deUint32		maxSampleMaskWords;
	deUint64		timestampFrequency;
	deUint32		maxClipDistances;
	deUint32		maxCullDistances;
	deUint32		maxCombinedClipAndCullDistances;
	float			pointSizeRange[2];
	float			lineWidthRange[2];
	float			pointSizeGranularity;
	float			lineWidthGranularity;
};

struct VkPhysicalDeviceProperties
{
	deUint32				apiVersion;
	deUint32				driverVersion;
	deUint32				vendorId;
	deUint32				deviceId;
	VkPhysicalDeviceType	deviceType;
	char					deviceName[VK_MAX_PHYSICAL_DEVICE_NAME];
	deUint8					pipelineCacheUUID[VK_UUID_LENGTH];
};

struct VkPhysicalDeviceQueueProperties
{
	VkQueueFlags	queueFlags;
	deUint32		queueCount;
	VkBool32		supportsTimestamps;
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
	deUint32	queueFamilyIndex;
	deUint32	queueCount;
};

struct VkDeviceCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						queueRecordCount;
	const VkDeviceQueueCreateInfo*	pRequestedQueues;
	deUint32						layerCount;
	const char*const*				ppEnabledLayerNames;
	deUint32						extensionCount;
	const char*const*				ppEnabledExtensionNames;
	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	VkDeviceCreateFlags				flags;
};

struct VkExtensionProperties
{
	char		extName[VK_MAX_EXTENSION_NAME];
	deUint32	specVersion;
};

struct VkLayerProperties
{
	char		layerName[VK_MAX_EXTENSION_NAME];
	deUint32	specVersion;
	deUint32	implVersion;
	char		description[VK_MAX_DESCRIPTION];
};

struct VkMemoryAllocInfo
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
	VkDeviceMemory	mem;
	VkDeviceSize	offset;
	VkDeviceSize	size;
};

struct VkMemoryRequirements
{
	VkDeviceSize	size;
	VkDeviceSize	alignment;
	deUint32		memoryTypeBits;
};

struct VkExtent3D
{
	deInt32	width;
	deInt32	height;
	deInt32	depth;
};

struct VkSparseImageFormatProperties
{
	VkImageAspect				aspect;
	VkExtent3D					imageGranularity;
	VkSparseImageFormatFlags	flags;
};

struct VkSparseImageMemoryRequirements
{
	VkSparseImageFormatProperties	formatProps;
	deUint32						imageMipTailStartLOD;
	VkDeviceSize					imageMipTailSize;
	VkDeviceSize					imageMipTailOffset;
	VkDeviceSize					imageMipTailStride;
};

struct VkSparseMemoryBindInfo
{
	VkDeviceSize			offset;
	VkDeviceSize			memOffset;
	VkDeviceMemory			mem;
	VkSparseMemoryBindFlags	flags;
};

struct VkImageSubresource
{
	VkImageAspect	aspect;
	deUint32		mipLevel;
	deUint32		arraySlice;
};

struct VkOffset3D
{
	deInt32	x;
	deInt32	y;
	deInt32	z;
};

struct VkSparseImageMemoryBindInfo
{
	VkImageSubresource		subresource;
	VkOffset3D				offset;
	VkExtent3D				extent;
	VkDeviceSize			memOffset;
	VkDeviceMemory			mem;
	VkSparseMemoryBindFlags	flags;
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
	VkQueryType						queryType;
	deUint32						slots;
	VkQueryPipelineStatisticFlags	pipelineStatistics;
};

struct VkBufferCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkDeviceSize		size;
	VkBufferUsageFlags	usage;
	VkBufferCreateFlags	flags;
	VkSharingMode		sharingMode;
	deUint32			queueFamilyCount;
	const deUint32*		pQueueFamilyIndices;
};

struct VkBufferViewCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkBuffer			buffer;
	VkBufferViewType	viewType;
	VkFormat			format;
	VkDeviceSize		offset;
	VkDeviceSize		range;
};

struct VkImageCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkImageType			imageType;
	VkFormat			format;
	VkExtent3D			extent;
	deUint32			mipLevels;
	deUint32			arraySize;
	deUint32			samples;
	VkImageTiling		tiling;
	VkImageUsageFlags	usage;
	VkImageCreateFlags	flags;
	VkSharingMode		sharingMode;
	deUint32			queueFamilyCount;
	const deUint32*		pQueueFamilyIndices;
};

struct VkSubresourceLayout
{
	VkDeviceSize	offset;
	VkDeviceSize	size;
	VkDeviceSize	rowPitch;
	VkDeviceSize	depthPitch;
};

struct VkChannelMapping
{
	VkChannelSwizzle	r;
	VkChannelSwizzle	g;
	VkChannelSwizzle	b;
	VkChannelSwizzle	a;
};

struct VkImageSubresourceRange
{
	VkImageAspect	aspect;
	deUint32		baseMipLevel;
	deUint32		mipLevels;
	deUint32		baseArraySlice;
	deUint32		arraySize;
};

struct VkImageViewCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImage					image;
	VkImageViewType			viewType;
	VkFormat				format;
	VkChannelMapping		channels;
	VkImageSubresourceRange	subresourceRange;
};

struct VkAttachmentViewCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkImage						image;
	VkFormat					format;
	deUint32					mipLevel;
	deUint32					baseArraySlice;
	deUint32					arraySize;
	VkAttachmentViewCreateFlags	flags;
};

struct VkShaderModuleCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	deUintptr					codeSize;
	const void*					pCode;
	VkShaderModuleCreateFlags	flags;
};

struct VkShaderCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkShaderModule		module;
	const char*			pName;
	VkShaderCreateFlags	flags;
};

struct VkPipelineCacheCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUintptr		initialSize;
	const void*		initialData;
	deUintptr		maxSize;
};

struct VkSpecializationMapEntry
{
	deUint32	constantId;
	deUintptr	size;
	deUint32	offset;
};

struct VkSpecializationInfo
{
	deUint32						mapEntryCount;
	const VkSpecializationMapEntry*	pMap;
	deUintptr						dataSize;
	const void*						pData;
};

struct VkPipelineShaderStageCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkShaderStage				stage;
	VkShader					shader;
	const VkSpecializationInfo*	pSpecializationInfo;
};

struct VkVertexInputBindingDescription
{
	deUint32				binding;
	deUint32				strideInBytes;
	VkVertexInputStepRate	stepRate;
};

struct VkVertexInputAttributeDescription
{
	deUint32	location;
	deUint32	binding;
	VkFormat	format;
	deUint32	offsetInBytes;
};

struct VkPipelineVertexInputStateCreateInfo
{
	VkStructureType								sType;
	const void*									pNext;
	deUint32									bindingCount;
	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
	deUint32									attributeCount;
	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
};

struct VkPipelineInputAssemblyStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkPrimitiveTopology	topology;
	VkBool32			primitiveRestartEnable;
};

struct VkPipelineTessellationStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		patchControlPoints;
};

struct VkPipelineViewportStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		viewportCount;
};

struct VkPipelineRasterStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkBool32		depthClipEnable;
	VkBool32		rasterizerDiscardEnable;
	VkFillMode		fillMode;
	VkCullMode		cullMode;
	VkFrontFace		frontFace;
};

struct VkPipelineMultisampleStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		rasterSamples;
	VkBool32		sampleShadingEnable;
	float			minSampleShading;
	VkSampleMask	sampleMask;
};

struct VkStencilOpState
{
	VkStencilOp	stencilFailOp;
	VkStencilOp	stencilPassOp;
	VkStencilOp	stencilDepthFailOp;
	VkCompareOp	stencilCompareOp;
};

struct VkPipelineDepthStencilStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkBool32			depthTestEnable;
	VkBool32			depthWriteEnable;
	VkCompareOp			depthCompareOp;
	VkBool32			depthBoundsEnable;
	VkBool32			stencilTestEnable;
	VkStencilOpState	front;
	VkStencilOpState	back;
};

struct VkPipelineColorBlendAttachmentState
{
	VkBool32		blendEnable;
	VkBlend			srcBlendColor;
	VkBlend			destBlendColor;
	VkBlendOp		blendOpColor;
	VkBlend			srcBlendAlpha;
	VkBlend			destBlendAlpha;
	VkBlendOp		blendOpAlpha;
	VkChannelFlags	channelWriteMask;
};

struct VkPipelineColorBlendStateCreateInfo
{
	VkStructureType								sType;
	const void*									pNext;
	VkBool32									alphaToCoverageEnable;
	VkBool32									logicOpEnable;
	VkLogicOp									logicOp;
	deUint32									attachmentCount;
	const VkPipelineColorBlendAttachmentState*	pAttachments;
};

struct VkGraphicsPipelineCreateInfo
{
	VkStructureType									sType;
	const void*										pNext;
	deUint32										stageCount;
	const VkPipelineShaderStageCreateInfo*			pStages;
	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
	const VkPipelineViewportStateCreateInfo*		pViewportState;
	const VkPipelineRasterStateCreateInfo*			pRasterState;
	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
	VkPipelineCreateFlags							flags;
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
	VkPipelineShaderStageCreateInfo	cs;
	VkPipelineCreateFlags			flags;
	VkPipelineLayout				layout;
	VkPipeline						basePipelineHandle;
	deInt32							basePipelineIndex;
};

struct VkPushConstantRange
{
	VkShaderStageFlags	stageFlags;
	deUint32			start;
	deUint32			length;
};

struct VkPipelineLayoutCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						descriptorSetCount;
	const VkDescriptorSetLayout*	pSetLayouts;
	deUint32						pushConstantRangeCount;
	const VkPushConstantRange*		pPushConstantRanges;
};

struct VkSamplerCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkTexFilter		magFilter;
	VkTexFilter		minFilter;
	VkTexMipmapMode	mipMode;
	VkTexAddress	addressU;
	VkTexAddress	addressV;
	VkTexAddress	addressW;
	float			mipLodBias;
	float			maxAnisotropy;
	VkBool32		compareEnable;
	VkCompareOp		compareOp;
	float			minLod;
	float			maxLod;
	VkBorderColor	borderColor;
};

struct VkDescriptorSetLayoutBinding
{
	VkDescriptorType	descriptorType;
	deUint32			arraySize;
	VkShaderStageFlags	stageFlags;
	const VkSampler*	pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							count;
	const VkDescriptorSetLayoutBinding*	pBinding;
};

struct VkDescriptorTypeCount
{
	VkDescriptorType	type;
	deUint32			count;
};

struct VkDescriptorPoolCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						count;
	const VkDescriptorTypeCount*	pTypeCount;
};

struct VkDescriptorInfo
{
	VkBufferView		bufferView;
	VkSampler			sampler;
	VkImageView			imageView;
	VkAttachmentView	attachmentView;
	VkImageLayout		imageLayout;
};

struct VkWriteDescriptorSet
{
	VkStructureType			sType;
	const void*				pNext;
	VkDescriptorSet			destSet;
	deUint32				destBinding;
	deUint32				destArrayElement;
	deUint32				count;
	VkDescriptorType		descriptorType;
	const VkDescriptorInfo*	pDescriptors;
};

struct VkCopyDescriptorSet
{
	VkStructureType	sType;
	const void*		pNext;
	VkDescriptorSet	srcSet;
	deUint32		srcBinding;
	deUint32		srcArrayElement;
	VkDescriptorSet	destSet;
	deUint32		destBinding;
	deUint32		destArrayElement;
	deUint32		count;
};

struct VkViewport
{
	float	originX;
	float	originY;
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
	deInt32	width;
	deInt32	height;
};

struct VkRect2D
{
	VkOffset2D	offset;
	VkExtent2D	extent;
};

struct VkDynamicViewportStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			viewportAndScissorCount;
	const VkViewport*	pViewports;
	const VkRect2D*		pScissors;
};

struct VkDynamicRasterStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			depthBias;
	float			depthBiasClamp;
	float			slopeScaledDepthBias;
	float			lineWidth;
};

struct VkDynamicColorBlendStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			blendConst[4];
};

struct VkDynamicDepthStencilStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			minDepthBounds;
	float			maxDepthBounds;
	deUint32		stencilReadMask;
	deUint32		stencilWriteMask;
	deUint32		stencilFrontRef;
	deUint32		stencilBackRef;
};

struct VkAttachmentBindInfo
{
	VkAttachmentView	view;
	VkImageLayout		layout;
};

struct VkFramebufferCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkRenderPass				renderPass;
	deUint32					attachmentCount;
	const VkAttachmentBindInfo*	pAttachments;
	deUint32					width;
	deUint32					height;
	deUint32					layers;
};

struct VkAttachmentDescription
{
	VkStructureType		sType;
	const void*			pNext;
	VkFormat			format;
	deUint32			samples;
	VkAttachmentLoadOp	loadOp;
	VkAttachmentStoreOp	storeOp;
	VkAttachmentLoadOp	stencilLoadOp;
	VkAttachmentStoreOp	stencilStoreOp;
	VkImageLayout		initialLayout;
	VkImageLayout		finalLayout;
};

struct VkAttachmentReference
{
	deUint32		attachment;
	VkImageLayout	layout;
};

struct VkSubpassDescription
{
	VkStructureType					sType;
	const void*						pNext;
	VkPipelineBindPoint				pipelineBindPoint;
	VkSubpassDescriptionFlags		flags;
	deUint32						inputCount;
	const VkAttachmentReference*	inputAttachments;
	deUint32						colorCount;
	const VkAttachmentReference*	colorAttachments;
	const VkAttachmentReference*	resolveAttachments;
	VkAttachmentReference			depthStencilAttachment;
	deUint32						preserveCount;
	const VkAttachmentReference*	preserveAttachments;
};

struct VkSubpassDependency
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				srcSubpass;
	deUint32				destSubpass;
	VkPipelineStageFlags	srcStageMask;
	VkPipelineStageFlags	destStageMask;
	VkMemoryOutputFlags		outputMask;
	VkMemoryInputFlags		inputMask;
	VkBool32				byRegion;
};

struct VkRenderPassCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						attachmentCount;
	const VkAttachmentDescription*	pAttachments;
	deUint32						subpassCount;
	const VkSubpassDescription*		pSubpasses;
	deUint32						dependencyCount;
	const VkSubpassDependency*		pDependencies;
};

struct VkCmdPoolCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				queueFamilyIndex;
	VkCmdPoolCreateFlags	flags;
};

struct VkCmdBufferCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkCmdPool				cmdPool;
	VkCmdBufferLevel		level;
	VkCmdBufferCreateFlags	flags;
};

struct VkCmdBufferBeginInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkCmdBufferOptimizeFlags	flags;
	VkRenderPass				renderPass;
	VkFramebuffer				framebuffer;
};

struct VkBufferCopy
{
	VkDeviceSize	srcOffset;
	VkDeviceSize	destOffset;
	VkDeviceSize	copySize;
};

struct VkImageCopy
{
	VkImageSubresource	srcSubresource;
	VkOffset3D			srcOffset;
	VkImageSubresource	destSubresource;
	VkOffset3D			destOffset;
	VkExtent3D			extent;
};

struct VkImageBlit
{
	VkImageSubresource	srcSubresource;
	VkOffset3D			srcOffset;
	VkExtent3D			srcExtent;
	VkImageSubresource	destSubresource;
	VkOffset3D			destOffset;
	VkExtent3D			destExtent;
};

struct VkBufferImageCopy
{
	VkDeviceSize		bufferOffset;
	deUint32			bufferRowLength;
	deUint32			bufferImageHeight;
	VkImageSubresource	imageSubresource;
	VkOffset3D			imageOffset;
	VkExtent3D			imageExtent;
};

union VkClearColorValue
{
	float		f32[4];
	deInt32		s32[4];
	deUint32	u32[4];
};

struct VkRect3D
{
	VkOffset3D	offset;
	VkExtent3D	extent;
};

struct VkImageResolve
{
	VkImageSubresource	srcSubresource;
	VkOffset3D			srcOffset;
	VkImageSubresource	destSubresource;
	VkOffset3D			destOffset;
	VkExtent3D			extent;
};

struct VkClearDepthStencilValue
{
	float		depth;
	deUint32	stencil;
};

union VkClearValue
{
	VkClearColorValue			color;
	VkClearDepthStencilValue	ds;
};

struct VkRenderPassBeginInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkRenderPass		renderPass;
	VkFramebuffer		framebuffer;
	VkRect2D			renderArea;
	deUint32			attachmentCount;
	const VkClearValue*	pAttachmentClearValues;
};

struct VkBufferMemoryBarrier
{
	VkStructureType		sType;
	const void*			pNext;
	VkMemoryOutputFlags	outputMask;
	VkMemoryInputFlags	inputMask;
	deUint32			srcQueueFamilyIndex;
	deUint32			destQueueFamilyIndex;
	VkBuffer			buffer;
	VkDeviceSize		offset;
	VkDeviceSize		size;
};

struct VkDispatchIndirectCmd
{
	deUint32	x;
	deUint32	y;
	deUint32	z;
};

struct VkDrawIndexedIndirectCmd
{
	deUint32	indexCount;
	deUint32	instanceCount;
	deUint32	firstIndex;
	deInt32		vertexOffset;
	deUint32	firstInstance;
};

struct VkDrawIndirectCmd
{
	deUint32	vertexCount;
	deUint32	instanceCount;
	deUint32	firstVertex;
	deUint32	firstInstance;
};

struct VkImageMemoryBarrier
{
	VkStructureType			sType;
	const void*				pNext;
	VkMemoryOutputFlags		outputMask;
	VkMemoryInputFlags		inputMask;
	VkImageLayout			oldLayout;
	VkImageLayout			newLayout;
	deUint32				srcQueueFamilyIndex;
	deUint32				destQueueFamilyIndex;
	VkImage					image;
	VkImageSubresourceRange	subresourceRange;
};

struct VkMemoryBarrier
{
	VkStructureType		sType;
	const void*			pNext;
	VkMemoryOutputFlags	outputMask;
	VkMemoryInputFlags	inputMask;
};

