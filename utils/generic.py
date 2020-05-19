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

class Generic_UI():

    # Creates the options used when generating generic ATP files
    def create_generic_options(self):
        self.generic_copyright = "not added"
        self.generic_type = StringVar()
        self.generic_type.set("READ")
        self.generic_master_id = StringVar()
        self.generic_start_fifo_level = StringVar()
        self.generic_start_fifo_level.set("EMPTY")
        self.generic_fifo_full_level = IntVar()
        self.generic_ot_limit = IntVar()
        self.generic_total_txn = IntVar()
        self.generic_rate = IntVar()
        self.generic_cmd = StringVar()
        self.generic_name = StringVar()
        self.fifo_level_list = ["", "EMPTY", "FULL"]
        self.type_list = ["", "READ", "WRITE"]

        self.generic_addressing_type = StringVar()
        self.generic_addressing_type.set("LINEAR")
        self.generic_address_base = IntVar()
        self.generic_address_incr = IntVar()
        self.generic_address_base_adjust = StringVar()
        self.generic_address_base_adjust.set("Yes")
        self.generic_address_min = IntVar()
        self.generic_address_max = IntVar()
        self.generic_address_std_dv = IntVar()
        self.generic_address_mean = IntVar()
        self.generic_address_shape = IntVar()
        self.generic_address_scale = IntVar()

        self.generic_size_type = StringVar()
        self.generic_size_type.set("FIXED")
        self.generic_size = IntVar()
        self.generic_size_min = IntVar()
        self.generic_size_max = IntVar()
        self.generic_size_std_dv = IntVar()
        self.generic_size_mean = IntVar()
        self.generic_size_shape = IntVar()
        self.generic_size_scale = IntVar()

        self.generic_options.add(Label(self.generic_options, text = "Generic General Settings"))
        self.generic_options.add(Label(self.generic_options, text = "Name"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_name))
        self.generic_options.add(Label(self.generic_options, text = "Master ID"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_master_id))
        self.generic_options.add(Label(self.generic_options, text = "Transaction Type: "))
        self.generic_options.add(ttk.OptionMenu(self.generic_options, self.generic_type, *self.type_list))
        self.generic_options.add(Label(self.generic_options, text = "Total Transactions"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_total_txn))
        self.generic_options.add(Label(self.generic_options, text = "Rate (B/s)"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_rate))
        self.generic_options.add(Label(self.generic_options, text = "Start FIFO Level"))
        self.generic_options.add(ttk.OptionMenu(self.generic_options, self.generic_start_fifo_level, *self.fifo_level_list))
        self.generic_options.add(Label(self.generic_options, text = "FIFO Full Level"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_fifo_full_level))
        self.generic_options.add(Label(self.generic_options, text = "OT Limit"))
        self.generic_options.add(ttk.Entry(self.generic_options, textvariable=self.generic_ot_limit))

        self.generic_address_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.generic_linear_options = ttk.Panedwindow(self.generic_address_options, orient=VERTICAL)
        self.generic_uniform_options = ttk.Panedwindow(self.generic_address_options, orient=VERTICAL)
        self.generic_normal_options = ttk.Panedwindow(self.generic_address_options, orient=VERTICAL)
        self.generic_poisson_options = ttk.Panedwindow(self.generic_address_options, orient=VERTICAL)
        self.generic_weibull_options = ttk.Panedwindow(self.generic_address_options, orient=VERTICAL)

        self.generic_size_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.generic_size_fixed_options = ttk.Panedwindow(self.generic_size_options, orient=VERTICAL)
        self.generic_size_uniform_options = ttk.Panedwindow(self.generic_size_options, orient=VERTICAL)
        self.generic_size_normal_options = ttk.Panedwindow(self.generic_size_options, orient=VERTICAL)
        self.generic_size_poisson_options = ttk.Panedwindow(self.generic_size_options, orient=VERTICAL)
        self.generic_size_weibull_options = ttk.Panedwindow(self.generic_size_options, orient=VERTICAL)

        self.generic_address_options.add(Label(self.generic_address_options, text = "Generic Address Options",
                                               justify = LEFT, padx = 20))
        self.generic_address_options.add(Label(self.generic_address_options, text = "Choose read address type: ",
                                               justify = LEFT, padx = 20))
        self.generic_address_options.add(ttk.OptionMenu(self.generic_address_options, self.generic_addressing_type, *self.address_types,
                                                        command=  lambda x : self.generate_address_options(self.generic_address_options,
                                                                                                           self.generic_addressing_type.get(), 3,
                                                                                                           self.generic_linear_options,
                                                                                                           self.generic_uniform_options,
                                                                                                           self.generic_normal_options,
                                                                                                           self.generic_poisson_options,
                                                                                                           self.generic_weibull_options)))

        self.generic_size_options.add(Label(self.generic_size_options, text = "Generic Size Options",
                                               justify = LEFT, padx = 20))
        self.generic_size_options.add(Label(self.generic_size_options, text = "Choose data size size type: ",
                                               justify = LEFT, padx = 20))
        self.generic_size_options.add(ttk.OptionMenu(self.generic_size_options, self.generic_size_type, *self.size_types,
                                                        command=  lambda x : self.generate_size_options(self.generic_size_options,
                                                                                                        self.generic_size_type.get(), 3,
                                                                                                        self.generic_size_fixed_options,
                                                                                                        self.generic_size_uniform_options,
                                                                                                        self.generic_size_normal_options,
                                                                                                        self.generic_size_poisson_options,
                                                                                                        self.generic_size_weibull_options)))

        self.create_addressing_options("", self.generic_linear_options, self.generic_uniform_options,
                                       self.generic_normal_options, self.generic_poisson_options,
                                       self.generic_weibull_options, self.generic_address_base,
                                       self.generic_address_incr, self.generic_address_min,
                                       self.generic_address_max, self.generic_address_std_dv,
                                       self.generic_address_mean, self.generic_address_shape,
                                       self.generic_address_scale)

        self.create_size_options("Data", self.generic_size_fixed_options, self.generic_size_uniform_options,
                                       self.generic_size_normal_options, self.generic_size_poisson_options,
                                       self.generic_size_weibull_options, self.generic_size,
                                       self.generic_size_min, self.generic_size_max,
                                       self.generic_size_std_dv, self.generic_size_mean,
                                       self.generic_size_shape, self.generic_size_scale)

        self.generic_address_options.add(self.generic_linear_options)
        self.generic_size_options.add(self.generic_size_fixed_options)

class Generic_Gen(Base_Gen):

    # Generates the ATP profile
    def generate(self, copyright):
        random_address_type = self.get_random(self.type + "_", "address", self.settings, self.settings)
        random_size_type = self.get_random("", "size", self.settings, self.settings)

        self.settings["rate"] = self.convert_rate(self.settings["rate"])

        if (copyright == "not added"):
            profile = self.get_copyright()
        else:
            profile = ""
        profile += self.get_base_profile(0, random_address_type, random_size_type).format(**self.settings)

        return profile

    def __init__(self, settings, type):
        self.settings = settings
        self.type = type