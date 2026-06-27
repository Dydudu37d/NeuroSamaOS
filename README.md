# NeuroSamaOS

A UEFI Operating System for Neuro-Sama
Erm......
## We have
- AVX256
- SSE1 ~ SSE4.2
- NT (Non-temporal instructions)
- ERMS / FRMS (REP MOVS/STOS)
- FPU
- JIT
- HolyC-JIT
- FishC-JIT

## Acknowledgments
Thanks to the Linux kernel open-source community and NVIDIA's official documentation for providing valuable reference material on hardware specifications. Thanks also to DeepSeek for its assistance in organizing technical ideas during development.

*(I used a translation tool and DeepSeek for this acknowledgment, as English is not my native language and I want to ensure accuracy.))*

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
