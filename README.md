# NeuroSamaOS

A UEFI Operating System for Neuro-Sama
Erm......
## Operating System have
- AVX256
- SSE1 ~ SSE4.2
- NT (Non-temporal instructions)
- ERMS / FRMS (REP MOVS/STOS)
- FPU
- JIT
- HolyC-JIT
- HolyFish-JIT
- La57
- VA=PA
- XHCIz
- No EHCI OHCI UHCI (I rm -rf ohci.* uhci.*)
- GM206
- ATA & FAT32 & GPT
- GOP
- DSLMaker(File name is DSLMake)
- rdtsc
- UEFI64

## Acknowledgments
Thanks to the Linux kernel open-source community and NVIDIA's official documentation and Nouveau for providing valuable reference material on hardware specifications. Thanks also to DeepSeek and Google Gemini for its assistance in organizing technical ideas during development.

*(I used a translation tool and DeepSeek for this acknowledgment, as English is not my native language and I want to ensure accuracy.)*

## Info
NeuroSamaOS, Polling,No int,irq only 0~31,gdt ring0,no ring3,VA=PA

## DSLMaker
Right,Everyone can Make DSL/General Lang like Scratch3.x

## Build
```bash
make
```
## Run
```bash
make run
```

## Debug
```bash
make debug
```

###### (PS:I like Use TempleOS,Its Good)