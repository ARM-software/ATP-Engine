# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2019-2020 ARM Limited
# All rights reserved.
# Authors: Adrian Herrera

from m5.objects.RealView import AmbaDmaDevice
from m5.params import Param
from m5.proxy import Parent
from m5.util.fdthelper import FdtPropertyStrings

class ATPDevice(AmbaDmaDevice):
    """AMBA ATP generic device"""

    type = 'ATPDevice'
    cxx_header = "gem5/atp_device.hh"
    cxx_class = 'ATP::Device'

    _node_name = "atpdevice"
    _pio_size = 0x1000

    adapter = Param.ProfileGen(Parent.any, "Reference to the ATP Adapter")
    atp_id = Param.String("ATP device ID, same as in .atp files")
    amba_id = 0x0

    def generateDeviceTree(self, state):
        node = self.generateBasicPioDeviceNode(state, self._node_name,
                                               self.pio_addr, self._pio_size,
                                               [ self.interrupt ])
        node.appendCompatible("arm,{}".format(self._node_name))
        self.addIommuProperty(state, node)
        node.append(FdtPropertyStrings("atp-id", [ self.atp_id ]))

        yield node
