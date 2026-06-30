CC := clang
CCArg = --target=x86_64-unknown-windows-gnu \
        -ffreestanding \
        -nostdlib \
        -mno-red-zone \
        -O3 \
        -Wall \
        -m64 \
        -fno-stack-protector \
        -mno-stack-arg-probe -g -fno-builtin -ffreestanding \
		-fno-builtin-all -mllvm --max-store-memcpy=999999 -fno-builtin \
		-fno-builtin-memcpy -fno-builtin-memset -ffreestanding -nostdinc -nostdlib -mstack-alignment=16

LD := ld.lld
LDArg = -m i386pep \
	--entry=efi_main \
	--no-undefined \
	--gc-sections 

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
	$(LD) $(LDArg) $^ -o boot.exe
	llvm-objcopy --subsystem=efi-app boot.exe $@

run-whpx: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel whpx \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d -device qemu-xhci\
	    -d int,cpu_reset -D qemu.log -no-reboot -no-shutdown -overcommit mem-lock=on


debug-whpx: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel whpx \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-s -S -no-reboot -no-shutdown -overcommit mem-lock=on

run-kvm: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	sudo prlimit --memlock=unlimited \
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel kvm \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d -device qemu-xhci \
		-d int,cpu_reset -D qemu.log -no-reboot -no-shutdown -overcommit mem-lock=on


debug-kvm: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	sudo prlimit --memlock=unlimited \
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:./disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel kvm \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-s -S -no-reboot -no-shutdown -overcommit mem-lock=on

run-tcg: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	sudo prlimit --memlock=unlimited \
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel tcg \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d -device qemu-xhci \
		-d int,cpu_reset -D qemu.log -no-reboot -no-shutdown -overcommit mem-lock=on


debug-tcg: boot.efi
	mkdir -p disk/EFI/BOOT/
	cp boot.efi disk/EFI/BOOT/BOOTX64.EFI
	sudo prlimit --memlock=unlimited \
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=off \
		-drive format=raw,file=fat:rw:./disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel tcg \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-s -S -no-reboot -no-shutdown -overcommit mem-lock=on

clear:
	rm -rf *.o boot.efi disk/ boot.dll
