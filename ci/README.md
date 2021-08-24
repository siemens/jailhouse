Jailhouse Continuous Integration Build Environment
==================================================

This collects tools and generates the Linux kernel binaries required to build
Jailhouse in continuous integration environments. Currently, GitHub Actions is
the target environment.

How to use
----------

- Prepare an Ubuntu system according to .github/workflows/main.yaml.
- Run gen-kernel-build.sh on that system.
- Upload ci/out/kernel-build.tar.xz to the location where Jailhouse's CI
  expects it.
