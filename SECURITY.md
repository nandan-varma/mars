# Security Policy

## Scope

MarsOS is a UEFI x86_64 desktop prototype. It currently runs as a single-address-space, cooperatively scheduled kernel with capability-gated syscalls — there is **no hardware process isolation** today. Treat any reported vulnerability against this scope:

- The kernel trusts code that runs inside it. Apps cannot be considered untrusted principals yet.
- Boot Services stay live at runtime, so firmware-level guarantees apply during the entire session.
- There is no network stack and no persistent storage write path in mainline; bugs in those stubs are still in scope as defense-in-depth.

What we *do* care about, even at prototype stage:

- Memory safety in the kernel: bounded copies, null checks, alignment, queue-full handling.
- Capability gating in `os/kernel/syscall.c` and the event-bus `EVENT_CHANNEL_SYSTEM` publish path.
- Input parsing surfaces — keyboard, mouse PS/2, and (once enabled) FAT and network drivers.
- Boot-time integrity: anything that lets attacker-supplied bytes alter `BOOTX64.EFI` behavior before `kernel_main` runs.

Out of scope today:

- Side-channel timing attacks.
- Anything that requires hardware process isolation to even matter.
- DoS via crafted apps — apps are trusted code in the current model.

## Reporting a vulnerability

Please **do not file a public GitHub issue** for security reports. Instead:

1. Email the maintainer privately (the address on the most recent `git log` commit author works), or
2. Use GitHub's private vulnerability reporting on the repository's Security tab.

Include:

- A minimal reproducer (a patch, a crafted input, or steps).
- The commit SHA you tested against.
- Your assessment of impact — even "I'm not sure how exploitable this is" is useful.

You can expect:

- Acknowledgement within **7 days**.
- A first triage assessment within **14 days** (in scope / out of scope / needs more info).
- For in-scope issues: a fix or a published mitigation plan. Timeline depends on severity; nothing here runs in production so we aim for "promptly" rather than committing to fixed SLAs.

We are happy to credit reporters in release notes; tell us how you'd like to be credited (or not).

## Disclosure

This is a hobby/educational project. We will coordinate disclosure on a case-by-case basis. For anything that requires a CVE we will request one through GitHub's process.
