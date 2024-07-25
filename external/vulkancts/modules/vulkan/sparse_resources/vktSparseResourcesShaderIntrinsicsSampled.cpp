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
 *//*
 * \file  vktSparseResourcesShaderIntrinsicsSampled.cpp
 * \brief Sparse Resources Shader Intrinsics for sampled images
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesShaderIntrinsicsSampled.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

Move<VkPipeline> makeGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                      const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                      const VkShaderModule vertexModule, const VkShaderModule fragmentModule,
                                      const VkShaderModule geometryModule)
{
    const std::vector<VkViewport> noViewports;
    const std::vector<VkRect2D> noScissors;

    const VkFormat format = VK_FORMAT_R32G32_SFLOAT;
    const uint32_t size   = tcu::getPixelSize(mapVkFormat(format));

    const VkVertexInputBindingDescription vertexBinding = {
        0u,                         // uint32_t binding;
        size * 2,                   // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        // position
        {
            0u,     // uint32_t location;
            0u,     // uint32_t binding;
            format, // VkFormat format;
            0u      // uint32_t offset;
        },
        // texture coordinates
        {
            1u,     // uint32_t location;
            0u,     // uint32_t binding;
            format, // VkFormat format;
            size    // uint32_t offset;
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                   // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexBinding,                  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        2u,                              // uint32_t vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkColorComponentFlags colorComponentsAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    const VkPipelineColorBlendAttachmentState defaultColorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorComponentsAll    // VkColorComponentFlags colorWriteMask;
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[] = {defaultColorBlendAttachmentState,
                                                                              defaultColorBlendAttachmentState};

    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        DE_LENGTH_OF_ARRAY(colorBlendAttachmentStates),           // uint32_t attachmentCount;
        colorBlendAttachmentStates, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f}    // float blendConstants[4];
    };

    return vk::makeGraphicsPipeline(
        vk,             // const DeviceInterface&                            vk
        device,         // const VkDevice                                    device
        pipelineLayout, // const VkPipelineLayout                            pipelineLayout
        vertexModule,   // const VkShaderModule                                vertexShaderModule
        VK_NULL_HANDLE, // const VkShaderModule                                tessellationControlModule
        VK_NULL_HANDLE, // const VkShaderModule                                tessellationEvalModule
        geometryModule, // const VkShaderModule                                geometryShaderModule
        fragmentModule, // const VkShaderModule                                fragmentShaderModule
        renderPass,     // const VkRenderPass                                renderPass
        noViewports,    // const std::vector<VkViewport>&                    viewports
        noScissors,     // const std::vector<VkRect2D>&                        scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                        topology
        0u,                                   // const uint32_t                                    subpass
        0u,                                   // const uint32_t                                    patchControlPoints
        &vertexInputStateCreateInfo, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
        DE_NULL,                     // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        DE_NULL,                     // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
        DE_NULL,                     // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
        &pipelineColorBlendStateInfo); // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo
}

} // namespace

void SparseShaderIntrinsicsCaseSampledBase::initPrograms(vk::SourceCollections &programCollection) const
{
    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(m_format);
    const uint32_t numLayers                        = getNumLayers(m_imageType, m_imageSize);
    const std::string coordString =
        getShaderImageCoordinates(m_imageType, "%local_texCoord_x", "%local_texCoord_xy", "%local_texCoord_xyz");

    // Create vertex shader
    std::ostringstream vs;

    vs << "#version 440\n"
       << "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
       << "#extension GL_EXT_shader_image_int64 : require\n"
       << "layout(location = 0) in highp vec2 vs_in_position;\n"
       << "layout(location = 1) in highp vec2 vs_in_texCoord;\n"
       << "\n"
       << "layout(location = 0) out highp vec3 vs_out_texCoord;\n"
       << "\n"
       << "out gl_PerVertex {\n"
       << "    vec4 gl_Position;\n"
       << "};\n"
       << "void main (void)\n"
       << "{\n"
       << "    gl_Position = vec4(vs_in_position, 0.0f, 1.0f);\n"
       << "    vs_out_texCoord = vec3(vs_in_texCoord, 0.0f);\n"
       << "}\n";

    programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

    if (numLayers > 1u)
    {
        const int32_t maxVertices = 3u * numLayers;

        // Create geometry shader
        std::ostringstream gs;

        gs << "#version 440\n"
           << "layout(triangles) in;\n"
           << "layout(triangle_strip, max_vertices = " << static_cast<int32_t>(maxVertices) << ") out;\n"
           << "\n"
           << "in gl_PerVertex {\n"
           << "    vec4  gl_Position;\n"
           << "} gl_in[];\n"
           << "out gl_PerVertex {\n"
           << "    vec4  gl_Position;\n"
           << "};\n"
           << "layout(location = 0) in  highp vec3 gs_in_texCoord[];\n"
           << "\n"
           << "layout(location = 0) out highp vec3 gs_out_texCoord;\n"
           << "\n"
           << "void main (void)\n"
           << "{\n"
           << "    for (int layerNdx = 0; layerNdx < " << static_cast<int32_t>(numLayers) << "; ++layerNdx)\n"
           << "    {\n"
           << "        for (int vertexNdx = 0; vertexNdx < gl_in.length(); ++vertexNdx)\n"
           << "        {\n"
           << "            gl_Layer = layerNdx;\n"
           << "            gl_Position = gl_in[vertexNdx].gl_Position;\n"
           << "            gs_out_texCoord = vec3(gs_in_texCoord[vertexNdx].xy, float(layerNdx));\n"
           << "            EmitVertex();\n"
           << "        }\n"
           << "        EndPrimitive();\n"
           << "    }\n"
           << "}\n";

        programCollection.glslSources.add("geometry_shader") << glu::GeometrySource(gs.str());
    }

    // Create fragment shader
    std::ostringstream fs;

    const std::string typeImgComp     = getImageComponentTypeName(formatDescription);
    const std::string typeImgCompVec4 = getImageComponentVec4TypeName(formatDescription);

    SpirvVersion spirvVersion = SPIRV_VERSION_1_0;
    std::string interfaceList = "";

    if (m_operand.find("Nontemporal") != std::string::npos)
    {
        spirvVersion  = SPIRV_VERSION_1_6;
        interfaceList = " %uniformconst_image_sparse %uniformblock_instance";
    }

    fs << "OpCapability Shader\n"
       << "OpCapability SampledCubeArray\n"
       << "OpCapability ImageCubeArray\n"
       << "OpCapability SparseResidency\n"
       << "OpCapability StorageImageExtendedFormats\n";

    if (formatIsR64(m_format))
    {
        fs << "OpCapability Int64\n"
           << "OpCapability Int64ImageEXT\n"
           << "OpExtension \"SPV_EXT_shader_image_int64\"\n";
    }

    fs << "%ext_import = OpExtInstImport \"GLSL.std.450\"\n"
       << "OpMemoryModel Logical GLSL450\n"
       << "OpEntryPoint Fragment %func_main \"main\" %varying_texCoord %output_texel %output_residency "
       << interfaceList << "\n"
       << "OpExecutionMode %func_main OriginUpperLeft\n"
       << "OpSource GLSL 440\n"

       << "OpName %func_main \"main\"\n"

       << "OpName %varying_texCoord \"varying_texCoord\"\n"

       << "OpName %output_texel \"out_texel\"\n"
       << "OpName %output_residency \"out_residency\"\n"

       << "OpName %type_uniformblock \"LodBlock\"\n"
       << "OpMemberName %type_uniformblock 0 \"lod\"\n"
       << "OpMemberName %type_uniformblock 1 \"size\"\n"
       << "OpName %uniformblock_instance \"lodInstance\"\n"

       << "OpName %uniformconst_image_sparse \"u_imageSparse\"\n"

       << "OpDecorate %varying_texCoord Location 0\n"

       << "OpDecorate %output_texel     Location 0\n"
       << "OpDecorate %output_residency Location 1\n"

       << "OpDecorate         %type_uniformblock Block\n"
       << "OpMemberDecorate %type_uniformblock 0 Offset 0\n"
       << "OpMemberDecorate %type_uniformblock 1 Offset 8\n"

       << "OpDecorate %uniformconst_image_sparse DescriptorSet 0\n"
       << "OpDecorate %uniformconst_image_sparse Binding " << BINDING_IMAGE_SPARSE << "\n"

       << "%type_void = OpTypeVoid\n"
       << "%type_void_func = OpTypeFunction %type_void\n"

       << "%type_bool = OpTypeBool\n"
       << "%type_int = OpTypeInt 32 1\n"
       << "%type_uint = OpTypeInt 32 0\n"
       << "%type_float = OpTypeFloat 32\n"
       << "%type_vec2 = OpTypeVector %type_float 2\n"
       << "%type_vec3 = OpTypeVector %type_float 3\n"
       << "%type_vec4 = OpTypeVector %type_float 4\n"
       << "%type_ivec4 = OpTypeVector %type_int 4\n"
       << "%type_uvec4 = OpTypeVector %type_uint 4\n"
       << "%type_uniformblock = OpTypeStruct %type_uint %type_vec2\n";

    if (formatIsR64(m_format))
    {
        fs << "%type_int64 = OpTypeInt 64 1\n"
           << "%type_uint64 = OpTypeInt 64 0\n"
           << "%type_i64vec2 = OpTypeVector %type_int64 2\n"
           << "%type_i64vec3 = OpTypeVector %type_int64 3\n"
           << "%type_i64vec4 = OpTypeVector %type_int64 4\n"
           << "%type_u64vec3 = OpTypeVector %type_uint64 3\n"
           << "%type_u64vec4 = OpTypeVector %type_uint64 4\n";
    }

    fs << "%type_struct_int_img_comp_vec4 = OpTypeStruct %type_int " << typeImgCompVec4 << "\n"
       << "%type_input_vec3 = OpTypePointer Input %type_vec3\n"
       << "%type_input_float = OpTypePointer Input %type_float\n";

    if (formatIsR64(m_format))
        fs << "%type_output_img_comp_vec4 = OpTypePointer Output "
           << "%type_ivec4"
           << "\n";
    else
        fs << "%type_output_img_comp_vec4 = OpTypePointer Output " << typeImgCompVec4 << "\n";

    fs << "%type_output_uint = OpTypePointer Output %type_uint\n"

       << "%type_function_int = OpTypePointer Function %type_int\n"
       << "%type_function_img_comp_vec4 = OpTypePointer Function " << typeImgCompVec4 << "\n"
       << "%type_function_int_img_comp_vec4 = OpTypePointer Function %type_struct_int_img_comp_vec4\n"

       << "%type_pushconstant_uniformblock = OpTypePointer PushConstant %type_uniformblock\n"
       << "%type_pushconstant_uniformblock_member_lod = OpTypePointer PushConstant %type_uint\n"
       << "%type_pushconstant_uniformblock_member_size = OpTypePointer PushConstant %type_vec2\n"

       << "%type_image_sparse = " << getOpTypeImageSparse(m_imageType, m_format, typeImgComp, true) << "\n"
       << "%type_sampled_image_sparse = OpTypeSampledImage %type_image_sparse\n"
       << "%type_uniformconst_image_sparse = OpTypePointer UniformConstant %type_sampled_image_sparse\n"

       << "%varying_texCoord = OpVariable %type_input_vec3 Input\n"

       << "%output_texel = OpVariable %type_output_img_comp_vec4 Output\n"
       << "%output_residency = OpVariable %type_output_uint Output\n"

       << "%uniformconst_image_sparse = OpVariable %type_uniformconst_image_sparse UniformConstant\n"

       << "%uniformblock_instance  = OpVariable %type_pushconstant_uniformblock PushConstant\n"

       // Declare constants
       << "%constant_uint_0 = OpConstant %type_uint 0\n"
       << "%constant_uint_1 = OpConstant %type_uint 1\n"
       << "%constant_uint_2 = OpConstant %type_uint 2\n"
       << "%constant_uint_3 = OpConstant %type_uint 3\n"
       << "%constant_int_0 = OpConstant %type_int  0\n"
       << "%constant_int_1 = OpConstant %type_int  1\n"
       << "%constant_int_2 = OpConstant %type_int  2\n"
       << "%constant_int_3 = OpConstant %type_int  3\n"
       << "%constant_float_0 = OpConstant %type_float 0.0\n"
       << "%constant_float_half = OpConstant %type_float 0.5\n"
       << "%constant_texel_resident = OpConstant %type_uint " << MEMORY_BLOCK_BOUND_VALUE << "\n"
       << "%constant_texel_not_resident = OpConstant %type_uint " << MEMORY_BLOCK_NOT_BOUND_VALUE
       << "\n"

       // Call main function
       << "%func_main         = OpFunction %type_void None %type_void_func\n"
       << "%label_func_main = OpLabel\n"

       << "%local_image_sparse = OpLoad %type_sampled_image_sparse %uniformconst_image_sparse\n"

       << "%texCoord = OpLoad %type_vec3 %varying_texCoord\n"

       << "%local_texCoord_x = OpCompositeExtract %type_float %texCoord 0\n"
       << "%local_texCoord_y = OpCompositeExtract %type_float %texCoord 1\n"
       << "%local_texCoord_z = OpCompositeExtract %type_float %texCoord 2\n"

       << "%local_texCoord_xy = OpCompositeConstruct %type_vec2 %local_texCoord_x %local_texCoord_y\n"
       << "%local_texCoord_xyz = OpCompositeConstruct %type_vec3 %local_texCoord_x %local_texCoord_y "
          "%local_texCoord_z\n"

       << "%access_uniformblock_member_uint_lod = OpAccessChain %type_pushconstant_uniformblock_member_lod "
          "%uniformblock_instance %constant_int_0\n"
       << "%local_uniformblock_member_uint_lod  = OpLoad %type_uint %access_uniformblock_member_uint_lod\n"
       << "%local_uniformblock_member_float_lod = OpConvertUToF %type_float %local_uniformblock_member_uint_lod\n"
       << "%access_uniformblock_member_size     = OpAccessChain %type_pushconstant_uniformblock_member_size "
          "%uniformblock_instance %constant_int_1\n"
       << "%local_uniformblock_member_size         = OpLoad %type_vec2 %access_uniformblock_member_size\n"

       << sparseImageOpString("%local_sparse_op_result", "%type_struct_int_img_comp_vec4", "%local_image_sparse",
                              coordString, "%local_uniformblock_member_float_lod")
       << "\n"

       // Load texel value
       << "%local_img_comp_vec4 = OpCompositeExtract " << typeImgCompVec4 << " %local_sparse_op_result 1\n";

    if (formatIsR64(m_format))
    {
        fs << "%local_img_comp32b = OpSConvert %type_ivec4 %local_img_comp_vec4\n"
           << "OpStore %output_texel %local_img_comp32b\n";
    }
    else
    {
        fs << "OpStore %output_texel %local_img_comp_vec4\n";
    }

    // Load residency code
    fs << "%local_residency_code = OpCompositeExtract %type_int %local_sparse_op_result 0\n"

       // Check if loaded texel is placed in resident memory
       << "%local_texel_resident = OpImageSparseTexelsResident %type_bool %local_residency_code\n"
       << "OpSelectionMerge %branch_texel_resident None\n"
       << "OpBranchConditional %local_texel_resident %label_texel_resident %label_texel_not_resident\n"
       << "%label_texel_resident = OpLabel\n"

       // Loaded texel is in resident memory
       << "OpStore %output_residency %constant_texel_resident\n"

       << "OpBranch %branch_texel_resident\n"
       << "%label_texel_not_resident = OpLabel\n"

       // Loaded texel is not in resident memory
       << "OpStore %output_residency %constant_texel_not_resident\n"

       << "OpBranch %branch_texel_resident\n"
       << "%branch_texel_resident = OpLabel\n"

       << "OpReturn\n"
       << "OpFunctionEnd\n";

    programCollection.spirvAsmSources.add("fragment_shader")
        << fs.str() << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, spirvVersion);
}

std::string SparseCaseOpImageSparseSampleExplicitLod::sparseImageOpString(const std::string &resultVariable,
                                                                          const std::string &resultType,
                                                                          const std::string &image,
                                                                          const std::string &coord,
                                                                          const std::string &miplevel) const
{
    std::ostringstream src;
    std::string additionalOperand = (m_operand.empty() ? " " : (std::string("|") + m_operand + " "));

    src << resultVariable << " = OpImageSparseSampleExplicitLod " << resultType << " " << image << " " << coord
        << " Lod" << additionalOperand << miplevel << "\n";

    return src.str();
}

std::string SparseCaseOpImageSparseSampleImplicitLod::sparseImageOpString(const std::string &resultVariable,
                                                                          const std::string &resultType,
                                                                          const std::string &image,
                                                                          const std::string &coord,
                                                                          const std::string &miplevel) const
{
    DE_UNREF(miplevel);

    std::ostringstream src;

    src << resultVariable << " = OpImageSparseSampleImplicitLod " << resultType << " " << image << " " << coord << " "
        << m_operand << "\n";

    return src.str();
}

std::string SparseCaseOpImageSparseGather::sparseImageOpString(const std::string &resultVariable,
                                                               const std::string &resultType, const std::string &image,
                                                               const std::string &coord,
                                                               const std::string &miplevel) const
{
    DE_UNREF(miplevel);

    std::ostringstream src;

    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(m_format);
    const std::string typeImgComp                   = getImageComponentTypeName(formatDescription);
    const std::string typeImgCompVec4               = getImageComponentVec4TypeName(formatDescription);

    // Bias the coord value by half a texel, so we sample from center of 2x2 gather rectangle

    src << "%local_image_width = OpCompositeExtract %type_float %local_uniformblock_member_size 0\n";
    src << "%local_image_height = OpCompositeExtract %type_float %local_uniformblock_member_size 1\n";
    src << "%local_coord_x_bias = OpFDiv %type_float %constant_float_half %local_image_width\n";
    src << "%local_coord_y_bias = OpFDiv %type_float %constant_float_half %local_image_height\n";

    switch (m_imageType)
    {
    case IMAGE_TYPE_2D:
    {
        src << "%local_coord_bias = OpCompositeConstruct %type_vec2 %local_coord_x_bias %local_coord_y_bias\n";
        src << "%local_coord_biased = OpFAdd %type_vec2 " << coord << " %local_coord_bias\n";

        break;
    }

    case IMAGE_TYPE_2D_ARRAY:
    case IMAGE_TYPE_3D:
    {
        src << "%local_coord_bias = OpCompositeConstruct %type_vec3 %local_coord_x_bias %local_coord_y_bias "
               "%constant_float_0\n";
        src << "%local_coord_biased = OpFAdd %type_vec3 " << coord << " %local_coord_bias\n";

        break;
    }

    default:
    {
        DE_FATAL("Unexpected image type");
    }
    }

    src << "%local_sparse_gather_result_x = OpImageSparseGather " << resultType << " " << image
        << " %local_coord_biased %constant_int_0 " + m_operand + "\n";
    src << "%local_sparse_gather_result_y = OpImageSparseGather " << resultType << " " << image
        << " %local_coord_biased %constant_int_1 " + m_operand + "\n";
    src << "%local_sparse_gather_result_z = OpImageSparseGather " << resultType << " " << image
        << " %local_coord_biased %constant_int_2 " + m_operand + "\n";
    src << "%local_sparse_gather_result_w = OpImageSparseGather " << resultType << " " << image
        << " %local_coord_biased %constant_int_3 " + m_operand + "\n";

    src << "%local_gather_residency_code = OpCompositeExtract %type_int %local_sparse_gather_result_x 0\n";

    src << "%local_gather_texels_x = OpCompositeExtract " << typeImgCompVec4 << " %local_sparse_gather_result_x 1\n";
    src << "%local_gather_texels_y = OpCompositeExtract " << typeImgCompVec4 << " %local_sparse_gather_result_y 1\n";
    src << "%local_gather_texels_z = OpCompositeExtract " << typeImgCompVec4 << " %local_sparse_gather_result_z 1\n";
    src << "%local_gather_texels_w = OpCompositeExtract " << typeImgCompVec4 << " %local_sparse_gather_result_w 1\n";

    src << "%local_gather_primary_texel_x = OpCompositeExtract " << typeImgComp << " %local_gather_texels_x 3\n";
    src << "%local_gather_primary_texel_y = OpCompositeExtract " << typeImgComp << " %local_gather_texels_y 3\n";
    src << "%local_gather_primary_texel_z = OpCompositeExtract " << typeImgComp << " %local_gather_texels_z 3\n";
    src << "%local_gather_primary_texel_w = OpCompositeExtract " << typeImgComp << " %local_gather_texels_w 3\n";

    src << "%local_gather_primary_texel = OpCompositeConstruct " << typeImgCompVec4
        << " %local_gather_primary_texel_x %local_gather_primary_texel_y %local_gather_primary_texel_z "
           "%local_gather_primary_texel_w\n";
    src << resultVariable << " = OpCompositeConstruct " << resultType
        << " %local_gather_residency_code %local_gather_primary_texel\n";

    return src.str();
}

class SparseShaderIntrinsicsInstanceSampledBase : public SparseShaderIntrinsicsInstanceBase
{
public:
    SparseShaderIntrinsicsInstanceSampledBase(Context &context, const SpirVFunction function, const ImageType imageType,
                                              const tcu::UVec3 &imageSize, const VkFormat format)
        : SparseShaderIntrinsicsInstanceBase(context, function, imageType, imageSize, format)
    {
    }

    VkImageUsageFlags imageSparseUsageFlags(void) const;
    VkImageUsageFlags imageOutputUsageFlags(void) const;

    VkQueueFlags getQueueFlags(void) const;

    void recordCommands(const VkCommandBuffer commandBuffer, const VkImageCreateInfo &imageSparseInfo,
                        const VkImage imageSparse, const VkImage imageTexels, const VkImage imageResidency);

    virtual void checkSupport(VkImageCreateInfo imageSparseInfo) const;

    virtual VkImageSubresourceRange sampledImageRangeToBind(const VkImageCreateInfo &imageSparseInfo,
                                                            const uint32_t mipLevel) const = 0;

private:
    typedef de::SharedPtr<vk::Unique<VkFramebuffer>> VkFramebufferSp;

    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;
    std::vector<VkFramebufferSp> m_framebuffers;
    Move<VkRenderPass> m_renderPass;
    Move<VkSampler> m_sampler;
};

VkImageUsageFlags SparseShaderIntrinsicsInstanceSampledBase::imageSparseUsageFlags(void) const
{
    return VK_IMAGE_USAGE_SAMPLED_BIT;
}

VkImageUsageFlags SparseShaderIntrinsicsInstanceSampledBase::imageOutputUsageFlags(void) const
{
    return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
}

VkQueueFlags SparseShaderIntrinsicsInstanceSampledBase::getQueueFlags(void) const
{
    return VK_QUEUE_GRAPHICS_BIT;
}

void SparseShaderIntrinsicsInstanceSampledBase::checkSupport(VkImageCreateInfo imageSparseInfo) const
{
    const InstanceInterface &instance                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice             = m_context.getPhysicalDevice();
    const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

    SparseShaderIntrinsicsInstanceBase::checkSupport(imageSparseInfo);

    if (imageSparseInfo.extent.width > deviceProperties.limits.maxFramebufferWidth ||
        imageSparseInfo.extent.height > deviceProperties.limits.maxFramebufferHeight ||
        imageSparseInfo.arrayLayers > deviceProperties.limits.maxFramebufferLayers)
    {
        TCU_THROW(NotSupportedError, "Image size exceeds allowed framebuffer dimensions");
    }

    // Check if device supports image format for sampled images
    if (!checkImageFormatFeatureSupport(instance, physicalDevice, imageSparseInfo.format,
                                        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Device does not support image format for sampled images");

    // Check if device supports image format for color attachment
    if (!checkImageFormatFeatureSupport(instance, physicalDevice, imageSparseInfo.format,
                                        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
        TCU_THROW(NotSupportedError, "Device does not support image format for color attachment");

    // Make sure device supports VK_FORMAT_R32_UINT format for color attachment
    if (!checkImageFormatFeatureSupport(instance, physicalDevice, mapTextureFormat(m_residencyFormat),
                                        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
        TCU_THROW(TestError, "Device does not support VK_FORMAT_R32_UINT format for color attachment");
}

void SparseShaderIntrinsicsInstanceSampledBase::recordCommands(const VkCommandBuffer commandBuffer,
                                                               const VkImageCreateInfo &imageSparseInfo,
                                                               const VkImage imageSparse, const VkImage imageTexels,
                                                               const VkImage imageResidency)
{
    const InstanceInterface &instance      = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice  = m_context.getPhysicalDevice();
    const DeviceInterface &deviceInterface = getDeviceInterface();

    // Create buffer storing vertex data
    std::vector<tcu::Vec2> vertexData;

    vertexData.push_back(tcu::Vec2(-1.0f, -1.0f));
    vertexData.push_back(tcu::Vec2(0.0f, 0.0f));

    vertexData.push_back(tcu::Vec2(-1.0f, 1.0f));
    vertexData.push_back(tcu::Vec2(0.0f, 1.0f));

    vertexData.push_back(tcu::Vec2(1.0f, -1.0f));
    vertexData.push_back(tcu::Vec2(1.0f, 0.0f));

    vertexData.push_back(tcu::Vec2(1.0f, 1.0f));
    vertexData.push_back(tcu::Vec2(1.0f, 1.0f));

    const VkDeviceSize vertexDataSizeInBytes = sizeInBytes(vertexData);
    const VkBufferCreateInfo vertexBufferCreateInfo =
        makeBufferCreateInfo(vertexDataSizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    m_vertexBuffer = createBuffer(deviceInterface, getDevice(), &vertexBufferCreateInfo);
    m_vertexBufferAlloc =
        bindBuffer(deviceInterface, getDevice(), getAllocator(), *m_vertexBuffer, MemoryRequirement::HostVisible);

    deMemcpy(m_vertexBufferAlloc->getHostPtr(), &vertexData[0], static_cast<std::size_t>(vertexDataSizeInBytes));
    flushAlloc(deviceInterface, getDevice(), *m_vertexBufferAlloc);

    // Create render pass
    const VkAttachmentDescription texelsAttachmentDescription = {
        (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags flags;
        imageSparseInfo.format,                   // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout finalLayout;
    };

    const VkAttachmentDescription residencyAttachmentDescription = {
        (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags flags;
        mapTextureFormat(m_residencyFormat),      // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout finalLayout;
    };

    const VkAttachmentDescription colorAttachmentsDescription[] = {texelsAttachmentDescription,
                                                                   residencyAttachmentDescription};

    const VkAttachmentReference texelsAttachmentReference = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    const VkAttachmentReference residencyAttachmentReference = {
        1u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    const VkAttachmentReference colorAttachmentsReference[] = {texelsAttachmentReference, residencyAttachmentReference};

    const VkAttachmentReference depthAttachmentReference = {
        VK_ATTACHMENT_UNUSED,     // uint32_t attachment;
        VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout layout;
    };

    const VkSubpassDescription subpassDescription = {
        (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                              // uint32_t inputAttachmentCount;
        DE_NULL,                         // const VkAttachmentReference* pInputAttachments;
        2u,                              // uint32_t colorAttachmentCount;
        colorAttachmentsReference,       // const VkAttachmentReference* pColorAttachments;
        DE_NULL,                         // const VkAttachmentReference* pResolveAttachments;
        &depthAttachmentReference,       // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                              // uint32_t preserveAttachmentCount;
        DE_NULL                          // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                   // const void* pNext;
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags flags;
        2u,                                        // uint32_t attachmentCount;
        colorAttachmentsDescription,               // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        DE_NULL                                    // const VkSubpassDependency* pDependencies;
    };

    m_renderPass = createRenderPass(deviceInterface, getDevice(), &renderPassInfo);

    // Create descriptor set layout
    DescriptorSetLayoutBuilder descriptorLayerBuilder;

    descriptorLayerBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(descriptorLayerBuilder.build(deviceInterface, getDevice()));

    // Create descriptor pool
    DescriptorPoolBuilder descriptorPoolBuilder;

    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSparseInfo.mipLevels);

    descriptorPool = descriptorPoolBuilder.build(
        deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, imageSparseInfo.mipLevels);

    VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        DE_NULL,
        (VkSamplerCreateFlags)0,
        mapFilterMode(tcu::Sampler::NEAREST),                // magFilter
        mapFilterMode(tcu::Sampler::NEAREST_MIPMAP_NEAREST), // minFilter
        mapMipmapMode(tcu::Sampler::NEAREST_MIPMAP_NEAREST), // mipMode
        mapWrapMode(tcu::Sampler::REPEAT_GL),                // addressU
        mapWrapMode(tcu::Sampler::REPEAT_GL),                // addressV
        mapWrapMode(tcu::Sampler::REPEAT_GL),                // addressW
        0.0f,                                                // mipLodBias
        VK_FALSE,                                            // anisotropyEnable
        1.0f,                                                // maxAnisotropy
        VK_FALSE,                                            // compareEnable
        mapCompareMode(tcu::Sampler::COMPAREMODE_ALWAYS),    // compareOp
        0.0f,                                                // minLod
        1000.0f,                                             // maxLod
        VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,               // borderColor
        VK_FALSE,                                            // unnormalizedCoords
    };
    m_sampler = createSampler(deviceInterface, getDevice(), &samplerCreateInfo);

    struct PushConstants
    {
        uint32_t lod;
        uint32_t padding; // padding needed to satisfy std430 rules
        float lodWidth;
        float lodHeight;
    };

    // Create pipeline layout
    const VkPushConstantRange lodConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        sizeof(PushConstants),        // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        1u,                                            // uint32_t setLayoutCount;
        &descriptorSetLayout.get(),                    // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                            // uint32_t pushConstantRangeCount;
        &lodConstantRange,                             // const VkPushConstantRange* pPushConstantRanges;
    };

    pipelineLayout = createPipelineLayout(deviceInterface, getDevice(), &pipelineLayoutParams);

    // Create graphics pipeline
    {
        Move<VkShaderModule> vertexModule =
            createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("vertex_shader"),
                               (VkShaderModuleCreateFlags)0);
        Move<VkShaderModule> fragmentModule =
            createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("fragment_shader"),
                               (VkShaderModuleCreateFlags)0);
        Move<VkShaderModule> geometryModule;

        if (imageSparseInfo.arrayLayers > 1u)
        {
            requireFeatures(instance, physicalDevice, FEATURE_GEOMETRY_SHADER);
            geometryModule =
                createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("geometry_shader"),
                                   (VkShaderModuleCreateFlags)0);
        }

        pipelines.push_back(
            makeVkSharedPtr(makeGraphicsPipeline(deviceInterface, getDevice(), *pipelineLayout, *m_renderPass,
                                                 *vertexModule, *fragmentModule, *geometryModule)));
    }

    const VkPipeline graphicsPipeline = **pipelines[0];

    {
        const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(
            VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

        VkImageMemoryBarrier imageShaderAccessBarriers[3];

        imageShaderAccessBarriers[0] = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, imageSparse, fullImageSubresourceRange);

        imageShaderAccessBarriers[1] =
            makeImageMemoryBarrier(0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, imageTexels, fullImageSubresourceRange);

        imageShaderAccessBarriers[2] =
            makeImageMemoryBarrier(0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, imageResidency, fullImageSubresourceRange);

        deviceInterface.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           0u, 0u, DE_NULL, 0u, DE_NULL, 3u, imageShaderAccessBarriers);
    }

    imageSparseViews.resize(imageSparseInfo.mipLevels);
    imageTexelsViews.resize(imageSparseInfo.mipLevels);
    imageResidencyViews.resize(imageSparseInfo.mipLevels);
    m_framebuffers.resize(imageSparseInfo.mipLevels);
    descriptorSets.resize(imageSparseInfo.mipLevels);

    std::vector<VkClearValue> clearValues;
    clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
    clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < imageSparseInfo.mipLevels; ++mipLevelNdx)
    {
        const VkExtent3D mipLevelSize = mipLevelExtents(imageSparseInfo.extent, mipLevelNdx);
        const VkRect2D renderArea     = makeRect2D(mipLevelSize);
        const VkViewport viewport     = makeViewport(mipLevelSize);
        const VkImageSubresourceRange mipLevelRange =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelNdx, 1u, 0u, imageSparseInfo.arrayLayers);

        // Create color attachments image views
        imageTexelsViews[mipLevelNdx] =
            makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), imageTexels, mapImageViewType(m_imageType),
                                          imageSparseInfo.format, mipLevelRange));
        imageResidencyViews[mipLevelNdx] =
            makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), imageResidency, mapImageViewType(m_imageType),
                                          mapTextureFormat(m_residencyFormat), mipLevelRange));

        const VkImageView attachmentsViews[] = {**imageTexelsViews[mipLevelNdx], **imageResidencyViews[mipLevelNdx]};

        // Create framebuffer
        const VkFramebufferCreateInfo framebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                   // const void* pNext;
            (VkFramebufferCreateFlags)0,               // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            2u,                                        // uint32_t attachmentCount;
            attachmentsViews,                          // const VkImageView* pAttachments;
            mipLevelSize.width,                        // uint32_t width;
            mipLevelSize.height,                       // uint32_t height;
            imageSparseInfo.arrayLayers,               // uint32_t layers;
        };

        m_framebuffers[mipLevelNdx] =
            makeVkSharedPtr(createFramebuffer(deviceInterface, getDevice(), &framebufferInfo));

        // Create descriptor set
        descriptorSets[mipLevelNdx] =
            makeVkSharedPtr(makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));
        const VkDescriptorSet descriptorSet = **descriptorSets[mipLevelNdx];

        // Update descriptor set
        const VkImageSubresourceRange sparseImageSubresourceRange =
            sampledImageRangeToBind(imageSparseInfo, mipLevelNdx);

        imageSparseViews[mipLevelNdx] =
            makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), imageSparse, mapImageViewType(m_imageType),
                                          imageSparseInfo.format, sparseImageSubresourceRange));

        const VkDescriptorImageInfo imageSparseDescInfo = makeDescriptorImageInfo(
            *m_sampler, **imageSparseViews[mipLevelNdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        DescriptorSetUpdateBuilder descriptorUpdateBuilder;

        descriptorUpdateBuilder.writeSingle(descriptorSet,
                                            DescriptorSetUpdateBuilder::Location::binding(BINDING_IMAGE_SPARSE),
                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageSparseDescInfo);
        descriptorUpdateBuilder.update(deviceInterface, getDevice());

        beginRenderPass(deviceInterface, commandBuffer, *m_renderPass, **m_framebuffers[mipLevelNdx], renderArea,
                        (uint32_t)clearValues.size(), &clearValues[0]);

        // Bind graphics pipeline
        deviceInterface.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // Bind descriptor set
        deviceInterface.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
                                              &descriptorSet, 0u, DE_NULL);

        // Bind vertex buffer
        {
            const VkDeviceSize offset = 0ull;
            deviceInterface.cmdBindVertexBuffers(commandBuffer, 0u, 1u, &m_vertexBuffer.get(), &offset);
        }

        // Bind Viewport
        deviceInterface.cmdSetViewport(commandBuffer, 0u, 1u, &viewport);

        // Bind Scissor Rectangle
        deviceInterface.cmdSetScissor(commandBuffer, 0u, 1u, &renderArea);

        const PushConstants pushConstants = {mipLevelNdx,
                                             0u, // padding
                                             static_cast<float>(mipLevelSize.width),
                                             static_cast<float>(mipLevelSize.height)};

        // Update push constants
        deviceInterface.cmdPushConstants(commandBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                         sizeof(PushConstants), &pushConstants);

        // Draw full screen quad
        deviceInterface.cmdDraw(commandBuffer, 4u, 1u, 0u, 0u);

        // End render pass
        endRenderPass(deviceInterface, commandBuffer);
    }

    {
        const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(
            VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

        VkImageMemoryBarrier imageOutputTransferSrcBarriers[2];

        imageOutputTransferSrcBarriers[0] = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageTexels, fullImageSubresourceRange);

        imageOutputTransferSrcBarriers[1] = makeImageMemoryBarrier(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageResidency, fullImageSubresourceRange);

        deviceInterface.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u,
                                           imageOutputTransferSrcBarriers);
    }
}

class SparseShaderIntrinsicsInstanceSampledExplicit : public SparseShaderIntrinsicsInstanceSampledBase
{
public:
    SparseShaderIntrinsicsInstanceSampledExplicit(Context &context, const SpirVFunction function,
                                                  const ImageType imageType, const tcu::UVec3 &imageSize,
                                                  const VkFormat format)
        : SparseShaderIntrinsicsInstanceSampledBase(context, function, imageType, imageSize, format)
    {
    }

    VkImageSubresourceRange sampledImageRangeToBind(const VkImageCreateInfo &imageSparseInfo,
                                                    const uint32_t mipLevel) const
    {
        DE_UNREF(mipLevel);
        return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u,
                                         imageSparseInfo.arrayLayers);
    }
};

TestInstance *SparseShaderIntrinsicsCaseSampledExplicit::createInstance(Context &context) const
{
    return new SparseShaderIntrinsicsInstanceSampledExplicit(context, m_function, m_imageType, m_imageSize, m_format);
}

class SparseShaderIntrinsicsInstanceSampledImplicit : public SparseShaderIntrinsicsInstanceSampledBase
{
public:
    SparseShaderIntrinsicsInstanceSampledImplicit(Context &context, const SpirVFunction function,
                                                  const ImageType imageType, const tcu::UVec3 &imageSize,
                                                  const VkFormat format)
        : SparseShaderIntrinsicsInstanceSampledBase(context, function, imageType, imageSize, format)
    {
    }

    VkImageSubresourceRange sampledImageRangeToBind(const VkImageCreateInfo &imageSparseInfo,
                                                    const uint32_t mipLevel) const
    {
        return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, 0u, imageSparseInfo.arrayLayers);
    }
};

TestInstance *SparseShaderIntrinsicsCaseSampledImplicit::createInstance(Context &context) const
{
    return new SparseShaderIntrinsicsInstanceSampledImplicit(context, m_function, m_imageType, m_imageSize, m_format);
}

} // namespace sparse
} // namespace vkt
