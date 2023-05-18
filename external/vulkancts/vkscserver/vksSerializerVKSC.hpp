#ifndef _VKSSERIALIZERVKSC_HPP
#define _VKSSERIALIZERVKSC_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 *-------------------------------------------------------------------------*/

#include "vksSerializer.hpp"
#include "vksJson.hpp"

#include "vkPrograms.hpp"

namespace vksc_server
{

template <typename ENUM>
inline void SerializeEnum (Serializer<ToWrite>& serializer, ENUM& v)
{
	static_assert(std::is_enum<ENUM>::value, "enums only");
	s32 raw = v;
	serializer.Serialize(raw);
}

template <typename ENUM>
inline void SerializeEnum (Serializer<ToRead>& serializer, ENUM& v)
{
	static_assert(std::is_enum<ENUM>::value, "enums only");
	s32 raw;
	serializer.Serialize(raw);
	v = static_cast<ENUM>(raw);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::SpirvVersion& v)
{
	SerializeEnum(serializer, v);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::SpirVAsmBuildOptions& v)
{
	serializer.Serialize(v.vulkanVersion, v.targetVersion, v.supports_VK_KHR_spirv_1_4);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::SpirVAsmSource& v)
{
	serializer.Serialize(v.buildOptions, v.source);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::SpirVProgramInfo& v)
{
	serializer.Serialize(v.source, v.infoLog, v.compileTimeUs, v.compileOk);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::ShaderBuildOptions& v)
{
	serializer.Serialize(v.vulkanVersion, v.targetVersion, v.flags, v.supports_VK_KHR_spirv_1_4);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::GlslSource& v)
{
	for (msize i{}; i < glu::SHADERTYPE_LAST; ++i)
	{
		serializer.Serialize(v.sources[i]);
	}
	serializer.Serialize(v.buildOptions);
}

template <typename TYPE>
inline void SerializeItem (Serializer<TYPE>& serializer, vk::HlslSource& v)
{
	for (msize i{}; i < glu::SHADERTYPE_LAST; ++i)
	{
		serializer.Serialize(v.sources[i]);
	}
	serializer.Serialize(v.buildOptions);
}

template <vk::HandleType VKTYPE>
inline void SerializeItem (Serializer<ToRead>& serializer, vk::Handle<VKTYPE>& v)
{
	u64 handle;
	serializer.Serialize(handle);
	v = handle;
}

template <vk::HandleType VKTYPE>
inline void SerializeItem (Serializer<ToWrite>& serializer, const vk::Handle<VKTYPE>& v)
{
	serializer.Serialize(v.getInternal());
}

inline void SerializeItem (Serializer<ToRead>& serializer, vk::VkDeviceObjectReservationCreateInfo& v)
{
	string input;
	serializer.Serialize(input);
	json::Context ctx;
	json::readJSON_VkDeviceObjectReservationCreateInfo(ctx, input, v);
}

inline void SerializeItem (Serializer<ToWrite>& serializer, vk::VkDeviceObjectReservationCreateInfo& v)
{
	string output = json::writeJSON_VkDeviceObjectReservationCreateInfo(v);
	serializer.Serialize(output);
}

inline void SerializeItem(Serializer<ToRead>& serializer, vk::VkPipelineOfflineCreateInfo& v)
{
	string input;
	serializer.Serialize(input);
	json::Context ctx;
	json::readJSON_VkPipelineOfflineCreateInfo(ctx, input, v);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, vk::VkPipelineOfflineCreateInfo& v)
{
	string output = json::writeJSON_VkPipelineOfflineCreateInfo(v);
	serializer.Serialize(output);
}

inline void SerializeItem(Serializer<ToRead>& serializer, vk::VkPhysicalDeviceFeatures2& v)
{
	string input;
	serializer.Serialize(input);
	json::Context ctx;
	json::readJSON_VkPhysicalDeviceFeatures2(ctx, input, v);
}

inline void SerializeItem(Serializer<ToWrite>& serializer, vk::VkPhysicalDeviceFeatures2& v)
{
	string output = json::writeJSON_VkPhysicalDeviceFeatures2(v);
	serializer.Serialize(output);
}

}

#endif // _VKSSERIALIZERVKSC_HPP
