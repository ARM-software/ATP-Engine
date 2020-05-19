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

class DPU_UI():

    # Creates the options for DPU ATP files
    def create_dpu_options(self):
        self.dpu_x = IntVar()
        self.dpu_y = IntVar()
        self.dpu_fps = IntVar()
        self.dpu_layers = IntVar()
        self.dpu_bpp = IntVar()
        self.dpu_size = IntVar()
        self.dpu_timing = DoubleVar()
        self.dpu_total_txn = IntVar()
        self.dpu_base_adjust_options = ["", "Yes", "No"]

        self.dpu_addressing_type = StringVar()
        self.dpu_addressing_type.set("LINEAR")
        self.dpu_address_base = IntVar()
        self.dpu_address_incr = IntVar()
        self.dpu_address_base_adjust = StringVar()
        self.dpu_address_base_adjust.set("Yes")
        self.dpu_address_min = IntVar()
        self.dpu_address_max = IntVar()
        self.dpu_address_std_dv = IntVar()
        self.dpu_address_mean = IntVar()
        self.dpu_address_shape = IntVar()
        self.dpu_address_scale = IntVar()

        self.dpu_options.add(Label(self.dpu_options, text = "DPU General Settings"))
        self.dpu_options.add(Label(self.dpu_options, text = "X Pixels: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_x))
        self.dpu_options.add(Label(self.dpu_options, text = "Y Pixels: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_y))
        self.dpu_options.add(Label(self.dpu_options, text = "FPS: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_fps))
        self.dpu_options.add(Label(self.dpu_options, text = "Number of Layers: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_layers))
        self.dpu_options.add(Label(self.dpu_options, text = "Bits per Pixel: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_bpp))
        self.dpu_options.add(Label(self.dpu_options, text = "Data size: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_size))
        self.dpu_options.add(Label(self.dpu_options, text = "Timing: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_timing))
        self.dpu_options.add(Label(self.dpu_options, text = "Total Transactions: "))
        self.dpu_options.add(ttk.Entry(self.dpu_options, textvariable=self.dpu_total_txn))

        self.dpu_address_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.dpu_linear_options = ttk.Panedwindow(self.dpu_address_options, orient=VERTICAL)
        self.dpu_uniform_options = ttk.Panedwindow(self.dpu_address_options, orient=VERTICAL)
        self.dpu_normal_options = ttk.Panedwindow(self.dpu_address_options, orient=VERTICAL)
        self.dpu_poisson_options = ttk.Panedwindow(self.dpu_address_options, orient=VERTICAL)
        self.dpu_weibull_options = ttk.Panedwindow(self.dpu_address_options, orient=VERTICAL)

        self.dpu_address_options.add(Label(self.dpu_address_options, text = "DPU Address Options",
                                           justify = LEFT, padx = 20))
        self.dpu_address_options.add(Label(self.dpu_address_options, text = "Choose read address type: ",
                                           justify = LEFT, padx = 20))
        self.dpu_address_options.add(ttk.OptionMenu(self.dpu_address_options, self.dpu_addressing_type, *self.address_types,
                                                    command=  lambda x : self.generate_address_options(self.dpu_address_options,
                                                                                                       self.dpu_addressing_type.get(), 3,
                                                                                                       self.dpu_linear_options,
                                                                                                       self.dpu_uniform_options,
                                                                                                       self.dpu_normal_options,
                                                                                                       self.dpu_poisson_options,
                                                                                                       self.dpu_weibull_options)))

        self.create_addressing_options("READ", self.dpu_linear_options, self.dpu_uniform_options,
                                       self.dpu_normal_options, self.dpu_poisson_options, self.dpu_weibull_options,
                                       self.dpu_address_base, self.dpu_address_incr, self.dpu_address_min,
                                       self.dpu_address_max, self.dpu_address_std_dv, self.dpu_address_mean,
                                       self.dpu_address_shape, self.dpu_address_scale)

        self.dpu_address_options.add(self.dpu_linear_options)

class DPU_Gen(Base_Gen):

    # Gets the defaults for the DPU
    def get_defaults(self):
        defaults = {"type" : "READ", "master_id" : "DPU_1",
                    "start_fifo_level" : "FULL", "cmd" : "READ_REQ" }
        return dict(self.settings, **defaults)

    # Gets the information about the ATP profile
    def get_info(self, defaults):
        info = ( '\n# AMBA Traffic Profile DPU:' +
                 '\n#' +
                 '\n# It does simulate DPU processing' +
                 '\n# X pixels: {x_pixels}' +
                 '\n# Y pixels: {y_pixels}' +
                 '\n# FPS: {FPS}' +
                 '\n# Bits per Pixel: {bits_per_pixel}' +
                 '\n# Layers: {layers}' +
                 '\n# Data Size: {size}' +
                 '\n# Total Transactions: {total_txn}' +
                 '\n# Timings: {timing}' )
        info += self.get_address_info(defaults, "READ", "")
        info += ( '\n# Total needed rate: {x_pixels} * {y_pixels} * {FPS} * {bits_per_pixel} / 8 = ' +
                  '\n# {rate} on {layers} layers.' +
                  '\n# FULL Level : data rate * {timing}s' +
                  '\n# OT : FULL Level / Packet Size' +
                  '\n\nlowId:0' +
                  '\nhighId:1024')

        return info.format(**defaults)

    # Generates the ATP profile
    def generate(self):
        defaults = self.get_defaults()
        rate = ( defaults["x_pixels"] * defaults["y_pixels"] *
                 defaults["FPS"] * defaults["bits_per_pixel"] / 8 )

        defaults["full_level"] = self.next_greatest_power_of_2(int(round(rate *
                                                               defaults["timing"])))
        defaults["ot_limit"] = int(round(defaults["full_level"] / defaults["size"]))
        defaults["full_level"] = self.validate_full_level(rate, defaults["size"], defaults["full_level"])
        defaults["rate"] = self.convert_rate(rate)

        address_random_type = self.get_random("read_", "address", defaults, defaults)

        profile = self.get_copyright()
        profile += self.get_info(defaults)
        for i in range(defaults["layers"]):
            defaults["name"] = "DPU_1_LAYER_" + str(i+1)
            if ( i and address_random_type is random_type.linear and defaults["base_adjust"] == "yes" ) :
                defaults["base"] = defaults["base"] + (defaults["total_txn"] * defaults["increment"])
            profile += self.get_base_profile(0, address_random_type, random_type.fixed).format(**defaults)

        return profile