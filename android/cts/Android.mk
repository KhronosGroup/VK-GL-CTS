# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := CtsDeqpTestCases

LOCAL_MODULE_TAGS := optional

# Tag this module as a cts_v2 test artifact
LOCAL_COMPATIBILITY_SUITE := cts_v2

LOCAL_SDK_VERSION := 22

LOCAL_SRC_FILES := $(call all-java-files-under, runner/src)
LOCAL_JAVA_LIBRARIES := cts-tradefed_v2 compatibility-host-util tradefed-prebuilt

DEQP_CASELISTS:=$(sort $(patsubst mnc/%,%, \
  $(shell cd $(LOCAL_PATH) ; \
          find -L mnc -maxdepth 1 -name "*.txt") \
  ))
LOCAL_COMPATIBILITY_SUPPORT_FILES := $(foreach file, $(DEQP_CASELISTS), ./mnc/$(file):$(file))


include $(BUILD_HOST_JAVA_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
