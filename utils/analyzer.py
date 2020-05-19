#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2013-2014, 2017 ARM Limited
# All rights reserved
# Authors: Matteo Andreozzi
#          Riken Gohil
#
# This script is used to parse m3i ASCII traces containing AXI transactions
# and profile them by using scipy distribution fitting capabilities
from numpy import array, median, arange, multiply
from scipy.stats import norm, rayleigh, pareto, expon
from os.path import basename
import re


class Analyzer(object):
    """
    M3I file analyzer class

    """
    def init_stats(self):
        self.num_transactions = 0
        self.num_uncachables = 0
        self.cumulative_cycles = 0
        self.total_data = 0
        self.master_name = ''

    def __init__(self, fig, bus=4):
        # Width of the data bus in bytes (typical configuration of a CCI port for
        # CPU/GPU master)
        self.type = "READ"
        self.figure = fig
        self.bus_width = bus
        self.max_samples = 10000
        # bandwidth sampling window
        self.bw_window = 10
        # histograms switch
        self.show_histograms = False
        # bandwidth only mode switch
        self.show_bw_only = False
        # data only mode switch
        self.show_data_only = False
        # stats fitting switch
        self.stats_fitting = False
        # burst mode switch
        self.show_burst_mode = False
        self.times = {}
        self.cycles = {}
        self.addresses = {}
        self.sizes = {}
        self.init_stats()
        self.sizeDict = dict(byte=1, size8=1, size16=2,
                             size32=4, size64=8, size128=16,
                             size256=32, size512=64, size1024=128,
                             half=1, word=2, dword=4)

    @staticmethod
    def fit(ax, func, bins, data, color):
        """
        TODO
        :param ax:
        :param func:
        :param bins:
        :param data:
        :param color:
        """
        params = func.fit(data)
        y = func.pdf(bins, *params)
        ax.plot(bins, y, color + '--', linewidth=2,
                label='{0} fit '.format(func.name) + ' '.join([str(p) for p in params]))

    def histogram(self, functions, data, title, s1, s2, s3):
        """
        TODO
        :param s3:
        :param s2:
        :param s1:
        :param functions:
        :param data:
        :param title:
        """
        ax = self.figure.add_subplot(s1, s2, s3)
        # the histogram of the data
        n, bins, patches = ax.hist(data, normed=True, alpha=0.5, facecolor='g', label='')

        for c, f in functions.iteritems():
            self.fit(ax, f, bins, data, c)

        # plot
        ax.set_title(title)
        ax.legend(loc='best', frameon=False)

    def yint(self, plt):
        # make the y ticks integers, not floats
        yi = []
        locs = plt.get_yticks()
        for each in locs:
            yi.append(int(each))
        plt.set_yticks(yi)
        plt.set_yticklabels(yi)

    def sequence(self, prev_ax, x, y, title, s1, s2, s3, file_type):
        """
        TODO
        :param s2:
        :param s3:
        :param s1:
        :param prev_ax:
        :param x:
        :param y:
        :param title:
        """
        ax = self.figure.add_subplot(s1, s2, s3, sharex=prev_ax)
        ax.scatter(x, y, picker=True, label='')
        ax.legend(loc='best', frameon=False)
        ax.autoscale_view(True, True, True)
        ax.autoscale(enable=True, axis='x', tight=True)
        if ( file_type == 'm3i' ):
            ax.set_xlabel('Cycles')
        elif ( file_type == 'trace' ):
            ax.set_xlabel('Seconds')
        ax.set_title(title)
        if "Sizes" in title:
            ax.set_ylabel('Bytes')
        elif "Addresses" in title:
            ax.set_ylabel('Addresses')
        self.yint(ax)
        return ax

    def median_sample(self, arr):
        """
        :param arr:
        :return:
        """
        x = array(arr)
        # compute how many elements we need to compute the median over
        n = (len(arr) / self.max_samples)
        # elements must be odd in order to get an integer median
        n = n + 1 if not n % 2 else n
        # last element of the array to take into account
        end = n * int(len(x) / n)
        # compute the median over every n-dimensional portion of the original array
        return list(median(x[:end].reshape(-1, n), 1))

    def sample(self, arr):
        """
        :param arr:
        :return:
        """
        x = array(arr)
        # compute how many elements we need to extract
        n = (len(arr) / self.max_samples)
        l = list(x.take(arange(0, len(arr), n)))
        return l

    def bandwidth(self, prev_ax, x, y, title, s1, s2, s3, file_type):
        """
        :param s3:
        :param s2:
        :param s1:
        :param prev_ax:
        :param x:
        :param y:
        :param title:
        """
        ax = self.figure.add_subplot(s1, s2, s3, sharex=prev_ax)
        mean_y = []
        prev_i = 0

        # limit bw_window to number of samples
        self.bw_window = min(self.bw_window, len(x)-1)

        for i in xrange(self.bw_window, len(x), self.bw_window):
            for j in xrange(self.bw_window):
                mean_y.append(sum(y[prev_i:i]) / ((x[i] - x[prev_i]) or 1))
            prev_i = i

        # extend mean_y to match x axis length
        for k in xrange(len(mean_y), len(x)):
            mean_y.append(mean_y[-1])

        ax.plot(x, mean_y, label='')
        # ax.set_title(r'{0}'.format(title))
        ax.legend(loc='best', frameon=False)
        if ( file_type == 'm3i' ):
            ax.set_xlabel('Cycles')
        elif ( file_type == 'trace' ):
            ax.set_xlabel('Seconds')
        ax.set_title(title)
        ax.set_ylabel('Bytes')
        ax.set_title(title)
        ax.autoscale_view(True, True, True)
        ax.autoscale(enable=True, axis='x', tight=True)
        return ax

    def parse_m3i(self, file_name):
        """
        Parses an m3i file and extracts data
        :param file_name: the m3i file name
        """

        global ascii_in
        try:
            ascii_in = open(file_name, 'r')
        except IOError:
            print "Failed to open ", file_name, " for reading"
            exit(-1)

        # reset all statistics
        self.init_stats()
        # init samples
        for t in ('READ','WRITE'):
            self.addresses[t] = []
            self.sizes[t] = []
            self.cycles[t] = []

        # For each line in the m3i trace, parse addresses and sizes
        for line in ascii_in:
            fields = line.split()
            transaction_type = 'READ' if fields[0] == 'AR' else 'WRITE'

            # Skips the line if it's not AR or AW
            if (not (fields[0] == 'AR' or fields[0] == 'AW')):
                continue

            # Convert hex address in m3i to dec
            address = long(fields[1], 16)

            # Sets defaults values for options
            length = 1
            data_size = self.bus_width
            burst_type = 'incr'
            transaction_timing = 1

            for field in fields[2:]:

                # Searches for size field, which is the amount of data transfered
                # in a single transfer
                if field in self.sizeDict:
                    data_size = self.sizeDict[field]
                # Search for field with prefix L which is the no. of transfers
                # on the read data or write data channel
                elif 'L' in field:
                    # Packet size in bytes for L transfers
                    length = long(field.strip('L'))
                # Search for field with prefix V that holds Valid timing wrt last
                # transaction (a delta in cycles). Make it absolute (in cycles).
                elif 'V' in field:
                    transaction_timing = long(field.strip('V'))
                    self.cumulative_cycles += transaction_timing
                # Search for field with prefix C to account for uncachables
                elif 'C' in field:
                    cache_attr = field.strip('C')
                    if not cache_attr[-1] == '1':
                        self.num_uncachables += 1
                # Searches the field for the burst mode
                elif 'fixed' in field:
                    burst_type = field
                elif 'incr' in field:
                    burst_type = field
                elif 'wrap' in field:
                    burst_type= field

            # Only plots the burst data points if the option is selected
            if not self.show_burst_mode:
                self.sizes[transaction_type].append(data_size * length)
                self.addresses[transaction_type].append(address)
                self.cycles[transaction_type].append(self.cumulative_cycles)
            else:
                if 'fixed' in burst_type:
                    for i in xrange(length):
                        self.addresses[transaction_type].append(address)
                        self.sizes[transaction_type].append(data_size)
                        self.cycles[transaction_type].append(self.cumulative_cycles)
                elif 'incr' in burst_type:
                    for i in xrange(length):
                        self.addresses[transaction_type].append(address)
                        self.sizes[transaction_type].append(data_size)
                        self.cycles[transaction_type].append(self.cumulative_cycles)
                        address += (data_size * 8)
                # Needs to be changed to wrapping around addresses
                # once a limit has been reached (limit not currently detected)
                elif 'wrap' in burst_type:
                    for i in xrange(length):
                        self.addresses[transaction_type].append(address)
                        self.sizes[transaction_type].append(data_size)
                        self.cycles[transaction_type].append(self.cumulative_cycles)
                        address += (data_size * 8)

            self.num_transactions += 1
            self.total_data += data_size * length

        ascii_in.close()

        if len(self.sizes['READ']) and len(self.sizes['WRITE']):
            return 'READ/WRITE'
        elif len(self.sizes['READ']):
            return 'READ'
        else:
            return 'WRITE'

    def parse_trace(self, file_name):
        """
        Parses a trace file and extracts data
        :param file_name: the trace file name
        """

        global ascii_in
        try:
            ascii_in = open(file_name, 'r')
        except IOError:
            print "Failed to open ", file_name, " for reading"
            exit(-1)

        # reset all statistics
        self.init_stats()

        match = re.search("(.*)\.(.+)\.trace", basename(file_name))
        self.master_name = match.group(1)
        transaction_type = match.group(2)

        # init samples
        self.times[transaction_type] = []
        self.addresses[transaction_type] = []
        self.sizes[transaction_type] = []

        # For each line in the m3i trace, parse addresses and sizes
        for line in ascii_in:
            fields = line.split()
            # store transaction time
            self.times[transaction_type].append(float(fields[0]))
            # Convert hex address in trace to dec
            self.addresses[transaction_type].append(long(fields[1], 16))
            # store transaction size
            size = long(fields[2])
            self.sizes[transaction_type].append(size)
            self.total_data += size

        ascii_in.close()

        return transaction_type

    def draw_trace(self):

        ax = None
        sub_index = 3 if not (self.show_bw_only or self.show_data_only) else 1
        r_index = 1
        t_index = 0

        if len(self.times[self.type]) > self.max_samples:
            addresses = self.sample(self.addresses[self.type])
            times = self.sample(self.times[self.type])
            sizes = self.sample(self.sizes[self.type])
            # bandwidth is sub-sampled when using 'sample', adjust magnitude
            bw_sizes = multiply(sizes, len(self.times[self.type]) / self.max_samples)
        else:
            addresses = self.addresses[self.type]
            times = self.times[self.type]
            sizes = self.sizes[self.type]
            bw_sizes = sizes

        ok_addresses = True if len(addresses) else False
        ok_sizes = True if len(sizes) else False
        ok_times = True if len(times) else False
        functions = {}

        if ok_addresses and self.show_histograms and not (self.show_bw_only or self.show_data_only):
            if self.stats_fitting:
                functions = {'r': norm, 'b': rayleigh, 'y': pareto, 'g': expon}
            self.histogram(functions, addresses, self.master_name + '.' + self.type + ' Addresses',
                           sub_index, r_index, 1 + t_index)

        if ok_sizes and self.show_histograms and not (self.show_bw_only or self.show_data_only):
            if self.stats_fitting:
                functions = {'r': norm, 'b': rayleigh, 'y': pareto}
            self.histogram(functions, sizes, self.master_name + '.' + self.type + ' Sizes',
                           sub_index, r_index, 2 + t_index)

        if ok_times and self.show_histograms and not (self.show_bw_only or self.show_data_only):
            if self.stats_fitting:
                functions = {'r': norm, 'b': rayleigh}
            self.histogram(functions, times, self.master_name + '.' + self.type + ' Times',
                           sub_index, r_index, 3 + t_index)

        if ok_times and ok_addresses and not self.show_histograms and not (self.show_bw_only or self.show_data_only):
            ax = self.sequence(ax, times, addresses,
                               self.master_name + '.' + self.type + ' Addresses', sub_index, r_index, 1 + t_index, 'trace')

        if ok_times and ok_sizes and not self.show_histograms:
            if not self.show_bw_only:
                sq_t_index = 2 + t_index if not self.show_data_only else 1 + t_index
                ax = self.sequence(ax, times, sizes,
                                   self.master_name + '.' + self.type + ' Sizes', sub_index, r_index, sq_t_index, 'trace')
            if not self.show_data_only:
                bw_t_index = 3 + t_index if not self.show_bw_only else 1 + t_index
                ax = self.bandwidth(ax, times, bw_sizes,
                                self.master_name + '.' + self.type + ' Bandwidth', sub_index, r_index, bw_t_index, 'trace')
        t_index += 3
        if ax is not None:
            ax.set_xlim(0, times[-1])
        self.figure.tight_layout()

    def draw_m3i(self):
        """
        TODO
        """
        ax = None
        types = self.type.split('/')
        sub_index = 3
        if len(types) == 2:
            r_index = 2
        else:
            r_index = 1

        t_index = 1;

        for transaction_type in types:
            if len(self.cycles[transaction_type]) > self.max_samples:
                addresses = self.median_sample(self.addresses[transaction_type])
                cycles = self.median_sample(self.cycles[transaction_type])
                sizes = self.median_sample(self.sizes[transaction_type])
            else:
                addresses = self.addresses[transaction_type]
                cycles = self.cycles[transaction_type]
                sizes = self.sizes[transaction_type]

            ok_addresses = True if len(addresses) else False
            ok_sizes = True if len(sizes) else False
            ok_cycles = True if len(cycles) else False
            functions = {}

            if ok_addresses and self.show_histograms:
                if self.stats_fitting:
                    functions = {'r': norm, 'b': rayleigh, 'y': pareto, 'g': expon}
                self.histogram(functions, addresses, transaction_type + ' Addresses',
                               sub_index, r_index, t_index)

            if ok_sizes and self.show_histograms:
                if self.stats_fitting:
                    functions = {'r': norm, 'b': rayleigh, 'y': pareto}
                t_index += r_index;
                self.histogram(functions, sizes, transaction_type + ' Sizes', sub_index, r_index, t_index)

            if ok_cycles and self.show_histograms:
                if self.stats_fitting:
                    functions = {'r': norm, 'b': rayleigh}
                t_index += r_index;
                self.histogram(functions, cycles, transaction_type + ' Cycles', sub_index, r_index, t_index)

            if ok_cycles and ok_addresses and not self.show_histograms:
                ax = self.sequence(ax, cycles, addresses,
                                   transaction_type + ' Addresses', sub_index, r_index, t_index, 'm3i')

            if ok_cycles and ok_sizes and not self.show_histograms:
                t_index += r_index;
                ax = self.sequence(ax, cycles, sizes,
                                   transaction_type + ' Sizes', sub_index, r_index, t_index, 'm3i')

                t_index += r_index;
                ax = self.bandwidth(ax, cycles, sizes,
                                    transaction_type + ' Bandwidth', sub_index, r_index, t_index, 'm3i')
            t_index = 2;

            self.figure.tight_layout()
