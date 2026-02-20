CROSS_PREFIX ?= x86_64-elf-
MINGW_PREFIX ?= x86_64-w64-mingw32-

DETECTED_CC := $(or \
	$(shell command -v $(CROSS_PREFIX)gcc 2>/dev/null), \
	$(shell command -v clang 2>/dev/null))

DETECTED_MINGW_CC := $(shell command -v $(MINGW_PREFIX)gcc 2>/dev/null)

DETECTED_LD := $(or \
	$(shell command -v $(CROSS_PREFIX)ld 2>/dev/null), \
	$(shell command -v ld.lld 2>/dev/null))

DETECTED_OBJCOPY := $(or \
	$(shell command -v $(CROSS_PREFIX)objcopy 2>/dev/null), \
	$(shell command -v llvm-objcopy 2>/dev/null), \
	$(shell command -v gobjcopy 2>/dev/null), \
	$(shell command -v objcopy 2>/dev/null))

ifeq ($(origin CC), default)
CC := $(DETECTED_CC)
endif

ifeq ($(origin LD), default)
LD := $(DETECTED_LD)
endif

ifndef OBJCOPY
OBJCOPY := $(DETECTED_OBJCOPY)
endif

QEMU ?= qemu-system-x86_64
QEMU_INPUT_DEVICES ?= -usb -device usb-tablet -device usb-mouse -device virtio-mouse-pci
OVMF_CODE ?= $(or \
	$(shell [ -f /opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd ] && echo /opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd), \
	$(shell ls -1 /opt/homebrew/Cellar/qemu/*/share/qemu/edk2-x86_64-code.fd 2>/dev/null | tail -n 1))
OVMF_VARS ?= ./OVMF_VARS.fd

BUILD_DIR := build
ESP_DIR := esp
EFI_BOOT_DIR := $(ESP_DIR)/EFI/BOOT
TARGET_EFI := $(EFI_BOOT_DIR)/BOOTX64.EFI

USE_MINGW ?= 0
ifneq ($(strip $(DETECTED_MINGW_CC)),)
USE_MINGW := 1
endif

ifeq ($(USE_MINGW),1)
CC := $(DETECTED_MINGW_CC)
endif

CFLAGS := -O2 -Iinclude -ffreestanding -fpic -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror
LDFLAGS := -nostdlib -znocombreloc -T linker.ld -shared -Bsymbolic -e efi_main
LDLIBS :=

ifeq ($(USE_MINGW),1)
CFLAGS := -O2 -Iinclude -ffreestanding -fshort-wchar -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror
LDFLAGS := -nostdlib -Wl,--subsystem,10 -Wl,-e,efi_main
endif

ifeq ($(notdir $(CC)),clang)
	CFLAGS += --target=x86_64-unknown-elf
endif

ifeq ($(strip $(CC)),)
$(error No C compiler found. Install x86_64-elf-gcc or clang)
endif

ifeq ($(USE_MINGW),0)
ifeq ($(strip $(LD)),)
$(error No linker found. Install x86_64-elf-ld or ld.lld)
endif

ifeq ($(strip $(OBJCOPY)),)
$(error No objcopy found. Install x86_64-elf-objcopy or llvm-objcopy)
endif

endif

ifeq ($(wildcard linker.ld),)
$(error Missing linker.ld in project root)
endif

SRCS := \
	boot/efi_main.c \
	kernel/kernel.c \
	kernel/timer.c \
	drivers/input.c \
	drivers/keyboard_uefi.c \
	drivers/mouse_uefi.c \
	graphics/framebuffer.c \
	graphics/font8x16.c \
	gui/gui.c \
	lib/string.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean run debug esp

all: $(TARGET_EFI)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.so: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

ifeq ($(USE_MINGW),1)
$(TARGET_EFI): $(OBJS)
	@mkdir -p $(EFI_BOOT_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
else
$(TARGET_EFI): $(BUILD_DIR)/kernel.so
	@mkdir -p $(EFI_BOOT_DIR)
	$(OBJCOPY) -I elf64-x86-64 -O pei-x86-64 --subsystem=10 \
		-j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
		$< $@
endif

esp: $(TARGET_EFI)

run: all
	@if [ -z "$(OVMF_CODE)" ] || [ ! -f "$(OVMF_CODE)" ]; then \
		echo "Missing OVMF code firmware. Set OVMF_CODE=/path/to/edk2-x86_64-code.fd"; \
		exit 1; \
	fi
	@if [ -f "$(OVMF_VARS)" ]; then \
		$(QEMU) \
			-machine q35 -m 512M \
			$(QEMU_INPUT_DEVICES) \
			-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
			-drive if=pflash,format=raw,file=$(OVMF_VARS) \
			-drive format=raw,file=fat:rw:$(ESP_DIR) \
			-serial stdio; \
	else \
		echo "OVMF_VARS not found; running without vars pflash."; \
		$(QEMU) \
			-machine q35 -m 512M \
			$(QEMU_INPUT_DEVICES) \
			-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
			-drive format=raw,file=fat:rw:$(ESP_DIR) \
			-serial stdio; \
	fi

debug: all
	@if [ -z "$(OVMF_CODE)" ] || [ ! -f "$(OVMF_CODE)" ]; then \
		echo "Missing OVMF code firmware. Set OVMF_CODE=/path/to/edk2-x86_64-code.fd"; \
		exit 1; \
	fi
	@if [ -f "$(OVMF_VARS)" ]; then \
		$(QEMU) \
			-machine q35 -m 512M \
			$(QEMU_INPUT_DEVICES) \
			-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
			-drive if=pflash,format=raw,file=$(OVMF_VARS) \
			-drive format=raw,file=fat:rw:$(ESP_DIR) \
			-serial stdio -s -S; \
	else \
		echo "OVMF_VARS not found; running debug without vars pflash."; \
		$(QEMU) \
			-machine q35 -m 512M \
			$(QEMU_INPUT_DEVICES) \
			-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
			-drive format=raw,file=fat:rw:$(ESP_DIR) \
			-serial stdio -s -S; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(TARGET_EFI)
