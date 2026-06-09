CC := clang
CCArg := --target=x86_64-unknown-windows-msvc -ffreestanding -nostdlib \
		 -mno-red-zone -O3 -Wall -mavx2 -msse

LD := ld.lld
LDArg := -flavor link -subsystem:efi_application -entry:efi_main

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
    -drive if=pflash,format=raw,readonly=on,file=./OVMF.fd \
    -drive format=raw,file=fat:rw:disk \
    -vnc :1 \
    -m 2G \
    -smp 2 \
    -monitor stdio

clear:
	rm *.o boot.efi
