@echo off
qemu-system-i386 -vga std -d int,cpu_reset -no-reboot -D qemu.log -kernel kernel.bin