CC = gcc
LD = ld
CFLAGS = -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fPIE -m64 -march=x86-64
LDFLAGS = -T linker.ld -static -nostdlib --no-warn-rwx-segments

all: binipos.iso

binip_sys/kernel/kernel.o: binip_sys/kernel/kernel.c
	$(CC) -c binip_sys/kernel/kernel.c -o binip_sys/kernel/kernel.o $(CFLAGS)

binipos.elf: binip_sys/kernel/kernel.o
	$(LD) $(LDFLAGS) binip_sys/kernel/kernel.o -o binipos.elf

binipos.iso: binipos.elf limine.cfg
	rm -rf iso_root
	mkdir -p iso_root/boot/limine
	cp binipos.elf iso_root/
	cp limine.cfg iso_root/
	cp limine.cfg iso_root/boot/limine/
	cp /usr/share/limine/limine-bios.sys iso_root/
	cp /usr/share/limine/limine-bios.sys iso_root/boot/limine/
	cp /usr/share/limine/limine-bios-cd.bin iso_root/
	xorriso -as mkisofs -R -r -J -allow-lowercase -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table iso_root -o binipos.iso
	limine bios-install binipos.iso --force

run: binipos.iso
	-killall qemu-system-x86_64 2>/dev/null || true
	qemu-system-x86_64 -hda binipos.iso -vnc :0 -vga std &
	sleep 1
	vncviewer localhost:5900

clean:
	rm -rf binip_sys/kernel/kernel.o binipos.elf iso_root binipos.iso
