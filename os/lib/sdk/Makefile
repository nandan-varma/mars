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
CFLAGS += $(INCLUDE_FLAGS) -ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror

CC ?= cc
AR ?= ar

.PHONY: all clean

all: $(SDK_LIBRARY)

$(SDK_LIBRARY): $(SDK_OBJECTS)
	$(AR) rcs $@ $(SDK_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SDK_OBJECTS) $(SDK_LIBRARY)
