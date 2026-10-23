#ifndef _VKTTESTGROUPUTIL_HPP
#define _VKTTESTGROUPUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief TestCaseGroup utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

#include <optional>
#include <utility>

namespace vkt
{

class TestGroupHelper0 : public tcu::TestCaseGroup
{
public:
    typedef void (*CreateChildrenFunc)(tcu::TestCaseGroup *testGroup);
    typedef void (*CleanupGroupFunc)(tcu::TestCaseGroup *testGroup);

    TestGroupHelper0(tcu::TestContext &testCtx, const std::string &name, CreateChildrenFunc createChildren,
                     CleanupGroupFunc cleanupGroup)
        : tcu::TestCaseGroup(testCtx, name.c_str())
        , m_createChildren(createChildren)
        , m_cleanupGroup(cleanupGroup)
    {
    }

    ~TestGroupHelper0(void)
    {
    }

    void init(void)
    {
        m_createChildren(this);
    }

    void deinit(void)
    {
        if (m_cleanupGroup)
            m_cleanupGroup(this);
        tcu::TestCaseGroup::deinit();
    }

private:
    const CreateChildrenFunc m_createChildren;
    const CleanupGroupFunc m_cleanupGroup;
};

// Like TestGroupHelper0, but using std::optional and templated callback function types, which makes it possible to use
// almost any callable as the create children and cleanup group callbacks, including lambdas with non-empty captures.
template <class CreateChildrenCallback, class CleanupGroupCallback>
class TestGroupHelper0_T : public tcu::TestCaseGroup
{
public:
    TestGroupHelper0_T(tcu::TestContext &testCtx, const std::string &name,
                       std::optional<CreateChildrenCallback> createChildren = std::nullopt,
                       std::optional<CleanupGroupCallback> cleanupGroup     = std::nullopt)
        : tcu::TestCaseGroup(testCtx, name.c_str())
        , m_createChildren(std::move(createChildren))
        , m_cleanupGroup(std::move(cleanupGroup))
    {
    }

    ~TestGroupHelper0_T(void)
    {
    }

    void init(void)
    {
        if (m_createChildren)
            (*m_createChildren)(this);
    }

    void deinit(void)
    {
        if (m_cleanupGroup)
            (*m_cleanupGroup)(this);
        tcu::TestCaseGroup::deinit();
    }

private:
    const std::optional<CreateChildrenCallback> m_createChildren;
    const std::optional<CleanupGroupCallback> m_cleanupGroup;
};

template <typename Arg0>
class TestGroupHelper1 : public tcu::TestCaseGroup
{
public:
    typedef void (*CreateChildrenFunc)(tcu::TestCaseGroup *testGroup, Arg0 arg0);
    typedef void (*CleanupGroupFunc)(tcu::TestCaseGroup *testGroup, Arg0 arg0);

    TestGroupHelper1(tcu::TestContext &testCtx, const std::string &name, CreateChildrenFunc createChildren,
                     const Arg0 &arg0, CleanupGroupFunc cleanupGroup)
        : tcu::TestCaseGroup(testCtx, name.c_str())
        , m_createChildren(createChildren)
        , m_cleanupGroup(cleanupGroup)
        , m_arg0(arg0)
    {
    }

    void init(void)
    {
        m_createChildren(this, m_arg0);
    }
    void deinit(void)
    {
        if (m_cleanupGroup)
            m_cleanupGroup(this, m_arg0);
        tcu::TestCaseGroup::deinit();
    }

private:
    const CreateChildrenFunc m_createChildren;
    const CleanupGroupFunc m_cleanupGroup;
    const Arg0 m_arg0;
};

template <typename Arg0, typename Arg1>
class TestGroupHelper2 : public tcu::TestCaseGroup
{
public:
    typedef void (*CreateChildrenFunc)(tcu::TestCaseGroup *testGroup, Arg0 arg0, Arg1 arg1);
    typedef void (*CleanupGroupFunc)(tcu::TestCaseGroup *testGroup, Arg0 arg0, Arg1 arg1);

    TestGroupHelper2(tcu::TestContext &testCtx, const std::string &name, CreateChildrenFunc createChildren,
                     const Arg0 &arg0, const Arg1 &arg1, CleanupGroupFunc cleanupGroup)
        : tcu::TestCaseGroup(testCtx, name.c_str())
        , m_createChildren(createChildren)
        , m_cleanupGroup(cleanupGroup)
        , m_arg0(arg0)
        , m_arg1(arg1)
    {
    }

    void init(void)
    {
        m_createChildren(this, m_arg0, m_arg1);
    }
    void deinit(void)
    {
        if (m_cleanupGroup)
            m_cleanupGroup(this, m_arg0, m_arg1);
        tcu::TestCaseGroup::deinit();
    }

private:
    const CreateChildrenFunc m_createChildren;
    const CleanupGroupFunc m_cleanupGroup;
    const Arg0 m_arg0;
    const Arg1 m_arg1;
};

inline tcu::TestCaseGroup *createTestGroup(tcu::TestContext &testCtx, const std::string &name,
                                           TestGroupHelper0::CreateChildrenFunc createChildren,
                                           TestGroupHelper0::CleanupGroupFunc cleanupGroup = nullptr)
{
    return new TestGroupHelper0(testCtx, name, createChildren, cleanupGroup);
}

// Like the previous template but allowing functors as callbacks.
template <class CreateChildrenCallback>
inline tcu::TestCaseGroup *createTestGroup2(tcu::TestContext &testCtx, const std::string &name,
                                            CreateChildrenCallback createChildren)
{
    return new TestGroupHelper0_T<CreateChildrenCallback, void (*)(tcu::TestCaseGroup *)>(
        testCtx, name, std::optional<CreateChildrenCallback>(createChildren), std::nullopt);
}

template <class CreateChildrenCallback, class CleanupGroupCallback>
inline tcu::TestCaseGroup *createTestGroup2(tcu::TestContext &testCtx, const std::string &name,
                                            CreateChildrenCallback createChildren, CleanupGroupCallback cleanupGroup)
{
    return new TestGroupHelper0_T<CreateChildrenCallback, CleanupGroupCallback>(
        testCtx, name, std::optional<CreateChildrenCallback>(createChildren),
        std::optional<CleanupGroupCallback>(cleanupGroup));
}

template <typename Arg0>
tcu::TestCaseGroup *createTestGroup(tcu::TestContext &testCtx, const std::string &name,
                                    typename TestGroupHelper1<Arg0>::CreateChildrenFunc createChildren, Arg0 arg0,
                                    typename TestGroupHelper1<Arg0>::CleanupGroupFunc cleanupGroup = nullptr)
{
    return new TestGroupHelper1<Arg0>(testCtx, name, createChildren, arg0, cleanupGroup);
}
template <typename Arg0, typename Arg1>
tcu::TestCaseGroup *createTestGroup(tcu::TestContext &testCtx, const std::string &name,
                                    typename TestGroupHelper2<Arg0, Arg1>::CreateChildrenFunc createChildren, Arg0 arg0,
                                    Arg1 arg1,
                                    typename TestGroupHelper2<Arg0, Arg1>::CleanupGroupFunc cleanupGroup = nullptr)
{
    return new TestGroupHelper2<Arg0, Arg1>(testCtx, name, createChildren, arg0, arg1, cleanupGroup);
}

inline void addTestGroup(tcu::TestCaseGroup *parent, const std::string &name,
                         TestGroupHelper0::CreateChildrenFunc createChildren)
{
    parent->addChild(createTestGroup(parent->getTestContext(), name, createChildren));
}

template <typename Arg0>
void addTestGroup(tcu::TestCaseGroup *parent, const std::string &name,
                  typename TestGroupHelper1<Arg0>::CreateChildrenFunc createChildren, Arg0 arg0,
                  typename TestGroupHelper1<Arg0>::CleanupGroupFunc cleanupGroup = nullptr)
{
    parent->addChild(createTestGroup<Arg0>(parent->getTestContext(), name, createChildren, arg0, cleanupGroup));
}

template <typename Arg0, typename Arg1>
void addTestGroup(tcu::TestCaseGroup *parent, const std::string &name,
                  typename TestGroupHelper2<Arg0, Arg1>::CreateChildrenFunc createChildren, Arg0 arg0, Arg1 arg1,
                  typename TestGroupHelper2<Arg0, Arg1>::CleanupGroupFunc cleanupGroup = nullptr)
{
    parent->addChild(
        createTestGroup<Arg0, Arg1>(parent->getTestContext(), name, createChildren, arg0, arg1, cleanupGroup));
}

template <typename InitFunc>
void addTestGroup2(tcu::TestCaseGroup *parent, const std::string &name, InitFunc initFunc)
{
    parent->addChild(createTestGroup2(parent->getTestContext(), name, initFunc));
}

template <typename InitFunc, typename CleanupFunc>
void addTestGroup2(tcu::TestCaseGroup *parent, const std::string &name, InitFunc initFunc, CleanupFunc cleanupFunc)
{
    parent->addChild(createTestGroup2(parent->getTestContext(), name, initFunc, cleanupFunc));
}

} // namespace vkt

#endif // _VKTTESTGROUPUTIL_HPP
