CC := clang
DISK_DIR := disk
EFI_BOOT_DIR := $(DISK_DIR)/EFI/BOOT

.PHONY: all clean clear run-whpx debug-whpx run-kvm debug-kvm run-tcg debug-tcg

all: boot.efi

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
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel whpx \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
	    -d int,cpu_reset -D qemu.log -no-reboot -no-shutdown \
		-device qemu-xhci,id=xhci0 \
		-device usb-kbd,bus=xhci0.0 \
		-device usb-mouse,bus=xhci0.0 \
		-trace events=./tmp/xhci.trace


debug-whpx: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel whpx \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-s -S -no-reboot -no-shutdown \
		-device intel-hda \
		-device qemu-xhci,id=xhci0 \
		-device usb-kbd,bus=xhci0.0 \
		-device usb-mouse,bus=xhci0.0 \
		-trace events=./tmp/xhci.trace

run-kvm: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel kvm \
		-cpu host \
		-boot order=d \
		-device intel-hda \
		-device hda-duplex,audiodev=my-audio \
		-audiodev pa,id=my-audio,server=/run/user/1000/pulse/native \
		-device qemu-xhci,id=xhci0 \
		-device usb-kbd,bus=xhci0.0 \
		-device usb-mouse,bus=xhci0.0 \
		-d guest_errors,unimp -D qemu.log \
		-no-reboot -no-shutdown -trace events=./tmp/xhci.trace


debug-kvm: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:./disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel kvm \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-device piix3-usb-xhci,id=xhci1 \
		-device usb-kbd,bus=xhci1.0,port=1 \
		-device usb-mouse,bus=xhci1.0,port=2 \
		-s -S -no-reboot -no-shutdown \
		-device intel-hda \
		-device hda-duplex,audiodev=my-audio \
		-audiodev pa,id=my-audio,server=/run/user/1000/pulse/native \
		-device qemu-xhci,id=xhci0 \
		-device usb-kbd,bus=xhci0.0 \
		-device usb-mouse,bus=xhci0.0 \
		-trace events=./tmp/xhci.trace

run-tcg: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel tcg \
		-cpu Broadwell \
		-boot order=d \
		-device piix3-usb-xhci,id=xhci-bus \
		-device usb-mouse,bus=xhci-bus.0 \
		-device usb-kbd,bus=xhci-bus.0 \
		-device usb-tablet,bus=xhci-bus.0 \
		-d guest_errors,unimp -D qemu.log \
		-no-reboot -no-shutdown -trace events=./tmp/xhci.trace


debug-tcg: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=./OVMF.fd,readonly=on \
		-drive format=raw,file=fat:rw:./disk \
		-m 4G \
		-smp 2 \
		-serial stdio \
		-accel tcg \
		-cpu Broadwell,hle=off,rtm=off \
		-boot order=d \
		-device piix3-usb-xhci,id=xhci1 \
		-device usb-kbd,bus=xhci1.0,port=1 \
		-device usb-mouse,bus=xhci1.0,port=2 \
		-s -S -no-reboot -no-shutdown \
		-device intel-hda \
		-device hda-duplex,audiodev=my-audio \
		-audiodev pa,id=my-audio,server=/run/user/1000/pulse/native \
		-device piix3-usb-xhci,id=xhci-bus \
		-device usb-mouse,bus=xhci-bus.0 \
		-device usb-kbd,bus=xhci-bus.0 \
		-device usb-tablet,bus=xhci-bus.0 \
		-trace events=./tmp/xhci.trace

iso: boot.efi
	mkdir -p $(EFI_BOOT_DIR)
	cp boot.efi $(EFI_BOOT_DIR)/BOOTX64.EFI
	xorriso -as mkisofs -o NeuroSamaOS.iso -J -R $(DISK_DIR)/

clean:
	rm -f *.o boot.exe boot.efi boot.dll qemu.log NeuroSamaOS.iso

clear: clean
