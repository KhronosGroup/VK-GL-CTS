/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
const char*	getResultName				(VkResult value);
const char*	getStructureTypeName		(VkStructureType value);
const char*	getSystemAllocTypeName		(VkSystemAllocType value);
const char*	getFormatName				(VkFormat value);
const char*	getImageTypeName			(VkImageType value);
const char*	getImageTilingName			(VkImageTiling value);
const char*	getPhysicalDeviceTypeName	(VkPhysicalDeviceType value);
const char*	getImageAspectName			(VkImageAspect value);
const char*	getQueryTypeName			(VkQueryType value);
const char*	getSharingModeName			(VkSharingMode value);
const char*	getImageLayoutName			(VkImageLayout value);
const char*	getImageViewTypeName		(VkImageViewType value);
const char*	getChannelSwizzleName		(VkChannelSwizzle value);
const char*	getShaderStageName			(VkShaderStage value);
const char*	getVertexInputStepRateName	(VkVertexInputStepRate value);
const char*	getPrimitiveTopologyName	(VkPrimitiveTopology value);
const char*	getFillModeName				(VkFillMode value);
const char*	getCullModeName				(VkCullMode value);
const char*	getFrontFaceName			(VkFrontFace value);
const char*	getCompareOpName			(VkCompareOp value);
const char*	getStencilOpName			(VkStencilOp value);
const char*	getLogicOpName				(VkLogicOp value);
const char*	getBlendName				(VkBlend value);
const char*	getBlendOpName				(VkBlendOp value);
const char*	getDynamicStateName			(VkDynamicState value);
const char*	getTexFilterName			(VkTexFilter value);
const char*	getTexMipmapModeName		(VkTexMipmapMode value);
const char*	getTexAddressModeName		(VkTexAddressMode value);
const char*	getBorderColorName			(VkBorderColor value);
const char*	getDescriptorTypeName		(VkDescriptorType value);
const char*	getDescriptorPoolUsageName	(VkDescriptorPoolUsage value);
const char*	getDescriptorSetUsageName	(VkDescriptorSetUsage value);
const char*	getAttachmentLoadOpName		(VkAttachmentLoadOp value);
const char*	getAttachmentStoreOpName	(VkAttachmentStoreOp value);
const char*	getPipelineBindPointName	(VkPipelineBindPoint value);
const char*	getCmdBufferLevelName		(VkCmdBufferLevel value);
const char*	getIndexTypeName			(VkIndexType value);
const char*	getTimestampTypeName		(VkTimestampType value);
const char*	getRenderPassContentsName	(VkRenderPassContents value);

inline tcu::Format::Enum<VkResult>				getResultStr				(VkResult value)				{ return tcu::Format::Enum<VkResult>(getResultName, value);								}
inline tcu::Format::Enum<VkStructureType>		getStructureTypeStr			(VkStructureType value)			{ return tcu::Format::Enum<VkStructureType>(getStructureTypeName, value);				}
inline tcu::Format::Enum<VkSystemAllocType>		getSystemAllocTypeStr		(VkSystemAllocType value)		{ return tcu::Format::Enum<VkSystemAllocType>(getSystemAllocTypeName, value);			}
inline tcu::Format::Enum<VkFormat>				getFormatStr				(VkFormat value)				{ return tcu::Format::Enum<VkFormat>(getFormatName, value);								}
inline tcu::Format::Enum<VkImageType>			getImageTypeStr				(VkImageType value)				{ return tcu::Format::Enum<VkImageType>(getImageTypeName, value);						}
inline tcu::Format::Enum<VkImageTiling>			getImageTilingStr			(VkImageTiling value)			{ return tcu::Format::Enum<VkImageTiling>(getImageTilingName, value);					}
inline tcu::Format::Enum<VkPhysicalDeviceType>	getPhysicalDeviceTypeStr	(VkPhysicalDeviceType value)	{ return tcu::Format::Enum<VkPhysicalDeviceType>(getPhysicalDeviceTypeName, value);		}
inline tcu::Format::Enum<VkImageAspect>			getImageAspectStr			(VkImageAspect value)			{ return tcu::Format::Enum<VkImageAspect>(getImageAspectName, value);					}
inline tcu::Format::Enum<VkQueryType>			getQueryTypeStr				(VkQueryType value)				{ return tcu::Format::Enum<VkQueryType>(getQueryTypeName, value);						}
inline tcu::Format::Enum<VkSharingMode>			getSharingModeStr			(VkSharingMode value)			{ return tcu::Format::Enum<VkSharingMode>(getSharingModeName, value);					}
inline tcu::Format::Enum<VkImageLayout>			getImageLayoutStr			(VkImageLayout value)			{ return tcu::Format::Enum<VkImageLayout>(getImageLayoutName, value);					}
inline tcu::Format::Enum<VkImageViewType>		getImageViewTypeStr			(VkImageViewType value)			{ return tcu::Format::Enum<VkImageViewType>(getImageViewTypeName, value);				}
inline tcu::Format::Enum<VkChannelSwizzle>		getChannelSwizzleStr		(VkChannelSwizzle value)		{ return tcu::Format::Enum<VkChannelSwizzle>(getChannelSwizzleName, value);				}
inline tcu::Format::Enum<VkShaderStage>			getShaderStageStr			(VkShaderStage value)			{ return tcu::Format::Enum<VkShaderStage>(getShaderStageName, value);					}
inline tcu::Format::Enum<VkVertexInputStepRate>	getVertexInputStepRateStr	(VkVertexInputStepRate value)	{ return tcu::Format::Enum<VkVertexInputStepRate>(getVertexInputStepRateName, value);	}
inline tcu::Format::Enum<VkPrimitiveTopology>	getPrimitiveTopologyStr		(VkPrimitiveTopology value)		{ return tcu::Format::Enum<VkPrimitiveTopology>(getPrimitiveTopologyName, value);		}
inline tcu::Format::Enum<VkFillMode>			getFillModeStr				(VkFillMode value)				{ return tcu::Format::Enum<VkFillMode>(getFillModeName, value);							}
inline tcu::Format::Enum<VkCullMode>			getCullModeStr				(VkCullMode value)				{ return tcu::Format::Enum<VkCullMode>(getCullModeName, value);							}
inline tcu::Format::Enum<VkFrontFace>			getFrontFaceStr				(VkFrontFace value)				{ return tcu::Format::Enum<VkFrontFace>(getFrontFaceName, value);						}
inline tcu::Format::Enum<VkCompareOp>			getCompareOpStr				(VkCompareOp value)				{ return tcu::Format::Enum<VkCompareOp>(getCompareOpName, value);						}
inline tcu::Format::Enum<VkStencilOp>			getStencilOpStr				(VkStencilOp value)				{ return tcu::Format::Enum<VkStencilOp>(getStencilOpName, value);						}
inline tcu::Format::Enum<VkLogicOp>				getLogicOpStr				(VkLogicOp value)				{ return tcu::Format::Enum<VkLogicOp>(getLogicOpName, value);							}
inline tcu::Format::Enum<VkBlend>				getBlendStr					(VkBlend value)					{ return tcu::Format::Enum<VkBlend>(getBlendName, value);								}
inline tcu::Format::Enum<VkBlendOp>				getBlendOpStr				(VkBlendOp value)				{ return tcu::Format::Enum<VkBlendOp>(getBlendOpName, value);							}
inline tcu::Format::Enum<VkDynamicState>		getDynamicStateStr			(VkDynamicState value)			{ return tcu::Format::Enum<VkDynamicState>(getDynamicStateName, value);					}
inline tcu::Format::Enum<VkTexFilter>			getTexFilterStr				(VkTexFilter value)				{ return tcu::Format::Enum<VkTexFilter>(getTexFilterName, value);						}
inline tcu::Format::Enum<VkTexMipmapMode>		getTexMipmapModeStr			(VkTexMipmapMode value)			{ return tcu::Format::Enum<VkTexMipmapMode>(getTexMipmapModeName, value);				}
inline tcu::Format::Enum<VkTexAddressMode>		getTexAddressModeStr		(VkTexAddressMode value)		{ return tcu::Format::Enum<VkTexAddressMode>(getTexAddressModeName, value);				}
inline tcu::Format::Enum<VkBorderColor>			getBorderColorStr			(VkBorderColor value)			{ return tcu::Format::Enum<VkBorderColor>(getBorderColorName, value);					}
inline tcu::Format::Enum<VkDescriptorType>		getDescriptorTypeStr		(VkDescriptorType value)		{ return tcu::Format::Enum<VkDescriptorType>(getDescriptorTypeName, value);				}
inline tcu::Format::Enum<VkDescriptorPoolUsage>	getDescriptorPoolUsageStr	(VkDescriptorPoolUsage value)	{ return tcu::Format::Enum<VkDescriptorPoolUsage>(getDescriptorPoolUsageName, value);	}
inline tcu::Format::Enum<VkDescriptorSetUsage>	getDescriptorSetUsageStr	(VkDescriptorSetUsage value)	{ return tcu::Format::Enum<VkDescriptorSetUsage>(getDescriptorSetUsageName, value);		}
inline tcu::Format::Enum<VkAttachmentLoadOp>	getAttachmentLoadOpStr		(VkAttachmentLoadOp value)		{ return tcu::Format::Enum<VkAttachmentLoadOp>(getAttachmentLoadOpName, value);			}
inline tcu::Format::Enum<VkAttachmentStoreOp>	getAttachmentStoreOpStr		(VkAttachmentStoreOp value)		{ return tcu::Format::Enum<VkAttachmentStoreOp>(getAttachmentStoreOpName, value);		}
inline tcu::Format::Enum<VkPipelineBindPoint>	getPipelineBindPointStr		(VkPipelineBindPoint value)		{ return tcu::Format::Enum<VkPipelineBindPoint>(getPipelineBindPointName, value);		}
inline tcu::Format::Enum<VkCmdBufferLevel>		getCmdBufferLevelStr		(VkCmdBufferLevel value)		{ return tcu::Format::Enum<VkCmdBufferLevel>(getCmdBufferLevelName, value);				}
inline tcu::Format::Enum<VkIndexType>			getIndexTypeStr				(VkIndexType value)				{ return tcu::Format::Enum<VkIndexType>(getIndexTypeName, value);						}
inline tcu::Format::Enum<VkTimestampType>		getTimestampTypeStr			(VkTimestampType value)			{ return tcu::Format::Enum<VkTimestampType>(getTimestampTypeName, value);				}
inline tcu::Format::Enum<VkRenderPassContents>	getRenderPassContentsStr	(VkRenderPassContents value)	{ return tcu::Format::Enum<VkRenderPassContents>(getRenderPassContentsName, value);		}

inline std::ostream&	operator<<	(std::ostream& s, VkResult value)				{ return s << getResultStr(value);				}
inline std::ostream&	operator<<	(std::ostream& s, VkStructureType value)		{ return s << getStructureTypeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkSystemAllocType value)		{ return s << getSystemAllocTypeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkFormat value)				{ return s << getFormatStr(value);				}
inline std::ostream&	operator<<	(std::ostream& s, VkImageType value)			{ return s << getImageTypeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkImageTiling value)			{ return s << getImageTilingStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkPhysicalDeviceType value)	{ return s << getPhysicalDeviceTypeStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkImageAspect value)			{ return s << getImageAspectStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkQueryType value)			{ return s << getQueryTypeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkSharingMode value)			{ return s << getSharingModeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkImageLayout value)			{ return s << getImageLayoutStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkImageViewType value)		{ return s << getImageViewTypeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkChannelSwizzle value)		{ return s << getChannelSwizzleStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkShaderStage value)			{ return s << getShaderStageStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkVertexInputStepRate value)	{ return s << getVertexInputStepRateStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkPrimitiveTopology value)	{ return s << getPrimitiveTopologyStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkFillMode value)				{ return s << getFillModeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkCullMode value)				{ return s << getCullModeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkFrontFace value)			{ return s << getFrontFaceStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkCompareOp value)			{ return s << getCompareOpStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkStencilOp value)			{ return s << getStencilOpStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkLogicOp value)				{ return s << getLogicOpStr(value);				}
inline std::ostream&	operator<<	(std::ostream& s, VkBlend value)				{ return s << getBlendStr(value);				}
inline std::ostream&	operator<<	(std::ostream& s, VkBlendOp value)				{ return s << getBlendOpStr(value);				}
inline std::ostream&	operator<<	(std::ostream& s, VkDynamicState value)			{ return s << getDynamicStateStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkTexFilter value)			{ return s << getTexFilterStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkTexMipmapMode value)		{ return s << getTexMipmapModeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkTexAddressMode value)		{ return s << getTexAddressModeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkBorderColor value)			{ return s << getBorderColorStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkDescriptorType value)		{ return s << getDescriptorTypeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkDescriptorPoolUsage value)	{ return s << getDescriptorPoolUsageStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkDescriptorSetUsage value)	{ return s << getDescriptorSetUsageStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkAttachmentLoadOp value)		{ return s << getAttachmentLoadOpStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkAttachmentStoreOp value)	{ return s << getAttachmentStoreOpStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkPipelineBindPoint value)	{ return s << getPipelineBindPointStr(value);	}
inline std::ostream&	operator<<	(std::ostream& s, VkCmdBufferLevel value)		{ return s << getCmdBufferLevelStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkIndexType value)			{ return s << getIndexTypeStr(value);			}
inline std::ostream&	operator<<	(std::ostream& s, VkTimestampType value)		{ return s << getTimestampTypeStr(value);		}
inline std::ostream&	operator<<	(std::ostream& s, VkRenderPassContents value)	{ return s << getRenderPassContentsStr(value);	}

tcu::Format::Bitfield<32>	getFormatFeatureFlagsStr			(VkFormatFeatureFlags value);
tcu::Format::Bitfield<32>	getImageUsageFlagsStr				(VkImageUsageFlags value);
tcu::Format::Bitfield<32>	getImageCreateFlagsStr				(VkImageCreateFlags value);
tcu::Format::Bitfield<32>	getSampleCountFlagsStr				(VkSampleCountFlags value);
tcu::Format::Bitfield<32>	getQueueFlagsStr					(VkQueueFlags value);
tcu::Format::Bitfield<32>	getMemoryPropertyFlagsStr			(VkMemoryPropertyFlags value);
tcu::Format::Bitfield<32>	getMemoryHeapFlagsStr				(VkMemoryHeapFlags value);
tcu::Format::Bitfield<32>	getSparseImageFormatFlagsStr		(VkSparseImageFormatFlags value);
tcu::Format::Bitfield<32>	getSparseMemoryBindFlagsStr			(VkSparseMemoryBindFlags value);
tcu::Format::Bitfield<32>	getFenceCreateFlagsStr				(VkFenceCreateFlags value);
tcu::Format::Bitfield<32>	getQueryPipelineStatisticFlagsStr	(VkQueryPipelineStatisticFlags value);
tcu::Format::Bitfield<32>	getQueryResultFlagsStr				(VkQueryResultFlags value);
tcu::Format::Bitfield<32>	getBufferUsageFlagsStr				(VkBufferUsageFlags value);
tcu::Format::Bitfield<32>	getBufferCreateFlagsStr				(VkBufferCreateFlags value);
tcu::Format::Bitfield<32>	getImageAspectFlagsStr				(VkImageAspectFlags value);
tcu::Format::Bitfield<32>	getImageViewCreateFlagsStr			(VkImageViewCreateFlags value);
tcu::Format::Bitfield<32>	getChannelFlagsStr					(VkChannelFlags value);
tcu::Format::Bitfield<32>	getPipelineCreateFlagsStr			(VkPipelineCreateFlags value);
tcu::Format::Bitfield<32>	getShaderStageFlagsStr				(VkShaderStageFlags value);
tcu::Format::Bitfield<32>	getAttachmentDescriptionFlagsStr	(VkAttachmentDescriptionFlags value);
tcu::Format::Bitfield<32>	getSubpassDescriptionFlagsStr		(VkSubpassDescriptionFlags value);
tcu::Format::Bitfield<32>	getPipelineStageFlagsStr			(VkPipelineStageFlags value);
tcu::Format::Bitfield<32>	getMemoryOutputFlagsStr				(VkMemoryOutputFlags value);
tcu::Format::Bitfield<32>	getMemoryInputFlagsStr				(VkMemoryInputFlags value);
tcu::Format::Bitfield<32>	getCmdPoolCreateFlagsStr			(VkCmdPoolCreateFlags value);
tcu::Format::Bitfield<32>	getCmdPoolResetFlagsStr				(VkCmdPoolResetFlags value);
tcu::Format::Bitfield<32>	getCmdBufferOptimizeFlagsStr		(VkCmdBufferOptimizeFlags value);
tcu::Format::Bitfield<32>	getCmdBufferResetFlagsStr			(VkCmdBufferResetFlags value);
tcu::Format::Bitfield<32>	getStencilFaceFlagsStr				(VkStencilFaceFlags value);
tcu::Format::Bitfield<32>	getQueryControlFlagsStr				(VkQueryControlFlags value);

std::ostream&	operator<<	(std::ostream& s, const VkApplicationInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkAllocCallbacks& value);
std::ostream&	operator<<	(std::ostream& s, const VkInstanceCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPhysicalDeviceFeatures& value);
std::ostream&	operator<<	(std::ostream& s, const VkFormatProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkExtent3D& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageFormatProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkPhysicalDeviceLimits& value);
std::ostream&	operator<<	(std::ostream& s, const VkPhysicalDeviceSparseProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkPhysicalDeviceProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkQueueFamilyProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkMemoryType& value);
std::ostream&	operator<<	(std::ostream& s, const VkMemoryHeap& value);
std::ostream&	operator<<	(std::ostream& s, const VkPhysicalDeviceMemoryProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkDeviceQueueCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkDeviceCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkExtensionProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkLayerProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkMemoryAllocInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkMappedMemoryRange& value);
std::ostream&	operator<<	(std::ostream& s, const VkMemoryRequirements& value);
std::ostream&	operator<<	(std::ostream& s, const VkSparseImageFormatProperties& value);
std::ostream&	operator<<	(std::ostream& s, const VkSparseImageMemoryRequirements& value);
std::ostream&	operator<<	(std::ostream& s, const VkSparseMemoryBindInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageSubresource& value);
std::ostream&	operator<<	(std::ostream& s, const VkOffset3D& value);
std::ostream&	operator<<	(std::ostream& s, const VkSparseImageMemoryBindInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkFenceCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkSemaphoreCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkEventCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkQueryPoolCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkBufferCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkBufferViewCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkSubresourceLayout& value);
std::ostream&	operator<<	(std::ostream& s, const VkChannelMapping& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageSubresourceRange& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageViewCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkShaderModuleCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkShaderCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineCacheCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkSpecializationMapEntry& value);
std::ostream&	operator<<	(std::ostream& s, const VkSpecializationInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineShaderStageCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkVertexInputBindingDescription& value);
std::ostream&	operator<<	(std::ostream& s, const VkVertexInputAttributeDescription& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineVertexInputStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineInputAssemblyStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineTessellationStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkViewport& value);
std::ostream&	operator<<	(std::ostream& s, const VkOffset2D& value);
std::ostream&	operator<<	(std::ostream& s, const VkExtent2D& value);
std::ostream&	operator<<	(std::ostream& s, const VkRect2D& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineViewportStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineRasterStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineMultisampleStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkStencilOpState& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineDepthStencilStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineColorBlendAttachmentState& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineColorBlendStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineDynamicStateCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkGraphicsPipelineCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkComputePipelineCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkPushConstantRange& value);
std::ostream&	operator<<	(std::ostream& s, const VkPipelineLayoutCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkSamplerCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorSetLayoutBinding& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorSetLayoutCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorTypeCount& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorPoolCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorBufferInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkDescriptorInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkWriteDescriptorSet& value);
std::ostream&	operator<<	(std::ostream& s, const VkCopyDescriptorSet& value);
std::ostream&	operator<<	(std::ostream& s, const VkFramebufferCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkAttachmentDescription& value);
std::ostream&	operator<<	(std::ostream& s, const VkAttachmentReference& value);
std::ostream&	operator<<	(std::ostream& s, const VkSubpassDescription& value);
std::ostream&	operator<<	(std::ostream& s, const VkSubpassDependency& value);
std::ostream&	operator<<	(std::ostream& s, const VkRenderPassCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkCmdPoolCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkCmdBufferCreateInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkCmdBufferBeginInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkBufferCopy& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageSubresourceCopy& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageCopy& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageBlit& value);
std::ostream&	operator<<	(std::ostream& s, const VkBufferImageCopy& value);
std::ostream&	operator<<	(std::ostream& s, const VkClearColorValue& value);
std::ostream&	operator<<	(std::ostream& s, const VkClearDepthStencilValue& value);
std::ostream&	operator<<	(std::ostream& s, const VkRect3D& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageResolve& value);
std::ostream&	operator<<	(std::ostream& s, const VkClearValue& value);
std::ostream&	operator<<	(std::ostream& s, const VkRenderPassBeginInfo& value);
std::ostream&	operator<<	(std::ostream& s, const VkBufferMemoryBarrier& value);
std::ostream&	operator<<	(std::ostream& s, const VkDispatchIndirectCmd& value);
std::ostream&	operator<<	(std::ostream& s, const VkDrawIndexedIndirectCmd& value);
std::ostream&	operator<<	(std::ostream& s, const VkDrawIndirectCmd& value);
std::ostream&	operator<<	(std::ostream& s, const VkImageMemoryBarrier& value);
std::ostream&	operator<<	(std::ostream& s, const VkMemoryBarrier& value);
