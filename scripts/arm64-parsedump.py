#!/usr/bin/env python

# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
#
# Authors:
#  Dmitry Voytik <dmitry.voytik@huawei.com>
#
# ARM64 dump parser.
# Usage ./scripts/arm64-parsedump.py [dump.txt]
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.


from __future__ import print_function
import subprocess
import sys
import fileinput
import os
import argparse

split1 = "Cell's stack before exception "
split2 = "Hypervisor stack before exception "


# yep, this is most important feature
class Col:
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    FAIL = '\033[91m'

    @staticmethod
    def init():
        t = os.environ['TERM']
        if t == '' or t == 'dumb' or t == 'vt220' or t == 'vt100':
            # The terminal doesn't support colors
            Col.ENDC = ''
            Col.BOLD = ''
            Col.FAIL = ''

    @staticmethod
    def bold(string):
        return Col.BOLD + str(string) + Col.ENDC

    @staticmethod
    def pr_err(string):
        print(Col.FAIL + "ERROR: " + Col.ENDC + str(string))

    @staticmethod
    def pr_note(string):
        print(Col.BOLD + "NOTE: " + Col.ENDC + str(string))


def addr2line(addr):
    return subprocess.check_output(["addr2line", "-a", "-f", "-p", "-e",
                                   objpath, hex(addr)])


def print_faddr(addr):
    s = str(addr2line(addr))
    if s.find("?") != -1:
        print("[{:#016x}] {}".format(addr, Col.bold("uknown")))
        return
    s = s.strip().split(" ")
    print("[{}] {} {}".format(s[0][:-1], Col.bold(s[1]),
          s[3].split('/')[-1]))


class Dump:
    def __init__(self, dump_str):
        if len(dump_str) < 50:
            raise ValueError('Dump is too small')
        # parse CPU state
        pc_i = dump_str.find("pc:") + 4
        self.pc = int(dump_str[pc_i: pc_i+16], 16)
        pc_i = dump_str.find("sp:") + 4
        self.sp = int(dump_str[pc_i: pc_i+16], 16)
        el_i = dump_str.rfind("EL") + 2
        self.el = int(dump_str[el_i:el_i+1])
        if (self.el != 2):
            Col.pr_err("This version supports only EL2 exception dump")

        # TODO: parse other registers: ESR, etc

        # parse stack dump
        stack_start = str.rfind(dump_str, split1)
        if (stack_start == -1):
            stack_start = str.rfind(dump_str, split2)
            if (stack_start == -1):
                raise ValueError('Dump is damaged')

        stack_str = dump_str[stack_start:].strip().split('\n')
        stack_addr_start = int(stack_str[0][35:53], 16)
        stack_addr_end = int(stack_str[0][56:74], 16)

        # parse stack memory dump
        stack = []
        for line in stack_str[1:]:
            if (len(line) < 5):
                continue
            if (line[4] != ':'):
                continue
            line = line[5:].strip().split(" ")
            for value in line:
                stack.append(int(value, 16))

        self.stack_mem = stack
        self.stack_start = stack_addr_start
        self.stack_end = stack_addr_end

    def stack_get64(self, addr):
        assert addr >= self.sp
        i = int((addr - self.sp) / 4)
        hi32 = self.stack_mem[i]
        lo32 = self.stack_mem[i + 1]
        return lo32 + (hi32 << 32)

    def print_unwinded_stack(self):
        print_faddr(self.pc)
        addr = self.sp
        while True:
            prev_sp = self.stack_get64(addr)
            print_faddr(self.stack_get64(addr+4))
            addr = prev_sp
            if (addr > self.stack_end - 256):
                break


def main():
    Col.init()

    parser = argparse.ArgumentParser(description='ARM64 exception dump parser')
    parser.add_argument('--objpath', '-o', default="./hypervisor/hypervisor.o",
                        type=str, help="Path to hypervisor.o file")
    parser.add_argument('-f', '--filedump', default="", type=str,
                        help="Exception dump text file")
    args = parser.parse_args()

    global objpath
    objpath = args.objpath

    stdin_used = False
    infile = [args.filedump]
    if args.filedump == "":
        infile = []
        Col.pr_note("Input dumped text then press Enter, Control+D, Control+D")
        stdin_used = True

    ilines = []
    for line in fileinput.input(infile):
        ilines.append(line)
    dump_str = "".join(ilines)
    if (not stdin_used):
        print(dump_str)
    else:
        print("\n")
    try:
        dump = Dump(dump_str)
    except ValueError as err:
        Col.pr_err(err)
        return
    dump.print_unwinded_stack()

if __name__ == "__main__":
    main()
