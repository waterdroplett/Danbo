@echo off
setlocal enabledelayedexpansion

set OBJS=

for %%f in (*.asm) do (
    nasm -f elf32 %%f -o %%~nf.o || exit /b 1
    set OBJS=!OBJS! %%~nf.o
)

for %%f in (*.c) do (
    clang --target=i686-elf -ffreestanding -fno-stack-protector -nostdlib -Wall -c %%f -o %%~nf.o || exit /b 1
    set OBJS=!OBJS! %%~nf.o
)

ld.lld -m elf_i386 -T linker.ld -o kernel.bin !OBJS! || exit /b 1

echo Build OK
if /i "%1"=="norun" exit /b 0

qemu-system-i386 -vga std -d int,cpu_reset -no-reboot -D qemu.log -kernel kernel.bin