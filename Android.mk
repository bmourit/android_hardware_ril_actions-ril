# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    huawei-ril.c \
    ril-cc.c \
    ril-dev.c \
    ril-mm.c \
    ril-sms.c \
    ril-sim.c \
    ril-ss.c \
    atchannel.c \
    ril-ps.c \
    ril-ps-api.c \
    ril-ps-sm.c \
    misc.c \
    at_tok.c \
    gsm_7bit.c \
    ucs2.c \
    sar_feature.c

LOCAL_SHARED_LIBRARIES := \
	libcutils libutils libril libnetutils

	# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE
LOCAL_MODULE_TAGS := eng optional
LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog

ifeq ($(TARGET_PRODUCT),sooner)
  LOCAL_CFLAGS += -DOMAP_CSMI_POWER_CONTROL -DUSE_TI_COMMANDS 
endif

ifeq ($(TARGET_PRODUCT),surf)
  LOCAL_CFLAGS += -DPOLL_CALL_STATE -DUSE_QMI
endif

ifeq ($(TARGET_PRODUCT),dream)
  LOCAL_CFLAGS += -DPOLL_CALL_STATE -DUSE_QMI
endif

ifeq ("$(VENDOR)", "YIHU")
LOCAL_CFLAGS += -DSUPPORT_YIHU
endif

ifeq ("$(VENDOR)", "BEISHANG")
LOCAL_CFLAGS += -DSUPPORT_BEISHANG
endif

ifeq ("$(VENDOR)", "DELL")
LOCAL_CFLAGS += -DSUPPORT_DELL
endif

ifeq ("$(ANDROID_VERSION)", "3")
LOCAL_CFLAGS += -DANDROID_3
endif

#ifeq ("$(ANDROID_VERSION)", "4")
LOCAL_CFLAGS += -DANDROID_3 -DANDROID_4
#endif

#MODIFY CMTI ISSUE
LOCAL_CFLAGS += -DCMTI_ISSUE
ifeq (foo,foo)
  #build shared library
  LOCAL_PRELINK_MODULE:=TRUE
  LOCAL_SHARED_LIBRARIES += \
	libcutils libutils
  LOCAL_LDLIBS += -lpthread
  LOCAL_CFLAGS += -DRIL_SHLIB
  LOCAL_MODULE:= libactions-ril
  include $(BUILD_SHARED_LIBRARY)
else
  #build executable
  LOCAL_SHARED_LIBRARIES += \
	libril
  LOCAL_MODULE:= actions-ril
  include $(BUILD_EXECUTABLE)
endif
