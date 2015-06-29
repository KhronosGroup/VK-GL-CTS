/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
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

struct VkExtent2D
{
	deInt32	width;
	deInt32	height;
};

struct VkExtent3D
{
	deInt32	width;
	deInt32	height;
	deInt32	depth;
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

struct VkRect
{
	VkOffset2D	offset;
	VkExtent2D	extent;
};

struct VkChannelMapping
{
	VkChannelSwizzle	r;
	VkChannelSwizzle	g;
	VkChannelSwizzle	b;
	VkChannelSwizzle	a;
};

struct VkPhysicalDeviceProperties
{
	deUint32				apiVersion;
	deUint32				driverVersion;
	deUint32				vendorId;
	deUint32				deviceId;
	VkPhysicalDeviceType	deviceType;
	char					deviceName[VK_MAX_PHYSICAL_DEVICE_NAME];
	VkDeviceSize			maxInlineMemoryUpdateSize;
	deUint32				maxBoundDescriptorSets;
	deUint32				maxThreadGroupSize;
	deUint64				timestampFrequency;
	deUint32				multiColorAttachmentClears;
	deUint32				maxDescriptorSets;
	deUint32				maxViewports;
	deUint32				maxColorAttachments;
};

struct VkPhysicalDevicePerformance
{
	float	maxDeviceClock;
	float	aluPerClock;
	float	texPerClock;
	float	primsPerClock;
	float	pixelsPerClock;
};

struct VkPhysicalDeviceCompatibilityInfo
{
	VkPhysicalDeviceCompatibilityFlags	compatibilityFlags;
};

struct VkExtensionProperties
{
	char		extName[VK_MAX_EXTENSION_NAME];
	deUint32	version;
};

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

struct VkDeviceQueueCreateInfo
{
	deUint32	queueNodeIndex;
	deUint32	queueCount;
};

struct VkDeviceCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						queueRecordCount;
	const VkDeviceQueueCreateInfo*	pRequestedQueues;
	deUint32						extensionCount;
	const char*const*				ppEnabledExtensionNames;
	VkDeviceCreateFlags				flags;
};

struct VkInstanceCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	const VkApplicationInfo*	pAppInfo;
	const VkAllocCallbacks*		pAllocCb;
	deUint32					extensionCount;
	const char*const*			ppEnabledExtensionNames;
};

struct VkLayerCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			layerCount;
	const char *const*	ppActiveLayerNames;
};

struct VkPhysicalDeviceQueueProperties
{
	VkQueueFlags	queueFlags;
	deUint32		queueCount;
	deUint32		maxAtomicCounters;
	deUint32		supportsTimestamps;
	deUint32		maxMemReferences;
};

struct VkPhysicalDeviceMemoryProperties
{
	deUint32	supportsMigration;
	deUint32	supportsPinning;
};

struct VkMemoryAllocInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkDeviceSize			allocationSize;
	VkMemoryPropertyFlags	memProps;
	VkMemoryPriority		memPriority;
};

struct VkMemoryOpenInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	sharedMem;
};

struct VkPeerMemoryOpenInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkDeviceMemory	originalMem;
};

struct VkMemoryRequirements
{
	VkDeviceSize			size;
	VkDeviceSize			alignment;
	VkDeviceSize			granularity;
	VkMemoryPropertyFlags	memPropsAllowed;
	VkMemoryPropertyFlags	memPropsRequired;
};

struct VkFormatProperties
{
	VkFormatFeatureFlags	linearTilingFeatures;
	VkFormatFeatureFlags	optimalTilingFeatures;
};

struct VkBufferViewAttachInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkBufferView	view;
};

struct VkImageViewAttachInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkImageView		view;
	VkImageLayout	layout;
};

struct VkUpdateSamplers
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			binding;
	deUint32			arrayIndex;
	deUint32			count;
	const VkSampler*	pSamplers;
};

struct VkSamplerImageViewInfo
{
	VkSampler						sampler;
	const VkImageViewAttachInfo*	pImageView;
};

struct VkUpdateSamplerTextures
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						binding;
	deUint32						arrayIndex;
	deUint32						count;
	const VkSamplerImageViewInfo*	pSamplerImageViews;
};

struct VkUpdateImages
{
	VkStructureType					sType;
	const void*						pNext;
	VkDescriptorType				descriptorType;
	deUint32						binding;
	deUint32						arrayIndex;
	deUint32						count;
	const VkImageViewAttachInfo*	pImageViews;
};

struct VkUpdateBuffers
{
	VkStructureType					sType;
	const void*						pNext;
	VkDescriptorType				descriptorType;
	deUint32						binding;
	deUint32						arrayIndex;
	deUint32						count;
	const VkBufferViewAttachInfo*	pBufferViews;
};

struct VkUpdateAsCopy
{
	VkStructureType		sType;
	const void*			pNext;
	VkDescriptorType	descriptorType;
	VkDescriptorSet		descriptorSet;
	deUint32			binding;
	deUint32			arrayElement;
	deUint32			count;
};

struct VkBufferCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkDeviceSize		size;
	VkBufferUsageFlags	usage;
	VkBufferCreateFlags	flags;
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

struct VkImageSubresource
{
	VkImageAspect	aspect;
	deUint32		mipLevel;
	deUint32		arraySlice;
};

struct VkImageSubresourceRange
{
	VkImageAspect	aspect;
	deUint32		baseMipLevel;
	deUint32		mipLevels;
	deUint32		baseArraySlice;
	deUint32		arraySize;
};

struct VkMemoryBarrier
{
	VkStructureType		sType;
	const void*			pNext;
	VkMemoryOutputFlags	outputMask;
	VkMemoryInputFlags	inputMask;
};

struct VkBufferMemoryBarrier
{
	VkStructureType		sType;
	const void*			pNext;
	VkMemoryOutputFlags	outputMask;
	VkMemoryInputFlags	inputMask;
	VkBuffer			buffer;
	VkDeviceSize		offset;
	VkDeviceSize		size;
};

struct VkImageMemoryBarrier
{
	VkStructureType			sType;
	const void*				pNext;
	VkMemoryOutputFlags		outputMask;
	VkMemoryInputFlags		inputMask;
	VkImageLayout			oldLayout;
	VkImageLayout			newLayout;
	VkImage					image;
	VkImageSubresourceRange	subresourceRange;
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
};

struct VkPeerImageOpenInfo
{
	VkImage	originalImage;
};

struct VkSubresourceLayout
{
	VkDeviceSize	offset;
	VkDeviceSize	size;
	VkDeviceSize	rowPitch;
	VkDeviceSize	depthPitch;
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
	float					minLod;
};

struct VkColorAttachmentViewCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkImage					image;
	VkFormat				format;
	deUint32				mipLevel;
	deUint32				baseArraySlice;
	deUint32				arraySize;
	VkImage					msaaResolveImage;
	VkImageSubresourceRange	msaaResolveSubResource;
};

struct VkDepthStencilViewCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkImage							image;
	deUint32						mipLevel;
	deUint32						baseArraySlice;
	deUint32						arraySize;
	VkImage							msaaResolveImage;
	VkImageSubresourceRange			msaaResolveSubResource;
	VkDepthStencilViewCreateFlags	flags;
};

struct VkColorAttachmentBindInfo
{
	VkColorAttachmentView	view;
	VkImageLayout			layout;
};

struct VkDepthStencilBindInfo
{
	VkDepthStencilView	view;
	VkImageLayout		layout;
};

struct VkBufferCopy
{
	VkDeviceSize	srcOffset;
	VkDeviceSize	destOffset;
	VkDeviceSize	copySize;
};

struct VkImageMemoryBindInfo
{
	VkImageSubresource	subresource;
	VkOffset3D			offset;
	VkExtent3D			extent;
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
	VkImageSubresource	imageSubresource;
	VkOffset3D			imageOffset;
	VkExtent3D			imageExtent;
};

struct VkImageResolve
{
	VkImageSubresource	srcSubresource;
	VkOffset3D			srcOffset;
	VkImageSubresource	destSubresource;
	VkOffset3D			destOffset;
	VkExtent3D			extent;
};

struct VkShaderCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUintptr			codeSize;
	const void*			pCode;
	VkShaderCreateFlags	flags;
};

struct VkDescriptorSetLayoutBinding
{
	VkDescriptorType	descriptorType;
	deUint32			count;
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

struct VkLinkConstBuffer
{
	deUint32	bufferId;
	deUintptr	bufferSize;
	const void*	pBufferData;
};

struct VkSpecializationMapEntry
{
	deUint32	constantId;
	deUint32	offset;
};

struct VkSpecializationInfo
{
	deUint32						mapEntryCount;
	const VkSpecializationMapEntry*	pMap;
	const void*						pData;
};

struct VkPipelineShader
{
	VkShaderStage				stage;
	VkShader					shader;
	deUint32					linkConstBufferCount;
	const VkLinkConstBuffer*	pLinkConstBufferInfo;
	const VkSpecializationInfo*	pSpecializationInfo;
};

struct VkComputePipelineCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkPipelineShader		cs;
	VkPipelineCreateFlags	flags;
	VkPipelineLayout		layout;
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

struct VkPipelineVertexInputCreateInfo
{
	VkStructureType								sType;
	const void*									pNext;
	deUint32									bindingCount;
	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
	deUint32									attributeCount;
	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
};

struct VkPipelineIaStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkPrimitiveTopology	topology;
	deUint32			disableVertexReuse;
	deUint32			primitiveRestartEnable;
	deUint32			primitiveRestartIndex;
};

struct VkPipelineTessStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		patchControlPoints;
};

struct VkPipelineVpStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			viewportCount;
	VkCoordinateOrigin	clipOrigin;
	VkDepthMode			depthMode;
};

struct VkPipelineRsStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			depthClipEnable;
	deUint32			rasterizerDiscardEnable;
	deUint32			programPointSize;
	VkCoordinateOrigin	pointOrigin;
	VkProvokingVertex	provokingVertex;
	VkFillMode			fillMode;
	VkCullMode			cullMode;
	VkFrontFace			frontFace;
};

struct VkPipelineMsStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	deUint32		samples;
	deUint32		multisampleEnable;
	deUint32		sampleShadingEnable;
	float			minSampleShading;
	VkSampleMask	sampleMask;
};

struct VkPipelineCbAttachmentState
{
	deUint32		blendEnable;
	VkFormat		format;
	VkBlend			srcBlendColor;
	VkBlend			destBlendColor;
	VkBlendOp		blendOpColor;
	VkBlend			srcBlendAlpha;
	VkBlend			destBlendAlpha;
	VkBlendOp		blendOpAlpha;
	VkChannelFlags	channelWriteMask;
};

struct VkPipelineCbStateCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							alphaToCoverageEnable;
	deUint32							logicOpEnable;
	VkLogicOp							logicOp;
	deUint32							attachmentCount;
	const VkPipelineCbAttachmentState*	pAttachments;
};

struct VkStencilOpState
{
	VkStencilOp	stencilFailOp;
	VkStencilOp	stencilPassOp;
	VkStencilOp	stencilDepthFailOp;
	VkCompareOp	stencilCompareOp;
};

struct VkPipelineDsStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkFormat			format;
	deUint32			depthTestEnable;
	deUint32			depthWriteEnable;
	VkCompareOp			depthCompareOp;
	deUint32			depthBoundsEnable;
	deUint32			stencilTestEnable;
	VkStencilOpState	front;
	VkStencilOpState	back;
};

struct VkPipelineShaderStageCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkPipelineShader	shader;
};

struct VkGraphicsPipelineCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	VkPipelineCreateFlags	flags;
	VkPipelineLayout		layout;
};

struct VkPipelineLayoutCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	deUint32						descriptorSetCount;
	const VkDescriptorSetLayout*	pSetLayouts;
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
	deUint32		maxAnisotropy;
	VkCompareOp		compareOp;
	float			minLod;
	float			maxLod;
	VkBorderColor	borderColor;
};

struct VkDynamicVpStateCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	deUint32			viewportAndScissorCount;
	const VkViewport*	pViewports;
	const VkRect*		pScissors;
};

struct VkDynamicRsStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			depthBias;
	float			depthBiasClamp;
	float			slopeScaledDepthBias;
	float			pointSize;
	float			pointFadeThreshold;
	float			lineWidth;
};

struct VkDynamicCbStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			blendConst[4];
};

struct VkDynamicDsStateCreateInfo
{
	VkStructureType	sType;
	const void*		pNext;
	float			minDepth;
	float			maxDepth;
	deUint32		stencilReadMask;
	deUint32		stencilWriteMask;
	deUint32		stencilFrontRef;
	deUint32		stencilBackRef;
};

struct VkCmdBufferCreateInfo
{
	VkStructureType			sType;
	const void*				pNext;
	deUint32				queueNodeIndex;
	VkCmdBufferCreateFlags	flags;
};

struct VkCmdBufferBeginInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkCmdBufferOptimizeFlags	flags;
};

struct VkRenderPassBegin
{
	VkRenderPass	renderPass;
	VkFramebuffer	framebuffer;
};

struct VkCmdBufferGraphicsBeginInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkRenderPassBegin	renderPassContinue;
};

struct VkClearColor
{
	VkClearColorValue	color;
	deUint32			useRawValue;
};

struct VkRenderPassCreateInfo
{
	VkStructureType				sType;
	const void*					pNext;
	VkRect						renderArea;
	deUint32					colorAttachmentCount;
	VkExtent2D					extent;
	deUint32					sampleCount;
	deUint32					layers;
	const VkFormat*				pColorFormats;
	const VkImageLayout*		pColorLayouts;
	const VkAttachmentLoadOp*	pColorLoadOps;
	const VkAttachmentStoreOp*	pColorStoreOps;
	const VkClearColor*			pColorLoadClearValues;
	VkFormat					depthStencilFormat;
	VkImageLayout				depthStencilLayout;
	VkAttachmentLoadOp			depthLoadOp;
	float						depthLoadClearValue;
	VkAttachmentStoreOp			depthStoreOp;
	VkAttachmentLoadOp			stencilLoadOp;
	deUint32					stencilLoadClearValue;
	VkAttachmentStoreOp			stencilStoreOp;
};

struct VkEventCreateInfo
{
	VkStructureType		sType;
	const void*			pNext;
	VkEventCreateFlags	flags;
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
	deUint32				initialCount;
	VkSemaphoreCreateFlags	flags;
};

struct VkSemaphoreOpenInfo
{
	VkStructureType	sType;
	const void*		pNext;
	VkSemaphore		sharedSemaphore;
};

struct VkQueryPoolCreateInfo
{
	VkStructureType					sType;
	const void*						pNext;
	VkQueryType						queryType;
	deUint32						slots;
	VkQueryPipelineStatisticFlags	pipelineStatistics;
};

struct VkFramebufferCreateInfo
{
	VkStructureType						sType;
	const void*							pNext;
	deUint32							colorAttachmentCount;
	const VkColorAttachmentBindInfo*	pColorAttachments;
	const VkDepthStencilBindInfo*		pDepthStencilAttachment;
	deUint32							sampleCount;
	deUint32							width;
	deUint32							height;
	deUint32							layers;
};

struct VkDrawIndirectCmd
{
	deUint32	vertexCount;
	deUint32	instanceCount;
	deUint32	firstVertex;
	deUint32	firstInstance;
};

struct VkDrawIndexedIndirectCmd
{
	deUint32	indexCount;
	deUint32	instanceCount;
	deUint32	firstIndex;
	deInt32		vertexOffset;
	deUint32	firstInstance;
};

struct VkDispatchIndirectCmd
{
	deUint32	x;
	deUint32	y;
	deUint32	z;
};

