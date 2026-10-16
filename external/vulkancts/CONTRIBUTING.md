# Contributing to Vulkan CTS: Architecture and Practical Advice

There are 3 main types of objects in the Vulkan CTS:

* Test Groups
* Test Cases
* Test Instances

Tests in the Vulkan CTS are organized in a tree. The depth of the tree varies
depending on the branch. Intermediate nodes in the tree are Test Groups, while
leaf nodes are Test Cases. The child nodes of a test group must all be either
Test Groups or Test Cases, but not both.

A test name, like `dEQP-VK.api.smoke.triangle`, encodes how to get to the test
case from the root node, with the dots being used as separators. `dEQP-VK` is
the root Test Group and `api` is one if its child Test Groups. `smoke` is a
child Test Group of `api`, and `triangle` is one of the Test Cases contained in
it.

To run a test, a new Test Instance object is created and its `iterate` method is
called. The `iterate` method returns a test status that is typically "pass" or
"fail". Since the Test Instance is short-lived and is destroyed after `iterate`,
it should be the one creating and destroying resources needed to run the test
(images, buffers, pipelines, etc).

## Object Wrappers and Helpers

The Vulkan CTS framework provides a large amount of helpers to create Vulkan
handles of different types, and to query the implementation about properties,
features or extensions. Most of these helpers return Vulkan handles wrapped in
`Move<T>` objects, where T is the type of handle. These `Move` objects own the
handle, provide a destructor that calls the right function to destroy the
object, and move the handle on assignments. For example:

* `createSampler` from `vkRefUtil.hpp` returns a `Move<VkSampler>`.
* `Move<VkSampler>` calls `vkDestroySampler` in its destructor.

In addition, there are many helpers for frequent operations. For example,
`BufferWithMemory` creates a buffer and allocates and binds memory to it in
a single step.

## Overview of Test Instances

See `vkt::TestInstance` in `vktTestCase.hpp`. When implementing a new test,
subclass `vkt::TestInstance` and provide an implementation of its `iterate`
method. When implementing a new group of tests in which all the tests are
variations of one another, a parameters structure is often used, passed as a
constructor argument and stored in the instance.

The status returned from `iterate` is typically built with
`tcu::TestStatus::pass` or `tcu::TestStatus::fail`, but there are a few other
options. For example, the test status can also be thrown as an exception, which
is useful to fail a test from a function that is deep in the call stack.

If failing, using the `TCU_FAIL(...)` macro is also useful because it will print
the line and file where the exception was thrown from.

Please avoid doing feature checks inside this method. Feature checks should be
done at `checkSupport` time in the `vkt::TestCase` object.

Be mindful of the amount of CPU and memory used by the test when running
iterate, and of the time it takes to run each single test. In general, shorter
and simpler tests are preferred over long tests that do many things and may be
harder to debug.

## Overview of Test Cases

See `vkt::TestCase` in `vktTestCase.hpp`. Test Case objects are part of the tree
described above and live longer, in general, than Test Instances. Exactly how
much longer depends on its parent Test Group and the tree traversal logic. See
below for more details. A Test Case has 3 main methods that are frequently
overriden in subclasses:

* `checkSupport`: it receives a context as its argument and needs to check if
  the implementation supports everything the test needs. This is one of the
  first methods called when executing a test. To speed up tree traversal,
  nothing major needs to happen before this method is called. In particular, it
  is generally a bad idea to generate large amounts of data or perform long
  computations before `checkSupport` is called (for example, during the
  constructor or before the constructor of a Test Case object). Test Case
  objects should be small and easy to construct so the cost paid before
  `checkSupport` is minimal. If the implementation does not support something
  needed to run the test, `TCU_THROW(NotSupportedError, ...)` can be used to
  throw an exception and quickly finish running the test.

* `initPrograms`: for tests needing shaders, they can be generated inside this
  method. The shaders will be later available as SPIR-V binaries from the
  context passed to the Test Instance inside their `iterate` method.

* `createInstance`: method that returns a newly-allocated Test Instance to run
  the test. It will be deleted after its `iterate` method returns a complete
  result.

The design does not force any way of pairing Test Cases and Instances. Multiple
Test Case types may return a single Test Instance type from their
`createInstance` methods, or a single Test Case type may return different Test
Instance types depending on the test parameters.

It is somewhat frequent to create a single new Test Case subclass and a single
new Test Instance subclass when writing new tests, and for both to receive the
same type of parameters structure in their constructors (or an `std::shared_ptr`
to it), and vary their behavior depending on those parameters.

## Overview of Test Groups

Test Groups are often overlooked but they play a major role in deciding how fast
traversing the tree is in some situations and how much memory and CPU the final
programs will use for some operations. A frequent mistake is immediately
creating all the groups, subgroups and test cases of a subtree when asked to
create the subtree root, instead of delegating that work to the `init` method of
each subgroup. Test Groups have 3 main methods worth mentioning:

* `init`: this method will be called when entering a Test Group to do something
  with its children. Since child nodes will not be obtained until after this
  method is called, it should be used to create the child nodes of every group.
  By default, its implementation is empty.

* `deinit`: this method will be called when leaving a Test Group after the tree
  traversal logic is done with it. By default, it destroys child nodes and
  clears the vector of children. If overriden, subclasses should not forget to
  call the `deinit` method from the parent class at the end of their own
  `deinit` logic.

* `addChild`: this method adds a new child node to a test group. Child nodes can
  be Test Cases or Test Groups, but not both in the same group. In general,
  `addChild` should only be called from the group's `init` method.

## Helpers and Convenience Functions for Groups, Cases and Instances

`vktTestCaseUtil.hpp` and `vktTestGroupUtil.hpp` contain helper template
functions that can make creating new test cases and groups easier.

`addFunctionCase` and `addFunctionCaseWithPrograms` (both having multiple
variants) allow easily creating new test cases without needing to subclass Test
Case and Test Instance. They typically need callback functions equivalent to the
`checkSupport`, `initPrograms` and `iterate` methods (these callbacks will be
run from those methods). When using a parameters structure, the callbacks will
receive it as an additional argument by value. If the structure is big, consider
passing an `std::shared_ptr` to it instead.

`addTestGroup`, `createTestGroup` and `createTestGroup2` are similar for new
test groups, letting us override `init` and `deinit` on test groups without
needing to explicitly subclass Test Group. They need callback functions that
will be called from those methods.

In particular, `createTestGroup2` accepts C++ lambdas with captures as
callbacks. As lambdas can be defined inline, this is convenient when generating
large hierarchies of tests using combinations of test parameter values. To
create those combinations, we would typically use a number of nested loops, and
the lambdas can be arranged in code to look somewhat similar. Note these
callbacks will be called from a different scope to the one they are defined in,
so they need to capture anything they need by value. Please consider using
`std::shared_ptr` if the captured values are large.
