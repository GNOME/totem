/*
 * Copyright (C) Simon Wenner 2011 <simon@wenner.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

using GLib;
using Totem;
using Peas;
using Clutter;
using GtkClutter;

public const string GIO_ROTATION_FILE_ATTRIBUTE = "metadata::totem::rotation";

class RotationPlugin: GLib.Object, Peas.Activatable
{
    private const int STATE_COUNT = 4;
    public GLib.Object object { owned get; construct; }
    private Bacon.VideoWidget bvw = null;
    private GLib.SimpleAction rotate_left_action = null;
    private GLib.SimpleAction rotate_right_action = null;

    public void activate ()
    {
        Totem.Object t = (Totem.Object) this.object;
        string mrl = t.get_current_mrl ();
        GLib.Menu menu = t.get_menu_section ("rotation-placeholder");

        this.bvw = t.get_video_widget () as Bacon.VideoWidget;

        // add interface elements to control the rotation
        this.rotate_left_action = new GLib.SimpleAction ("rotate-left", null);
        this.rotate_left_action.activate.connect (this.cb_rotate_left);
        t.add_action (this.rotate_left_action);
        t.add_accelerator("<Primary><Shift>R", "app.rotate-left", null);

        this.rotate_right_action = new GLib.SimpleAction ("rotate-right", null);
        this.rotate_right_action.activate.connect (this.cb_rotate_right);
        t.add_action (this.rotate_right_action);
        t.add_accelerator("<Primary>R", "app.rotate-right", null);

        GLib.MenuItem item = new GLib.MenuItem (_("_Rotate Clockwise"), "app.rotate-right");
        item.set_attribute ("accel", "s", "<Primary>R");
        menu.append_item (item);

        item = new GLib.MenuItem (_("Rotate Counterc_lockwise"), "app.rotate-left");
        item.set_attribute ("accel", "s", "<Primary><Shift>R");
        menu.append_item (item);

        if (mrl == null) {
            this.rotate_right_action.set_enabled (false);
            this.rotate_left_action.set_enabled (false);
        }

        // read the state of the current video from the GIO attribute
        if (mrl != null) {
            this.try_restore_state.begin (mrl, (o, r) => {
                this.try_restore_state.end (r);
            });
        }

        t.file_closed.connect (this.cb_file_closed);
        t.file_opened.connect (this.cb_file_opened);
    }

    public void deactivate ()
    {
        Totem.Object t = (Totem.Object) this.object;

        // disconnect callbacks
        t.file_closed.disconnect (this.cb_file_closed);
        t.file_opened.disconnect (this.cb_file_opened);

        // remove interface elements to control the rotation
        t.empty_menu_section ("rotation-placeholder");

        // remove accelerators
        t.remove_accelerator("app.rotate-right", null);
        t.remove_accelerator("app.rotate-left", null);

        // undo transformations
        this.bvw.set_rotation (Bacon.Rotation.R_ZERO);
    }

    public void update_state ()
    {
        //no-op
    }

    private void cb_rotate_left ()
    {
        int state = (this.bvw.get_rotation() - 1) % STATE_COUNT;
        this.bvw.set_rotation ((Bacon.Rotation) state);
        this.store_state.begin ((o, r) => { this.store_state.end (r); });
    }

    private void cb_rotate_right ()
    {
        int state = (this.bvw.get_rotation() + 1) % STATE_COUNT;
        this.bvw.set_rotation ((Bacon.Rotation) state);
        this.store_state.begin ((o, r) => { this.store_state.end (r); });
    }

    private void cb_file_closed ()
    {
        // reset the rotation
        this.bvw.set_rotation (Bacon.Rotation.R_ZERO);
        this.rotate_right_action.set_enabled (false);
        this.rotate_left_action.set_enabled (false);
    }

    private void cb_file_opened (string mrl)
    {
        this.rotate_right_action.set_enabled (true);
        this.rotate_left_action.set_enabled (true);
        this.try_restore_state.begin (mrl, (o, r) => {
            this.try_restore_state.end (r);
        });
    }

    private async void store_state ()
    {
        Totem.Object t = (Totem.Object) this.object;
        string mrl = t.get_current_mrl ();

        if (mrl == null) {
            return;
        }

        var file = GLib.File.new_for_uri (mrl);
        try {
            Bacon.Rotation rotation;
            var file_info = yield file.query_info_async (GIO_ROTATION_FILE_ATTRIBUTE,
                    GLib.FileQueryInfoFlags.NONE);

            string state_str = "";
            rotation = this.bvw.get_rotation ();
            if (rotation != Bacon.Rotation.R_ZERO) {
                state_str = "%u".printf ((uint) rotation);
            }
            file_info.set_attribute_string (GIO_ROTATION_FILE_ATTRIBUTE, state_str);
            yield file.set_attributes_async (file_info, GLib.FileQueryInfoFlags.NONE,
                    GLib.Priority.DEFAULT, null, null);
        } catch (GLib.Error e) {
            GLib.warning ("Could not store file attribute: %s", e.message);
        }
    }

    private async void try_restore_state (string mrl)
    {
        var file = GLib.File.new_for_uri (mrl);
        if (file.has_uri_scheme ("http") || file.has_uri_scheme ("dvd"))
          return;
        try {
            var file_info = yield file.query_info_async (GIO_ROTATION_FILE_ATTRIBUTE,
                    GLib.FileQueryInfoFlags.NONE);
            string state_str = file_info.get_attribute_string (GIO_ROTATION_FILE_ATTRIBUTE);
            if (state_str != null) {
                int state = (Bacon.Rotation) uint64.parse (state_str);
                this.bvw.set_rotation ((Bacon.Rotation) state);
            }
        } catch (GLib.Error e) {
            GLib.warning ("Could not query file attribute: %s", e.message);
        }
    }
}

[ModuleInit]
public void peas_register_types (GLib.TypeModule module) {
    var objmodule = module as Peas.ObjectModule;
    objmodule.register_extension_type (typeof (Peas.Activatable), typeof (RotationPlugin));
}
