/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Cooperative Vector Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktCooperativeVectorUtils.hpp"
#include "deMath.h"

using namespace vk;

namespace vkt
{
namespace cooperative_vector
{

ComponentTypeInfo componentTypeInfo[] = {
    {"float16_t", "gl_ComponentTypeFloat16NV", 16},      {"float32_t", "gl_ComponentTypeFloat32NV", 32},
    {"float64_t", "gl_ComponentTypeFloat64NV", 64},      {"int8_t", "gl_ComponentTypeSignedInt8NV", 8},
    {"int16_t", "gl_ComponentTypeSignedInt16NV", 16},    {"int32_t", "gl_ComponentTypeSignedInt32NV", 32},
    {"int64_t", "gl_ComponentTypeSignedInt64NV", 64},    {"uint8_t", "gl_ComponentTypeUnsignedInt8NV", 8},
    {"uint16_t", "gl_ComponentTypeUnsignedInt16NV", 16}, {"uint32_t", "gl_ComponentTypeUnsignedInt32NV", 32},
    {"uint64_t", "gl_ComponentTypeUnsignedInt64NV", 64},
};

ComponentTypeInfo const getComponentTypeInfo(uint32_t idx)
{
    switch (idx)
    {
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
        return ComponentTypeInfo{"float16_t", "gl_ComponentTypeFloatE4M3NV", 8};
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
        return ComponentTypeInfo{"float16_t", "gl_ComponentTypeFloatE5M2NV", 8};
    case VK_COMPONENT_TYPE_SINT8_PACKED_NV:
        return ComponentTypeInfo{"int8_t", "gl_ComponentTypeSignedInt8PackedNV", 8};
    case VK_COMPONENT_TYPE_UINT8_PACKED_NV:
        return ComponentTypeInfo{"uint8_t", "gl_ComponentTypeUnsignedInt8PackedNV", 8};
    default:
        return componentTypeInfo[idx];
    }
}

bool isFloatType(VkComponentTypeKHR t)
{
    switch (t)
    {
    default:
        return false;
    case VK_COMPONENT_TYPE_FLOAT16_NV:
    case VK_COMPONENT_TYPE_FLOAT32_NV:
    case VK_COMPONENT_TYPE_FLOAT64_NV:
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
        return true;
    }
}

bool isSIntType(VkComponentTypeKHR t)
{
    switch (t)
    {
    default:
        return false;
    case VK_COMPONENT_TYPE_SINT8_NV:
    case VK_COMPONENT_TYPE_SINT16_NV:
    case VK_COMPONENT_TYPE_SINT32_NV:
    case VK_COMPONENT_TYPE_SINT64_NV:
        return true;
    }
}

void GetFloatExpManBits(VkComponentTypeKHR dt, uint32_t &expBits, uint32_t &manBits, uint32_t &byteSize)
{
    switch (dt)
    {
    case VK_COMPONENT_TYPE_FLOAT16_NV:
        expBits  = 5;
        manBits  = 10;
        byteSize = 2;
        break;
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
        expBits  = 4;
        manBits  = 3;
        byteSize = 1;
        break;
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
        expBits  = 5;
        manBits  = 2;
        byteSize = 1;
        break;
    default:
        DE_ASSERT(0);
        break;
    }
}

void setDataFloat(void *base, VkComponentTypeKHR dt, uint32_t i, float value)
{
    switch (dt)
    {
    case VK_COMPONENT_TYPE_FLOAT32_NV:
        ((float *)base)[i] = value;
        break;
    case VK_COMPONENT_TYPE_FLOAT16_NV:
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
    {
        uint32_t expBits = 0, manBits = 0, byteSize = 0;
        GetFloatExpManBits(dt, expBits, manBits, byteSize);
        uint32_t signBit = manBits + expBits;

        uint32_t intVal   = deFloatBitsToUint32(value);
        uint32_t sign     = intVal & 0x80000000;
        int32_t exp       = intVal & 0x7F800000;
        uint32_t mantissa = intVal & 0x007FFFFF;
        if (exp == 0x7F800000)
        {
            // E4M3 has no +/-inf encoding, so inf maps to NaN
            if (mantissa != 0 || (dt == VK_COMPONENT_TYPE_FLOAT_E4M3_NV))
            {
                exp      = (1 << expBits) - 1;
                mantissa = (1 << manBits) - 1;
                sign     = 0;
            }
            else
            {
                exp      = (1 << expBits) - 1;
                mantissa = 0;
            }
        }
        else
        {

            exp >>= 23;
            exp -= (1 << (8 - 1)) - 1;
            exp += (1 << (expBits - 1)) - 1;

            if (exp <= 0)
            {
                // If the denorm is too small, flush it to zero. Otherwise, add a leading one.
                if (-exp > (int32_t)manBits)
                {
                    value = 0;
                    exp   = 0;
                }
                else
                {
                    mantissa |= 1 << 23;
                }
                // RTNE
                if ((mantissa & (1 << (24 - manBits - exp))))
                {
                    mantissa++;
                }

                // Shift way the LSBs and the negative exponent
                mantissa += (1 << (23 - manBits - exp)) - 1;
                mantissa >>= 23 - manBits;
                mantissa >>= 1 - exp;
                exp = 0;
            }
            else
            {
                // RTNE
                if ((mantissa & (1 << (23 - manBits))))
                {
                    mantissa++;
                }
                mantissa += (1 << (22 - manBits)) - 1;
                if (mantissa & (1 << 23))
                {
                    exp += 1;
                    mantissa = 0;
                }
                mantissa >>= 23 - manBits;
            }

            if (exp >= (1 << expBits) - 1)
            {
                // E4M3 has no infinity, but if the exponent is too large it becomes NaN
                if (dt == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
                {
                    if (exp >= (1 << expBits))
                    {
                        exp      = (1 << expBits) - 1;
                        mantissa = (1 << manBits) - 1;
                        sign     = 0;
                    }
                }
                else
                {
                    exp      = (1 << expBits) - 1;
                    mantissa = 0;
                }
            }
        }
        sign >>= 31;
        sign <<= signBit;
        exp <<= manBits;
        uint32_t result = sign | exp | mantissa;
        DE_ASSERT(result < (1ULL << (byteSize * 8)));
        if (value == 0 && !deFloatIsIEEENaN(value))
        {
            result = sign;
        }
        deMemcpy(&((uint8_t *)base)[i * byteSize], &result, byteSize);
    }
    break;
    default:
        DE_ASSERT(0);
        break;
    }
}

float getDataFloat(void *base, VkComponentTypeKHR dt, uint32_t i)
{
    switch (dt)
    {
    case VK_COMPONENT_TYPE_FLOAT32_NV:
        return ((float *)base)[i];
    case VK_COMPONENT_TYPE_FLOAT16_NV:
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
    {
        uint32_t expBits = 0, manBits = 0, byteSize = 0;
        GetFloatExpManBits(dt, expBits, manBits, byteSize);
        uint32_t intVal = 0;
        deMemcpy(&intVal, &((uint8_t *)base)[i * byteSize], byteSize);

        uint32_t signBit  = manBits + expBits;
        uint32_t signMask = 1 << signBit;
        uint32_t expMask  = (1 << expBits) - 1;

        uint32_t sign     = intVal & signMask;
        uint32_t mantissa = intVal & ((1 << manBits) - 1);
        int32_t exp       = (intVal >> manBits) & expMask;

        // Check for the only E4M3 NaN encoding
        if (dt == VK_COMPONENT_TYPE_FLOAT_E4M3_NV && (intVal & 0x7F) == 0x7F)
        {
            exp      = 0xFF;
            mantissa = 0x7FFFFF;
        }
        else if (dt != VK_COMPONENT_TYPE_FLOAT_E4M3_NV && (uint32_t)exp == expMask)
        {
            // NaN or +/-infinity, depending on mantissa value
            exp = 0xFF;
            if (mantissa != 0)
            {
                mantissa = 0x7FFFFF;
            }
            else
            {
                mantissa = 0;
            }
        }
        else
        {
            if (exp == 0 && mantissa != 0)
            {
                // Shift the denorm value until it has a leading one, adjusting the exponent.
                // Then clear the leading one.
                while ((mantissa & (1 << manBits)) == 0)
                {
                    mantissa <<= 1;
                    exp--;
                }
                exp++;
                mantissa &= ~(1 << manBits);
            }
            exp -= (1 << (expBits - 1)) - 1;
            exp += (1 << (8 - 1)) - 1;
            mantissa <<= 23 - manBits;
        }
        exp <<= 23;
        sign <<= 31 - signBit;
        uint32_t result = sign | exp | mantissa;
        float ret       = (intVal == 0 || intVal == signMask) ? 0.0f : deUint32BitsToFloat(result);
        return ret;
    }
    default:
        DE_ASSERT(0);
        return 0.f;
    }
}

float getDataFloatOffsetIndex(void *base, VkComponentTypeKHR dt, uint32_t offset, uint32_t index)
{
    return getDataFloat(((uint8_t *)base) + offset, dt, index);
}

void setDataFloatOffsetIndex(void *base, VkComponentTypeKHR dt, uint32_t offset, uint32_t index, float value)
{
    setDataFloat(((uint8_t *)base) + offset, dt, index, value);
}

void setDataInt(void *base, VkComponentTypeKHR dt, uint32_t i, uint32_t value)
{
    DE_ASSERT(getComponentTypeInfo(dt).bits <= 32);
    switch (dt)
    {
    default:
        DE_ASSERT(0); // fallthrough
    case VK_COMPONENT_TYPE_UINT8_NV:
        ((uint8_t *)base)[i] = (uint8_t)value;
        break;
    case VK_COMPONENT_TYPE_UINT16_NV:
        ((uint16_t *)base)[i] = (uint16_t)value;
        break;
    case VK_COMPONENT_TYPE_UINT32_NV:
        ((uint32_t *)base)[i] = (uint32_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT8_NV:
        ((int8_t *)base)[i] = (int8_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT16_NV:
        ((int16_t *)base)[i] = (int16_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT32_NV:
        ((int32_t *)base)[i] = (int32_t)value;
        break;
    }
}

int64_t getDataInt(void *base, VkComponentTypeKHR dt, uint32_t i)
{
    DE_ASSERT(getComponentTypeInfo(dt).bits <= 32);
    switch (dt)
    {
    default:
        DE_ASSERT(0); // fallthrough
    case VK_COMPONENT_TYPE_UINT8_NV:
        return ((uint8_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT16_NV:
        return ((uint16_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT32_NV:
        return ((uint32_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT8_NV:
        return ((int8_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT16_NV:
        return ((int16_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT32_NV:
        return ((int32_t *)base)[i];
    case VK_COMPONENT_TYPE_FLOAT32_NV:
        return (int64_t)((float *)base)[i];
    }
}

int64_t getDataIntOffsetIndex(void *base, VkComponentTypeKHR dt, uint32_t offset, uint32_t index)
{
    return getDataInt(((uint8_t *)base) + offset, dt, index);
}

void setDataIntOffsetIndex(void *base, VkComponentTypeKHR dt, uint32_t offset, uint32_t index, uint32_t value)
{
    setDataInt(((uint8_t *)base) + offset, dt, index, value);
}

int64_t truncInt(int64_t x, VkComponentTypeKHR dt)
{
    DE_ASSERT(getComponentTypeInfo(dt).bits <= 32);
    switch (dt)
    {
    default:
        DE_ASSERT(0); // fallthrough
    case VK_COMPONENT_TYPE_UINT8_NV:
        return (uint8_t)x;
    case VK_COMPONENT_TYPE_UINT16_NV:
        return (uint16_t)x;
    case VK_COMPONENT_TYPE_UINT32_NV:
        return (uint32_t)x;
    case VK_COMPONENT_TYPE_SINT8_NV:
        return (int8_t)x;
    case VK_COMPONENT_TYPE_SINT16_NV:
        return (int16_t)x;
    case VK_COMPONENT_TYPE_SINT32_NV:
        return (int32_t)x;
    }
}

} // namespace cooperative_vector
} // namespace vkt
