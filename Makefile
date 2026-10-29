gcc -c ./binip_sys/kernel/kernel.c -o kernel.o \
    -std=c11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fPIE \
    -m64 \
    -march=x86-64


ld -T linker.ld kernel.o -o myos.elf -static -nostdlib


mkdir -p iso_root/boot
mkdir -p iso_root/EFI/BOOT


cp myos.elf iso_root/boot/
cp limine.cfg iso_root/boot/


cp /usr/share/limine/limine-bios.sys iso_root/boot/
cp /usr/share/limine/limine-bios-cd.bin iso_root/boot/


xorriso -as mkisofs -b boot/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        iso_root -o binipos.iso


limine bios-install binipos.iso

