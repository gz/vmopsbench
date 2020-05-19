#!/usr/bin/python3

import argparse
import os
import sys
import pathlib
import shutil
import pexpect

from plumbum import colors, local
from plumbum.cmd import git, apt_get, sudo, mkdir, bash, make

#
# run.py script settings
#
SCRIPT_PATH = pathlib.Path(os.path.dirname(os.path.realpath(__file__)))

MENU_LST = """
timeout 0

title Barrelfish
root (nd)
kernel /x86_64/sbin/elver loglevel=3
module /x86_64/sbin/cpu loglevel=3
module /x86_64/sbin/init

# Domains spawned by init
module /x86_64/sbin/mem_serv
module /x86_64/sbin/monitor

# Special boot time domains spawned by monitor
module  /x86_64/sbin/ramfsd boot
module  /x86_64/sbin/skb boot
modulenounzip /eclipseclp_ramfs.cpio.gz nospawn
modulenounzip /skb_ramfs.cpio.gz nospawn
module  /x86_64/sbin/kaluga boot
module  /x86_64/sbin/acpi boot
module  /x86_64/sbin/spawnd boot
module  /x86_64/sbin/proc_mgmt boot
# bootapic-x86_64=1-15
module  /x86_64/sbin/startd boot

# Drivers
module /x86_64/sbin/corectrl auto
# module /x86_64/sbin/ahcid auto

#
module /x86_64/sbin/vmops_array_mcn -p {} {} -b {} -m {}
"""

#
# Command line argument parser
#
parser = argparse.ArgumentParser()
# General build arguments
parser.add_argument("-v", "--verbose", action="store_true",
                    help="increase output verbosity")
parser.add_argument("-n", "--norun", action="store_true",
                    help="Only build, don't run")
parser.add_argument("-c", "--cores", type=int,
                    help="How many cores to run on")
parser.add_argument("-b", "--benchmark",
                    help="How many cores to run on")
parser.add_argument("-o", "--nops", type=int, default=-1,
                    help="How many operations should be done")
parser.add_argument("-t", "--time", type=int, default=-1,
                    help="how long to run the benchmark")
parser.add_argument("--hake", action="store_true", default=False,
                    help="Run hake to regen Makefile")
parser.add_argument("--csvthpt", type=str,
                    help="CSV file for throughput measurements")
parser.add_argument("--csvlat",  type=str,
                    help="CSV file for latency measurements")


def log(msg):
    print(colors.bold | ">>>", end=" "),
    print(colors.bold.reset & colors.info | msg)


SCRIPT_PATH = pathlib.Path(os.path.dirname(os.path.realpath(__file__)))
BARRELFISH_CHECKOUT_DIR = SCRIPT_PATH
BARRELFISH_DIR = BARRELFISH_CHECKOUT_DIR / 'barrelfish'
BARRELIFH_BUILD = BARRELFISH_DIR / 'build'
VMOPS_DIR = SCRIPT_PATH / '..'
MENU_LST_PATH = BARRELFISH_DIR / 'build' / \
    'platforms' / 'x86' / 'menu.lst.x86_64'

RESULTS_PATH = SCRIPT_PATH / '..'





def get_barreflish(args):
    if not BARRELFISH_DIR.exists():
        log("Checking out Barrelfish")

        with local.cwd(BARRELFISH_CHECKOUT_DIR):
            git(['clone', 'https://github.com/achreto/barrelfish.git'])
    else:
        log("Update Barrelfish to latest version")
        with local.cwd(BARRELFISH_CHECKOUT_DIR):
            git(['pull', 'origin', 'master'])


def install_deps(args):
    deps = ['build-essential',
            'g++',
            'gcc-arm-linux-gnueabi',
            'g++-arm-linux-gnueabi',
            'gcc-aarch64-linux-gnu',
            'g++-aarch64-linux-gnu',
            'binutils',
            'make',
            'cabal-install',
            'libghc-src-exts-dev',
            'libghc-ghc-paths-dev',
            'libghc-parsec3-dev',
            'libghc-random-dev',
            'libghc-ghc-mtl-dev',
            'libghc-async-dev',
            'libghc-aeson-pretty-dev',
            'libghc-aeson-dev',
            'libghc-missingh-dev',
            'libelf-freebsd-dev',
            'freebsd-glue',
            'qemu-system-x86',
            'qemu-efi',
            ]
    log("Install dependencies")
    args = ['apt-get', '-y', 'install'] + deps
    sudo(*args)
#    from plumbum.cmd import cabal
#    cabal['update']()
#    cabal['install', 'bytestring-trie', 'pretty-simple']()


def deploy_vmops(args):
    log("Deploying vmops in Barrelfish build")
    VMOPS_IN_BF_DIR = BARRELFISH_DIR / 'usr' / 'vmops'
    VMOPS_IN_BF_DIR.mkdir(parents=True, exist_ok=True)
    shutil.rmtree(VMOPS_IN_BF_DIR /
                  'src', ignore_errors=True)
    shutil.copytree(VMOPS_DIR / 'src', VMOPS_IN_BF_DIR /
                    'src')
    shutil.copy2(VMOPS_DIR / 'Hakefile',
                 VMOPS_IN_BF_DIR / 'Hakefile')


def build_barrelfish(args):
    log("Building Barrelfish")
    with local.cwd(BARRELFISH_DIR):
        mkdir['-p', 'build']()
        with local.cwd(BARRELIFH_BUILD):
            hake_cmd = ['../hake/hake.sh', '-s', '../',
                        '-a', 'x86_64', '-f', '-g -O2 -DNDEBUG']
            if args.verbose:
                print("cd {}".format(BARRELIFH_BUILD))
            if args.hake:
                if args.verbose:
                    print(" ".join(hake_cmd))
                bash(*hake_cmd)

            make_args = ['-j', '6', 'X86_64_Basic']
            if args.verbose:
                print("make " + " ".join(make_args))
            make(*make_args)

            make_args = ['-j', '6', 'x86_64/sbin/vmops_array_mcn']
            if args.verbose:
                print("make " + " ".join(make_args))
            make(*make_args)


# #module /x86_64/sbin/vmops_array_mcn -p 3 -b maponly-isolated -n 2000
# #module /x86_64/sbin/vmops_array_mcn -p 3 -b mapunmap-isolated -n 6000
# #module /x86_64/sbin/vmops_array_mcn -p 3 -b protect-isolated -n 2000
# module /x86_64/sbin/vmops_array_mcn -p 3 -b elevate-isolated -m 40960000 -n 10000
# for the last one: make -m =(4096 * -n)

def run_barrelfish(args):
    if args.norun:
        return True

    log("Running Barrelfish {}".format(args.cores))

    with open(MENU_LST_PATH, 'w') as menu_lst_file:
        if args.nops != -1:
            bench_arg = "-o {} -s {} -r 0".format(args.nops, args.nops)
        elif args.time != -1:
            bench_arg = "-t {}".format(args.time)
        else:
            bench_arg = "-t 4000"
        mem = "4096"
        if args.benchmark.startswith("elevate") :
            mem = "40960000"

        my_menu = MENU_LST.format(args.cores, bench_arg, args.benchmark, mem)
        if args.verbose:
            print("Using the following generated menu.lst")
            print(my_menu)
        menu_lst_file.write(my_menu)

    with local.cwd(BARRELIFH_BUILD):
        cmd_args = ["../tools/qemu-wrapper.sh",
                    "--menu", "platforms/x86/menu.lst.x86_64", "--arch", "x86_64"]
        if args.verbose:
            print(" ".join(cmd_args))
        CSV_ROW_BEGIN = "===================== BEGIN CSV ====================="
        CSV_ROW_END = "====================== END CSV ======================"
        print("Running with timeout: %d" % (60+args.cores*90))
        qemu_instance = pexpect.spawn(
            ' '.join(cmd_args), cwd=BARRELIFH_BUILD, env={'SMP': str(34), 'MEMORY': '48G'}, timeout=120+args.cores*90)


        qemu_instance.expect("Checking HUGEPAGE availability")
        memopt = ["NO HUGEPAGES AVAILABLE", "USING HUGE MEM OPTION 1GB", "USING HUGE MEM OPTION 2MB"]
        idx = qemu_instance.expect(memopt)
        print("FOUND: %s" % memopt[idx]);
        if idx == 0 :
            print("WARNING!!!!! NO HUGE PAGES FOR KVM!!!")

        print(qemu_instance.before.decode('utf-8'))

        qemu_instance.expect("KVM is -enable-kvm")

        qemu_instance.expect(CSV_ROW_BEGIN)
        qemu_instance.expect(CSV_ROW_END)

        results = qemu_instance.before.decode('utf-8')
        with open(RESULTS_PATH / args.csvthpt, 'a') as results_file:
            print(results.strip())
            results_file.write(results.strip() + "\n")

        if args.nops != -1 :
            qemu_instance.expect("====================== BEGIN STATS ======================")
            qemu_instance.expect("====================== END STATS ======================")
            results = qemu_instance.before.decode('utf-8')

            with open(RESULTS_PATH / args.csvlat, 'a') as results_file:
                #print(results.strip())
                results_file.write(results.strip() + "\n")

        qemu_instance.terminate(force=True)



if __name__ == '__main__':
    "Execution pipeline for building and launching bespin"
    args = parser.parse_args()
    install_deps(args)
    get_barreflish(args)
    deploy_vmops(args)
    build_barrelfish(args)
    run_barrelfish(args)
