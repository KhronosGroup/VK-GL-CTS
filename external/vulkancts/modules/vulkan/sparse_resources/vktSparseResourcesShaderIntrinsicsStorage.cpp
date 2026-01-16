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
 * \file  vktSparseResourcesShaderIntrinsicsStorage.cpp
 * \brief Sparse Resources Shader Intrinsics for storage images
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesShaderIntrinsicsStorage.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

tcu::UVec3 computeWorkGroupSize(const tcu::UVec3 &gridSize)
{
    const uint32_t maxComputeWorkGroupInvocations = 128u;
    const tcu::UVec3 maxComputeWorkGroupSize      = tcu::UVec3(128u, 128u, 64u);

    const uint32_t xWorkGroupSize =
        std::min(std::min(gridSize.x(), maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
    const uint32_t yWorkGroupSize =
        std::min(std::min(gridSize.y(), maxComputeWorkGroupSize.y()), maxComputeWorkGroupInvocations / xWorkGroupSize);
    const uint32_t zWorkGroupSize = std::min(std::min(gridSize.z(), maxComputeWorkGroupSize.z()),
                                             maxComputeWorkGroupInvocations / (xWorkGroupSize * yWorkGroupSize));

    return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

void SparseShaderIntrinsicsCaseStorage::initPrograms(vk::SourceCollections &programCollection) const
{
    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(m_format);
    const uint32_t numPlanes = (formatDescription.numPlanes > 0) ? formatDescription.numPlanes : 1u;

    // Create compute program with multiple bindings for all planes and mip levels
    std::ostringstream src;

    // Use base format for shader generation
    const std::string imageTypeStr    = getShaderImageType(formatDescription, m_imageType);
    const std::string formatDataStr   = getShaderImageDataType(formatDescription);
    const std::string formatQualStr   = getShaderImageFormatQualifier(m_format);
    const std::string typeImgComp     = getImageComponentTypeName(formatDescription);
    const std::string typeImgCompVec4 = getImageComponentVec4TypeName(formatDescription);

    SpirvVersion spirvVersion = SPIRV_VERSION_1_0;
    std::string interfaceList = "";

    if (m_operand.find("Nontemporal") != std::string::npos)
    {
        spirvVersion = SPIRV_VERSION_1_6;
        // Interface list will be generated dynamically below
    }

    src << "OpCapability Shader\n"
        << "OpCapability ImageCubeArray\n"
        << "OpCapability SparseResidency\n"
        << "OpCapability StorageImageExtendedFormats\n";

    if (formatIsR64(m_format))
    {
        src << "OpCapability Int64\n"
            << "OpCapability Int64ImageEXT\n"
            << "OpExtension \"SPV_EXT_shader_image_int64\"\n";
    }

    // Build interface list for SPIRV 1.4+ (includes UniformConstant variables)
    if (spirvVersion >= SPIRV_VERSION_1_4)
    {
        interfaceList = "";
        for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
        {
            interfaceList += "%uniform_image_sparse_plane" + de::toString(planeNdx) + " ";
            interfaceList += "%uniform_image_texels_plane" + de::toString(planeNdx) + " ";
            interfaceList += "%uniform_image_residency_plane" + de::toString(planeNdx) + " ";
        }
    }

    src << "%ext_import = OpExtInstImport \"GLSL.std.450\"\n"
        << "OpMemoryModel Logical GLSL450\n"
        << "OpEntryPoint GLCompute %func_main \"main\" %input_GlobalInvocationID " << interfaceList << "\n"
        << "OpExecutionMode %func_main LocalSize 1 1 1\n"
        << "OpSource GLSL 440\n"

        << "OpName %func_main \"main\"\n"

        << "OpName %input_GlobalInvocationID \"gl_GlobalInvocationID\"\n"
        << "OpName %input_WorkGroupSize \"gl_WorkGroupSize\"\n";

    // Name all plane-specific images
    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        src << "OpName %uniform_image_sparse_plane" << planeNdx << " \"u_imageSparse_plane" << planeNdx << "\"\n"
            << "OpName %uniform_image_texels_plane" << planeNdx << " \"u_imageTexels_plane" << planeNdx << "\"\n"
            << "OpName %uniform_image_residency_plane" << planeNdx << " \"u_imageResidency_plane" << planeNdx << "\"\n";
    }

    src << "OpDecorate %input_GlobalInvocationID BuiltIn GlobalInvocationId\n"

        << "OpDecorate %input_WorkGroupSize BuiltIn WorkgroupSize\n"

        << "OpDecorate %constant_uint_grid_x SpecId 1\n"
        << "OpDecorate %constant_uint_grid_y SpecId 2\n"
        << "OpDecorate %constant_uint_grid_z SpecId 3\n"

        << "OpDecorate %constant_uint_work_group_size_x SpecId 4\n"
        << "OpDecorate %constant_uint_work_group_size_y SpecId 5\n"
        << "OpDecorate %constant_uint_work_group_size_z SpecId 6\n";

    // Decorate bindings for all planes
    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        const uint32_t baseBinding = planeNdx * 3; // 3 images per plane
        src << "OpDecorate %uniform_image_sparse_plane" << planeNdx << " DescriptorSet 0\n"
            << "OpDecorate %uniform_image_sparse_plane" << planeNdx << " Binding " << (baseBinding + 0) << "\n"

            << "OpDecorate %uniform_image_texels_plane" << planeNdx << " DescriptorSet 0\n"
            << "OpDecorate %uniform_image_texels_plane" << planeNdx << " Binding " << (baseBinding + 1) << "\n"
            << "OpDecorate %uniform_image_texels_plane" << planeNdx << " NonReadable\n"

            << "OpDecorate %uniform_image_residency_plane" << planeNdx << " DescriptorSet 0\n"
            << "OpDecorate %uniform_image_residency_plane" << planeNdx << " Binding " << (baseBinding + 2) << "\n"
            << "OpDecorate %uniform_image_residency_plane" << planeNdx << " NonReadable\n";
    }

    // Declare data types
    src << "%type_bool = OpTypeBool\n";

    if (formatIsR64(m_format))
    {
        src << "%type_int64 = OpTypeInt 64 1\n"
            << "%type_uint64 = OpTypeInt 64 0\n"
            << "%type_i64vec2 = OpTypeVector %type_int64  2\n"
            << "%type_i64vec3 = OpTypeVector %type_int64  3\n"
            << "%type_i64vec4 = OpTypeVector %type_int64  4\n"
            << "%type_u64vec3 = OpTypeVector %type_uint64 3\n"
            << "%type_u64vec4 = OpTypeVector %type_uint64 4\n";
    }

    src << "%type_int = OpTypeInt 32 1\n"
        << "%type_uint = OpTypeInt 32 0\n"
        << "%type_float = OpTypeFloat 32\n"
        << "%type_ivec2 = OpTypeVector %type_int  2\n"
        << "%type_ivec3 = OpTypeVector %type_int  3\n"
        << "%type_ivec4 = OpTypeVector %type_int  4\n"
        << "%type_uvec3 = OpTypeVector %type_uint 3\n"
        << "%type_uvec4 = OpTypeVector %type_uint 4\n"
        << "%type_vec2 = OpTypeVector %type_float 2\n"
        << "%type_vec3 = OpTypeVector %type_float 3\n"
        << "%type_vec4 = OpTypeVector %type_float 4\n"

        << "%type_input_uint = OpTypePointer Input %type_uint\n"
        << "%type_input_uvec3 = OpTypePointer Input %type_uvec3\n"

        << "%type_function_int             = OpTypePointer Function %type_int\n"
        << "%type_function_img_comp_vec4 = OpTypePointer Function " << typeImgCompVec4 << "\n"

        << "%type_void = OpTypeVoid\n"
        << "%type_void_func = OpTypeFunction %type_void\n";

    // Declare image types and variables for each plane
    // Track which OpTypeImage definitions we've already declared to avoid duplicates
    std::set<std::string> declaredImageTypes;
    std::vector<std::string> planeImageSparseTypeNames(numPlanes);
    std::vector<std::string> planeImageSparseWithSamplerTypeNames(numPlanes);
    std::vector<std::string> planeImageResidencyTypeNames(numPlanes);

    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        VkFormat planeFormat =
            (formatDescription.numPlanes > 1) ? getPlaneCompatibleFormat(formatDescription, planeNdx) : m_format;
        PlanarFormatDescription planeFormatDesc = getPlanarFormatDescription(planeFormat);
        std::string planeTypeImgComp            = getImageComponentTypeName(planeFormatDesc);
        std::string planeTypeImgCompVec4        = getImageComponentVec4TypeName(planeFormatDesc);

        const std::string typeImageSparse             = getSparseImageTypeName();
        const std::string typeUniformConstImageSparse = getUniformConstSparseImageTypeName();
        const std::string opTypeImageSparse = getOpTypeImageSparse(m_imageType, planeFormat, planeTypeImgComp, false);
        const std::string opTypeImageSparseWithSampler =
            getOpTypeImageSparse(m_imageType, planeFormat, planeTypeImgComp, true);
        const std::string opTypeImageResidency = getOpTypeImageResidency(m_imageType);

        // Generate type names based on format to naturally deduplicate identical types
        const std::string formatId = de::toString(static_cast<uint32_t>(planeFormat));

        src << "%type_struct_int_img_comp_vec4_plane" << planeNdx << " = OpTypeStruct %type_int "
            << planeTypeImgCompVec4 << "\n";

        // Declare storage image type (once per unique type)
        planeImageSparseTypeNames[planeNdx] = "%type_image_sparse_fmt" + formatId;
        if (declaredImageTypes.find(opTypeImageSparse) == declaredImageTypes.end())
        {
            src << planeImageSparseTypeNames[planeNdx] << " = " << opTypeImageSparse << "\n";
            declaredImageTypes.insert(opTypeImageSparse);
        }
        src << "%type_uniformconst_image_sparse_plane" << planeNdx << " = OpTypePointer UniformConstant "
            << planeImageSparseTypeNames[planeNdx] << "\n";

        // Sparse image with sampler type declaration (once per unique type)
        planeImageSparseWithSamplerTypeNames[planeNdx] = "%type_image_sparse_with_sampler_fmt" + formatId;
        if (declaredImageTypes.find(opTypeImageSparseWithSampler) == declaredImageTypes.end())
        {
            src << planeImageSparseWithSamplerTypeNames[planeNdx] << " = " << opTypeImageSparseWithSampler << "\n";
            declaredImageTypes.insert(opTypeImageSparseWithSampler);
        }
        src << "%type_uniformconst_image_sparse_with_sampler_plane" << planeNdx << " = OpTypePointer UniformConstant "
            << planeImageSparseWithSamplerTypeNames[planeNdx] << "\n";

        // Residency image type declaration: it's not possible to declare two OpTypeImage aliases for the same data type
        planeImageResidencyTypeNames[planeNdx] =
            (opTypeImageSparse == opTypeImageResidency) ? planeImageSparseTypeNames[planeNdx] : "%type_image_residency";
        if (opTypeImageSparse != opTypeImageResidency &&
            declaredImageTypes.find(opTypeImageResidency) == declaredImageTypes.end())
        {
            src << planeImageResidencyTypeNames[planeNdx] << " = " << opTypeImageResidency << "\n";
            declaredImageTypes.insert(opTypeImageResidency);
        }

        // Declare per-plane residency pointer type
        src << "%type_uniformconst_image_residency_plane" << planeNdx << " = OpTypePointer UniformConstant "
            << planeImageResidencyTypeNames[planeNdx]
            << "\n"

            // Declare plane-specific sparse image variables
            << "%uniform_image_sparse_plane" << planeNdx << " = OpVariable "
            << (typeUniformConstImageSparse == "%type_uniformconst_image_sparse_with_sampler" ?
                    ("%type_uniformconst_image_sparse_with_sampler_plane" + de::toString(planeNdx)) :
                    ("%type_uniformconst_image_sparse_plane" + de::toString(planeNdx)))
            << " UniformConstant\n"

            // Declare plane-specific output image variables for storing texels
            << "%uniform_image_texels_plane" << planeNdx << " = OpVariable %type_uniformconst_image_sparse_plane"
            << planeNdx
            << " UniformConstant\n"

            // Declare plane-specific output image variables for storing residency information
            << "%uniform_image_residency_plane" << planeNdx << " = OpVariable %type_uniformconst_image_residency_plane"
            << planeNdx << " UniformConstant\n";
    }

    // Declare input variables
    src << "%input_GlobalInvocationID = OpVariable %type_input_uvec3 Input\n"

        << "%constant_uint_grid_x = OpSpecConstant %type_uint 1\n"
        << "%constant_uint_grid_y = OpSpecConstant %type_uint 1\n"
        << "%constant_uint_grid_z = OpSpecConstant %type_uint 1\n"

        << "%constant_uint_work_group_size_x = OpSpecConstant %type_uint 1\n"
        << "%constant_uint_work_group_size_y = OpSpecConstant %type_uint 1\n"
        << "%constant_uint_work_group_size_z = OpSpecConstant %type_uint 1\n"
        << "%input_WorkGroupSize = OpSpecConstantComposite %type_uvec3 %constant_uint_work_group_size_x "
           "%constant_uint_work_group_size_y %constant_uint_work_group_size_z\n"

        // Declare constants
        << "%constant_uint_0 = OpConstant %type_uint 0\n"
        << "%constant_uint_1 = OpConstant %type_uint 1\n"
        << "%constant_uint_2 = OpConstant %type_uint 2\n"
        << "%constant_int_0 = OpConstant %type_int 0\n"
        << "%constant_int_1 = OpConstant %type_int 1\n"
        << "%constant_int_2 = OpConstant %type_int 2\n"
        << "%constant_bool_true = OpConstantTrue %type_bool\n"

        << "%constant_uint_resident = OpConstant %type_uint " << MEMORY_BLOCK_BOUND_VALUE << "\n"
        << "%constant_uvec4_resident = OpConstantComposite %type_uvec4 %constant_uint_resident %constant_uint_resident "
           "%constant_uint_resident %constant_uint_resident\n"
        << "%constant_uint_not_resident = OpConstant %type_uint " << MEMORY_BLOCK_NOT_BOUND_VALUE << "\n"
        << "%constant_uvec4_not_resident = OpConstantComposite %type_uvec4 %constant_uint_not_resident "
           "%constant_uint_not_resident %constant_uint_not_resident %constant_uint_not_resident\n"

        // Call main function
        << "%func_main         = OpFunction %type_void None %type_void_func\n"
        << "%label_func_main = OpLabel\n"

        // Load GlobalInvocationID.xyz into local variables
        << "%access_GlobalInvocationID_x = OpAccessChain %type_input_uint %input_GlobalInvocationID %constant_uint_0\n"
        << "%local_uint_GlobalInvocationID_x = OpLoad %type_uint %access_GlobalInvocationID_x\n"
        << "%local_int_GlobalInvocationID_x = OpBitcast %type_int %local_uint_GlobalInvocationID_x\n"

        << "%access_GlobalInvocationID_y = OpAccessChain %type_input_uint %input_GlobalInvocationID %constant_uint_1\n"
        << "%local_uint_GlobalInvocationID_y = OpLoad %type_uint %access_GlobalInvocationID_y\n"
        << "%local_int_GlobalInvocationID_y = OpBitcast %type_int %local_uint_GlobalInvocationID_y\n"

        << "%access_GlobalInvocationID_z = OpAccessChain %type_input_uint %input_GlobalInvocationID %constant_uint_2\n"
        << "%local_uint_GlobalInvocationID_z = OpLoad %type_uint %access_GlobalInvocationID_z\n"
        << "%local_int_GlobalInvocationID_z = OpBitcast %type_int %local_uint_GlobalInvocationID_z\n"

        << "%local_ivec2_GlobalInvocationID_xy = OpCompositeConstruct %type_ivec2 %local_int_GlobalInvocationID_x "
           "%local_int_GlobalInvocationID_y\n"
        << "%local_ivec3_GlobalInvocationID_xyz = OpCompositeConstruct %type_ivec3 %local_int_GlobalInvocationID_x "
           "%local_int_GlobalInvocationID_y %local_int_GlobalInvocationID_z\n"

        << "%comparison_range_x = OpULessThan %type_bool %local_uint_GlobalInvocationID_x %constant_uint_grid_x\n"
        << "OpSelectionMerge %label_out_range_x None\n"
        << "OpBranchConditional %comparison_range_x %label_in_range_x %label_out_range_x\n"
        << "%label_in_range_x = OpLabel\n"

        << "%comparison_range_y = OpULessThan %type_bool %local_uint_GlobalInvocationID_y %constant_uint_grid_y\n"
        << "OpSelectionMerge %label_out_range_y None\n"
        << "OpBranchConditional %comparison_range_y %label_in_range_y %label_out_range_y\n"
        << "%label_in_range_y = OpLabel\n"

        << "%comparison_range_z = OpULessThan %type_bool %local_uint_GlobalInvocationID_z %constant_uint_grid_z\n"
        << "OpSelectionMerge %label_out_range_z None\n"
        << "OpBranchConditional %comparison_range_z %label_in_range_z %label_out_range_z\n"
        << "%label_in_range_z = OpLabel\n";

    // Process each plane
    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        VkFormat planeFormat =
            (formatDescription.numPlanes > 1) ? getPlaneCompatibleFormat(formatDescription, planeNdx) : m_format;
        PlanarFormatDescription planeFormatDesc = getPlanarFormatDescription(planeFormat);
        std::string planeTypeImgComp            = getImageComponentTypeName(planeFormatDesc);
        std::string planeTypeImgCompVec4        = getImageComponentVec4TypeName(planeFormatDesc);

        const std::string coordString =
            getShaderImageCoordinates(m_imageType, "%local_int_GlobalInvocationID_x",
                                      "%local_ivec2_GlobalInvocationID_xy", "%local_ivec3_GlobalInvocationID_xyz");

        const std::string typeImageSparse = getSparseImageTypeName();

        // Process plane " << planeNdx << "\n"
        src << "%local_image_sparse_plane" << planeNdx << " = OpLoad "
            << (typeImageSparse == "%type_image_sparse_with_sampler" ? planeImageSparseWithSamplerTypeNames[planeNdx] :
                                                                       planeImageSparseTypeNames[planeNdx])
            << " %uniform_image_sparse_plane" << planeNdx << "\n"
            << sparseImageOpString("%local_sparse_op_result_plane" + de::toString(planeNdx),
                                   "%type_struct_int_img_comp_vec4_plane" + de::toString(planeNdx),
                                   "%local_image_sparse_plane" + de::toString(planeNdx), coordString, "%constant_int_0")
            << "\n"

            // Load the texel from the sparse image to local variable for OpImageSparse*
            << "%local_img_comp_vec4_plane" << planeNdx << " = OpCompositeExtract " << planeTypeImgCompVec4
            << " %local_sparse_op_result_plane" << planeNdx
            << " 1\n"

            // Load residency code for OpImageSparse*
            << "%local_residency_code_plane" << planeNdx
            << " = OpCompositeExtract %type_int %local_sparse_op_result_plane" << planeNdx
            << " 0\n"
            // End Call OpImageSparse*

            // Load texels image
            << "%local_image_texels_plane" << planeNdx << " = OpLoad " << planeImageSparseTypeNames[planeNdx]
            << " %uniform_image_texels_plane" << planeNdx
            << "\n"

            // Write the texel to output image via OpImageWrite
            << "OpImageWrite %local_image_texels_plane" << planeNdx << " " << coordString
            << " %local_img_comp_vec4_plane" << planeNdx
            << "\n"

            // Load residency info image
            << "%local_image_residency_plane" << planeNdx << " = OpLoad " << planeImageResidencyTypeNames[planeNdx]
            << " %uniform_image_residency_plane" << planeNdx
            << "\n"

            // Check if loaded texel is placed in resident memory
            << "%local_texel_resident_plane" << planeNdx
            << " = OpImageSparseTexelsResident %type_bool %local_residency_code_plane" << planeNdx << "\n"
            << "OpSelectionMerge %branch_texel_resident_plane" << planeNdx << " None\n"
            << "OpBranchConditional %local_texel_resident_plane" << planeNdx << " %label_texel_resident_plane"
            << planeNdx << " %label_texel_not_resident_plane" << planeNdx << "\n"
            << "%label_texel_resident_plane" << planeNdx
            << " = OpLabel\n"

            // Loaded texel is in resident memory
            << "OpImageWrite %local_image_residency_plane" << planeNdx << " " << coordString
            << " %constant_uvec4_resident\n"

            << "OpBranch %branch_texel_resident_plane" << planeNdx << "\n"
            << "%label_texel_not_resident_plane" << planeNdx
            << " = OpLabel\n"

            // Loaded texel is not in resident memory
            << "OpImageWrite %local_image_residency_plane" << planeNdx << " " << coordString
            << " %constant_uvec4_not_resident\n"
            << "OpBranch %branch_texel_resident_plane" << planeNdx << "\n"
            << "%branch_texel_resident_plane" << planeNdx << " = OpLabel\n";
    }

    src << "OpBranch %label_out_range_z\n"
        << "%label_out_range_z = OpLabel\n"

        << "OpBranch %label_out_range_y\n"
        << "%label_out_range_y = OpLabel\n"

        << "OpBranch %label_out_range_x\n"
        << "%label_out_range_x = OpLabel\n"

        << "OpReturn\n"
        << "OpFunctionEnd\n";

    programCollection.spirvAsmSources.add("compute")
        << src.str() << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, spirvVersion);
}

std::string SparseCaseOpImageSparseFetch::getSparseImageTypeName(void) const
{
    return "%type_image_sparse_with_sampler";
}

std::string SparseCaseOpImageSparseFetch::getUniformConstSparseImageTypeName(void) const
{
    return "%type_uniformconst_image_sparse_with_sampler";
}

std::string SparseCaseOpImageSparseFetch::sparseImageOpString(const std::string &resultVariable,
                                                              const std::string &resultType, const std::string &image,
                                                              const std::string &coord,
                                                              const std::string &mipLevel) const
{
    std::ostringstream src;
    std::string additionalOperand = (m_operand.empty() ? " " : (std::string("|") + m_operand + " "));

    src << resultVariable << " = OpImageSparseFetch " << resultType << " " << image << " " << coord << " Lod"
        << additionalOperand << mipLevel << "\n";

    return src.str();
}

std::string SparseCaseOpImageSparseRead::getSparseImageTypeName(void) const
{
    return "%type_image_sparse";
}

std::string SparseCaseOpImageSparseRead::getUniformConstSparseImageTypeName(void) const
{
    return "%type_uniformconst_image_sparse";
}

std::string SparseCaseOpImageSparseRead::sparseImageOpString(const std::string &resultVariable,
                                                             const std::string &resultType, const std::string &image,
                                                             const std::string &coord,
                                                             const std::string &mipLevel) const
{
    DE_UNREF(mipLevel);

    std::ostringstream src;

    src << resultVariable << " = OpImageSparseRead " << resultType << " " << image << " " << coord << " " << m_operand
        << "\n";

    return src.str();
}

class SparseShaderIntrinsicsInstanceStorage : public SparseShaderIntrinsicsInstanceBase
{
public:
    SparseShaderIntrinsicsInstanceStorage(Context &context, const SpirVFunction function, const ImageType imageType,
                                          const tcu::UVec3 &imageSize, const VkFormat format)
        : SparseShaderIntrinsicsInstanceBase(context, function, imageType, imageSize, format)
    {
    }

    VkImageUsageFlags imageOutputUsageFlags(void) const;

    VkQueueFlags getQueueFlags(void) const;

    void recordCommands(const VkCommandBuffer commandBuffer, const VkImageCreateInfo &imageSparseInfo,
                        const VkImage imageSparse, const VkImage imageTexels, const VkImage imageResidency);
    virtual void checkSupport(VkImageCreateInfo imageSparseInfo) const;

    virtual VkDescriptorType imageSparseDescType(void) const = 0;
};

void SparseShaderIntrinsicsInstanceStorage::checkSupport(VkImageCreateInfo imageSparseInfo) const
{
    const InstanceInterface &instance     = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    SparseShaderIntrinsicsInstanceBase::checkSupport(imageSparseInfo);

    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(imageSparseInfo.format);
    if (formatDescription.numPlanes > 1)
    {
        // For multi-planar formats, check storage-compatible formats support storage
        // We use block-compatible formats (e.g. R16 for R10X6) for actual storage operations
        for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
        {
            const VkFormat planeFormat   = getPlaneCompatibleFormat(formatDescription, planeNdx);
            const VkFormat storageFormat = getStorageCompatibleFormat(planeFormat);
            if (!checkImageFormatFeatureSupport(instance, physicalDevice, storageFormat,
                                                VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
                TCU_THROW(NotSupportedError, "Device does not support storage-compatible format for plane");
        }
    }
    else
    {
        // Check if device supports image format for storage image
        if (!checkImageFormatFeatureSupport(instance, physicalDevice, imageSparseInfo.format,
                                            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
            TCU_THROW(NotSupportedError, "Device does not support image format for storage image");
    }

    // Make sure device supports VK_FORMAT_R32_UINT format for storage image
    if (!checkImageFormatFeatureSupport(instance, physicalDevice, mapTextureFormat(m_residencyFormat),
                                        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(TestError, "Device does not support VK_FORMAT_R32_UINT format for storage image");
}

VkImageUsageFlags SparseShaderIntrinsicsInstanceStorage::imageOutputUsageFlags(void) const
{
    return VK_IMAGE_USAGE_STORAGE_BIT;
}

VkQueueFlags SparseShaderIntrinsicsInstanceStorage::getQueueFlags(void) const
{
    return VK_QUEUE_COMPUTE_BIT;
}

void SparseShaderIntrinsicsInstanceStorage::recordCommands(const VkCommandBuffer commandBuffer,
                                                           const VkImageCreateInfo &imageSparseInfo,
                                                           const VkImage imageSparse, const VkImage imageTexels,
                                                           const VkImage imageResidency)
{
    const DeviceInterface &deviceInterface          = getDeviceInterface();
    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(imageSparseInfo.format);
    const PlanarFormatDescription residencyFormatDescription =
        getPlanarFormatDescription(mapTextureFormat(m_residencyFormat));
    const bool useArrayLayersForPlanes = (formatDescription.numPlanes > 1 && residencyFormatDescription.numPlanes == 1);

    const uint32_t numPlanes          = (formatDescription.numPlanes > 0) ? formatDescription.numPlanes : 1u;
    const uint32_t numPlanesTimesMips = numPlanes * imageSparseInfo.mipLevels;
    imageSparseViews.resize(numPlanesTimesMips);
    imageTexelsViews.resize(numPlanesTimesMips);
    imageResidencyViews.resize(numPlanesTimesMips);

    // Create descriptor set layout with bindings for all planes
    // Each plane gets 3 bindings: sparse, texels, residency
    DescriptorSetLayoutBuilder descriptorLayerBuilder;

    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        descriptorLayerBuilder.addSingleBinding(imageSparseDescType(), VK_SHADER_STAGE_COMPUTE_BIT);
        descriptorLayerBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        descriptorLayerBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(descriptorLayerBuilder.build(deviceInterface, getDevice()));

    // Create pipeline layout
    pipelineLayout = makePipelineLayout(deviceInterface, getDevice(), *descriptorSetLayout);

    // Create descriptor pool (one set per mip level, each with all plane bindings)
    DescriptorPoolBuilder descriptorPoolBuilder;

    descriptorPoolBuilder.addType(imageSparseDescType(), numPlanesTimesMips);
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numPlanesTimesMips);
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numPlanesTimesMips);

    descriptorPool = descriptorPoolBuilder.build(
        deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, imageSparseInfo.mipLevels);

    std::vector<VkImageMemoryBarrier> imageShaderAccessBarriers;

    for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
    {
        const VkImageAspectFlags aspect =
            (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
        const VkImageSubresourceRange planeSubresourceRange =
            makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

        imageShaderAccessBarriers.push_back(makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, imageSparse, planeSubresourceRange));

        imageShaderAccessBarriers.push_back(makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
                                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                                                   imageTexels, planeSubresourceRange));
    }

    // Residency image uses COLOR aspect for single-plane formats
    // For multi-planar sparse with single-plane residency, use extended array layer count
    const uint32_t residencyArrayLayers = useArrayLayersForPlanes ?
                                              (imageSparseInfo.arrayLayers * formatDescription.numPlanes) :
                                              imageSparseInfo.arrayLayers;
    const VkImageSubresourceRange residencySubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, residencyArrayLayers);
    imageShaderAccessBarriers.push_back(makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                                               imageResidency, residencySubresourceRange));

    deviceInterface.cmdPipelineBarrier(
        commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u,
        nullptr, static_cast<uint32_t>(imageShaderAccessBarriers.size()), imageShaderAccessBarriers.data());

    Unique<VkShaderModule> shaderModule(
        createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("compute"), 0u));

    const VkSpecializationMapEntry specializationMapEntries[6] = {
        {1u, 0u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // GridSize.x
        {2u, 1u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // GridSize.y
        {3u, 2u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // GridSize.z
        {4u, 3u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // WorkGroupSize.x
        {5u, 4u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // WorkGroupSize.y
        {6u, 5u * (uint32_t)sizeof(uint32_t), sizeof(uint32_t)}, // WorkGroupSize.z
    };

    pipelines.resize(imageSparseInfo.mipLevels);
    descriptorSets.resize(imageSparseInfo.mipLevels);

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < imageSparseInfo.mipLevels; ++mipLevelNdx)
    {
        const tcu::UVec3 gridSize              = getShaderGridSize(m_imageType, m_imageSize, mipLevelNdx);
        const tcu::UVec3 workGroupSize         = computeWorkGroupSize(gridSize);
        const tcu::UVec3 specializationData[2] = {gridSize, workGroupSize};

        const VkSpecializationInfo specializationInfo = {
            (uint32_t)DE_LENGTH_OF_ARRAY(specializationMapEntries), // mapEntryCount
            specializationMapEntries,                               // pMapEntries
            sizeof(specializationData),                             // dataSize
            specializationData,                                     // pData
        };

        // Create and bind compute pipeline
        pipelines[mipLevelNdx] = makeVkSharedPtr(
            makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, (VkPipelineCreateFlags)0u, nullptr,
                                *shaderModule, (VkPipelineShaderStageCreateFlags)0u, &specializationInfo));

        deviceInterface.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **pipelines[mipLevelNdx]);

        // Create descriptor set
        descriptorSets[mipLevelNdx] =
            makeVkSharedPtr(makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));
        std::vector<VkDescriptorImageInfo> mipDescriptorImageInfos;

        for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
        {
            const uint32_t viewIndex = mipLevelNdx * numPlanes + planeNdx;
            const VkImageAspectFlags aspect =
                (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkFormat planeCompatibleFormat = (formatDescription.numPlanes > 1) ?
                                                       getPlaneCompatibleFormat(formatDescription, planeNdx) :
                                                       imageSparseInfo.format;
            // For storage images, use block-compatible format (e.g. R16 for R10X6)
            const VkFormat storageViewFormat = getStorageCompatibleFormat(planeCompatibleFormat);

            // Create views for THIS specific mip level only
            const VkImageSubresourceRange mipLevelRange =
                makeImageSubresourceRange(aspect, mipLevelNdx, 1u, 0u, imageSparseInfo.arrayLayers);

            imageSparseViews[viewIndex] =
                makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), imageSparse, mapImageViewType(m_imageType),
                                              storageViewFormat, mipLevelRange));

            imageTexelsViews[viewIndex] =
                makeVkSharedPtr(makeImageView(deviceInterface, getDevice(), imageTexels, mapImageViewType(m_imageType),
                                              storageViewFormat, mipLevelRange));

            // Residency image uses COLOR aspect, with array layers for planes
            const uint32_t residencyBaseLayer = useArrayLayersForPlanes ? (planeNdx * imageSparseInfo.arrayLayers) : 0u;
            const VkImageSubresourceRange residencyMipLevelRange = makeImageSubresourceRange(
                VK_IMAGE_ASPECT_COLOR_BIT, mipLevelNdx, 1u, residencyBaseLayer, imageSparseInfo.arrayLayers);

            imageResidencyViews[viewIndex] = makeVkSharedPtr(
                makeImageView(deviceInterface, getDevice(), imageResidency, mapImageViewType(m_imageType),
                              mapTextureFormat(m_residencyFormat), residencyMipLevelRange));

            // Store descriptor infos for binding
            mipDescriptorImageInfos.push_back(
                makeDescriptorImageInfo(VK_NULL_HANDLE, **imageSparseViews[viewIndex], VK_IMAGE_LAYOUT_GENERAL));
            mipDescriptorImageInfos.push_back(
                makeDescriptorImageInfo(VK_NULL_HANDLE, **imageTexelsViews[viewIndex], VK_IMAGE_LAYOUT_GENERAL));
            mipDescriptorImageInfos.push_back(
                makeDescriptorImageInfo(VK_NULL_HANDLE, **imageResidencyViews[viewIndex], VK_IMAGE_LAYOUT_GENERAL));
        }

        // Bind all plane resources for this mip level to the descriptor set
        DescriptorSetUpdateBuilder descriptorUpdateBuilder;
        for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
        {
            const uint32_t baseBinding = planeNdx * 3;
            descriptorUpdateBuilder.writeSingle(**descriptorSets[mipLevelNdx],
                                                DescriptorSetUpdateBuilder::Location::binding(baseBinding + 0),
                                                imageSparseDescType(), &mipDescriptorImageInfos[planeNdx * 3 + 0]);
            descriptorUpdateBuilder.writeSingle(
                **descriptorSets[mipLevelNdx], DescriptorSetUpdateBuilder::Location::binding(baseBinding + 1),
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &mipDescriptorImageInfos[planeNdx * 3 + 1]);
            descriptorUpdateBuilder.writeSingle(
                **descriptorSets[mipLevelNdx], DescriptorSetUpdateBuilder::Location::binding(baseBinding + 2),
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &mipDescriptorImageInfos[planeNdx * 3 + 2]);
        }
        descriptorUpdateBuilder.update(deviceInterface, getDevice());

        deviceInterface.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                              &descriptorSets[mipLevelNdx]->get(), 0u, nullptr);

        const uint32_t xWorkGroupCount =
            gridSize.x() / workGroupSize.x() + (gridSize.x() % workGroupSize.x() ? 1u : 0u);
        const uint32_t yWorkGroupCount =
            gridSize.y() / workGroupSize.y() + (gridSize.y() % workGroupSize.y() ? 1u : 0u);
        const uint32_t zWorkGroupCount =
            gridSize.z() / workGroupSize.z() + (gridSize.z() % workGroupSize.z() ? 1u : 0u);
        const tcu::UVec3 maxWorkGroupCount = tcu::UVec3(65535u, 65535u, 65535u);

        if (maxWorkGroupCount.x() < xWorkGroupCount || maxWorkGroupCount.y() < yWorkGroupCount ||
            maxWorkGroupCount.z() < zWorkGroupCount)
        {
            TCU_THROW(NotSupportedError, "Image size exceeds compute invocations limit");
        }

        deviceInterface.cmdDispatch(commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);
    }

    // Create final barriers for all planes
    std::vector<VkImageMemoryBarrier> imageOutputTransferSrcBarriers;

    for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
    {
        const VkImageAspectFlags aspect =
            (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
        const VkImageSubresourceRange planeSubresourceRange =
            makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers);

        imageOutputTransferSrcBarriers.push_back(
            makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageTexels, planeSubresourceRange));
    }

    // Residency image uses COLOR aspect for single-plane formats
    // For multi-planar sparse with single-plane residency, use extended array layer count
    const VkImageSubresourceRange residencyFinalSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageSparseInfo.mipLevels, 0u, residencyArrayLayers);
    imageOutputTransferSrcBarriers.push_back(
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageResidency, residencyFinalSubresourceRange));

    deviceInterface.cmdPipelineBarrier(
        commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
        nullptr, static_cast<uint32_t>(imageOutputTransferSrcBarriers.size()), imageOutputTransferSrcBarriers.data());
}

class SparseShaderIntrinsicsInstanceFetch : public SparseShaderIntrinsicsInstanceStorage
{
public:
    SparseShaderIntrinsicsInstanceFetch(Context &context, const SpirVFunction function, const ImageType imageType,
                                        const tcu::UVec3 &imageSize, const VkFormat format)
        : SparseShaderIntrinsicsInstanceStorage(context, function, imageType, imageSize, format)
    {
    }

    VkImageUsageFlags imageSparseUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    VkDescriptorType imageSparseDescType(void) const
    {
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
};

TestInstance *SparseCaseOpImageSparseFetch::createInstance(Context &context) const
{
    return new SparseShaderIntrinsicsInstanceFetch(context, m_function, m_imageType, m_imageSize, m_format);
}

class SparseShaderIntrinsicsInstanceRead : public SparseShaderIntrinsicsInstanceStorage
{
public:
    SparseShaderIntrinsicsInstanceRead(Context &context, const SpirVFunction function, const ImageType imageType,
                                       const tcu::UVec3 &imageSize, const VkFormat format)
        : SparseShaderIntrinsicsInstanceStorage(context, function, imageType, imageSize, format)
    {
    }

    VkImageUsageFlags imageSparseUsageFlags(void) const
    {
        return VK_IMAGE_USAGE_STORAGE_BIT;
    }
    VkDescriptorType imageSparseDescType(void) const
    {
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
};

TestInstance *SparseCaseOpImageSparseRead::createInstance(Context &context) const
{
    return new SparseShaderIntrinsicsInstanceRead(context, m_function, m_imageType, m_imageSize, m_format);
}

} // namespace sparse
} // namespace vkt
