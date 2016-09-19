FREQUENTLY ASKED QUESTIONS
==========================

General
-------

**Q: Linux already has KVM. Why do I need another hypervisor?**

A: Short answer: in most cases, you don't. There are many hypervisors available
in Linux: KVM, Xen, Oracle VM VirtualBox, to name a few. Most of them are
full-featured versatile solutions you can use in almost any case, including
real-time virtualization. However, specialized solution can optimize its size
and complexity more aggressively, thus can do better when it comes to real-time
and validation of its correct isolation.

Jailhouse is such a specialized hypervisor. It is all about static partitioning,
and it doesn't provide many features you'd expect from a virtual machine. There
is no overcommitting of resources, VM scheduling, or device emulation.

Instead, Jailhouse focuses on two main things: being small and simple, and
allowing guests (called "inmates") to execute with nearly-zero latencies. It is
not to substitute KVM on your desktop or server, it is to run real-time code,
including bare-metal applications and RTOSes. Jailhouse also aims to provide a
platform for mixing critical applications in functional safety scenarios.
It can also fulfill secure isolation requirements, although this was not the
focus so far.

**Q: Jailhouse is Asymmetric Multiprocessing (AMP). This means it will be slow due
to CPU cache thrashing.**

A: These concerns do have grounds. However, what is "slow" is determined by
Service Level Agreement (SLA), and we hope the effect will be negligible in the
majority of cases. Jailhouse faces the same problem here as cloud services do,
and they are expected to be quite successful despite it. Future processors may
introduce QoS mechanisms for cache control, which will be helpful as well. One
example for such a feature is Intel's Cache Allocation Technology (CAT) which is
announced for new Xeon processors. Of course, running code under Jailhouse is
slightly slower than on a dedicated uniprocessor machine, but virtualization
always comes at price.

Debugging
---------

**Q: When I enable Jailhouse or run an inmate, my machine hangs. How do I know
what's going on? Can I use dmesg, ftrace or similar tool?**

A: No. Jailhouse runs at the level lower than the Linux kernel, and if something
goes wrong, there are no guarantees that Linux can continue executing. Instead,
Jailhouse provides its own logging mechanism. In nested QEMU setup (see
README.md), log messages are simply sent to the virtual terminal; on real
hardware, you'll need a serial cable. Connect it to the COM port on your
motherboard. Many modern motherboards come with no COM ports, but they usually
have a header you can attach the socket to. Servers often have serial console
available through IPMI.

If everything else fails, consider buying a PCI serial adapter. Now, attach
a Linux machine to the other side of serial connection and use terminal emulator
like minicom to grab the log messages.

To enable error tracing, put ```#define CONFIG_TRACE_ERROR``` in file
hypervisor/include/jailhouse/config.h before compiling.

Please note Jailhouse developers may ask you for these logs, shall you come for
help to jailhouse-dev mailing list, because they are extremely useful to analyze
machine hangs. So please have the logs at hand, if possible.
