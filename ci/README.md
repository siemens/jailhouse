Jailhouse Continuous Integration Build Environment
==================================================

This collects tools and generates the Linux kernel binaries required to build
Jailhouse in continuous integration environments. Currently, travis-ci.org is
the target environment.

How to use
----------

- Prepare an Ubuntu system according to the
  [travis-ci specifications](http://docs.travis-ci.com/user/ci-environment)
  or via the [Chef recipes](https://github.com/travis-ci/travis-cookbooks).
- Run gen-kernel-build.sh on that system.
- Upload ci/out/kernel-build.tar.xz to the location where Jailhouse's
  .travis.yml expects it.
