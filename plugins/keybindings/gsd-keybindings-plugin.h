/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GSD_KEYBINDINGS_PLUGIN_H__
#define __GSD_KEYBINDINGS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "gnome-settings-plugin.h"

G_BEGIN_DECLS

#define GSD_TYPE_KEYBINDINGS_PLUGIN                (gsd_keybindings_plugin_get_type ())
#define GSD_KEYBINDINGS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_KEYBINDINGS_PLUGIN, GsdKeybindingsPlugin))
#define GSD_KEYBINDINGS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_KEYBINDINGS_PLUGIN, GsdKeybindingsPluginClass))
#define GSD_IS_KEYBINDINGS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_KEYBINDINGS_PLUGIN))
#define GSD_IS_KEYBINDINGS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_KEYBINDINGS_PLUGIN))
#define GSD_KEYBINDINGS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_KEYBINDINGS_PLUGIN, GsdKeybindingsPluginClass))

typedef struct GsdKeybindingsPluginPrivate GsdKeybindingsPluginPrivate;

typedef struct
{
        GnomeSettingsPlugin    parent;
        GsdKeybindingsPluginPrivate *priv;
} GsdKeybindingsPlugin;

typedef struct
{
        GnomeSettingsPluginClass parent_class;
} GsdKeybindingsPluginClass;

GType   gsd_keybindings_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_gnome_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __GSD_KEYBINDINGS_PLUGIN_H__ */
