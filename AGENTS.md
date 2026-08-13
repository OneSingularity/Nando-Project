# AGENTS.md

## Cursor Cloud specific instructions

### What runs in this Cloud Agent VM

This repo is primarily a **Windows physical-lab** project (`Nando` / `NandoKernel` / Corsair BYOVD scripts). Those components need MSVC+WDK, the `CorsairLLAccess64` service, and a host with VBS/HVCI and hypervisor disabled (see root `README.md` and `.cursor/rules/lab-safety.mdc`). They **cannot** be built or exercised on this Linux Cloud Agent image.

The Linux-runnable surface is **DrvEye** (`/workspace/DrvEye`): a Python static analyzer for Windows `.sys` drivers.

### DrvEye (primary Cloud Agent workflow)

- Deps: `pefile`, `capstone`, `cryptography`, optional `unicorn` / `yara-python` (see `DrvEye/README.md`). There is no committed `requirements.txt`.
- Working directory for imports: `cd DrvEye` (entry point `DrvEye.py` imports `drivertool`).
- Unit tests: `python3 -m unittest discover -s tests -v`
- Smoke / core CLI: `python3 DrvEye.py --help` and analyze a sample under `/workspace/samples/*.bin` (native PE drivers). Prefer report-only flags (`--json`, `-o`); avoid generating PoCs unless explicitly needed.
- Syntax check: `python3 -m py_compile DrvEye.py drivertool/**/*.py` (or compile the package tree). No separate linter config is committed.
- Optional network refresh (`--live-check` / `--loldrivers`) caches under `~/.cache/drivertool/`.

### Out of scope here

Do not expect `Nando.sln` / `NandoKernel.vcxproj` builds, Corsair device IOCTLs, or Ark ASA injection flows to work in this environment.
