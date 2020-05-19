#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2013-2014, 2017 ARM Limited
# All rights reserved
# Authors: Riken Gohil
#

import ttk
import Tkconstants, tkFileDialog
import datetime, platform, linecache
from Tkinter import *
from base import *
from generic import *
from cpu import *
from dpu import *
from gpu import *

class App(Generic_UI, CPU_UI, DPU_UI, GPU_UI):

    # Initialises the GUI with default options needed for each processor
    def __init__(self, root):
        root.title("ATP File Generator")
        root.resizable(False, False)

        self.m = Menu(root)
        root.config(menu=self.m)
        self.fm = Menu(self.m, tearoff=0)
        self.m.add_cascade(label="File", menu=self.fm)

        if (platform.system() == "Darwin"):
            self.create_shortcuts("Command", "Command")
        else:
            self.create_shortcuts("Control", "Ctrl")

        self.default = ttk.Frame(root)
        self.options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.generic_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.cpu_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.dpu_options = ttk.Panedwindow(self.default, orient=VERTICAL)
        self.gpu_options = ttk.Panedwindow(self.default, orient=VERTICAL)

        self.atp_string = ""
        self.processor = StringVar()
        self.author = StringVar()
        self.processor_list = ["", "CPU", "DPU", "GPU", "Generic"]
        self.processor.set("CPU")
        self.address_types = ["", "LINEAR", "UNIFORM", "NORMAL", "POISSON", "WEIBULL"]
        self.size_types = ["", "FIXED", "UNIFORM", "NORMAL", "POISSON", "WEIBULL"]

        self.options.add(Label(self.options, text = "General Settings", justify = LEFT))
        self.options.add(Label(self.options, text = "Choose a processor type: ",
                               justify = LEFT))
        self.options.add(ttk.OptionMenu(self.options, self.processor, *self.processor_list,
                                        command=lambda x: self.choose_processor_options()))
        self.options.add(Label(self.options, text = "Author: "))
        self.options.add(ttk.Entry(self.options, textvariable = self.author))
        self.button_generate_file = ttk.Button(self.options, text = "Generate ATP Profile",
                                               command=self.print_atp)
        self.options.add(self.button_generate_file)
        self.button_generate_profile = ttk.Button(self.options, text = "Add ATP Profile",
                                                  command=self.append_atp)
        self.button_generate_profile.configure(state = "disable")
        self.options.add(self.button_generate_profile)
        self.button_preview_file = ttk.Button(self.options, text = "Preview ATP Profile",
                                                  command=self.preview_atp)
        self.button_preview_file.configure(state = "enable")
        self.options.add(self.button_preview_file)
        self.options.add(ttk.Button(self.options, text = "Open ATP Profile",
                                    command=self.open_atp))
        self.options.add(ttk.Button(self.options, text = "Clear",
                                    command= lambda : self.clear_fields(self.default)))
        self.options.add(ttk.Button(self.options, text = "Quit", command=quit))

        self.options.grid(sticky = N, row = 0, column = 1)
        self.create_processor_options()
        self.default.pack()

    # Recursively changes the state of all widgets within a frame
    def change_widget_state(self, frame, state):
        for child in frame.winfo_children():
            if (isinstance(child, PanedWindow)):
                self.change_widget_state(child, state)
            else:
                child.configure(state = state)

    # Recursively clears all fields with a frame, and resets to a zero
    def clear_fields(self, frame):
        self.atp_string = ""
        for child in frame.winfo_children():
            if (isinstance(child, PanedWindow)):
                self.clear_fields(child)
            elif (isinstance(child, ttk.Entry)):
                child.delete(0, 'end')
                child.insert(0, 0)

    # Converts the rate back to B/s for reading ATP files
    def convert_rate_to_KBS(self, rate):
        if (" B/s" in rate):
            return int(rate[:rate.find("/") - 4])
        elif (" KB/s" in rate):
            return int(rate[:rate.find("K") - 3]) * 1000
        elif (" MB/s" in rate):
            return int(rate[:rate.find("M") - 3]) * 1000000
        elif (" GB/s" in rate):
            return int(rate[:rate.find("G") - 3]) * 1000000000
        elif (" TB/s" in rate):
            return int(rate[:rate.find("T") - 3]) * 1000000000000

    # Creates the keyboard shortcuts
    def create_shortcuts(self, button, shortbutton):
        self.fm.add_command(label="Save ATP Profile", command=self.print_atp, accelerator=shortbutton + '+S')
        self.fm.add_command(label="Open ATP Profile", command=self.open_atp, accelerator=shortbutton + '+O')
        self.fm.add_command(label="Quit", command=quit, accelerator=shortbutton + '+Q')
        root.bind('<' + button + '-Q>', quit)
        root.bind('<' + button + '-q>', quit)
        root.bind('<' + button + '-S>', lambda x:self.print_atp())
        root.bind('<' + button + '-s>', lambda x:self.print_atp())
        root.bind('<' + button + '-O>', lambda x:self.open_atp())
        root.bind('<' + button + '-o>', lambda x:self.open_atp())


    # Creates all ATP options
    def create_processor_options(self):
        self.create_generic_options()
        self.create_cpu_options()
        self.create_dpu_options()
        self.create_gpu_options()

    # Selects which processor options to display on the GUI
    def choose_processor_options(self):
        self.atp_string = ""
        self.generic_copyright = "not added"
        if ( self.processor.get() == "CPU" ):
            self.button_generate_file.configure(state = "enable")
            self.button_preview_file.configure(state = "enable")
            self.button_generate_profile.configure(state = "disable")
            self.remove_processor_options()
            self.cpu_options.grid(sticky = N, row = 0, column = 2)
            self.cpu_pc_options.grid(sticky = N, row = 0, column = 3)
            self.cpu_mc_options.grid(sticky = N, row = 0, column = 4)
        elif ( self.processor.get() == "DPU" ):
            self.button_generate_file.configure(state = "enable")
            self.button_preview_file.configure(state = "enable")
            self.button_generate_profile.configure(state = "disable")
            self.remove_processor_options()
            self.dpu_options.grid(sticky = N, row = 0, column = 2)
            self.dpu_address_options.grid(sticky = N, row = 0, column = 3)
        elif ( self.processor.get() == "GPU" ):
            self.button_generate_file.configure(state = "enable")
            self.button_preview_file.configure(state = "enable")
            self.button_generate_profile.configure(state = "disable")
            self.remove_processor_options()
            self.gpu_options.grid(sticky = N, row = 0, column = 2)
            self.gpu_read_options.grid(sticky = N, row = 0, column = 3)
            self.gpu_write_options.grid(sticky = N, row = 0, column = 4)
        elif ( self.processor.get() == "Generic" ):
            self.button_generate_file.configure(state = "disable")
            self.button_preview_file.configure(state = "disable")
            self.button_generate_profile.configure(state = "enable")
            self.remove_processor_options()
            self.generic_options.grid(sticky = N, row = 0, column = 2)
            self.generic_address_options.grid(sticky = N, row = 0, column = 3)
            self.generic_size_options.grid(sticky = N, row = 0, column = 4)

    # Removes processor options from the GUI
    def remove_processor_options(self):
        self.generic_options.grid_forget()
        self.generic_address_options.grid_forget()
        self.generic_size_options.grid_forget()
        self.cpu_options.grid_forget()
        self.cpu_pc_options.grid_forget()
        self.cpu_mc_options.grid_forget()
        self.dpu_options.grid_forget()
        self.dpu_address_options.grid_forget()
        self.gpu_options.grid_forget()
        self.gpu_read_options.grid_forget()
        self.gpu_write_options.grid_forget()

    # Creates the different addressing options to be shown in the GUI
    def create_addressing_options(self, transaction_type, linear, uniform, normal, poisson, weibull, address_base, address_incr,
                                  address_min, address_max, address_std_dv, address_mean, address_shape, address_scale):

        linear.add(Label(linear, text = transaction_type + " Address Base: "))
        linear.add(ttk.Entry(linear, textvariable=address_base))
        linear.add(Label(linear, text = transaction_type + " Address Increment: "))
        linear.add(ttk.Entry(linear, textvariable=address_incr))

        uniform.add(Label(uniform, text = transaction_type + " Address Minimum: "))
        uniform.add(ttk.Entry(uniform, textvariable=address_min))
        uniform.add(Label(uniform, text = transaction_type + " Address Maximum: "))
        uniform.add(ttk.Entry(uniform, textvariable=address_max))

        normal.add(Label(normal, text = transaction_type + " Address Standard Deviation: "))
        normal.add(ttk.Entry(normal, textvariable=address_std_dv))
        normal.add(Label(normal, text = transaction_type + " Address Mean: "))
        normal.add(ttk.Entry(normal, textvariable=address_mean))

        poisson.add(Label(poisson, text = transaction_type + " Address Mean: "))
        poisson.add(ttk.Entry(poisson, textvariable=address_mean))

        weibull.add(Label(weibull, text = transaction_type + " Address Shape: "))
        weibull.add(ttk.Entry(weibull, textvariable=address_shape))
        weibull.add(Label(weibull, text = transaction_type + " Address Scale: "))
        weibull.add(ttk.Entry(weibull, textvariable=address_scale))

    # Generates the correct addressing options to be shown in the GUI
    def generate_address_options(self, main_options, addressing_type, placement, linear, uniform, normal, poisson, weibull):
        if (addressing_type == "LINEAR" ):
            main_options.forget(placement)
            main_options.add(linear)
        elif (addressing_type == "UNIFORM" ):
            main_options.forget(placement)
            main_options.add(uniform)
        elif (addressing_type == "NORMAL" ):
            main_options.forget(placement)
            main_options.add(normal)
        elif (addressing_type == "POISSON" ):
            main_options.forget(placement)
            main_options.add(poisson)
        elif (addressing_type == "WEIBULL" ):
            main_options.forget(placement)
            main_options.add(weibull)

    # Chooses the address options to be added to the dictionary when generating ATP files
    def choose_address_options(self, settings, addressing_type, transaction_type, address_base, address_incr,
             address_min, address_max, address_std_dv, address_mean, address_shape, address_scale):
        if ( addressing_type == "LINEAR" ):
            settings[transaction_type + "_base_address"] = address_base
            settings[transaction_type + "_incr"] = address_incr
        elif ( addressing_type == "UNIFORM" ):
            settings[transaction_type + "_address_random_type"] = "UNIFORM"
            settings[transaction_type + "_address_random_type_desc"] = "uniform_desc"
            settings[transaction_type + "_address_random_min"] = address_min
            settings[transaction_type + "_address_random_max"] = address_max
        elif ( addressing_type == "NORMAL" ):
            settings[transaction_type + "_address_random_type"] = "NORMAL"
            settings[transaction_type + "_address_random_type_desc"] = "normal_desc"
            settings[transaction_type + "_address_random_std_dev"] = address_std_dv
            settings[transaction_type + "_address_random_mean"] = address_mean
        elif ( addressing_type == "POISSON" ):
            settings[transaction_type + "_address_random_type"] = "POISSON"
            settings[transaction_type + "_address_random_type_desc"] = "poisson_desc"
            settings[transaction_type + "_address_random_mean"] = address_mean
        elif ( addressing_type == "WEIBULL" ):
            settings[transaction_type + "_address_random_type"] = "WEIBULL"
            settings[transaction_type + "_address_random_type_desc"] = "weibull_desc"
            settings[transaction_type + "_address_random_shape"] = address_shape
            settings[transaction_type + "_address_random_scale"] = address_scale

    # Reads the address options when reading ATP files
    def read_address_options(self, line, specific_type, transaction_type, addressing_type, address_base,
                             address_incr, address_min, address_max, address_std_dv,
                             address_mean, address_shape, address_scale):
        if (transaction_type == "WRITE"):
            placement = 1
        else:
            placement = 0
        if (not specific_type == ""):
            placement += len(specific_type)

        if (specific_type + transaction_type + " Address Type" in line):
            addressing_type.set(line[21 + placement:])
        elif (specific_type + transaction_type + " Address Base" in line):
            address_base.set(int(line[20 + placement:]))
        elif (specific_type + transaction_type + " Address Increment" in line):
            address_incr.set(int(line[25 + placement:]))
        elif (specific_type + transaction_type + " Address Min" in line):
            address_min.set(int(line[20 + placement:]))
        elif (specific_type + transaction_type + " Address Max" in line):
            address_max.set(int(line[20 + placement:]))
        elif (specific_type + transaction_type + " Address Standard Deviation" in line):
            address_std_dv.set(int(line[35 + placement:]))
        elif (specific_type + transaction_type + " Address Mean" in line):
            address_mean.set(int(line[21 + placement:]))
        elif (specific_type + transaction_type + " Address Shape" in line):
            address_shape.set(int(line[22 + placement:]))
        elif (specific_type + transaction_type + " Address Scale" in line):
            address_scale.set(int(line[22 + placement:]))

    # Creates the different data size options to be shown in the GUI
    def create_size_options(self, transaction_type, fixed, uniform, normal, poisson, weibull, size,
                                  size_min, size_max, size_std_dv, size_mean, size_shape, size_scale):

        fixed.add(Label(fixed, text = transaction_type + " Size: "))
        fixed.add(ttk.Entry(fixed, textvariable=size))

        uniform.add(Label(uniform, text = transaction_type + " Size Minimum: "))
        uniform.add(ttk.Entry(uniform, textvariable=size_min))
        uniform.add(Label(uniform, text = transaction_type + " Size Maximum: "))
        uniform.add(ttk.Entry(uniform, textvariable=size_max))

        normal.add(Label(normal, text = transaction_type + " Size Standard Deviation: "))
        normal.add(ttk.Entry(normal, textvariable=size_std_dv))
        normal.add(Label(normal, text = transaction_type + " Size Mean: "))
        normal.add(ttk.Entry(normal, textvariable=size_mean))

        poisson.add(Label(poisson, text = transaction_type + " Size Mean: "))
        poisson.add(ttk.Entry(poisson, textvariable=size_mean))

        weibull.add(Label(weibull, text = transaction_type + " Size Shape: "))
        weibull.add(ttk.Entry(weibull, textvariable=size_shape))
        weibull.add(Label(weibull, text = transaction_type + " Size Scale: "))
        weibull.add(ttk.Entry(weibull, textvariable=size_scale))

    # Generates the correct data size options to be shown in the GUI
    def generate_size_options(self, main_options, size_type, placement, fixed, uniform, normal, poisson, weibull):
        if (size_type == "FIXED" ):
            main_options.forget(placement)
            main_options.add(fixed)
        elif (size_type == "UNIFORM" ):
            main_options.forget(placement)
            main_options.add(uniform)
        elif (size_type == "NORMAL" ):
            main_options.forget(placement)
            main_options.add(normal)
        elif (size_type == "POISSON" ):
            main_options.forget(placement)
            main_options.add(poisson)
        elif (size_type == "WEIBULL" ):
            main_options.forget(placement)
            main_options.add(weibull)

    # Chooses the size options to be added to the dictionary when generating ATP files
    def choose_size_options(self, settings, size_type, size, size_min, size_max,
                             size_std_dv, size_mean, size_shape, size_scale):
        if ( size_type == "FIXED" ):
            settings["size"] = size
        elif ( size_type == "UNIFORM" ):
            settings["size_random_type"] = "UNIFORM"
            settings["size_random_type_desc"] = "uniform_desc"
            settings["size_random_min"] = size_min
            settings["size_random_max"] = size_max
        elif ( size_type == "NORMAL" ):
            settings["size_random_type"] = "NORMAL"
            settings["size_random_type_desc"] = "normal_desc"
            settings["size_random_std_dev"] = size_std_dv
            settings["size_random_mean"] = size_mean
        elif ( size_type == "POISSON" ):
            settings["size_random_type"] = "POISSON"
            settings["size_random_type_desc"] = "poisson_desc"
            settings["size_random_mean"] = size_mean
        elif ( size_type == "WEIBULL" ):
            settings["size_random_type"] = "WEIBULL"
            settings["size_random_type_desc"] = "weibull_desc"
            settings["size_random_shape"] = size_shape
            settings["size_random_scale"] = size_scale

    # Creates a notification to appear at the top level window
    def create_notification(self, title, text, height, width):
        self.top = Toplevel()
        self.top.title(title)
        self.top.protocol("WM_DELETE_WINDOW", self.dismiss_notification)
        self.preview = Text(self.top, height=height, width=width)
        self.preview.pack()
        self.preview.insert(END, text)
        self.button = Button(self.top, text="Dismiss", command=self.dismiss_notification)
        self.button.pack()

    # Destroys the top level window and resets each widget to it's 'normal' state
    def dismiss_notification(self):
        self.top.destroy()
        self.change_widget_state(self.default, "normal")

    # Appends the ATP file when creating generic ATP files
    def append_atp(self):
        if (self.generic_type.get() == "READ"):
            self.generic_cmd.set("READ_REQ")
        elif (self.generic_type.get() == "WRITE"):
            self.generic_cmd.set("WRITE_REQ")
        settings = {"author" : self.author.get(),
                    "type" : self.generic_type.get(),
                    "master_id" : self.generic_master_id.get(),
                    "start_fifo_level" : self.generic_start_fifo_level.get(),
                    "full_level" : self.generic_fifo_full_level.get(),
                    "ot_limit" : self.generic_ot_limit.get(),
                    "total_txn" : self.generic_total_txn.get(),
                    "rate" : self.generic_rate.get(),
                    "cmd" : self.generic_cmd.get(),
                    "name" : self.generic_name.get() }

        self.choose_address_options(settings, self.generic_addressing_type.get(), self.generic_type.get().lower(),
                                     self.generic_address_base.get(), self.generic_address_incr.get(),
                                     self.generic_address_min.get(), self.generic_address_max.get(),
                                     self.generic_address_std_dv.get(), self.generic_address_mean.get(),
                                     self.generic_address_shape.get(), self.generic_address_scale.get())

        self.choose_size_options(settings, self.generic_size_type.get(), self.generic_size.get(),
                                  self.generic_size_min.get(), self.generic_size_max.get(),
                                  self.generic_size_std_dv.get(), self.generic_size_mean.get(),
                                  self.generic_size_shape.get(), self.generic_size_scale.get())

        x = Generic_Gen(settings, self.generic_type.get().lower())

        if (self.generic_copyright == "not added"):
            self.atp_string = x.generate("not added")
            self.generic_copyright = "added"
        else:
            self.atp_string += x.generate("added")

        self.create_notification("ATP Profile added!", "ATP Profile added to buffer.\nUse the Generate ATP Profile button to create the ATP file.",
                                 3, 60)
        self.change_widget_state(self.default, "disable")

    # Allows for the ATP files to be previewed before printing to a file
    def preview_atp(self):
        self.generate_atp()
        self.create_notification("ATP Profile Preview", self.atp_string, 50, 100)
        self.change_widget_state(self.default, "disable")

    # Generates the ATP file and stores it within a string buffer
    def generate_atp(self):
        if (self.processor.get() == "CPU"):
            settings = {"author" : self.author.get(),
                        "pointer_chase" : self.cpu_point_chase_percentage.get() / float(100),
                        "data_limit" : self.cpu_data_limit.get(),
                        "size" : self.cpu_data_size.get() }
            if (self.cpu_point_chase_percentage.get() > 0):
                settings_pc = { "ot_limit" : self.cpu_pc_ot_limit.get() }

                self.choose_address_options(settings_pc, self.cpu_pc_read_addressing_type.get(), "read",
                                             self.cpu_pc_read_address_base.get(), self.cpu_pc_read_address_incr.get(),
                                             self.cpu_pc_read_address_min.get(), self.cpu_pc_read_address_max.get(),
                                             self.cpu_pc_read_address_std_dv.get(), self.cpu_pc_read_address_mean.get(),
                                             self.cpu_pc_read_address_shape.get(), self.cpu_pc_read_address_scale.get())

                self.choose_address_options(settings_pc, self.cpu_pc_write_addressing_type.get(), "write",
                                             self.cpu_pc_write_address_base.get(), self.cpu_pc_write_address_incr.get(),
                                             self.cpu_pc_write_address_min.get(), self.cpu_pc_write_address_max.get(),
                                             self.cpu_pc_write_address_std_dv.get, self.cpu_pc_write_address_mean.get(),
                                             self.cpu_pc_write_address_shape.get(), self.cpu_pc_write_address_scale.get())
            else:
                settings_pc = {}

            if (self.cpu_point_chase_percentage.get() < 100):

                settings_mc = {"ot_limit" : self.cpu_mc_ot_limit.get(),
                               "full_level" : self.cpu_mc_fifo_full_level.get(),
                               "bandwidth" : self.cpu_mc_bandwidth.get() }

                self.choose_address_options(settings_mc, self.cpu_mc_read_addressing_type.get(), "read",
                                            self.cpu_mc_read_address_base.get(), self.cpu_mc_read_address_incr.get(),
                                            self.cpu_mc_read_address_min.get(), self.cpu_mc_read_address_max.get(),
                                            self.cpu_mc_read_address_std_dv.get(), self.cpu_mc_read_address_mean.get(),
                                            self.cpu_mc_read_address_shape.get(), self.cpu_mc_read_address_scale.get())

                self.choose_address_options(settings_mc, self.cpu_mc_write_addressing_type.get(), "write",
                                            self.cpu_mc_write_address_base.get(), self.cpu_mc_write_address_incr.get(),
                                            self.cpu_mc_write_address_min.get(), self.cpu_mc_write_address_max.get(),
                                            self.cpu_mc_write_address_std_dv.get(), self.cpu_mc_write_address_mean.get(),
                                            self.cpu_mc_write_address_shape.get(), self.cpu_mc_write_address_scale.get())
            else:
                settings_mc = {}

            x = CPU_Gen(settings, settings_pc, settings_mc)
            self.atp_string = x.generate()
        elif (self.processor.get() == "DPU"):
            settings = {"author" : self.author.get(),
                        "x_pixels" : self.dpu_x.get(),
                        "y_pixels" : self.dpu_y.get(),
                        "FPS" : self.dpu_fps.get(),
                        "layers" : self.dpu_layers.get(),
                        "bits_per_pixel" : self.dpu_bpp.get(),
                        "timing" : self.dpu_timing.get(),
                        "total_txn" : self.dpu_total_txn.get(),
                        "size" : self.dpu_size.get(),
                        "base_adjust" : self.dpu_address_base_adjust.get() }

            self.choose_address_options(settings, self.dpu_addressing_type.get(), "read",
                                        self.dpu_address_base.get(), self.dpu_address_incr.get(),
                                        self.dpu_address_min.get(), self.dpu_address_max.get(),
                                        self.dpu_address_std_dv.get(), self.dpu_address_mean.get(),
                                        self.dpu_address_shape.get(), self.dpu_address_scale.get())

            x = DPU_Gen(settings)
            self.atp_string = x.generate()
        elif (self.processor.get() == "GPU"):
            settings = {"author" : self.author.get(),
                        "x_pixels" : self.gpu_x.get(),
                        "y_pixels" : self.gpu_y.get(),
                        "FPS" : self.gpu_fps.get(),
                        "ot_limit" : self.gpu_ot.get(),
                        "bytes_per_pixel" : self.gpu_bpp.get(),
                        "frames" : self.gpu_frames.get(),
                        "total_txn" : self.gpu_total_txn.get(),
                        "size" : self.gpu_size.get() }

            self.choose_address_options(settings, self.gpu_read_addressing_type.get(), "read",
                                        self.gpu_read_address_base.get(), self.gpu_read_address_incr.get(),
                                        self.gpu_read_address_min.get(), self.gpu_read_address_max.get(),
                                        self.gpu_read_address_std_dv.get(), self.gpu_read_address_mean.get(),
                                        self.gpu_read_address_shape.get(), self.gpu_read_address_scale.get())

            self.choose_address_options(settings, self.gpu_write_addressing_type.get(), "write",
                                        self.gpu_write_address_base.get(), self.gpu_write_address_incr.get(),
                                        self.gpu_write_address_min.get(), self.gpu_write_address_max.get(),
                                        self.gpu_write_address_std_dv.get(), self.gpu_write_address_mean.get(),
                                        self.gpu_write_address_shape.get(), self.gpu_write_address_scale.get())

            x = GPU_Gen(settings)
            self.atp_string = x.generate()
        elif (self.processor.get() == "Generic"):
            pass

    # Prints out the ATP string into a file
    def print_atp(self):
        self.generate_atp()
        filename = ""
        filename = tkFileDialog.asksaveasfilename(initialdir = ".",title = "Save ATP Profile",
                                                  filetypes = (("all files","*.*"), ("atp files","*.atp")))
        if (not filename == "") :
            new_file = open(filename, "w+")
            new_file.write(self.atp_string)
        self.atp_string = ""

    # Opens an ATP file that can be edited and saved
    def open_atp(self):
        file_path = ""
        file_path = tkFileDialog.askopenfilename(initialdir = ".",title = "Open ATP Profile",
                                                 filetypes = (("all files","*.*"), ("atp files","*.atp")))
        if ( file_path == "" ):
            return

        self.author.set(linecache.getline(file_path, 14)[10:].rstrip("\n"))
        processor_line = linecache.getline(file_path, 16)
        if ( "CPU" in processor_line):
            self.processor.set("CPU")
            self.cpu_data_limit.set(0)
        elif ( "DPU" in processor_line):
            self.processor.set("DPU")
        elif ( "GPU" in processor_line):
            self.processor.set("GPU")
        file_object  = open(file_path, "r")
        for line in file_object:
            line = line.rstrip("\n")
            if (not "#" in line):
                break
            if ( self.processor.get() == "CPU" ):
                if ("Pointer Chasing" in line):
                    self.cpu_point_chase_percentage.set(int(line[2:line.find("%")-2]))
                elif ("MEM COPY" in line):
                    self.cpu_point_chase_percentage.set(100 - int(line[2:line.find("%")-2]))
                elif ("CPU PC Data" in line):
                    data_size = int(line[line.find("*")+1:line.find("B")-1])
                    self.cpu_data_size.set(data_size)
                    self.cpu_data_limit.set(int(line[14:line.find("*")]) * data_size + self.cpu_data_limit.get())
                elif ("CPU MC Data" in line):
                    data_size = int(line[line.find("*")+1:line.find("B")-1])
                    self.cpu_data_size.set(data_size)
                    self.cpu_data_limit.set(int(line[14:line.find("*")]) * data_size + self.cpu_data_limit.get())
                elif ("PC OT" in line):
                    self.cpu_pc_ot_limit.set(int(line[8:]))
                elif ( "PC" in line and "READ Address" in line ):
                    self.read_address_options(line, "PC ", "READ", self.cpu_pc_read_addressing_type,
                                              self.cpu_pc_read_address_base, self.cpu_pc_read_address_incr,
                                              self.cpu_pc_read_address_min, self.cpu_pc_read_address_max,
                                              self.cpu_pc_read_address_std_dv, self.cpu_pc_read_address_mean,
                                              self.cpu_pc_read_address_shape, self.cpu_pc_read_address_scale)
                elif ( "PC" in line and "WRITE Address" in line ):
                    self.read_address_options(line, "PC ", "WRITE", self.cpu_pc_write_addressing_type,
                                              self.cpu_pc_write_address_base, self.cpu_pc_write_address_incr,
                                              self.cpu_pc_write_address_min, self.cpu_pc_write_address_max,
                                              self.cpu_pc_write_address_std_dv, self.cpu_pc_write_address_mean,
                                              self.cpu_pc_write_address_shape, self.cpu_pc_write_address_scale)
                elif ("MC OT" in line):
                    self.cpu_mc_ot_limit.set(int(line[8:]))
                elif ("MC FIFO" in line):
                    self.cpu_mc_fifo_full_level.set(int(line[11:]))
                elif ("MC Total bandwidth" in line):
                    self.cpu_mc_bandwidth.set(self.convert_rate_to_KBS(line[21:]))
                elif ( "MC" in line and "READ Address" in line ):
                    self.read_address_options(line, "MC ", "READ", self.cpu_mc_read_addressing_type,
                                              self.cpu_mc_read_address_base, self.cpu_mc_read_address_incr,
                                              self.cpu_mc_read_address_min, self.cpu_mc_read_address_max,
                                              self.cpu_mc_read_address_std_dv, self.cpu_mc_read_address_mean,
                                              self.cpu_mc_read_address_shape, self.cpu_mc_read_address_scale)
                elif ( "MC" in line and "WRITE Address" in line ):
                    self.read_address_options(line, "MC ", "WRITE", self.cpu_mc_write_addressing_type,
                                              self.cpu_mc_write_address_base, self.cpu_mc_write_address_incr,
                                              self.cpu_mc_write_address_min, self.cpu_mc_write_address_max,
                                              self.cpu_mc_write_address_std_dv, self.cpu_mc_write_address_mean,
                                              self.cpu_mc_write_address_shape, self.cpu_mc_write_address_scale)
            elif (self.processor.get() == "DPU"):
                if ("X pixels:" in line):
                    self.dpu_x.set(int(line[11:]))
                elif ("Y pixels:" in line):
                    self.dpu_y.set(int(line[11:]))
                elif ("FPS:" in line):
                    self.dpu_fps.set(int(line[6:]))
                elif ("Bits per Pixel:" in line):
                    self.dpu_bpp.set(int(line[17:]))
                elif ("Layers:" in line):
                    self.dpu_layers.set(int(line[9:]))
                elif ("Data Size:" in line):
                    self.dpu_size.set(int(line[13:]))
                elif ("Total Transactions:" in line):
                    self.dpu_total_txn.set(int(line[22:]))
                elif ("Timings:" in line):
                    self.dpu_timing.set(float(line[10:]))
                elif ( "READ Address" in line ):
                    self.read_address_options(line, "", "READ", self.dpu_addressing_type,
                                              self.dpu_address_base, self.dpu_address_incr,
                                              self.dpu_address_min, self.dpu_address_max,
                                              self.dpu_address_std_dv, self.dpu_address_mean,
                                              self.dpu_address_shape, self.dpu_address_scale)
            elif (self.processor.get() == "GPU"):
                if ("X pixels:" in line):
                    self.gpu_x.set(int(line[11:]))
                elif ("Y pixels:" in line):
                    self.gpu_y.set(int(line[11:]))
                elif ("FPS:" in line):
                    self.gpu_fps.set(int(line[6:]))
                elif ("Bytes per Pixel:" in line):
                    self.gpu_bpp.set(int(line[19:]))
                elif ("Frames:" in line):
                    self.gpu_frames.set(int(line[9:]))
                elif ("Data Size:" in line):
                    self.gpu_size.set(int(line[13:]))
                elif ("Total Transactions:" in line):
                    self.gpu_total_txn.set(int(line[22:]))
                elif ("OT:" in line):
                    self.gpu_ot.set(int(line[5:]))
                elif ( "READ Address" in line ):
                    self.read_address_options(line, "", "READ", self.gpu_read_addressing_type,
                                              self.gpu_read_address_base, self.gpu_read_address_incr,
                                              self.gpu_read_address_min, self.gpu_read_address_max,
                                              self.gpu_read_address_std_dv, self.gpu_read_address_mean,
                                              self.gpu_read_address_shape, self.gpu_read_address_scale)
                elif ( "WRITE Address" in line ):
                    self.read_address_options(line, "", "WRITE", self.gpu_write_addressing_type,
                                              self.gpu_write_address_base, self.gpu_write_address_incr,
                                              self.gpu_write_address_min, self.gpu_write_address_max,
                                              self.gpu_write_address_std_dv, self.gpu_write_address_mean,
                                              self.gpu_write_address_shape, self.gpu_write_address_scale)

        if ( self.processor.get() == "CPU" ):
            self.generate_cpu_specific_options(self.cpu_point_chase_percentage.get())
            self.generate_address_options(self.cpu_pc_read_options,
                                          self.cpu_pc_read_addressing_type.get(), 2,
                                          self.cpu_pc_read_linear_options,
                                          self.cpu_pc_read_uniform_options,
                                          self.cpu_pc_read_normal_options,
                                          self.cpu_pc_read_poisson_options,
                                          self.cpu_pc_read_weibull_options)
            self.generate_address_options(self.cpu_pc_write_options,
                                          self.cpu_pc_write_addressing_type.get(), 2,
                                          self.cpu_pc_write_linear_options,
                                          self.cpu_pc_write_uniform_options,
                                          self.cpu_pc_write_normal_options,
                                          self.cpu_pc_write_poisson_options,
                                          self.cpu_pc_write_weibull_options)
            self.generate_address_options(self.cpu_mc_read_options,
                                          self.cpu_mc_read_addressing_type.get(), 2,
                                          self.cpu_mc_read_linear_options,
                                          self.cpu_mc_read_uniform_options,
                                          self.cpu_mc_read_normal_options,
                                          self.cpu_mc_read_poisson_options,
                                          self.cpu_mc_read_weibull_options)
            self.generate_address_options(self.cpu_mc_write_options,
                                          self.cpu_mc_write_addressing_type.get(), 2,
                                          self.cpu_mc_write_linear_options,
                                          self.cpu_mc_write_uniform_options,
                                          self.cpu_mc_write_normal_options,
                                          self.cpu_mc_write_poisson_options,
                                          self.cpu_mc_write_weibull_options)
        elif ( self.processor.get() == "DPU" ):
            self.generate_address_options(self.dpu_address_options,
                                          self.dpu_addressing_type.get(), 3,
                                          self.dpu_linear_options,
                                          self.dpu_uniform_options,
                                          self.dpu_normal_options,
                                          self.dpu_poisson_options,
                                          self.dpu_weibull_options)
        elif ( self.processor.get() == "GPU" ):
            self.generate_address_options(self.gpu_read_options,
                                          self.gpu_read_addressing_type.get(), 3,
                                          self.gpu_read_linear_options,
                                          self.gpu_read_uniform_options,
                                          self.gpu_read_normal_options,
                                          self.gpu_read_poisson_options,
                                          self.gpu_read_weibull_options)
            self.generate_address_options(self.gpu_write_options,
                                          self.gpu_write_addressing_type.get(), 3,
                                          self.gpu_write_linear_options,
                                          self.gpu_write_uniform_options,
                                          self.gpu_write_normal_options,
                                          self.gpu_write_poisson_options,
                                          self.gpu_write_weibull_options)
        self.choose_processor_options()


if __name__ == '__main__':
    root = Tk()
    app = App(root)
    root.mainloop()
