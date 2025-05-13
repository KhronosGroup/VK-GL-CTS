#ifndef _VKTSHADERBFLOAT16TESTS_HPP
#define _VKTSHADERBFLOAT16TESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Tests for types introduced in VK_KHR_shader_bfloat16.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "deUniquePtr.hpp"
#include "vkRef.hpp"

#include <sstream>

[[maybe_unused]] static void raiseDeAssertIfFail(bool x, int index, const char *file, int line)
{
    if (!x)
    {
        std::ostringstream os;
        os << "DE_MULTI_ASSERT macro failed, "
              "false argument at "
           << index;
        os.flush();
        deAssertFail(os.str().c_str(), file, line);
    }
}

#if defined(DE_DEBUG) && !defined(DE_COVERAGE_BUILD)
#define DE_MULTI_ASSERT_EXPAND(file_, line_, ...)                                                           \
    do                                                                                                      \
    {                                                                                                       \
        int index    = 0;                                                                                   \
        auto asserte = [&index](auto &&...u) { (..., raiseDeAssertIfFail(!!(u), index++, file_, line_)); }; \
        asserte(__VA_ARGS__);                                                                               \
    } while (false)
#else
#define DE_MULTI_ASSERT_EXPAND(file_, line_, ...)
#endif

#define DE_MULTI_ASSERT(...) DE_MULTI_ASSERT_EXPAND(__FILE__, __LINE__, __VA_ARGS__)

#define DE_MULTI_UNREF(...)             \
    do                                  \
    {                                   \
        auto unref = [](auto &&...) {}; \
        unref(__VA_ARGS__);             \
    } while (false)

namespace vkt
{
namespace shaderexecutor
{

namespace bf16
{
template <typename, uint32_t>
const char *getVecTypeName();

template <typename>
const char *getExtensionName();

template <class T, class P = T (*)[1], class R = decltype(std::begin(*std::declval<P>()))>
static auto makeStdBeginEnd(void *p, uint32_t n) -> std::pair<R, R>
{
    auto begin = std::begin(*P(p));
    return {begin, std::next(begin, n)};
}

template <typename X, typename... CtorArgs>
de::MovePtr<X> makeMovePtr(CtorArgs &&...args)
{
    return de::MovePtr<X>(new X(std::forward<CtorArgs>(args)...));
}

template <typename X, typename... CtorArgs>
void makeMovePtr(de::MovePtr<X> &x, CtorArgs &&...args)
{
    x = makeMovePtr<X>(std::forward<CtorArgs>(args)...);
}

template <typename X, typename... CtorArgs>
vk::Move<X> makeMove(CtorArgs &&...args)
{
    return vk::Move<X>(makeMovePtr<X>(std::forward<CtorArgs>(args)...).release());
}

template <typename X>
const X *fwd_as_ptr(const X &x)
{
    return &x;
}

} // namespace bf16

tcu::TestCaseGroup *createBFloat16Tests(tcu::TestContext &testCtx);

} // namespace shaderexecutor
} // namespace vkt

#endif // _VKTSHADERBFLOAT16TESTS_HPP
