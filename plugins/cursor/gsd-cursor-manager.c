/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include "gnome-settings-profile.h"
#include "gsd-cursor-manager.h"
#include "gsd-input-helper.h"

#define XFIXES_CURSOR_HIDING_MAJOR 4

#define GSD_CURSOR_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_CURSOR_MANAGER, GsdCursorManagerPrivate))

struct GsdCursorManagerPrivate
{
        guint start_idle_id;
        guint added_id;
        guint removed_id;
        gboolean cursor_shown;
};

enum {
        PROP_0,
};

static void     gsd_cursor_manager_class_init  (GsdCursorManagerClass *klass);
static void     gsd_cursor_manager_init        (GsdCursorManager      *cursor_manager);
static void     gsd_cursor_manager_finalize    (GObject               *object);

G_DEFINE_TYPE (GsdCursorManager, gsd_cursor_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean
device_is_xtest (XDevice *xdevice)
{
        Atom realtype, prop;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "XTEST Device", False);
        if (!prop)
                return FALSE;

        gdk_error_trap_push ();
        if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdevice, prop, 0, 1, False,
                                XA_INTEGER, &realtype, &realformat, &nitems,
                                &bytes_after, &data) == Success) && (realtype != None)) {
                gdk_error_trap_pop_ignored ();
                XFree (data);
                return TRUE;
        }
        gdk_error_trap_pop_ignored ();

        return FALSE;
}

static void
set_cursor_visibility (GsdCursorManager *manager,
                       gboolean          visible)
{
        Display *xdisplay;
        GdkDisplay *display;
        guint n_screens;
        guint i;

        g_debug ("Attempting to %s the cursor", visible ? "show" : "hide");

        display = gdk_display_get_default ();
        xdisplay = GDK_DISPLAY_XDISPLAY (display);

        n_screens = gdk_display_get_n_screens (display);

        gdk_error_trap_push ();

        for (i = 0; i < n_screens; i++) {
                GdkScreen *screen;

                screen = gdk_display_get_screen (display, i);

                if (visible)
                        XFixesShowCursor (xdisplay, GDK_WINDOW_XID (gdk_screen_get_root_window (screen)));
                else
                        XFixesHideCursor (xdisplay, GDK_WINDOW_XID (gdk_screen_get_root_window (screen)));
        }
        if (gdk_error_trap_pop ()) {
                g_warning ("An error occurred trying to %s the cursor",
                           visible ? "show" : "hide");
        }

        manager->priv->cursor_shown = visible;
}

static gboolean
device_info_is_ps2_mouse (XDeviceInfo *info)
{
	return (g_strcmp0 (info->name, "ImPS/2 Generic Wheel Mouse") == 0);
}

static void
update_cursor_for_current (GsdCursorManager *manager)
{
        XDeviceInfo *device_info;
        guint num_mice;
        int n_devices;
        guint i;

        /* List all the pointer devices
         * ignore the touchscreens
         * ignore the XTest devices
         * see if there's anything left */

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL)
                return;

        num_mice = 0;

        for (i = 0; i < n_devices; i++) {
                XDevice *device;

                if (device_info[i].use != IsXExtensionPointer)
                        continue;

                if (device_info_is_touchscreen (&device_info[i]))
                        continue;

                if (device_info_is_ps2_mouse (&device_info[i]))
                        continue;

                gdk_error_trap_push ();
                device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_info[i].id);
                if (gdk_error_trap_pop () || (device == NULL))
                        continue;

                if (device_is_xtest (device)) {
                        XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                        continue;
                }

                num_mice++;
        }
        XFreeDeviceList (device_info);

        g_debug ("Found %d devices that aren't touchscreens or fake devices", num_mice);

        if (num_mice > 0) {
                g_debug ("Mice are present");

                if (manager->priv->cursor_shown == FALSE) {
                        set_cursor_visibility (manager, TRUE);
                }
        } else {
                g_debug ("No mice present");
                if (manager->priv->cursor_shown != FALSE) {
                        set_cursor_visibility (manager, FALSE);
                }
        }
}

static void
devices_added_cb (GdkDeviceManager *device_manager,
                  GdkDevice        *device,
                  GsdCursorManager *manager)
{
        update_cursor_for_current (manager);
}

static void
devices_removed_cb (GdkDeviceManager *device_manager,
                    GdkDevice        *device,
                    GsdCursorManager *manager)
{
        /* If devices are removed, then it's unlikely
         * a mouse appeared */
        if (manager->priv->cursor_shown == FALSE)
                return;

        update_cursor_for_current (manager);
}

static gboolean
supports_xfixes (void)
{
        gint op_code, event, error;

        return XQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                "XFIXES",
                                &op_code,
                                &event,
                                &error);
}

static gboolean
supports_cursor_xfixes (void)
{
        int major = XFIXES_CURSOR_HIDING_MAJOR;
        int minor = 0;

        gdk_error_trap_push ();

        if (!supports_xfixes ()) {
                gdk_error_trap_pop_ignored ();
                return FALSE;
        }

        if (!XFixesQueryVersion (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &major, &minor)) {
                gdk_error_trap_pop_ignored ();
                return FALSE;
        }
        gdk_error_trap_pop_ignored ();

        if (major >= XFIXES_CURSOR_HIDING_MAJOR)
                return TRUE;

        return FALSE;
}

static gboolean
gsd_cursor_manager_idle_cb (GsdCursorManager *manager)
{
        GdkDeviceManager *device_manager;

        gnome_settings_profile_start (NULL);

        if (supports_cursor_xfixes () == FALSE) {
                g_debug ("XFixes cursor extension not available, will not hide the cursor");
                return FALSE;
        }

        if (supports_xinput_devices () == FALSE) {
                g_debug ("XInput support not available, will not hide the cursor");
                return FALSE;
        }

        /* We assume that the touchscreen is builtin and
         * won't be appearing in the middle of the session... */
        if (touchscreen_is_present () == FALSE) {
                g_debug ("Did not find a touchscreen, will not hide the cursor");
                gnome_settings_profile_end (NULL);
                return FALSE;
        }

        update_cursor_for_current (manager);

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
        manager->priv->added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                    G_CALLBACK (devices_added_cb), manager);
        manager->priv->removed_id = g_signal_connect (G_OBJECT (device_manager), "device-removed",
                                                      G_CALLBACK (devices_removed_cb), manager);

        gnome_settings_profile_end (NULL);

        return FALSE;
}

gboolean
gsd_cursor_manager_start (GsdCursorManager *manager,
                          GError               **error)
{
        g_debug ("Starting cursor manager");
        gnome_settings_profile_start (NULL);

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) gsd_cursor_manager_idle_cb, manager);

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_cursor_manager_stop (GsdCursorManager *manager)
{
        GdkDeviceManager *device_manager;

        g_debug ("Stopping cursor manager");

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());

        if (manager->priv->added_id > 0) {
                g_signal_handler_disconnect (G_OBJECT (device_manager), manager->priv->added_id);
                manager->priv->added_id = 0;
        }

        if (manager->priv->removed_id > 0) {
                g_signal_handler_disconnect (G_OBJECT (device_manager), manager->priv->removed_id);
                manager->priv->removed_id = 0;
        }

        if (manager->priv->cursor_shown == FALSE) {
                set_cursor_visibility (manager, TRUE);
        }
}

static void
gsd_cursor_manager_set_property (GObject        *object,
                                 guint           prop_id,
                                 const GValue   *value,
                                 GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_cursor_manager_get_property (GObject        *object,
                                 guint           prop_id,
                                 GValue         *value,
                                 GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gsd_cursor_manager_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
        GsdCursorManager      *cursor_manager;

        cursor_manager = GSD_CURSOR_MANAGER (G_OBJECT_CLASS (gsd_cursor_manager_parent_class)->constructor (type,
                                                                                                            n_construct_properties,
                                                                                                            construct_properties));

        return G_OBJECT (cursor_manager);
}

static void
gsd_cursor_manager_dispose (GObject *object)
{
        G_OBJECT_CLASS (gsd_cursor_manager_parent_class)->dispose (object);
}

static void
gsd_cursor_manager_class_init (GsdCursorManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsd_cursor_manager_get_property;
        object_class->set_property = gsd_cursor_manager_set_property;
        object_class->constructor = gsd_cursor_manager_constructor;
        object_class->dispose = gsd_cursor_manager_dispose;
        object_class->finalize = gsd_cursor_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdCursorManagerPrivate));
}

static void
gsd_cursor_manager_init (GsdCursorManager *manager)
{
        manager->priv = GSD_CURSOR_MANAGER_GET_PRIVATE (manager);
        manager->priv->cursor_shown = TRUE;

}

static void
gsd_cursor_manager_finalize (GObject *object)
{
        GsdCursorManager *cursor_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_CURSOR_MANAGER (object));

        cursor_manager = GSD_CURSOR_MANAGER (object);

        g_return_if_fail (cursor_manager->priv != NULL);

        G_OBJECT_CLASS (gsd_cursor_manager_parent_class)->finalize (object);
}

GsdCursorManager *
gsd_cursor_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_CURSOR_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_CURSOR_MANAGER (manager_object);
}
