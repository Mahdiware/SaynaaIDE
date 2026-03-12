NABLE_WARNINGS := false # Top-level switch: set to true to enable warnings, false to disable

ifeq ($(ENABLE_WARNINGS),true)
    WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
else
    WARNINGS := -w
endif

# Define global flags
LOCAL_CFLAGS += -std=c99 \
                 -fpermissive \
                 -fno-rtti \
                 -fno-exceptions \
                 -g0 \
                 -fomit-frame-pointer \
                 -ffunction-sections \
                 -fdata-sections \
                 -fvisibility=hidden \
                 -fvisibility-inlines-hidden \
                 $(WARNINGS)

LOCAL_CPPFLAGS += -std=c++14 \
                   -fpic \
                   -fpermissive \
                   -fno-rtti \
                   -fno-exceptions \
                   -fvisibility=hidden \
                   -ffunction-sections \
                   -fdata-sections \
                   $(WARNINGS)