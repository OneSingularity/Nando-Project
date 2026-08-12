# Admin PowerShell helper — does NOT map physical memory.
# bcdedit changes REQUIRE a normal reboot (that is not a bugcheck).

$ErrorActionPreference = "Continue"
$script = "C:\Users\justin hernando\Documents\VulnDriver\driver_interact.py"

Write-Host "=== Start driver ===" -ForegroundColor Cyan
sc.exe start CorsairLLAccess64 2>$null
sc.exe query CorsairLLAccess64

Write-Host "`n=== MSR-only check (no phys maps) ===" -ForegroundColor Cyan
python $script

Write-Host @"

=== IMPORTANT: two kinds of 'restart' ===
1) After bcdedit /debug on  -> you MUST reboot. That is NORMAL.
2) Blue screen / unexpected reboot during python --probe or scans -> BUGCHECK.
   Do not use --probe / --find-cr3 / --translate-lstar until WinDbg CR3 is ready.

=== Enable local kernel debugging (then REBOOT on purpose) ===
  bcdedit /debug on
  bcdedit /dbgsettings local
  shutdown /r /t 0

=== After reboot ===
  sc.exe start CorsairLLAccess64
  python "$script"

=== WinDbg Preview (Admin) ===
  File -> Attach to kernel -> Local
  Then type (in WinDbg, NOT PowerShell):
    !process 0 0 System
  Copy DirectoryTableBase / DirBase

=== Translate (only after you have CR3 + a snapshot) ===
  python "$script" --cr3 0xYOUR_CR3 --translate-lstar

"@ -ForegroundColor Yellow
