SDK_SOURCES = \
	sdk_core.c \
	sdk_input.c \
	sdk_graphics.c \
	sdk_ui.c \
	sdk_layout.c \
	sdk_app.c

SDK_OBJECTS = $(SDK_SOURCES:.c=.o)
SDK_LIBRARY = libmarssdk.a

# Include paths relative to os/lib/sdk directory
INCLUDE_FLAGS = -I../../include -I../../include/sdk

# Use MinGW for consistent symbol names with kernel. Allow override so the
# parent Makefile or a CI runner can pin a specific toolchain, but fall back
# to PATH lookup so this works on Linux as well as macOS.
MINGW_PREFIX ?= x86_64-w64-mingw32-
CC ?= $(shell command -v $(MINGW_PREFIX)gcc 2>/dev/null)
AR ?= $(shell command -v $(MINGW_PREFIX)ar 2>/dev/null)

ifeq ($(strip $(CC)),)
$(error No $(MINGW_PREFIX)gcc on PATH. Install via os/scripts/setup_linux_toolchain.sh or setup_macos_toolchain.sh)
endif

CFLAGS += $(INCLUDE_FLAGS) -ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -O2

.PHONY: all clean

all: $(SDK_LIBRARY)

$(SDK_LIBRARY): $(SDK_OBJECTS)
	$(AR) rcs $@ $(SDK_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SDK_OBJECTS) $(SDK_LIBRARY)
