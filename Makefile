CC      = gcc
AS      = nasm
LD      = ld

CFLAGS  = -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check \
          -fno-lto -fPIE -m64 -march=x86-64 -Wall -Wextra -Ibinip_sys
ASFLAGS = -f elf64
LDFLAGS = -T linker.ld -static -nostdlib --no-warn-rwx-segments

BUILD_DIR  = build
OUTPUT_DIR = output
ISO_DIR    = $(BUILD_DIR)/iso_root

KERNEL     = $(OUTPUT_DIR)/binipos.elf
ISO        = $(OUTPUT_DIR)/binipos.iso

# Automatically find all .c and .asm files inside binip_sys/
C_SRCS     = $(shell find binip_sys -name '*.c')
ASM_SRCS   = $(shell find binip_sys -name '*.asm')

OBJS       = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
             $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))

.PHONY: all run clean

all: $(ISO)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	@mkdir -p $(OUTPUT_DIR)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(ISO): $(KERNEL) limine.conf
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/limine \
	          $(ISO_DIR)/binip_sys/kernel \
	          $(ISO_DIR)/binip_sys/drivers \
	          $(ISO_DIR)/binip_sys/biots \
	          $(OUTPUT_DIR)
	cp $(KERNEL) $(ISO_DIR)/binip_sys/kernel/
	cp limine.conf $(ISO_DIR)/boot/limine/
	cp /usr/share/limine/limine-bios.sys $(ISO_DIR)/boot/limine/
	cp /usr/share/limine/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	xorriso -as mkisofs -R -r -J -allow-lowercase -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table $(ISO_DIR) -o $(ISO)
	limine bios-install $(ISO) --force

run: $(ISO)
	-killall qemu-system-x86_64 2>/dev/null || true
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR) binip_sys/**/*.o binipos.elf binipos.iso iso_root
