/*
 * Copyright (C) 2014 The Android Open Source Project
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
 */
package com.drawelements.deqp.runner;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.targetprep.IncrementalDeqpPreparer;
import com.android.ddmlib.AdbCommandRejectedException;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.ddmlib.MultiLineReceiver;
import com.android.ddmlib.ShellCommandUnresponsiveException;
import com.android.ddmlib.TimeoutException;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.error.HarnessRuntimeException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.error.TestErrorIdentifier;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IRuntimeHintProvider;
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.testtype.ITestCollector;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunInterruptedException;
import com.android.tradefed.util.RunUtil;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test runner for dEQP tests
 *
 * Supports running drawElements Quality Program tests found under external/deqp.
 */
@OptionClass(alias="deqp-test-runner")
public class DeqpTestRunner implements IBuildReceiver, IDeviceTest,
        ITestFilterReceiver, IAbiReceiver, IShardableTest, ITestCollector,
        IRuntimeHintProvider {
    private static final String DEQP_ONDEVICE_APK = "com.drawelements.deqp.apk";
    private static final String DEQP_ONDEVICE_PKG = "com.drawelements.deqp";
    private static final String INCOMPLETE_LOG_MESSAGE = "Crash: Incomplete test log";
    private static final String SKIPPED_INSTANCE_LOG_MESSAGE = "Configuration skipped";
    private static final String NOT_EXECUTABLE_LOG_MESSAGE = "Abort: Test cannot be executed";
    private static final String APP_DIR = "/sdcard/";
    private static final String CASE_LIST_FILE_NAME = "dEQP-TestCaseList.txt";
    private static final String LOG_FILE_NAME = "TestLog.qpa";
    public static final String FEATURE_LANDSCAPE = "android.hardware.screen.landscape";
    public static final String FEATURE_PORTRAIT = "android.hardware.screen.portrait";
    public static final String FEATURE_VULKAN_LEVEL = "android.hardware.vulkan.level";
    public static final String FEATURE_VULKAN_DEQP_LEVEL = "android.software.vulkan.deqp.level";
    public static final String FEATURE_OPENGLES_DEQP_LEVEL = "android.software.opengles.deqp.level";

    private static final int TESTCASE_BATCH_LIMIT = 1000;
    private static final int TESTCASE_BATCH_LIMIT_LARGE = 10000;
    private static final int UNRESPONSIVE_CMD_TIMEOUT_MS = 10 * 60 * 1000; // 10min

    private static final String ANGLE_NONE = "none";
    private static final String ANGLE_VULKAN = "vulkan";
    private static final String ANGLE_OPENGLES = "opengles";

    // !NOTE: There's a static method copyOptions() for copying options during split.
    // If you add state update copyOptions() as appropriate!

    @Option(name="deqp-package",
            description="Name of the deqp module used. Determines GLES version.",
            importance=Option.Importance.ALWAYS)
    private String mDeqpPackage;
    @Option(name="deqp-gl-config-name",
            description="GL render target config. See deqp documentation for syntax. ",
            importance=Option.Importance.NEVER)
    private String mConfigName = "";
    @Option(name="deqp-caselist-file",
            description="File listing the names of the cases to be run.",
            importance=Option.Importance.ALWAYS)
    private String mCaselistFile;
    @Option(name="deqp-screen-rotation",
            description="Screen orientation. Defaults to 'unspecified'",
            importance=Option.Importance.NEVER)
    private String mScreenRotation = "unspecified";
    @Option(name="deqp-surface-type",
            description="Surface type ('window', 'pbuffer', 'fbo'). Defaults to 'window'",
            importance=Option.Importance.NEVER)
    private String mSurfaceType = "window";
    @Option(name="deqp-config-required",
            description="Is current config required if API is supported? Defaults to false.",
            importance=Option.Importance.NEVER)
    private boolean mConfigRequired = false;
    @Option(name = "include-filter",
            description="Test include filter. '*' is zero or more letters. '.' has no special meaning.")
    private List<String> mIncludeFilters = new ArrayList<>();
    @Option(name = "include-filter-file",
            description="Load list of includes from the files given.")
    private List<String> mIncludeFilterFiles = new ArrayList<>();
    @Option(name = "exclude-filter",
            description="Test exclude filter. '*' is zero or more letters. '.' has no special meaning.")
    private List<String> mExcludeFilters = new ArrayList<>();
    @Option(name = "exclude-filter-file",
            description="Load list of excludes from the files given.")
    private List<String> mExcludeFilterFiles = new ArrayList<>();
    @Option(name = "incremental-deqp-include-file",
            description="Load list of includes from the files given for incremental dEQP.")
    private List<String> mIncrementalDeqpIncludeFiles = new ArrayList<>();
    @Option(name = "collect-tests-only",
            description = "Only invoke the instrumentation to collect list of applicable test "
                    + "cases. All test run callbacks will be triggered, but test execution will "
                    + "not be actually carried out.")
    private boolean mCollectTestsOnly = false;
    @Option(name = "runtime-hint",
            isTimeVal = true,
            description="The estimated config runtime. Defaults to 200ms x num tests.")
    private long mRuntimeHint = -1;

    @Option(name="deqp-use-angle",
            description="ANGLE backend ('none', 'vulkan', 'opengles'). Defaults to 'none' (don't use ANGLE)",
            importance=Option.Importance.NEVER)
    private String mAngle = "none";

    @Option(
            name = "disable-watchdog",
            description =
                    "Disable the native testrunner's per-test watchdog.")
    private boolean mDisableWatchdog = false;

    private Collection<TestDescription> mRemainingTests = null;
    private Map<TestDescription, Set<BatchRunConfiguration>> mTestInstances = null;
    private final TestInstanceResultListener mInstanceListerner = new TestInstanceResultListener();
    private final Map<TestDescription, Integer> mTestInstabilityRatings = new HashMap<>();
    private IAbi mAbi;
    private CompatibilityBuildHelper mBuildHelper;
    private boolean mLogData = false;
    private ITestDevice mDevice;
    private Map<String, Optional<Integer>> mDeviceFeatures;
    private Map<String, Boolean> mConfigQuerySupportCache = new HashMap<>();
    private IRunUtil mRunUtil = RunUtil.getDefault();
    private Set<String> mIncrementalDeqpIncludeTests = new HashSet<>();

    private IRecovery mDeviceRecovery = new Recovery(); {
        mDeviceRecovery.setSleepProvider(new SleepProvider());
    }

    public DeqpTestRunner() {
    }

    private DeqpTestRunner(DeqpTestRunner optionTemplate,
                Map<TestDescription, Set<BatchRunConfiguration>> tests) {
        copyOptions(this, optionTemplate);
        mTestInstances = tests;
    }

    /**
     * @param abi the ABI to run the test on
     */
    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuild(IBuildInfo buildInfo) {
        setBuildHelper(new CompatibilityBuildHelper(buildInfo));
    }

    /**
     * Exposed for better mockability during testing. In real use, always flows from
     * setBuild() called by the framework
     */
    public void setBuildHelper(CompatibilityBuildHelper helper) {
        mBuildHelper = helper;
    }

    /**
     * Enable or disable raw dEQP test log collection.
     */
    public void setCollectLogs(boolean logData) {
        mLogData = logData;
    }

    /**
     * Get the deqp-package option contents.
     */
    public String getPackageName() {
        return mDeqpPackage;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /**
     * Set recovery handler.
     *
     * Exposed for unit testing.
     */
    public void setRecovery(IRecovery deviceRecovery) {
        mDeviceRecovery = deviceRecovery;
    }

    /**
     * Set IRunUtil.
     *
     * Exposed for unit testing.
     */
    public void setRunUtil(IRunUtil runUtil) {
        mRunUtil = runUtil;
    }

    private static final class CapabilityQueryFailureException extends Exception {
    }

    /**
     * dEQP test instance listerer and invocation result forwarded
     */
    private class TestInstanceResultListener {
        private ITestInvocationListener mSink;
        private BatchRunConfiguration mRunConfig;

        private TestDescription mCurrentTestId;
        private boolean mGotTestResult;
        private String mCurrentTestLog;

        private class PendingResult {
            boolean allInstancesPassed;
            Map<BatchRunConfiguration, String> testLogs;
            Map<BatchRunConfiguration, String> errorMessages;
            Set<BatchRunConfiguration> remainingConfigs;
        }

        private final Map<TestDescription, PendingResult> mPendingResults = new HashMap<>();

        public void setSink(ITestInvocationListener sink) {
            mSink = sink;
        }

        public void setCurrentConfig(BatchRunConfiguration runConfig) {
            mRunConfig = runConfig;
        }

        /**
         * Get currently processed test id, or null if not currently processing a test case
         */
        public TestDescription getCurrentTestId() {
            return mCurrentTestId;
        }

        /**
         * Forward result to sink
         */
        private void forwardFinalizedPendingResult(TestDescription testId) {
            if (mRemainingTests.contains(testId)) {
                final PendingResult result = mPendingResults.get(testId);

                mPendingResults.remove(testId);
                mRemainingTests.remove(testId);

                // Forward results to the sink
                mSink.testStarted(testId);

                // Test Log
                if (mLogData) {
                    for (Map.Entry<BatchRunConfiguration, String> entry :
                            result.testLogs.entrySet()) {
                        final ByteArrayInputStreamSource source
                                = new ByteArrayInputStreamSource(entry.getValue().getBytes());

                        mSink.testLog(testId.getClassName() + "." + testId.getTestName() + "@"
                                + entry.getKey().getId(), LogDataType.XML, source);

                        source.close();
                    }
                }

                // Error message
                if (!result.allInstancesPassed) {
                    final StringBuilder errorLog = new StringBuilder();

                    for (Map.Entry<BatchRunConfiguration, String> entry :
                            result.errorMessages.entrySet()) {
                        if (errorLog.length() > 0) {
                            errorLog.append('\n');
                        }
                        errorLog.append(String.format("=== with config %s ===\n",
                                entry.getKey().getId()));
                        errorLog.append(entry.getValue());
                    }

                    mSink.testFailed(testId, errorLog.toString());
                }

                final HashMap<String, Metric> emptyMap = new HashMap<>();
                mSink.testEnded(testId, emptyMap);
            }
        }

        /**
         * Declare existence of a test and instances
         */
        public void setTestInstances(TestDescription testId, Set<BatchRunConfiguration> configs) {
            // Test instances cannot change at runtime, ignore if we have already set this
            if (!mPendingResults.containsKey(testId)) {
                final PendingResult pendingResult = new PendingResult();
                pendingResult.allInstancesPassed = true;
                pendingResult.testLogs = new LinkedHashMap<>();
                pendingResult.errorMessages = new LinkedHashMap<>();
                pendingResult.remainingConfigs = new HashSet<>(configs); // avoid mutating argument
                mPendingResults.put(testId, pendingResult);
            }
        }

        /**
         * Query if test instance has not yet been executed
         */
        public boolean isPendingTestInstance(TestDescription testId,
                BatchRunConfiguration config) {
            final PendingResult result = mPendingResults.get(testId);
            if (result == null) {
                // test is not in the current working batch of the runner, i.e. it cannot be
                // "partially" completed.
                if (!mRemainingTests.contains(testId)) {
                    // The test has been fully executed. Not pending.
                    return false;
                } else {
                    // Test has not yet been executed. Check if such instance exists
                    return mTestInstances.get(testId).contains(config);
                }
            } else {
                // could be partially completed, check this particular config
                return result.remainingConfigs.contains(config);
            }
        }

        /**
         * Fake execution of an instance with current config
         */
        public void skipTest(TestDescription testId) {
            final PendingResult result = mPendingResults.get(testId);

            result.errorMessages.put(mRunConfig, SKIPPED_INSTANCE_LOG_MESSAGE);
            result.remainingConfigs.remove(mRunConfig);

            // Pending result finished, report result
            if (result.remainingConfigs.isEmpty()) {
                forwardFinalizedPendingResult(testId);
            }
        }

        /**
         * Fake failure of an instance with current config
         */
        public void abortTest(TestDescription testId, String errorMessage) {
            final PendingResult result = mPendingResults.get(testId);

            // Mark as executed
            result.allInstancesPassed = false;
            result.errorMessages.put(mRunConfig, errorMessage);
            result.remainingConfigs.remove(mRunConfig);

            // Pending result finished, report result
            if (result.remainingConfigs.isEmpty()) {
                forwardFinalizedPendingResult(testId);
            }

            if (testId.equals(mCurrentTestId)) {
                mCurrentTestId = null;
            }
        }

        /**
         * Handles beginning of dEQP session.
         */
        private void handleBeginSession(Map<String, String> values) {
            // ignore
        }

        /**
         * Handles end of dEQP session.
         */
        private void handleEndSession(Map<String, String> values) {
            // ignore
        }

        /**
         * Handles beginning of dEQP testcase.
         */
        private void handleBeginTestCase(Map<String, String> values) {
            mCurrentTestId = pathToIdentifier(values.get("dEQP-BeginTestCase-TestCasePath"));
            mCurrentTestLog = "";
            mGotTestResult = false;

            // mark instance as started
            if (mPendingResults.get(mCurrentTestId) != null) {
                mPendingResults.get(mCurrentTestId).remainingConfigs.remove(mRunConfig);
            } else {
                CLog.w("Got unexpected start of %s", mCurrentTestId);
            }
        }

        /**
         * Handles end of dEQP testcase.
         */
        private void handleEndTestCase(Map<String, String> values) {
            final PendingResult result = mPendingResults.get(mCurrentTestId);

            if (result != null) {
                if (!mGotTestResult) {
                    result.allInstancesPassed = false;
                    result.errorMessages.put(mRunConfig, INCOMPLETE_LOG_MESSAGE);
                }

                if (mLogData && mCurrentTestLog != null && mCurrentTestLog.length() > 0) {
                    result.testLogs.put(mRunConfig, mCurrentTestLog);
                }

                // Pending result finished, report result
                if (result.remainingConfigs.isEmpty()) {
                    forwardFinalizedPendingResult(mCurrentTestId);
                }
            } else {
                CLog.w("Got unexpected end of %s", mCurrentTestId);
            }
            mCurrentTestId = null;
        }

        /**
         * Handles dEQP testcase result.
         */
        private void handleTestCaseResult(Map<String, String> values) {
            String code = values.get("dEQP-TestCaseResult-Code");
            String details = values.get("dEQP-TestCaseResult-Details");

            if (mPendingResults.get(mCurrentTestId) == null) {
                CLog.w("Got unexpected result for %s", mCurrentTestId);
                mGotTestResult = true;
                return;
            }

            if (code.compareTo("Pass") == 0) {
                mGotTestResult = true;
            } else if (code.compareTo("NotSupported") == 0) {
                mGotTestResult = true;
            } else if (code.compareTo("QualityWarning") == 0) {
                mGotTestResult = true;
            } else if (code.compareTo("CompatibilityWarning") == 0) {
                mGotTestResult = true;
            } else if (code.compareTo("Fail") == 0 || code.compareTo("ResourceError") == 0
                    || code.compareTo("InternalError") == 0 || code.compareTo("Crash") == 0
                    || code.compareTo("Timeout") == 0) {
                mPendingResults.get(mCurrentTestId).allInstancesPassed = false;
                mPendingResults.get(mCurrentTestId)
                        .errorMessages.put(mRunConfig, code + ": " + details);
                mGotTestResult = true;
            } else {
                String codeError = "Unknown result code: " + code;
                mPendingResults.get(mCurrentTestId).allInstancesPassed = false;
                mPendingResults.get(mCurrentTestId)
                        .errorMessages.put(mRunConfig, codeError + ": " + details);
                mGotTestResult = true;
            }
        }

        /**
         * Handles terminated dEQP testcase.
         */
        private void handleTestCaseTerminate(Map<String, String> values) {
            final PendingResult result = mPendingResults.get(mCurrentTestId);

            if (result != null) {
                String reason = values.get("dEQP-TerminateTestCase-Reason");
                mPendingResults.get(mCurrentTestId).allInstancesPassed = false;
                mPendingResults.get(mCurrentTestId)
                        .errorMessages.put(mRunConfig, "Terminated: " + reason);

                // Pending result finished, report result
                if (result.remainingConfigs.isEmpty()) {
                    forwardFinalizedPendingResult(mCurrentTestId);
                }
            } else {
                CLog.w("Got unexpected termination of %s", mCurrentTestId);
            }

            mCurrentTestId = null;
            mGotTestResult = true;
        }

        /**
         * Handles dEQP testlog data.
         */
        private void handleTestLogData(Map<String, String> values) {
            mCurrentTestLog = mCurrentTestLog + values.get("dEQP-TestLogData-Log");
        }

        /**
         * Handles new instrumentation status message.
         */
        public void handleStatus(Map<String, String> values) {
            String eventType = values.get("dEQP-EventType");

            if (eventType == null) {
                return;
            }

            if (eventType.compareTo("BeginSession") == 0) {
                handleBeginSession(values);
            } else if (eventType.compareTo("EndSession") == 0) {
                handleEndSession(values);
            } else if (eventType.compareTo("BeginTestCase") == 0) {
                handleBeginTestCase(values);
            } else if (eventType.compareTo("EndTestCase") == 0) {
                handleEndTestCase(values);
            } else if (eventType.compareTo("TestCaseResult") == 0) {
                handleTestCaseResult(values);
            } else if (eventType.compareTo("TerminateTestCase") == 0) {
                handleTestCaseTerminate(values);
            } else if (eventType.compareTo("TestLogData") == 0) {
                handleTestLogData(values);
            }
        }

        /**
         * Signal listener that batch ended and forget incomplete results.
         */
        public void endBatch() {
            // end open test if when stream ends
            if (mCurrentTestId != null) {
                // Current instance was removed from remainingConfigs when case
                // started. Mark current instance as pending.
                if (mPendingResults.get(mCurrentTestId) != null) {
                    mPendingResults.get(mCurrentTestId).remainingConfigs.add(mRunConfig);
                } else {
                    CLog.w("Got unexpected internal state of %s", mCurrentTestId);
                }
            }
            mCurrentTestId = null;
        }
    }

    /**
     * dEQP instrumentation parser
     */
    private static class InstrumentationParser extends MultiLineReceiver {
        private TestInstanceResultListener mListener;

        private Map<String, String> mValues;
        private String mCurrentName;
        private String mCurrentValue;
        private int mResultCode;
        private boolean mGotExitValue = false;


        public InstrumentationParser(TestInstanceResultListener listener) {
            mListener = listener;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void processNewLines(String[] lines) {
            for (String line : lines) {
                if (mValues == null) mValues = new HashMap<String, String>();

                if (line.startsWith("INSTRUMENTATION_STATUS_CODE: ")) {
                    if (mCurrentName != null) {
                        mValues.put(mCurrentName, mCurrentValue);

                        mCurrentName = null;
                        mCurrentValue = null;
                    }

                    mListener.handleStatus(mValues);
                    mValues = null;
                } else if (line.startsWith("INSTRUMENTATION_STATUS: dEQP-")) {
                    if (mCurrentName != null) {
                        mValues.put(mCurrentName, mCurrentValue);

                        mCurrentValue = null;
                        mCurrentName = null;
                    }

                    String prefix = "INSTRUMENTATION_STATUS: ";
                    int nameBegin = prefix.length();
                    int nameEnd = line.indexOf('=');
                    int valueBegin = nameEnd + 1;

                    mCurrentName = line.substring(nameBegin, nameEnd);
                    mCurrentValue = line.substring(valueBegin);
                } else if (line.startsWith("INSTRUMENTATION_CODE: ")) {
                    try {
                        mResultCode = Integer.parseInt(line.substring(22));
                        mGotExitValue = true;
                    } catch (NumberFormatException ex) {
                        CLog.w("Instrumentation code format unexpected");
                    }
                } else if (mCurrentValue != null) {
                    mCurrentValue = mCurrentValue + line;
                }
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void done() {
            if (mCurrentName != null) {
                mValues.put(mCurrentName, mCurrentValue);

                mCurrentName = null;
                mCurrentValue = null;
            }

            if (mValues != null) {
                mListener.handleStatus(mValues);
                mValues = null;
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean isCancelled() {
            return false;
        }

        /**
         * Returns whether target instrumentation exited normally.
         */
        public boolean wasSuccessful() {
            return mGotExitValue;
        }

        /**
         * Returns Instrumentation return code
         */
        public int getResultCode() {
            return mResultCode;
        }
    }

    /**
     * dEQP platfom query instrumentation parser
     */
    private static class PlatformQueryInstrumentationParser extends MultiLineReceiver {
        private Map<String,String> mResultMap = new LinkedHashMap<>();
        private int mResultCode;
        private boolean mGotExitValue = false;

        /**
         * {@inheritDoc}
         */
        @Override
        public void processNewLines(String[] lines) {
            for (String line : lines) {
                if (line.startsWith("INSTRUMENTATION_RESULT: ")) {
                    final String parts[] = line.substring(24).split("=",2);
                    if (parts.length == 2) {
                        mResultMap.put(parts[0], parts[1]);
                    } else {
                        CLog.w("Instrumentation status format unexpected");
                    }
                } else if (line.startsWith("INSTRUMENTATION_CODE: ")) {
                    try {
                        mResultCode = Integer.parseInt(line.substring(22));
                        mGotExitValue = true;
                    } catch (NumberFormatException ex) {
                        CLog.w("Instrumentation code format unexpected");
                    }
                }
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean isCancelled() {
            return false;
        }

        /**
         * Returns whether target instrumentation exited normally.
         */
        public boolean wasSuccessful() {
            return mGotExitValue;
        }

        /**
         * Returns Instrumentation return code
         */
        public int getResultCode() {
            return mResultCode;
        }

        public Map<String,String> getResultMap() {
            return mResultMap;
        }
    }

    /**
     * Interface for sleeping.
     *
     * Exposed for unit testing
     */
    public static interface ISleepProvider {
        public void sleep(int milliseconds);
    }

    private static class SleepProvider implements ISleepProvider {
        @Override
        public void sleep(int milliseconds) {
            RunUtil.getDefault().sleep(milliseconds);
        }
    }

    /**
     * Interface for failure recovery.
     *
     * Exposed for unit testing
     */
    public static interface IRecovery {
        /**
         * Sets the sleep provider IRecovery works on
         */
        public void setSleepProvider(ISleepProvider sleepProvider);

        /**
         * Sets the device IRecovery works on
         */
        public void setDevice(ITestDevice device);

        /**
         * Informs Recovery that test execution has progressed since the last recovery
         */
        public void onExecutionProgressed();

        /**
         * Tries to recover device after failed refused connection.
         *
         * @throws DeviceNotAvailableException if recovery did not succeed
         */
        public void recoverConnectionRefused() throws DeviceNotAvailableException;

        /**
         * Tries to recover device after abnormal execution termination or link failure.
         *
         * @throws DeviceNotAvailableException if recovery did not succeed
         */
        public void recoverComLinkKilled() throws DeviceNotAvailableException;
    }

    /**
     * State machine for execution failure recovery.
     *
     * Exposed for unit testing
     */
    public static class Recovery implements IRecovery {
        private int RETRY_COOLDOWN_MS = 6000; // 6 seconds
        private int PROCESS_KILL_WAIT_MS = 1000; // 1 second

        private static enum MachineState {
            WAIT, // recover by waiting
            RECOVER, // recover by calling recover()
            REBOOT, // recover by rebooting
            FAIL, // cannot recover
        }

        private MachineState mState = MachineState.WAIT;
        private ITestDevice mDevice;
        private ISleepProvider mSleepProvider;

        private static class ProcessKillFailureException extends Exception {
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void setSleepProvider(ISleepProvider sleepProvider) {
            mSleepProvider = sleepProvider;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void setDevice(ITestDevice device) {
            mDevice = device;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void onExecutionProgressed() {
            mState = MachineState.WAIT;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void recoverConnectionRefused() throws DeviceNotAvailableException {
            switch (mState) {
                case WAIT: // not a valid stratedy for connection refusal, fallthrough
                case RECOVER:
                    // First failure, just try to recover
                    CLog.w("ADB connection failed, trying to recover");
                    mState = MachineState.REBOOT; // the next step is to reboot

                    try {
                        recoverDevice();
                    } catch (DeviceNotAvailableException ex) {
                        // chain forward
                        recoverConnectionRefused();
                    }
                    break;

                case REBOOT:
                    // Second failure in a row, try to reboot
                    CLog.w("ADB connection failed after recovery, rebooting device");
                    mState = MachineState.FAIL; // the next step is to fail

                    try {
                        rebootDevice();
                    } catch (DeviceNotAvailableException ex) {
                        // chain forward
                        recoverConnectionRefused();
                    }
                    break;

                case FAIL:
                    // Third failure in a row, just fail
                    CLog.w("Cannot recover ADB connection");
                    throw new DeviceNotAvailableException("failed to connect after reboot",
                            mDevice.getSerialNumber());
            }
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public void recoverComLinkKilled() throws DeviceNotAvailableException {
            switch (mState) {
                case WAIT:
                    // First failure, just try to wait and try again
                    CLog.w("ADB link failed, retrying after a cooldown period");
                    mState = MachineState.RECOVER; // the next step is to recover the device

                    waitCooldown();

                    // even if the link to deqp on-device process was killed, the process might
                    // still be alive. Locate and terminate such unwanted processes.
                    try {
                        killDeqpProcess();
                    } catch (DeviceNotAvailableException ex) {
                        // chain forward
                        recoverComLinkKilled();
                    } catch (ProcessKillFailureException ex) {
                        // chain forward
                        recoverComLinkKilled();
                    }
                    break;

                case RECOVER:
                    // Second failure, just try to recover
                    CLog.w("ADB link failed, trying to recover");
                    mState = MachineState.REBOOT; // the next step is to reboot

                    try {
                        recoverDevice();
                        killDeqpProcess();
                    } catch (DeviceNotAvailableException ex) {
                        // chain forward
                        recoverComLinkKilled();
                    } catch (ProcessKillFailureException ex) {
                        // chain forward
                        recoverComLinkKilled();
                    }
                    break;

                case REBOOT:
                    // Third failure in a row, try to reboot
                    CLog.w("ADB link failed after recovery, rebooting device");
                    mState = MachineState.FAIL; // the next step is to fail

                    try {
                        rebootDevice();
                    } catch (DeviceNotAvailableException ex) {
                        // chain forward
                        recoverComLinkKilled();
                    }
                    break;

                case FAIL:
                    // Fourth failure in a row, just fail
                    CLog.w("Cannot recover ADB connection");
                    throw new DeviceNotAvailableException("link killed after reboot",
                            mDevice.getSerialNumber());
            }
        }

        private void waitCooldown() {
            mSleepProvider.sleep(RETRY_COOLDOWN_MS);
        }

        private Iterable<Integer> getDeqpProcessPids() throws DeviceNotAvailableException {
            final List<Integer> pids = new ArrayList<Integer>(2);
            final String processes = mDevice.executeShellCommand("ps | grep com.drawelements");
            final String[] lines = processes.split("(\\r|\\n)+");
            for (String line : lines) {
                final String[] fields = line.split("\\s+");
                if (fields.length < 2) {
                    continue;
                }

                try {
                    final int processId = Integer.parseInt(fields[1], 10);
                    pids.add(processId);
                } catch (NumberFormatException ex) {
                    continue;
                }
            }
            return pids;
        }

        private void killDeqpProcess() throws DeviceNotAvailableException,
                ProcessKillFailureException {
            for (Integer processId : getDeqpProcessPids()) {
                mDevice.executeShellCommand(String.format("kill -9 %d", processId));
            }

            mSleepProvider.sleep(PROCESS_KILL_WAIT_MS);

            // check that processes actually died
            if (getDeqpProcessPids().iterator().hasNext()) {
                // a process is still alive, killing failed
                throw new ProcessKillFailureException();
            }
        }

        public void recoverDevice() throws DeviceNotAvailableException {
            ((IManagedTestDevice) mDevice).recoverDevice();
        }

        private void rebootDevice() throws DeviceNotAvailableException {
            mDevice.reboot();
        }
    }


    private static void addTestsToInstancesMap(
        File testlist,
        String configName,
        String screenRotation,
        String surfaceType,
        boolean required,
        Map<TestDescription, Set<BatchRunConfiguration>> instances) {

        try (final FileReader testlistInnerReader = new FileReader(testlist);
             final BufferedReader testlistReader = new BufferedReader(testlistInnerReader)) {

            String testName;
            while ((testName = testlistReader.readLine()) != null) {
                testName = testName.trim();

                // Skip empty lines.
                if (testName.isEmpty()) {
                    continue;
                }

                // Lines starting with "#" are comments.
                if (testName.startsWith("#")) {
                    continue;
                }

                // If the "testName" ends with .txt, then it is a path to another test list
                // (relative to the current test list, path separator is "/") that we need to
                // read.
                if (testName.endsWith(".txt")) {
                    addTestsToInstancesMap(
                        Paths.get(testlist.getParent(), testName.split("/")).toFile(),
                        configName,
                        screenRotation,
                        surfaceType,
                        required,
                        instances);
                    continue;
                }

                // Test name -> testId -> only one config -> done.
                final Set<BatchRunConfiguration> testInstanceSet = new LinkedHashSet<>();
                BatchRunConfiguration config = new BatchRunConfiguration(configName, screenRotation, surfaceType, required);
                testInstanceSet.add(config);
                TestDescription test = pathToIdentifier(testName);
                instances.put(test, testInstanceSet);
            }
        }
        catch (IOException e)
        {
            throw new RuntimeException("Failure while reading the test case list for deqp: " + e.getMessage());
        }
    }

    private Set<BatchRunConfiguration> getTestRunConfigs(TestDescription testId) {
        return mTestInstances.get(testId);
    }

    /**
     * Get the test instance of the runner. Exposed for testing.
     */
    Map<TestDescription, Set<BatchRunConfiguration>> getTestInstance() {
        return mTestInstances;
    }

    /**
     * Converts dEQP testcase path to TestDescription.
     */
    private static TestDescription pathToIdentifier(String testPath) {
        int indexOfLastDot = testPath.lastIndexOf('.');
        String className = testPath.substring(0, indexOfLastDot);
        String testName = testPath.substring(indexOfLastDot+1);

        return new TestDescription(className, testName);
    }

    // \todo [2015-10-16 kalle] How unique should this be?
    private String getId() {
        return AbiUtils.createId(mAbi.getName(), mDeqpPackage);
    }

    /**
     * Generates tescase trie from dEQP testcase paths. Used to define which testcases to execute.
     */
    private static String generateTestCaseTrieFromPaths(Collection<String> tests) {
        String result = "{";
        boolean first = true;

        // Add testcases to results
        for (Iterator<String> iter = tests.iterator(); iter.hasNext();) {
            String test = iter.next();
            String[] components = test.split("\\.");

            if (components.length == 1) {
                if (!first) {
                    result = result + ",";
                }
                first = false;

                result += components[0];
                iter.remove();
            }
        }

        if (!tests.isEmpty()) {
            HashMap<String, ArrayList<String> > testGroups = new HashMap<>();

            // Collect all sub testgroups
            for (String test : tests) {
                String[] components = test.split("\\.");
                ArrayList<String> testGroup = testGroups.get(components[0]);

                if (testGroup == null) {
                    testGroup = new ArrayList<String>();
                    testGroups.put(components[0], testGroup);
                }

                testGroup.add(test.substring(components[0].length()+1));
            }

            for (String testGroup : testGroups.keySet()) {
                if (!first) {
                    result = result + ",";
                }

                first = false;
                result = result + testGroup
                        + generateTestCaseTrieFromPaths(testGroups.get(testGroup));
            }
        }

        return result + "}";
    }

    /**
     * Generates testcase trie from TestDescriptions.
     */
    private static String generateTestCaseTrie(Collection<TestDescription> tests) {
        ArrayList<String> testPaths = new ArrayList<String>();

        for (TestDescription test : tests) {
            testPaths.add(test.getClassName() + "." + test.getTestName());
        }

        return generateTestCaseTrieFromPaths(testPaths);
    }

    private static class TestBatch {
        public BatchRunConfiguration config;
        public List<TestDescription> tests;
    }

    /**
     * Creates a TestBatch from the given tests or null if not tests remaining.
     *
     *  @param pool List of tests to select from
     *  @param requiredConfig Select only instances with pending requiredConfig, or null to select
     *         any run configuration.
     */
    private TestBatch selectRunBatch(Collection<TestDescription> pool,
            BatchRunConfiguration requiredConfig) {
        // select one test (leading test) that is going to be executed and then pack along as many
        // other compatible instances as possible.

        TestDescription leadingTest = null;
        for (TestDescription test : pool) {
            if (!mRemainingTests.contains(test)) {
                continue;
            }
            if (requiredConfig != null &&
                    !mInstanceListerner.isPendingTestInstance(test, requiredConfig)) {
                continue;
            }
            leadingTest = test;
            break;
        }

        // no remaining tests?
        if (leadingTest == null) {
            return null;
        }

        BatchRunConfiguration leadingTestConfig = null;
        if (requiredConfig != null) {
            leadingTestConfig = requiredConfig;
        } else {
            for (BatchRunConfiguration runConfig : getTestRunConfigs(leadingTest)) {
                if (mInstanceListerner.isPendingTestInstance(leadingTest, runConfig)) {
                    leadingTestConfig = runConfig;
                    break;
                }
            }
        }

        // test pending <=> test has a pending config
        if (leadingTestConfig == null) {
            throw new AssertionError("search postcondition failed");
        }

        final int leadingInstability = getTestInstabilityRating(leadingTest);

        final TestBatch runBatch = new TestBatch();
        runBatch.config = leadingTestConfig;
        runBatch.tests = new ArrayList<>();
        runBatch.tests.add(leadingTest);

        for (TestDescription test : pool) {
            if (test == leadingTest) {
                // do not re-select the leading tests
                continue;
            }
            if (!mInstanceListerner.isPendingTestInstance(test, leadingTestConfig)) {
                // select only compatible
                continue;
            }
            if (getTestInstabilityRating(test) != leadingInstability) {
                // pack along only cases in the same stability category. Packing more dangerous
                // tests along jeopardizes the stability of this run. Packing more stable tests
                // along jeopardizes their stability rating.
                continue;
            }
            if (runBatch.tests.size() >= getBatchSizeLimitForInstability(leadingInstability)) {
                // batch size is limited.
                break;
            }
            runBatch.tests.add(test);
        }

        return runBatch;
    }

    private int getBatchSizeLimit() {
        if (isIncrementalDeqpRun()) {
            return TESTCASE_BATCH_LIMIT_LARGE;
        }
        return TESTCASE_BATCH_LIMIT;
    }

    private int getBatchNumPendingCases(TestBatch batch) {
        int numPending = 0;
        for (TestDescription test : batch.tests) {
            if (mInstanceListerner.isPendingTestInstance(test, batch.config)) {
                ++numPending;
            }
        }
        return numPending;
    }

    private int getBatchSizeLimitForInstability(int batchInstabilityRating) {
        // reduce group size exponentially down to one
        return Math.max(1, getBatchSizeLimit() / (1 << batchInstabilityRating));
    }

    private int getTestInstabilityRating(TestDescription testId) {
        if (mTestInstabilityRatings.containsKey(testId)) {
            return mTestInstabilityRatings.get(testId);
        } else {
            return 0;
        }
    }

    private void recordTestInstability(TestDescription testId) {
        mTestInstabilityRatings.put(testId, getTestInstabilityRating(testId) + 1);
    }

    private void clearTestInstability(TestDescription testId) {
        mTestInstabilityRatings.put(testId, 0);
    }

    /**
     * Executes all tests on the device.
     */
    private void runTests() throws DeviceNotAvailableException, CapabilityQueryFailureException {
        for (;;) {
            TestBatch batch = selectRunBatch(mRemainingTests, null);

            if (batch == null) {
                break;
            }

            runTestRunBatch(batch);
        }
    }

    /**
     * Runs a TestBatch by either faking it or executing it on a device.
     */
    private void runTestRunBatch(TestBatch batch) throws DeviceNotAvailableException,
            CapabilityQueryFailureException {
        // prepare instance listener
        mInstanceListerner.setCurrentConfig(batch.config);
        for (TestDescription test : batch.tests) {
            mInstanceListerner.setTestInstances(test, getTestRunConfigs(test));
        }

        // When incremental dEQP is enabled, skip all tests except those in
        // mIncrementalDeqpIncludeTests
        if (isIncrementalDeqpRun()) {
            TestBatch skipBatch = new TestBatch();
            skipBatch.config = batch.config;
            skipBatch.tests = new ArrayList<>();
            TestBatch runBatch = new TestBatch();
            runBatch.config = batch.config;
            runBatch.tests = new ArrayList<>();
            for (TestDescription test : batch.tests) {
                if (mIncrementalDeqpIncludeTests.contains(test.getClassName() + "."
                      + test.getTestName())) {
                  runBatch.tests.add(test);
                } else {
                  skipBatch.tests.add(test);
                }
            }
            batch = runBatch;
            fakePassTestRunBatch(skipBatch);
            if (batch.tests.isEmpty()) {
                return;
            }
        }
        // execute only if config is executable, else fake results
        if (isSupportedRunConfiguration(batch.config)) {
            executeTestRunBatch(batch);
        } else {
            if (batch.config.isRequired()) {
                fakeFailTestRunBatch(batch);
            } else {
                fakePassTestRunBatch(batch);
            }
        }
    }

    private boolean isIncrementalDeqpRun() {
        IBuildInfo buildInfo = mBuildHelper.getBuildInfo();
        return buildInfo.getBuildAttributes().containsKey(
            IncrementalDeqpPreparer.INCREMENTAL_DEQP_ATTRIBUTE_NAME);
    }

    private boolean isSupportedRunConfiguration(BatchRunConfiguration runConfig)
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        // orientation support
        if (!BatchRunConfiguration.ROTATION_UNSPECIFIED.equals(runConfig.getRotation())) {
            final Map<String, Optional<Integer>> features = getDeviceFeatures(mDevice);

            if (isPortraitClassRotation(runConfig.getRotation()) &&
                    !features.containsKey(FEATURE_PORTRAIT)) {
                return false;
            }
            if (isLandscapeClassRotation(runConfig.getRotation()) &&
                    !features.containsKey(FEATURE_LANDSCAPE)) {
                return false;
            }
        }

        if (isOpenGlEsPackage()) {
            // renderability support for OpenGL ES tests
            return isSupportedGlesRenderConfig(runConfig);
        } else {
            return true;
        }
    }

    private static final class AdbComLinkOpenError extends Exception {
        public AdbComLinkOpenError(String description, Throwable inner) {
            super(description, inner);
        }
    }

    private static final class AdbComLinkKilledError extends Exception {
        public AdbComLinkKilledError(String description, Throwable inner) {
            super(description, inner);
        }
    }

    /**
     * Executes a given command in adb shell
     *
     * @throws AdbComLinkOpenError if connection cannot be established.
     * @throws AdbComLinkKilledError if established connection is killed prematurely.
     */
    private void executeShellCommandAndReadOutput(final String command,
            final IShellOutputReceiver receiver)
            throws AdbComLinkOpenError, AdbComLinkKilledError {
        try {
            mDevice.getIDevice().executeShellCommand(command, receiver,
                    UNRESPONSIVE_CMD_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (TimeoutException ex) {
            // Opening connection timed out
            throw new AdbComLinkOpenError("opening connection timed out", ex);
        } catch (AdbCommandRejectedException ex) {
            // Command rejected
            throw new AdbComLinkOpenError("command rejected", ex);
        } catch (IOException ex) {
            // shell command channel killed
            throw new AdbComLinkKilledError("command link killed", ex);
        } catch (ShellCommandUnresponsiveException ex) {
            // shell command halted
            throw new AdbComLinkKilledError("command link hung", ex);
        }
    }

    /**
     * Executes given test batch on a device
     */
    private void executeTestRunBatch(TestBatch batch) throws DeviceNotAvailableException {
        // attempt full run once
        executeTestRunBatchRun(batch);

        // split remaining tests to two sub batches and execute both. This will terminate
        // since executeTestRunBatchRun will always progress for a batch of size 1.
        final ArrayList<TestDescription> pendingTests = new ArrayList<>();

        for (TestDescription test : batch.tests) {
            if (mInstanceListerner.isPendingTestInstance(test, batch.config)) {
                pendingTests.add(test);
            }
        }

        final int divisorNdx = pendingTests.size() / 2;
        final List<TestDescription> headList = pendingTests.subList(0, divisorNdx);
        final List<TestDescription> tailList = pendingTests.subList(divisorNdx, pendingTests.size());

        // head
        for (;;) {
            TestBatch subBatch = selectRunBatch(headList, batch.config);

            if (subBatch == null) {
                break;
            }

            executeTestRunBatch(subBatch);
        }

        // tail
        for (;;) {
            TestBatch subBatch = selectRunBatch(tailList, batch.config);

            if (subBatch == null) {
                break;
            }

            executeTestRunBatch(subBatch);
        }

        if (getBatchNumPendingCases(batch) != 0) {
            throw new AssertionError("executeTestRunBatch postcondition failed");
        }
    }

    /**
     * Runs one execution pass over the given batch.
     *
     * Tries to run the batch. Always makes progress (executes instances or modifies stability
     * scores).
     */
    private void executeTestRunBatchRun(TestBatch batch) throws DeviceNotAvailableException {
        if (getBatchNumPendingCases(batch) != batch.tests.size()) {
            throw new AssertionError("executeTestRunBatchRun precondition failed");
        }

        checkInterrupted(); // throws if interrupted

        final String testCases = generateTestCaseTrie(batch.tests);
        final String testCaseFilename = APP_DIR + CASE_LIST_FILE_NAME;
        mDevice.executeShellCommand("rm " + testCaseFilename);
        mDevice.executeShellCommand("rm " + APP_DIR + LOG_FILE_NAME);
        if (!mDevice.pushString(testCases + "\n", testCaseFilename)) {
            throw new RuntimeException("Failed to write test cases to " + testCaseFilename);
        }

        final String instrumentationName =
                "com.drawelements.deqp/com.drawelements.deqp.testercore.DeqpInstrumentation";

        final StringBuilder deqpCmdLine = new StringBuilder();
        deqpCmdLine.append("--deqp-caselist-file=");
        deqpCmdLine.append(APP_DIR + CASE_LIST_FILE_NAME);
        deqpCmdLine.append(" ");
        deqpCmdLine.append(getRunConfigDisplayCmdLine(batch.config));

        // If we are not logging data, do not bother outputting the images from the test exe.
        if (!mLogData) {
            deqpCmdLine.append(" --deqp-log-images=disable");
        }

        if (!mDisableWatchdog) {
            deqpCmdLine.append(" --deqp-watchdog=enable");
        }

        final String command = String.format(
                "am instrument %s -w -e deqpLogFilename \"%s\" -e deqpCmdLine \"%s\""
                    + " -e deqpLogData \"%s\" %s",
                AbiUtils.createAbiFlag(mAbi.getName()), APP_DIR + LOG_FILE_NAME,
                deqpCmdLine.toString(), mLogData, instrumentationName);

        final int numRemainingInstancesBefore = getNumRemainingInstances();
        final InstrumentationParser parser = new InstrumentationParser(mInstanceListerner);
        Throwable interruptingError = null;

        try {
            executeShellCommandAndReadOutput(command, parser);
        } catch (Throwable ex) {
            interruptingError = ex;
        } finally {
            parser.flush();
        }

        final boolean progressedSinceLastCall = mInstanceListerner.getCurrentTestId() != null ||
                getNumRemainingInstances() < numRemainingInstancesBefore;

        if (progressedSinceLastCall) {
            mDeviceRecovery.onExecutionProgressed();
        }

        // interrupted, try to recover
        if (interruptingError != null) {
            if (interruptingError instanceof AdbComLinkOpenError) {
                mDeviceRecovery.recoverConnectionRefused();
            } else if (interruptingError instanceof AdbComLinkKilledError) {
                mDeviceRecovery.recoverComLinkKilled();
            } else if (interruptingError instanceof RunInterruptedException) {
                // external run interruption request. Terminate immediately.
                throw (RunInterruptedException)interruptingError;
            } else {
                CLog.e(interruptingError);
                throw new RuntimeException(interruptingError);
            }

            // recoverXXX did not throw => recovery succeeded
        } else if (!parser.wasSuccessful()) {
            mDeviceRecovery.recoverComLinkKilled();
            // recoverXXX did not throw => recovery succeeded
        }

        // Progress guarantees.
        if (batch.tests.size() == 1) {
            final TestDescription onlyTest = batch.tests.iterator().next();
            final boolean wasTestExecuted =
                    !mInstanceListerner.isPendingTestInstance(onlyTest, batch.config) &&
                    mInstanceListerner.getCurrentTestId() == null;
            final boolean wasLinkFailure = !parser.wasSuccessful() || interruptingError != null;

            // Link failures can be caused by external events, require at least two observations
            // until bailing.
            if (!wasTestExecuted && (!wasLinkFailure || getTestInstabilityRating(onlyTest) > 0)) {
                recordTestInstability(onlyTest);
                // If we cannot finish the test, mark the case as a crash.
                //
                // If we couldn't even start the test, fail the test instance as non-executable.
                // This is required so that a consistently crashing or non-existent tests will
                // not cause futile (non-terminating) re-execution attempts.
                if (mInstanceListerner.getCurrentTestId() != null) {
                    mInstanceListerner.abortTest(onlyTest, INCOMPLETE_LOG_MESSAGE);
                } else {
                    mInstanceListerner.abortTest(onlyTest, NOT_EXECUTABLE_LOG_MESSAGE);
                }
            } else if (wasTestExecuted) {
                clearTestInstability(onlyTest);
            }
        }
        else
        {
            // Analyze results to update test stability ratings. If there is no interrupting test
            // logged, increase instability rating of all remaining tests. If there is a
            // interrupting test logged, increase only its instability rating.
            //
            // A successful run of tests clears instability rating.
            if (mInstanceListerner.getCurrentTestId() == null) {
                for (TestDescription test : batch.tests) {
                    if (mInstanceListerner.isPendingTestInstance(test, batch.config)) {
                        recordTestInstability(test);
                    } else {
                        clearTestInstability(test);
                    }
                }
            } else {
                recordTestInstability(mInstanceListerner.getCurrentTestId());
                for (TestDescription test : batch.tests) {
                    // \note: isPendingTestInstance is false for getCurrentTestId. Current ID is
                    // considered 'running' and will be restored to 'pending' in endBatch().
                    if (!test.equals(mInstanceListerner.getCurrentTestId()) &&
                            !mInstanceListerner.isPendingTestInstance(test, batch.config)) {
                        clearTestInstability(test);
                    }
                }
            }
        }

        mInstanceListerner.endBatch();
    }

    private static String getRunConfigDisplayCmdLine(BatchRunConfiguration runConfig) {
        final StringBuilder deqpCmdLine = new StringBuilder();
        if (!runConfig.getGlConfig().isEmpty()) {
            deqpCmdLine.append("--deqp-gl-config-name=");
            deqpCmdLine.append(runConfig.getGlConfig());
        }
        if (!runConfig.getRotation().isEmpty()) {
            if (deqpCmdLine.length() != 0) {
                deqpCmdLine.append(" ");
            }
            deqpCmdLine.append("--deqp-screen-rotation=");
            deqpCmdLine.append(runConfig.getRotation());
        }
        if (!runConfig.getSurfaceType().isEmpty()) {
            if (deqpCmdLine.length() != 0) {
                deqpCmdLine.append(" ");
            }
            deqpCmdLine.append("--deqp-surface-type=");
            deqpCmdLine.append(runConfig.getSurfaceType());
        }
        return deqpCmdLine.toString();
    }

    private int getNumRemainingInstances() {
        int retVal = 0;
        for (TestDescription testId : mRemainingTests) {
            // If case is in current working set, sum only not yet executed instances.
            // If case is not in current working set, sum all instances (since they are not yet
            // executed).
            if (mInstanceListerner.mPendingResults.containsKey(testId)) {
                retVal += mInstanceListerner.mPendingResults.get(testId).remainingConfigs.size();
            } else {
                retVal += mTestInstances.get(testId).size();
            }
        }
        return retVal;
    }

    /**
     * Checks if this execution has been marked as interrupted and throws if it has.
     */
    private void checkInterrupted() throws RunInterruptedException {
        // Work around the API. RunUtil::checkInterrupted is private but we can call it indirectly
        // by sleeping a value <= 0.
        mRunUtil.sleep(0);
    }

    /**
     * Pass given batch tests without running it
     */
    private void fakePassTestRunBatch(TestBatch batch) {
        for (TestDescription test : batch.tests) {
            CLog.d("Marking '%s' invocation in config '%s' as passed without running", test.toString(),
                    batch.config.getId());
            mInstanceListerner.skipTest(test);
        }
    }

    /**
     * Fail given batch tests without running it
     */
    private void fakeFailTestRunBatch(TestBatch batch) {
        for (TestDescription test : batch.tests) {
            CLog.d("Marking '%s' invocation in config '%s' as failed without running", test.toString(),
                    batch.config.getId());
            mInstanceListerner.abortTest(test, "Required config not supported");
        }
    }

    /**
     * Pass all remaining tests without running them
     */
    private void fakePassTests(ITestInvocationListener listener) {
        HashMap<String, Metric> emptyMap = new HashMap<>();
        for (TestDescription test : mRemainingTests) {
            listener.testStarted(test);
            listener.testEnded(test, emptyMap);
        }
        // Log only once all the skipped tests
        CLog.d("Skipping tests '%s', either because they are not supported by the device or "
            + "because tests are simply being collected", mRemainingTests);
        mRemainingTests.clear();
    }

    /**
     * Check if device supports Vulkan.
     */
    private boolean isSupportedVulkan ()
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        final Map<String, Optional<Integer>> features = getDeviceFeatures(mDevice);

        for (String feature : features.keySet()) {
            if (feature.startsWith(FEATURE_VULKAN_LEVEL)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Check whether the device's claimed dEQP level is high enough that it should
     * pass the tests in the caselist.
     */
    private boolean claimedDeqpLevelIsRecentEnough() throws CapabilityQueryFailureException,
            DeviceNotAvailableException {
        // Determine whether we need to check the dEQP feature flag for Vulkan or OpenGL ES.
        final String featureName;
        if (isVulkanPackage()) {
            featureName = FEATURE_VULKAN_DEQP_LEVEL;
        } else if (isOpenGlEsPackage() || isEglPackage()) {
            // The OpenGL ES feature flag is used for EGL as well.
            featureName = FEATURE_OPENGLES_DEQP_LEVEL;
        } else {
            throw new AssertionError(
                "Claims about dEQP support should only be checked for Vulkan, OpenGL ES, or EGL "
                    + "packages");
        }

        CLog.d("For caselist \"%s\", the dEQP level feature flag is \"%s\".", mCaselistFile,
            featureName);

        // A Vulkan/OpenGL ES caselist filename has the form:
        //     {gles2,gles3,gles31,vk,egl}-master-YYYY-MM-DD.txt
        final Pattern caseListFilenamePattern = Pattern
            .compile("-master-(\\d\\d\\d\\d)-(\\d\\d)-(\\d\\d)\\.txt$");
        final Matcher matcher = caseListFilenamePattern.matcher(mCaselistFile);
        if (!matcher.find()) {
            CLog.d("No dEQP level date found in caselist. Running unconditionally.");
            return true;
        }
        final int year = Integer.parseInt(matcher.group(1));
        final int month = Integer.parseInt(matcher.group(2));
        final int day = Integer.parseInt(matcher.group(3));
        CLog.d("Caselist date is %04d-%02d-%02d", year, month, day);

        // As per the documentation for FEATURE_VULKAN_DEQP_LEVEL and
        // FEATURE_OPENGLES_DEQP_LEVEL in android.content.pm.PackageManager, a year is
        // encoded as an integer by devoting bits 31-16 to year, 15-8 to month and 7-0
        // to day.
        final int minimumLevel = (year << 16) + (month << 8) + day;

        CLog.d("For reference, date -> level mappings are:");
        CLog.d("    2019-03-01 -> 132317953");
        CLog.d("    2020-03-01 -> 132383489");
        CLog.d("    2021-03-01 -> 132449025");
        CLog.d("    2022-03-01 -> 132514561");

        CLog.d("Minimum level required to run this caselist is %d", minimumLevel);

        // Now look for the feature flag.
        final Map<String, Optional<Integer>> features = getDeviceFeatures(mDevice);

        for (String feature : features.keySet()) {
            if (feature.startsWith(featureName)) {
                final Optional<Integer> claimedDeqpLevel = features.get(feature);
                if (!claimedDeqpLevel.isPresent()) {
                    throw new IllegalStateException("Feature " + featureName
                        + " has no associated version");
                }
                CLog.d("Device level is %d", claimedDeqpLevel.get());

                final boolean shouldRunCaselist = claimedDeqpLevel.get() >= minimumLevel;
                CLog.d("Running caselist? %b", shouldRunCaselist);
                return shouldRunCaselist;
            }
        }

        CLog.d("Could not find dEQP level feature flag \"%s\". Running caselist unconditionally.",
            featureName);
        return true;
    }

    /**
     * Check if device supports OpenGL ES version.
     */
    private static boolean isSupportedGles(ITestDevice device, int requiredMajorVersion,
            int requiredMinorVersion) throws DeviceNotAvailableException {
        String roOpenglesVersion = device.getProperty("ro.opengles.version");

        if (roOpenglesVersion == null)
            return false;

        int intValue = Integer.parseInt(roOpenglesVersion);

        int majorVersion = ((intValue & 0xffff0000) >> 16);
        int minorVersion = (intValue & 0xffff);

        return (majorVersion > requiredMajorVersion)
                || (majorVersion == requiredMajorVersion && minorVersion >= requiredMinorVersion);
    }

    /**
     * Query if rendertarget is supported
     */
    private boolean isSupportedGlesRenderConfig(BatchRunConfiguration runConfig)
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        // query if configuration is supported
        final StringBuilder configCommandLine =
                new StringBuilder(getRunConfigDisplayCmdLine(runConfig));
        if (configCommandLine.length() != 0) {
            configCommandLine.append(" ");
        }
        configCommandLine.append("--deqp-gl-major-version=");
        configCommandLine.append(getGlesMajorVersion());
        configCommandLine.append(" --deqp-gl-minor-version=");
        configCommandLine.append(getGlesMinorVersion());

        final String commandLine = configCommandLine.toString();

        // check for cached result first
        if (mConfigQuerySupportCache.containsKey(commandLine)) {
            return mConfigQuerySupportCache.get(commandLine);
        }

        final boolean supported = queryIsSupportedConfigCommandLine(commandLine);
        mConfigQuerySupportCache.put(commandLine, supported);
        return supported;
    }

    private boolean queryIsSupportedConfigCommandLine(String deqpCommandLine)
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        final String instrumentationName =
                "com.drawelements.deqp/com.drawelements.deqp.platformutil.DeqpPlatformCapabilityQueryInstrumentation";
        final String command = String.format(
                "am instrument %s -w -e deqpQueryType renderConfigSupported -e deqpCmdLine \"%s\""
                    + " %s",
                AbiUtils.createAbiFlag(mAbi.getName()), deqpCommandLine, instrumentationName);

        final PlatformQueryInstrumentationParser parser = new PlatformQueryInstrumentationParser();
        mDevice.executeShellCommand(command, parser);
        parser.flush();

        if (parser.wasSuccessful() && parser.getResultCode() == 0 &&
                parser.getResultMap().containsKey("Supported")) {
            if ("Yes".equals(parser.getResultMap().get("Supported"))) {
                return true;
            } else if ("No".equals(parser.getResultMap().get("Supported"))) {
                return false;
            } else {
                CLog.e("Capability query did not return a result");
                throw new CapabilityQueryFailureException();
            }
        } else if (parser.wasSuccessful()) {
            CLog.e("Failed to run capability query. Code: %d, Result: %s",
                    parser.getResultCode(), parser.getResultMap().toString());
            throw new CapabilityQueryFailureException();
        } else {
            CLog.e("Failed to run capability query");
            throw new CapabilityQueryFailureException();
        }
    }

    /**
     * Return feature set supported by the device, mapping integer-valued features to their values
     */
    private Map<String, Optional<Integer>> getDeviceFeatures(ITestDevice device)
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        if (mDeviceFeatures == null) {
            mDeviceFeatures = queryDeviceFeatures(device);
        }
        return mDeviceFeatures;
    }

    /**
     * Query feature set supported by the device
     */
    private static Map<String, Optional<Integer>> queryDeviceFeatures(ITestDevice device)
            throws DeviceNotAvailableException, CapabilityQueryFailureException {
        // NOTE: Almost identical code in BaseDevicePolicyTest#hasDeviceFeatures
        // TODO: Move this logic to ITestDevice.
        String command = "pm list features";
        String commandOutput = device.executeShellCommand(command);

        // Extract the id of the new user.
        Map<String, Optional<Integer>> availableFeatures = new HashMap<>();
        for (String feature: commandOutput.split("\\s+")) {
            // Each line in the output of the command has the format "feature:{FEATURE_NAME}",
            // optionally followed by "={FEATURE_VERSION}".
            String[] tokens = feature.split(":|=");
            if (tokens.length < 2 || !"feature".equals(tokens[0])) {
                CLog.e("Failed parse features. Unexpect format on line \"%s\"", feature);
                throw new CapabilityQueryFailureException();
            }
            final String featureName = tokens[1];
            Optional<Integer> featureValue = Optional.empty();
            if (tokens.length > 2) {
                try {
                    // Integer.decode, rather than Integer.parseInt, is used here since some
                    // feature versions may be presented in decimal and others in hexadecimal.
                    featureValue = Optional.of(Integer.decode(tokens[2]));
                } catch (NumberFormatException numberFormatException) {
                    CLog.e("Failed parse features. Feature value \"%s\" was not an integer on "
                        + "line \"%s\"", tokens[2], feature);
                    throw new CapabilityQueryFailureException();
                }

            }
            availableFeatures.put(featureName, featureValue);
        }
        return availableFeatures;
    }

    private boolean isPortraitClassRotation(String rotation) {
        return BatchRunConfiguration.ROTATION_PORTRAIT.equals(rotation) ||
                BatchRunConfiguration.ROTATION_REVERSE_PORTRAIT.equals(rotation);
    }

    private boolean isLandscapeClassRotation(String rotation) {
        return BatchRunConfiguration.ROTATION_LANDSCAPE.equals(rotation) ||
                BatchRunConfiguration.ROTATION_REVERSE_LANDSCAPE.equals(rotation);
    }

    private void checkRecognizedPackage() {
        if (!isRecognizedPackage()) {
            throw new IllegalStateException("dEQP runner was created with illegal package name");
        }
    }

    private boolean isRecognizedPackage() {
        return "dEQP-EGL".equals(mDeqpPackage) || "dEQP-GLES2".equals(mDeqpPackage)
                || "dEQP-GLES3".equals(mDeqpPackage) || "dEQP-GLES31".equals(mDeqpPackage)
                || "dEQP-VK".equals(mDeqpPackage);
    }

    /**
     * Parse EGL nature from package name
     */
    private boolean isEglPackage() {
        checkRecognizedPackage();
        return "dEQP-EGL".equals(mDeqpPackage);
    }

    /**
     * Parse gl nature from package name
     */
    private boolean isOpenGlEsPackage() {
        checkRecognizedPackage();
        return "dEQP-GLES2".equals(mDeqpPackage) || "dEQP-GLES3".equals(mDeqpPackage)
                || "dEQP-GLES31".equals(mDeqpPackage);
    }

    /**
     * Parse vulkan nature from package name
     */
    private boolean isVulkanPackage() {
        checkRecognizedPackage();
        return "dEQP-VK".equals(mDeqpPackage);
    }

    /**
     * Check GL support (based on package name)
     */
    private boolean isSupportedGles() throws DeviceNotAvailableException {
        return isSupportedGles(mDevice, getGlesMajorVersion(), getGlesMinorVersion());
    }

    /**
     * Get GL major version (based on package name)
     */
    private int getGlesMajorVersion() {
        if ("dEQP-GLES2".equals(mDeqpPackage)) {
            return 2;
        } else if ("dEQP-GLES3".equals(mDeqpPackage)) {
            return 3;
        } else if ("dEQP-GLES31".equals(mDeqpPackage)) {
            return 3;
        } else {
            throw new IllegalStateException("getGlesMajorVersion called for non gles pkg");
        }
    }

    /**
     * Get GL minor version (based on package name)
     */
    private int getGlesMinorVersion() {
        if ("dEQP-GLES2".equals(mDeqpPackage)) {
            return 0;
        } else if ("dEQP-GLES3".equals(mDeqpPackage)) {
            return 0;
        } else if ("dEQP-GLES31".equals(mDeqpPackage)) {
            return 1;
        } else {
            throw new IllegalStateException("getGlesMinorVersion called for non gles pkg");
        }
    }

    private static List<Pattern> getPatternFilters(List<String> filters) {
        List<Pattern> patterns = new ArrayList<Pattern>();
        for (String filter : filters) {
            if (filter.contains("*")) {
                patterns.add(Pattern.compile(filter.replace(".","\\.").replace("*",".*")));
            }
        }
        return patterns;
    }

    private static Set<String> getNonPatternFilters(List<String> filters) {
        Set<String> nonPatternFilters = new HashSet<String>();
        for (String filter : filters) {
            if (filter.startsWith("#") || filter.isEmpty()) {
                // Skip comments and empty lines
                continue;
            }
            if (!filter.contains("*")) {
                // Deqp usesly only dots for separating between parts of the names
                // Convert last dot to hash if needed.
                if (!filter.contains("#")) {
                    int lastSeparator = filter.lastIndexOf('.');
                    String filterWithHash = filter.substring(0, lastSeparator) + "#" +
                        filter.substring(lastSeparator + 1, filter.length());
                    nonPatternFilters.add(filterWithHash);
                }
                else {
                    nonPatternFilters.add(filter);
                }
            }
        }
        return nonPatternFilters;
    }

    private static boolean matchesAny(TestDescription test, List<Pattern> patterns) {
        for (Pattern pattern : patterns) {
            if (pattern.matcher(test.toString()).matches()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Filter tests with the option of filtering by pattern.
     *
     * '*' is 0 or more characters.
     * '.' is interpreted verbatim.
     */
    private static void filterTests(Map<TestDescription, Set<BatchRunConfiguration>> tests,
                                    List<String> includeFilters,
                                    List<String> excludeFilters) {
        // We could filter faster by building the test case tree.
        // Let's see if this is fast enough.
        Set<String> includeStrings = getNonPatternFilters(includeFilters);
        Set<String> excludeStrings = getNonPatternFilters(excludeFilters);
        List<Pattern> includePatterns = getPatternFilters(includeFilters);
        List<Pattern> excludePatterns = getPatternFilters(excludeFilters);

        List<TestDescription> testList = new ArrayList<>(tests.keySet());
        for (TestDescription test : testList) {
            if (excludeStrings.contains(test.toString())) {
                tests.remove(test); // remove test if explicitly excluded
                continue;
            }
            boolean includesExist = !includeStrings.isEmpty() || !includePatterns.isEmpty();
            boolean testIsIncluded = includeStrings.contains(test.toString())
                    || matchesAny(test, includePatterns);
            if ((includesExist && !testIsIncluded) || matchesAny(test, excludePatterns)) {
                // if this test isn't included and other tests are,
                // or if test matches exclude pattern, exclude test
                tests.remove(test);
            }
        }
    }

    /**
     * Read each line from a file.
     */
    static private void readFile(Collection<String> lines, File file) throws FileNotFoundException {
        if (!file.canRead()) {
            CLog.e("Failed to read file '%s'", file.getPath());
            throw new FileNotFoundException();
        }
        try (Reader plainReader = new FileReader(file);
             BufferedReader reader = new BufferedReader(plainReader)) {
            String line = "";
            while ((line = reader.readLine()) != null) {
                // TOOD: Quick check filter
                lines.add(line);
            }
            // Rely on try block to autoclose
        }
        catch (IOException e)
        {
            throw new RuntimeException("Failed to read file '" + file.getPath() + "': " +
                     e.getMessage());
        }
    }

    /**
     * Prints filters into debug log stream, limiting to 20 entries.
     */
    static private void printFilters(List<String> filters) {
        int numPrinted = 0;
        for (String filter : filters) {
            CLog.d("    %s", filter);
            if (++numPrinted == 20) {
                CLog.d("    ... AND %d others", filters.size() - numPrinted);
                break;
            }
        }
    }

    /**
     * Loads tests into mTestInstances based on the options. Assumes
     * that no tests have been loaded for this instance before.
     */
    private void loadTests() {
        if (mTestInstances != null) throw new AssertionError("Re-load of tests not supported");

        // Note: This is specifically a LinkedHashMap to guarantee that tests are iterated
        // in the insertion order.
        mTestInstances = new LinkedHashMap<>();

        try {
            File testlist = new File(mBuildHelper.getTestsDir(), mCaselistFile);
            if (!testlist.isFile()) {
                // Finding file in sub directory if no matching file in the first layer of
                // testdir.
                testlist = FileUtil.findFile(mBuildHelper.getTestsDir(), mCaselistFile);
                if (testlist == null || !testlist.isFile()) {
                    throw new FileNotFoundException("Cannot find deqp test list file: "
                        + mCaselistFile);
                }
            }
            addTestsToInstancesMap(
                testlist,
                mConfigName,
                mScreenRotation,
                mSurfaceType,
                mConfigRequired,
                mTestInstances);
        }
        catch (FileNotFoundException e) {
            throw new RuntimeException("Cannot read deqp test list file: "  + mCaselistFile);
        }

        try
        {
            if (isIncrementalDeqpRun()) {
                for (String testFile : mIncrementalDeqpIncludeFiles) {
                    CLog.d("Read incremental dEQP include file '%s'", testFile);
                    File file = new File(mBuildHelper.getTestsDir(), testFile);
                    if (!file.isFile()) {
                        // Find file in sub directory if no matching file in the first layer of
                        // testdir.
                        file = FileUtil.findFile(mBuildHelper.getTestsDir(), testFile);
                        if (file == null || !file.isFile()) {
                            throw new FileNotFoundException(
                                "Cannot find incremental dEQP include file: " + testFile);
                        }
                    }
                    readFile(mIncrementalDeqpIncludeTests, file);
                }
            }
            for (String filterFile : mIncludeFilterFiles) {
                CLog.d("Read include filter file '%s'", filterFile);
                File file = new File(mBuildHelper.getTestsDir(), filterFile);
                readFile(mIncludeFilters, file);
            }
            for (String filterFile : mExcludeFilterFiles) {
                CLog.d("Read exclude filter file '%s'", filterFile);
                File file = new File(mBuildHelper.getTestsDir(), filterFile);
                readFile(mExcludeFilters, file);
            }
        }
        catch (FileNotFoundException e) {
            throw new HarnessRuntimeException("Cannot read deqp filter list file." + e,
                TestErrorIdentifier.TEST_ABORTED);
        }

        CLog.d("Include filters:");
        printFilters(mIncludeFilters);
        CLog.d("Exclude filters:");
        printFilters(mExcludeFilters);

        long originalTestCount = mTestInstances.size();
        CLog.i("Num tests before filtering: %d", originalTestCount);
        if ((!mIncludeFilters.isEmpty() || !mExcludeFilters.isEmpty()) && originalTestCount > 0) {
            filterTests(mTestInstances, mIncludeFilters, mExcludeFilters);

            // Update runtime estimation hint.
            if (mRuntimeHint != -1) {
                mRuntimeHint = (mRuntimeHint * mTestInstances.size()) / originalTestCount;
            }
        }
        CLog.i("Num tests after filtering: %d", mTestInstances.size());
    }

    /**
     * Set up the test environment.
     */
    private void setupTestEnvironment() throws DeviceNotAvailableException {
        try {
            // Get the system into a known state.
            // Clear ANGLE Global.Settings values
            mDevice.executeShellCommand("settings delete global angle_gl_driver_selection_pkgs");
            mDevice.executeShellCommand("settings delete global angle_gl_driver_selection_values");

            // ANGLE
            if (mAngle.equals(ANGLE_VULKAN)) {
                CLog.i("Configuring ANGLE to use: " + mAngle);
                // Force dEQP to use ANGLE
                mDevice.executeShellCommand(
                    "settings put global angle_gl_driver_selection_pkgs " + DEQP_ONDEVICE_PKG);
                mDevice.executeShellCommand(
                    "settings put global angle_gl_driver_selection_values angle");
                // Configure ANGLE to use Vulkan
                mDevice.executeShellCommand("setprop debug.angle.backend 2");
            } else if (mAngle.equals(ANGLE_OPENGLES)) {
                CLog.i("Configuring ANGLE to use: " + mAngle);
                // Force dEQP to use ANGLE
                mDevice.executeShellCommand(
                    "settings put global angle_gl_driver_selection_pkgs " + DEQP_ONDEVICE_PKG);
                mDevice.executeShellCommand(
                    "settings put global angle_gl_driver_selection_values angle");
                // Configure ANGLE to use Vulkan
                mDevice.executeShellCommand("setprop debug.angle.backend 0");
            }
        } catch (DeviceNotAvailableException ex) {
            // chain forward
            CLog.e("Failed to set up ANGLE correctly.");
            throw new DeviceNotAvailableException("Device not available", ex,
                mDevice.getSerialNumber());
        }
    }

    /**
     * Clean up the test environment.
     */
    private void teardownTestEnvironment() throws DeviceNotAvailableException {
        // ANGLE
        try {
            CLog.i("Cleaning up ANGLE");
            // Stop forcing dEQP to use ANGLE
            mDevice.executeShellCommand("settings delete global angle_gl_driver_selection_pkgs");
            mDevice.executeShellCommand("settings delete global angle_gl_driver_selection_values");
        } catch (DeviceNotAvailableException ex) {
            // chain forward
            CLog.e("Failed to clean up ANGLE correctly.");
            throw new DeviceNotAvailableException("Device not available", ex,
                mDevice.getSerialNumber());
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        final HashMap<String, Metric> emptyMap = new HashMap<>();
        // If sharded, split() will load the tests.
        if (mTestInstances == null) {
            loadTests();
        }

        mRemainingTests = new LinkedList<>(mTestInstances.keySet());
        long startTime = System.currentTimeMillis();
        listener.testRunStarted(getId(), mRemainingTests.size());

        try {
            if (mRemainingTests.isEmpty()) {
                CLog.d("No tests to run.");
                return;
            }
            final boolean isSupportedApi = (isOpenGlEsPackage() && isSupportedGles())
                                            || (isVulkanPackage() && isSupportedVulkan())
                                            || (!isOpenGlEsPackage() && !isVulkanPackage());
            if (mCollectTestsOnly
                || !isSupportedApi
                || !claimedDeqpLevelIsRecentEnough()) {
                // Pass all tests trivially if:
                // - we are collecting the names of the tests only, or
                // - the relevant API is not supported, or
                // - the device's feature flags do not claim to pass the tests
                fakePassTests(listener);
            } else if (!mRemainingTests.isEmpty()) {
                mInstanceListerner.setSink(listener);
                mDeviceRecovery.setDevice(mDevice);
                setupTestEnvironment();
                runTests();
                teardownTestEnvironment();
            }
        } catch (CapabilityQueryFailureException ex) {
            // Platform is not behaving correctly, for example crashing when trying to create
            // a window. Instead of silently failing, signal failure by leaving the rest of the
            // test cases in "NotExecuted" state
            CLog.e("Capability query failed - leaving tests unexecuted.");
        } finally {
            listener.testRunEnded(System.currentTimeMillis() - startTime, emptyMap);
        }
    }

   /**
     * {@inheritDoc}
     */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Set<String> getIncludeFilters() {
        return new HashSet<>(mIncludeFilters);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Set<String> getExcludeFilters() {
        return new HashSet<>(mExcludeFilters);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setCollectTestsOnly(boolean collectTests) {
        mCollectTestsOnly = collectTests;
    }

    /**
     * These methods are for testing.
     */
    public void addIncrementalDeqpIncludeTest(String test) {
        mIncrementalDeqpIncludeTests.add(test);
    }

    /**
     * These methods are for testing.
     */
    public void addIncrementalDeqpIncludeTests(Collection<String> tests) {
        mIncrementalDeqpIncludeTests.addAll(tests);
    }

    private static void copyOptions(DeqpTestRunner destination, DeqpTestRunner source) {
        destination.mDeqpPackage = source.mDeqpPackage;
        destination.mConfigName = source.mConfigName;
        destination.mCaselistFile = source.mCaselistFile;
        destination.mScreenRotation = source.mScreenRotation;
        destination.mSurfaceType = source.mSurfaceType;
        destination.mConfigRequired = source.mConfigRequired;
        destination.mIncludeFilters = new ArrayList<>(source.mIncludeFilters);
        destination.mIncludeFilterFiles = new ArrayList<>(source.mIncludeFilterFiles);
        destination.mExcludeFilters = new ArrayList<>(source.mExcludeFilters);
        destination.mExcludeFilterFiles = new ArrayList<>(source.mExcludeFilterFiles);
        destination.mAbi = source.mAbi;
        destination.mLogData = source.mLogData;
        destination.mCollectTestsOnly = source.mCollectTestsOnly;
        destination.mAngle = source.mAngle;
        destination.mDisableWatchdog = source.mDisableWatchdog;
        destination.mIncrementalDeqpIncludeFiles = new ArrayList<>(source.mIncrementalDeqpIncludeFiles);

    }

    /**
     * Helper to update the RuntimeHint of the tests after being sharded.
     */
    private void updateRuntimeHint(long originalSize, Collection<IRemoteTest> runners) {
        if (originalSize > 0) {
            long fullRuntimeMs = getRuntimeHint();
            for (IRemoteTest remote: runners) {
                DeqpTestRunner runner = (DeqpTestRunner)remote;
                long shardRuntime = (fullRuntimeMs * runner.mTestInstances.size()) / originalSize;
                runner.mRuntimeHint = shardRuntime;
            }
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Collection<IRemoteTest> split() {
        if (mTestInstances != null) {
            throw new AssertionError("Re-splitting or splitting running instance?");
        }
        // \todo [2015-11-23 kalle] If we split to batches at shard level, we could
        // basically get rid of batching. Except that sharding is optional?

        // Assume that tests have not been yet loaded.
        loadTests();

        Collection<IRemoteTest> runners = new ArrayList<>();
        // NOTE: Use linked hash map to keep the insertion order in iteration
        Map<TestDescription, Set<BatchRunConfiguration>> currentSet = new LinkedHashMap<>();
        Map<TestDescription, Set<BatchRunConfiguration>> iterationSet = this.mTestInstances;

        if (iterationSet.keySet().isEmpty()) {
            CLog.i("Cannot split deqp tests, no tests to run");
            return null;
        }

        // Go through tests, split
        for (TestDescription test: iterationSet.keySet()) {
            currentSet.put(test, iterationSet.get(test));
            if (currentSet.size() >= getBatchSizeLimit()) {
                runners.add(new DeqpTestRunner(this, currentSet));
                // NOTE: Use linked hash map to keep the insertion order in iteration
                currentSet = new LinkedHashMap<>();
            }
        }
        runners.add(new DeqpTestRunner(this, currentSet));

        // Compute new runtime hints
        updateRuntimeHint(iterationSet.size(), runners);
        CLog.i("Split deqp tests into %d shards", runners.size());
        return runners;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public long getRuntimeHint() {
        if (mRuntimeHint != -1) {
            return mRuntimeHint;
        }
        if (mTestInstances == null) {
            loadTests();
        }
        // Tests normally take something like ~100ms. Some take a
        // second. Let's guess 200ms per test.
        return 200 * mTestInstances.size();
    }
}
