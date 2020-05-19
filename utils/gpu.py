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

class GPU_UI():
    # Creates the options for GPU ATP files
    def create_gpu_options(self):
        self.gpu_x = IntVar()
        self.gpu_y = IntVar()
        self.gpu_fps = IntVar()
        self.gpu_frames = IntVar()
        self.gpu_bpp = IntVar()
        self.gpu_size = IntVar()
        self.gpu_total_txn = IntVar()
        self.gpu_ot = IntVar()

        self.gpu_options.add(Label(self.gpu_options, text = "GPU General Settings"))
        self.gpu_options.add(Label(self.gpu_options, text = "X Pixels: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_x))
        self.gpu_options.add(Label(self.gpu_options, text = "Y Pixels: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_y))
        self.gpu_options.add(Label(self.gpu_options, text = "FPS: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_fps))
        self.gpu_options.add(Label(self.gpu_options, text = "Number of Frames: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_frames))
        self.gpu_options.add(Label(self.gpu_options, text = "Bytes per Pixel: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_bpp))
        self.gpu_options.add(Label(self.gpu_options, text = "Data size: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_size))
        self.gpu_options.add(Label(self.gpu_options, text = "Total Transactions: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_total_txn))
        self.gpu_options.add(Label(self.gpu_options, text = "OT Limit: "))
        self.gpu_options.add(ttk.Entry(self.gpu_options, textvariable=self.gpu_ot))

        self.gpu_read_addressing_type = StringVar()
        self.gpu_read_addressing_type.set("LINEAR")
        self.gpu_read_address_base = IntVar()
        self.gpu_read_address_incr = IntVar()
        self.gpu_read_address_base_adjust = StringVar()
        self.gpu_read_address_base_adjust.set("Yes")
        self.gpu_read_address_min = IntVar()
        self.gpu_read_address_max = IntVar()
        self.gpu_read_address_std_dv = IntVar()
        self.gpu_read_address_mean = IntVar()
        self.gpu_read_address_shape = IntVar()
        self.gpu_read_address_scale = IntVar()

        self.gpu_read_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.gpu_read_linear_options = ttk.Panedwindow(self.gpu_read_options, orient=VERTICAL)
        self.gpu_read_uniform_options = ttk.Panedwindow(self.gpu_read_options, orient=VERTICAL)
        self.gpu_read_normal_options = ttk.Panedwindow(self.gpu_read_options, orient=VERTICAL)
        self.gpu_read_poisson_options = ttk.Panedwindow(self.gpu_read_options, orient=VERTICAL)
        self.gpu_read_weibull_options = ttk.Panedwindow(self.gpu_read_options, orient=VERTICAL)

        self.gpu_read_options.add(Label(self.gpu_read_options, text = "GPU READ Options", justify = LEFT, padx = 20))
        self.gpu_read_options.add(Label(self.gpu_read_options, text = "Choose read address type: ", justify = LEFT, padx = 20))
        self.gpu_read_options.add(ttk.OptionMenu(self.gpu_read_options, self.gpu_read_addressing_type, *self.address_types,
                                                 command=  lambda x : self.generate_address_options(self.gpu_read_options,
                                                                                                    self.gpu_read_addressing_type.get(), 3,
                                                                                                    self.gpu_read_linear_options,
                                                                                                    self.gpu_read_uniform_options,
                                                                                                    self.gpu_read_normal_options,
                                                                                                    self.gpu_read_poisson_options,
                                                                                                    self.gpu_read_weibull_options)))

        self.create_addressing_options("READ", self.gpu_read_linear_options, self.gpu_read_uniform_options,
                                       self.gpu_read_normal_options, self.gpu_read_poisson_options, self.gpu_read_weibull_options,
                                       self.gpu_read_address_base, self.gpu_read_address_incr, self.gpu_read_address_min,
                                       self.gpu_read_address_max, self.gpu_read_address_std_dv, self.gpu_read_address_mean,
                                       self.gpu_read_address_shape, self.gpu_read_address_scale)

        self.gpu_write_addressing_type = StringVar()
        self.gpu_write_addressing_type.set("LINEAR")
        self.gpu_write_address_base = IntVar()
        self.gpu_write_address_incr = IntVar()
        self.gpu_write_address_base_adjust = StringVar()
        self.gpu_write_address_base_adjust.set("Yes")
        self.gpu_write_address_min = IntVar()
        self.gpu_write_address_max = IntVar()
        self.gpu_write_address_std_dv = IntVar()
        self.gpu_write_address_mean = IntVar()
        self.gpu_write_address_shape = IntVar()
        self.gpu_write_address_scale = IntVar()

        self.gpu_write_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.gpu_write_linear_options = ttk.Panedwindow(self.gpu_write_options, orient=VERTICAL)
        self.gpu_write_uniform_options = ttk.Panedwindow(self.gpu_write_options, orient=VERTICAL)
        self.gpu_write_normal_options = ttk.Panedwindow(self.gpu_write_options, orient=VERTICAL)
        self.gpu_write_poisson_options = ttk.Panedwindow(self.gpu_write_options, orient=VERTICAL)
        self.gpu_write_weibull_options = ttk.Panedwindow(self.gpu_write_options, orient=VERTICAL)

        self.gpu_write_options.add(Label(self.gpu_write_options, text = "GPU WRITE Options", justify = LEFT, padx = 20))
        self.gpu_write_options.add(Label(self.gpu_write_options, text = "Choose write address type: ", justify = LEFT, padx = 20))
        self.gpu_write_options.add(ttk.OptionMenu(self.gpu_write_options, self.gpu_write_addressing_type, *self.address_types,
                                                  command=  lambda x : self.generate_address_options(self.gpu_write_options,
                                                                                                     self.gpu_write_addressing_type.get(), 3,
                                                                                                     self.gpu_write_linear_options,
                                                                                                     self.gpu_write_uniform_options,
                                                                                                     self.gpu_write_normal_options,
                                                                                                     self.gpu_write_poisson_options,
                                                                                                     self.gpu_write_weibull_options)))

        self.create_addressing_options("WRITE", self.gpu_write_linear_options, self.gpu_write_uniform_options,
                                       self.gpu_write_normal_options, self.gpu_write_poisson_options, self.gpu_write_weibull_options,
                                       self.gpu_write_address_base, self.gpu_write_address_incr, self.gpu_write_address_min,
                                       self.gpu_write_address_max, self.gpu_write_address_std_dv, self.gpu_write_address_mean,
                                       self.gpu_write_address_shape, self.gpu_write_address_scale)

        self.gpu_read_options.add(self.gpu_read_linear_options)
        self.gpu_write_options.add(self.gpu_write_linear_options)

class GPU_Gen(Base_Gen):

    # Gets the defaults for the GPU
    def get_defaults(self):
        defaults = {"master_id" : "GPU", }
        return dict(self.settings, **defaults)

    # Gets the read defaults for the GPU
    def get_read_defaults(self):
        return {"type" : "READ", "start_fifo_level" : "FULL",
                "name" : "GPU_READS", "cmd" : "READ_REQ"}

    # Gets the write defaults for the GPU
    def get_write_defaults(self):
        return {"type" : "WRITE", "start_fifo_level" : "EMPTY",
                "name" : "GPU_WRITES", "cmd" : "WRITE_REQ",
                "wait_for_0" : "GPU_READS PROFILE_LOCKED"}

    # Gets the information about the ATP profile
    def get_info(self, defaults):
        info = ( '\n# AMBA Traffic Profile GPU (flat bandwidth):' +
                 '\n#' +
                 '\n# Reading / Writing data equal to {x_pixels} * {y_pixels} * {bytes_per_pixel} bytes per pixel' +
                 '\n# ({frames} frame) at {FPS} Frames per second: {rate}' +
                 '\n# ')
        info += ('\n# X pixels: {x_pixels}' +
                 '\n# Y pixels: {y_pixels}' +
                 '\n# FPS: {FPS}' +
                 '\n# Bytes per Pixel: {bytes_per_pixel}' +
                 '\n# Frames: {frames}' +
                 '\n# Data Size: {size}' +
                 '\n# Total Transactions: {total_txn}' +
                 '\n# OT: {ot_limit}' +
                 '\n# FIFO : OT*PacketSize = {full_level}' )
        info += self.get_address_info(defaults, "READ", "")
        info += self.get_address_info(defaults, "WRITE", "")
        info += ('\n#' +
                 '\n\nlowId:0' +
                 '\nhighId:1024' )

        return info.format(**defaults)

    # Generates the ATP profile
    def generate(self):
        defaults = self.get_defaults()
        rate = ( defaults["x_pixels"] * defaults["y_pixels"] * defaults["FPS"] *
                 defaults["bytes_per_pixel"] * defaults["frames"] )
        defaults["full_level"] = defaults["ot_limit"] * defaults["size"]
        defaults["full_level"] = self.validate_full_level(rate, defaults["size"], defaults["full_level"])
        defaults["rate"] = self.convert_rate(rate)

        read_defaults = dict(defaults, **self.get_read_defaults())
        read_address_random_type = self.get_random("read_", "address", read_defaults, defaults)

        write_defaults = dict(defaults, **self.get_write_defaults())
        if ( read_address_random_type is random_type.linear ):
            write_defaults["base"] = hex(read_defaults["base"] +
                                         (defaults["total_txn"] * read_defaults["increment"]))
        write_address_random_type = self.get_random("write_", "address", write_defaults, defaults)

        profile = self.get_copyright()
        profile += self.get_info(defaults)
        profile += self.get_base_profile(0, read_address_random_type, random_type.fixed).format(**read_defaults)
        profile += self.get_base_profile(1, write_address_random_type, random_type.fixed).format(**write_defaults)

        return profile