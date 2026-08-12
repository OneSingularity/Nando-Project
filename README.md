# Nando Project: Stealth Kernel Driver for Ark: Survival Ascended

## Overview
This repository contains a specialized, stealth-focused kernel-mode framework designed for game research on Ark: Survival Ascended. The project shifted from a Virtual Machine (VM) environment to physical hardware to achieve stable physical memory primitives and bypass hypervisor-level protections (VBS/HVCI).

## Project Structure
- `/NandoKernel`: The core kernel-mode component (shellcode-based) that hijacks existing system functions for execution.
- `/Nando`: The original user-mode DLL project for Ark: Survival Ascended.
- `/VulnDriver`: Research and utilities for exploiting the `CorsairLLAccess64.sys` driver to gain kernel read/write access.

## History & VM Limitations
Initially, research was conducted in a Windows 11 VM. However, several critical bottlenecks were encountered:
1. **Nested Virtualization**: Hyper-V/VBS prevented the Corsair driver from reliably mapping physical memory, leading to `0x4E` and `0x3B` bugchecks.
2. **Hypervisor Protection**: Direct KVA→PA translation via page table walking was blocked by the hypervisor.
3. **KASLR Instability**: Frequent reboots in the VM made hardcoded physical addresses unreliable.

**Resolution**: The project moved to physical hardware with `hypervisorlaunchtype off` and VBS disabled. This allows for stable, direct physical memory access and surgical kernel hijacking.

## Current Strategy: "The Customer Driver"
- **Injection**: No memory allocation or new threads are used. Instead, code is injected into a "Code Cave" in `dxgkrnl.sys`.
- **Execution**: A surgical hook is placed on `nt!NtYieldExecution` to redirect control to the injected code.
- **Communication**: A shared physical page (Mailbox) facilitates high-speed, stealthy interaction between `Nando.dll` (User-mode) and `NandoKernel` (Kernel-mode).

## Disclaimer
This project is for educational and research purposes only. Developing and using kernel-mode drivers carries significant risks, including system instability and permanent damage to software environments.
