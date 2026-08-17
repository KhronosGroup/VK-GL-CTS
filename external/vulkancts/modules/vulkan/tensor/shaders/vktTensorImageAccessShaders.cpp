/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor Shader Utility Classes
 */
/*--------------------------------------------------------------------*/

#include "vktTensorShaderUtil.hpp"
#include "vktTensorShaders.hpp"

#include "deStringUtil.hpp"
#include "../vktTensorTestsUtil.hpp"

#include <sstream>
#include <string>

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string genShaderImageAccess(const TensorDimensions &shape, VkFormat bufferFormat, VkFormat imageFormat,
                                 ImageAliasingVariant variant)
{
    const bool sampledAccess = variant == ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE;

    const size_t tensorRank = shape.size();
    const size_t imageRank  = tensorRank - 1;
    DE_ASSERT(tensorRank > 1); // nD tensors map to (n-1)D image

    DE_ASSERT(imageRank < 4); // we support images from 1D to 3D

    const size_t componentsIdx = tensorRank - 1;
    const size_t components    = static_cast<size_t>(shape[componentsIdx]);
    DE_UNREF(components);
    DE_ASSERT(components < 5); // we support up to 4 components
    DE_ASSERT(components == getFormatComponents(imageFormat));

    const std::string bufferType        = getTensorFormat(bufferFormat);
    const std::string imageFormatString = getImageFormat(imageFormat);
    const std::string imageType         = getImageType(imageFormat, static_cast<size_t>(shape.size() - 1));

    const std::string imageCoordType   = imageRank == 1 ? "int" : (std::string("ivec") + std::to_string(imageRank));
    const std::string sampledCoordType = imageRank == 1 ? "float" : (std::string("vec") + std::to_string(imageRank));

    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_EXT_shader_explicit_arithmetic_types : require
)";
    if (isFormat64BitInteger(imageFormat))
    {
        shader << "#extension GL_EXT_shader_image_int64 : require\n";
    }

    shader << "layout(local_size_x = " << shaderImageAccessWorkgroupSize
           << ", local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set = 0, binding = 2, " << imageFormatString << ") uniform " << imageType << " image;\n";
    shader << "layout(set = 0, binding = 3, std430) buffer _buff { " << bufferType << " data[]; };\n";

    if (sampledAccess)
    {
        shader << "layout(set = 0, binding = 4) uniform " << getImagePrefix(imageFormat) << "sampler" << imageRank
               << "D"
               << " samplr;\n";
    }

    shader << "void main()\n{\n";

    shader << "\tconst uint buffer_idx = gl_GlobalInvocationID.x * " << components << ";\n";
    shader << "\tconst " << imageCoordType << " image_dims = imageSize(image);\n";

    // We annoyingly have to special case 1D images because there are no *vec1
    if (imageRank > 1)
    {
        // Check that the shader invocation is not after the end of the image
        // If it is, end early
        shader << "\tconst uint texel_count = ";
        for (size_t i = 0; i < imageRank; ++i)
        {
            shader << "image_dims[" << i << "] " << (i == imageRank - 1 ? "" : " * ");
        }
        shader << ";\n";
        shader << "\tif (gl_GlobalInvocationID.x >= texel_count) return;\n";

        for (size_t i = 0; i < imageRank; ++i)
        {
            shader << "\tconst uint image_coord_" << i << " = gl_GlobalInvocationID.x / (1";
            for (size_t j = 0; j < i; ++j)
            {
                shader << " * image_dims[" << j << "]";
            }
            shader << ") % image_dims[" << i << "];\n";
        }

        if (sampledAccess)
        {
            for (size_t i = 0; i < imageRank; ++i)
            {
                // Offset by half a pixel in positive direction
                shader << "\tconst float sampled_coord_" << i << " = (float(image_coord_" << i
                       << ") + 0.5) / float(image_dims[" << i << "]);\n";
            }
        }
    }
    else
    {
        // Check that the shader invocation is not after the end of the image
        // If it is, end early
        shader << "\tif (gl_GlobalInvocationID.x >= image_dims) return;\n";

        shader << "\tconst uint image_coord_0 = gl_GlobalInvocationID.x % image_dims;\n";

        if (sampledAccess)
        {
            // Offset by half a pixel in positive direction
            shader << "\tconst float sampled_coord_0 = (float(image_coord_0) + 0.5) / float(image_dims);\n";
        }
    }

    if (!sampledAccess)
    {
        shader << "\tconst " << imageCoordType << " image_coord = " << imageCoordType << "(";
        for (size_t i = 0; i < imageRank; ++i)
        {
            shader << "image_coord_" << i << (i == imageRank - 1 ? "" : ", ");
        }
        shader << ");\n";

        if (variant == ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_CONSUMER)
        {
            shader << "\tconst " << getImagePrefix(imageFormat) << "vec4 pixel_data = imageLoad(image, image_coord);\n";
            for (size_t i = 0; i < components; ++i)
            {
                shader << "\tdata[buffer_idx+" << i << "] = " << bufferType << "(pixel_data[" << i << "]);\n";
            }
        }
        else
        {
            shader << "\tconst " << getImagePrefix(imageFormat) << "vec4 pixel_data = " << getImagePrefix(imageFormat)
                   << "vec4(data[buffer_idx + 0], data[buffer_idx + 1], data[buffer_idx + 2], data[buffer_idx + 3]);\n";
            shader << "\timageStore(image, image_coord, pixel_data);\n";
        }
    }
    else
    {
        shader << "\tconst " << sampledCoordType << " sampled_coord = " << sampledCoordType << "(";
        for (size_t i = 0; i < imageRank; ++i)
        {
            shader << "sampled_coord_" << i << (i == imageRank - 1 ? "" : ", ");
        }
        shader << ");\n";

        shader << "\tconst " << getImagePrefix(imageFormat) << "vec4 pixel_data = texture(samplr, sampled_coord);\n";
        for (size_t i = 0; i < components; ++i)
        {
            shader << "\tdata[buffer_idx+" << i << "] = " << bufferType << "(pixel_data[" << i << "]);\n";
        }
    }

    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
