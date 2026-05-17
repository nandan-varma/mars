# Hermetic build environment for MarsOS.
#
# Produces an image that can build BOOTX64.EFI and run all host-side tests
# byte-identically across machines. Pin to a specific Debian point release so
# upstream toolchain updates do not silently change build output.
#
# Usage:
#   docker build -t marsos-build .
#   docker run --rm -v "$PWD":/src -w /src/os marsos-build make ci

FROM debian:bookworm-20251110-slim

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    SOURCE_DATE_EPOCH=1700000000

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential=12.9 \
        gcc-mingw-w64-x86-64 \
        make \
        ca-certificates \
        qemu-system-x86 \
        ovmf \
 && rm -rf /var/lib/apt/lists/*

# Verify the toolchain is wired up. If this fails, the image build fails
# loudly rather than producing a half-broken image.
RUN x86_64-w64-mingw32-gcc --version >/dev/null \
 && cc --version >/dev/null \
 && qemu-system-x86_64 --version >/dev/null

WORKDIR /src

# Default command runs the full CI gate. Override with `make ...` to do
# something else (e.g. `make run` won't work inside a container without
# display/passthrough, but `make all test-host` will).
CMD ["make", "-C", "os", "ci"]
