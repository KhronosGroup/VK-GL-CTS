/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief GLSL textureGather[Offset[s]] tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderTextureGatherTests.hpp"
#include "vktShaderRender.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "gluTextureUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTexCompareVerifier.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuCommandLine.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include <algorithm>
#include <iterator>

using de::MovePtr;
using tcu::ConstPixelBufferAccess;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::PixelBufferAccess;
using tcu::TestLog;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

using std::string;
using std::vector;

namespace vkt
{
namespace sr
{
namespace
{

typedef ShaderRenderCaseInstance::ImageBackingMode ImageBackingMode;

enum
{
    SPEC_MAX_MIN_OFFSET = -8,
    SPEC_MIN_MAX_OFFSET = 7,
    // textureGatherOffsets requires parameters at compile time
    // Most implementations minimum is -32 and maximum is 31 so we will use those values
    IMPLEMENTATION_MIN_MIN_OFFSET = -32,
    IMPLEMENTATION_MAX_MAX_OFFSET = 31
};

enum TextureType
{
    TEXTURETYPE_2D,
    TEXTURETYPE_2D_ARRAY,
    TEXTURETYPE_CUBE,

    TEXTURETYPE_LAST
};

// \note TextureTestUtil functions are copied from glsTextureTestUtil
namespace TextureTestUtil
{

inline tcu::IVec4 getBitsVec(const tcu::PixelFormat &format)
{
    return tcu::IVec4(format.redBits, format.greenBits, format.blueBits, format.alphaBits);
}

inline tcu::BVec4 getCompareMask(const tcu::PixelFormat &format)
{
    return tcu::BVec4(format.redBits > 0, format.greenBits > 0, format.blueBits > 0, format.alphaBits > 0);
}

void computeQuadTexCoord2D(std::vector<float> &dst, const tcu::Vec2 &bottomLeft, const tcu::Vec2 &topRight)
{
    dst.resize(4 * 2);

    dst[0] = bottomLeft.x();
    dst[1] = bottomLeft.y();
    dst[2] = bottomLeft.x();
    dst[3] = topRight.y();
    dst[4] = topRight.x();
    dst[5] = bottomLeft.y();
    dst[6] = topRight.x();
    dst[7] = topRight.y();
}

void computeQuadTexCoord2DArray(std::vector<float> &dst, int layerNdx, const tcu::Vec2 &bottomLeft,
                                const tcu::Vec2 &topRight)
{
    dst.resize(4 * 3);

    dst[0]  = bottomLeft.x();
    dst[1]  = bottomLeft.y();
    dst[2]  = (float)layerNdx;
    dst[3]  = bottomLeft.x();
    dst[4]  = topRight.y();
    dst[5]  = (float)layerNdx;
    dst[6]  = topRight.x();
    dst[7]  = bottomLeft.y();
    dst[8]  = (float)layerNdx;
    dst[9]  = topRight.x();
    dst[10] = topRight.y();
    dst[11] = (float)layerNdx;
}

void computeQuadTexCoordCube(std::vector<float> &dst, tcu::CubeFace face, const tcu::Vec2 &bottomLeft,
                             const tcu::Vec2 &topRight)
{
    int sRow    = 0;
    int tRow    = 0;
    int mRow    = 0;
    float sSign = 1.0f;
    float tSign = 1.0f;
    float mSign = 1.0f;

    switch (face)
    {
    case tcu::CUBEFACE_NEGATIVE_X:
        mRow  = 0;
        sRow  = 2;
        tRow  = 1;
        mSign = -1.0f;
        tSign = -1.0f;
        break;
    case tcu::CUBEFACE_POSITIVE_X:
        mRow  = 0;
        sRow  = 2;
        tRow  = 1;
        sSign = -1.0f;
        tSign = -1.0f;
        break;
    case tcu::CUBEFACE_NEGATIVE_Y:
        mRow  = 1;
        sRow  = 0;
        tRow  = 2;
        mSign = -1.0f;
        tSign = -1.0f;
        break;
    case tcu::CUBEFACE_POSITIVE_Y:
        mRow = 1;
        sRow = 0;
        tRow = 2;
        break;
    case tcu::CUBEFACE_NEGATIVE_Z:
        mRow  = 2;
        sRow  = 0;
        tRow  = 1;
        mSign = -1.0f;
        sSign = -1.0f;
        tSign = -1.0f;
        break;
    case tcu::CUBEFACE_POSITIVE_Z:
        mRow  = 2;
        sRow  = 0;
        tRow  = 1;
        tSign = -1.0f;
        break;
    default:
        DE_ASSERT(false);
        return;
    }

    dst.resize(3 * 4);

    dst[0 + mRow] = mSign;
    dst[3 + mRow] = mSign;
    dst[6 + mRow] = mSign;
    dst[9 + mRow] = mSign;

    dst[0 + sRow] = sSign * bottomLeft.x();
    dst[3 + sRow] = sSign * bottomLeft.x();
    dst[6 + sRow] = sSign * topRight.x();
    dst[9 + sRow] = sSign * topRight.x();

    dst[0 + tRow] = tSign * bottomLeft.y();
    dst[3 + tRow] = tSign * topRight.y();
    dst[6 + tRow] = tSign * bottomLeft.y();
    dst[9 + tRow] = tSign * topRight.y();
}

} // namespace TextureTestUtil

// Round-to-zero int division, because pre-c++11 it's somewhat implementation-defined for negative values.
static inline int divRoundToZero(int a, int b)
{
    return de::abs(a) / de::abs(b) * deSign32(a) * deSign32(b);
}

static void fillWithRandomColorTiles(const PixelBufferAccess &dst, const Vec4 &minVal, const Vec4 &maxVal,
                                     uint32_t seed)
{
    const int numCols = dst.getWidth() >= 7 ? 7 : dst.getWidth();
    const int numRows = dst.getHeight() >= 5 ? 5 : dst.getHeight();
    de::Random rnd(seed);

    for (int slice = 0; slice < dst.getDepth(); slice++)
        for (int row = 0; row < numRows; row++)
            for (int col = 0; col < numCols; col++)
            {
                const int yBegin = (row + 0) * dst.getHeight() / numRows;
                const int yEnd   = (row + 1) * dst.getHeight() / numRows;
                const int xBegin = (col + 0) * dst.getWidth() / numCols;
                const int xEnd   = (col + 1) * dst.getWidth() / numCols;
                const Vec4 color = tcu::randomVector<float, 4>(rnd, minVal, maxVal);

                tcu::clear(tcu::getSubregion(dst, xBegin, yBegin, slice, xEnd - xBegin, yEnd - yBegin, 1), color);
            }
}

static inline bool isDepthFormat(const tcu::TextureFormat &fmt)
{
    return fmt.order == tcu::TextureFormat::D || fmt.order == tcu::TextureFormat::DS;
}

static inline bool isUnormFormatType(tcu::TextureFormat::ChannelType type)
{
    return type == tcu::TextureFormat::UNORM_INT8 || type == tcu::TextureFormat::UNORM_INT16 ||
           type == tcu::TextureFormat::UNORM_INT32;
}

static inline bool isSIntFormatType(tcu::TextureFormat::ChannelType type)
{
    return type == tcu::TextureFormat::SIGNED_INT8 || type == tcu::TextureFormat::SIGNED_INT16 ||
           type == tcu::TextureFormat::SIGNED_INT32;
}

static inline bool isUIntFormatType(tcu::TextureFormat::ChannelType type)
{
    return type == tcu::TextureFormat::UNSIGNED_INT8 || type == tcu::TextureFormat::UNSIGNED_INT16 ||
           type == tcu::TextureFormat::UNSIGNED_INT32;
}

enum TextureSwizzleComponent
{
    TEXTURESWIZZLECOMPONENT_R = 0,
    TEXTURESWIZZLECOMPONENT_G,
    TEXTURESWIZZLECOMPONENT_B,
    TEXTURESWIZZLECOMPONENT_A,
    TEXTURESWIZZLECOMPONENT_ZERO,
    TEXTURESWIZZLECOMPONENT_ONE,

    TEXTURESWIZZLECOMPONENT_LAST
};

static std::ostream &operator<<(std::ostream &stream, TextureSwizzleComponent comp)
{
    switch (comp)
    {
    case TEXTURESWIZZLECOMPONENT_R:
        return stream << "RED";
    case TEXTURESWIZZLECOMPONENT_G:
        return stream << "GREEN";
    case TEXTURESWIZZLECOMPONENT_B:
        return stream << "BLUE";
    case TEXTURESWIZZLECOMPONENT_A:
        return stream << "ALPHA";
    case TEXTURESWIZZLECOMPONENT_ZERO:
        return stream << "ZERO";
    case TEXTURESWIZZLECOMPONENT_ONE:
        return stream << "ONE";
    default:
        DE_ASSERT(false);
        return stream;
    }
}

struct MaybeTextureSwizzle
{
public:
    static MaybeTextureSwizzle createNoneTextureSwizzle(void);
    static MaybeTextureSwizzle createSomeTextureSwizzle(void);

    bool isSome(void) const;
    bool isNone(void) const;
    bool isIdentitySwizzle(void) const;

    tcu::Vector<TextureSwizzleComponent, 4> &getSwizzle(void);
    const tcu::Vector<TextureSwizzleComponent, 4> &getSwizzle(void) const;

private:
    MaybeTextureSwizzle(void);

    tcu::Vector<TextureSwizzleComponent, 4> m_swizzle;
    bool m_isSome;
};

static std::ostream &operator<<(std::ostream &stream, const MaybeTextureSwizzle &comp)
{
    if (comp.isNone())
        stream << "[default swizzle state]";
    else
        stream << "(" << comp.getSwizzle()[0] << ", " << comp.getSwizzle()[1] << ", " << comp.getSwizzle()[2] << ", "
               << comp.getSwizzle()[3] << ")";

    return stream;
}

MaybeTextureSwizzle MaybeTextureSwizzle::createNoneTextureSwizzle(void)
{
    MaybeTextureSwizzle swizzle;

    swizzle.m_swizzle[0] = TEXTURESWIZZLECOMPONENT_LAST;
    swizzle.m_swizzle[1] = TEXTURESWIZZLECOMPONENT_LAST;
    swizzle.m_swizzle[2] = TEXTURESWIZZLECOMPONENT_LAST;
    swizzle.m_swizzle[3] = TEXTURESWIZZLECOMPONENT_LAST;
    swizzle.m_isSome     = false;

    return swizzle;
}

MaybeTextureSwizzle MaybeTextureSwizzle::createSomeTextureSwizzle(void)
{
    MaybeTextureSwizzle swizzle;

    swizzle.m_swizzle[0] = TEXTURESWIZZLECOMPONENT_R;
    swizzle.m_swizzle[1] = TEXTURESWIZZLECOMPONENT_G;
    swizzle.m_swizzle[2] = TEXTURESWIZZLECOMPONENT_B;
    swizzle.m_swizzle[3] = TEXTURESWIZZLECOMPONENT_A;
    swizzle.m_isSome     = true;

    return swizzle;
}

bool MaybeTextureSwizzle::isSome(void) const
{
    return m_isSome;
}

bool MaybeTextureSwizzle::isNone(void) const
{
    return !m_isSome;
}

bool MaybeTextureSwizzle::isIdentitySwizzle(void) const
{
    return m_isSome && m_swizzle[0] == TEXTURESWIZZLECOMPONENT_R && m_swizzle[1] == TEXTURESWIZZLECOMPONENT_G &&
           m_swizzle[2] == TEXTURESWIZZLECOMPONENT_B && m_swizzle[3] == TEXTURESWIZZLECOMPONENT_A;
}

tcu::Vector<TextureSwizzleComponent, 4> &MaybeTextureSwizzle::getSwizzle(void)
{
    return m_swizzle;
}

const tcu::Vector<TextureSwizzleComponent, 4> &MaybeTextureSwizzle::getSwizzle(void) const
{
    return m_swizzle;
}

MaybeTextureSwizzle::MaybeTextureSwizzle(void)
    : m_swizzle(TEXTURESWIZZLECOMPONENT_LAST, TEXTURESWIZZLECOMPONENT_LAST, TEXTURESWIZZLECOMPONENT_LAST,
                TEXTURESWIZZLECOMPONENT_LAST)
    , m_isSome(false)
{
}

static vk::VkComponentSwizzle getTextureSwizzleComponent(TextureSwizzleComponent c)
{
    switch (c)
    {
    case TEXTURESWIZZLECOMPONENT_R:
        return vk::VK_COMPONENT_SWIZZLE_R;
    case TEXTURESWIZZLECOMPONENT_G:
        return vk::VK_COMPONENT_SWIZZLE_G;
    case TEXTURESWIZZLECOMPONENT_B:
        return vk::VK_COMPONENT_SWIZZLE_B;
    case TEXTURESWIZZLECOMPONENT_A:
        return vk::VK_COMPONENT_SWIZZLE_A;
    case TEXTURESWIZZLECOMPONENT_ZERO:
        return vk::VK_COMPONENT_SWIZZLE_ZERO;
    case TEXTURESWIZZLECOMPONENT_ONE:
        return vk::VK_COMPONENT_SWIZZLE_ONE;
    default:
        DE_ASSERT(false);
        return (vk::VkComponentSwizzle)0;
    }
}

template <typename T>
static inline T swizzleColorChannel(const tcu::Vector<T, 4> &src, TextureSwizzleComponent swizzle)
{
    switch (swizzle)
    {
    case TEXTURESWIZZLECOMPONENT_R:
        return src[0];
    case TEXTURESWIZZLECOMPONENT_G:
        return src[1];
    case TEXTURESWIZZLECOMPONENT_B:
        return src[2];
    case TEXTURESWIZZLECOMPONENT_A:
        return src[3];
    case TEXTURESWIZZLECOMPONENT_ZERO:
        return (T)0;
    case TEXTURESWIZZLECOMPONENT_ONE:
        return (T)1;
    default:
        DE_ASSERT(false);
        return (T)-1;
    }
}

template <typename T>
static inline tcu::Vector<T, 4> swizzleColor(const tcu::Vector<T, 4> &src, const MaybeTextureSwizzle &swizzle)
{
    DE_ASSERT(swizzle.isSome());

    tcu::Vector<T, 4> result;
    for (int i = 0; i < 4; i++)
        result[i] = swizzleColorChannel(src, swizzle.getSwizzle()[i]);
    return result;
}

template <typename T>
static void swizzlePixels(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src,
                          const MaybeTextureSwizzle &swizzle)
{
    DE_ASSERT(dst.getWidth() == src.getWidth() && dst.getHeight() == src.getHeight() &&
              dst.getDepth() == src.getDepth());
    for (int z = 0; z < src.getDepth(); z++)
        for (int y = 0; y < src.getHeight(); y++)
            for (int x = 0; x < src.getWidth(); x++)
                dst.setPixel(swizzleColor(src.getPixelT<T>(x, y, z), swizzle), x, y, z);
}

static void swizzlePixels(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src,
                          const MaybeTextureSwizzle &swizzle)
{
    if (isDepthFormat(dst.getFormat()))
        DE_ASSERT(swizzle.isNone() || swizzle.isIdentitySwizzle());

    if (swizzle.isNone() || swizzle.isIdentitySwizzle())
        tcu::copy(dst, src);
    else if (isUnormFormatType(dst.getFormat().type))
        swizzlePixels<float>(dst, src, swizzle);
    else if (isUIntFormatType(dst.getFormat().type))
        swizzlePixels<uint32_t>(dst, src, swizzle);
    else if (isSIntFormatType(dst.getFormat().type))
        swizzlePixels<int32_t>(dst, src, swizzle);
    else
        DE_ASSERT(false);
}

static void swizzleTexture(tcu::Texture2D &dst, const tcu::Texture2D &src, const MaybeTextureSwizzle &swizzle)
{
    dst = tcu::Texture2D(src.getFormat(), src.getWidth(), src.getHeight());
    for (int levelNdx = 0; levelNdx < src.getNumLevels(); levelNdx++)
    {
        if (src.isLevelEmpty(levelNdx))
            continue;
        dst.allocLevel(levelNdx);
        swizzlePixels(dst.getLevel(levelNdx), src.getLevel(levelNdx), swizzle);
    }
}

static void swizzleTexture(tcu::Texture2DArray &dst, const tcu::Texture2DArray &src, const MaybeTextureSwizzle &swizzle)
{
    dst = tcu::Texture2DArray(src.getFormat(), src.getWidth(), src.getHeight(), src.getNumLayers());
    for (int levelNdx = 0; levelNdx < src.getNumLevels(); levelNdx++)
    {
        if (src.isLevelEmpty(levelNdx))
            continue;
        dst.allocLevel(levelNdx);
        swizzlePixels(dst.getLevel(levelNdx), src.getLevel(levelNdx), swizzle);
    }
}

static void swizzleTexture(tcu::TextureCube &dst, const tcu::TextureCube &src, const MaybeTextureSwizzle &swizzle)
{
    dst = tcu::TextureCube(src.getFormat(), src.getSize());
    for (int faceI = 0; faceI < tcu::CUBEFACE_LAST; faceI++)
    {
        const tcu::CubeFace face = (tcu::CubeFace)faceI;
        for (int levelNdx = 0; levelNdx < src.getNumLevels(); levelNdx++)
        {
            if (src.isLevelEmpty(face, levelNdx))
                continue;
            dst.allocLevel(face, levelNdx);
            swizzlePixels(dst.getLevelFace(levelNdx, face), src.getLevelFace(levelNdx, face), swizzle);
        }
    }
}

static tcu::Texture2DView getOneLevelSubView(const tcu::Texture2DView &view, int level)
{
    return tcu::Texture2DView(1, view.getLevels() + level);
}

static tcu::Texture2DArrayView getOneLevelSubView(const tcu::Texture2DArrayView &view, int level)
{
    return tcu::Texture2DArrayView(1, view.getLevels() + level);
}

static tcu::TextureCubeView getOneLevelSubView(const tcu::TextureCubeView &view, int level)
{
    const tcu::ConstPixelBufferAccess *levels[tcu::CUBEFACE_LAST];

    for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        levels[face] = view.getFaceLevels((tcu::CubeFace)face) + level;

    return tcu::TextureCubeView(1, levels);
}

class PixelOffsets
{
public:
    virtual void operator()(const IVec2 &pixCoord, IVec2 (&dst)[4]) const = 0;
    virtual ~PixelOffsets(void)
    {
    }
};

class MultiplePixelOffsets : public PixelOffsets
{
public:
    MultiplePixelOffsets(const IVec2 &a, const IVec2 &b, const IVec2 &c, const IVec2 &d)
    {
        m_offsets[0] = a;
        m_offsets[1] = b;
        m_offsets[2] = c;
        m_offsets[3] = d;
    }

    void operator()(const IVec2 & /* pixCoord */, IVec2 (&dst)[4]) const
    {
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(dst); i++)
            dst[i] = m_offsets[i];
    }

private:
    IVec2 m_offsets[4];
};

class SinglePixelOffsets : public MultiplePixelOffsets
{
public:
    SinglePixelOffsets(const IVec2 &offset)
        : MultiplePixelOffsets(offset + IVec2(0, 1), offset + IVec2(1, 1), offset + IVec2(1, 0), offset + IVec2(0, 0))
    {
    }
};

class DynamicSinglePixelOffsets : public PixelOffsets
{
public:
    DynamicSinglePixelOffsets(const IVec2 &offsetRange) : m_offsetRange(offsetRange)
    {
    }

    void operator()(const IVec2 &pixCoord, IVec2 (&dst)[4]) const
    {
        const int offsetRangeSize = m_offsetRange.y() - m_offsetRange.x() + 1;
        SinglePixelOffsets(tcu::mod(pixCoord.swizzle(1, 0), IVec2(offsetRangeSize)) + m_offsetRange.x())(IVec2(), dst);
    }

private:
    IVec2 m_offsetRange;
};

template <typename T>
static inline T triQuadInterpolate(const T (&values)[4], float xFactor, float yFactor)
{
    if (xFactor + yFactor < 1.0f)
        return values[0] + (values[2] - values[0]) * xFactor + (values[1] - values[0]) * yFactor;
    else
        return values[3] + (values[1] - values[3]) * (1.0f - xFactor) + (values[2] - values[3]) * (1.0f - yFactor);
}

template <int N>
static inline void computeTexCoordVecs(const vector<float> &texCoords, tcu::Vector<float, N> (&dst)[4])
{
    DE_ASSERT((int)texCoords.size() == 4 * N);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < N; j++)
            dst[i][j] = texCoords[i * N + j];
}

#if defined(DE_DEBUG)
// Whether offsets correspond to the sample offsets used with plain textureGather().
static inline bool isZeroOffsetOffsets(const IVec2 (&offsets)[4])
{
    IVec2 ref[4];
    SinglePixelOffsets(IVec2(0))(IVec2(), ref);
    return std::equal(DE_ARRAY_BEGIN(offsets), DE_ARRAY_END(offsets), DE_ARRAY_BEGIN(ref));
}
#endif

template <typename ColorScalarType>
static tcu::Vector<ColorScalarType, 4> gatherOffsets(const tcu::Texture2DView &texture, const tcu::Sampler &sampler,
                                                     const Vec2 &coord, int componentNdx, const IVec2 (&offsets)[4])
{
    return texture.gatherOffsets(sampler, coord.x(), coord.y(), componentNdx, offsets).cast<ColorScalarType>();
}

template <typename ColorScalarType>
static tcu::Vector<ColorScalarType, 4> gatherOffsets(const tcu::Texture2DArrayView &texture,
                                                     const tcu::Sampler &sampler, const Vec3 &coord, int componentNdx,
                                                     const IVec2 (&offsets)[4])
{
    return texture.gatherOffsets(sampler, coord.x(), coord.y(), coord.z(), componentNdx, offsets)
        .cast<ColorScalarType>();
}

template <typename ColorScalarType>
static tcu::Vector<ColorScalarType, 4> gatherOffsets(const tcu::TextureCubeView &texture, const tcu::Sampler &sampler,
                                                     const Vec3 &coord, int componentNdx, const IVec2 (&offsets)[4])
{
    DE_ASSERT(isZeroOffsetOffsets(offsets));
    DE_UNREF(offsets);
    return texture.gather(sampler, coord.x(), coord.y(), coord.z(), componentNdx).cast<ColorScalarType>();
}

static Vec4 gatherOffsetsCompare(const tcu::Texture2DView &texture, const tcu::Sampler &sampler, float refZ,
                                 const Vec2 &coord, const IVec2 (&offsets)[4])
{
    return texture.gatherOffsetsCompare(sampler, refZ, coord.x(), coord.y(), offsets);
}

static Vec4 gatherOffsetsCompare(const tcu::Texture2DArrayView &texture, const tcu::Sampler &sampler, float refZ,
                                 const Vec3 &coord, const IVec2 (&offsets)[4])
{
    return texture.gatherOffsetsCompare(sampler, refZ, coord.x(), coord.y(), coord.z(), offsets);
}

static Vec4 gatherOffsetsCompare(const tcu::TextureCubeView &texture, const tcu::Sampler &sampler, float refZ,
                                 const Vec3 &coord, const IVec2 (&offsets)[4])
{
    DE_ASSERT(isZeroOffsetOffsets(offsets));
    DE_UNREF(offsets);
    return texture.gatherCompare(sampler, refZ, coord.x(), coord.y(), coord.z());
}

template <typename PrecType, typename ColorScalarT>
static bool isGatherOffsetsResultValid(const tcu::TextureCubeView &texture, const tcu::Sampler &sampler,
                                       const PrecType &prec, const Vec3 &coord, int componentNdx,
                                       const IVec2 (&offsets)[4], const tcu::Vector<ColorScalarT, 4> &result)
{
    DE_ASSERT(isZeroOffsetOffsets(offsets));
    DE_UNREF(offsets);
    return tcu::isGatherResultValid(texture, sampler, prec, coord, componentNdx, result);
}

static bool isGatherOffsetsCompareResultValid(const tcu::TextureCubeView &texture, const tcu::Sampler &sampler,
                                              const tcu::TexComparePrecision &prec, const Vec3 &coord,
                                              const IVec2 (&offsets)[4], float cmpReference, const Vec4 &result)
{
    DE_ASSERT(isZeroOffsetOffsets(offsets));
    DE_UNREF(offsets);
    return tcu::isGatherCompareResultValid(texture, sampler, prec, coord, cmpReference, result);
}

template <typename ColorScalarType, typename PrecType, typename TexViewT, typename TexCoordT>
static bool verifyGatherOffsets(TestLog &log, const ConstPixelBufferAccess &result, const TexViewT &texture,
                                const TexCoordT (&texCoords)[4], const tcu::Sampler &sampler,
                                const PrecType &lookupPrec, int componentNdx, const PixelOffsets &getPixelOffsets)
{
    typedef tcu::Vector<ColorScalarType, 4> ColorVec;

    const int width  = result.getWidth();
    const int height = result.getWidth();
    tcu::TextureLevel ideal(result.getFormat(), width, height);
    const PixelBufferAccess idealAccess = ideal.getAccess();
    tcu::Surface errorMask(width, height);
    bool success = true;

    tcu::clear(errorMask.getAccess(), tcu::RGBA::green().toVec());

    for (int py = 0; py < height; py++)
        for (int px = 0; px < width; px++)
        {
            IVec2 offsets[4];
            getPixelOffsets(IVec2(px, py), offsets);

            const Vec2 viewportCoord = (Vec2((float)px, (float)py) + 0.5f) / Vec2((float)width, (float)height);
            const TexCoordT texCoord = triQuadInterpolate(texCoords, viewportCoord.x(), viewportCoord.y());
            const ColorVec resultPix = result.getPixelT<ColorScalarType>(px, py);
            const ColorVec idealPix = gatherOffsets<ColorScalarType>(texture, sampler, texCoord, componentNdx, offsets);

            idealAccess.setPixel(idealPix, px, py);

            if (tcu::boolAny(
                    tcu::logicalAnd(lookupPrec.colorMask,
                                    tcu::greaterThan(tcu::absDiff(resultPix, idealPix),
                                                     lookupPrec.colorThreshold.template cast<ColorScalarType>()))))
            {
                if (!isGatherOffsetsResultValid(texture, sampler, lookupPrec, texCoord, componentNdx, offsets,
                                                resultPix))
                {
                    errorMask.setPixel(px, py, tcu::RGBA::red());
                    success = false;
                }
            }
        }

    log << TestLog::ImageSet("VerifyResult", "Verification result")
        << TestLog::Image("Rendered", "Rendered image", result);

    if (!success)
    {
        log << TestLog::Image("Reference", "Ideal reference image", ideal)
            << TestLog::Image("ErrorMask", "Error mask", errorMask);
    }

    log << TestLog::EndImageSet;

    return success;
}

class PixelCompareRefZ
{
public:
    virtual ~PixelCompareRefZ()
    {
    }

    virtual float operator()(const IVec2 &pixCoord) const = 0;
};

class PixelCompareRefZDefault : public PixelCompareRefZ
{
public:
    PixelCompareRefZDefault(const IVec2 &renderSize) : m_renderSize(renderSize)
    {
    }

    float operator()(const IVec2 &pixCoord) const
    {
        return ((float)pixCoord.x() + 0.5f) / (float)m_renderSize.x();
    }

private:
    IVec2 m_renderSize;
};

template <typename TexViewT, typename TexCoordT>
static bool verifyGatherOffsetsCompare(TestLog &log, const ConstPixelBufferAccess &result, const TexViewT &texture,
                                       const TexCoordT (&texCoords)[4], const tcu::Sampler &sampler,
                                       const tcu::TexComparePrecision &compPrec, const PixelCompareRefZ &getPixelRefZ,
                                       const PixelOffsets &getPixelOffsets)
{
    const int width  = result.getWidth();
    const int height = result.getWidth();
    tcu::Surface ideal(width, height);
    const PixelBufferAccess idealAccess = ideal.getAccess();
    tcu::Surface errorMask(width, height);
    bool success = true;

    tcu::clear(errorMask.getAccess(), tcu::RGBA::green().toVec());

    for (int py = 0; py < height; py++)
        for (int px = 0; px < width; px++)
        {
            IVec2 offsets[4];
            getPixelOffsets(IVec2(px, py), offsets);

            const Vec2 viewportCoord = (Vec2((float)px, (float)py) + 0.5f) / Vec2((float)width, (float)height);
            const TexCoordT texCoord = triQuadInterpolate(texCoords, viewportCoord.x(), viewportCoord.y());
            const float refZ         = getPixelRefZ(IVec2(px, py));
            const Vec4 resultPix     = result.getPixel(px, py);
            const Vec4 idealPix      = gatherOffsetsCompare(texture, sampler, refZ, texCoord, offsets);

            idealAccess.setPixel(idealPix, px, py);

            if (!tcu::boolAll(tcu::equal(resultPix, idealPix)))
            {
                if (!isGatherOffsetsCompareResultValid(texture, sampler, compPrec, texCoord, offsets, refZ, resultPix))
                {
                    errorMask.setPixel(px, py, tcu::RGBA::red());
                    success = false;
                }
            }
        }

    log << TestLog::ImageSet("VerifyResult", "Verification result")
        << TestLog::Image("Rendered", "Rendered image", result);

    if (!success)
    {
        log << TestLog::Image("Reference", "Ideal reference image", ideal)
            << TestLog::Image("ErrorMask", "Error mask", errorMask);
    }

    log << TestLog::EndImageSet;

    return success;
}

enum GatherType
{
    GATHERTYPE_BASIC = 0,
    GATHERTYPE_OFFSET,
    GATHERTYPE_OFFSET_DYNAMIC,
    GATHERTYPE_OFFSETS,

    GATHERTYPE_LAST
};

enum GatherCaseFlags
{
    GATHERCASE_DONT_SAMPLE_CUBE_CORNERS = (1 << 0) //!< For cube map cases: do not sample cube corners
};

enum OffsetSize
{
    OFFSETSIZE_NONE = 0,
    OFFSETSIZE_MINIMUM_REQUIRED,
    OFFSETSIZE_IMPLEMENTATION_MAXIMUM,

    OFFSETSIZE_LAST
};

static inline const char *gatherTypeName(GatherType type)
{
    switch (type)
    {
    case GATHERTYPE_BASIC:
        return "basic";
    case GATHERTYPE_OFFSET:
        return "offset";
    case GATHERTYPE_OFFSET_DYNAMIC:
        return "offset_dynamic";
    case GATHERTYPE_OFFSETS:
        return "offsets";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static inline bool requireGpuShader5(GatherType gatherType, OffsetSize offsetSize)
{
    return gatherType == GATHERTYPE_OFFSET_DYNAMIC || gatherType == GATHERTYPE_OFFSETS ||
           offsetSize ==
               OFFSETSIZE_IMPLEMENTATION_MAXIMUM; // \note Implementation limits are not available while generating the shaders, they are passed dynamically at runtime
}

struct GatherArgs
{
    int componentNdx; // If negative, implicit component index 0 is used (i.e. the parameter is not given).
    IVec2 offsets
        [4]; // \note Unless GATHERTYPE_OFFSETS is used, only offsets[0] is relevant; also, for GATHERTYPE_OFFSET_DYNAMIC, none are relevant.

    GatherArgs(void) : componentNdx(-1)
    {
        std::fill(DE_ARRAY_BEGIN(offsets), DE_ARRAY_END(offsets), IVec2());
    }

    GatherArgs(int comp, const IVec2 &off0 = IVec2(), const IVec2 &off1 = IVec2(), const IVec2 &off2 = IVec2(),
               const IVec2 &off3 = IVec2())
        : componentNdx(comp)
    {
        offsets[0] = off0;
        offsets[1] = off1;
        offsets[2] = off2;
        offsets[3] = off3;
    }
};

static MovePtr<PixelOffsets> makePixelOffsetsFunctor(GatherType gatherType, const GatherArgs &gatherArgs,
                                                     const IVec2 &offsetRange)
{
    if (gatherType == GATHERTYPE_BASIC || gatherType == GATHERTYPE_OFFSET)
    {
        const IVec2 offset = gatherType == GATHERTYPE_BASIC ? IVec2(0) : gatherArgs.offsets[0];
        return MovePtr<PixelOffsets>(new SinglePixelOffsets(offset));
    }
    else if (gatherType == GATHERTYPE_OFFSET_DYNAMIC)
    {
        return MovePtr<PixelOffsets>(new DynamicSinglePixelOffsets(offsetRange));
    }
    else if (gatherType == GATHERTYPE_OFFSETS)
        return MovePtr<PixelOffsets>(new MultiplePixelOffsets(gatherArgs.offsets[0], gatherArgs.offsets[1],
                                                              gatherArgs.offsets[2], gatherArgs.offsets[3]));
    else
    {
        DE_ASSERT(false);
        return MovePtr<PixelOffsets>(nullptr);
    }
}

static inline glu::DataType getSamplerType(TextureType textureType, const tcu::TextureFormat &format)
{
    if (isDepthFormat(format))
    {
        switch (textureType)
        {
        case TEXTURETYPE_2D:
            return glu::TYPE_SAMPLER_2D_SHADOW;
        case TEXTURETYPE_2D_ARRAY:
            return glu::TYPE_SAMPLER_2D_ARRAY_SHADOW;
        case TEXTURETYPE_CUBE:
            return glu::TYPE_SAMPLER_CUBE_SHADOW;
        default:
            DE_ASSERT(false);
            return glu::TYPE_LAST;
        }
    }
    else
    {
        switch (textureType)
        {
        case TEXTURETYPE_2D:
            return glu::getSampler2DType(format);
        case TEXTURETYPE_2D_ARRAY:
            return glu::getSampler2DArrayType(format);
        case TEXTURETYPE_CUBE:
            return glu::getSamplerCubeType(format);
        default:
            DE_ASSERT(false);
            return glu::TYPE_LAST;
        }
    }
}

static inline glu::DataType getSamplerGatherResultType(glu::DataType samplerType)
{
    switch (samplerType)
    {
    case glu::TYPE_SAMPLER_2D_SHADOW:
    case glu::TYPE_SAMPLER_2D_ARRAY_SHADOW:
    case glu::TYPE_SAMPLER_CUBE_SHADOW:
    case glu::TYPE_SAMPLER_2D:
    case glu::TYPE_SAMPLER_2D_ARRAY:
    case glu::TYPE_SAMPLER_CUBE:
        return glu::TYPE_FLOAT_VEC4;

    case glu::TYPE_INT_SAMPLER_2D:
    case glu::TYPE_INT_SAMPLER_2D_ARRAY:
    case glu::TYPE_INT_SAMPLER_CUBE:
        return glu::TYPE_INT_VEC4;

    case glu::TYPE_UINT_SAMPLER_2D:
    case glu::TYPE_UINT_SAMPLER_2D_ARRAY:
    case glu::TYPE_UINT_SAMPLER_CUBE:
        return glu::TYPE_UINT_VEC4;

    default:
        DE_ASSERT(false);
        return glu::TYPE_LAST;
    }
}

static inline int getNumTextureSamplingDimensions(TextureType type)
{
    switch (type)
    {
    case TEXTURETYPE_2D:
        return 2;
    case TEXTURETYPE_2D_ARRAY:
        return 3;
    case TEXTURETYPE_CUBE:
        return 3;
    default:
        DE_ASSERT(false);
        return -1;
    }
}

enum class LevelMode
{
    NORMAL = 0,
    AMD_BIAS,
    AMD_LOD,
};

vector<GatherArgs> generateBasic2DCaseIterations(GatherType gatherType, OffsetSize offsetSize, LevelMode levelMode,
                                                 const tcu::TextureFormat &textureFormat, const IVec2 &offsetRange)
{
    const int numComponentCases =
        isDepthFormat(textureFormat) ?
            1 :
            4 + 1; // \note For non-depth textures, test explicit components 0 to 3 and implicit component 0.
    const bool skipImplicitCase = (levelMode == LevelMode::AMD_BIAS);
    vector<GatherArgs> result;

    for (int componentCaseNdx = (skipImplicitCase ? 1 : 0); componentCaseNdx < numComponentCases; componentCaseNdx++)
    {
        const int componentNdx = componentCaseNdx - 1;

        switch (gatherType)
        {
        case GATHERTYPE_BASIC:
            result.push_back(GatherArgs(componentNdx));
            break;

        case GATHERTYPE_OFFSET:
        {
            const int min  = offsetRange.x();
            const int max  = offsetRange.y();
            const int hmin = divRoundToZero(min, 2);
            const int hmax = divRoundToZero(max, 2);

            result.push_back(GatherArgs(componentNdx, IVec2(min, max)));

            if (componentCaseNdx ==
                0) // Don't test all offsets variants for all color components (they should be pretty orthogonal).
            {
                result.push_back(GatherArgs(componentNdx, IVec2(min, min)));
                result.push_back(GatherArgs(componentNdx, IVec2(max, min)));
                result.push_back(GatherArgs(componentNdx, IVec2(max, max)));

                result.push_back(GatherArgs(componentNdx, IVec2(0, hmax)));
                result.push_back(GatherArgs(componentNdx, IVec2(hmin, 0)));
                result.push_back(GatherArgs(componentNdx, IVec2(0, 0)));
            }

            break;
        }

        case GATHERTYPE_OFFSET_DYNAMIC:
            result.push_back(GatherArgs(componentNdx));
            break;

        case GATHERTYPE_OFFSETS:
        {
            // textureGatherOffsets requires parameters at compile time
            // Most implementations minimum is -32 and maximum is 31 so we will use those values
            // and verify them in checkSupport
            if (offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
            {
                result.push_back(GatherArgs(componentNdx,
                                            IVec2(IMPLEMENTATION_MIN_MIN_OFFSET, IMPLEMENTATION_MIN_MIN_OFFSET),
                                            IVec2(IMPLEMENTATION_MIN_MIN_OFFSET, IMPLEMENTATION_MAX_MAX_OFFSET),
                                            IVec2(IMPLEMENTATION_MAX_MAX_OFFSET, IMPLEMENTATION_MIN_MIN_OFFSET),
                                            IVec2(IMPLEMENTATION_MAX_MAX_OFFSET, IMPLEMENTATION_MAX_MAX_OFFSET)));
            }
            else
            {
                const int min  = offsetRange.x();
                const int max  = offsetRange.y();
                const int hmin = divRoundToZero(min, 2);
                const int hmax = divRoundToZero(max, 2);

                result.push_back(
                    GatherArgs(componentNdx, IVec2(min, min), IVec2(min, max), IVec2(max, min), IVec2(max, max)));

                if (componentCaseNdx ==
                    0) // Don't test all offsets variants for all color components (they should be pretty orthogonal).
                    result.push_back(
                        GatherArgs(componentNdx, IVec2(min, hmax), IVec2(hmin, max), IVec2(0, hmax), IVec2(hmax, 0)));
            }
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }

    return result;
}

struct GatherCaseBaseParams
{
    GatherType gatherType;
    OffsetSize offsetSize;
    tcu::TextureFormat textureFormat;
    tcu::Sampler::CompareMode shadowCompareMode;
    tcu::Sampler::WrapMode wrapS;
    tcu::Sampler::WrapMode wrapT;
    MaybeTextureSwizzle textureSwizzle;
    tcu::Sampler::FilterMode minFilter;
    tcu::Sampler::FilterMode magFilter;
    LevelMode levelMode;
    int baseLevel;
    uint32_t flags;
    TextureType textureType;
    ImageBackingMode sparseCase;

    GatherCaseBaseParams(const TextureType textureType_, const GatherType gatherType_, const OffsetSize offsetSize_,
                         const tcu::TextureFormat textureFormat_, const tcu::Sampler::CompareMode shadowCompareMode_,
                         const tcu::Sampler::WrapMode wrapS_, const tcu::Sampler::WrapMode wrapT_,
                         const MaybeTextureSwizzle &textureSwizzle_, const tcu::Sampler::FilterMode minFilter_,
                         const tcu::Sampler::FilterMode magFilter_, const LevelMode levelMode_, const int baseLevel_,
                         const uint32_t flags_, const ImageBackingMode sparseCase_)
        : gatherType(gatherType_)
        , offsetSize(offsetSize_)
        , textureFormat(textureFormat_)
        , shadowCompareMode(shadowCompareMode_)
        , wrapS(wrapS_)
        , wrapT(wrapT_)
        , textureSwizzle(textureSwizzle_)
        , minFilter(minFilter_)
        , magFilter(magFilter_)
        , levelMode(levelMode_)
        , baseLevel(baseLevel_)
        , flags(flags_)
        , textureType(textureType_)
        , sparseCase(sparseCase_)
    {
    }

    GatherCaseBaseParams(void)
        : gatherType(GATHERTYPE_LAST)
        , offsetSize(OFFSETSIZE_LAST)
        , textureFormat()
        , shadowCompareMode(tcu::Sampler::COMPAREMODE_LAST)
        , wrapS(tcu::Sampler::WRAPMODE_LAST)
        , wrapT(tcu::Sampler::WRAPMODE_LAST)
        , textureSwizzle(MaybeTextureSwizzle::createNoneTextureSwizzle())
        , minFilter(tcu::Sampler::FILTERMODE_LAST)
        , magFilter(tcu::Sampler::FILTERMODE_LAST)
        , levelMode(LevelMode::NORMAL)
        , baseLevel(0)
        , flags(0)
        , textureType(TEXTURETYPE_LAST)
        , sparseCase(ShaderRenderCaseInstance::IMAGE_BACKING_MODE_REGULAR)
    {
    }
};

static void checkMutableComparisonSamplersSupport(Context &context, const GatherCaseBaseParams &m_baseParams)
{
    // when compare mode is not none then ShaderRenderCaseInstance::createSamplerUniform
    // uses mapSampler utill from vkImageUtil that sets compareEnable to true
    // for portability this needs to be under feature flag
#ifndef CTS_USES_VULKANSC
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().mutableComparisonSamplers &&
        (m_baseParams.shadowCompareMode != tcu::Sampler::COMPAREMODE_NONE))
    {
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: mutableComparisonSamplers are not supported by this implementation");
    }
#else
    DE_UNREF(context);
    DE_UNREF(m_baseParams);
#endif // CTS_USES_VULKANSC
}

IVec2 getOffsetRange(const OffsetSize offsetSize, const vk::VkPhysicalDeviceLimits &deviceLimits)
{
    switch (offsetSize)
    {
    case OFFSETSIZE_NONE:
        return IVec2(0);

    case OFFSETSIZE_MINIMUM_REQUIRED:
        // \note Defined by spec.
        return IVec2(SPEC_MAX_MIN_OFFSET, SPEC_MIN_MAX_OFFSET);

    case OFFSETSIZE_IMPLEMENTATION_MAXIMUM:
        return IVec2(deviceLimits.minTexelGatherOffset, deviceLimits.maxTexelGatherOffset);

    default:
        DE_ASSERT(false);
        return IVec2(-1);
    }
}

IVec2 getOffsetRange(const OffsetSize offsetSize)
{
    switch (offsetSize)
    {
    case OFFSETSIZE_NONE:
        return IVec2(0);

    case OFFSETSIZE_MINIMUM_REQUIRED:
        // \note Defined by spec.
        return IVec2(SPEC_MAX_MIN_OFFSET, SPEC_MIN_MAX_OFFSET);

    case OFFSETSIZE_IMPLEMENTATION_MAXIMUM:
        DE_FATAL("Not known");
        return IVec2(-1);

    default:
        DE_ASSERT(false);
        return IVec2(-1);
    }
}

class TextureGatherInstance : public ShaderRenderCaseInstance
{
public:
    TextureGatherInstance(Context &context, const GatherCaseBaseParams &baseParams);
    virtual ~TextureGatherInstance(void);

    virtual tcu::TestStatus iterate(void);

protected:
    void init(void);

    virtual int getNumIterations(void) const                 = 0;
    virtual GatherArgs getGatherArgs(int iterationNdx) const = 0;

    virtual void setupDefaultInputs(void);
    virtual void setupUniforms(const tcu::Vec4 &);

    template <typename TexViewT, typename TexCoordT>
    bool verify(const ConstPixelBufferAccess &rendered, const TexViewT &texture, const TexCoordT (&bottomLeft)[4],
                const GatherArgs &gatherArgs) const;

    virtual TextureBindingSp createTexture(void)                                        = 0;
    virtual vector<float> computeQuadTexCoord(int iterationNdx) const                   = 0;
    virtual bool verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const = 0;

protected:
    static const IVec2 RENDER_SIZE;

    const GatherCaseBaseParams m_baseParams;

private:
    const tcu::TextureFormat m_colorBufferFormat;
    int m_currentIteration;
};

const IVec2 TextureGatherInstance::RENDER_SIZE = IVec2(64, 64);

TextureGatherInstance::TextureGatherInstance(Context &context, const GatherCaseBaseParams &baseParams)
    : ShaderRenderCaseInstance(context, false, nullptr, nullptr, nullptr, baseParams.sparseCase)
    , m_baseParams(baseParams)
    , m_colorBufferFormat(tcu::TextureFormat(tcu::TextureFormat::RGBA, isDepthFormat(baseParams.textureFormat) ?
                                                                           tcu::TextureFormat::UNORM_INT8 :
                                                                           baseParams.textureFormat.type))
    , m_currentIteration(0)
{
    DE_ASSERT((m_baseParams.gatherType == GATHERTYPE_BASIC) == (m_baseParams.offsetSize == OFFSETSIZE_NONE));
    DE_ASSERT((m_baseParams.shadowCompareMode != tcu::Sampler::COMPAREMODE_NONE) ==
              isDepthFormat(m_baseParams.textureFormat));
    DE_ASSERT(isUnormFormatType(m_colorBufferFormat.type) ||
              m_colorBufferFormat.type == tcu::TextureFormat::UNSIGNED_INT8 ||
              m_colorBufferFormat.type == tcu::TextureFormat::UNSIGNED_INT16 ||
              m_colorBufferFormat.type == tcu::TextureFormat::SIGNED_INT8 ||
              m_colorBufferFormat.type == tcu::TextureFormat::SIGNED_INT16);
    DE_ASSERT(glu::isGLInternalColorFormatFilterable(glu::getInternalFormat(m_colorBufferFormat)) ||
              (m_baseParams.magFilter == tcu::Sampler::NEAREST &&
               (m_baseParams.minFilter == tcu::Sampler::NEAREST ||
                m_baseParams.minFilter == tcu::Sampler::NEAREST_MIPMAP_NEAREST)));
    DE_ASSERT(m_baseParams.textureType == TEXTURETYPE_CUBE ||
              !(m_baseParams.flags & GATHERCASE_DONT_SAMPLE_CUBE_CORNERS));

    m_renderSize  = RENDER_SIZE.asUint();
    m_colorFormat = vk::mapTextureFormat(m_colorBufferFormat);

#ifdef CTS_USES_VULKANSC
    const VkDevice vkDevice         = getDevice();
    const DeviceInterface &vk       = getDeviceInterface();
    const uint32_t queueFamilyIndex = getUniversalQueueFamilyIndex();
    m_externalCommandPool           = de::SharedPtr<Unique<VkCommandPool>>(new vk::Unique<VkCommandPool>(
        createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex)));
#endif // CTS_USES_VULKANSC
}

TextureGatherInstance::~TextureGatherInstance(void)
{
}

void TextureGatherInstance::init(void)
{
    TestLog &log = m_context.getTestContext().getLog();
    TextureBindingSp textureBinding;
    TextureBinding::Parameters textureParams;

    // Check prerequisites.
    if (requireGpuShader5(m_baseParams.gatherType, m_baseParams.offsetSize))
    {
        const vk::VkPhysicalDeviceFeatures &deviceFeatures = m_context.getDeviceFeatures();
        if (!deviceFeatures.shaderImageGatherExtended)
            TCU_THROW(NotSupportedError, "Extended set of image gather instructions are not supported");
    }

    // Check general extension support.
    if (m_baseParams.levelMode != LevelMode::NORMAL)
    {
        m_context.requireDeviceFunctionality("VK_AMD_texture_gather_bias_lod");
    }

    // Log and check implementation offset limits, if appropriate.
    if (m_baseParams.offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
    {
        const IVec2 offsetRange = getOffsetRange(m_baseParams.offsetSize, m_context.getDeviceProperties().limits);
        log << TestLog::Integer("ImplementationMinTextureGatherOffset",
                                "Implementation's value for minTexelGatherOffset", "", QP_KEY_TAG_NONE, offsetRange[0])
            << TestLog::Integer("ImplementationMaxTextureGatherOffset",
                                "Implementation's value for maxTexelGatherOffset", "", QP_KEY_TAG_NONE, offsetRange[1]);
        TCU_CHECK_MSG(offsetRange[0] <= SPEC_MAX_MIN_OFFSET,
                      ("minTexelGatherOffset must be at most " + de::toString((int)SPEC_MAX_MIN_OFFSET)).c_str());
        TCU_CHECK_MSG(offsetRange[1] >= SPEC_MIN_MAX_OFFSET,
                      ("maxTexelGatherOffset must be at least " + de::toString((int)SPEC_MIN_MAX_OFFSET)).c_str());
    }

    // Initialize texture.
    textureBinding = createTexture();

    // Check image format support.
    // This should happen earlier but it's easier to retrieve texture parameters once created and this is not expected to fail.
#ifndef CTS_USES_VULKANSC
    if (m_baseParams.levelMode != LevelMode::NORMAL)
    {
        const auto format                             = vk::mapTextureFormat(m_baseParams.textureFormat);
        const auto bindingType                        = textureBinding->getType();
        const auto imageViewType                      = textureTypeToImageViewType(bindingType);
        const auto imageType                          = viewTypeToImageType(imageViewType);
        const vk::VkImageUsageFlags usageFlags        = textureUsageFlags();
        const vk::VkImageCreateFlags imageCreateFlags = textureCreateFlags(imageViewType, m_baseParams.sparseCase);

        const vk::VkPhysicalDeviceImageFormatInfo2 formatInfo = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            format,                                                    // VkFormat format;
            imageType,                                                 // VkImageType type;
            vk::VK_IMAGE_TILING_OPTIMAL,                               // VkImageTiling tiling;
            usageFlags,                                                // VkImageUsageFlags usage;
            imageCreateFlags,                                          // VkImageCreateFlags flags;
        };

        vk::VkTextureLODGatherFormatPropertiesAMD lodGatherProperties = {
            vk::VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD, // VkStructureType sType;
            nullptr,                                                        // void* pNext;
            VK_FALSE,                                                       // VkBool32 supportsTextureGatherLODBiasAMD;
        };

        vk::VkImageFormatProperties2 properties2 = vk::initVulkanStructure();
        properties2.pNext                        = &lodGatherProperties;

        const auto retCode = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(
            m_context.getPhysicalDevice(), &formatInfo, &properties2);

        if (retCode != vk::VK_SUCCESS && retCode != vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties2 returned " + de::toString(retCode));

        if (retCode == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format does not support the required parameters");

        if (!lodGatherProperties.supportsTextureGatherLODBiasAMD)
            TCU_THROW(NotSupportedError, "Format does not support texture gather LOD/Bias operations");
    }
#endif

    if (m_baseParams.textureSwizzle.isSome())
    {
        const tcu::Vector<TextureSwizzleComponent, 4> &swizzle = m_baseParams.textureSwizzle.getSwizzle();

        const vk::VkComponentMapping components = {
            getTextureSwizzleComponent(swizzle[0]), getTextureSwizzleComponent(swizzle[1]),
            getTextureSwizzleComponent(swizzle[2]), getTextureSwizzleComponent(swizzle[3])};

        textureParams.componentMapping = components;
    }

    // Set base mip level and mode.
    if (m_baseParams.levelMode == LevelMode::NORMAL)
    {
        textureParams.baseMipLevel = m_baseParams.baseLevel;
    }
    else
    {
        const auto textureType = textureBinding->getType();
        int levels             = 0;

        switch (textureType)
        {
        case TextureBinding::TYPE_1D:
            levels = textureBinding->get1D().getNumLevels();
            break;
        case TextureBinding::TYPE_2D:
            levels = textureBinding->get2D().getNumLevels();
            break;
        case TextureBinding::TYPE_3D:
            levels = textureBinding->get3D().getNumLevels();
            break;
        case TextureBinding::TYPE_CUBE_MAP:
            levels = textureBinding->getCube().getNumLevels();
            break;
        case TextureBinding::TYPE_1D_ARRAY:
            levels = textureBinding->get1DArray().getNumLevels();
            break;
        case TextureBinding::TYPE_2D_ARRAY:
            levels = textureBinding->get2DArray().getNumLevels();
            break;
        case TextureBinding::TYPE_CUBE_ARRAY:
            levels = textureBinding->getCubeArray().getNumLevels();
            break;
        default:
            DE_ASSERT(false);
            break;
        }

        DE_ASSERT(levels > 0);
        textureParams.minMaxLod = tcu::just(TextureBinding::MinMaxLod(0.0f, static_cast<float>(levels - 1)));
    }

    textureBinding->setParameters(textureParams);
    m_textures.push_back(textureBinding);

    log << TestLog::Message << "Texture base level is " << textureParams.baseMipLevel << TestLog::EndMessage
        << TestLog::Message << "s and t wrap modes are " << vk::mapWrapMode(m_baseParams.wrapS) << " and "
        << vk::mapWrapMode(m_baseParams.wrapT) << ", respectively" << TestLog::EndMessage << TestLog::Message
        << "Minification and magnification filter modes are " << vk::mapFilterMode(m_baseParams.minFilter) << " and "
        << vk::mapFilterMode(m_baseParams.magFilter) << ", respectively "
        << "(note that they should have no effect on gather result)" << TestLog::EndMessage << TestLog::Message
        << "Using texture swizzle " << m_baseParams.textureSwizzle << TestLog::EndMessage;

    if (m_baseParams.shadowCompareMode != tcu::Sampler::COMPAREMODE_NONE)
        log << TestLog::Message << "Using texture compare func " << vk::mapCompareMode(m_baseParams.shadowCompareMode)
            << TestLog::EndMessage;
}

void TextureGatherInstance::setupDefaultInputs(void)
{
    const int numVertices       = 4;
    const float position[4 * 2] = {
        -1.0f, -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f,
    };
    const float normalizedCoord[4 * 2] = {
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    };
    const vector<float> texCoord = computeQuadTexCoord(m_currentIteration);
    const bool needNormalizedCoordInShader =
        m_baseParams.gatherType == GATHERTYPE_OFFSET_DYNAMIC || isDepthFormat(m_baseParams.textureFormat);

    addAttribute(0u, vk::VK_FORMAT_R32G32_SFLOAT, 2 * (uint32_t)sizeof(float), numVertices, position);

    if (texCoord.size() == 2 * 4)
        addAttribute(1u, vk::VK_FORMAT_R32G32_SFLOAT, 2 * (uint32_t)sizeof(float), numVertices, texCoord.data());
    else if (texCoord.size() == 3 * 4)
        addAttribute(1u, vk::VK_FORMAT_R32G32B32_SFLOAT, 3 * (uint32_t)sizeof(float), numVertices, texCoord.data());
    else
        DE_ASSERT(false);

    if (needNormalizedCoordInShader)
        addAttribute(2u, vk::VK_FORMAT_R32G32_SFLOAT, 2 * (uint32_t)sizeof(float), numVertices, normalizedCoord);
}

tcu::TestStatus TextureGatherInstance::iterate(void)
{
    TestLog &log = m_context.getTestContext().getLog();
    const tcu::ScopedLogSection iterationSection(log, "Iteration" + de::toString(m_currentIteration),
                                                 "Iteration " + de::toString(m_currentIteration));

    // Render.

    {
        const uint32_t numVertices   = 4;
        const uint32_t numTriangles  = 2;
        const uint16_t indices[6]    = {0, 1, 2, 2, 1, 3};
        const vector<float> texCoord = computeQuadTexCoord(m_currentIteration);

        if (texCoord.size() == 2 * 4)
        {
            Vec2 texCoordVec[4];
            computeTexCoordVecs(texCoord, texCoordVec);
            log << TestLog::Message << "Texture coordinates run from " << texCoordVec[0] << " to " << texCoordVec[3]
                << TestLog::EndMessage;
        }
        else if (texCoord.size() == 3 * 4)
        {
            Vec3 texCoordVec[4];
            computeTexCoordVecs(texCoord, texCoordVec);
            log << TestLog::Message << "Texture coordinates run from " << texCoordVec[0] << " to " << texCoordVec[3]
                << TestLog::EndMessage;
        }
        else
            DE_ASSERT(false);

        m_vertexShaderName   = "vert";
        m_fragmentShaderName = "frag_" + de::toString(m_currentIteration);

        setup();

        render(numVertices, numTriangles, indices);
    }

    // Verify result.
    bool result = verify(m_currentIteration, getResultImage().getAccess());
#ifdef CTS_USES_VULKANSC
    if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
    {
        if (!result)
            return tcu::TestStatus::fail("Result verification failed");
    }

    m_currentIteration++;
    if (m_currentIteration == getNumIterations())
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::incomplete();
}

void TextureGatherInstance::setupUniforms(const tcu::Vec4 &)
{
    uint32_t binding = 0;

    useSampler(binding++, 0u);

    if (m_baseParams.gatherType == GATHERTYPE_OFFSET_DYNAMIC)
        addUniform(binding++, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(tcu::Vec2), RENDER_SIZE.asFloat().getPtr());

    if (m_baseParams.offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
    {
        if (m_baseParams.gatherType == GATHERTYPE_OFFSET)
        {
            const GatherArgs &gatherArgs = getGatherArgs(m_currentIteration);
            addUniform(binding++, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(tcu::IVec2),
                       gatherArgs.offsets[0].getPtr());
        }
        else if (m_baseParams.gatherType == GATHERTYPE_OFFSET_DYNAMIC)
        {
            const IVec2 &offsetRange = getOffsetRange(m_baseParams.offsetSize, m_context.getDeviceProperties().limits);
            addUniform(binding++, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(tcu::IVec2), offsetRange.getPtr());
        }
        else if (m_baseParams.gatherType != GATHERTYPE_OFFSETS)
            DE_ASSERT(false);
    }
}

template <typename TexViewT, typename TexCoordT>
bool TextureGatherInstance::verify(const ConstPixelBufferAccess &rendered, const TexViewT &texture,
                                   const TexCoordT (&texCoords)[4], const GatherArgs &gatherArgs) const
{
    TestLog &log = m_context.getTestContext().getLog();

    {
        DE_ASSERT(m_colorBufferFormat.order == tcu::TextureFormat::RGBA);
        DE_ASSERT(m_colorBufferFormat.type == tcu::TextureFormat::UNORM_INT8 ||
                  m_colorBufferFormat.type == tcu::TextureFormat::UNSIGNED_INT8 ||
                  m_colorBufferFormat.type == tcu::TextureFormat::SIGNED_INT8);

        const MovePtr<PixelOffsets> pixelOffsets =
            makePixelOffsetsFunctor(m_baseParams.gatherType, gatherArgs,
                                    getOffsetRange(m_baseParams.offsetSize, m_context.getDeviceProperties().limits));
        const tcu::PixelFormat pixelFormat = tcu::PixelFormat(8, 8, 8, 8);
        const IVec4 colorBits              = tcu::max(TextureTestUtil::getBitsVec(pixelFormat) - 1, tcu::IVec4(0));
        const IVec3 coordBits              = m_baseParams.textureType == TEXTURETYPE_2D       ? IVec3(20, 20, 0) :
                                             m_baseParams.textureType == TEXTURETYPE_CUBE     ? IVec3(10, 10, 10) :
                                             m_baseParams.textureType == TEXTURETYPE_2D_ARRAY ? IVec3(20, 20, 20) :
                                                                                                IVec3(-1);
        const IVec3 uvwBits                = m_baseParams.textureType == TEXTURETYPE_2D       ? IVec3(7, 7, 0) :
                                             m_baseParams.textureType == TEXTURETYPE_CUBE     ? IVec3(6, 6, 0) :
                                             m_baseParams.textureType == TEXTURETYPE_2D_ARRAY ? IVec3(7, 7, 7) :
                                                                                                IVec3(-1);
        tcu::Sampler sampler;
        sampler.wrapS           = m_baseParams.wrapS;
        sampler.wrapT           = m_baseParams.wrapT;
        sampler.compare         = m_baseParams.shadowCompareMode;
        sampler.seamlessCubeMap = true;

        if (isDepthFormat(m_baseParams.textureFormat))
        {
            tcu::TexComparePrecision comparePrec;
            comparePrec.coordBits     = coordBits;
            comparePrec.uvwBits       = uvwBits;
            comparePrec.referenceBits = 16;
            comparePrec.resultBits    = pixelFormat.redBits - 1;

            return verifyGatherOffsetsCompare(log, rendered, texture, texCoords, sampler, comparePrec,
                                              PixelCompareRefZDefault(RENDER_SIZE), *pixelOffsets);
        }
        else
        {
            const int componentNdx = de::max(0, gatherArgs.componentNdx);

            if (isUnormFormatType(m_baseParams.textureFormat.type))
            {
                tcu::LookupPrecision lookupPrec;
                lookupPrec.colorThreshold = tcu::computeFixedPointThreshold(colorBits);
                lookupPrec.coordBits      = coordBits;
                lookupPrec.uvwBits        = uvwBits;
                lookupPrec.colorMask      = TextureTestUtil::getCompareMask(pixelFormat);
                return verifyGatherOffsets<float>(log, rendered, texture, texCoords, sampler, lookupPrec, componentNdx,
                                                  *pixelOffsets);
            }
            else if (isUIntFormatType(m_baseParams.textureFormat.type) ||
                     isSIntFormatType(m_baseParams.textureFormat.type))
            {
                tcu::IntLookupPrecision lookupPrec;
                lookupPrec.colorThreshold = UVec4(0);
                lookupPrec.coordBits      = coordBits;
                lookupPrec.uvwBits        = uvwBits;
                lookupPrec.colorMask      = TextureTestUtil::getCompareMask(pixelFormat);

                if (isUIntFormatType(m_baseParams.textureFormat.type))
                    return verifyGatherOffsets<uint32_t>(log, rendered, texture, texCoords, sampler, lookupPrec,
                                                         componentNdx, *pixelOffsets);
                else if (isSIntFormatType(m_baseParams.textureFormat.type))
                    return verifyGatherOffsets<int32_t>(log, rendered, texture, texCoords, sampler, lookupPrec,
                                                        componentNdx, *pixelOffsets);
                else
                {
                    DE_ASSERT(false);
                    return false;
                }
            }
            else
            {
                DE_ASSERT(false);
                return false;
            }
        }
    }
}

glu::VertexSource genVertexShaderSource(bool requireGpuShader5, int numTexCoordComponents, bool useNormalizedCoordInput)
{
    DE_ASSERT(numTexCoordComponents == 2 || numTexCoordComponents == 3);

    const string texCoordType = "vec" + de::toString(numTexCoordComponents);
    std::ostringstream vert;

    vert << "#version 310 es\n";

    if (requireGpuShader5)
        vert << "#extension GL_EXT_gpu_shader5 : require\n";

    vert << "\n"
            "layout (location = 0) in highp vec2 a_position;\n"
            "layout (location = 1) in highp "
         << texCoordType << " a_texCoord;\n";

    if (useNormalizedCoordInput)
        vert << "layout (location = 2) in highp vec2 a_normalizedCoord; // (0,0) to (1,1)\n";

    vert << "\n"
            "layout (location = 0) out highp "
         << texCoordType << " v_texCoord;\n";

    if (useNormalizedCoordInput)
        vert << "layout (location = 1) out highp vec2 v_normalizedCoord;\n";

    vert << "\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\n"
            "    v_texCoord = a_texCoord;\n";

    if (useNormalizedCoordInput)
        vert << "    v_normalizedCoord = a_normalizedCoord;\n";

    vert << "}\n";

    return glu::VertexSource(vert.str());
}

glu::FragmentSource genFragmentShaderSource(bool requireGpuShader5, int numTexCoordComponents,
                                            glu::DataType samplerType, const string &funcCall,
                                            bool useNormalizedCoordInput, bool usePixCoord, OffsetSize offsetSize,
                                            const ImageBackingMode sparseCase, LevelMode levelMode)
{
    DE_ASSERT(glu::isDataTypeSampler(samplerType));
    DE_ASSERT(de::inRange(numTexCoordComponents, 2, 3));
    DE_ASSERT(!usePixCoord || useNormalizedCoordInput);

    const string texCoordType = "vec" + de::toString(numTexCoordComponents);
    uint32_t binding          = 0;
    std::ostringstream frag;
    const string outType = glu::getDataTypeName(getSamplerGatherResultType(samplerType));

    frag << "#version 450\n";

    if (sparseCase == ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE)
        frag << "#extension GL_ARB_sparse_texture2 : require\n";

    if (levelMode != LevelMode::NORMAL)
        frag << "#extension GL_AMD_texture_gather_bias_lod : require\n";

    if (requireGpuShader5)
        frag << "#extension GL_EXT_gpu_shader5 : require\n";

    frag << "\n"
            "layout (location = 0) out mediump "
         << outType
         << " o_color;\n"
            "\n"
            "layout (location = 0) in highp "
         << texCoordType << " v_texCoord;\n";

    if (useNormalizedCoordInput)
        frag << "layout (location = 1) in highp vec2 v_normalizedCoord;\n";

    frag << "\n"
            "layout (binding = "
         << binding++ << ") uniform highp " << glu::getDataTypeName(samplerType) << " u_sampler;\n";

    if (usePixCoord)
        frag << "layout (binding = " << binding++ << ") uniform viewportSize { highp vec2 u_viewportSize; };\n";

    if (offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
        frag << "layout (binding = " << binding++ << ") uniform offset { highp ivec2 u_offset; };\n";

    frag << "\n"
            "void main(void)\n"
            "{\n";

    if (usePixCoord)
        frag << "    ivec2 pixCoord = ivec2(v_normalizedCoord*u_viewportSize);\n";

    if (sparseCase == ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE)
    {
        // Texel declaration
        frag << "\t" << outType << " texel;\n";
        frag << "\tint success = " << funcCall << ";\n";

        // Check sparse validity, and handle each case
        frag << "\tif (sparseTexelsResidentARB(success))\n"
             << "\t\to_color = texel;\n"
             << "\telse\n"
             << "\t\to_color = " << outType << "(0.0, 0.0, 0.0, 1.0);\n";
    }
    else
    {
        frag << "\t\to_color = " << funcCall << ";\n";
    }

    frag << "}\n";

    return glu::FragmentSource(frag.str());
}

string genGatherFuncCall(GatherType gatherType, const tcu::TextureFormat &textureFormat, const GatherArgs &gatherArgs,
                         LevelMode levelMode, uint32_t baseLevel, const string &refZExpr, const IVec2 &offsetRange,
                         int indentationDepth, OffsetSize offsetSize, const ImageBackingMode sparseCase)
{
    string result;
    string levelStr;

    if (levelMode != LevelMode::NORMAL)
    {
        levelStr = de::toString(baseLevel) + ".0";
    }

    if (sparseCase == ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE)
    {
        if (levelMode == LevelMode::NORMAL || levelMode == LevelMode::AMD_BIAS)
        {
            switch (gatherType)
            {
            case GATHERTYPE_BASIC:
                result += "sparseTextureGatherARB";
                break;
            case GATHERTYPE_OFFSET: // \note Fallthrough.
            case GATHERTYPE_OFFSET_DYNAMIC:
                result += "sparseTextureGatherOffsetARB";
                break;
            case GATHERTYPE_OFFSETS:
                result += "sparseTextureGatherOffsetsARB";
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else // LevelMode::AMD_LOD
        {
            switch (gatherType)
            {
            case GATHERTYPE_BASIC:
                result += "sparseTextureGatherLodAMD";
                break;
            case GATHERTYPE_OFFSET: // \note Fallthrough.
            case GATHERTYPE_OFFSET_DYNAMIC:
                result += "sparseTextureGatherLodOffsetAMD";
                break;
            case GATHERTYPE_OFFSETS:
                result += "sparseTextureGatherLodOffsetsAMD";
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }
    else
    {
        if (levelMode == LevelMode::NORMAL || levelMode == LevelMode::AMD_BIAS)
        {
            switch (gatherType)
            {
            case GATHERTYPE_BASIC:
                result += "textureGather";
                break;
            case GATHERTYPE_OFFSET: // \note Fallthrough.
            case GATHERTYPE_OFFSET_DYNAMIC:
                result += "textureGatherOffset";
                break;
            case GATHERTYPE_OFFSETS:
                result += "textureGatherOffsets";
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else // LevelMode::AMD_LOD
        {
            switch (gatherType)
            {
            case GATHERTYPE_BASIC:
                result += "textureGatherLodAMD";
                break;
            case GATHERTYPE_OFFSET: // \note Fallthrough.
            case GATHERTYPE_OFFSET_DYNAMIC:
                result += "textureGatherLodOffsetAMD";
                break;
            case GATHERTYPE_OFFSETS:
                result += "textureGatherLodOffsetsAMD";
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }

    result += "(u_sampler, v_texCoord";

    if (isDepthFormat(textureFormat))
    {
        DE_ASSERT(gatherArgs.componentNdx < 0);
        result += ", " + refZExpr;
    }

    if (levelMode == LevelMode::AMD_LOD)
    {
        result += ", " + levelStr;
    }

    if (gatherType == GATHERTYPE_OFFSET || gatherType == GATHERTYPE_OFFSET_DYNAMIC || gatherType == GATHERTYPE_OFFSETS)
    {
        result += ", ";
        switch (gatherType)
        {
        case GATHERTYPE_OFFSET:
            if (offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
                result += "u_offset";
            else
                result += "ivec2" + de::toString(gatherArgs.offsets[0]);
            break;

        case GATHERTYPE_OFFSET_DYNAMIC:
            if (offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
                result += "pixCoord.yx % ivec2(u_offset.y - u_offset.x + 1) + u_offset.x";
            else
                result += "pixCoord.yx % ivec2(" + de::toString(offsetRange.y() - offsetRange.x() + 1) + ") + " +
                          de::toString(offsetRange.x());
            break;

        case GATHERTYPE_OFFSETS:
            if (offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
            {
                // textureGatherOffsets requires parameters at compile time
                // Most implementations minimum is -32 and maximum is 31 so we will use those values
                // and verify them in checkSupport
                result += "ivec2[4](\n" + string(indentationDepth, '\t') + "\tivec2(" +
                          de::toString(IMPLEMENTATION_MIN_MIN_OFFSET) + ", " +
                          de::toString(IMPLEMENTATION_MIN_MIN_OFFSET) + "),\n" + string(indentationDepth, '\t') +
                          "\tivec2(" + de::toString(IMPLEMENTATION_MIN_MIN_OFFSET) + ", " +
                          de::toString(IMPLEMENTATION_MAX_MAX_OFFSET) + "),\n" + string(indentationDepth, '\t') +
                          "\tivec2(" + de::toString(IMPLEMENTATION_MAX_MAX_OFFSET) + ", " +
                          de::toString(IMPLEMENTATION_MIN_MIN_OFFSET) + "),\n" + string(indentationDepth, '\t') +
                          "\tivec2(" + de::toString(IMPLEMENTATION_MAX_MAX_OFFSET) + ", " +
                          de::toString(IMPLEMENTATION_MAX_MAX_OFFSET) + "))\n" + string(indentationDepth, '\t') + "\t";
            }
            else
            {
                result += "ivec2[4](\n" + string(indentationDepth, '\t') + "\tivec2" +
                          de::toString(gatherArgs.offsets[0]) + ",\n" + string(indentationDepth, '\t') + "\tivec2" +
                          de::toString(gatherArgs.offsets[1]) + ",\n" + string(indentationDepth, '\t') + "\tivec2" +
                          de::toString(gatherArgs.offsets[2]) + ",\n" + string(indentationDepth, '\t') + "\tivec2" +
                          de::toString(gatherArgs.offsets[3]) + ")\n" + string(indentationDepth, '\t') + "\t";
            }
            break;

        default:
            DE_ASSERT(false);
        }
    }

    if (sparseCase == ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE)
        result += ", texel";

    if (gatherArgs.componentNdx >= 0)
    {
        DE_ASSERT(gatherArgs.componentNdx < 4);
        result += ", " + de::toString(gatherArgs.componentNdx);
    }

    if (levelMode == LevelMode::AMD_BIAS)
    {
        result += ", " + levelStr;
    }

    result += ")";

    return result;
}

// \todo [2016-07-08 pyry] Re-use programs if sources are identical

void genGatherPrograms(vk::SourceCollections &programCollection, const GatherCaseBaseParams &baseParams,
                       const vector<GatherArgs> &iterations)
{
    const int numIterations = (int)iterations.size();
    const string refZExpr   = "v_normalizedCoord.x";
    const IVec2 &offsetRange =
        baseParams.offsetSize != OFFSETSIZE_IMPLEMENTATION_MAXIMUM ? getOffsetRange(baseParams.offsetSize) : IVec2(0);
    const bool usePixCoord          = baseParams.gatherType == GATHERTYPE_OFFSET_DYNAMIC;
    const bool useNormalizedCoord   = usePixCoord || isDepthFormat(baseParams.textureFormat);
    const bool isDynamicOffset      = baseParams.gatherType == GATHERTYPE_OFFSET_DYNAMIC;
    const bool isShadow             = isDepthFormat(baseParams.textureFormat);
    const glu::DataType samplerType = getSamplerType(baseParams.textureType, baseParams.textureFormat);
    const int numDims               = getNumTextureSamplingDimensions(baseParams.textureType);
    glu::VertexSource vert = genVertexShaderSource(requireGpuShader5(baseParams.gatherType, baseParams.offsetSize),
                                                   numDims, isDynamicOffset || isShadow);

    // Check sampler type is valid.
    if (baseParams.levelMode != LevelMode::NORMAL)
    {
        std::vector<glu::DataType> validSamplerTypes = {
            glu::TYPE_SAMPLER_2D,           glu::TYPE_SAMPLER_2D_ARRAY, glu::TYPE_INT_SAMPLER_2D,
            glu::TYPE_INT_SAMPLER_2D_ARRAY, glu::TYPE_UINT_SAMPLER_2D,  glu::TYPE_UINT_SAMPLER_2D_ARRAY,
        };

        if (baseParams.gatherType == GATHERTYPE_BASIC)
        {
            static const std::vector<glu::DataType> kAdditionalTypes = {
                glu::TYPE_SAMPLER_CUBE,           glu::TYPE_SAMPLER_CUBE_ARRAY, glu::TYPE_INT_SAMPLER_CUBE,
                glu::TYPE_INT_SAMPLER_CUBE_ARRAY, glu::TYPE_UINT_SAMPLER_CUBE,  glu::TYPE_UINT_SAMPLER_CUBE_ARRAY,
            };

            std::copy(begin(kAdditionalTypes), end(kAdditionalTypes), std::back_inserter(validSamplerTypes));
        }

        const auto itr = std::find(begin(validSamplerTypes), end(validSamplerTypes), samplerType);
        DE_ASSERT(itr != end(validSamplerTypes));
        DE_UNREF(itr); // For release builds.
    }

    programCollection.glslSources.add("vert") << vert;

    for (int iterNdx = 0; iterNdx < numIterations; iterNdx++)
    {
        const GatherArgs &gatherArgs = iterations[iterNdx];
        const string funcCall        = genGatherFuncCall(baseParams.gatherType, baseParams.textureFormat, gatherArgs,
                                                         baseParams.levelMode, baseParams.baseLevel, refZExpr, offsetRange, 1,
                                                         baseParams.offsetSize, baseParams.sparseCase);
        glu::FragmentSource frag     = genFragmentShaderSource(
            requireGpuShader5(baseParams.gatherType, baseParams.offsetSize), numDims, samplerType, funcCall,
            useNormalizedCoord, usePixCoord, baseParams.offsetSize, baseParams.sparseCase, baseParams.levelMode);

        programCollection.glslSources.add("frag_" + de::toString(iterNdx)) << frag;
    }
}

// 2D

class TextureGather2DInstance : public TextureGatherInstance
{
public:
    TextureGather2DInstance(Context &context, const GatherCaseBaseParams &baseParams, const IVec2 &textureSize,
                            const vector<GatherArgs> &iterations);
    virtual ~TextureGather2DInstance(void);

protected:
    virtual int getNumIterations(void) const
    {
        return (int)m_iterations.size();
    }
    virtual GatherArgs getGatherArgs(int iterationNdx) const
    {
        return m_iterations[iterationNdx];
    }

    virtual TextureBindingSp createTexture(void);
    virtual vector<float> computeQuadTexCoord(int iterationNdx) const;
    virtual bool verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const;

private:
    const IVec2 m_textureSize;
    const vector<GatherArgs> m_iterations;

    tcu::Texture2D m_swizzledTexture;
};

TextureGather2DInstance::TextureGather2DInstance(Context &context, const GatherCaseBaseParams &baseParams,
                                                 const IVec2 &textureSize, const vector<GatherArgs> &iterations)
    : TextureGatherInstance(context, baseParams)
    , m_textureSize(textureSize)
    , m_iterations(iterations)
    , m_swizzledTexture(tcu::TextureFormat(), 1, 1)
{
    init();
}

TextureGather2DInstance::~TextureGather2DInstance(void)
{
}

vector<float> TextureGather2DInstance::computeQuadTexCoord(int /* iterationNdx */) const
{
    const bool biasMode   = (m_baseParams.levelMode == LevelMode::AMD_BIAS);
    const auto bottomLeft = (biasMode ? Vec2(0.0f, 0.0f) : Vec2(-0.3f, -0.4f));
    const auto topRight   = (biasMode ? Vec2(1.0f, 1.0f) : Vec2(1.5f, 1.6f));
    vector<float> res;
    TextureTestUtil::computeQuadTexCoord2D(res, bottomLeft, topRight);
    return res;
}

TextureBindingSp TextureGather2DInstance::createTexture(void)
{
    TestLog &log                            = m_context.getTestContext().getLog();
    const tcu::TextureFormatInfo texFmtInfo = tcu::getTextureFormatInfo(m_baseParams.textureFormat);
    MovePtr<tcu::Texture2D> texture =
        MovePtr<tcu::Texture2D>(new tcu::Texture2D(m_baseParams.textureFormat, m_textureSize.x(), m_textureSize.y()));
    const tcu::Sampler sampler(m_baseParams.wrapS, m_baseParams.wrapT, tcu::Sampler::REPEAT_GL, m_baseParams.minFilter,
                               m_baseParams.magFilter, 0.0f /* LOD threshold */, true /* normalized coords */,
                               m_baseParams.shadowCompareMode, 0 /* compare channel */,
                               tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f) /* border color */, true /* seamless cube*/);

    {
        const int levelBegin = ((m_baseParams.levelMode == LevelMode::NORMAL) ? m_baseParams.baseLevel : 0);
        const int levelEnd   = texture->getNumLevels();
        DE_ASSERT(m_baseParams.baseLevel < texture->getNumLevels());

        for (int levelNdx = levelBegin; levelNdx < levelEnd; levelNdx++)
        {
            texture->allocLevel(levelNdx);
            const PixelBufferAccess &level = texture->getLevel(levelNdx);
            fillWithRandomColorTiles(level, texFmtInfo.valueMin, texFmtInfo.valueMax,
                                     (uint32_t)m_context.getTestContext().getCommandLine().getBaseSeed());
            log << TestLog::Image("InputTextureLevel" + de::toString(levelNdx),
                                  "Input texture, level " + de::toString(levelNdx), level)
                << TestLog::Message << "Note: texture level's size is " << IVec2(level.getWidth(), level.getHeight())
                << TestLog::EndMessage;
        }

        swizzleTexture(m_swizzledTexture, *texture, m_baseParams.textureSwizzle);
    }

    return TextureBindingSp(new TextureBinding(texture.release(), sampler));
}

bool TextureGather2DInstance::verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const
{
    Vec2 texCoords[4];
    computeTexCoordVecs(computeQuadTexCoord(iterationNdx), texCoords);
    return TextureGatherInstance::verify(
        rendered, getOneLevelSubView(tcu::Texture2DView(m_swizzledTexture), m_baseParams.baseLevel), texCoords,
        m_iterations[iterationNdx]);
}

class TextureGather2DCase : public TestCase
{
public:
    TextureGather2DCase(tcu::TestContext &testCtx, const string &name, const GatherType gatherType,
                        const OffsetSize offsetSize, const tcu::TextureFormat textureFormat,
                        const tcu::Sampler::CompareMode shadowCompareMode, const tcu::Sampler::WrapMode wrapS,
                        const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
                        const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter,
                        const LevelMode levelMode, const int baseLevel, const uint32_t flags, const IVec2 &textureSize,
                        const ImageBackingMode sparseCase);
    virtual ~TextureGather2DCase(void);

    virtual void initPrograms(vk::SourceCollections &dst) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const GatherCaseBaseParams m_baseParams;
    const IVec2 m_textureSize;
};

TextureGather2DCase::TextureGather2DCase(
    tcu::TestContext &testCtx, const string &name, const GatherType gatherType, const OffsetSize offsetSize,
    const tcu::TextureFormat textureFormat, const tcu::Sampler::CompareMode shadowCompareMode,
    const tcu::Sampler::WrapMode wrapS, const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
    const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter, const LevelMode levelMode,
    const int baseLevel, const uint32_t flags, const IVec2 &textureSize, const ImageBackingMode sparseCase)
    : TestCase(testCtx, name)
    , m_baseParams(TEXTURETYPE_2D, gatherType, offsetSize, textureFormat, shadowCompareMode, wrapS, wrapT,
                   textureSwizzle, minFilter, magFilter, levelMode, baseLevel, flags, sparseCase)
    , m_textureSize(textureSize)
{
}

TextureGather2DCase::~TextureGather2DCase(void)
{
}

void TextureGather2DCase::initPrograms(vk::SourceCollections &dst) const
{
    const vector<GatherArgs> iterations = generateBasic2DCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        m_baseParams.offsetSize != OFFSETSIZE_IMPLEMENTATION_MAXIMUM ? getOffsetRange(m_baseParams.offsetSize) :
                                                                       IVec2(0));
    genGatherPrograms(dst, m_baseParams, iterations);
}

TestInstance *TextureGather2DCase::createInstance(Context &context) const
{
    const vector<GatherArgs> iterations = generateBasic2DCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        getOffsetRange(m_baseParams.offsetSize, context.getDeviceProperties().limits));

    return new TextureGather2DInstance(context, m_baseParams, m_textureSize, iterations);
}

void TextureGather2DCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED);
    checkMutableComparisonSamplersSupport(context, m_baseParams);

    if (m_baseParams.gatherType == GATHERTYPE_OFFSETS && m_baseParams.offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
    {
        if (context.getDeviceProperties().limits.minTexelGatherOffset > IMPLEMENTATION_MIN_MIN_OFFSET ||
            context.getDeviceProperties().limits.maxTexelGatherOffset < IMPLEMENTATION_MAX_MAX_OFFSET)
            TCU_THROW(NotSupportedError,
                      "Required minTexelGatherOffset and maxTexelGatherOffset limits are not supported");
    }
}

// 2D array

struct Gather2DArrayArgs
{
    GatherArgs gatherArgs;
    int layerNdx;

    operator GatherArgs() const
    {
        return gatherArgs;
    }
};

vector<Gather2DArrayArgs> generate2DArrayCaseIterations(GatherType gatherType, OffsetSize offsetSize,
                                                        LevelMode levelMode, const tcu::TextureFormat &textureFormat,
                                                        const IVec2 &offsetRange, const IVec3 &textureSize)
{
    const vector<GatherArgs> basicIterations =
        generateBasic2DCaseIterations(gatherType, offsetSize, levelMode, textureFormat, offsetRange);
    vector<Gather2DArrayArgs> iterations;

    // \note Out-of-bounds layer indices are tested too.
    for (int layerNdx = -1; layerNdx < textureSize.z() + 1; layerNdx++)
    {
        // Don't duplicate all cases for all layers.
        if (layerNdx == 0)
        {
            for (int basicNdx = 0; basicNdx < (int)basicIterations.size(); basicNdx++)
            {
                iterations.push_back(Gather2DArrayArgs());
                iterations.back().gatherArgs = basicIterations[basicNdx];
                iterations.back().layerNdx   = layerNdx;
            }
        }
        else
        {
            // For other layers than 0, only test one component and one set of offsets per layer.
            for (int basicNdx = 0; basicNdx < (int)basicIterations.size(); basicNdx++)
            {
                if (isDepthFormat(textureFormat) || basicIterations[basicNdx].componentNdx == (layerNdx + 2) % 4)
                {
                    iterations.push_back(Gather2DArrayArgs());
                    iterations.back().gatherArgs = basicIterations[basicNdx];
                    iterations.back().layerNdx   = layerNdx;
                    break;
                }
            }
        }
    }

    return iterations;
}

class TextureGather2DArrayInstance : public TextureGatherInstance
{
public:
    TextureGather2DArrayInstance(Context &context, const GatherCaseBaseParams &baseParams, const IVec3 &textureSize,
                                 const vector<Gather2DArrayArgs> &iterations);
    virtual ~TextureGather2DArrayInstance(void);

protected:
    virtual int getNumIterations(void) const
    {
        return (int)m_iterations.size();
    }
    virtual GatherArgs getGatherArgs(int iterationNdx) const
    {
        return m_iterations[iterationNdx].gatherArgs;
    }

    virtual TextureBindingSp createTexture(void);
    virtual vector<float> computeQuadTexCoord(int iterationNdx) const;
    virtual bool verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const;

private:
    const IVec3 m_textureSize;
    const vector<Gather2DArrayArgs> m_iterations;

    tcu::Texture2DArray m_swizzledTexture;
};

TextureGather2DArrayInstance::TextureGather2DArrayInstance(Context &context, const GatherCaseBaseParams &baseParams,
                                                           const IVec3 &textureSize,
                                                           const vector<Gather2DArrayArgs> &iterations)
    : TextureGatherInstance(context, baseParams)
    , m_textureSize(textureSize)
    , m_iterations(iterations)
    , m_swizzledTexture(tcu::TextureFormat(), 1, 1, 1)
{
    init();
}

TextureGather2DArrayInstance::~TextureGather2DArrayInstance(void)
{
}

vector<float> TextureGather2DArrayInstance::computeQuadTexCoord(int iterationNdx) const
{
    const bool biasMode   = (m_baseParams.levelMode == LevelMode::AMD_BIAS);
    const auto bottomLeft = (biasMode ? Vec2(0.0f, 0.0f) : Vec2(-0.3f, -0.4f));
    const auto topRight   = (biasMode ? Vec2(1.0f, 1.0f) : Vec2(1.5f, 1.6f));
    vector<float> res;
    TextureTestUtil::computeQuadTexCoord2DArray(res, m_iterations[iterationNdx].layerNdx, bottomLeft, topRight);
    return res;
}

TextureBindingSp TextureGather2DArrayInstance::createTexture(void)
{
    TestLog &log                            = m_context.getTestContext().getLog();
    const tcu::TextureFormatInfo texFmtInfo = tcu::getTextureFormatInfo(m_baseParams.textureFormat);
    MovePtr<tcu::Texture2DArray> texture    = MovePtr<tcu::Texture2DArray>(
        new tcu::Texture2DArray(m_baseParams.textureFormat, m_textureSize.x(), m_textureSize.y(), m_textureSize.z()));
    const tcu::Sampler sampler(m_baseParams.wrapS, m_baseParams.wrapT, tcu::Sampler::REPEAT_GL, m_baseParams.minFilter,
                               m_baseParams.magFilter, 0.0f /* LOD threshold */, true /* normalized coords */,
                               m_baseParams.shadowCompareMode, 0 /* compare channel */,
                               tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f) /* border color */, true /* seamless cube*/);

    {
        const int levelBegin = ((m_baseParams.levelMode == LevelMode::NORMAL) ? m_baseParams.baseLevel : 0);
        const int levelEnd   = texture->getNumLevels();
        DE_ASSERT(m_baseParams.baseLevel < texture->getNumLevels());

        for (int levelNdx = levelBegin; levelNdx < levelEnd; levelNdx++)
        {
            texture->allocLevel(levelNdx);
            const PixelBufferAccess &level = texture->getLevel(levelNdx);
            fillWithRandomColorTiles(level, texFmtInfo.valueMin, texFmtInfo.valueMax,
                                     (uint32_t)m_context.getTestContext().getCommandLine().getBaseSeed());

            log << TestLog::ImageSet("InputTextureLevel", "Input texture, level " + de::toString(levelNdx));
            for (int layerNdx = 0; layerNdx < m_textureSize.z(); layerNdx++)
                log << TestLog::Image("InputTextureLevel" + de::toString(layerNdx) + "Layer" + de::toString(layerNdx),
                                      "Layer " + de::toString(layerNdx),
                                      tcu::getSubregion(level, 0, 0, layerNdx, level.getWidth(), level.getHeight(), 1));
            log << TestLog::EndImageSet << TestLog::Message << "Note: texture level's size is "
                << IVec3(level.getWidth(), level.getHeight(), level.getDepth()) << TestLog::EndMessage;
        }

        swizzleTexture(m_swizzledTexture, *texture, m_baseParams.textureSwizzle);
    }

    return TextureBindingSp(new TextureBinding(texture.release(), sampler));
}

bool TextureGather2DArrayInstance::verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const
{
    Vec3 texCoords[4];
    computeTexCoordVecs(computeQuadTexCoord(iterationNdx), texCoords);
    return TextureGatherInstance::verify(
        rendered, getOneLevelSubView(tcu::Texture2DArrayView(m_swizzledTexture), m_baseParams.baseLevel), texCoords,
        m_iterations[iterationNdx].gatherArgs);
}

class TextureGather2DArrayCase : public TestCase
{
public:
    TextureGather2DArrayCase(tcu::TestContext &testCtx, const string &name, const GatherType gatherType,
                             const OffsetSize offsetSize, const tcu::TextureFormat textureFormat,
                             const tcu::Sampler::CompareMode shadowCompareMode, const tcu::Sampler::WrapMode wrapS,
                             const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
                             const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter,
                             const LevelMode levelMode, const int baseLevel, const uint32_t flags,
                             const IVec3 &textureSize, const ImageBackingMode sparseCase);
    virtual ~TextureGather2DArrayCase(void);

    virtual void initPrograms(vk::SourceCollections &dst) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const GatherCaseBaseParams m_baseParams;
    const IVec3 m_textureSize;
};

TextureGather2DArrayCase::TextureGather2DArrayCase(
    tcu::TestContext &testCtx, const string &name, const GatherType gatherType, const OffsetSize offsetSize,
    const tcu::TextureFormat textureFormat, const tcu::Sampler::CompareMode shadowCompareMode,
    const tcu::Sampler::WrapMode wrapS, const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
    const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter, const LevelMode levelMode,
    const int baseLevel, const uint32_t flags, const IVec3 &textureSize, const ImageBackingMode sparseCase)
    : TestCase(testCtx, name)
    , m_baseParams(TEXTURETYPE_2D_ARRAY, gatherType, offsetSize, textureFormat, shadowCompareMode, wrapS, wrapT,
                   textureSwizzle, minFilter, magFilter, levelMode, baseLevel, flags, sparseCase)
    , m_textureSize(textureSize)
{
}

TextureGather2DArrayCase::~TextureGather2DArrayCase(void)
{
}

void TextureGather2DArrayCase::initPrograms(vk::SourceCollections &dst) const
{
    const vector<Gather2DArrayArgs> iterations = generate2DArrayCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        m_baseParams.offsetSize != OFFSETSIZE_IMPLEMENTATION_MAXIMUM ? getOffsetRange(m_baseParams.offsetSize) :
                                                                       IVec2(0),
        m_textureSize);

    genGatherPrograms(dst, m_baseParams, vector<GatherArgs>(iterations.begin(), iterations.end()));
}

TestInstance *TextureGather2DArrayCase::createInstance(Context &context) const
{
    const vector<Gather2DArrayArgs> iterations = generate2DArrayCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        getOffsetRange(m_baseParams.offsetSize, context.getDeviceProperties().limits), m_textureSize);

    return new TextureGather2DArrayInstance(context, m_baseParams, m_textureSize, iterations);
}

void TextureGather2DArrayCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED);
    checkMutableComparisonSamplersSupport(context, m_baseParams);

    if (m_baseParams.gatherType == GATHERTYPE_OFFSETS && m_baseParams.offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM)
    {
        if (context.getDeviceProperties().limits.minTexelGatherOffset > IMPLEMENTATION_MIN_MIN_OFFSET ||
            context.getDeviceProperties().limits.maxTexelGatherOffset < IMPLEMENTATION_MAX_MAX_OFFSET)
            TCU_THROW(NotSupportedError,
                      "Required minTexelGatherOffset and maxTexelGatherOffset limits are not supported");
    }
}

// Cube

struct GatherCubeArgs
{
    GatherArgs gatherArgs;
    tcu::CubeFace face;

    operator GatherArgs() const
    {
        return gatherArgs;
    }
};

vector<GatherCubeArgs> generateCubeCaseIterations(GatherType gatherType, OffsetSize offsetSize, LevelMode levelMode,
                                                  const tcu::TextureFormat &textureFormat, const IVec2 &offsetRange)
{
    const vector<GatherArgs> basicIterations =
        generateBasic2DCaseIterations(gatherType, offsetSize, levelMode, textureFormat, offsetRange);
    vector<GatherCubeArgs> iterations;

    for (int cubeFaceI = 0; cubeFaceI < tcu::CUBEFACE_LAST; cubeFaceI++)
    {
        const tcu::CubeFace cubeFace = (tcu::CubeFace)cubeFaceI;

        // Don't duplicate all cases for all faces.
        if (cubeFaceI == 0)
        {
            for (int basicNdx = 0; basicNdx < (int)basicIterations.size(); basicNdx++)
            {
                iterations.push_back(GatherCubeArgs());
                iterations.back().gatherArgs = basicIterations[basicNdx];
                iterations.back().face       = cubeFace;
            }
        }
        else
        {
            // For other faces than first, only test one component per face.
            for (int basicNdx = 0; basicNdx < (int)basicIterations.size(); basicNdx++)
            {
                if (isDepthFormat(textureFormat) || basicIterations[basicNdx].componentNdx == cubeFaceI % 4)
                {
                    iterations.push_back(GatherCubeArgs());
                    iterations.back().gatherArgs = basicIterations[basicNdx];
                    iterations.back().face       = cubeFace;
                    break;
                }
            }
        }
    }

    return iterations;
}

class TextureGatherCubeInstance : public TextureGatherInstance
{
public:
    TextureGatherCubeInstance(Context &context, const GatherCaseBaseParams &baseParams, const int textureSize,
                              const vector<GatherCubeArgs> &iterations);
    virtual ~TextureGatherCubeInstance(void);

protected:
    virtual int getNumIterations(void) const
    {
        return (int)m_iterations.size();
    }
    virtual GatherArgs getGatherArgs(int iterationNdx) const
    {
        return m_iterations[iterationNdx].gatherArgs;
    }

    virtual TextureBindingSp createTexture(void);
    virtual vector<float> computeQuadTexCoord(int iterationNdx) const;
    virtual bool verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const;

private:
    const int m_textureSize;
    const vector<GatherCubeArgs> m_iterations;

    tcu::TextureCube m_swizzledTexture;
};

TextureGatherCubeInstance::TextureGatherCubeInstance(Context &context, const GatherCaseBaseParams &baseParams,
                                                     const int textureSize, const vector<GatherCubeArgs> &iterations)
    : TextureGatherInstance(context, baseParams)
    , m_textureSize(textureSize)
    , m_iterations(iterations)
    , m_swizzledTexture(tcu::TextureFormat(), 1)
{
    init();
}

TextureGatherCubeInstance::~TextureGatherCubeInstance(void)
{
}

vector<float> TextureGatherCubeInstance::computeQuadTexCoord(int iterationNdx) const
{
    const bool biasMode = (m_baseParams.levelMode == LevelMode::AMD_BIAS);
    const bool corners  = (m_baseParams.flags & GATHERCASE_DONT_SAMPLE_CUBE_CORNERS) == 0;
    const Vec2 minC     = (biasMode ? Vec2(-1.0f) : (corners ? Vec2(-1.2f) : Vec2(-0.6f, -1.2f)));
    const Vec2 maxC     = (biasMode ? Vec2(1.0f) : (corners ? Vec2(1.2f) : Vec2(0.6f, 1.2f)));
    vector<float> res;
    TextureTestUtil::computeQuadTexCoordCube(res, m_iterations[iterationNdx].face, minC, maxC);
    return res;
}

TextureBindingSp TextureGatherCubeInstance::createTexture(void)
{
    TestLog &log                            = m_context.getTestContext().getLog();
    const tcu::TextureFormatInfo texFmtInfo = tcu::getTextureFormatInfo(m_baseParams.textureFormat);
    MovePtr<tcu::TextureCube> texture =
        MovePtr<tcu::TextureCube>(new tcu::TextureCube(m_baseParams.textureFormat, m_textureSize));
    const tcu::Sampler sampler(m_baseParams.wrapS, m_baseParams.wrapT, tcu::Sampler::REPEAT_GL, m_baseParams.minFilter,
                               m_baseParams.magFilter, 0.0f /* LOD threshold */, true /* normalized coords */,
                               m_baseParams.shadowCompareMode, 0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */,
                               true /* seamless cube map */);

    {
        const int levelBegin = ((m_baseParams.levelMode == LevelMode::NORMAL) ? m_baseParams.baseLevel : 0);
        const int levelEnd   = texture->getNumLevels();
        DE_ASSERT(m_baseParams.baseLevel < texture->getNumLevels());

        for (int levelNdx = levelBegin; levelNdx < levelEnd; levelNdx++)
        {
            log << TestLog::ImageSet("InputTextureLevel" + de::toString(levelNdx),
                                     "Input texture, level " + de::toString(levelNdx));

            for (int cubeFaceI = 0; cubeFaceI < tcu::CUBEFACE_LAST; cubeFaceI++)
            {
                const tcu::CubeFace cubeFace = (tcu::CubeFace)cubeFaceI;
                texture->allocLevel(cubeFace, levelNdx);
                const PixelBufferAccess &levelFace = texture->getLevelFace(levelNdx, cubeFace);
                fillWithRandomColorTiles(levelFace, texFmtInfo.valueMin, texFmtInfo.valueMax,
                                         (uint32_t)m_context.getTestContext().getCommandLine().getBaseSeed() ^
                                             (uint32_t)cubeFaceI);

                log << TestLog::Image("InputTextureLevel" + de::toString(levelNdx) + "Face" +
                                          de::toString((int)cubeFace),
                                      de::toString(cubeFace), levelFace);
            }

            log << TestLog::EndImageSet << TestLog::Message << "Note: texture level's size is "
                << texture->getLevelFace(levelNdx, tcu::CUBEFACE_NEGATIVE_X).getWidth() << TestLog::EndMessage;
        }

        swizzleTexture(m_swizzledTexture, *texture, m_baseParams.textureSwizzle);
    }

    return TextureBindingSp(new TextureBinding(texture.release(), sampler));
}

bool TextureGatherCubeInstance::verify(int iterationNdx, const ConstPixelBufferAccess &rendered) const
{
    Vec3 texCoords[4];
    computeTexCoordVecs(computeQuadTexCoord(iterationNdx), texCoords);
    return TextureGatherInstance::verify(
        rendered, getOneLevelSubView(tcu::TextureCubeView(m_swizzledTexture), m_baseParams.baseLevel), texCoords,
        m_iterations[iterationNdx].gatherArgs);
}

// \note Cube case always uses just basic textureGather(); offset versions are not defined for cube maps.
class TextureGatherCubeCase : public TestCase
{
public:
    TextureGatherCubeCase(tcu::TestContext &testCtx, const string &name, const tcu::TextureFormat textureFormat,
                          const tcu::Sampler::CompareMode shadowCompareMode, const tcu::Sampler::WrapMode wrapS,
                          const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
                          const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter,
                          const LevelMode levelMode, const int baseLevel, const uint32_t flags, const int textureSize,
                          const ImageBackingMode sparseCase);
    virtual ~TextureGatherCubeCase(void);

    virtual void initPrograms(vk::SourceCollections &dst) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const GatherCaseBaseParams m_baseParams;
    const int m_textureSize;
};

TextureGatherCubeCase::TextureGatherCubeCase(
    tcu::TestContext &testCtx, const string &name, const tcu::TextureFormat textureFormat,
    const tcu::Sampler::CompareMode shadowCompareMode, const tcu::Sampler::WrapMode wrapS,
    const tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &textureSwizzle,
    const tcu::Sampler::FilterMode minFilter, const tcu::Sampler::FilterMode magFilter, const LevelMode levelMode,
    const int baseLevel, const uint32_t flags, const int textureSize, const ImageBackingMode sparseCase)
    : TestCase(testCtx, name)
    , m_baseParams(TEXTURETYPE_CUBE, GATHERTYPE_BASIC, OFFSETSIZE_NONE, textureFormat, shadowCompareMode, wrapS, wrapT,
                   textureSwizzle, minFilter, magFilter, levelMode, baseLevel, flags, sparseCase)
    , m_textureSize(textureSize)
{
}

TextureGatherCubeCase::~TextureGatherCubeCase(void)
{
}

void TextureGatherCubeCase::initPrograms(vk::SourceCollections &dst) const
{
    const vector<GatherCubeArgs> iterations = generateCubeCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        m_baseParams.offsetSize != OFFSETSIZE_IMPLEMENTATION_MAXIMUM ? getOffsetRange(m_baseParams.offsetSize) :
                                                                       IVec2(0));

    genGatherPrograms(dst, m_baseParams, vector<GatherArgs>(iterations.begin(), iterations.end()));
}

TestInstance *TextureGatherCubeCase::createInstance(Context &context) const
{
    const vector<GatherCubeArgs> iterations = generateCubeCaseIterations(
        m_baseParams.gatherType, m_baseParams.offsetSize, m_baseParams.levelMode, m_baseParams.textureFormat,
        getOffsetRange(m_baseParams.offsetSize, context.getDeviceProperties().limits));

    return new TextureGatherCubeInstance(context, m_baseParams, m_textureSize, iterations);
}

void TextureGatherCubeCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED);
    checkMutableComparisonSamplersSupport(context, m_baseParams);
}

class TextureGatherTests : public tcu::TestCaseGroup
{
public:
    TextureGatherTests(tcu::TestContext &context);
    virtual ~TextureGatherTests(void);
    virtual void init(void);

private:
    TextureGatherTests(const TextureGatherTests &);            // not allowed!
    TextureGatherTests &operator=(const TextureGatherTests &); // not allowed!
};

TextureGatherTests::TextureGatherTests(tcu::TestContext &context) : TestCaseGroup(context, "texture_gather")
{
}

TextureGatherTests::~TextureGatherTests(void)
{
}

static inline TestCase *makeTextureGatherCase(
    TextureType textureType, tcu::TestContext &testCtx, const string &name, GatherType gatherType,
    OffsetSize offsetSize, tcu::TextureFormat textureFormat, tcu::Sampler::CompareMode shadowCompareMode,
    tcu::Sampler::WrapMode wrapS, tcu::Sampler::WrapMode wrapT, const MaybeTextureSwizzle &texSwizzle,
    tcu::Sampler::FilterMode minFilter, tcu::Sampler::FilterMode magFilter, LevelMode levelMode, int baseLevel,
    const IVec3 &textureSize, uint32_t flags = 0,
    const ImageBackingMode sparseCase = ShaderRenderCaseInstance::IMAGE_BACKING_MODE_REGULAR)
{
    switch (textureType)
    {
    case TEXTURETYPE_2D:
        return new TextureGather2DCase(testCtx, name, gatherType, offsetSize, textureFormat, shadowCompareMode, wrapS,
                                       wrapT, texSwizzle, minFilter, magFilter, levelMode, baseLevel, flags,
                                       textureSize.swizzle(0, 1), sparseCase);

    case TEXTURETYPE_2D_ARRAY:
        return new TextureGather2DArrayCase(testCtx, name, gatherType, offsetSize, textureFormat, shadowCompareMode,
                                            wrapS, wrapT, texSwizzle, minFilter, magFilter, levelMode, baseLevel, flags,
                                            textureSize, sparseCase);

    case TEXTURETYPE_CUBE:
        DE_ASSERT(gatherType == GATHERTYPE_BASIC);
        DE_ASSERT(offsetSize == OFFSETSIZE_NONE);
        return new TextureGatherCubeCase(testCtx, name, textureFormat, shadowCompareMode, wrapS, wrapT, texSwizzle,
                                         minFilter, magFilter, levelMode, baseLevel, flags, textureSize.x(),
                                         sparseCase);

    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static inline const char *compareModeName(tcu::Sampler::CompareMode mode)
{
    switch (mode)
    {
    case tcu::Sampler::COMPAREMODE_LESS:
        return "less";
    case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:
        return "less_or_equal";
    case tcu::Sampler::COMPAREMODE_GREATER:
        return "greater";
    case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:
        return "greater_or_equal";
    case tcu::Sampler::COMPAREMODE_EQUAL:
        return "equal";
    case tcu::Sampler::COMPAREMODE_NOT_EQUAL:
        return "not_equal";
    case tcu::Sampler::COMPAREMODE_ALWAYS:
        return "always";
    case tcu::Sampler::COMPAREMODE_NEVER:
        return "never";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void TextureGatherTests::init(void)
{
    const struct
    {
        const char *name;
        TextureType type;
    } textureTypes[] = {{"2d", TEXTURETYPE_2D}, {"2d_array", TEXTURETYPE_2D_ARRAY}, {"cube", TEXTURETYPE_CUBE}};

    const struct
    {
        const char *name;
        tcu::TextureFormat format;
    } formats[] = {{"rgba8", tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)},
                   {"rgba8ui", tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT8)},
                   {"rgba8i", tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT8)},
                   {"depth32f", tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT)}};

    const struct
    {
        const char *name;
        IVec3 size;
    } textureSizes[] = {{"size_pot", IVec3(64, 64, 3)}, {"size_npot", IVec3(17, 23, 3)}};

    const struct
    {
        const char *name;
        tcu::Sampler::WrapMode mode;
    } wrapModes[] = {{"clamp_to_edge", tcu::Sampler::CLAMP_TO_EDGE},
                     {"repeat", tcu::Sampler::REPEAT_GL},
                     {"mirrored_repeat", tcu::Sampler::MIRRORED_REPEAT_GL}};

    for (int gatherTypeI = 0; gatherTypeI < GATHERTYPE_LAST; gatherTypeI++)
    {
        const GatherType gatherType          = (GatherType)gatherTypeI;
        TestCaseGroup *const gatherTypeGroup = new TestCaseGroup(m_testCtx, gatherTypeName(gatherType));
        addChild(gatherTypeGroup);

        for (int offsetSizeI = 0; offsetSizeI < OFFSETSIZE_LAST; offsetSizeI++)
        {
            const OffsetSize offsetSize = (OffsetSize)offsetSizeI;
            if ((gatherType == GATHERTYPE_BASIC) != (offsetSize == OFFSETSIZE_NONE))
                continue;

            TestCaseGroup *const offsetSizeGroup =
                offsetSize == OFFSETSIZE_NONE ?
                    gatherTypeGroup :
                    new TestCaseGroup(m_testCtx, offsetSize == OFFSETSIZE_MINIMUM_REQUIRED ? "min_required_offset" :
                                                 offsetSize == OFFSETSIZE_IMPLEMENTATION_MAXIMUM ?
                                                                                             "implementation_offset" :
                                                                                             nullptr);

            if (offsetSizeGroup != gatherTypeGroup)
                gatherTypeGroup->addChild(offsetSizeGroup);

            for (int textureTypeNdx = 0; textureTypeNdx < DE_LENGTH_OF_ARRAY(textureTypes); textureTypeNdx++)
            {
                const TextureType textureType = textureTypes[textureTypeNdx].type;

                if (textureType == TEXTURETYPE_CUBE && gatherType != GATHERTYPE_BASIC)
                    continue;

                TestCaseGroup *const textureTypeGroup = new TestCaseGroup(m_testCtx, textureTypes[textureTypeNdx].name);
                offsetSizeGroup->addChild(textureTypeGroup);

                for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
                {
                    const tcu::TextureFormat &format = formats[formatNdx].format;
                    TestCaseGroup *const formatGroup = new TestCaseGroup(m_testCtx, formats[formatNdx].name);
                    textureTypeGroup->addChild(formatGroup);

                    for (int noCornersI = 0; noCornersI <= ((textureType == TEXTURETYPE_CUBE) ? 1 : 0); noCornersI++)
                    {
                        // Test case variants that don't sample around cube map corners
                        const bool noCorners = noCornersI != 0;
                        TestCaseGroup *const cornersGroup =
                            noCorners ? new TestCaseGroup(m_testCtx, "no_corners") : formatGroup;

                        if (formatGroup != cornersGroup)
                            formatGroup->addChild(cornersGroup);

                        for (int textureSizeNdx = 0; textureSizeNdx < DE_LENGTH_OF_ARRAY(textureSizes);
                             textureSizeNdx++)
                        {
                            const IVec3 &textureSize = textureSizes[textureSizeNdx].size;
                            TestCaseGroup *const textureSizeGroup =
                                new TestCaseGroup(m_testCtx, textureSizes[textureSizeNdx].name);
                            cornersGroup->addChild(textureSizeGroup);

                            for (int compareModeI = 0; compareModeI < tcu::Sampler::COMPAREMODE_LAST; compareModeI++)
                            {
                                const tcu::Sampler::CompareMode compareMode = (tcu::Sampler::CompareMode)compareModeI;

                                if ((compareMode != tcu::Sampler::COMPAREMODE_NONE) != isDepthFormat(format))
                                    continue;

                                if (compareMode != tcu::Sampler::COMPAREMODE_NONE &&
                                    compareMode != tcu::Sampler::COMPAREMODE_LESS &&
                                    compareMode != tcu::Sampler::COMPAREMODE_GREATER)
                                    continue;

                                TestCaseGroup *const compareModeGroup =
                                    compareMode == tcu::Sampler::COMPAREMODE_NONE ?
                                        textureSizeGroup :
                                        new TestCaseGroup(
                                            m_testCtx, (string() + "compare_" + compareModeName(compareMode)).c_str());
                                if (compareModeGroup != textureSizeGroup)
                                    textureSizeGroup->addChild(compareModeGroup);

                                for (int wrapCaseNdx = 0; wrapCaseNdx < DE_LENGTH_OF_ARRAY(wrapModes); wrapCaseNdx++)
                                {
                                    const int wrapSNdx = wrapCaseNdx;
                                    const int wrapTNdx = (wrapCaseNdx + 1) % DE_LENGTH_OF_ARRAY(wrapModes);
                                    const tcu::Sampler::WrapMode wrapS = wrapModes[wrapSNdx].mode;
                                    const tcu::Sampler::WrapMode wrapT = wrapModes[wrapTNdx].mode;

                                    const string caseName =
                                        string() + wrapModes[wrapSNdx].name + "_" + wrapModes[wrapTNdx].name;

                                    compareModeGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, caseName.c_str(), gatherType, offsetSize, format,
                                        compareMode, wrapS, wrapT, MaybeTextureSwizzle::createNoneTextureSwizzle(),
                                        tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, LevelMode::NORMAL, 0, textureSize,
                                        noCorners ? GATHERCASE_DONT_SAMPLE_CUBE_CORNERS : 0));
#ifndef CTS_USES_VULKANSC
                                    compareModeGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, "sparse_" + caseName, gatherType, offsetSize, format,
                                        compareMode, wrapS, wrapT, MaybeTextureSwizzle::createNoneTextureSwizzle(),
                                        tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, LevelMode::NORMAL, 0, textureSize,
                                        noCorners ? GATHERCASE_DONT_SAMPLE_CUBE_CORNERS : 0,
                                        ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE));
#endif // CTS_USES_VULKANSC
                                }
                            }
                        }
                    }

                    if (offsetSize != OFFSETSIZE_MINIMUM_REQUIRED ||
                        gatherType ==
                            GATHERTYPE_OFFSETS) // Don't test all features for both offset size types, as they should be rather orthogonal.
                    {
                        if (!isDepthFormat(format))
                        {
                            TestCaseGroup *const swizzleGroup = new TestCaseGroup(m_testCtx, "texture_swizzle");
                            formatGroup->addChild(swizzleGroup);

                            DE_STATIC_ASSERT(TEXTURESWIZZLECOMPONENT_R == 0);
                            for (int swizzleCaseNdx = 0; swizzleCaseNdx < TEXTURESWIZZLECOMPONENT_LAST;
                                 swizzleCaseNdx++)
                            {
                                MaybeTextureSwizzle swizzle = MaybeTextureSwizzle::createSomeTextureSwizzle();
                                string caseName;

                                for (int i = 0; i < 4; i++)
                                {
                                    swizzle.getSwizzle()[i] =
                                        (TextureSwizzleComponent)((swizzleCaseNdx + i) %
                                                                  (int)TEXTURESWIZZLECOMPONENT_LAST);
                                    caseName += (i > 0 ? "_" : "") + de::toLower(de::toString(swizzle.getSwizzle()[i]));
                                }

                                swizzleGroup->addChild(makeTextureGatherCase(
                                    textureType, m_testCtx, caseName.c_str(), gatherType, offsetSize, format,
                                    tcu::Sampler::COMPAREMODE_NONE, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                    swizzle, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, LevelMode::NORMAL, 0,
                                    IVec3(64, 64, 3)));
#ifndef CTS_USES_VULKANSC
                                swizzleGroup->addChild(makeTextureGatherCase(
                                    textureType, m_testCtx, "sparse_" + caseName, gatherType, offsetSize, format,
                                    tcu::Sampler::COMPAREMODE_NONE, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                    swizzle, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, LevelMode::NORMAL, 0,
                                    IVec3(64, 64, 3), 0, ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE));
#endif // CTS_USES_VULKANSC
                            }
                        }

                        {
                            TestCaseGroup *const filterModeGroup = new TestCaseGroup(m_testCtx, "filter_mode");
                            formatGroup->addChild(filterModeGroup);

                            const struct
                            {
                                const char *name;
                                tcu::Sampler::FilterMode filter;
                            } magFilters[] = {{"linear", tcu::Sampler::LINEAR}, {"nearest", tcu::Sampler::NEAREST}};

                            const struct
                            {
                                const char *name;
                                tcu::Sampler::FilterMode filter;
                            } minFilters[] = {
                                // \note Don't test NEAREST here, as it's covered by other cases.
                                {"linear", tcu::Sampler::LINEAR},
                                {"nearest_mipmap_nearest", tcu::Sampler::NEAREST_MIPMAP_NEAREST},
                                {"nearest_mipmap_linear", tcu::Sampler::NEAREST_MIPMAP_LINEAR},
                                {"linear_mipmap_nearest", tcu::Sampler::LINEAR_MIPMAP_NEAREST},
                                {"linear_mipmap_linear", tcu::Sampler::LINEAR_MIPMAP_LINEAR},
                            };

                            for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(minFilters); minFilterNdx++)
                                for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(magFilters);
                                     magFilterNdx++)
                                {
                                    const tcu::Sampler::FilterMode minFilter    = minFilters[minFilterNdx].filter;
                                    const tcu::Sampler::FilterMode magFilter    = magFilters[magFilterNdx].filter;
                                    const tcu::Sampler::CompareMode compareMode = isDepthFormat(format) ?
                                                                                      tcu::Sampler::COMPAREMODE_LESS :
                                                                                      tcu::Sampler::COMPAREMODE_NONE;

                                    if ((isUnormFormatType(format.type) || isDepthFormat(format)) &&
                                        magFilter == tcu::Sampler::NEAREST)
                                        continue; // Covered by other cases.
                                    if ((isUIntFormatType(format.type) || isSIntFormatType(format.type)) &&
                                        (magFilter != tcu::Sampler::NEAREST ||
                                         minFilter != tcu::Sampler::NEAREST_MIPMAP_NEAREST))
                                        continue;

                                    const string caseName = string() + "min_" + minFilters[minFilterNdx].name +
                                                            "_mag_" + magFilters[magFilterNdx].name;

                                    filterModeGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, caseName.c_str(), gatherType, offsetSize, format,
                                        compareMode, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                        MaybeTextureSwizzle::createNoneTextureSwizzle(), minFilter, magFilter,
                                        LevelMode::NORMAL, 0, IVec3(64, 64, 3)));
#ifndef CTS_USES_VULKANSC
                                    filterModeGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, "sparse_" + caseName, gatherType, offsetSize, format,
                                        compareMode, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                        MaybeTextureSwizzle::createNoneTextureSwizzle(), minFilter, magFilter,
                                        LevelMode::NORMAL, 0, IVec3(64, 64, 3), 0,
                                        ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE));
#endif // CTS_USES_VULKANSC
                                }
                        }

                        {
                            TestCaseGroup *const baseLevelGroup = new TestCaseGroup(m_testCtx, "base_level");
                            formatGroup->addChild(baseLevelGroup);

                            for (int baseLevel = 1; baseLevel <= 2; baseLevel++)
                            {
                                static const struct
                                {
                                    const std::string suffix;
                                    LevelMode levelMode;
                                } levelModes[] = {
                                    {"", LevelMode::NORMAL},
#ifndef CTS_USES_VULKANSC
                                    {"_amd_bias", LevelMode::AMD_BIAS},
                                    {"_amd_lod", LevelMode::AMD_LOD},
#endif
                                };

                                for (int modeIdx = 0; modeIdx < DE_LENGTH_OF_ARRAY(levelModes); ++modeIdx)
                                {
                                    const auto &mode = levelModes[modeIdx].levelMode;

                                    // Not supported for these sampler types.
                                    if (isDepthFormat(format) && mode != LevelMode::NORMAL)
                                        continue;

                                    const string caseName =
                                        "level_" + de::toString(baseLevel) + levelModes[modeIdx].suffix;
                                    const tcu::Sampler::CompareMode compareMode = isDepthFormat(format) ?
                                                                                      tcu::Sampler::COMPAREMODE_LESS :
                                                                                      tcu::Sampler::COMPAREMODE_NONE;
                                    // The minFilter mode may need to be NEAREST_MIPMAP_NEAREST so the sampler creating code will not limit maxLod.
                                    const auto minFilter =
                                        ((mode == LevelMode::NORMAL) ? tcu::Sampler::NEAREST :
                                                                       tcu::Sampler::NEAREST_MIPMAP_NEAREST);
                                    baseLevelGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, caseName.c_str(), gatherType, offsetSize, format,
                                        compareMode, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                        MaybeTextureSwizzle::createNoneTextureSwizzle(), minFilter,
                                        tcu::Sampler::NEAREST, mode, baseLevel, IVec3(64, 64, 3)));
#ifndef CTS_USES_VULKANSC
                                    baseLevelGroup->addChild(makeTextureGatherCase(
                                        textureType, m_testCtx, "sparse_" + caseName, gatherType, offsetSize, format,
                                        compareMode, tcu::Sampler::REPEAT_GL, tcu::Sampler::REPEAT_GL,
                                        MaybeTextureSwizzle::createNoneTextureSwizzle(), minFilter,
                                        tcu::Sampler::NEAREST, mode, baseLevel, IVec3(64, 64, 3), 0,
                                        ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE));
#endif // CTS_USES_VULKANSC
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace

tcu::TestCaseGroup *createTextureGatherTests(tcu::TestContext &testCtx)
{
    return new TextureGatherTests(testCtx);
}

} // namespace sr
} // namespace vkt
