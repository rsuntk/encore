RELEASE_TYPE := release
APP_ABI := arm64-v8a armeabi-v7a
APP_PLATFORM := latest

ifeq ($(RELEASE_TYPE),debug)
APP_OPTIM := $(RELEASE_TYPE)
APP_DEBUG := true
else
APP_OPTIM := release
APP_DEBUG := false
endif
