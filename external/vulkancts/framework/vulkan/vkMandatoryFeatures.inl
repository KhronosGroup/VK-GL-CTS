/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
bool checkMandatoryFeatures(const vkt::Context& context)
{
	if ( !vk::isInstanceExtensionSupported(context.getUsedApiVersion(), context.getInstanceExtensions(), "VK_KHR_get_physical_device_properties2") )
		TCU_THROW(NotSupportedError, "Extension VK_KHR_get_physical_device_properties2 is not present");

	tcu::TestLog& log = context.getTestContext().getLog();
	vk::VkPhysicalDeviceFeatures2 coreFeatures;
	deMemset(&coreFeatures, 0, sizeof(coreFeatures));
	coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	void** nextPtr = &coreFeatures.pNext;

	vk::VkPhysicalDeviceInlineUniformBlockFeaturesEXT physicalDeviceInlineUniformBlockFeaturesEXT;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block"))
	{
		deMemset(&physicalDeviceInlineUniformBlockFeaturesEXT, 0, sizeof(physicalDeviceInlineUniformBlockFeaturesEXT));
		physicalDeviceInlineUniformBlockFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT;
		*nextPtr = &physicalDeviceInlineUniformBlockFeaturesEXT;
		nextPtr  = &physicalDeviceInlineUniformBlockFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_scalar_block_layout"))
	{
		deMemset(&physicalDeviceScalarBlockLayoutFeaturesEXT, 0, sizeof(physicalDeviceScalarBlockLayoutFeaturesEXT));
		physicalDeviceScalarBlockLayoutFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
		*nextPtr = &physicalDeviceScalarBlockLayoutFeaturesEXT;
		nextPtr  = &physicalDeviceScalarBlockLayoutFeaturesEXT.pNext;
	}

	vk::VkPhysicalDevice8BitStorageFeaturesKHR physicalDevice8BitStorageFeaturesKHR;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_8bit_storage"))
	{
		deMemset(&physicalDevice8BitStorageFeaturesKHR, 0, sizeof(physicalDevice8BitStorageFeaturesKHR));
		physicalDevice8BitStorageFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR;
		*nextPtr = &physicalDevice8BitStorageFeaturesKHR;
		nextPtr  = &physicalDevice8BitStorageFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeaturesEXT;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing"))
	{
		deMemset(&physicalDeviceDescriptorIndexingFeaturesEXT, 0, sizeof(physicalDeviceDescriptorIndexingFeaturesEXT));
		physicalDeviceDescriptorIndexingFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		*nextPtr = &physicalDeviceDescriptorIndexingFeaturesEXT;
		nextPtr  = &physicalDeviceDescriptorIndexingFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceVariablePointersFeatures physicalDeviceVariablePointersFeatures;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_variable_pointers"))
	{
		deMemset(&physicalDeviceVariablePointersFeatures, 0, sizeof(physicalDeviceVariablePointersFeatures));
		physicalDeviceVariablePointersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES;
		*nextPtr = &physicalDeviceVariablePointersFeatures;
		nextPtr  = &physicalDeviceVariablePointersFeatures.pNext;
	}

	vk::VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_multiview"))
	{
		deMemset(&physicalDeviceMultiviewFeatures, 0, sizeof(physicalDeviceMultiviewFeatures));
		physicalDeviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
		*nextPtr = &physicalDeviceMultiviewFeatures;
		nextPtr  = &physicalDeviceMultiviewFeatures.pNext;
	}

	vk::VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
	if (vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_uniform_buffer_standard_layout"))
	{
		deMemset(&physicalDeviceUniformBufferStandardLayoutFeaturesKHR, 0, sizeof(physicalDeviceUniformBufferStandardLayoutFeaturesKHR));
		physicalDeviceUniformBufferStandardLayoutFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR;
		*nextPtr = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
		nextPtr  = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR.pNext;
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

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( coreFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( coreFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_8bit_storage") )
	{
		if ( physicalDevice8BitStorageFeaturesKHR.storageBuffer8BitAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature storageBuffer8BitAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_multiview") )
	{
		if ( physicalDeviceMultiviewFeatures.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_variable_pointers") )
	{
		if ( physicalDeviceVariablePointersFeatures.variablePointersStorageBuffer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature variablePointersStorageBuffer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block") )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.inlineUniformBlock == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature inlineUniformBlock not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_inline_uniform_block") && vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_descriptor_indexing") )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.descriptorBindingInlineUniformBlockUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingInlineUniformBlockUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_EXT_scalar_block_layout") )
	{
		if ( physicalDeviceScalarBlockLayoutFeaturesEXT.scalarBlockLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature scalarBlockLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( vk::isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_uniform_buffer_standard_layout") )
	{
		if ( physicalDeviceUniformBufferStandardLayoutFeaturesKHR.uniformBufferStandardLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature uniformBufferStandardLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	return result;
}

