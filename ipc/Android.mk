# Android makefile for audio kernel modules

# Assume no targets will be supported

# Check if this driver needs be built for current target
ifeq ($(call is-board-platform,msmnile),true)
ifeq ($(TARGET_PRODUCT), $(filter $(TARGET_PRODUCT), msmnile_au msmnile_gvmq))
AUDIO_SELECT  := CONFIG_SND_SOC_SA8155=m
else
AUDIO_SELECT  := CONFIG_SND_SOC_SM8150=m
endif
endif

ifeq ($(call is-board-platform-in-list,$(MSMSTEPPE) $(TRINKET)),true)
ifeq ($(TARGET_PRODUCT), $(filter $(TARGET_PRODUCT), sm6150_au))
AUDIO_SELECT  := CONFIG_SND_SOC_SA6155=m
else
AUDIO_SELECT  := CONFIG_SND_SOC_SM6150=m
endif
endif

ifeq ($(call is-board-platform,kona),true)
AUDIO_SELECT  := CONFIG_SND_SOC_KONA=m
endif

ifeq ($(call is-board-platform,lito),true)
AUDIO_SELECT  := CONFIG_SND_SOC_LITO=m
endif

AUDIO_CHIPSET := audio
# Build/Package only in case of supported target
ifeq ($(call is-board-platform-in-list,msmnile $(MSMSTEPPE) $(TRINKET) kona lito),true)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
	AUDIO_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/audio-kernel
endif # opensource

DLKM_DIR := $(TOP)/device/qcom/common/dlkm

# Build audio.ko as $(AUDIO_CHIPSET)_audio.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := AUDIO_ROOT=$(AUDIO_BLD_DIR)

# We are actually building audio.ko here, as per the
# requirement we are specifying <chipset>_audio.ko as LOCAL_MODULE.
# This means we need to rename the module to <chipset>_audio.ko
# after audio.ko is built.
KBUILD_OPTIONS += MODNAME=apr_dlkm
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(AUDIO_SELECT)

###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_apr.ko
LOCAL_MODULE_KBUILD_NAME  := apr_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
ifeq ($(call is-board-platform-in-list,msmnile $(MSMSTEPPE) $(TRINKET)),true)
ifneq ($(TARGET_PRODUCT), $(filter $(TARGET_PRODUCT), msmnile_au sm6150_au msmnile_gvmq))
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wglink.ko
LOCAL_MODULE_KBUILD_NAME  := wglink_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
endif
###########################################################

endif # DLKM check
endif # supported target check
