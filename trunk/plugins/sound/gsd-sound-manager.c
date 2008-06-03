/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
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
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#ifdef HAVE_ESD
# include <esd.h>
#endif

#include <libgnome/gnome-sound.h>
#include <libgnome/gnome-exec.h>
#include <libgnomeui/gnome-client.h>
#include "libsounds/sound-properties.h"

#include "gsd-sound-manager.h"
#include "gnome-settings-profile.h"

#define GSD_SOUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_SOUND_MANAGER, GsdSoundManagerPrivate))

struct GsdSoundManagerPrivate
{
        /* esd/PulseAudio pid */
        GPid  pid;
        guint child_watch_id;

        gboolean inited;
        guint notify;
};

#define GCONF_SOUND_DIR         "/desktop/gnome/sound"

static void     gsd_sound_manager_class_init  (GsdSoundManagerClass *klass);
static void     gsd_sound_manager_init        (GsdSoundManager      *sound_manager);
static void     gsd_sound_manager_finalize    (GObject             *object);

G_DEFINE_TYPE (GsdSoundManager, gsd_sound_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
reset_esd_pid (GPid             pid,
               int              status,
               GsdSoundManager *manager)
{
        g_debug ("GsdSoundManager: **** child (pid:%d) done (%s:%d)",
                 (int) pid,
                 WIFEXITED (status) ? "status"
                 : WIFSIGNALED (status) ? "signal"
                 : "unknown",
                 WIFEXITED (status) ? WEXITSTATUS (status)
                 : WIFSIGNALED (status) ? WTERMSIG (status)
                 : -1);

        if (pid == manager->priv->pid) {
                manager->priv->pid = 0;
        }

        g_spawn_close_pid (manager->priv->pid);
        manager->priv->child_watch_id = 0;
}

static gboolean
start_gnome_sound (GsdSoundManager *manager)
{
        char    *argv[] = { "esd", "-nobeeps", NULL};
        GError  *error;
        gboolean res;
        time_t   starttime;
        gboolean ret;

        ret = FALSE;

        gnome_settings_profile_start (NULL);

        error = NULL;
        res = g_spawn_async (NULL,
                             argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                             NULL,
                             NULL,
                             &manager->priv->pid,
                             &error);
        if (! res) {
                g_printerr ("Could not start esd: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        manager->priv->child_watch_id = g_child_watch_add (manager->priv->pid,
                                                           (GChildWatchFunc)reset_esd_pid,
                                                           manager);

        starttime = time (NULL);
        gnome_sound_init (NULL);

        while (gnome_sound_connection_get () < 0
               && ((time (NULL) - starttime) < 4)) {
                g_usleep (200);
                gnome_sound_init (NULL);
        }

        ret = gnome_sound_connection_get () >= 0;

 out:
        gnome_settings_profile_end (NULL);

        return ret;
}

static int
wait_on_child (int pid)
{
        int status;

 wait_again:
        if (waitpid (pid, &status, 0) < 0) {
                if (errno == EINTR) {
                        goto wait_again;
                } else if (errno == ECHILD) {
                        ; /* do nothing, child already reaped */
                } else {
                        g_debug ("GsdSoundManager: waitpid () should not fail");
                }
        }

        return status;
}

static void
stop_child (GsdSoundManager *manager)
{
        int exit_status;

        if (manager->priv->pid <= 0) {
                return;
        }

        if (manager->priv->child_watch_id > 0) {
                g_source_remove (manager->priv->child_watch_id);
                manager->priv->child_watch_id = 0;
        }

        if (kill (manager->priv->pid, SIGTERM) == -1) {
                g_printerr ("Failed to kill esd (pid %d)\n", manager->priv->pid);
                return;
        }

        exit_status = wait_on_child (manager->priv->pid);

        g_spawn_close_pid (manager->priv->pid);
        manager->priv->pid = -1;
}

#ifdef HAVE_ESD
static gboolean set_esd_standby = TRUE;
#endif

static void
stop_gnome_sound (GsdSoundManager *manager)
{
#ifdef HAVE_ESD
        /* Can't think of a way to do this reliably, so we fake it for now */
        esd_standby (gnome_sound_connection_get ());
        set_esd_standby = TRUE;
#else
        gnome_sound_shutdown ();

        stop_child (manager);
#endif
}

struct reload_foreach_closure {
        gboolean enable_system_sounds;
};

/* reload_foreach_cb
 *
 * For a given SoundEvent, reload the sound file associate with the event.
 */
static void
reload_foreach_cb (SoundEvent *event, gpointer data)
{
        struct reload_foreach_closure *closure;
        char *key, *file;
        int sid;
        gboolean do_load;

        closure = data;

        gnome_settings_profile_start (NULL);

        key = sound_event_compose_key (event);

#ifdef HAVE_ESD
        /* We need to free up the old sample, because
         * esd allows multiple samples with the same name,
         * putting memory to waste. */
        sid = esd_sample_getid (gnome_sound_connection_get (), key);
        if (sid >= 0) {
                esd_sample_free (gnome_sound_connection_get (), sid);
        }
#endif
        /* We only disable sounds for system events.  Other events, like sounds
         * in games, should be preserved.  The games should have their own
         * configuration for sound anyway.
         */
        if ((strcmp (event->category, "gnome-2") == 0
             || strcmp (event->category, "gtk-events-2") == 0)) {
                do_load = closure->enable_system_sounds;
        } else {
                do_load = TRUE;
        }

        if (!do_load) {
                goto out;
        }

        if (!event->file || !strcmp (event->file, "")) {
                goto out;
        }

        if (event->file[0] == '/') {
                file = g_strdup (event->file);
        } else {
                file = gnome_program_locate_file (NULL,
                                                  GNOME_FILE_DOMAIN_SOUND,
                                                  event->file, TRUE, NULL);
        }

        if (!file) {
                goto out;
        }

        sid = gnome_sound_sample_load (key, file);

        if (sid < 0) {
                g_warning (_("Couldn't load sound file %s as sample %s"),
                           file, key);
        }

        g_free (file);

 out:
        gnome_settings_profile_end (NULL);

        g_free (key);
}

static void
apply_settings (GsdSoundManager *manager)
{
        GConfClient    *client;
        gboolean        enable_sound;
        gboolean        event_sounds;
        struct reload_foreach_closure closure;

        gnome_settings_profile_start (NULL);

        client = gconf_client_get_default ();

        enable_sound = gconf_client_get_bool (client, "/desktop/gnome/sound/enable_esd", NULL);
        event_sounds = gconf_client_get_bool (client, "/desktop/gnome/sound/event_sounds", NULL);

        closure.enable_system_sounds = event_sounds;

        if (enable_sound) {
                if (gnome_sound_connection_get () < 0) {
                        if (!start_gnome_sound (manager)) {
                                goto out;
                        }
                }
#ifdef HAVE_ESD
                else if (set_esd_standby) {
                        esd_resume (gnome_sound_connection_get ());
                        set_esd_standby = FALSE;
                }
#endif
        } else {
#ifdef HAVE_ESD
                if (! set_esd_standby)
#endif
                        stop_gnome_sound (manager);
        }

        if (enable_sound && !manager->priv->inited) {
                SoundProperties *props;

                manager->priv->inited = TRUE;
                props = sound_properties_new ();
                sound_properties_add_defaults (props, NULL);
                sound_properties_foreach (props, reload_foreach_cb, &closure);
                gtk_object_destroy (GTK_OBJECT (props));
        }
 out:
        gnome_settings_profile_end (NULL);

}

static guint
register_config_callback (GsdSoundManager         *manager,
                          const char              *path,
                          GConfClientNotifyFunc    func)
{
        GConfClient *client;
        guint notify;

        client = gconf_client_get_default ();

        gconf_client_add_dir (client, path, GCONF_CLIENT_PRELOAD_NONE, NULL);
        notify = gconf_client_notify_add (client, path, func, manager, NULL, NULL);

        g_object_unref (client);
        return notify;
}

static void
sound_callback (GConfClient        *client,
                guint               cnxn_id,
                GConfEntry         *entry,
                GsdSoundManager    *manager)
{
        apply_settings (manager);
}

gboolean
gsd_sound_manager_start (GsdSoundManager *manager,
                         GError         **error)
{
        g_debug ("Starting sound manager");

        gnome_settings_profile_start (NULL);

        manager->priv->notify =
                register_config_callback (manager,
                                          GCONF_SOUND_DIR,
                                          (GConfClientNotifyFunc) sound_callback);
        apply_settings (manager);

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_sound_manager_stop (GsdSoundManager *manager)
{
        GsdSoundManagerPrivate *p = manager->priv;

        g_debug ("Stopping sound manager");

        if (p->notify != 0) {
                GConfClient *client = gconf_client_get_default ();
                gconf_client_remove_dir (client, GCONF_SOUND_DIR, NULL);
                gconf_client_notify_remove (client, p->notify);
                g_object_unref (client);
                p->notify = 0;
        }

        stop_gnome_sound (manager);
        p->inited = FALSE;
}

static void
gsd_sound_manager_set_property (GObject        *object,
                                guint           prop_id,
                                const GValue   *value,
                                GParamSpec     *pspec)
{
        GsdSoundManager *self;

        self = GSD_SOUND_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_sound_manager_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        GsdSoundManager *self;

        self = GSD_SOUND_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gsd_sound_manager_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        GsdSoundManager      *sound_manager;
        GsdSoundManagerClass *klass;

        klass = GSD_SOUND_MANAGER_CLASS (g_type_class_peek (GSD_TYPE_SOUND_MANAGER));

        sound_manager = GSD_SOUND_MANAGER (G_OBJECT_CLASS (gsd_sound_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (sound_manager);
}

static void
gsd_sound_manager_dispose (GObject *object)
{
        GsdSoundManager *manager;

        manager = GSD_SOUND_MANAGER (object);

        stop_child (manager);

        G_OBJECT_CLASS (gsd_sound_manager_parent_class)->dispose (object);
}

static void
gsd_sound_manager_class_init (GsdSoundManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsd_sound_manager_get_property;
        object_class->set_property = gsd_sound_manager_set_property;
        object_class->constructor = gsd_sound_manager_constructor;
        object_class->dispose = gsd_sound_manager_dispose;
        object_class->finalize = gsd_sound_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdSoundManagerPrivate));
}

static void
gsd_sound_manager_init (GsdSoundManager *manager)
{
        manager->priv = GSD_SOUND_MANAGER_GET_PRIVATE (manager);

}

static void
gsd_sound_manager_finalize (GObject *object)
{
        GsdSoundManager *sound_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_SOUND_MANAGER (object));

        sound_manager = GSD_SOUND_MANAGER (object);

        g_return_if_fail (sound_manager->priv != NULL);

        G_OBJECT_CLASS (gsd_sound_manager_parent_class)->finalize (object);
}

GsdSoundManager *
gsd_sound_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_SOUND_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_SOUND_MANAGER (manager_object);
}