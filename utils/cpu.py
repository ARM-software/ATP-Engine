#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2017 ARM Limited
# All rights reserved
# Authors: Riken Gohil

import ttk
from Tkinter import *
from base import *

class CPU_UI():

    # Creates the options for CPU ATP files
    def create_cpu_options(self):
        self.cpu_point_chase_percentage = IntVar()
        self.cpu_data_limit = IntVar()
        self.cpu_data_size = IntVar()

        self.cpu_pc_ot_limit = IntVar()
        self.cpu_pc_read_addressing_type = StringVar()
        self.cpu_pc_read_addressing_type.set("LINEAR")
        self.cpu_pc_read_address_base = IntVar()
        self.cpu_pc_read_address_incr = IntVar()
        self.cpu_pc_read_address_min = IntVar()
        self.cpu_pc_read_address_max = IntVar()
        self.cpu_pc_read_address_std_dv = IntVar()
        self.cpu_pc_read_address_mean = IntVar()
        self.cpu_pc_read_address_shape = IntVar()
        self.cpu_pc_read_address_scale = IntVar()

        self.cpu_pc_write_addressing_type = StringVar()
        self.cpu_pc_write_addressing_type.set("LINEAR")
        self.cpu_pc_write_address_base = IntVar()
        self.cpu_pc_write_address_incr = IntVar()
        self.cpu_pc_write_address_min = IntVar()
        self.cpu_pc_write_address_max = IntVar()
        self.cpu_pc_write_address_std_dv = IntVar()
        self.cpu_pc_write_address_mean = IntVar()
        self.cpu_pc_write_address_shape = IntVar()
        self.cpu_pc_write_address_scale = IntVar()

        self.cpu_pc_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.cpu_pc_read_options = ttk.Panedwindow(self.cpu_pc_options, orient=VERTICAL)
        self.cpu_pc_read_linear_options = ttk.Panedwindow(self.cpu_pc_read_options, orient=VERTICAL)
        self.cpu_pc_read_uniform_options = ttk.Panedwindow(self.cpu_pc_read_options, orient=VERTICAL)
        self.cpu_pc_read_normal_options = ttk.Panedwindow(self.cpu_pc_read_options, orient=VERTICAL)
        self.cpu_pc_read_poisson_options = ttk.Panedwindow(self.cpu_pc_read_options, orient=VERTICAL)
        self.cpu_pc_read_weibull_options = ttk.Panedwindow(self.cpu_pc_read_options, orient=VERTICAL)

        self.cpu_pc_write_options = ttk.Panedwindow(self.cpu_pc_options, orient=VERTICAL)
        self.cpu_pc_write_linear_options = ttk.Panedwindow(self.cpu_pc_write_options, orient=VERTICAL)
        self.cpu_pc_write_uniform_options = ttk.Panedwindow(self.cpu_pc_write_options, orient=VERTICAL)
        self.cpu_pc_write_normal_options = ttk.Panedwindow(self.cpu_pc_write_options, orient=VERTICAL)
        self.cpu_pc_write_poisson_options = ttk.Panedwindow(self.cpu_pc_write_options, orient=VERTICAL)
        self.cpu_pc_write_weibull_options = ttk.Panedwindow(self.cpu_pc_write_options, orient=VERTICAL)

        self.cpu_options.add(Label(self.cpu_options, text = "CPU General Settings"))
        self.cpu_options.add(Label(self.cpu_options, text = "Pointer Chase Percentage: "))
        self.cpu_options.add(Scale(self.cpu_options, orient = HORIZONTAL, length = 100, variable = self.cpu_point_chase_percentage,
                                   command=self.generate_cpu_specific_options, from_ = 0.0, to = 100.0, resolution = 1 ))
        self.cpu_options.add(Label(self.cpu_options, text = "Data limit: "))
        self.cpu_options.add(ttk.Entry(self.cpu_options, textvariable=self.cpu_data_limit))
        self.cpu_options.add(Label(self.cpu_options, text = "Data size: "))
        self.cpu_options.add(ttk.Entry(self.cpu_options, textvariable=self.cpu_data_size))
        self.cpu_options.grid(sticky = N, row = 0, column = 2)

        self.cpu_pc_options.add(Label(self.cpu_pc_options, text = "Pointer Chase Settings"))
        self.cpu_pc_options.add(Label(self.cpu_pc_options, text = "OT Limit: "))
        self.cpu_pc_options.add(ttk.Entry(self.cpu_pc_options, textvariable=self.cpu_pc_ot_limit))

        self.cpu_pc_read_options.add(Label(self.cpu_pc_read_options, text = "Choose read address type: ",
                                           justify = LEFT, padx = 20))
        self.cpu_pc_read_options.add(ttk.OptionMenu(self.cpu_pc_read_options, self.cpu_pc_read_addressing_type, *self.address_types,
                                                    command=  lambda x : self.generate_address_options(self.cpu_pc_read_options,
                                                                                                       self.cpu_pc_read_addressing_type.get(), 2,
                                                                                                       self.cpu_pc_read_linear_options,
                                                                                                       self.cpu_pc_read_uniform_options,
                                                                                                       self.cpu_pc_read_normal_options,
                                                                                                       self.cpu_pc_read_poisson_options,
                                                                                                       self.cpu_pc_read_weibull_options)))

        self.create_addressing_options("READ", self.cpu_pc_read_linear_options, self.cpu_pc_read_uniform_options,
                                       self.cpu_pc_read_normal_options, self.cpu_pc_read_poisson_options, self.cpu_pc_read_weibull_options,
                                       self.cpu_pc_read_address_base, self.cpu_pc_read_address_incr, self.cpu_pc_read_address_min,
                                       self.cpu_pc_read_address_max, self.cpu_pc_read_address_std_dv, self.cpu_pc_read_address_mean,
                                       self.cpu_pc_read_address_shape, self.cpu_pc_read_address_scale)

        self.cpu_pc_read_options.add(self.cpu_pc_read_linear_options)
        self.cpu_pc_options.add(self.cpu_pc_read_options)

        self.cpu_pc_write_options.add(Label(self.cpu_pc_write_options, text = "Choose write address type: ",
                                            justify = LEFT, padx = 20))
        self.cpu_pc_write_options.add(ttk.OptionMenu(self.cpu_pc_write_options, self.cpu_pc_write_addressing_type, *self.address_types,
                                                     command=  lambda x : self.generate_address_options(self.cpu_pc_write_options,
                                                                                                        self.cpu_pc_write_addressing_type.get(), 2,
                                                                                                        self.cpu_pc_write_linear_options,
                                                                                                        self.cpu_pc_write_uniform_options,
                                                                                                        self.cpu_pc_write_normal_options,
                                                                                                        self.cpu_pc_write_poisson_options,
                                                                                                        self.cpu_pc_write_weibull_options)))

        self.create_addressing_options("WRITE", self.cpu_pc_write_linear_options, self.cpu_pc_write_uniform_options,
                                       self.cpu_pc_write_normal_options, self.cpu_pc_write_poisson_options, self.cpu_pc_write_weibull_options,
                                       self.cpu_pc_write_address_base, self.cpu_pc_write_address_incr, self.cpu_pc_write_address_min,
                                       self.cpu_pc_write_address_max, self.cpu_pc_write_address_std_dv, self.cpu_pc_write_address_mean,
                                       self.cpu_pc_write_address_shape, self.cpu_pc_write_address_scale)

        self.cpu_pc_write_options.add(self.cpu_pc_write_linear_options)
        self.cpu_pc_options.add(self.cpu_pc_write_options)

        self.cpu_mc_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.cpu_mc_read_options = ttk.Panedwindow(self.cpu_mc_options, orient=VERTICAL)
        self.cpu_mc_read_linear_options = ttk.Panedwindow(self.cpu_mc_read_options, orient=VERTICAL)
        self.cpu_mc_read_uniform_options = ttk.Panedwindow(self.cpu_mc_read_options, orient=VERTICAL)
        self.cpu_mc_read_normal_options = ttk.Panedwindow(self.cpu_mc_read_options, orient=VERTICAL)
        self.cpu_mc_read_poisson_options = ttk.Panedwindow(self.cpu_mc_read_options, orient=VERTICAL)
        self.cpu_mc_read_weibull_options = ttk.Panedwindow(self.cpu_mc_read_options, orient=VERTICAL)

        self.cpu_mc_write_options = ttk.Panedwindow(self.cpu_mc_options, orient=VERTICAL)
        self.cpu_mc_write_linear_options = ttk.Panedwindow(self.cpu_mc_write_options, orient=VERTICAL)
        self.cpu_mc_write_uniform_options = ttk.Panedwindow(self.cpu_mc_write_options, orient=VERTICAL)
        self.cpu_mc_write_normal_options = ttk.Panedwindow(self.cpu_mc_write_options, orient=VERTICAL)
        self.cpu_mc_write_poisson_options = ttk.Panedwindow(self.cpu_mc_write_options, orient=VERTICAL)
        self.cpu_mc_write_weibull_options = ttk.Panedwindow(self.cpu_mc_write_options, orient=VERTICAL)

        self.cpu_mc_ot_limit = IntVar()
        self.cpu_mc_fifo_full_level= IntVar()
        self.cpu_mc_bandwidth = IntVar()

        self.cpu_mc_read_addressing_type = StringVar()
        self.cpu_mc_read_addressing_type.set("LINEAR")
        self.cpu_mc_read_address_base = IntVar()
        self.cpu_mc_read_address_incr = IntVar()
        self.cpu_mc_read_address_min = IntVar()
        self.cpu_mc_read_address_max = IntVar()
        self.cpu_mc_read_address_std_dv = IntVar()
        self.cpu_mc_read_address_mean = IntVar()
        self.cpu_mc_read_address_shape = IntVar()
        self.cpu_mc_read_address_scale = IntVar()

        self.cpu_mc_write_addressing_type = StringVar()
        self.cpu_mc_write_addressing_type.set("LINEAR")
        self.cpu_mc_write_address_base = IntVar()
        self.cpu_mc_write_address_incr = IntVar()
        self.cpu_mc_write_address_min = IntVar()
        self.cpu_mc_write_address_max = IntVar()
        self.cpu_mc_write_address_std_dv = IntVar()
        self.cpu_mc_write_address_mean = IntVar()
        self.cpu_mc_write_address_shape = IntVar()
        self.cpu_mc_write_address_scale = IntVar()

        self.cpu_mc_options.add(Label(self.cpu_mc_options, text = "Memory Copy Settings"))
        self.cpu_mc_options.add(Label(self.cpu_mc_options, text = "OT Limit: "))
        self.cpu_mc_options.add(ttk.Entry(self.cpu_mc_options, textvariable=self.cpu_mc_ot_limit))
        self.cpu_mc_options.add(Label(self.cpu_mc_options, text = "FIFO Full Level: "))
        self.cpu_mc_options.add(ttk.Entry(self.cpu_mc_options, textvariable=self.cpu_mc_fifo_full_level))
        self.cpu_mc_options.add(Label(self.cpu_mc_options, text = "Bandwidth (B/s): "))
        self.cpu_mc_options.add(ttk.Entry(self.cpu_mc_options, textvariable=self.cpu_mc_bandwidth))

        self.cpu_mc_read_options.add(Label(self.cpu_mc_read_options, text = "Choose read address type: ",
                                           justify = LEFT, padx = 20))
        self.cpu_mc_read_options.add(ttk.OptionMenu(self.cpu_mc_read_options, self.cpu_mc_read_addressing_type, *self.address_types,
                                                    command=  lambda x : self.generate_address_options(self.cpu_mc_read_options,
                                                                                                       self.cpu_mc_read_addressing_type.get(), 2,
                                                                                                       self.cpu_mc_read_linear_options,
                                                                                                       self.cpu_mc_read_uniform_options,
                                                                                                       self.cpu_mc_read_normal_options,
                                                                                                       self.cpu_mc_read_poisson_options,
                                                                                                       self.cpu_mc_read_weibull_options)))

        self.create_addressing_options("READ", self.cpu_mc_read_linear_options, self.cpu_mc_read_uniform_options,
                                       self.cpu_mc_read_normal_options, self.cpu_mc_read_poisson_options, self.cpu_mc_read_weibull_options,
                                       self.cpu_mc_read_address_base, self.cpu_mc_read_address_incr, self.cpu_mc_read_address_min,
                                       self.cpu_mc_read_address_max, self.cpu_mc_read_address_std_dv, self.cpu_mc_read_address_mean,
                                       self.cpu_mc_read_address_shape, self.cpu_mc_read_address_scale)

        self.cpu_mc_read_options.add(self.cpu_mc_read_linear_options)
        self.cpu_mc_options.add(self.cpu_mc_read_options)

        self.cpu_mc_write_options.add(Label(self.cpu_mc_write_options, text = "Choose write address type: ",
                                            justify = LEFT, padx = 20))
        self.cpu_mc_write_options.add(ttk.OptionMenu(self.cpu_mc_write_options, self.cpu_mc_write_addressing_type, *self.address_types,
                                                     command=  lambda x : self.generate_address_options(self.cpu_mc_write_options,
                                                                                                        self.cpu_mc_write_addressing_type.get(), 2,
                                                                                                        self.cpu_mc_write_linear_options,
                                                                                                        self.cpu_mc_write_uniform_options,
                                                                                                        self.cpu_mc_write_normal_options,
                                                                                                        self.cpu_mc_write_poisson_options,
                                                                                                        self.cpu_mc_write_weibull_options)))

        self.create_addressing_options("WRITE", self.cpu_mc_write_linear_options, self.cpu_mc_write_uniform_options,
                                       self.cpu_mc_write_normal_options, self.cpu_mc_write_poisson_options, self.cpu_mc_write_weibull_options,
                                       self.cpu_mc_write_address_base, self.cpu_mc_write_address_incr, self.cpu_mc_write_address_min,
                                       self.cpu_mc_write_address_max, self.cpu_mc_write_address_std_dv, self.cpu_mc_write_address_mean,
                                       self.cpu_mc_write_address_shape, self.cpu_mc_write_address_scale)

        self.cpu_mc_write_options.add(self.cpu_mc_write_linear_options)
        self.cpu_mc_options.add(self.cpu_mc_write_options)


        self.cpu_pc_options.grid(sticky = N, row = 0, column = 3)
        self.cpu_mc_options.grid(sticky = N, row = 0, column = 4)

    # Selects whether the CPU options should display pointer chase options,
    # memory copy options or both
    def generate_cpu_specific_options(self, pointer_chase_percentage):
        if ( int(pointer_chase_percentage) == 0 ):
            self.change_widget_state(self.cpu_pc_options, 'disable')
            self.change_widget_state(self.cpu_mc_options, 'normal')
        elif ( int(pointer_chase_percentage) == 100 ):
            self.change_widget_state(self.cpu_pc_options, 'normal')
            self.change_widget_state(self.cpu_mc_options, 'disable')
        else:
            self.change_widget_state(self.cpu_pc_options, 'normal')
            self.change_widget_state(self.cpu_mc_options, 'normal')

class CPU_Gen(Base_Gen):

    # Gets the settings for pointer chase
    def get_settings_pc(self):
        return dict(self.settings, **self.settings_pc)

    # Gets the defaults for pointer chase
    def get_defaults_pc(self):
        defaults = {"start_fifo_level" : "EMPTY", "full_level" : "0",
                "rate":"0", "master_id" : "CPU_1"}
        return dict(self.get_settings_pc(), **defaults)

    # Gets the read defaults for pointer chase
    def get_read_defaults_pc(self):
        return {"type" : "READ", "cmd" : "READ_REQ",
                "name" : "CPU_POINTER_READS_1", "base" : "0"}

    # Gets the write defaults for pointer chase
    def get_write_defaults_pc(self):
        return {"type" : "WRITE", "cmd" : "WRITE_REQ",
                "name" :"CPU_POINTER_WRITES_1",
                "wait_for_0" : "CPU_POINTER_READS_1 PROFILE_LOCKED",
                "base" : "0"}

    # Gets the defaults for memory copy
    def get_settings_mc(self):
        return dict(self.settings, **self.settings_mc)

    # Gets the defaults for memory copy
    def get_defaults_mc(self):
        defaults = {"master_id" : "CPU_1"}
        return dict(self.get_settings_mc(), **defaults)

    # Gets the read defaults for memory copy
    def get_read_defaults_mc(self):
        return {"type" : "READ", "cmd" : "READ_REQ",
                "name" : "CPU_MEMCPY_READS", "base" : "0",
                "start_fifo_level" : "FULL", "wait_for_0" : "CPU_POINTER_READS_1",
                "wait_for_1" : "CPU_POINTER_WRITES_1"}

    # Gets the write defaults for memory copy
    def get_write_defaults_mc(self):
        return {"type":"WRITE", "cmd":"WRITE_REQ",
                "name":"CPU_MEMCPY_WRITES", "wait_for_0": "CPU_MEMCPY_READS PROFILE_LOCKED",
                "wait_for_1" : "CPU_POINTER_READS_1",
                "wait_for_2" : "CPU_POINTER_WRITES_1",
                "base" : "0", "start_fifo_level" : "EMPTY"}

    # Gets information about the ATP profile
    def get_info(self, defaults_pc, defaults_mc):
        info = ( '\n# AMBA Traffic Profile CPU:' +
                 '\n#' )
        if (self.settings["pointer_chase"] > 0):
            info += ('\n# {pointer_chase_percentage}% CPU Pointer Chasing:' +
                     '\n# It does simulate a CPU issuing instruction with {ot_limit} max OT each' +
                     '\n# PC OT: {ot_limit}' +
                     '\n# PC FIFO : disabled (rate {rate}, level {full_level})' +
                     '\n# PC Total bandwidth OT Limited (READS/WRITES 1:1)' )
            info += self.get_address_info(defaults_pc, "READ", "PC ")
            info += self.get_address_info(defaults_pc, "WRITE", "PC ")
            info += ('\n#' )
            info = info.format(**defaults_pc)
        if (self.settings["pointer_chase"] < 1):
            info += ('\n# {mem_copy_percentage}% CPU MEM COPY in foreground (flat bandwidth)' +
                     '\n# It does simulate a CPU doing a mem copy operation' +
                     '\n# MC OT: {ot_limit}' +
                     '\n# MC FIFO : {full_level}' +
                     '\n# MC Total bandwidth {bandwidth} (READS/WRITES 1:1)' )
            info += self.get_address_info(defaults_mc, "READ", "MC ")
            info += self.get_address_info(defaults_mc, "WRITE", "MC ")
            info += ('\n#' )
            info = info.format(**defaults_mc)
        info += ('\n# Total Data Transferred: ' )
        if (self.settings["pointer_chase"] > 0):
            info += ( '\n# CPU PC Data: {total_txn}*{size} Bytes' ).format(**defaults_pc)
        if (self.settings["pointer_chase"] < 1):
            info += ( '\n# CPU MC Data: {total_txn}*{size} Bytes' ).format(**defaults_mc)
        info += ('\n\nlowId:0' +
                 '\nhighId:2014')

        return info

    # Generates the CPU ATP profile
    def generate(self):
        defaults_pc = self.get_defaults_pc()
        defaults_mc = self.get_defaults_mc()
        mem_copy_read_wait_for = 0
        mem_copy_write_wait_for = 1
        total_txn = self.settings["data_limit"] / self.settings["size"]

        if (self.settings["pointer_chase"] > 0):
            mem_copy_read_wait_for = 2
            mem_copy_write_wait_for = 3

            defaults_pc["total_txn"] = str(int(total_txn * self.settings["pointer_chase"]))
            defaults_pc["pointer_chase_percentage"] = self.settings["pointer_chase"] * 100

            read_defaults_pc = dict(self.get_read_defaults_pc(), **defaults_pc)
            read_pc_address_random_type = self.get_random("read_", "address", read_defaults_pc, defaults_pc)

            write_defaults_pc = dict(self.get_write_defaults_pc(), **defaults_pc)
            if ( read_pc_address_random_type is random_type.linear ):
                write_defaults_pc["base"] = hex(int(read_defaults_pc["base"]) +
                                                (total_txn * read_defaults_pc["increment"]))
            write_pc_address_random_type = self.get_random("write_", "address", write_defaults_pc, defaults_pc)


        if (self.settings["pointer_chase"] < 1):
            defaults_mc["mem_copy_percentage"] = (1 - self.settings["pointer_chase"]) * 100
            defaults_mc["total_txn"] = str(int(total_txn * (1 - self.settings["pointer_chase"])))
            defaults_mc["rate"] = self.convert_rate(defaults_mc["bandwidth"] / 2)
            defaults_mc["bandwidth"] = self.convert_rate(defaults_mc["bandwidth"])

            read_defaults_mc = dict(self.get_read_defaults_mc(), **defaults_mc)
            read_mc_address_random_type = self.get_random("read_", "address", read_defaults_mc, defaults_mc)

            write_defaults_mc = dict(self.get_write_defaults_mc(), **defaults_mc)
            if ( read_mc_address_random_type is random_type.linear ):
                write_defaults_mc["base"] = hex(read_defaults_mc["base"] +
                                             (total_txn * read_defaults_mc["increment"]))
            write_mc_address_random_type = self.get_random("write_", "address", write_defaults_mc, defaults_mc)

        profile = self.get_copyright()
        profile += self.get_info(defaults_pc, defaults_mc)
        if (self.settings["pointer_chase"] > 0):
            profile += self.get_base_profile(0, read_pc_address_random_type, random_type.fixed).format(**read_defaults_pc)

            profile += self.get_base_profile(1, write_pc_address_random_type, random_type.fixed).format(**write_defaults_pc)
        if (self.settings["pointer_chase"] < 1):
            profile += self.get_base_profile(mem_copy_read_wait_for, read_mc_address_random_type, random_type.fixed).format(**read_defaults_mc)

            profile += self.get_base_profile(mem_copy_write_wait_for, write_mc_address_random_type, random_type.fixed).format(**write_defaults_mc)

        return profile

    def __init__(self, settings, settings_pc, settings_mc):
        self.settings = settings
        self.settings_pc = settings_pc
        self.settings_mc = settings_mc