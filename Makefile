CC := clang
CCArg = --target=x86_64-unknown-windows-msvc \
        -ffreestanding \
        -nostdlib \
        -mno-red-zone \
        -O3 \
        -Wall \
        -m64 \
        -fno-stack-protector \
        -mno-stack-arg-probe -g -fno-builtin -ffreestanding \
		-fno-builtin-all -mllvm --max-store-memcpy=999999 -fno-builtin -fno-builtin-memcpy -fno-builtin-memset

LDArg = --target=x86_64-unknown-windows-msvc \
        -nostdlib \
		-Wl,-subsystem:efi_application \
		-Wl,-entry:efi_main

S_OBJS = $(wildcard *.s)
C_OBJS = $(wildcard *.c)
BIG_SOBJS = $(wildcard *.S)

O_OBJS = $(patsubst %.c,%.o,$(C_OBJS)) \
         $(patsubst %.s,%.o,$(S_OBJS)) \
         $(patsubst %.S,%.o,$(BIG_SOBJS))

%.o: %.c
	$(CC) $(CCArg) -c $< -o $@

%.o: %.s
	$(CC) $(CCArg) -c $< -o $@

%.o: %.S
	$(CC) $(CCArg) -c $< -o $@

boot.efi: $(O_OBJS)
	$(CC) $(LDArg) $^ -o boot.exe
	llvm-objcopy --subsystem=efi-app boot.exe $@

run: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-vnc :1 \
		-cpu max,+avx,+avx2,+sse,+sse2,+sse4.1,+sse4.2 \
		-boot order=d -device qemu-xhci\
	    -d int,cpu_reset -D qemu.log -no-reboot -no-shutdown

debug: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-vnc :1 \
		-cpu max,+avx,+avx2,+sse,+sse2,+sse4.1,+sse4.2 \
		-boot order=d \
		-s -S -no-reboot -no-shutdown

clear:
	rm -rf *.o boot.efi disk/ boot.dll
