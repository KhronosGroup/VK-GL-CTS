# Manual for bottleneck_report.py script

## Script name and location

| name     | bottleneck_report.py     |
| location | VKGLCTS_ROOT/scripts/log |

## Description

The script parses qpa report file and produces an output containing three lists:

1. A list of single tests sorted descending by the duration of execution. On this list one can
find exceptionally lenghty tests.

2. A list of groups of tests sorted descending by their total duration of execution. This list
can be used to spot a groups that either contain a large amount of tests or multiple slow tests.

3. A list of groups of tests sorted descending by an average duration of the test in this group
(a total duration of group divided by number of tests in the group). Presents groups containing
slow tests.

This script requires that qpa file contains tests timing data (one created with sufficiently new
version of cts framework).

## Usage

`python path/to/bottleneck_report.py TESTLOG COUNT`

`TESTLOG` - a fully qualified path to read-accessible qpa report file.
`COUNT` - number of entries output in each section.

## Example

`python bottleneck_report.py c:\khronos\build\external\vulkancts\modules\vulkan\TestResults.qpa 5`

Produces following output:

```
The biggest tests time of execution
Index	Time		Full name
1		624010		dEQP-VK.subgroups.ballot_other.subgroupballotfindmsb_tess_control
2		614621		dEQP-VK.subgroups.shuffle.subgroupshuffle_int_tess_control
3		549420		dEQP-VK.subgroups.quad.subgroupquadbroadcast_1_int_tess_control
4		532983		dEQP-VK.subgroups.ballot_other.subgroupballotinclusivebitcount_tess_control
5		524019		dEQP-VK.subgroups.quad.subgroupquadbroadcast_0_int_tess_control

Groups Statistics
Total time of execution: 758611214
Number of executed tests: 4935

The biggest total time of execution
Index	Time		Test count	Full name
1		324242753	2100		dEQP-VK.subgroups.arithmetic
2		137952758	980			dEQP-VK.subgroups.quad
3		124482580	700			dEQP-VK.subgroups.clustered
4		82749504	560			dEQP-VK.subgroups.shuffle
5		49100267	287			dEQP-VK.subgroups.ballot_broadcast

The biggest time of execution per test
Index	Time		Test count	Avg. test time	Full name
1		124482580	700			177832			dEQP-VK.subgroups.clustered
2		49100267	287			171081			dEQP-VK.subgroups.ballot_broadcast
3		324242753	2100		154401			dEQP-VK.subgroups.arithmetic
4		82749504	560			147766			dEQP-VK.subgroups.shuffle
5		1992289		14			142306			dEQP-VK.subgroups.shape
```
