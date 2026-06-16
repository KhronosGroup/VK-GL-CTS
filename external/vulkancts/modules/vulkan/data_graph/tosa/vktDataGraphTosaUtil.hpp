#ifndef _VKTDATAGRAPHTOSAUTIL_HPP
#define _VKTDATAGRAPHTOSAUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025-2026 Arm Ltd.
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
 * \brief Tosa instruction set utilities
 */
/*--------------------------------------------------------------------*/

#include "../../tensor/vktTensorTestsUtil.hpp"
#include "../vktDataGraphTestUtil.hpp"
#include "vktDataGraphTosaSpirv.hpp"
#include "vktDataGraphTosaReference.hpp"

#include "spirv-tools/libspirv.hpp"

#include "vkTensorMemoryUtil.hpp"

#include <algorithm>
#include <array>
#include <regex>

namespace vkt
{
namespace dataGraph
{

using namespace vk;

inline std::vector<VkFormat> getVkFormats(const std::string &formatStr)
{
    if (formatStr == "i8")
        return {VK_FORMAT_R8_SINT};
    else if (formatStr == "fp32")
        return {VK_FORMAT_R32_SFLOAT};
    else if (formatStr == "fp16")
        return {VK_FORMAT_R16_SFLOAT};
    else if (formatStr == "i8i8i32")
        return {VK_FORMAT_R8_SINT, VK_FORMAT_R32_SINT};
    else if (formatStr == "fp32fp32fp32")
        return {VK_FORMAT_R32_SFLOAT};
    else if (formatStr == "fp16fp16fp16")
        return {VK_FORMAT_R16_SFLOAT};

    TCU_THROW(InternalError, "Unknown format string: " + formatStr);
}

template <VkFormat inOutFormat = VK_FORMAT_R32_SINT>
class DataGraphTestTosaAddSub : public DataGraphTest
{

private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    enum resourcesId
    {
        _input1,
        _input2,
        _output1,
        _output2,
        _num_resources
    };

public:
    DataGraphTestTosaAddSub(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input1) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_input2) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 1, 0, 1, nullptr, {}, ""};
        m_resInfo.at(_output1) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 2, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output2) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 3, 0, 1, nullptr, {}, ""};

        if (params.shuffleBindings)
        {
            m_resInfo.at(_input1).binding  = 2;
            m_resInfo.at(_input2).binding  = 3;
            m_resInfo.at(_output1).binding = 0;
            m_resInfo.at(_output2).binding = 1;
        }

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input1).params.strides =
                getTensorStrides(m_resInfo.at(_input1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input1).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
            m_resInfo.at(_input2).params.strides =
                getTensorStrides(m_resInfo.at(_input2).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input2).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 6 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output1).params.strides =
                getTensorStrides(m_resInfo.at(_output1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output1).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
            m_resInfo.at(_output2).params.strides =
                getTensorStrides(m_resInfo.at(_output2).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output2).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 6 : 1);
        }

        m_inData1  = {m_resInfo.at(_input1).params.dimensions, m_resInfo.at(_input1).params.strides};
        m_inData2  = {m_resInfo.at(_input2).params.dimensions, m_resInfo.at(_input2).params.strides};
        m_outData1 = {m_resInfo.at(_output1).params.dimensions, m_resInfo.at(_output1).params.strides};
        m_outData2 = {m_resInfo.at(_output2).params.dimensions, m_resInfo.at(_output2).params.strides};

        m_resInfo.at(_input1).hostData  = m_inData1.data();
        m_resInfo.at(_input2).hostData  = m_inData2.data();
        m_resInfo.at(_output1).hostData = m_outData1.data();
        m_resInfo.at(_output2).hostData = m_outData2.data();
    }

    ~DataGraphTestTosaAddSub() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::string out1 = dataGraphSpirv.addSpirvOp("ADD", {m_resInfo.at(_input1).label, m_resInfo.at(_input2).label},
                                                     m_resInfo.at(_output1).label);
        std::string out2 = dataGraphSpirv.addSpirvOp("SUB", {m_resInfo.at(_input1).label, m_resInfo.at(_input2).label},
                                                     m_resInfo.at(_output2).label);

        dataGraphSpirv.setOutput(out1, m_resInfo.at(_output1));
        dataGraphSpirv.setOutput(out2, m_resInfo.at(_output2));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input1:
        {
            m_inData1.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(5));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData1.data(),
                           m_inData1.memorySize());
        }
        break;
        case _input2:
        {
            m_inData2.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(3));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData2.data(),
                           m_inData2.memorySize());
        }
        break;
        case _output1:
        {
            m_outData1.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        case _output2:
        {
            m_outData2.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        if (id == _output1 || id == _output2)
        {
            const auto &r = m_resInfo.at(id);

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            if (id == _output1)
            {
                TosaReferenceImplementation::add(m_inData1, m_inData2, m_outData1);
                return verifyTensor(outTensorMemory, m_outData1);
            }

            if (id == _output2)
            {
                TosaReferenceImplementation::sub(m_inData1, m_inData2, m_outData2);
                return verifyTensor(outTensorMemory, m_outData2);
            }
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i32", "fp32", "fp16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i32")
        {
            return new DataGraphTestTosaAddSub<VK_FORMAT_R32_SINT>(testCtx, params);
        }
        else if (params.formats == "fp32")
        {
            return new DataGraphTestTosaAddSub<VK_FORMAT_R32_SFLOAT>(testCtx, params);
        }
        else if (params.formats == "fp16")
        {
            return new DataGraphTestTosaAddSub<VK_FORMAT_R16_SFLOAT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData1;
    StridedMemoryUtils<inOutHostType> m_inData2;
    StridedMemoryUtils<inOutHostType> m_outData1;
    StridedMemoryUtils<inOutHostType> m_outData2;
};

template <VkFormat inOutFormat = VK_FORMAT_R8_SINT>
class DataGraphTestTosaMaxpool : public DataGraphTest
{
private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    int32_t m_kernelY   = 2;
    int32_t m_kernelX   = 2;
    int32_t m_strideY   = 2;
    int32_t m_strideX   = 2;
    int32_t m_padTop    = 0;
    int32_t m_padBottom = 0;
    int32_t m_padLeft   = 0;
    int32_t m_padRight  = 0;

    enum resourcesId
    {
        _input,
        _output,
        _num_resources
    };

public:
    DataGraphTestTosaMaxpool(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        const TensorDimensions inDimensions =
            params.imageAliasing ? TensorDimensions{1, 128, 256, 1} : TensorDimensions{1, 8, 16, 4};
        const TensorDimensions outDimensions =
            params.imageAliasing ? TensorDimensions{1, 64, 128, 1} : TensorDimensions{1, 4, 8, 4};

        m_resInfo.at(_input) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, inDimensions, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, outDimensions, {}}, 1, 0, 0, nullptr, {}, ""};

        if (params.shuffleBindings)
        {
            m_resInfo.at(_input).binding  = 1;
            m_resInfo.at(_output).binding = 0;
        }

        if (params.strides.inputs == TENSOR_STRIDES_PACKED || params.strides.inputs == TENSOR_STRIDES_NOT_PACKED)
        {
            m_resInfo.at(_input).params.strides = getTensorStrides(
                m_resInfo.at(_input).params.dimensions, vkt::tensor::getFormatSize(m_resInfo.at(_input).params.format),
                (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs == TENSOR_STRIDES_PACKED || params.strides.outputs == TENSOR_STRIDES_NOT_PACKED)
        {
            m_resInfo.at(_output).params.strides =
                getTensorStrides(m_resInfo.at(_output).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        // In image aliasing tests, tensor strides are written later by ImageAliasedTensorWithMemory from the image
        // layout. For these tests, these host-side buffers intentionally use packed strides as logical input/reference data.
        m_inData  = {m_resInfo.at(_input).params.dimensions, m_resInfo.at(_input).params.strides};
        m_outData = {m_resInfo.at(_output).params.dimensions, m_resInfo.at(_output).params.strides};

        m_resInfo.at(_input).hostData  = m_inData.data();
        m_resInfo.at(_output).hostData = m_outData.data();
    }

    ~DataGraphTestTosaMaxpool() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::vector<int64_t> kernels  = {m_kernelY, m_kernelX};
        std::vector<int64_t> strides  = {m_strideY, m_strideX};
        std::vector<int64_t> paddings = {m_padTop, m_padBottom, m_padLeft, m_padRight};

        const std::string kernel = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, kernels, "kernel");
        const std::string stride = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, strides, "stride");
        const std::string pad    = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, paddings, "pad");
        const std::string nan_mode =
            dataGraphSpirv.addAttribute(VK_FORMAT_R32_UINT, spirv_nan_mode::PROPAGATE, "nan_mode");

        const std::string maxpool = dataGraphSpirv.addSpirvOp(
            "MAX_POOL2D", {m_resInfo.at(_input).label}, m_resInfo.at(_output).label, {kernel, stride, pad, nan_mode});

        dataGraphSpirv.setOutput(maxpool, m_resInfo.at(_output));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input:
        {
            m_inData.fill(options.startingValue);
            if (m_params.imageAliasing)
            {
                uploadToImageAliasedTensor(m_context, vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData,
                                           m_resInfo.at(_input).params);
            }
            else
            {
                uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData.data(),
                               m_inData.memorySize());
            }
        }
        break;
        case _output:
        {
            m_outData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        if (id == _output)
        {
            const auto &r = m_resInfo.at(id);

            /* compute reference values for graph */
            TosaReferenceImplementation::maxpool2d(m_inData, m_kernelY, m_kernelX, m_outData);

            if (m_params.imageAliasing)
            {
                return verifyImageAliasedTensorWithShader(m_context, *tensor, m_outData, r.params);
            }
            else
            {
                const DeviceInterface &vk       = m_context.getDeviceInterface();
                const VkDevice device           = m_context.getDevice();
                const VkQueue queue             = m_context.getUniversalQueue();
                const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
                Allocator &allocator            = m_context.getDefaultAllocator();
                tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                                   outTensorMemory.memorySize());
                return verifyTensor(outTensorMemory, m_outData);
            }
        }
        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i8", "fp32", "fp16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i8")
        {
            return new DataGraphTestTosaMaxpool<VK_FORMAT_R8_SINT>(testCtx, params);
        }
        else if (params.formats == "fp32")
        {
            return new DataGraphTestTosaMaxpool<VK_FORMAT_R32_SFLOAT>(testCtx, params);
        }
        else if (params.formats == "fp16")
        {
            return new DataGraphTestTosaMaxpool<VK_FORMAT_R16_SFLOAT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData;
    StridedMemoryUtils<inOutHostType> m_outData;
};

template <VkFormat inOutFormat = VK_FORMAT_R8_SINT>
class DataGraphTestTosaMaxpoolTwoLayers : public DataGraphTest
{

private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    int32_t m_kernelY   = 2;
    int32_t m_kernelX   = 2;
    int32_t m_strideY   = 2;
    int32_t m_strideX   = 2;
    int32_t m_padTop    = 0;
    int32_t m_padBottom = 0;
    int32_t m_padLeft   = 0;
    int32_t m_padRight  = 0;

    tensor::TensorParameters transientParams{inOutFormat, VK_TENSOR_TILING_LINEAR_ARM, {1, 4, 8, 4}, {}};

    enum resourcesId
    {
        _input,
        _output,
        _num_resources
    };

public:
    DataGraphTestTosaMaxpoolTwoLayers(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 2, 4, 4}, {}}, 1, 0, 0, nullptr, {}, ""};

        if (params.shuffleBindings)
        {
            m_resInfo.at(_input).binding  = 1;
            m_resInfo.at(_output).binding = 0;
        }

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input).params.strides = getTensorStrides(
                m_resInfo.at(_input).params.dimensions, vkt::tensor::getFormatSize(m_resInfo.at(_input).params.format),
                (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output).params.strides =
                getTensorStrides(m_resInfo.at(_output).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        m_inData        = {m_resInfo.at(_input).params.dimensions, m_resInfo.at(_input).params.strides};
        m_outData       = {m_resInfo.at(_output).params.dimensions, m_resInfo.at(_output).params.strides};
        m_transientData = {transientParams.dimensions, transientParams.strides};

        m_resInfo.at(_input).hostData  = m_inData.data();
        m_resInfo.at(_output).hostData = m_outData.data();
    }

    ~DataGraphTestTosaMaxpoolTwoLayers() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::vector<int64_t> kernels  = {m_kernelY, m_kernelX};
        std::vector<int64_t> strides  = {m_strideY, m_strideX};
        std::vector<int64_t> paddings = {m_padTop, m_padBottom, m_padLeft, m_padRight};

        const std::string kernel = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, kernels, "kernel");
        const std::string stride = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, strides, "stride");
        const std::string pad    = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, paddings, "pad");
        const std::string nan_mode =
            dataGraphSpirv.addAttribute(TosaSpirv::format::i32_t, spirv_nan_mode::PROPAGATE, "nan_mode");

        const std::string transient =
            dataGraphSpirv.defineTensor(transientParams.format, transientParams.dimensions.data(),
                                        static_cast<uint32_t>(transientParams.dimensions.size()));

        const std::string maxpool1 = dataGraphSpirv.addSpirvOp("MAX_POOL2D", {m_resInfo.at(_input).label}, transient,
                                                               {kernel, stride, pad, nan_mode});
        const std::string maxpool2 = dataGraphSpirv.addSpirvOp("MAX_POOL2D", {maxpool1}, m_resInfo.at(_output).label,
                                                               {kernel, stride, pad, nan_mode});

        dataGraphSpirv.setOutput(maxpool2, m_resInfo.at(_output));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input:
        {
            m_inData.fill(options.startingValue);
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData.data(),
                           m_inData.memorySize());
        }
        break;
        case _output:
        {
            m_outData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        if (id == _output)
        {
            const auto &r = m_resInfo.at(id);

            const DeviceInterface &vk       = m_context.getDeviceInterface();
            const VkDevice device           = m_context.getDevice();
            const VkQueue queue             = m_context.getUniversalQueue();
            const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            Allocator &allocator            = m_context.getDefaultAllocator();

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            /* compute reference values for graph */
            TosaReferenceImplementation::maxpool2d(m_inData, m_kernelY, m_kernelX, m_transientData);
            TosaReferenceImplementation::maxpool2d(m_transientData, m_kernelY, m_kernelX, m_outData);

            return verifyTensor(outTensorMemory, m_outData);
        }
        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i8", "fp32", "fp16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i8")
        {
            return new DataGraphTestTosaMaxpoolTwoLayers<VK_FORMAT_R8_SINT>(testCtx, params);
        }
        else if (params.formats == "fp32")
        {
            return new DataGraphTestTosaMaxpoolTwoLayers<VK_FORMAT_R32_SFLOAT>(testCtx, params);
        }
        else if (params.formats == "fp16")
        {
            return new DataGraphTestTosaMaxpoolTwoLayers<VK_FORMAT_R16_SFLOAT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData;
    StridedMemoryUtils<inOutHostType> m_outData;
    StridedMemoryUtils<inOutHostType> m_transientData;
};

template <VkFormat inFormat = VK_FORMAT_R8_SINT, VkFormat weightsFormat = VK_FORMAT_R8_SINT,
          VkFormat outFormat = VK_FORMAT_R32_SINT>
class DataGraphTestTosaConvolution : public DataGraphTest
{

private:
    using inHostType      = typename DataGraphTest::vkFormatInfo<inFormat>::hostType;
    using outHostType     = typename DataGraphTest::vkFormatInfo<outFormat>::hostType;
    using weightsHostType = typename DataGraphTest::vkFormatInfo<weightsFormat>::hostType;

    int32_t m_strideY           = 2;
    int32_t m_strideX           = 2;
    int32_t m_dilationY         = 1;
    int32_t m_dilationX         = 1;
    int32_t m_padTop            = 0;
    int32_t m_padBottom         = 0;
    int32_t m_padLeft           = 0;
    int32_t m_padRight          = 0;
    inHostType m_inZp           = 0;
    weightsHostType m_weightsZp = 0;

    enum resourcesId
    {
        _input = 0,
        _output,
        _weights,
        _bias,
        _num_resources
    };

public:
    DataGraphTestTosaConvolution(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        const TensorDimensions inDimensions =
            params.imageAliasing ? TensorDimensions{1, 128, 256, 1} : TensorDimensions{1, 8, 16, 4};
        const TensorDimensions outDimensions =
            params.imageAliasing ? TensorDimensions{1, 64, 128, 1} : TensorDimensions{1, 4, 8, 4};
        const TensorDimensions weightsDimensions =
            params.imageAliasing ? TensorDimensions{1, 2, 2, 1} : TensorDimensions{4, 2, 2, 4};
        const TensorDimensions biasDimensions = params.imageAliasing ? TensorDimensions{1} : TensorDimensions{4};

        m_resInfo.at(_input) = {
            RESOURCE_TYPE_INPUT, {inFormat, params.tiling, inDimensions, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output) = {
            RESOURCE_TYPE_OUTPUT, {outFormat, params.tiling, outDimensions, {}}, 1, 0, 0, nullptr, {}, ""};
        // VUID-VkDataGraphPipelineConstantARM-pNext-09917 tiling member must be VK_TENSOR_TILING_LINEAR_ARM. Hence, constants ignore the test requirements
        m_resInfo.at(_weights) = {RESOURCE_TYPE_CONSTANT,
                                  {weightsFormat, VK_TENSOR_TILING_LINEAR_ARM, weightsDimensions, {}},
                                  0,
                                  0,
                                  0,
                                  nullptr,
                                  {},
                                  "weights"};
        // VUID-VkDataGraphPipelineConstantARM-pNext-09917 tiling member must be VK_TENSOR_TILING_LINEAR_ARM. Hence, constants ignore the test requirements
        m_resInfo.at(_bias) = {RESOURCE_TYPE_CONSTANT,
                               {outFormat, VK_TENSOR_TILING_LINEAR_ARM, biasDimensions, {}},
                               0,
                               0,
                               1,
                               nullptr,
                               {},
                               "bias"};

        if (params.shuffleBindings)
        {
            m_resInfo.at(_input).binding  = 1;
            m_resInfo.at(_output).binding = 0;
        }

        if (params.strides.inputs == TENSOR_STRIDES_PACKED || params.strides.inputs == TENSOR_STRIDES_NOT_PACKED)
        {
            m_resInfo.at(_input).params.strides = getTensorStrides(
                m_resInfo.at(_input).params.dimensions, vkt::tensor::getFormatSize(m_resInfo.at(_input).params.format),
                (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs == TENSOR_STRIDES_PACKED || params.strides.outputs == TENSOR_STRIDES_NOT_PACKED)
        {
            m_resInfo.at(_output).params.strides =
                getTensorStrides(m_resInfo.at(_output).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        switch (params.sparsity)
        {
        case SparsityVariation::NONE:
            break;

        case SparsityVariation::VARIATION_1:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 3, 4}, {1, 1, 2}, {2, 1, 2}, {3, 2, 4}};
            m_resInfo.at(_bias).sparsityInfo    = {{0, 1, 4}};
            break;
        }
        case SparsityVariation::VARIATION_2:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 1, 2}, {1, 1, 2}, {2, 1, 2}, {3, 1, 2}};
            m_resInfo.at(_bias).sparsityInfo    = {{0, 1, 2}};
            break;
        }
        case SparsityVariation::VARIATION_3:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 2, 4}};
            m_resInfo.at(_bias).sparsityInfo    = {};
            break;
        }
        }

        // In image aliasing tests, tensor strides are written later by ImageAliasedTensorWithMemory from the image
        // layout. These host-side buffers intentionally use packed strides as logical input/reference data.
        m_inData      = {m_resInfo.at(_input).params.dimensions, m_resInfo.at(_input).params.strides};
        m_outData     = {m_resInfo.at(_output).params.dimensions, m_resInfo.at(_output).params.strides};
        m_weightsData = {m_resInfo.at(_weights).params.dimensions, m_resInfo.at(_weights).params.strides};
        m_biasData    = {m_resInfo.at(_bias).params.dimensions, m_resInfo.at(_bias).params.strides};

        m_resInfo.at(_input).hostData   = m_inData.data();
        m_resInfo.at(_output).hostData  = m_outData.data();
        m_resInfo.at(_weights).hostData = m_weightsData.data();
        m_resInfo.at(_bias).hostData    = m_biasData.data();
    }

    ~DataGraphTestTosaConvolution() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::vector<int64_t> paddings  = {m_padTop, m_padBottom, m_padLeft, m_padRight};
        std::vector<int64_t> strides   = {m_strideY, m_strideX};
        std::vector<int64_t> dilations = {m_dilationY, m_dilationX};

        const std::string input_zp  = dataGraphSpirv.addAttributeTensor(m_resInfo.at(_input).params.format,
                                                                        {static_cast<int64_t>(m_inZp)}, "input_zp");
        const std::string weight_zp = dataGraphSpirv.addAttributeTensor(
            m_resInfo.at(_weights).params.format, {static_cast<int64_t>(m_weightsZp)}, "weight_zp");

        const std::string pad      = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, paddings, "pad");
        const std::string stride   = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, strides, "stride");
        const std::string dilation = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, dilations, "dilation");
        const std::string accType  = dataGraphSpirv.addAttribute(
            TosaSpirv::format::i32_t, TosaSpirv::spirvAccType(m_resInfo.at(_output).params.format), "acc_type");
        const std::string local_bound = dataGraphSpirv.addAttribute(TosaSpirv::format::bool_t, 0, "local_bound");

        const std::string conv2d = dataGraphSpirv.addSpirvOp(
            "CONV2D",
            {m_resInfo.at(_input).label, m_resInfo.at(_weights).label, m_resInfo.at(_bias).label, input_zp, weight_zp},
            m_resInfo.at(_output).label, {pad, stride, dilation, accType, local_bound});

        dataGraphSpirv.setOutput(conv2d, m_resInfo.at(_output));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input:
        {
            m_inData.fill(static_cast<inHostType>(options.startingValue + 7));
            if (m_params.imageAliasing)
            {
                uploadToImageAliasedTensor(m_context, vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData,
                                           m_resInfo.at(_input).params);
            }
            else
            {
                uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData.data(),
                               m_inData.memorySize());
            }
        }
        break;
        case _output:
        {
            m_outData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        case _weights:
        {
            m_weightsData.fill(static_cast<weightsHostType>(static_cast<weightsHostType>(options.startingValue) +
                                                            static_cast<weightsHostType>(1.5)),
                               options.sparsityInfo);
        }
        break;
        case _bias:
        {
            m_biasData.fill(static_cast<outHostType>(static_cast<outHostType>(options.startingValue) +
                                                     static_cast<outHostType>(2.3)),
                            options.sparsityInfo);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        if (id == _output)
        {
            const auto &r = m_resInfo.at(id);

            /* compute reference values for graph */
            TosaReferenceImplementation::conv2d<inHostType, weightsHostType, outHostType>(
                m_inData, m_weightsData, m_biasData, m_outData, {m_padTop, m_padBottom, m_padLeft, m_padRight},
                {m_strideY, m_strideX}, {m_dilationY, m_dilationX}, m_inZp, m_weightsZp);

            if (m_params.imageAliasing)
            {
                return verifyImageAliasedTensorWithShader(m_context, *tensor, m_outData, r.params);
            }
            else
            {
                const DeviceInterface &vk       = m_context.getDeviceInterface();
                const VkDevice device           = m_context.getDevice();
                const VkQueue queue             = m_context.getUniversalQueue();
                const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
                Allocator &allocator            = m_context.getDefaultAllocator();
                tensor::StridedMemoryUtils<outHostType> outTensorMemory(r.params.dimensions, r.params.strides);
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                                   outTensorMemory.memorySize());
                return verifyTensor(outTensorMemory, m_outData);
            }
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i8i8i32", "fp32fp32fp32", "fp16fp16fp16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i8i8i32")
        {
            return new DataGraphTestTosaConvolution<VK_FORMAT_R8_SINT, VK_FORMAT_R8_SINT, VK_FORMAT_R32_SINT>(testCtx,
                                                                                                              params);
        }
        else if (params.formats == "fp32fp32fp32")
        {
            return new DataGraphTestTosaConvolution<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT>(
                testCtx, params);
        }
        else if (params.formats == "fp16fp16fp16")
        {
            return new DataGraphTestTosaConvolution<VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT>(
                testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inHostType> m_inData;
    StridedMemoryUtils<outHostType> m_outData;
    StridedMemoryUtils<weightsHostType> m_weightsData;
    StridedMemoryUtils<outHostType> m_biasData;
};

template <VkFormat inFormat = VK_FORMAT_R8_SINT, VkFormat weightsFormat = VK_FORMAT_R8_SINT,
          VkFormat outFormat = VK_FORMAT_R32_SINT>
class DataGraphTestTosaConvolutionTwoLayers : public DataGraphTest
{

private:
    using inHostType      = typename DataGraphTest::vkFormatInfo<inFormat>::hostType;
    using outHostType     = typename DataGraphTest::vkFormatInfo<outFormat>::hostType;
    using weightsHostType = typename DataGraphTest::vkFormatInfo<weightsFormat>::hostType;

    int32_t m_strideY           = 2;
    int32_t m_strideX           = 2;
    int32_t m_dilationY         = 1;
    int32_t m_dilationX         = 1;
    int32_t m_padTop            = 0;
    int32_t m_padBottom         = 0;
    int32_t m_padLeft           = 0;
    int32_t m_padRight          = 0;
    inHostType m_inZp           = 0;
    weightsHostType m_weightsZp = 0;

    tensor::TensorParameters transientParams1{outFormat, VK_TENSOR_TILING_LINEAR_ARM, {1, 4, 8, 4}, {}};
    tensor::TensorParameters transientParams2{inFormat, VK_TENSOR_TILING_LINEAR_ARM, {1, 4, 8, 4}, {}};

    enum resourcesId
    {
        _input,
        _output,
        _weights,
        _bias,
        _num_resources
    };

public:
    DataGraphTestTosaConvolutionTwoLayers(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input) = {
            RESOURCE_TYPE_INPUT, {inFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output) = {
            RESOURCE_TYPE_OUTPUT, {outFormat, params.tiling, {1, 2, 4, 4}, {}}, 1, 0, 0, nullptr, {}, ""};
        // VUID-VkDataGraphPipelineConstantARM-pNext-09917 tiling member must be VK_TENSOR_TILING_LINEAR_ARM. Hence, constants ignore the test requirements
        m_resInfo.at(_weights) = {RESOURCE_TYPE_CONSTANT,
                                  {weightsFormat, VK_TENSOR_TILING_LINEAR_ARM, {4, 2, 2, 4}, {}},
                                  0,
                                  0,
                                  0,
                                  nullptr,
                                  {},
                                  ""};
        // VUID-VkDataGraphPipelineConstantARM-pNext-09917 tiling member must be VK_TENSOR_TILING_LINEAR_ARM. Hence, constants ignore the test requirements
        m_resInfo.at(_bias) = {
            RESOURCE_TYPE_CONSTANT, {outFormat, VK_TENSOR_TILING_LINEAR_ARM, {4}, {}}, 0, 0, 1, nullptr, {}, ""};

        if (params.shuffleBindings)
        {
            m_resInfo.at(_input).binding  = 1;
            m_resInfo.at(_output).binding = 0;
        }

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input).params.strides = getTensorStrides(
                m_resInfo.at(_input).params.dimensions, vkt::tensor::getFormatSize(m_resInfo.at(_input).params.format),
                (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output).params.strides =
                getTensorStrides(m_resInfo.at(_output).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        switch (params.sparsity)
        {
        case SparsityVariation::NONE:
            break;

        case SparsityVariation::VARIATION_1:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 3, 4}, {1, 1, 2}, {2, 1, 2}, {3, 2, 4}};
            m_resInfo.at(_bias).sparsityInfo    = {{0, 1, 4}};
            break;
        }
        case SparsityVariation::VARIATION_2:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 1, 2}, {1, 1, 2}, {2, 1, 2}, {3, 1, 2}};
            m_resInfo.at(_bias).sparsityInfo    = {{0, 1, 2}};
            break;
        }
        case SparsityVariation::VARIATION_3:
        {
            m_resInfo.at(_weights).sparsityInfo = {{0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 2, 4}};
            m_resInfo.at(_bias).sparsityInfo    = {};
            break;
        }
        }

        m_inData         = {m_resInfo.at(_input).params.dimensions, m_resInfo.at(_input).params.strides};
        m_outData        = {m_resInfo.at(_output).params.dimensions, m_resInfo.at(_output).params.strides};
        m_weightsData    = {m_resInfo.at(_weights).params.dimensions, m_resInfo.at(_weights).params.strides};
        m_biasData       = {m_resInfo.at(_bias).params.dimensions, m_resInfo.at(_bias).params.strides};
        m_transientData1 = {transientParams1.dimensions, transientParams1.strides};
        m_transientData2 = {transientParams2.dimensions, transientParams2.strides};

        m_resInfo.at(_input).hostData   = m_inData.data();
        m_resInfo.at(_output).hostData  = m_outData.data();
        m_resInfo.at(_weights).hostData = m_weightsData.data();
        m_resInfo.at(_bias).hostData    = m_biasData.data();
    }

    ~DataGraphTestTosaConvolutionTwoLayers() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::vector<int64_t> paddings  = {m_padTop, m_padBottom, m_padLeft, m_padRight};
        std::vector<int64_t> strides   = {m_strideY, m_strideX};
        std::vector<int64_t> dilations = {m_dilationY, m_dilationX};

        const std::string transient1 =
            dataGraphSpirv.defineTensor(transientParams1.format, transientParams1.dimensions.data(),
                                        static_cast<uint32_t>(transientParams1.dimensions.size()));
        const std::string transient2 =
            dataGraphSpirv.defineTensor(transientParams2.format, transientParams2.dimensions.data(),
                                        static_cast<uint32_t>(transientParams2.dimensions.size()));

        const std::string input_zp  = dataGraphSpirv.addAttributeTensor(m_resInfo.at(_input).params.format,
                                                                        {static_cast<int64_t>(m_inZp)}, "input_zp");
        const std::string weight_zp = dataGraphSpirv.addAttributeTensor(
            m_resInfo.at(_weights).params.format, {static_cast<int64_t>(m_weightsZp)}, "weight_zp");

        const std::string pad      = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, paddings, "pad");
        const std::string stride   = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, strides, "stride");
        const std::string dilation = dataGraphSpirv.addAttributeTensor(TosaSpirv::format::i32_t, dilations, "dilation");
        const std::string accType1 = dataGraphSpirv.addAttribute(
            TosaSpirv::format::i32_t, TosaSpirv::spirvAccType(transientParams1.format), "acc_type");
        const std::string accType2 = dataGraphSpirv.addAttribute(
            TosaSpirv::format::i32_t, TosaSpirv::spirvAccType(m_resInfo.at(_output).params.format), "acc_type");
        const std::string local_bound = dataGraphSpirv.addAttribute(TosaSpirv::format::bool_t, 0, "local_bound");

        std::string conv1 = dataGraphSpirv.addSpirvOp(
            "CONV2D",
            {m_resInfo.at(_input).label, m_resInfo.at(_weights).label, m_resInfo.at(_bias).label, input_zp, weight_zp},
            transient1, {pad, stride, dilation, accType1, local_bound});

        std::string cast;
        if (outFormat == inFormat)
        {
            /* CAST not necessary, skip it or will fail graph compilation */
            cast = conv1;
        }
        else
        {
            cast = dataGraphSpirv.addSpirvOp("CAST", {conv1}, transient2);
        }

        std::string conv2 = dataGraphSpirv.addSpirvOp(
            "CONV2D", {cast, m_resInfo.at(_weights).label, m_resInfo.at(_bias).label, input_zp, weight_zp},
            m_resInfo.at(_output).label, {pad, stride, dilation, accType2, local_bound});

        dataGraphSpirv.setOutput(conv2, m_resInfo.at(_output));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input:
        {
            m_inData.fill(static_cast<inHostType>(options.startingValue + 7));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData.data(),
                           m_inData.memorySize());
        }
        break;
        case _output:
        {
            m_outData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        case _weights:
        {
            m_weightsData.fill(static_cast<weightsHostType>(static_cast<weightsHostType>(options.startingValue) +
                                                            static_cast<weightsHostType>(1.5)),
                               options.sparsityInfo);
        }
        break;
        case _bias:
        {
            m_biasData.fill(static_cast<outHostType>(static_cast<outHostType>(options.startingValue) +
                                                     static_cast<outHostType>(2.3)),
                            options.sparsityInfo);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        if (id == _output)
        {
            const auto &r = m_resInfo.at(id);

            const DeviceInterface &vk       = m_context.getDeviceInterface();
            const VkDevice device           = m_context.getDevice();
            const VkQueue queue             = m_context.getUniversalQueue();
            const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            Allocator &allocator            = m_context.getDefaultAllocator();

            tensor::StridedMemoryUtils<outHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            /* compute reference values for graph */
            TosaReferenceImplementation::conv2d<inHostType, weightsHostType, outHostType>(
                m_inData, m_weightsData, m_biasData, m_transientData1, {m_padTop, m_padBottom, m_padLeft, m_padRight},
                {m_strideY, m_strideX}, {m_dilationY, m_dilationX}, m_inZp, m_weightsZp);
            TosaReferenceImplementation::vector_cast<outHostType, inHostType>(m_transientData1, m_transientData2);
            TosaReferenceImplementation::conv2d<inHostType, weightsHostType, outHostType>(
                m_transientData2, m_weightsData, m_biasData, m_outData, {m_padTop, m_padBottom, m_padLeft, m_padRight},
                {m_strideY, m_strideX}, {m_dilationY, m_dilationX}, m_inZp, m_weightsZp);

            return verifyTensor(outTensorMemory, m_outData);
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i8i8i32", "fp32fp32fp32", "fp16fp16fp16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i8i8i32")
        {
            return new DataGraphTestTosaConvolutionTwoLayers<VK_FORMAT_R8_SINT, VK_FORMAT_R8_SINT, VK_FORMAT_R32_SINT>(
                testCtx, params);
        }
        else if (params.formats == "fp32fp32fp32")
        {
            return new DataGraphTestTosaConvolutionTwoLayers<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT,
                                                             VK_FORMAT_R32_SFLOAT>(testCtx, params);
        }
        else if (params.formats == "fp16fp16fp16")
        {
            return new DataGraphTestTosaConvolutionTwoLayers<VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT,
                                                             VK_FORMAT_R16_SFLOAT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inHostType> m_inData;
    StridedMemoryUtils<outHostType> m_outData;
    StridedMemoryUtils<weightsHostType> m_weightsData;
    StridedMemoryUtils<outHostType> m_biasData;
    StridedMemoryUtils<outHostType> m_transientData1;
    StridedMemoryUtils<inHostType> m_transientData2;
};

template <VkFormat inOutFormat = VK_FORMAT_R16_SINT>
class DataGraphTestTosaSpecConstant : public DataGraphTest
{

private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    enum resourcesId
    {
        _input1,
        _output1,
        _num_resources
    };

    static constexpr inOutHostType minValue        = 20;
    static constexpr inOutHostType maxValue        = 200;
    static constexpr inOutHostType minValueInitial = 10;
    static constexpr inOutHostType maxValueInitial = 100;

public:
    DataGraphTestTosaSpecConstant(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input1) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output1) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 1, 0, 0, nullptr, {}, ""};

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input1).params.strides =
                getTensorStrides(m_resInfo.at(_input1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input1).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output1).params.strides =
                getTensorStrides(m_resInfo.at(_output1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output1).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        m_inData1  = {m_resInfo.at(_input1).params.dimensions, m_resInfo.at(_input1).params.strides};
        m_outData1 = {m_resInfo.at(_output1).params.dimensions, m_resInfo.at(_output1).params.strides};

        m_resInfo.at(_input1).hostData  = m_inData1.data();
        m_resInfo.at(_output1).hostData = m_outData1.data();

        m_specializationConstants.emplace_back(SpecConstant::fromValue<inOutHostType>(0, minValue));
        m_specializationConstants.emplace_back(SpecConstant::fromValue<inOutHostType>(1, maxValue));
    }

    ~DataGraphTestTosaSpecConstant() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }
        const std::string minVal =
            dataGraphSpirv.addSpecializationAttribute(inOutFormat, minValueInitial, 0, "min_val");
        const std::string maxVal =
            dataGraphSpirv.addSpecializationAttribute(inOutFormat, maxValueInitial, 1, "max_val");

        const std::string nanMode =
            dataGraphSpirv.addAttribute(VK_FORMAT_R32_UINT, spirv_nan_mode::PROPAGATE, "nan_mode");

        std::string out1 = dataGraphSpirv.addSpirvOp("CLAMP", {m_resInfo.at(_input1).label},
                                                     m_resInfo.at(_output1).label, {minVal, maxVal, nanMode});

        dataGraphSpirv.setOutput(out1, m_resInfo.at(_output1));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input1:
        {
            m_inData1.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(5));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData1.data(),
                           m_inData1.memorySize());
        }
        break;
        case _output1:
        {
            m_outData1.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        if (id == _output1)
        {
            const auto &r = m_resInfo.at(id);

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            TosaReferenceImplementation::clamp(m_inData1, m_outData1, minValue, maxValue);
            return verifyTensor(m_outData1, outTensorMemory);
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i16")
        {
            return new DataGraphTestTosaSpecConstant<VK_FORMAT_R16_SINT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData1;
    StridedMemoryUtils<inOutHostType> m_outData1;
};

template <VkFormat inOutFormat = VK_FORMAT_R16_SINT, bool shouldRound = true>
class DataGraphTestTosaSpecConstantBool : public DataGraphTest
{

private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    enum resourcesId
    {
        _input1,
        _input2,
        _output1,
        _num_resources
    };

    static constexpr inOutHostType shiftAmount = 1;

public:
    DataGraphTestTosaSpecConstantBool(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input1) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_input2) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 1, 0, 1, nullptr, {}, ""};
        m_resInfo.at(_output1) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 2, 0, 0, nullptr, {}, ""};

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input1).params.strides =
                getTensorStrides(m_resInfo.at(_input1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input1).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
            m_resInfo.at(_input2).params.strides =
                getTensorStrides(m_resInfo.at(_input2).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input2).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output1).params.strides =
                getTensorStrides(m_resInfo.at(_output1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output1).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        m_inData1  = {m_resInfo.at(_input1).params.dimensions, m_resInfo.at(_input1).params.strides};
        m_inData2  = {m_resInfo.at(_input2).params.dimensions, m_resInfo.at(_input2).params.strides};
        m_outData1 = {m_resInfo.at(_output1).params.dimensions, m_resInfo.at(_output1).params.strides};

        m_resInfo.at(_input1).hostData  = m_inData1.data();
        m_resInfo.at(_input2).hostData  = m_inData2.data();
        m_resInfo.at(_output1).hostData = m_outData1.data();

        m_specializationConstants.emplace_back(SpecConstant::fromValue<uint32_t>(0, shouldRound ? 1 : 0));
    }

    ~DataGraphTestTosaSpecConstantBool() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        const std::string round =
            dataGraphSpirv.addSpecializationAttribute(VK_FORMAT_R8_BOOL_ARM, shouldRound ? 0 : 1, 0, "round");

        std::string out1 = dataGraphSpirv.addSpirvOp("ARITHMETIC_RIGHT_SHIFT",
                                                     {m_resInfo.at(_input1).label, m_resInfo.at(_input2).label},
                                                     m_resInfo.at(_output1).label, {round});

        dataGraphSpirv.setOutput(out1, m_resInfo.at(_output1));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input1:
        {
            m_inData1.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(5));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData1.data(),
                           m_inData1.memorySize());
        }
        break;
        case _input2:
        {
            m_inData2.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(5));
            for (size_t i = 0; i < m_inData2.elementCount(); ++i)
            {
                m_inData2.at(i) = shiftAmount;
            }
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData2.data(),
                           m_inData2.memorySize());
        }
        break;
        case _output1:
        {
            m_outData1.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        if (id == _output1)
        {
            const auto &r = m_resInfo.at(id);

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            TosaReferenceImplementation::arithmetic_right_shift(m_inData1, m_inData2, m_outData1, shouldRound);
            return verifyTensor(m_outData1, outTensorMemory);
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i16")
        {
            return new DataGraphTestTosaSpecConstantBool<VK_FORMAT_R16_SINT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData1;
    StridedMemoryUtils<inOutHostType> m_inData2;
    StridedMemoryUtils<inOutHostType> m_outData1;
};

template <VkFormat inOutFormat = VK_FORMAT_R8_SINT, bool Replicated = false>
class DataGraphTestTosaSpecConstantComposite : public DataGraphTest
{
private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    static constexpr int32_t m_kernelY   = 2;
    static constexpr int32_t m_kernelX   = 2;
    static constexpr int32_t m_strideY   = 2;
    static constexpr int32_t m_strideX   = 2;
    static constexpr int32_t m_padTop    = 0;
    static constexpr int32_t m_padBottom = 0;
    static constexpr int32_t m_padLeft   = 0;
    static constexpr int32_t m_padRight  = 0;

    static constexpr uint32_t m_specIdKernelY   = 0;
    static constexpr uint32_t m_specIdKernelX   = 1;
    static constexpr uint32_t m_specIdStrideY   = 2;
    static constexpr uint32_t m_specIdStrideX   = 3;
    static constexpr uint32_t m_specIdPadTop    = 4;
    static constexpr uint32_t m_specIdPadBottom = 5;
    static constexpr uint32_t m_specIdPadLeft   = 6;
    static constexpr uint32_t m_specIdPadRight  = 7;

    enum resourcesId
    {
        _input,
        _output,
        _num_resources
    };

public:
    DataGraphTestTosaSpecConstantComposite(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        if (Replicated)
        {
            context.requireDeviceFunctionality("VK_EXT_shader_replicated_composites");

            static_assert(m_kernelY == m_kernelX);
            static_assert(m_strideY == m_strideX);
            static_assert(m_padTop == m_padBottom && m_padTop == m_padLeft && m_padTop == m_padRight);
        }

        m_resInfo.at(_input) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 4, 8, 4}, {}}, 1, 0, 0, nullptr, {}, ""};

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input).params.strides = getTensorStrides(
                m_resInfo.at(_input).params.dimensions, vkt::tensor::getFormatSize(m_resInfo.at(_input).params.format),
                (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output).params.strides =
                getTensorStrides(m_resInfo.at(_output).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        m_inData  = {m_resInfo.at(_input).params.dimensions, m_resInfo.at(_input).params.strides};
        m_outData = {m_resInfo.at(_output).params.dimensions, m_resInfo.at(_output).params.strides};

        m_resInfo.at(_input).hostData  = m_inData.data();
        m_resInfo.at(_output).hostData = m_outData.data();

        if (!Replicated)
        {
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdKernelY, m_kernelY));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdKernelX, m_kernelX));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdStrideY, m_strideY));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdStrideX, m_strideX));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdPadTop, m_padTop));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdPadBottom, m_padBottom));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdPadLeft, m_padLeft));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdPadRight, m_padRight));
        }
        else
        {
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdKernelY, m_kernelY));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdStrideY, m_strideY));
            m_specializationConstants.emplace_back(SpecConstant::fromValue<int32_t>(m_specIdPadTop, m_padTop));
        }
    }

    ~DataGraphTestTosaSpecConstantComposite() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        if (Replicated)
        {
            dataGraphSpirv.addExtension("SPV_EXT_replicated_composites");
            dataGraphSpirv.addCapability("ReplicatedCompositesEXT");
        }

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }

        std::string kernel;
        std::string stride;
        std::string pad;

        if (!Replicated)
        {
            std::vector<int64_t> kernels  = {42, 24};
            std::vector<int64_t> strides  = {24, 42};
            std::vector<int64_t> paddings = {24, 42, 24, 42};

            kernel = dataGraphSpirv.addSpecializationAttributeTensor(TosaSpirv::format::i32_t, kernels,
                                                                     {m_specIdKernelY, m_specIdKernelX}, "kernel");
            stride = dataGraphSpirv.addSpecializationAttributeTensor(TosaSpirv::format::i32_t, strides,
                                                                     {m_specIdStrideY, m_specIdStrideX}, "stride");
            pad    = dataGraphSpirv.addSpecializationAttributeTensor(
                TosaSpirv::format::i32_t, paddings,
                {m_specIdPadTop, m_specIdPadBottom, m_specIdPadLeft, m_specIdPadRight}, "pad");
        }
        else
        {
            kernel = dataGraphSpirv.addSpecializationAttributeTensorReplicated(TosaSpirv::format::i32_t, 2, 42,
                                                                               m_specIdKernelY, "kernel");
            stride = dataGraphSpirv.addSpecializationAttributeTensorReplicated(TosaSpirv::format::i32_t, 2, 24,
                                                                               m_specIdStrideY, "stride");
            pad    = dataGraphSpirv.addSpecializationAttributeTensorReplicated(TosaSpirv::format::i32_t, 4, 42,
                                                                               m_specIdPadTop, "pad");
        }

        const std::string nan_mode =
            dataGraphSpirv.addAttribute(VK_FORMAT_R32_UINT, spirv_nan_mode::PROPAGATE, "nan_mode");

        const std::string maxpool = dataGraphSpirv.addSpirvOp(
            "MAX_POOL2D", {m_resInfo.at(_input).label}, m_resInfo.at(_output).label, {kernel, stride, pad, nan_mode});

        dataGraphSpirv.setOutput(maxpool, m_resInfo.at(_output));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input:
        {
            m_inData.fill(options.startingValue);
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData.data(),
                           m_inData.memorySize());
        }
        break;
        case _output:
        {
            m_outData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        if (id == _output)
        {
            const auto &r = m_resInfo.at(id);

            const DeviceInterface &vk       = m_context.getDeviceInterface();
            const VkDevice device           = m_context.getDevice();
            const VkQueue queue             = m_context.getUniversalQueue();
            const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            Allocator &allocator            = m_context.getDefaultAllocator();

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            /* compute reference values for graph */
            TosaReferenceImplementation::maxpool2d(m_inData, m_kernelY, m_kernelX, m_outData);

            return verifyTensor(m_outData, outTensorMemory);
        }
        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i8"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i8")
        {
            switch (params.specConstants)
            {
            case SpecConstantTest::COMPOSITE:
                return new DataGraphTestTosaSpecConstantComposite<VK_FORMAT_R8_SINT>(testCtx, params);
            case SpecConstantTest::COMPOSITE_REPLICATED:
                return new DataGraphTestTosaSpecConstantComposite<VK_FORMAT_R8_SINT, true>(testCtx, params);
            default:
                break;
            }
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData;
    StridedMemoryUtils<inOutHostType> m_outData;
};

template <VkFormat inOutFormat = VK_FORMAT_R16_SINT>
class DataGraphTestTosaSpecConstantOp : public DataGraphTest
{

private:
    using inOutHostType = typename DataGraphTest::vkFormatInfo<inOutFormat>::hostType;

    enum resourcesId
    {
        _input1,
        _output1,
        _num_resources
    };

    static constexpr inOutHostType minValue        = 20;
    static constexpr inOutHostType maxValue        = 200;
    static constexpr inOutHostType maxValueOffset  = 42;
    static constexpr inOutHostType minValueInitial = 10;
    static constexpr inOutHostType maxValueInitial = 100;

public:
    DataGraphTestTosaSpecConstantOp(Context &context, TestParams params)
        : DataGraphTest(context, _num_resources)
        , m_params{params}
    {
        m_resInfo.at(_input1) = {
            RESOURCE_TYPE_INPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 0, 0, 0, nullptr, {}, ""};
        m_resInfo.at(_output1) = {
            RESOURCE_TYPE_OUTPUT, {inOutFormat, params.tiling, {1, 8, 16, 4}, {}}, 1, 0, 0, nullptr, {}, ""};

        if (params.strides.inputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_input1).params.strides =
                getTensorStrides(m_resInfo.at(_input1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_input1).params.format),
                                 (params.strides.inputs == TENSOR_STRIDES_NOT_PACKED) ? 3 : 1);
        }

        if (params.strides.outputs != TENSOR_STRIDES_IMPLICIT)
        {
            m_resInfo.at(_output1).params.strides =
                getTensorStrides(m_resInfo.at(_output1).params.dimensions,
                                 vkt::tensor::getFormatSize(m_resInfo.at(_output1).params.format),
                                 (params.strides.outputs == TENSOR_STRIDES_NOT_PACKED) ? 5 : 1);
        }

        m_inData1  = {m_resInfo.at(_input1).params.dimensions, m_resInfo.at(_input1).params.strides};
        m_outData1 = {m_resInfo.at(_output1).params.dimensions, m_resInfo.at(_output1).params.strides};

        m_resInfo.at(_input1).hostData  = m_inData1.data();
        m_resInfo.at(_output1).hostData = m_outData1.data();

        m_specializationConstants.emplace_back(SpecConstant::fromValue<inOutHostType>(0, minValue));
        m_specializationConstants.emplace_back(SpecConstant::fromValue<inOutHostType>(1, maxValue - maxValueOffset));
        m_specializationConstants.emplace_back(SpecConstant::fromValue<inOutHostType>(2, maxValueOffset));
    }

    ~DataGraphTestTosaSpecConstantOp() = default;

    std::vector<uint32_t> spirvBinary() override
    {
        TosaSpirv dataGraphSpirv;

        for (auto &r : m_resInfo)
        {
            r.label = dataGraphSpirv.addResource(r);
        }
        const std::string minVal =
            dataGraphSpirv.addSpecializationAttribute(inOutFormat, minValueInitial, 0, "min_val");
        const std::string maxVal =
            dataGraphSpirv.addSpecializationAttribute(inOutFormat, maxValueInitial, 1, "max_val");
        const std::string offsetVal = dataGraphSpirv.addSpecializationAttribute(inOutFormat, 24, 2, "offset_val");

        const std::string nanMode =
            dataGraphSpirv.addAttribute(VK_FORMAT_R32_UINT, spirv_nan_mode::PROPAGATE, "nan_mode");

        const std::string sum = dataGraphSpirv.addSpecConstantOp("IAdd", inOutFormat, {offsetVal, maxVal}, "sum");

        std::string out1 = dataGraphSpirv.addSpirvOp("CLAMP", {m_resInfo.at(_input1).label},
                                                     m_resInfo.at(_output1).label, {minVal, sum, nanMode});

        dataGraphSpirv.setOutput(out1, m_resInfo.at(_output1));

        std::string spirvEntryPoint = dataGraphSpirv.bake();
        std::string spirvSource     = dataGraphSpirv.source();

        spvtools::SpirvTools tools{SPV_ENV_UNIVERSAL_1_6};
        std::string spirvErrors;
        std::vector<uint32_t> binary;

        tools.SetMessageConsumer(
            [&spirvErrors](spv_message_level_t level, const char *, const spv_position_t &position, const char *message)
            { TosaSpirv::spirvMessageConsumer(level, position, message, spirvErrors); });

        if (!tools.Assemble(spirvSource, &binary))
        {
            TCU_THROW(InternalError, "Shader assembly failed: " + spirvErrors);
        }

        if (!tools.Validate(binary))
        {
            TCU_THROW(InternalError, "Invalid shader: " + spirvErrors);
        }

        return binary;
    }

    void initData(size_t id, TensorWithMemory *tensor, InitDataOptions options) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        switch (id)
        {
        case _input1:
        {
            m_inData1.fill(static_cast<inOutHostType>(options.startingValue) + static_cast<inOutHostType>(5));
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, m_inData1.data(),
                           m_inData1.memorySize());
        }
        break;
        case _output1:
        {
            m_outData1.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor);
        }
        break;
        default:
        {
        }
        break;
        }
    }

    tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor) override
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &allocator            = m_context.getDefaultAllocator();

        if (id == _output1)
        {
            const auto &r = m_resInfo.at(id);

            tensor::StridedMemoryUtils<inOutHostType> outTensorMemory(r.params.dimensions, r.params.strides);
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensor, outTensorMemory.data(),
                               outTensorMemory.memorySize());

            TosaReferenceImplementation::clamp(m_inData1, m_outData1, minValue, maxValue);
            return verifyTensor(m_outData1, outTensorMemory);
        }

        return tcu::TestStatus::pass("");
    }

    inline static const std::vector<std::string> supportedFormats = {"i16"};

    static DataGraphTest *getTest(Context &testCtx, TestParams params)
    {
        if (params.formats == "i16")
        {
            return new DataGraphTestTosaSpecConstantOp<VK_FORMAT_R16_SINT>(testCtx, params);
        }
        TCU_THROW(InternalError, "Unsupported test type the data graph test");
        return nullptr;
    }

private:
    de::MovePtr<ProgramBinary> m_programBinary;
    TestParams m_params;

    StridedMemoryUtils<inOutHostType> m_inData1;
    StridedMemoryUtils<inOutHostType> m_outData1;
};

class DataGraphTestProviderTosa
{
public:
    static const std::vector<std::string> &getSupportedFormats(TestParams params)
    {
        static const std::vector<std::string> emptyFormats = {};

        if (params.specConstants != SpecConstantTest::NONE)
        {
            switch (params.specConstants)
            {
            case SpecConstantTest::BASIC:
                return DataGraphTestTosaSpecConstant<>::supportedFormats;
            case SpecConstantTest::BOOL:
                return DataGraphTestTosaSpecConstantBool<>::supportedFormats;
            case SpecConstantTest::COMPOSITE:
            case SpecConstantTest::COMPOSITE_REPLICATED:
                return DataGraphTestTosaSpecConstantComposite<>::supportedFormats;
            case SpecConstantTest::OP:
                return DataGraphTestTosaSpecConstantOp<>::supportedFormats;
            default:
                break;
            }
        }

        if (params.imageAliasing)
        {
            if (params.cardinalities.constants == NONE)
                return DataGraphTestTosaMaxpool<>::supportedFormats;
            else if (params.cardinalities.constants == MANY)
                return DataGraphTestTosaConvolution<>::supportedFormats;

            return emptyFormats;
        }

        if (params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == NONE && !params.sessionMemory)
        {
            return DataGraphTestTosaMaxpool<>::supportedFormats;
        }

        if (params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == MANY && !params.sessionMemory)
        {
            return DataGraphTestTosaConvolution<>::supportedFormats;
        }

        if (params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == NONE && params.sessionMemory)
        {
            return DataGraphTestTosaMaxpoolTwoLayers<>::supportedFormats;
        }

        if (params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == MANY && params.sessionMemory)
        {
            return DataGraphTestTosaConvolutionTwoLayers<>::supportedFormats;
        }

        if (params.cardinalities.inputs == MANY && params.cardinalities.outputs == MANY &&
            params.cardinalities.constants == NONE && !params.sessionMemory)
        {
            return DataGraphTestTosaAddSub<>::supportedFormats;
        }

        return emptyFormats;
    }

    static DataGraphTest *getDataGraphTest(Context &testCtx, TestParams params)
    {
        if (params.imageAliasing)
        {
            if (params.cardinalities.constants == NONE)
                return DataGraphTestTosaMaxpool<>::getTest(testCtx, params);
            else if (params.cardinalities.constants == MANY)
                return DataGraphTestTosaConvolution<>::getTest(testCtx, params);
        }

        if (params.specConstants != SpecConstantTest::NONE)
        {
            switch (params.specConstants)
            {
            case SpecConstantTest::BASIC:
                return DataGraphTestTosaSpecConstant<>::getTest(testCtx, params);
            case SpecConstantTest::BOOL:
                return DataGraphTestTosaSpecConstantBool<>::getTest(testCtx, params);
            case SpecConstantTest::COMPOSITE:
            case SpecConstantTest::COMPOSITE_REPLICATED:
                return DataGraphTestTosaSpecConstantComposite<>::getTest(testCtx, params);
            case SpecConstantTest::OP:
                return DataGraphTestTosaSpecConstantOp<>::getTest(testCtx, params);
            default:
                break;
            }
        }

        if (!params.sessionMemory && params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == NONE)
        {
            return DataGraphTestTosaMaxpool<>::getTest(testCtx, params);
        }

        if (!params.sessionMemory && params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == MANY)
        {
            return DataGraphTestTosaConvolution<>::getTest(testCtx, params);
        }

        if (params.sessionMemory && params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == NONE)
        {
            return DataGraphTestTosaMaxpoolTwoLayers<>::getTest(testCtx, params);
        }

        if (params.sessionMemory && params.cardinalities.inputs == ONE && params.cardinalities.outputs == ONE &&
            params.cardinalities.constants == MANY)
        {
            return DataGraphTestTosaConvolutionTwoLayers<>::getTest(testCtx, params);
        }

        if (!params.sessionMemory && params.cardinalities.inputs == MANY && params.cardinalities.outputs == MANY &&
            params.cardinalities.constants == NONE)
        {
            return DataGraphTestTosaAddSub<>::getTest(testCtx, params);
        }

        TCU_THROW(NotSupportedError, "No test available for the given test parameters");
    }

private:
    /* No need to instantiate the class as we only expose static methods */
    DataGraphTestProviderTosa() = default;
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTOSAUTIL_HPP
