# -*- mode:python -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2012 ARM Limited
# All rights reserved.
# Authors: Matteo Andreozzi

Import('*')

# Only build the traffic generator if we have support for protobuf as the
# tracing relies on it
if env['CONF']['HAVE_PROTOBUF']:
    Source('traffic_profile_manager.cc')
    Source('traffic_profile_desc.cc')
    Source('traffic_profile_master.cc')
    Source('traffic_profile_checker.cc')
    Source('traffic_profile_slave.cc')
    Source('traffic_profile_delay.cc')
    Source('random_generator.cc')
    Source('packet_desc.cc')
    Source('packet_tagger.cc')
    Source('packet_tracer.cc')
    Source('event.cc')
    Source('event_manager.cc')
    Source('logger.cc')
    Source('fifo.cc')
    Source('stats.cc')
    Source('kronos.cc')
    Source('utilities.cc')
