# MarsOS Security Audit

This document is the kernel's security posture statement: what's defended today, what isn't, and where the boundaries are. It is intentionally narrower than [SECURITY.md](../../SECURITY.md) (which covers how to *report* issues) — this file is about what the code actually does.

## Threat model

MarsOS is a single-address-space, cooperatively scheduled prototype OS. The threat model is therefore narrow on purpose:

| In scope | Out of scope (for now) |
|---|---|
| Memory-safety bugs in the kernel (bounded copies, overflows, UAF) | Anything an app does to itself or to another app — apps are trusted code |
| Input-parsing surfaces (PS/2, UEFI input, eventually FAT, network) | Side-channel timing attacks |
| Capability gating on privileged syscalls and event-bus channels | Hardware process isolation (no MMU separation today) |
| Boot-time integrity of `BOOTX64.EFI` (via Secure Boot signing in M3) | Anything that requires hardware isolation to be exploitable |
| Bounded resource consumption in event queues, processes, memory | DoS via crafted apps |

The boundary will tighten in M4 (MMU isolation, FAT write safety, preemption races) and M5 (network exposure). When those land, update the columns above.

## Asset inventory

| Asset | Where it lives | Why it matters |
|---|---|---|
| `BOOTX64.EFI` | [esp/EFI/BOOT/BOOTX64.EFI](../esp/EFI/BOOT/) | The only attacker-modifiable file with code execution. Defended at boot by Secure Boot (M3 signing) if enrolled. |
| Diag ring + stage history | RAM, [kernel/diag.c](../kernel/diag.c) | Tells operators what happened on a crash. Drained to serial so an attacker can't easily hide tracks even with RAM access. |
| Capability bitmap | [kernel/process.c](../kernel/process.c), per-process | Gates privileged syscalls and `EVENT_CHANNEL_SYSTEM` publishes. |
| Boot Services memory map | Captured pre-exit in `exit_boot_services` | After M3 ExitBootServices fully lands, this is the authoritative description of safe-to-use memory. |
| FAT volume | Block device at `vfs_mount_boot_device` | Read-only today. M4 makes it writable; corruption risk requires care. |

## Defenses in place

These are things the codebase actively defends against. Removing any of them is a regression — call it out in the PR.

- **Bounded string and payload copies.** All `os_strcpy16`, event payload writes, and similar are gated on explicit max sizes. See [test_input_security_fuzz.c](../tests/test_input_security_fuzz.c) for the fuzz coverage.
- **Null + alignment + payload-size checks** at the entry of public API surfaces. See [event_bus.c](../kernel/event_bus.c), [memory.c](../kernel/memory.c), [process.c](../kernel/process.c).
- **Bounded polling loops** on PS/2 I/O and `WaitForKey`-style probes ([drivers/mouse_ps2.c](../drivers/mouse_ps2.c), [drivers/mouse_uefi.c](../drivers/mouse_uefi.c)). A wedged device cannot hang the kernel.
- **Event queue full + drop accounting.** Producers can never block; dropped events are counted and visible in the WM debug overlay.
- **Capability checks** on every syscall that publishes to `EVENT_CHANNEL_SYSTEM` ([kernel/syscall.c](../kernel/syscall.c)). Removing the gate is the single highest-impact change you can make to the security posture; CI does not enforce this — reviewers must.
- **Process slot bounds** (`MAX_PROCESSES`, `MAX_TASKS`). Prevents process-table exhaustion.
- **Outstanding-page tracking** with explicit log on exhaustion. Not yet rate-limited; an app that loops alloc could fill the diag ring.
- **`-Wall -Wextra -Werror`** in CI. Sign-compare and uninitialized-use bugs fail the build.
- **13 host-side contract tests** ([os/tests/](../tests/)) covering event bus, scheduler fairness, process slot reuse, input bounds, heap, freelist, VM, security audit fixes for CWE-119/120/190/269/362/416/657/835/1025.
- **Panic dump.** Crashes can no longer hide — the dump in [kernel/panic.c](../kernel/panic.c) puts fault + last 20 stage transitions + last 64 diag records on serial before halting.

## Known gaps

These are areas with no defense today. Each is either out-of-scope (single-address-space prototype) or scheduled for a future milestone.

| Gap | Mitigation timeline |
|---|---|
| Apps share an address space with the kernel. A bad app can corrupt anything. | M4 — per-process CR3 install once VM builder is promoted. |
| FAT writes can corrupt the volume on power loss (no journaling). | M4 — write path with `fsync`-equivalent barrier; document "always shutdown cleanly" until then. |
| Preemption races on WM/app global state (`g_state` in `wm_state.c`, etc.). | M4 — big-WM-lock + per-subsystem audit. |
| Network parsing surfaces are stubs ([drivers/network.c](../drivers/network.c), [kernel/network.c](../kernel/network.c)) but will be exposed in M5. Must come with fuzz harnesses **before** the first real driver lands. | M5 — fuzz harness for ethernet/ARP/IPv4 parsers prior to PCI/e1000 enablement. |
| No rate limiting on diag_log. An app spamming syscalls can fill the ring and drown out real signal. | Open — track as follow-up; cheap to add a token bucket per (domain, code) pair. |
| `g_boot_services` consumers (post-M3) may dereference function-pointer tables that firmware unmapped. | M3 follow-up — investigation tracked in [HARDWARE_COMPAT.md](HARDWARE_COMPAT.md). |
| No ASLR / stack canary / W^X enforcement. `-fno-stack-protector` is required for the freestanding build today; once we have MMU isolation in M4, restoring stack canaries in user processes is the next defense layer. | M4 (after MMU lands) — re-enable for user code. Kernel remains canary-free by necessity. |

## Capability surface

Capabilities live in `process_t.capabilities` (bitmap). Today defined:

| Bit | Constant | Grants |
|---|---|---|
| 0x01 | `CAP_SYSTEM` | Publish to `EVENT_CHANNEL_SYSTEM` (app launch, supervisor control). |

Reviewer rule: any new syscall that affects a process other than the caller, or that emits to a non-broadcast channel, **must** check a capability. The capability check is the line between "untrusted code" and "trusted code" once M4's MMU isolation makes that distinction real.

## Audit checklist for new code

When reviewing or writing code in privileged paths, walk this list:

1. **Inputs validated at the boundary** — null checks, range checks, payload-size checks before any pointer arithmetic.
2. **Bounded loops** — every `for`/`while` that walks external data has a hard upper bound; document it if not obvious.
3. **No unbounded allocations** — page allocations have `MAX_*` ceilings; if not, justify in a comment.
4. **No new `g_boot_services` consumers** without checking `platform_context()->boot_services != NULL` first.
5. **Capability check** if the operation crosses process boundaries or touches a privileged channel.
6. **Host test** covers the happy path and at least one boundary condition. Run `make -C os test-host` locally before pushing.
7. **Diag log** on every error return path you care about debugging later. Don't log inside hot loops — log at the loop boundary.

## How to add a fuzz target

The pattern from [test_input_security_fuzz.c](../tests/test_input_security_fuzz.c) is reproducible:

1. Pick the function under test (must be deterministic and have a clear input bound).
2. Add `tests/test_<thing>_fuzz.c` driving it with `MAX_ITERS` random inputs from a seeded PRNG.
3. Wire it into [os/Makefile](../Makefile) `HOST_TEST_BINS` and the per-target rule.
4. Run locally; if it crashes or hangs, fix the function — do not weaken the test.

Targets to add (in priority order, all M4):
- Event bus payload boundaries.
- FAT directory entry parser (boundary on `name[0]` and `attributes`).
- Network packet headers (once stubs become real in M5).
