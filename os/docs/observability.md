# MarsOS Observability

This document is the operational reference for diagnostic output produced by the kernel. Use it when reading a captured serial log or debugging a panic.

## Where the signal goes

All diagnostic output lands on **COM1 (port 0x3F8)** at 8N1 / 115200 baud. Capture mechanisms:

| Context | How to capture |
|---|---|
| `make -C os run` (interactive QEMU) | Serial is wired to `stdio` — printed in the terminal |
| `make -C os smoke-test` (headless QEMU) | Written to `os/build/smoke/serial.log` |
| Real hardware (planned for M3) | USB-TTL adapter on the COM1 header, 115200 8N1 |

The drain is on by default. Toggle with `diag_set_serial_drain(FALSE)` from inside the kernel if a hot path needs to be silenced.

## Output formats

The drain emits three categories of line. All are designed to be `grep`-friendly.

**Stage transitions** (every call to `diag_set_stage`):
```
[stage 200 tick=12345]
```
`200` is the stage ID; `tick` is the kernel tick at the time of the transition. The very last successful stage is `200 — services_and_apps`, which means the boot reached the desktop init. The CI smoke test greps for `stage 200`.

**Diag records** (every call to `diag_log`):
```
[d 12345 dom=00000700 code=00000001 a=0000000000000000 b=0000000000000000]
```
- `d 12345` is the tick at which the record was written.
- `dom` is the 32-bit domain identifier (see registry below).
- `code` is the 32-bit code (domain-specific).
- `a` and `b` are 64-bit operands (also domain-specific).

**Panic dump** (any call to `panic_trap` or `panic_now`):
```
*** MARS PANIC ***
  vector=0x000000000000000e  code=0x0000000000000002  address=0xffff800000123456
  stage=0x000000c8
-- diag stage history (oldest first) --
[stage 10 tick=0]
[stage 20 tick=1]
...
-- diag records (newest first, 64) --
[d 4321 dom=0000dead code=0000000e a=0000000000000002 b=ffff800000123456]
...
*** halted ***
```
After the dump, the CPU is parked with `cli; hlt`. Triple-faults are gated by `-no-reboot` in the smoke harness so the dump survives.

## Domain registry

Domains identify the subsystem that emitted a record. Codes are domain-specific.

| Domain | Subsystem | Source file |
|---|---|---|
| `0x0002` | Memory allocator | `kernel/memory.c` |
| `0x0200` | Kernel bring-up assertions | `kernel/kernel.c` |
| `0x0610` | Managed process spawn | `kernel/kernel.c` |
| `0x0700–0x0708` | Block driver | `drivers/block.c` |
| `0x0800–0x0808` | Network driver | `drivers/network.c` |
| `0x0810–0x0821` | Network stack | `kernel/network.c` |
| `0x0830` | Network app | `apps/network_app.c` |
| `0x0900–0x0903` | Audio driver | `drivers/audio.c` |
| `0x0A00–0x0A10` | PCI enumeration | `drivers/pci.c` |
| `0xDEAD` | Panic / crash capture | `kernel/diag.c` (`diag_capture_crash`) |

Codes inside a domain are documented at the call site. When you add a new domain, **append to this table in the same PR** — the table is authoritative.

## Bring-up stage values

These come from `kernel.c` and indicate how far boot got. Useful in panic dumps and smoke-test failures.

| Stage | Step |
|---|---|
| 10 | `diag_init` |
| 20 | `interrupts_init` |
| 30 | `event_bus_init` |
| 40 | `timer_init` |
| 50 | `scheduler_process_init` |
| 60 | `memory_init` |
| 70 | `framebuffer_init` |
| 80 | `vm_init` |
| 90 | `heap_init` |
| 100 | `input_init` |
| 110 | `wm_init` |
| 120 | `vfs_init` |
| 130 | `pci_init` (M5 scaffold — populates the PCI device table) |
| 200 | `services_and_apps` (last init step before scheduler_run) |

## When the smoke test fails

`make -C os smoke-test` fails if `stage 200` never appears in the captured serial within the timeout (default 60s in CI, 30s locally). What to check, in order:

1. **Did boot start at all?** Look for `[mars] efi_main:start`. If absent, BOOTX64.EFI never loaded — toolchain issue or wrong file in ESP.
2. **What was the last stage seen?** That tells you which init step hung or crashed.
3. **Was there a `*** MARS PANIC ***`?** If so, the dump that follows tells you the rest. The vector identifies the CPU exception (0x0E = page fault, 0x0D = GP fault, 0xFF = `panic_now`).
4. **Tail of `serial.log`** is uploaded as a CI artifact (`smoke-serial-log`) — always start there.

Stage `120 → 200` is the gap where most failures will land today (it spawns managed processes and launches the core app suite).
