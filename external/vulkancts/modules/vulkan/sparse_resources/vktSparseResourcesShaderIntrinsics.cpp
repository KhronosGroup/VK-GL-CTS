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

	static const std::string functions[SPARSE_SPIRV_FUNCTION_TYPE_LAST] =
	{
		"_sparse_fetch",
		"_sparse_read",
		"_sparse_sample_explicit_lod",
		"_sparse_sample_implicit_lod",
		"_sparse_gather",
	};

	for (deUint32 functionNdx = 0; functionNdx < SPARSE_SPIRV_FUNCTION_TYPE_LAST; ++functionNdx)
	{
		const SpirVFunction function = static_cast<SpirVFunction>(functionNdx);

		for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
		{
			const ImageType					imageType		= imageParameters[imageTypeNdx].imageType;
			de::MovePtr<tcu::TestCaseGroup> imageTypeGroup	(new tcu::TestCaseGroup(testCtx, (getImageTypeName(imageType) + functions[functionNdx]).c_str(), ""));

			for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
			{
				VkFormat						format				= imageParameters[imageTypeNdx].formats[formatNdx].format;
				tcu::UVec3						imageSizeAlignment	= getImageSizeAlignment(format);
				de::MovePtr<tcu::TestCaseGroup> formatGroup			(new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str(), ""));

				for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size(); ++imageSizeNdx)
				{
					const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

					// skip test for images with odd sizes for some YCbCr formats
					if ((imageSize.x() % imageSizeAlignment.x()) != 0)
						continue;
					if ((imageSize.y() % imageSizeAlignment.y()) != 0)
						continue;

					std::ostringstream stream;
					stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

					switch (function)
					{
						case SPARSE_FETCH:
							if ((imageType == IMAGE_TYPE_CUBE) || (imageType == IMAGE_TYPE_CUBE_ARRAY)) continue;
							break;
						case SPARSE_SAMPLE_EXPLICIT_LOD:
						case SPARSE_SAMPLE_IMPLICIT_LOD:
						case SPARSE_GATHER:
							if ((imageType == IMAGE_TYPE_CUBE) || (imageType == IMAGE_TYPE_CUBE_ARRAY) || (imageType == IMAGE_TYPE_3D)) continue;
							break;
						default:
							break;
					}

					switch (function)
					{
						case SPARSE_FETCH:
							formatGroup->addChild(new SparseCaseOpImageSparseFetch(testCtx, stream.str(), function, imageType, imageSize, format));
							break;
						case SPARSE_READ:
							formatGroup->addChild(new SparseCaseOpImageSparseRead(testCtx, stream.str(), function, imageType, imageSize, format));
							break;
						case SPARSE_SAMPLE_EXPLICIT_LOD:
							formatGroup->addChild(new SparseCaseOpImageSparseSampleExplicitLod(testCtx, stream.str(), function, imageType, imageSize, format));
							break;
						case SPARSE_SAMPLE_IMPLICIT_LOD:
							formatGroup->addChild(new SparseCaseOpImageSparseSampleImplicitLod(testCtx, stream.str(), function, imageType, imageSize, format));
							break;
						case SPARSE_GATHER:
							formatGroup->addChild(new SparseCaseOpImageSparseGather(testCtx, stream.str(), function, imageType, imageSize, format));
							break;
						default:
							DE_FATAL("Unexpected function type");
							break;
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
