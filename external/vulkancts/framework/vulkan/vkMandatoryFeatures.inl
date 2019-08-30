/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
bool checkMandatoryFeatures(const vkt::Context& context)
{
	if ( !vk::isInstanceExtensionSupported(context.getUsedApiVersion(), context.getInstanceExtensions(), "VK_KHR_get_physical_device_properties2") )
		TCU_THROW(NotSupportedError, "Extension VK_KHR_get_physical_device_properties2 is not present");

	VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const InstanceInterface&			vki					= context.getInstanceInterface();
	const vector<VkExtensionProperties>	deviceExtensions	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	tcu::TestLog& log = context.getTestContext().getLog();
	vk::VkPhysicalDeviceFeatures2 coreFeatures;
	deMemset(&coreFeatures, 0, sizeof(coreFeatures));
	coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	void** nextPtr = &coreFeatures.pNext;

	vk::VkPhysicalDevice8BitStorageFeaturesKHR physicalDevice8BitStorageFeaturesKHR;
	deMemset(&physicalDevice8BitStorageFeaturesKHR, 0, sizeof(physicalDevice8BitStorageFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_8bit_storage")) )
	{
		physicalDevice8BitStorageFeaturesKHR.sType = getStructureType<VkPhysicalDevice8BitStorageFeaturesKHR>();
		*nextPtr = &physicalDevice8BitStorageFeaturesKHR;
		nextPtr  = &physicalDevice8BitStorageFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeaturesEXT;
	deMemset(&physicalDeviceDescriptorIndexingFeaturesEXT, 0, sizeof(physicalDeviceDescriptorIndexingFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		physicalDeviceDescriptorIndexingFeaturesEXT.sType = getStructureType<VkPhysicalDeviceDescriptorIndexingFeaturesEXT>();
		*nextPtr = &physicalDeviceDescriptorIndexingFeaturesEXT;
		nextPtr  = &physicalDeviceDescriptorIndexingFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceInlineUniformBlockFeaturesEXT physicalDeviceInlineUniformBlockFeaturesEXT;
	deMemset(&physicalDeviceInlineUniformBlockFeaturesEXT, 0, sizeof(physicalDeviceInlineUniformBlockFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) )
	{
		physicalDeviceInlineUniformBlockFeaturesEXT.sType = getStructureType<VkPhysicalDeviceInlineUniformBlockFeaturesEXT>();
		*nextPtr = &physicalDeviceInlineUniformBlockFeaturesEXT;
		nextPtr  = &physicalDeviceInlineUniformBlockFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
	deMemset(&physicalDeviceMultiviewFeatures, 0, sizeof(physicalDeviceMultiviewFeatures));

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) || isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_multiview")) )
	{
		physicalDeviceMultiviewFeatures.sType = getStructureType<VkPhysicalDeviceMultiviewFeatures>();
		*nextPtr = &physicalDeviceMultiviewFeatures;
		nextPtr  = &physicalDeviceMultiviewFeatures.pNext;
	}

	vk::VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR physicalDevicePipelineExecutablePropertiesFeaturesKHR;
	deMemset(&physicalDevicePipelineExecutablePropertiesFeaturesKHR, 0, sizeof(physicalDevicePipelineExecutablePropertiesFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_pipeline_executable_properties")) )
	{
		physicalDevicePipelineExecutablePropertiesFeaturesKHR.sType = getStructureType<VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR>();
		*nextPtr = &physicalDevicePipelineExecutablePropertiesFeaturesKHR;
		nextPtr  = &physicalDevicePipelineExecutablePropertiesFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT;
	deMemset(&physicalDeviceScalarBlockLayoutFeaturesEXT, 0, sizeof(physicalDeviceScalarBlockLayoutFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_scalar_block_layout")) )
	{
		physicalDeviceScalarBlockLayoutFeaturesEXT.sType = getStructureType<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT>();
		*nextPtr = &physicalDeviceScalarBlockLayoutFeaturesEXT;
		nextPtr  = &physicalDeviceScalarBlockLayoutFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
	deMemset(&physicalDeviceUniformBufferStandardLayoutFeaturesKHR, 0, sizeof(physicalDeviceUniformBufferStandardLayoutFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_uniform_buffer_standard_layout")) )
	{
		physicalDeviceUniformBufferStandardLayoutFeaturesKHR.sType = getStructureType<VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR>();
		*nextPtr = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
		nextPtr  = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceVariablePointersFeatures physicalDeviceVariablePointersFeatures;
	deMemset(&physicalDeviceVariablePointersFeatures, 0, sizeof(physicalDeviceVariablePointersFeatures));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_variable_pointers")) )
	{
		physicalDeviceVariablePointersFeatures.sType = getStructureType<VkPhysicalDeviceVariablePointersFeatures>();
		*nextPtr = &physicalDeviceVariablePointersFeatures;
		nextPtr  = &physicalDeviceVariablePointersFeatures.pNext;
	}

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);
	bool result = true;

	{
		if ( coreFeatures.features.robustBufferAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature robustBufferAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( coreFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( coreFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_8bit_storage")) )
	{
		if ( physicalDevice8BitStorageFeaturesKHR.storageBuffer8BitAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature storageBuffer8BitAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) )
	{
		if ( physicalDeviceMultiviewFeatures.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_multiview")) )
	{
		if ( physicalDeviceMultiviewFeatures.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_variable_pointers")) )
	{
		if ( physicalDeviceVariablePointersFeatures.variablePointersStorageBuffer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature variablePointersStorageBuffer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.inlineUniformBlock == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature inlineUniformBlock not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.descriptorBindingInlineUniformBlockUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingInlineUniformBlockUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_scalar_block_layout")) )
	{
		if ( physicalDeviceScalarBlockLayoutFeaturesEXT.scalarBlockLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature scalarBlockLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_uniform_buffer_standard_layout")) )
	{
		if ( physicalDeviceUniformBufferStandardLayoutFeaturesKHR.uniformBufferStandardLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature uniformBufferStandardLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_pipeline_executable_properties")) )
	{
		if ( physicalDevicePipelineExecutablePropertiesFeaturesKHR.pipelineExecutableInfo == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature pipelineExecutableInfo not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	return result;
}

