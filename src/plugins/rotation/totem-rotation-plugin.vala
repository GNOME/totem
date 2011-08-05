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
    private enum Rotation
    {
        _IDENTITY = 0,
        _90R = 1,
        _180 = 2,
        _90L = 3
    }
    private const int STATE_COUNT = 4;
    public weak GLib.Object object { get; construct; }
    private weak Clutter.Actor video = null;
    private uint ui_id;
    private Gtk.ActionGroup action_group;
    private Rotation state = Rotation._IDENTITY;
    private float width;
    private float height;

    public void activate ()
    {
        Totem.Object t = (Totem.Object) this.object;
        GtkClutter.Embed bvw = (GtkClutter.Embed) Totem.get_video_widget (t);
        unowned Clutter.Stage stage = (Clutter.Stage) bvw.get_stage ();
        string mrl = t.get_current_mrl ();

        // add interface elements to control the rotation
        unowned Gtk.UIManager ui_manager = t.get_ui_manager ();
        this.ui_id = ui_manager.new_merge_id ();
        ui_manager.add_ui (this.ui_id, "/ui/tmw-menubar/view/next-angle",
                "rotate-left", "rotate-left", Gtk.UIManagerItemType.AUTO, false);
        ui_manager.add_ui (this.ui_id, "/ui/tmw-menubar/view/next-angle",
                "rotate-right", "rotate-right", Gtk.UIManagerItemType.AUTO, false);

        var rotate_right  = new Gtk.Action ("rotate-right", _("_Rotate Clockwise"), null, null);
        rotate_right.activate.connect (this.cb_rotate_right);
        var rotate_left = new Gtk.Action ("rotate-left", _("Rotate Counterc_lockwise"), null, null);
        rotate_left.activate.connect (this.cb_rotate_left);

        this.action_group = new Gtk.ActionGroup ("RotationActions");
        this.action_group.add_action_with_accel (rotate_right, "<ctrl>R");
        this.action_group.add_action_with_accel (rotate_left, "<ctrl><shift>R");
        if (mrl == null) {
            this.action_group.sensitive = false;
        }
        ui_manager.insert_action_group (this.action_group, 0);

        // search for the actor which contains the video
        for (int i = 0; i < stage.get_n_children (); i++) {
            Clutter.Actor actor = stage.get_nth_child (i);
            if (actor.name == "frame") {
                this.video = actor;
                break;
            }
        }

        if (video == null) {
            GLib.critical ("Could not find the clutter actor 'frame'.");
            return;
        }

        this.width = this.video.width;
        this.height = this.video.height;

        // read the state of the current video from the GIO attribute
        if (mrl != null) {
            this.try_restore_state (mrl);
        }

        // get notified if the video gets resized
        this.video.allocation_changed.connect (this.cb_allocation_changed);

        t.file_closed.connect (this.cb_file_closed);
        t.file_opened.connect (this.cb_file_opened);
    }

    public void deactivate ()
    {
        Totem.Object t = (Totem.Object) this.object;

        // disconnect callbacks
        this.video.allocation_changed.disconnect (this.cb_allocation_changed);
        t.file_closed.disconnect (this.cb_file_closed);
        t.file_opened.disconnect (this.cb_file_opened);

        // remove interface elements to control the rotation
        unowned Gtk.UIManager ui_manager = t.get_ui_manager ();
        ui_manager.remove_ui (this.ui_id);
        ui_manager.remove_action_group (this.action_group);

        // undo transformations
        this.state = Rotation._IDENTITY;
        this.video.set_rotation (Clutter.RotateAxis.Z_AXIS, 0, 0, 0, 0);
        this.video.set_scale_full (1, 1, 0, 0);
    }

    public void update_state ()
    {
        this.update_video_geometry ();
    }

    private void update_video_geometry ()
    {
        float center_x = this.width * 0.5f;
        float center_y = this.height * 0.5f;
        double scale = 1.0;

        // scale so that the larger side fits the smaller side
        if (this.state % 2 == 1) { // _90R or _90L
            if (this.width > this.height) {
                scale = this.height / (double) this.width;
            } else {
                scale = this.width / (double) this.height;
            }
        }

        this.video.set_rotation (Clutter.RotateAxis.Z_AXIS, 90.0 * this.state, center_x, center_y, 0);
        this.video.set_scale_full (scale, scale, center_x, center_y);
    }

    private void cb_allocation_changed (Clutter.ActorBox box, Clutter.AllocationFlags flags)
    {
        this.width = box.x2 - box.x1;
        this.height = box.y2 - box.y1;
        this.update_video_geometry ();
    }

    private void cb_rotate_left ()
    {
        this.state = (this.state - 1) % STATE_COUNT;
        this.update_video_geometry ();
        this.store_state ();
    }

    private void cb_rotate_right ()
    {
        this.state = (this.state + 1) % STATE_COUNT;
        this.update_video_geometry ();
        this.store_state ();
    }

    private void cb_file_closed ()
    {
        // reset the rotation
        this.state = Rotation._IDENTITY;
        this.update_video_geometry ();
        this.action_group.sensitive = false;
    }

    private void cb_file_opened (string mrl)
    {
        this.action_group.sensitive = true;
        this.try_restore_state (mrl);
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
            var file_info = yield file.query_info_async (GIO_ROTATION_FILE_ATTRIBUTE,
                    GLib.FileQueryInfoFlags.NONE);

            string state_str = "";
            if (this.state != Rotation._IDENTITY) {
                state_str = "%u".printf ((uint) this.state);
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
        try {
            var file_info = yield file.query_info_async (GIO_ROTATION_FILE_ATTRIBUTE,
                    GLib.FileQueryInfoFlags.NONE);
            string state_str = file_info.get_attribute_string (GIO_ROTATION_FILE_ATTRIBUTE);
            if (state_str != null) {
                this.state = (Rotation) uint64.parse (state_str);
                this.update_video_geometry ();
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
