#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2013-2014, 2017 ARM Limited
# All rights reserved
# Authors: Matteo Andreozzi
#          Riken Gohil
#
# The recommended way to use wx with mpl is with the WXAgg
# backend.
#
import matplotlib
import warnings
import wx

matplotlib.use('WXAgg')

from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import \
    FigureCanvasWxAgg as FigCanvas, \
    NavigationToolbar2WxAgg as NavigationToolbar
from analyzer import Analyzer


class BarsFrame(wx.Frame):
    """
    The main frame of the application
    """
    title = 'Profiler'

    def __init__(self):
        wx.Frame.__init__(self, None, -1, self.title)

        self.mode = ''
        self.create_menu()
        self.create_status_bar()
        self.create_main_panel()
        self.Center()

    def create_menu(self):
        self.menubar = wx.MenuBar()

        menu_file = wx.Menu()
        m_open_m3i = menu_file.Append(-1, "&Open m3i file\tCtrl-O", "Open M3i File")
        self.Bind(wx.EVT_MENU, self.on_open_m3i, m_open_m3i)

        m_open_trace = menu_file.Append(-1, "&Open trace file\tCtrl-T", "Open Trace File")
        self.Bind(wx.EVT_MENU, self.on_open_trace, m_open_trace)

        m_expt = menu_file.Append(-1, "&Save plot\tCtrl-S", "Save plot to file")
        self.Bind(wx.EVT_MENU, self.on_save_plot, m_expt)
        menu_file.AppendSeparator()

        m_exit = menu_file.Append(-1, "E&xit\tCtrl-X", "Exit")
        self.Bind(wx.EVT_MENU, self.on_exit, m_exit)

        menu_help = wx.Menu()
        m_about = menu_help.Append(-1, "&About\tF1", "About")
        self.Bind(wx.EVT_MENU, self.on_about, m_about)

        self.menubar.Append(menu_file, "&File")
        self.menubar.Append(menu_help, "&Help")
        self.SetMenuBar(self.menubar)

    def create_main_panel(self):
        """ Creates the main panel with all the controls on it:
             * mpl canvas
             * mpl navigation toolbar
             * Control panel for interaction
        """
        self.panel = wx.Panel(self)

        # Create the mpl Figure and FigCanvas objects.
        # 5x4 inches, 100 dots-per-inch
        #

        self.dpi = 100
        self.fig = Figure((15.0, 14.0), dpi=self.dpi)
        self.analyzer = Analyzer(self.fig)
        self.canvas = FigCanvas(self.panel, -1, self.fig)
        # self.canvas.mpl_connect('motion_notify_event', self.on_pick)
        self.canvas.mpl_connect('pick_event', self.on_pick)
        # Create the navigation toolbar, tied to the canvas
        #
        self.toolbar = NavigationToolbar(self.canvas)

        self.radio1 = wx.RadioButton(self.panel, -1, "READ", style=wx.RB_GROUP)
        self.radio2 = wx.RadioButton(self.panel, -1, "WRITE")
        self.radio3 = wx.RadioButton(self.panel, -1, "READ/WRITE")

        self.textbox = wx.TextCtrl(
            self.panel,
            size=(200, -1),
            style=wx.TE_PROCESS_ENTER,
            value="10")
        self.Bind(wx.EVT_TEXT_ENTER, self.on_text_enter, self.textbox)

        self.textbox_label = wx.StaticText(self.panel, -1, "Bandwidth Window")
        self.title1 = wx.StaticText(self.panel, -1, "General Settings: ")
        self.title2 = wx.StaticText(self.panel, -1, ".m3i Settings: ")
        self.title3 = wx.StaticText(self.panel, -1, ".trace Settings: ")
        font = wx.Font(15, wx.ROMAN, wx.NORMAL, wx.FONTWEIGHT_BOLD)
        self.title1.SetFont(font)
        self.title2.SetFont(font)
        self.title3.SetFont(font)

        self.fitting = wx.CheckBox(self.panel, -1, "Fitting")
        self.toogle = wx.ToggleButton(self.panel, -1, "Histograms")
        self.clear = wx.Button(self.panel, -1, "Clear")
        self.radio4 = wx.RadioButton(self.panel, -1, "All", style=wx.RB_GROUP)
        self.radio5 = wx.RadioButton(self.panel, -1, "Bandwidth Only")
        self.radio6 = wx.RadioButton(self.panel, -1, "Data Only")
        self.burst_mode = wx.CheckBox(self.panel, -1, "Burst Mode")

        self.Bind(wx.EVT_CHECKBOX, self.on_burst_mode, self.burst_mode)
        self.Bind(wx.EVT_CHECKBOX, self.on_check, self.fitting)
        self.Bind(wx.EVT_TOGGLEBUTTON, self.on_toggle, self.toogle)
        self.Bind(wx.EVT_BUTTON, self.on_clear, self.clear)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_rw, self.radio1)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_rw, self.radio2)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_rw, self.radio3)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_show, self.radio4)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_show, self.radio5)
        self.Bind(wx.EVT_RADIOBUTTON, self.on_radio_select_show, self.radio6)

        self.burst_mode.Enable()
        self.textbox.Disable()
        self.radio1.Disable()
        self.radio2.Disable()
        self.radio3.Disable()
        self.radio4.Disable()
        self.radio5.Disable()
        self.radio6.Disable()
        self.fitting.Disable()
        self.toogle.Disable()
        self.clear.Disable()

        self.coords = wx.TextCtrl(self.panel, -1, value=" 0, 0 ", style=wx.TE_READONLY | wx.TE_MULTILINE,
                                  size=(250, 215))

        self.stats = wx.TextCtrl(self.panel, -1, value="Welcome to Profiler\n\nTo use burst mode, select the checkbox BEFORE opening the .m3i file.", size=(250, 110),
                                 style=wx.TE_READONLY | wx.TE_MULTILINE)

        #
        # Layout with box sizers
        #

        self.mainbox = wx.BoxSizer(wx.VERTICAL)
        self.mainbox.Add(self.canvas, 1, wx.LEFT | wx.TOP | wx.GROW)
        self.mainbox.Add(self.toolbar, 0, wx.EXPAND)
        self.box = wx.BoxSizer(wx.HORIZONTAL)
        self.lbox = wx.BoxSizer(wx.VERTICAL)
        self.rbox = wx.BoxSizer(wx.VERTICAL)

        self.box.AddSpacer(5)
        self.lbox.AddSpacer(5)
        self.rbox.AddSpacer(10)

        self.lbox.AddSpacer(5)
        self.lbox.Add(self.stats, 0, wx.LEFT)
        self.lbox.AddSpacer(5)
        self.lbox.Add(self.coords, 0, wx.RIGHT)
        self.lbox.AddSpacer(5)

        self.rbox.Add(self.title1, 0, wx.LEFT)
        self.rbox.AddSpacer(5)
        self.rbox.Add(self.textbox_label, 0, wx.LEFT)
        self.rbox.Add(self.textbox, 0, wx.LEFT)
        self.rbox.AddSpacer(10)
        self.hrbox = wx.BoxSizer(wx.HORIZONTAL)
        self.hrbox.Add(self.toogle, 0, wx.RIGHT)
        self.hrbox.AddSpacer(2)
        self.hrbox.Add(self.fitting, 0, wx.LEFT)
        self.rbox.Add(self.hrbox, 0, wx.LEFT)
        self.rbox.AddSpacer(10)
        self.rbox.Add(self.clear, 0, wx.RIGHT)
        self.rbox.AddSpacer(5)
        self.rbox.Add(self.title2, 0, wx.LEFT)
        self.rbox.AddSpacer(7)
        self.rbox.Add(self.burst_mode, 0, wx.RIGHT)
        self.rbox.AddSpacer(7)
        self.rbox.Add(self.radio1, 0, wx.LEFT)
        self.rbox.Add(self.radio2, 0, wx.LEFT)
        self.rbox.Add(self.radio3, 0, wx.LEFT)
        self.rbox.AddSpacer(5)
        self.rbox.Add(self.title3, 0, wx.LEFT)
        self.rbox.AddSpacer(7)
        self.rbox.Add(self.radio4, 0, wx.LEFT)
        self.rbox.Add(self.radio5, 0, wx.LEFT)
        self.rbox.Add(self.radio6, 0, wx.LEFT)
        self.rbox.AddSpacer(5)

        self.mainbox.Add(self.box, 0, wx.LEFT)
        self.box.Add(self.lbox, 0, wx.LEFT)
        self.box.AddSpacer(10)
        self.box.Add(self.rbox, 0, wx.RIGHT)

        self.panel.SetSizer(self.mainbox)
        self.panel.SetAutoLayout(True)
        self.mainbox.Fit(self)

    def create_status_bar(self):
        """
        Creates the status bar
        """
        self.statusbar = self.CreateStatusBar()

    def draw(self):
        """
        Redraws the figure
        """
        # setup matplotlib fontsize
        matplotlib.rcParams['font.size'] = 10.0

        try:
            if self.mode == 'm3i':
                self.analyzer.draw_m3i()
            elif self.mode == 'trace':
                self.analyzer.draw_trace()
            self.canvas.draw()
        except ValueError:
            pass

        if self.mode == 'm3i':
            stats_text = "Total Transactions: {0}\nTotal Data: {1} Bytes \nTotal Cycles: {2}".format(self.analyzer.num_transactions,
                                                                                                     self.analyzer.total_data,
                                                                                                     self.analyzer.cumulative_cycles)
        elif self.mode == 'trace':
            total_data = self.analyzer.total_data
            finish_time = self.analyzer.times[self.analyzer.type][-1]
            bandwidth = (float(total_data) / float(finish_time)) if finish_time > 0 else "infinity"
            stats_text = ("Total Data: {0} Bytes \nFinish Time: {1} seconds\nBandwidth: {2} B/s\n\n"
                         "The Bandwidth might not be exact for sub-sampled traces").format(total_data, finish_time, bandwidth)
        else:
            stats_text = "Welcome to Profiler\n\nTo use burst mode, select the checkbox BEFORE opening the .m3i file."

        self.stats.SetValue(stats_text)
        self.coords.SetValue(" 0, 0 ",)

    def on_clear(self, event):

        self.textbox.Disable()
        self.radio1.Disable()
        self.radio2.Disable()
        self.radio3.Disable()
        self.radio4.Disable()
        self.radio5.Disable()
        self.radio6.Disable()
        self.radio1.SetValue(False)
        self.radio2.SetValue(False)
        self.radio3.SetValue(False)
        self.radio4.SetValue(False)
        self.radio5.SetValue(False)
        self.radio6.SetValue(False)
        self.fitting.Disable()
        self.toogle.Disable()
        self.clear.Disable()
        self.burst_mode.Enable()

        self.mode = ''
        self.analyzer.init_stats()
        self.fig.clf()
        self.draw()

    def on_check(self, event):
        self.analyzer.stats_fitting = event.IsChecked()
        self.fig.clf()
        self.draw()

    def on_radio_select_rw(self, event):
        radio_selected = event.GetEventObject().GetLabel()
        self.analyzer.type = radio_selected
        self.fig.clf()
        self.draw()

    def on_radio_select_show(self, event):
        radio_selected = event.GetEventObject().GetLabel()
        if radio_selected ==  "All":
            self.analyzer.show_bw_only = False
            self.analyzer.show_data_only = False
        elif radio_selected == "Bandwidth Only":
            self.analyzer.show_bw_only = True
            self.analyzer.show_data_only = False
        elif radio_selected == "Data Only":
            self.analyzer.show_bw_only = False
            self.analyzer.show_data_only = True
        self.fig.clf()
        self.draw()


    def on_text_enter(self, event):
        self.analyzer.bw_window = int(self.textbox.GetValue())
        self.fig.clf()
        self.draw()

    def on_toggle(self, event):
        self.analyzer.show_histograms = not self.analyzer.show_histograms
        if self.analyzer.show_histograms:
                self.fitting.Enable()
        else:
                self.fitting.Disable()
        self.fig.clf()
        self.draw()

    def on_burst_mode(self, event):
        self.analyzer.show_burst_mode = not self.analyzer.show_burst_mode
        self.fig.clf()
        self.draw()

    def on_pick(self, event):
        try:
            # try scatterplot
            xdata, ydata = event.artist.get_offsets().T
            x, y = xdata[event.ind], ydata[event.ind]
            text = ""
            for xe, ye in zip(x, y):
                text += "{0:d} , {1:d}\n".format(int(xe), int(ye))
            self.coords.SetValue(text)
        except AttributeError:
            # not a scatterplot, do not pick
            pass

    def on_open_m3i(self, event):
        """
        :param event:
        """

        if self.mode == 'trace':
            msg = ("Cannot open an .m3i file whilst displaying a .trace file.\n"
                   "Clear the current display to open a .m3i file.")
            title = "File open error."
            self.on_popup(msg, title)
            return

        file_choices = "M3i *.m3i|*.trc"
        dlg = wx.FileDialog(
            self,
            message="Open M3i File",
            wildcard=file_choices,
            style=wx.FD_OPEN | wx.FD_CHANGE_DIR)

        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.SetTitle(self.title + " " + path)
            self.statusbar.SetStatusText(path)
            self.analyzer.type = self.analyzer.parse_m3i(path)
            self.textbox.Enable()
            self.panel.Enable()
            self.toogle.Enable()
            self.clear.Enable()
            self.radio4.Disable()
            self.radio5.Disable()
            self.radio6.Disable()
            self.burst_mode.Disable()
            if self.analyzer.type != 'READ/WRITE':
                if self.analyzer.type == 'READ':
                    self.radio1.SetValue(True)
                    self.radio1.Enable()
                    self.radio2.Disable()
                    self.radio3.Disable()
                else:
                    self.radio1.Disable()
                    self.radio2.SetValue(True)
                    self.radio2.Enable()
                    self.radio3.Disable()
            else:
                self.radio1.Enable()
                self.radio2.Enable()
                self.radio3.SetValue(True)
                self.radio3.Enable()

            self.mode = 'm3i'
            self.draw()

    def on_open_trace(self, event):
        """
        :param event:
        """

        if self.mode == 'm3i':
            msg = ("Cannot open an .trace file whilst displaying a .m3i file.\n"
                   "Clear the current display to open a .trace file.")
            title = "File open error."
            self.on_popup(msg, title)
            return

        file_choices = "Trace (*.trace)|*.trace"
        dlg = wx.FileDialog(
            self,
            message="Open Trace File",
            wildcard=file_choices,
            style=wx.FD_OPEN | wx.FD_CHANGE_DIR)

        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.SetTitle(self.title + " " + path)
            self.statusbar.SetStatusText(path)

            trace_type = self.analyzer.parse_trace(path)
            self.analyzer.type = trace_type
            self.mode = 'trace'

            self.textbox.Enable()
            self.panel.Enable()
            self.toogle.Enable()
            self.clear.Enable()
            self.radio4.Enable()
            self.radio4.SetValue(True)
            self.radio5.Enable()
            self.radio6.Enable()
            self.burst_mode.Disable()

            self.draw()

    def on_save_plot(self, event):
        """
        :param event:
        """
        file_choices = "PNG (*.png)|*.png"

        dlg = wx.FileDialog(
            self,
            message="Save plot as...",
            defaultFile="plot.png",
            wildcard=file_choices,
            style=wx.SAVE)

        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.canvas.print_figure(path, dpi=self.dpi)
            self.flash_status_message("Saved to %s" % path)

    def on_exit(self, event):
        self.Destroy()

    def on_about(self, event):
        """
        :param event:
        """
        msg = ("Copyright (c) 2017 ARM Limited\n"
        "All rights reserved\n"
        "For Internal Use Only\n"
        "Authors: Matteo Andreozzi\n"
        "               Riken Gohil")
        dlg = wx.MessageDialog(self, msg, "About", wx.OK)
        dlg.ShowModal()
        dlg.Destroy()

    def on_popup(self, msg, title):
        dlg = wx.MessageDialog(self, msg, title, wx.OK)
        dlg.ShowModal()
        dlg.Destroy()

    def flash_status_message(self, msg, flash_len_ms=1500):
        self.statusbar.SetStatusText(msg)
        self.timeroff = wx.Timer(self)
        self.Bind(
            wx.EVT_TIMER,
            self.on_flash_status_off,
            self.timeroff)
        self.timeroff.Start(flash_len_ms, oneShot=True)

    def on_flash_status_off(self, event):
        self.statusbar.SetStatusText('')


if __name__ == '__main__':
    app = wx.App(False)
    app.frame = BarsFrame()
    app.frame.Show()
    warnings.filterwarnings("ignore", category=UserWarning, module="matplotlib")
    app.MainLoop()
