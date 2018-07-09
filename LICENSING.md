Jailhouse Licensing
===================

The Jailhouse hypervisor is primarily licensed under the terms of the GNU
General Public License version 2.

Each of its source code files contains a license declaration in its header.
Whenever a file is provided under an additional or different license than
GPLv2, this is stated in the file header. Any file that may lack such a
header has to be considered licensed under GPLv2 (default license).

The binary font file hypervisor/arch/x86/altc-8x16 was taken from the KBD
project in version 2.0.4 (https://www.kernel.org/pub/linux/utils/kbd,
http://kbd-project.org) which is distributed under the GNU GPL version 2 or
later (SPDX identifier GPL-2.0-or-later).

If two licenses are specified in a file header, you are free to pick the one
that suits best your particular use case. You can also continue to use the
file under the dual license. When choosing only one, remove the reference to
the other from the file header.


License Usage
-------------

The default license GPLv2 shall be used unless valid reasons are provided for a
deviation. Note the additional statement in GPLv2.txt about which code is not
considered derivative work of Jailhouse before considering a different license.

For code that shall be licensed under more permissive terms, a dual-license
model of GPLv2 together with the BSD 2-clause license is preferred. This form
can be applicable on

  - interfaces to code that runs inside Jailhouse cells
  - library-like code that applications or operating systems can include to
    run in a Jailhouse cell
  - generated configuration files


License Header Format
---------------------

Use the following template (replacing comment markers as required) when
creating a new file in the Jailhouse project:

```
/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) <COPYRIGHT HOLDER>, <YEAR>
 *
 * Authors:
 *  Your Name <your.email@host.dom>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
```

When applying a dual GPL/BSD license, append the following to the above:

```
 ...
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
```
