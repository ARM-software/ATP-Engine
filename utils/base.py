#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2017 ARM Limited
# All rights reserved
# Authors: Riken Gohil

import datetime
from enum import Enum
from fractions import Fraction

# Base class for each different kind of specific ATP file configurationz
class Base_Gen():

    # Returns the basic profile layout that can be used with a dictionary
    def get_base_profile(self, wait_for, address_random_type, size_random_type):
        base_profile = ('\nprofile {{' +
                        '\n  type: {type}' +
                        '\n  master_id: "{master_id}"' +
                        '\n  fifo {{' +
                        '\n    start_fifo_level: {start_fifo_level}' +
                        '\n    full_level: {full_level}' +
                        '\n    ot_limit: {ot_limit}' +
                        '\n    total_txn: {total_txn}' +
                        '\n    rate: "{rate}"' +
                        '\n  }}' +
                        '\n  pattern {{' +
                        '\n    cmd: {cmd}' )

        if (address_random_type is random_type.linear):
            base_profile += ('\n    address {{' +
                             '\n      base: {base}' +
                             '\n      increment: {increment}' +
                             '\n    }}' )
        else:
            base_profile += ('\n    random_address {{' +
                             '\n      type: {address_random_type}' +
                             '\n      {address_random_type_desc} {{' )
            if (address_random_type is random_type.uniform):
                base_profile += ('\n        min: {address_random_min}' +
                                 '\n        max: {address_random_max}')
            elif (address_random_type is random_type.normal):
                base_profile += ('\n        mean: {address_random_mean}' +
                                 '\n        std_dev: {address_random_std_dev}')
            elif (address_random_type is random_type.poisson):
                base_profile += ('\n        mean: {address_random_mean}' )
            elif (address_random_type is random_type.weibull):
                base_profile += ('\n        shape: {address_random_shape}' +
                                 '\n        scale: {address_random_scale}')

        if (size_random_type is random_type.fixed):
            base_profile += ('\n    size: {size}')
        else:
            base_profile += ('\n    random_size {{' +
                             '\n      type: {size_random_type}' +
                             '\n      {size_random_type_desc} {{' )
            if (size_random_type is random_type.uniform):
                base_profile += ('\n        min: {size_random_min}' +
                                 '\n        max: {size_random_max}')
            elif (size_random_type is random_type.normal):
                base_profile += ('\n        mean: {size_random_mean}' +
                                 '\n        std_dev: {size_random_std_dev}')
            elif (size_random_type is random_type.poisson):
                base_profile += ('\n        mean: {size_random_mean}' )
            elif (size_random_type is random_type.weibull):
                base_profile += ('\n        shape: {size_random_shape}' +
                                 '\n        scale: {size_random_scale}')
        base_profile += ('\n  }}' +
                         '\n  name: "{name}"')
        for i in xrange(wait_for):
            base_profile += '\n  wait_for: "{wait_for_' + str(i) + '}"'
        base_profile += '\n}}\n'
        return base_profile

    # Returns the copyright message to go above each generated ATP file
    def get_copyright(self):
        copyright = ( "#" +
                    "\n# The confidential and proprietary information contained in this file may" +
                    "\n# only be used by a person authorised under and to the extent permitted" +
                    "\n# by a subsisting licensing agreement from ARM Limited." +
                    "\n#" +
                    "\n#            (C) COPYRIGHT " + str(datetime.datetime.now().year) + " ARM Limited." +
                    "\n#                    ALL RIGHTS RESERVED" +
                    "\n#" +
                    "\n# This entire notice must be reproduced on all copies of this file" +
                    "\n# and copies of this file may only be made by a person if such person is" +
                    "\n# permitted to do so under the terms of a subsisting license agreement" +
                    "\n# from ARM Limited." +
                    "\n#" +
                    "\n# Author: {author}" +
                    "\n#" )
        return copyright.format(**self.settings)

    # Gets the address info for the comments of the ATP file
    def get_address_info(self, defaults, transaction_type, type):
        lower_transaction_type = transaction_type.lower()
        info = ""
        if ( lower_transaction_type + "_address_random_type" in defaults.keys() ):
            info += ('\n# ' + type + transaction_type + ' Address Type: {' + lower_transaction_type + '_address_random_type}')
            if (defaults[lower_transaction_type + "_address_random_type"] == "UNIFORM"):
                info += ('\n# ' + type + transaction_type + ' Address Min: {' + lower_transaction_type + '_address_random_min}' +
                         '\n# ' + type + transaction_type + ' Address Max: {' + lower_transaction_type + '_address_random_max}')
            elif (defaults[lower_transaction_type + "_address_random_type"] == "NORMAL"):
                info += ('\n# ' + type + transaction_type + ' Address Standard Deviation: {' + lower_transaction_type + '_address_random_std_dev}' +
                         '\n# ' + type + transaction_type + ' Address Mean: {' + lower_transaction_type + '_address_random_mean}')
            elif (defaults[lower_transaction_type + "_address_random_type"] == "POISSON"):
                info += ('\n# ' + type + transaction_type + ' Address Mean: {' + lower_transaction_type + '_address_random_mean}')
            elif (defaults[lower_transaction_type + "_address_random_type"] == "WEIBULL"):
                info += ('\n# ' + type + transaction_type + ' Address Shape: {' + lower_transaction_type + '_address_random_shape}' +
                         '\n# ' + type + transaction_type + ' Address Scale: {' + lower_transaction_type + '_address_random_scale}')
        else:
            info += ('\n# ' + type + transaction_type + ' Address Type: LINEAR' +
                     '\n# ' + type + transaction_type + ' Address Base: {' + lower_transaction_type + '_base_address}' +
                     '\n# ' + type + transaction_type + ' Address Increment: {' + lower_transaction_type + '_incr}' )
        return info

    # Retrieves if there is a random address/size configuration
    def get_random(self, transaction_type, type, type_defaults, defaults):
        if ( transaction_type + type + "_random_type" in defaults.keys() ):
            type_defaults[type + "_random_type"] = defaults[transaction_type + type + "_random_type"]
            type_defaults[type + "_random_type_desc"] = defaults[transaction_type + type + "_random_type_desc"]
            if ( defaults[transaction_type + type + "_random_type"] == "UNIFORM" ):
                type_defaults[type + "_random_min"] = defaults[transaction_type + type + "_random_min"]
                type_defaults[type + "_random_max"] = defaults[transaction_type + type + "_random_max"]
                return random_type.uniform
            elif ( defaults[transaction_type + type + "_random_type"] == "NORMAL" ):
                type_defaults[type + "_random_std_dev"] = defaults[transaction_type + type + "_random_std_dev"]
                type_defaults[type + "_random_mean"] = defaults[transaction_type + type + "_random_mean"]
                return random_type.normal
            elif ( defaults[transaction_type + type + "_random_type"] == "POISSON" ):
                type_defaults[type + "_random_mean"] = defaults[transaction_type + type + "_random_mean"]
                return random_type.poisson
            elif ( defaults[transaction_type + type + "_random_type"] == "WEIBULL" ):
                type_defaults[type + "_random_shape"] = defaults[transaction_type + type + "_random_shape"]
                type_defaults[type + "_random_scale"] = defaults[transaction_type + type + "_random_scale"]
                return random_type.weibull
        else:
            if ( type == "address" ):
                type_defaults["increment"] = defaults[transaction_type + "incr"]
                if ( transaction_type + "base_address" in defaults.keys() ):
                    type_defaults["base"] = defaults[transaction_type + "base_address"]
                return random_type.linear
            elif ( type == "size" ):
                type_defaults["size"] = defaults["size"]
                return random_type.fixed

    # Returns the next greatest power of 2
    def next_greatest_power_of_2(self, x):
        return 2**(x-1).bit_length()

    # Converts B/s
    def convert_rate(selfs, x):
        x = float(x)
        if ( x >= 1000000000000 ):
            x = str(x / 1000000000000) + " TB/s"
        elif  ( x >= 1000000000 ):
            x = str(x / 1000000000) + " GB/s"
        elif ( x >= 1000000 ):
            x = str(x / 1000000) + " MB/s"
        elif ( x >= 1000 ):
            x = str(x / 1000) + " KB/s"
        else:
            x = str(x) + " B/s"
        return x

    # Checks to see if that with the full level that has been given by the
    # user would result in any underruns or overruns. 
    def validate_full_level(self, rate, size, current_full_level):
        # Converts the rate from B/s to B/ps
        next_full_level = Fraction(str(rate) + "/1000000000000").numerator
        # Get's the next full level that is just greater than the size of packets.
        while ( next_full_level < size ):
            next_full_level += next_full_level
        return max(current_full_level, next_full_level)

    def __init__(self, settings):
        self.settings = settings

class random_type(Enum):
    fixed = 0
    linear = 1
    uniform = 2
    normal = 3
    poisson = 4
    weibull = 5