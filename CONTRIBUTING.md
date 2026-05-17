# Contributing to MarsOS

MarsOS is a UEFI x86_64 desktop prototype written in freestanding C. It targets QEMU today and real hardware as a stated goal; see the production-readiness milestones for the broader trajectory.

## Quick start

```sh
# macOS
./os/scripts/setup_macos_toolchain.sh

# Debian / Ubuntu (also what CI uses)
./os/scripts/setup_linux_toolchain.sh

# Build everything + run host tests
make -C os ci

# Run under QEMU
make -C os run

# Hermetic build (matches CI byte-for-byte)
docker build -t marsos-build .
docker run --rm -v "$PWD":/src -w /src marsos-build make -C os ci
```

If `make -C os ci` is green and `make -C os run` reaches the desktop, your environment is good.

## What we expect from a PR

1. **`-Werror` clean.** Both the EFI build (`make -C os all`) and host tests (`make -C os test-host`) build with `-Wall -Wextra -Werror`. CI rejects warnings.
2. **Host tests still pass.** If you change a subsystem with a contract test (event bus, scheduler, process, memory, input bounds, VM), keep it green or update the test alongside the change.
3. **Style matches [AGENTS.md](AGENTS.md).** Naming, indentation, header order, bounded copies, capability checks — that file is the authority.
4. **Keep public headers stable.** Anything under [os/include/](os/include/) is part of the contract: `wm.h`, `app.h`, `event_bus.h`, `memory.h`, `vm.h`, etc. Internal refactors are fine; signature changes need a reason.
5. **Preserve invariants.** Event-bus queue/drop semantics, mouse source priority (PS/2 → Absolute → Simple), kernel bring-up ordering with preserved stage markers, framebuffer bounds checks.
6. **No libc.** Use UEFI types from [os/include/uefi.h](os/include/uefi.h). Avoid `<stdio.h>`, `<stdlib.h>`, etc. — they aren't available in the freestanding runtime.

## Adding a host test

Host tests live in [os/tests/](os/tests/) and run on your dev machine, not under UEFI. They link against real kernel sources directly so they exercise the same code path the firmware would.

1. Create `os/tests/test_<thing>.c`.
2. Add a build rule and entry to `HOST_TEST_BINS` in [os/Makefile](os/Makefile) (see existing entries around line 268).
3. `make -C os test-host` should run it and exit non-zero on failure.

## Commits and PRs

- Commits explain **why**, not what. The diff already shows what.
- Prefer many small commits over one giant one — easier to review and bisect later.
- One topic per PR. Refactors, behavior changes, and dependency bumps go in separate PRs.
- If you change something subtle (timing, locking, invariant), call it out in the PR description so reviewers know where to look.

## Things to avoid

- Large new subsystems without prior discussion — file an issue first.
- Unbounded loops, unbounded allocations, missing null checks in new code.
- Adding TODO/FIXME comments to land work. If something is incomplete, file an issue and link it.
- Refactoring the WM during invariant-sensitive milestones (see the production-readiness plan); preserve it under existing locks.

## Reporting bugs

Open an issue with:
- What you ran (`make -C os run` flags, host OS, toolchain).
- Serial output if available (`make -C os run` already wires `-serial stdio`).
- Whether `make -C os test-host` is green on your tree.

For security issues, see [SECURITY.md](SECURITY.md) — do not file a public issue first.
