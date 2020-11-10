# -*- coding: utf-8 -*-

# console.py -- Console widget
#
# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

# Parts from "Interactive Python-GTK Console" (stolen from epiphany's
# console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from gedit Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx
#
# The Totem project hereby grant permission for non-gpl compatible GStreamer
# plugins to be used and distributed together with GStreamer and Totem. This
# permission are above and beyond the permissions granted by the GPL license
# Totem is covered by.
#
# Monday 7th February 2005: Christian Schaller: Add exception clause.
# See license_change file for details.

import sys
import re
import traceback
import gi

gi.require_version('Gtk', '3.0')
gi.require_version('Pango', '1.0')

from gi.repository import GLib, Pango, Gtk, Gdk # pylint: disable=wrong-import-position

class PythonConsole(Gtk.ScrolledWindow): # pylint: disable=R0902
    def __init__(self, namespace = {}, # pylint: disable=W0102
                 destroy_cb = None):
        Gtk.ScrolledWindow.__init__(self)

        self.destroy_cb = destroy_cb
        self.set_policy(Gtk.PolicyType.NEVER, # pylint: disable=E1101
                        Gtk.PolicyType.AUTOMATIC)
        self.set_shadow_type(Gtk.ShadowType.IN) # pylint: disable=E1101
        self.view = Gtk.TextView()
        self.view.modify_font(Pango.font_description_from_string('Monospace'))
        self.view.set_editable(True)
        self.view.set_wrap_mode(Gtk.WrapMode.CHAR)
        self.add(self.view) # pylint: disable=E1101
        self.view.show()

        buf = self.view.get_buffer()
        self.normal = buf.create_tag("normal")
        self.error  = buf.create_tag("error")
        self.error.set_property("foreground", "red")
        self.command = buf.create_tag("command")
        self.command.set_property("foreground", "blue")

        self.__spaces_pattern = re.compile(r'^\s+')
        self.namespace = namespace

        self.block_command = False

        # Init first line
        buf.create_mark("input-line", buf.get_end_iter(), True)
        buf.insert(buf.get_end_iter(), ">>> ")
        buf.create_mark("input", buf.get_end_iter(), True)

        # Init history
        self.history = ['']
        self.history_pos = 0
        self.current_command = ''
        self.namespace['__history__'] = self.history

        # Set up hooks for standard output.
        self.stdout = OutFile(self, sys.stdout.fileno(), self.normal)
        self.stderr = OutFile(self, sys.stderr.fileno(), self.error)

        # Signals
        self.view.connect("key-press-event", self.__key_press_event_cb)
        buf.connect("mark-set", self.__mark_set_cb)


    def __key_press_event_cb(self, view, # pylint: disable=R0911,R0912,R0914,R0915
                             event):
        modifier_mask = Gtk.accelerator_get_default_mod_mask()
        event_state = event.state & modifier_mask

        if event.keyval == Gdk.KEY_d and \
           event_state == Gdk.ModifierType.CONTROL_MASK:
            self.destroy()

        elif event.keyval == Gdk.KEY_Return and \
             event_state == Gdk.ModifierType.CONTROL_MASK:
            # Get the command
            buf = view.get_buffer()
            inp_mark = buf.get_mark("input")
            inp = buf.get_iter_at_mark(inp_mark)
            cur = buf.get_end_iter()
            line = buf.get_text(inp, cur, True)
            self.current_command = self.current_command + line + "\n"
            self.history_add(line)

            # Prepare the new line
            cur = buf.get_end_iter()
            buf.insert(cur, "\n... ")
            cur = buf.get_end_iter()
            buf.move_mark(inp_mark, cur)

            # Keep indentation of precendent line
            spaces = re.match(self.__spaces_pattern, line)
            if spaces is not None:
                buf.insert(cur, line[spaces.start() : spaces.end()])
                cur = buf.get_end_iter()

            buf.place_cursor(cur)
            GLib.idle_add(self.scroll_to_end)
            return True

        elif event.keyval == Gdk.KEY_Return:
            # Get the marks
            buf = view.get_buffer()
            lin_mark = buf.get_mark("input-line")
            inp_mark = buf.get_mark("input")

            # Get the command line
            inp = buf.get_iter_at_mark(inp_mark)
            cur = buf.get_end_iter()
            line = buf.get_text(inp, cur, True)
            self.current_command = self.current_command + line + "\n"
            self.history_add(line)

            # Make the line blue
            lin = buf.get_iter_at_mark(lin_mark)
            buf.apply_tag(self.command, lin, cur)
            buf.insert(cur, "\n")

            cur_strip = self.current_command.rstrip()

            if cur_strip.endswith(":") \
            or (self.current_command[-2:] != "\n\n" and self.block_command):
                # Unfinished block command
                self.block_command = True
                com_mark = "... "
            elif cur_strip.endswith("\\"):
                com_mark = "... "
            else:
                # Eval the command
                self.__run(self.current_command)
                self.current_command = ''
                self.block_command = False
                com_mark = ">>> "

            # Prepare the new line
            cur = buf.get_end_iter()
            buf.move_mark(lin_mark, cur)
            buf.insert(cur, com_mark)
            cur = buf.get_end_iter()
            buf.move_mark(inp_mark, cur)
            buf.place_cursor(cur)
            GLib.idle_add(self.scroll_to_end)
            return True

        elif event.keyval == Gdk.KEY_KP_Down or event.keyval == Gdk.KEY_Down:
            # Next entry from history
            view.emit_stop_by_name("key_press_event")
            self.history_down()
            GLib.idle_add(self.scroll_to_end)
            return True

        elif event.keyval == Gdk.KEY_KP_Up or event.keyval == Gdk.KEY_Up:
            # Previous entry from history
            view.emit_stop_by_name("key_press_event")
            self.history_up()
            GLib.idle_add(self.scroll_to_end)
            return True

        elif event.keyval == Gdk.KEY_KP_Left or \
             event.keyval == Gdk.KEY_Left or \
             event.keyval == Gdk.KEY_BackSpace:
            buf = view.get_buffer()
            inp = buf.get_iter_at_mark(buf.get_mark("input"))
            cur = buf.get_iter_at_mark(buf.get_insert())
            return inp.compare(cur) == 0

        elif event.keyval == Gdk.KEY_Home:
            # Go to the begin of the command instead of the begin of the line
            buf = view.get_buffer()
            inp = buf.get_iter_at_mark(buf.get_mark("input"))
            if event_state == Gdk.ModifierType.SHIFT_MASK:
                buf.move_mark_by_name("insert", inp)
            else:
                buf.place_cursor(inp)
            return True

        return False

    def __mark_set_cb(self, buf, _, name):
        inp = buf.get_iter_at_mark(buf.get_mark("input"))
        pos   = buf.get_iter_at_mark(buf.get_insert())
        self.view.set_editable(pos.compare(inp) != -1)

    def get_command_line(self):
        buf = self.view.get_buffer()
        inp = buf.get_iter_at_mark(buf.get_mark("input"))
        cur = buf.get_end_iter()
        return buf.get_text(inp, cur, True)

    def set_command_line(self, command):
        buf = self.view.get_buffer()
        mark = buf.get_mark("input")
        inp = buf.get_iter_at_mark(mark)
        cur = buf.get_end_iter()
        buf.delete(inp, cur)
        buf.insert(inp, command)
        buf.select_range(buf.get_iter_at_mark(mark),
                         buf.get_end_iter())
        self.view.grab_focus()

    def history_add(self, line):
        if line.strip() != '':
            self.history_pos = len(self.history)
            self.history[self.history_pos - 1] = line
            self.history.append('')

    def history_up(self):
        if self.history_pos > 0:
            self.history[self.history_pos] = self.get_command_line()
            self.history_pos = self.history_pos - 1
            self.set_command_line(self.history[self.history_pos])

    def history_down(self):
        if self.history_pos < len(self.history) - 1:
            self.history[self.history_pos] = self.get_command_line()
            self.history_pos = self.history_pos + 1
            self.set_command_line(self.history[self.history_pos])

    def scroll_to_end(self):
        iterator = self.view.get_buffer().get_end_iter()
        self.view.scroll_to_iter(iterator, 0.0, False, 0.5, 0.5)
        return False

    def write(self, text, tag = None):
        buf = self.view.get_buffer()
        if tag is None:
            buf.insert(buf.get_end_iter(), text)
        else:
            buf.insert_with_tags(buf.get_end_iter(), text, tag)

        GLib.idle_add(self.scroll_to_end)

    def eval(self, command, display_command = False):
        buf = self.view.get_buffer()
        lin = buf.get_mark("input-line")
        buf.delete(buf.get_iter_at_mark(lin),
                   buf.get_end_iter())

        if isinstance(command, (list, tuple)):
            for char in command:
                if display_command:
                    self.write(">>> " + char + "\n", self.command)
                self.__run(char)
        else:
            if display_command:
                self.write(">>> " + char + "\n", self.command)
            self.__run(command)

        cur = buf.get_end_iter()
        buf.move_mark_by_name("input-line", cur)
        buf.insert(cur, ">>> ")
        cur = buf.get_end_iter()
        buf.move_mark_by_name("input", cur)
        self.view.scroll_to_iter(buf.get_end_iter(), 0.0, False, 0.5, 0.5)

    def __run(self, command):
        sys.stdout, self.stdout = self.stdout, sys.stdout
        sys.stderr, self.stderr = self.stderr, sys.stderr

        try:
            try:
                res = eval(command, self.namespace, self.namespace) # pylint: disable=eval-used
                if res is not None:
                    print(res)
            except SyntaxError:
                exec(command, self.namespace) # pylint: disable=W0122
        except: # pylint: disable=W0702
            if hasattr(sys, 'last_type') and sys.last_type == SystemExit: # pylint: disable=E1101
                self.destroy()
            else:
                traceback.print_exc()

        sys.stdout, self.stdout = self.stdout, sys.stdout
        sys.stderr, self.stderr = self.stderr, sys.stderr

    def destroy(self): # pylint: disable=arguments-differ
        pass
        #Gtk.ScrolledWindow.destroy(self)

class OutFile:
    """A fake output file object. It sends output to a TK test widget,
    and if asked for a file number, returns one set on instance creation"""
    def __init__(self, console, file_no, tag):
        self.file_no = file_no
        self.console = console
        self.tag = tag
    def close(self):
        pass
    def flush(self):
        pass
    def fileno(self):
        return self.file_no
    def isatty(self): # pylint: disable=R0201
        return 0
    def read(self, _): # pylint: disable=R0201
        return ''
    def readline(self): # pylint: disable=R0201
        return ''
    def readlines(self): # pylint: disable=R0201
        return []
    def write(self, seg):
        self.console.write(seg, self.tag)
    def writelines(self, lines):
        self.console.write(lines, self.tag)
    def seek(self, _): # pylint: disable=R0201
        raise IOError((29, 'Illegal seek'))
    def tell(self): # pylint: disable=R0201
        raise IOError((29, 'Illegal seek'))
    truncate = tell
