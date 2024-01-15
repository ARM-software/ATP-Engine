# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2020 ARM Limited
# All rights reserved.
# Authors: Adrian Herrera

"""
Official script for use with meta-atp Yocto layer
"""

from m5 import options
from m5.objects import *
from m5.util import addToPath
from argparse import ArgumentParser
from os import getenv
from pathlib import Path
from tempfile import NamedTemporaryFile

gem5_datadir = Path(getenv("GEM5_DATADIR"))
gem5_configs_dir = gem5_datadir / "configs"
atp_datadir = Path(getenv("ATP_DATADIR"))
atp_configs_dir = atp_datadir / "configs"
outdir = Path(options.outdir)

addToPath(gem5_configs_dir)

from common import (Benchmarks, CacheConfig, FSConfig, MemConfig, Options,
                    Simulation)

def connect_adapter(adapter, smmu):
    """Helper function to connect the adapter port to the SMMU"""
    interface = SMMUv3DeviceInterface()
    adapter.port = interface.device_port
    smmu.device_interfaces.append(interface)

def create_cow_image(name):
    """Helper function to create a Copy-on-Write disk image"""
    image = CowDiskImage()
    image.child.image_file = name
    return image

def create(args):
    # System
    CpuClass, mem_mode, _ = Simulation.setCPUClass(args)
    sys_cfg = Benchmarks.SysConfig(args.script, args.mem_size)
    system = FSConfig.makeArmSystem(mem_mode, "VExpress_GEM5_V2",
                                    args.num_cpus, sys_cfg, bare_metal=True)
    system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)
    system.clk_domain = SrcClockDomain(clock=args.sys_clock,
                                       voltage_domain=system.voltage_domain)
    system.workload.object_file = args.kernel
    # CPU cluster
    system.cpu_voltage_domain = VoltageDomain()
    system.cpu_clk_domain = SrcClockDomain(clock=args.cpu_clock,
            voltage_domain=system.cpu_voltage_domain)
    system.cpu = [CpuClass(clk_domain=system.cpu_clk_domain, cpu_id=i)
                  for i in range(args.num_cpus)]
    for cpu in system.cpu:
        cpu.createThreads()
    CacheConfig.config_cache(args, system)
    # Devices
    system.realview.atp_adapter = ProfileGen(config_files=args.atp_file,
                                             exit_when_done=False,
                                             init_only=True,
                                             disable_watchdog=True,
                                             disable_mem_check=True)
    system.realview.atp_device = ATPDevice(pio_addr=0x2b500000,
                                           interrupt=ArmSPI(num=104),
                                           atp_id="STREAM")
    system.realview.attachSmmu([system.realview.atp_device], system.membus)
    # Enable SMMUv3 interrupt interface to boot Linux
    system.realview.smmu.irq_interface_enable = True
    connect_adapter(system.realview.atp_adapter, system.realview.smmu)
    if args.disk_image:
        system.disk = [PciVirtIO(vio=VirtIOBlock(image=create_cow_image(disk)))
                       for disk in args.disk_image]
        for disk in system.disk:
            system.realview.attachPciDevice(disk, system.iobus)
    # Memory
    MemConfig.config_mem(args, system)

    return system

def main():
    parser = ArgumentParser(epilog=__doc__)
    Options.addCommonOptions(parser)
    Options.addFSOptions(parser)
    parser.add_argument("--atp-file", action="append", type=str,
                      default=[str(atp_configs_dir / "stream.atp")],
                      help=".atp file to load in Engine")
    parser.add_argument("--dtb-gen", action="store_true",
                      help="Doesn't run simulation, it generates a DTB only")
    parser.add_argument("--checkpoint", action="store_true")
    args = parser.parse_args()

    if args.checkpoint:
        script = NamedTemporaryFile()
        script.write(b"#!/bin/sh\nm5 checkpoint && m5 readfile | sh\n")
        script.flush()
        args.script = script.name
        args.max_checkpoints = 1

    system = create(args)
    root = Root(full_system=True, system=system)

    if args.dtb_gen:
        # No run, autogenerate DTB and exit
        system.generateDtb(str(outdir / "system.dtb"))
    else:
        Simulation.run(args, root, system, None)

if __name__ == "__m5_main__":
    main()
