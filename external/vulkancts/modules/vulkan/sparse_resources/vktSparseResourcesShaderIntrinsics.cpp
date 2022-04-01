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
 * \file  vktSparseResourcesShaderIntrinsics.cpp
 * \brief Sparse Resources Shader Intrinsics
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesShaderIntrinsicsSampled.hpp"
#include "vktSparseResourcesShaderIntrinsicsStorage.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

struct SparseCaseParams
{
	std::string			name;
	SpirVFunction		function;
	ImageType			imageType;
	tcu::UVec3			imageSize;
	vk::VkFormat		format;
	std::string			operand;
};

template <typename SparseCase>
void addSparseCase(const SparseCaseParams& params, tcu::TestContext& testCtx, tcu::TestCaseGroup* group)
{
	group->addChild(new SparseCase(testCtx, params.name, params.function, params.imageType, params.imageSize, params.format, params.operand));
}

tcu::TestCaseGroup* createSparseResourcesShaderIntrinsicsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "shader_intrinsics", "Sparse Resources Shader Intrinsics"));

	const std::vector<TestImageParameters> imageParameters
	{
		{ IMAGE_TYPE_2D,			{ tcu::UVec3(512u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u), tcu::UVec3(503u, 137u, 1u), tcu::UVec3(11u, 37u, 1u) },	getTestFormats(IMAGE_TYPE_2D) },
		{ IMAGE_TYPE_2D_ARRAY,		{ tcu::UVec3(512u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u),	tcu::UVec3(11u, 37u, 3u) },	getTestFormats(IMAGE_TYPE_2D_ARRAY) },
		{ IMAGE_TYPE_CUBE,			{ tcu::UVec3(256u, 256u, 1u),	tcu::UVec3(128u, 128u, 1u),	tcu::UVec3(137u, 137u, 1u),	tcu::UVec3(11u, 11u, 1u) },	getTestFormats(IMAGE_TYPE_CUBE) },
		{ IMAGE_TYPE_CUBE_ARRAY,	{ tcu::UVec3(256u, 256u, 6u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(137u, 137u, 3u),	tcu::UVec3(11u, 11u, 3u) },	getTestFormats(IMAGE_TYPE_CUBE_ARRAY) },
		{ IMAGE_TYPE_3D,			{ tcu::UVec3(256u, 256u, 16u),	tcu::UVec3(128u, 128u, 8u),	tcu::UVec3(503u, 137u, 3u),	tcu::UVec3(11u, 37u, 3u) },	getTestFormats(IMAGE_TYPE_3D) }
	};

	static const std::string functions[SPARSE_SPIRV_FUNCTION_TYPE_LAST]
	{
		"_sparse_fetch",
		"_sparse_read",
		"_sparse_sample_explicit_lod",
		"_sparse_sample_implicit_lod",
		"_sparse_gather",
	};

	// store functions constructing cases in a map to avoid switch in a loop
	typedef void(*AddSparseCaseFun)(const SparseCaseParams&, tcu::TestContext&, tcu::TestCaseGroup*);
	const std::map<SpirVFunction, AddSparseCaseFun> sparseCaseFunMap
	{
		{ SPARSE_FETCH,					&addSparseCase<SparseCaseOpImageSparseFetch> },
		{ SPARSE_READ,					&addSparseCase<SparseCaseOpImageSparseRead> },
		{ SPARSE_SAMPLE_EXPLICIT_LOD,	&addSparseCase<SparseCaseOpImageSparseSampleExplicitLod> },
		{ SPARSE_SAMPLE_IMPLICIT_LOD,	&addSparseCase<SparseCaseOpImageSparseSampleImplicitLod> },
		{ SPARSE_GATHER,				&addSparseCase<SparseCaseOpImageSparseGather> }
	};

	SparseCaseParams caseParams;

	for (deUint32 functionNdx = 0; functionNdx < SPARSE_SPIRV_FUNCTION_TYPE_LAST; ++functionNdx)
	{
		caseParams.function = static_cast<SpirVFunction>(functionNdx);

		// grab function that should be used to construct case of proper type
		auto addCaseFunctionPtr = sparseCaseFunMap.at(caseParams.function);

		for (const auto& imageParams : imageParameters)
		{
			caseParams.imageType = imageParams.imageType;

			de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(new tcu::TestCaseGroup(testCtx, (getImageTypeName(caseParams.imageType) + functions[functionNdx]).c_str(), ""));

			for (const auto& testFormat : imageParams.formats)
			{
				caseParams.format = testFormat.format;

				tcu::UVec3						imageSizeAlignment	= getImageSizeAlignment(caseParams.format);
				de::MovePtr<tcu::TestCaseGroup> formatGroup			(new tcu::TestCaseGroup(testCtx, getImageFormatID(caseParams.format).c_str(), ""));

				for (size_t imageSizeNdx = 0; imageSizeNdx < imageParams.imageSizes.size(); ++imageSizeNdx)
				{
					caseParams.imageSize = imageParams.imageSizes[imageSizeNdx];

					// skip test for images with odd sizes for some YCbCr formats
					if (((caseParams.imageSize.x() % imageSizeAlignment.x()) != 0) ||
						((caseParams.imageSize.y() % imageSizeAlignment.y()) != 0))
						continue;

					// skip cases depending on image type
					switch (caseParams.function)
					{
						case SPARSE_FETCH:
							if ((caseParams.imageType == IMAGE_TYPE_CUBE) || (caseParams.imageType == IMAGE_TYPE_CUBE_ARRAY))
								continue;
							break;
						case SPARSE_SAMPLE_EXPLICIT_LOD:
						case SPARSE_SAMPLE_IMPLICIT_LOD:
						case SPARSE_GATHER:
							if ((caseParams.imageType == IMAGE_TYPE_CUBE) || (caseParams.imageType == IMAGE_TYPE_CUBE_ARRAY) || (caseParams.imageType == IMAGE_TYPE_3D))
								continue;
							break;
						default:
							break;
					}

					std::ostringstream nameStream;
					nameStream << caseParams.imageSize.x() << "_" << caseParams.imageSize.y() << "_" << caseParams.imageSize.z();
					caseParams.name = nameStream.str();

					caseParams.operand = "";
					(*addCaseFunctionPtr)(caseParams, testCtx, formatGroup.get());

					// duplicate tests with Nontemporal operand just for smallest size (which is the last one)
					if (imageSizeNdx == (imageParams.imageSizes.size() - 1))
					{
						caseParams.operand = "Nontemporal";
						caseParams.name += "_nontemporal";
						(*addCaseFunctionPtr)(caseParams, testCtx, formatGroup.get());
					}
				}
				imageTypeGroup->addChild(formatGroup.release());
			}
			testGroup->addChild(imageTypeGroup.release());
		}
	}

	return testGroup.release();
}

} // sparse
} // vkt
