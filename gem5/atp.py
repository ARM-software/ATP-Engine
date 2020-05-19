# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2014-2015 ARM Limited
# All rights reserved.
# Authors: Matteo Andreozzi

from argparse import ArgumentParser
from os import walk
from os.path import join
import sys
import m5
from m5.objects import *
from m5.util import addToPath
from m5.stats import periodicStatDump

# this script tests the AMBA Traffic Profiles adaptor for GEM5, by loading an ATP
# configuration from file and instatiating an ATP generator plugged into a DRAM controller

parser = ArgumentParser()

parser.add_argument("--abs-max-tick", type=int, default=m5.MaxTick, metavar="TICKS", help = "Absolute number of ticks to simulate")

parser.add_argument("--dump-stats", type=int, default=1, help = "Number of times stats should be dumped")

parser.add_argument("--xbar-width", type=int ,default=128, help = "Crossbar width in bits")

parser.add_argument("--system-frequency", type=str ,default="1000000THz", help = "System frequency")

parser.add_argument("--system-voltage", type=str ,default="1V", help = "System voltage")

parser.add_argument("--master-ports", type=int, default=16, help= "Number of ATP Master ports")

parser.add_argument("--mem-type", type=str, default="LPDDR3_1600_1x32",
                    help = "type of memory to use")

parser.add_argument("--mem-channels", type=int, default=1, help = "Number of memory channels")

parser.add_argument("--mem-address-range", type=str, default="512MB", help = "System memory address range in MB")

parser.add_argument("--page-policy", type=str, choices=PageManage.vals, default='open_adaptive',
                    help="Page management policy")

parser.add_argument("--write-buffer-size", type=int, default=64,
                    help = "Write buffer size in packets")

parser.add_argument("--read-buffer-size", type=int, default=64,
                    help = "Read buffer size in packets")

parser.add_argument("--write_high_thresh_perc", type=int, default=85,
                    help = "Threshold to force writes")

parser.add_argument("--write_low_thresh_perc", type=int, default=50,
                    help = "Threshold to start writes")

parser.add_argument("--min_writes_per_switch", type=int, default=16,
                    help = "Minimum write bursts before switching to reads")

parser.add_argument("--max-accesses-per-row", type=int, default=16,
                    help="Max accesses per row before force closing a page")

# Note: default interleave size is max(128, system.cache_line_size.value)
parser.add_argument("--intlv-size", type=int,
                    help = "Memory Interleave Size (bytes)")

parser.add_argument("--addr_map", type=int, default=1,
                    help = "0: RoCoRaBaCh; 1: RoRaBaCoCh/RoRaBaChCo")

parser.add_argument("--gem5-configs-path", type=str,
                    default="../../gem5/configs", help="Path to gem5 configs")

parser.add_argument("--config_files", type=str, help = "ATP config file")
parser.add_argument("--config-paths", type=str, nargs="+", default=[],
                    help="Paths to walk for ATP files")

parser.add_argument("--trace-atp", action="store_true", help = "Enables tracing ATP generated packets")

parser.add_argument("--trace-gem", action="store_true", help = "Enables tracing GEM5 generated packets")

parser.add_argument("--trace-m3i", action="store_true", help = "Enables tracing GEM5 generated packets as m3i")

parser.add_argument("--exit-when-one-master-ends", action="store_true",
                    help = "Terminates the simulation when at least one ATP master depletes all \
                    its configured packets, across all of its traffic profiles")

parser.add_argument("--out-of-range-addresses", action="store_true",
                    help="Allows out of range addresses to be generated by ATP Engine")

parser.add_argument("--profiles-as-masters", action="store_true",
                    help = "Enables the creation of one master per configured profile (debug mode)")

parser.add_argument("--tracker-latency", action="store_true",
                    help= "Enables the ATP tracker latency")

parser.add_argument("--core-engine-debug", action="store_true",
                    help="Enables ATP Engine debug mode")
parser.add_argument("--init-only", action="store_true",
                    help="Initialises ATP Engine but does not trigger an update immediately after")
parser.add_argument("--disable-watchdog", action="store_true",
                    help="Disables ProfileGen watchdog")
parser.add_argument("--disable-mem-check", action="store_true",
                    help="Disables check for ATP Engine valid physical memory addresses")
parser.add_argument("--streams", type=str, nargs="*", default=[],
                    help=("Initial Streams of Traffic Profiles "
                          "<stream>,<master>[,R,<base>,<range>,W,<base>,"
                          "<range>,<task_id>]"))


parser.add_argument("--sink-request-latency", type=str, default="2ns",
                    help="Inter-request latency for QoS memory sink")

parser.add_argument("--sink-response-latency", type=str, default="80ns",
                    help="Request to response latency for QoS memory sink")

options = parser.parse_args()

addToPath(options.gem5_configs_path)
from common.MemConfig import config_mem
from common.ObjectList import mem_list

if options.mem_type not in mem_list.get_names():
    mem_list.print()
    fatal("Invalid memory type")

# start with the system itself, using a multi-layer X GHz
# crossbar, delivering N bytes / 3 cycles (one header cycle)
system = System(membus = IOXBar(width = options.xbar_width))
system.clk_domain = SrcClockDomain(clock = options.system_frequency,
                                   voltage_domain = VoltageDomain(voltage = options.system_voltage))

#set global frequency
m5.ticks.setGlobalFrequency(options.system_frequency)

# add memory
mem_range = AddrRange(options.mem_address_range)
system.mem_ranges = [mem_range]

# do not worry about reserving space for the backing store
mmap_using_noreserve = True

options.external_memory_system = 0
options.tlm_memory = 0
options.elastic_trace_en = 0
config_mem(options, system)

# the following assumes that we are using the native DRAM
# controller, check to be sure

update = lambda o, attr, v: setattr(o, attr, v) if hasattr(o, attr) else None
for mem_ctrl in system.mem_ctrls:
    mem_ctrl.null = True
    update(mem_ctrl, "write_buffer_size", options.write_buffer_size)
    update(mem_ctrl, "read_buffer_size", options.read_buffer_size)
    if isinstance(mem_ctrl, m5.objects.DRAMCtrl):
        mem_ctrl.write_high_thresh_perc = options.write_high_thresh_perc
        mem_ctrl.write_low_thresh_perc = options.write_low_thresh_perc
        mem_ctrl.min_writes_per_switch = options.min_writes_per_switch
        mem_ctrl.page_policy = options.page_policy
        mem_ctrl.max_accesses_per_row = options.max_accesses_per_row
        # Set the address mapping based on input argument
        if options.addr_map == 0:
            mem_ctrl.addr_mapping = "RoCoRaBaCh"
        elif options.addr_map == 1:
            mem_ctrl.addr_mapping = "RoRaBaCoCh"
        else:
            fatal("Did not specify a valid address map argument")
    elif isinstance(mem_ctrl, m5.objects.QoSMemSinkCtrl):
        mem_ctrl.request_latency = options.sink_request_latency
        mem_ctrl.response_latency = options.sink_response_latency

# disable ATP kill switches for configured simulation run time
atp_exit = False if options.abs_max_tick is not m5.MaxTick else True
master_exit = False if options.abs_max_tick is not m5.MaxTick else options.exit_when_one_master_ends

# helper for retrieving the set of ATP Files from user input
def get_atp_files(options):
    ret = set(options.config_files.split(" ") if options.config_files else [])
    for pathn in options.config_paths:
        for dirn, _, filens in walk(pathn):
            for filen in filens:
                ret.add(join(dirn, filen))
    return list(ret)

# create an ATP traffic generator, and point it to the file we just created
system.atp = ProfileGen(config_files = get_atp_files(options),
                        exit_when_done=atp_exit,
                        trace_atp=options.trace_atp,
                        trace_gem=options.trace_gem,
                        trace_m3i=options.trace_m3i,
                        exit_when_one_master_ends=master_exit,
                        out_of_range_addresses=options.out_of_range_addresses,
                        profiles_as_masters=options.profiles_as_masters,
                        tracker_latency=options.tracker_latency,
                        core_engine_debug=options.core_engine_debug,
                        init_only=options.init_only,
                        disable_watchdog=options.disable_watchdog,
                        disable_mem_check=options.disable_mem_check)

# Parse and initialise Streams
for stream in options.streams:
    stream = stream.split(',')
    rootp, master = stream[0:2]
    rbase = rrange = wbase = wrange = MaxAddr
    task_id = 1024
    if len(stream) == 3:
        task_id = int(stream[2])
    if len(stream) > 3:
        b, r = [ Addr(v) for v in stream[3:5] ]
        if stream[2] == 'R':
            rbase, rrange = b, r
        elif stream[2] == 'W':
            wbase, wrange = b, r
    if len(stream) == 6:
        task_id = int(stream[5])
    if len(stream) > 6:
        wbase, wrange = [ Addr(v) for v in stream[6:8] ]
    if len(stream) == 9:
        task_id = int(stream[8])
    system.atp.initStream(master, rootp, rbase, rrange, wbase, wrange, task_id)

# connect the ATP gem5 adaptor to the system bus (hence to the memory)
for i in xrange(options.master_ports):
    system.atp.port = system.membus.slave

# connect the system port even if it is not used in this example
system.system_port = system.membus.slave

root = Root(full_system = False, system = system)
root.system.mem_mode = 'timing'

m5.instantiate()

#take x stats snapshots
if options.abs_max_tick is not m5.MaxTick:
    period = int(options.abs_max_tick/options.dump_stats)
    periodicStatDump(period)

exit_event = m5.simulate(options.abs_max_tick)

print ('Exiting @ tick %i because %s' % (m5.curTick(), exit_event.getCause()))

sys.exit(exit_event.getCode())
