CC := clang
CCArg = --target=x86_64-unknown-windows-msvc -ffreestanding -nostdlib -mno-red-zone -O3 -Wall -mavx2 -msse4.2 -masm=att -mno-avx512f -m64 -mno-stack-arg-probe -fno-stack-check -fno-stack-protector -Wl,-export:efi_main

LD := ld.lld
LDArg = -flavor link -subsystem:efi_application -entry:efi_main -export:efi_main -force:multiple

S_OBJS = $(wildcard *.s)
C_OBJS = $(wildcard *.c)
O_OBJS = $(C_OBJS:.c=.o) $(S_OBJS:.s=.o)

%.o: %.c
	$(CC) $(CCArg) -c $< -o $@

%.o: %.s
	$(CC) $(CCArg) -c $< -o $@

boot.efi: $(O_OBJS)
	$(LD) $(LDArg) $^ -out:$@

run: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
    -drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
    -drive format=raw,file=fat:rw:disk \
    -vnc :1 \
	-m 2G \
    -smp 2 \
	-serial stdio \
	-cpu max,+avx,+avx2,+sse,+sse2,+sse4.1,+sse4.2 \
	-device qemu-xhci -device usb-mouse -device usb-kbd \
	-boot order=d

debug: CCArg+= -g
debug: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
    -drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
    -drive format=raw,file=fat:rw:disk \
    -vnc :1 \
	-m 2G \
    -smp 2 \
	-serial stdio \
	-s -S \
	-cpu max,+avx,+avx2,+sse,+sse2,+sse4.1,+sse4.2 \
	-device qemu-xhci -device usb-mouse -device usb-kbd \
	-boot order=d

clear:
	rm *.o boot.efi
