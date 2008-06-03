/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright � 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-keyboard-config.h>

#include "gsd-xmodmap.h"
#include "gsd-keyboard-xkb.h"
#include "delayed-dialog.h"

static XklEngine *xkl_engine;

static GkbdDesktopConfig current_config;
static GkbdKeyboardConfig current_kbd_config;

/* never terminated */
static GkbdKeyboardConfig initial_sys_kbd_config;

static gboolean inited_ok = FALSE;

static guint notify_desktop = 0;
static guint notify_keyboard = 0;

static PostActivationCallback pa_callback = NULL;
static void *pa_callback_user_data = NULL;

static const char KNOWN_FILES_KEY[] =
    "/desktop/gnome/peripherals/keyboard/general/known_file_list";

static char *gdm_keyboard_layout = NULL;

#define noGSDKX

#ifdef GSDKX
static FILE *logfile;

static void
gsd_keyboard_log_appender (const char file[],
                           const char function[],
                           int        level,
                           const char format[],
                           va_list    args)
{
        time_t now = time (NULL);
        fprintf (logfile, "[%08ld,%03d,%s:%s/] \t", now,
                 level, file, function);
        vfprintf (logfile, format, args);
        fflush (logfile);
}
#endif

static void
activation_error (void)
{
        char const *vendor = ServerVendor (GDK_DISPLAY ());
        int         release = VendorRelease (GDK_DISPLAY ());
        GtkWidget  *dialog;
        gboolean    badXFree430Release;

        badXFree430Release = (vendor != NULL)
            && (0 == strcmp (vendor, "The XFree86 Project, Inc"))
            && (release / 100000 == 403);

        /* VNC viewers will not work, do not barrage them with warnings */
        if (NULL != vendor && NULL != strstr (vendor, "VNC"))
                return;

        dialog = gtk_message_dialog_new_with_markup (NULL,
                                                     0,
                                                     GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_CLOSE,
                                                     _
                                                     ("Error activating XKB configuration.\n"
                                                      "It can happen under various circumstances:\n"
                                                      "- a bug in libxklavier library\n"
                                                      "- a bug in X server (xkbcomp, xmodmap utilities)\n"
                                                      "- X server with incompatible libxkbfile implementation\n\n"
                                                      "X server version data:\n%s\n%d\n%s\n"
                                                      "If you report this situation as a bug, please include:\n"
                                                      "- The result of <b>%s</b>\n"
                                                      "- The result of <b>%s</b>"),
                                                     vendor,
                                                     release,
                                                     badXFree430Release
                                                     ?
                                                     _
                                                     ("You are using XFree 4.3.0.\n"
                                                      "There are known problems with complex XKB configurations.\n"
                                                      "Try using a simpler configuration or taking a fresher version of XFree software.")
                                                     : "",
                                                     "xprop -root | grep XKB",
                                                     "gconftool-2 -R /desktop/gnome/peripherals/keyboard/kbd");
        g_signal_connect (dialog, "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);
        gsd_delayed_show_dialog (dialog);
}

static void
apply_settings (void)
{
        if (!inited_ok)
                return;

        gkbd_desktop_config_load_from_gconf (&current_config);
        /* again, probably it would be nice to compare things
           before activating them */
        gkbd_desktop_config_activate (&current_config);
}

static void
apply_xkb_settings (void)
{
        GConfClient *conf_client;
        GkbdKeyboardConfig current_sys_kbd_config;

        if (!inited_ok)
                return;

        conf_client = gconf_client_get_default ();

        gkbd_keyboard_config_init (&current_sys_kbd_config,
                                   conf_client,
                                   xkl_engine);

        gkbd_keyboard_config_load_from_gconf (&current_kbd_config,
                                              &initial_sys_kbd_config);

        gkbd_keyboard_config_load_from_x_current (&current_sys_kbd_config,
                                                  NULL);

        /* With GDM the user can already set a layout from the login
         * screen. Try to keep that setting */
        if (gdm_keyboard_layout != NULL) {
                if (current_kbd_config.layouts_variants == NULL) {
                        current_kbd_config.layouts_variants = g_slist_append (NULL, gdm_keyboard_layout);
                        gconf_client_set_list (conf_client,
                                               GKBD_KEYBOARD_CONFIG_KEY_LAYOUTS,
                                               GCONF_VALUE_STRING,
                                               current_kbd_config.layouts_variants,
                                               NULL);
                } else {
                         GSList *l;
                         int i;
                         for (i = 0, l = current_kbd_config.layouts_variants; l; i++, l = l->next) {
                                 if (strcmp (gdm_keyboard_layout, l->data) == 0) {
                                        xkl_engine_lock_group (current_config.engine, i);
                                        break;
                                 }
                         }
                }
                gdm_keyboard_layout = NULL;
        }

        /* Activate - only if different! */
        if (!gkbd_keyboard_config_equals
            (&current_kbd_config, &current_sys_kbd_config)) {
                if (gkbd_keyboard_config_activate (&current_kbd_config)) {
                        if (pa_callback != NULL) {
                                (*pa_callback) (pa_callback_user_data);
                        }
                } else {
                        g_warning
                            ("Could not activate the XKB configuration");
                        activation_error ();
                }
        } else
                xkl_debug (100,
                           "Actual KBD configuration was not changed: redundant notification\n");

        gkbd_keyboard_config_term (&current_sys_kbd_config);
}

static void
gsd_keyboard_xkb_analyze_sysconfig (void)
{
        GConfClient *conf_client;

        if (!inited_ok)
                return;

        conf_client = gconf_client_get_default ();
        gkbd_keyboard_config_init (&initial_sys_kbd_config,
                                   conf_client,
                                   xkl_engine);
        gkbd_keyboard_config_load_from_x_initial (&initial_sys_kbd_config,
                                                  NULL);
        g_object_unref (conf_client);
}

static gboolean
gsd_chk_file_list (void)
{
        GDir        *home_dir;
        const char  *fname;
        GSList      *file_list = NULL;
        GSList      *last_login_file_list = NULL;
        GSList      *tmp = NULL;
        GSList      *tmp_l = NULL;
        gboolean     new_file_exist = FALSE;
        GConfClient *conf_client;

        home_dir = g_dir_open (g_get_home_dir (), 0, NULL);
        while ((fname = g_dir_read_name (home_dir)) != NULL) {
                if (g_strrstr (fname, "modmap")) {
                        file_list = g_slist_append (file_list, g_strdup (fname));
                }
        }
        g_dir_close (home_dir);

        conf_client = gconf_client_get_default ();

        last_login_file_list = gconf_client_get_list (conf_client,
                                                      KNOWN_FILES_KEY,
                                                      GCONF_VALUE_STRING,
                                                      NULL);

        /* Compare between the two file list, currently available modmap files
           and the files available in the last log in */
        tmp = file_list;
        while (tmp != NULL) {
                tmp_l = last_login_file_list;
                new_file_exist = TRUE;
                while (tmp_l != NULL) {
                        if (strcmp (tmp->data, tmp_l->data) == 0) {
                                new_file_exist = FALSE;
                                break;
                        } else {
                                tmp_l = tmp_l->next;
                        }
                }
                if (new_file_exist) {
                        break;
                } else {
                        tmp = tmp->next;
                }
        }

        if (new_file_exist) {
                gconf_client_set_list (conf_client,
                                       KNOWN_FILES_KEY,
                                       GCONF_VALUE_STRING,
                                       file_list,
                                       NULL);
        }

        g_object_unref (conf_client);

        g_slist_foreach (file_list, (GFunc) g_free, NULL);
        g_slist_free (file_list);

        g_slist_foreach (last_login_file_list, (GFunc) g_free, NULL);
        g_slist_free (last_login_file_list);

        return new_file_exist;

}

static void
gsd_keyboard_xkb_chk_lcl_xmm (void)
{
        if (gsd_chk_file_list ()) {
                gsd_modmap_dialog_call ();
        }
        gsd_load_modmap_files ();
}

void
gsd_keyboard_xkb_set_post_activation_callback (PostActivationCallback fun,
                                               void *user_data)
{
        pa_callback = fun;
        pa_callback_user_data = user_data;
}

static GdkFilterReturn
gsd_keyboard_xkb_evt_filter (GdkXEvent * xev,
                             GdkEvent  * event)
{
        XEvent *xevent = (XEvent *) xev;
        xkl_engine_filter_events (xkl_engine, xevent);
        return GDK_FILTER_CONTINUE;
}

static guint
register_config_callback (GConfClient          *client,
                          const char           *path,
                          GConfClientNotifyFunc func)
{
        gconf_client_add_dir (client, path, GCONF_CLIENT_PRELOAD_NONE, NULL);
        return gconf_client_notify_add (client, path, func, NULL, NULL, NULL);
}

void
gsd_keyboard_xkb_init (GConfClient *client)
{
#ifdef GSDKX
        xkl_set_debug_level (200);
        logfile = fopen ("/tmp/gsdkx.log", "a");
        xkl_set_log_appender (gsd_keyboard_log_appender);
#endif
        xkl_engine = xkl_engine_get_instance (GDK_DISPLAY ());
        if (xkl_engine) {
                inited_ok = TRUE;

                gdm_keyboard_layout = g_getenv ("GDM_KEYBOARD_LAYOUT");

                gkbd_desktop_config_init (&current_config,
                                          client,
                                          xkl_engine);
                gkbd_keyboard_config_init (&current_kbd_config,
                                           client,
                                           xkl_engine);
                xkl_engine_backup_names_prop (xkl_engine);
                gsd_keyboard_xkb_analyze_sysconfig ();
                gsd_keyboard_xkb_chk_lcl_xmm ();

                notify_desktop =
                        register_config_callback (client,
                                                  GKBD_DESKTOP_CONFIG_DIR,
                                                  (GConfClientNotifyFunc) apply_settings);

                notify_keyboard =
                        register_config_callback (client,
                                                  GKBD_KEYBOARD_CONFIG_DIR,
                                                  (GConfClientNotifyFunc) apply_xkb_settings);

                gdk_window_add_filter (NULL, (GdkFilterFunc)
                                       gsd_keyboard_xkb_evt_filter,
                                       NULL);
                xkl_engine_start_listen (xkl_engine,
                                         XKLL_MANAGE_LAYOUTS |
                                         XKLL_MANAGE_WINDOW_STATES);

                apply_settings ();
                apply_xkb_settings ();
        }
}

void
gsd_keyboard_xkb_shutdown (void)
{
        GConfClient *client;

        pa_callback = NULL;
        pa_callback_user_data = NULL;

        if (!inited_ok)
                return;

        xkl_engine_stop_listen (xkl_engine);

        gdk_window_remove_filter (NULL,
                                  (GdkFilterFunc) gsd_keyboard_xkb_evt_filter,
                                  NULL);

        client = gconf_client_get_default ();

        if (notify_desktop != 0) {
                gconf_client_remove_dir (client, GKBD_DESKTOP_CONFIG_DIR, NULL);
                gconf_client_notify_remove (client, notify_desktop);
                notify_desktop = 0;
        }

        if (notify_keyboard != 0) {
                gconf_client_remove_dir (client, GKBD_KEYBOARD_CONFIG_DIR, NULL);
                gconf_client_notify_remove (client, notify_keyboard);
                notify_keyboard = 0;
        }

        g_object_unref (client);
        g_object_unref (xkl_engine);

        xkl_engine = NULL;
        inited_ok = FALSE;
}