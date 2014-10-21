LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := $(call all-java-files-under,src)
LOCAL_JNI_SHARED_LIBRARIES := libdeqp

LOCAL_ASSET_DIR := $(LOCAL_PATH)/../../data
LOCAL_PACKAGE_NAME := com.drawelements.deqp
LOCAL_MULTILIB := both

include $(BUILD_PACKAGE)
