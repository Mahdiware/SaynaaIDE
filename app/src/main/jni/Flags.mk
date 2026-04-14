ENABLE_WARNINGS := false
COMPUTED_GOTO   := -DNO_COMPUTED_GOTO


ifeq ($(ENABLE_WARNINGS),true)
    WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
else
    WARNINGS := -w
endif

ifeq ($(APP_OPTIM),debug)
    DEBUG_FLAGS := -DDEBUG -O0 -g3 -fno-omit-frame-pointer
else
    DEBUG_FLAGS := -DNDEBUG -g0 -fomit-frame-pointer
endif

# Define global flags
LOCAL_CFLAGS += -std=c99 \
                 -fpermissive \
                 -fno-rtti \
                 -fno-exceptions \
                 -ffunction-sections \
                 -fdata-sections \
                 -fvisibility=hidden \
                 -fvisibility-inlines-hidden \
                 $(COMPUTED_GOTO) \
                 $(DEBUG_FLAGS) \
                 $(WARNINGS)

LOCAL_CPPFLAGS += -std=c++14 \
                   -fpic \
                   -fpermissive \
                   -fno-rtti \
                   -fno-exceptions \
                   -fvisibility=hidden \
                   -ffunction-sections \
                   -fdata-sections \
                   $(COMPUTED_GOTO) \
                   $(DEBUG_FLAGS) \
                   $(WARNINGS)