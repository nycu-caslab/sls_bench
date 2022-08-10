#!/usr/bin/python
# @lint-avoid-python-3-compatibility-imports
#
# biopattern - Identify random/sequential disk access patterns.
#              For Linux, uses BCC, eBPF.
#
# Copyright (c) 2022 Rocky Xing.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# 21-Feb-2022   Rocky Xing   Created this.

from __future__ import print_function
from bcc import BPF
from time import sleep, strftime
import argparse
import os

examples = """examples:
    ./biopattern            # show block device I/O pattern.
    ./biopattern 1 10       # print 1 second summaries, 10 times
    ./biopattern -d sdb     # show sdb only
"""
parser = argparse.ArgumentParser(
    description="Show block device I/O pattern.",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-d", "--disk", type=str,
    help="Trace this disk only")
parser.add_argument("interval", nargs="?", default=99999999,
    help="Output interval in seconds")
parser.add_argument("count", nargs="?", default=99999999,
    help="Number of outputs")
args = parser.parse_args()
countdown = int(args.count)

bpf_text="""
#include <linux/blkdev.h>

struct counter {
    u64 last_sector;
    u64 bytes;
    u64 seq_bytes;
    u32 sequential;
    u64 rdm_bytes;
    u32 random;
    char name[TASK_COMM_LEN];
    u32 pid;
};

BPF_HASH(counters, u32, struct counter);

TRACEPOINT_PROBE(block, block_rq_complete)
{
    struct counter *counterp;
    struct counter zero = {};
    u32 dev = args->dev;
    u64 sector = args->sector;
    u32 nr_sector = args->nr_sector;

    DISK_FILTER

    counterp = counters.lookup_or_try_init(&dev, &zero);
    if (counterp == 0) {
        return 0;
    }

    counterp->pid = bpf_get_current_pid_tgid() >> 32;
    if (counterp->pid)
        bpf_get_current_comm(&counterp->name, sizeof(&counterp->name));

    if (counterp->last_sector) {
        if (counterp->last_sector == sector) {
            __sync_fetch_and_add(&counterp->sequential, 1);
            __sync_fetch_and_add(&counterp->seq_bytes, nr_sector * 512);
        } else {
            __sync_fetch_and_add(&counterp->random, 1);
            __sync_fetch_and_add(&counterp->rdm_bytes, nr_sector * 512);
        }
    }
    counterp->last_sector = sector + nr_sector;

    return 0;
}
"""

dev_minor_bits = 20

def mkdev(major, minor):
   return (major << dev_minor_bits) | minor


partitions = {}

with open("/proc/partitions", 'r') as f:
    lines = f.readlines()
    for line in lines[2:]:
        words = line.strip().split()
        major = int(words[0])
        minor = int(words[1])
        part_name = words[3]
        partitions[mkdev(major, minor)] = part_name

if args.disk is not None:
    disk_path = os.path.join('/dev', args.disk)
    if os.path.exists(disk_path) == False:
        print("no such disk '%s'" % args.disk)
        exit(1)

    stat_info = os.stat(disk_path)
    major = os.major(stat_info.st_rdev)
    minor = os.minor(stat_info.st_rdev)
    bpf_text = bpf_text.replace('DISK_FILTER',
                                'if (dev != %s) { return 0; }' % mkdev(major, minor))
else:
    bpf_text = bpf_text.replace('DISK_FILTER', '')

b = BPF(text=bpf_text)

exiting = 0 if args.interval else 1
counters = b.get_table("counters")

print("%-8s %-9s %-9s %-7s %-5s %-5s %-8s %-10s %-10s" % (
    "PID", "CMD", "TIME", "DISK", "RND", "SEQ", "COUNT", "RDM_BYTES", "SEQ_BYTES"))

while True:
    try:
        sleep(int(args.interval))
    except KeyboardInterrupt:
        exiting = 1
    
    for k, v in counters.items():
        total = v.random + v.sequential
        if total == 0:
            continue

        part_name = partitions.get(k.value, "Unknown")

        print("%8d %-9s %-9s %-7s %5d %5d %8d %10d %10d" % (
            v.pid,
            v.name.decode('utf-8', 'replace'),
            strftime("%H:%M:%S"),
            part_name,
            v.random,
            v.sequential,
            total,
            v.rdm_bytes,
            v.seq_bytes))

    counters.clear()

    countdown -= 1
    if exiting or countdown == 0:
        exit()

