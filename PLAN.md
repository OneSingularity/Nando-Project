# Development Plan: Nando Stealth Driver

## Phase 1: Stable Kernel Heartbeat (Current)
- [x] Disable VBS/HVCI on physical host.
- [x] Identify stable, non-paged code caves in `ntoskrnl` or `dxgkrnl`.
- [x] Implement surgical hijack of `nt!NtYieldExecution`.
- [x] Establish trace-byte mailbox communication.

## Phase 2: Kernel Functionalization (In Progress)
- [x] Implement **Command Dispatcher** in `NandoKernel/functional_stub.py`.
- [x] Establish **Shared Mailbox Protocol** (Cmd/Status/Args).
- [x] Implement **Surgical Physical Read/Write** primitives inside the kernel stub.
- [x] Implement **Process Token Stealing** (LPE) as a kernel-level command.
- [x] Implement **MmCopyMemory Integration** for safe cross-process reads.
- [x] Implement **Surgical Page Table Walking** in the stub (for Phase 3).
- [x] Implement **Ark: Survival Ascended Structure Scanner** (Next Step).
- [x] EMERGENCY: Revert all hooks for main account safety.
- [x] RE-ESTABLISHED: Kernel Functionalization (Dispatcher 0x01-0x07).
- [x] Implement **Surgical DKOM** to hide the process (Phase 4).
- [x] RESEARCHED: **Surgical IDT Hijack** (Phase 5).
- [x] Implement **Surgical IDT Hijack** in the stub (Phase 5).
- [x] RESEARCHED: **VTL 1 / Hyper-V Interaction** (Phase 6).
- [x] Implement **Surgical Hypercall Hook** to bypass VTL restrictions (Phase 6).
- [ ] TEST: **Ark Single Player Integration** (Ready for Final Strike).
- [ ] Implement **Surgical UEFI DXE Driver** for pre-boot persistence.
- [ ] Integrate **CR3 Cache** to speed up KVA→PA translation for game memory.

## Phase 3: Ark: Survival Ascended Integration
- [ ] Update `Nando.dll` to communicate via the Shared Mailbox.
- [ ] Offload sensitive memory reads (Player positions, structures) to the kernel.
- [ ] Implement a kernel-side "Object Scanner" to minimize user-kernel context switching.

## Phase 4: Long-Term Stealth
- [ ] Implement **DKOM (Direct Kernel Object Manipulation)** to hide the mailbox page if necessary.
- [ ] Rotate hook targets to avoid static signature detection by anti-cheats.
- [ ] Use `IA32_LSTAR` or `IDT` redirection for even more stealthy execution.

## Security Precautions
- Always use **Signature Verification** before writing to physical memory.
- Avoid any API calls that trigger `EtwTi` (Threat Intelligence) events.
- No new memory objects (`PoolAllocations`) or system threads.
