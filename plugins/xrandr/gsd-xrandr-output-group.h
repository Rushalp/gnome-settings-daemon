/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Evan Broder <evan@ebroder.net>
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

#ifndef __GSD_XRANDR_OUTPUT_GROUP_H
#define __GSD_XRANDR_OUTPUT_GROUP_H

#include <glib-object.h>
#include <gdk/gdk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr-config.h>
#undef GNOME_DESKTOP_USE_UNSTABLE_API

G_BEGIN_DECLS

#define GSD_TYPE_XRANDR_OUTPUT_GROUP         (gsd_xrandr_output_group_get_type ())
#define GSD_XRANDR_OUTPUT_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_XRANDR_OUTPUT_GROUP, GsdXrandrOutputGroup))
#define GSD_XRANDR_OUTPUT_GROUP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSD_TYPE_XRANDR_OUTPUT_GROUP, GsdXrandrOutputGroupClass))
#define GSD_IS_XRANDR_OUTPUT_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_XRANDR_OUTPUT_GROUP))
#define GSD_IS_XRANDR_OUTPUT_GROUP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_XRANDR_OUTPUT_GROUP))
#define GSD_XRANDR_OUTPUT_GROUP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_XRANDR_OUTPUT_GROUP, GsdXrandrOutputGroupClass))

typedef struct GsdXrandrOutputGroupPrivate GsdXrandrOutputGroupPrivate;

typedef struct
{
        GObject                     parent_instance;
        GsdXrandrOutputGroupPrivate *priv;
} GsdXrandrOutputGroup;

typedef struct
{
        GObjectClass   parent_class;
} GsdXrandrOutputGroupClass;

GType                   gsd_xrandr_output_group_get_type            (void);

GsdXrandrOutputGroup *  gsd_xrandr_output_group_new                 (GnomeRROutputInfo **outputs);

GnomeRROutputInfo **    gsd_xrandr_output_group_get_outputs         (GsdXrandrOutputGroup *self);

void gsd_xrandr_output_group_get_geometry (GsdXrandrOutputGroup *self, int *x, int *y, int *width, int *height);
void gsd_xrandr_output_group_set_geometry (GsdXrandrOutputGroup *self, int  x, int  y, int  width, int  height);

void gsd_xrandr_output_gorup_get_rect (GsdXrandrOutputGroup *self, GdkRectangle *rect);
void gsd_xrandr_output_gorup_set_rect (GsdXrandrOutputGroup *self, const GdkRectangle *rect);

G_END_DECLS

#endif /* __GSD_XRANDR_OUTPUT_GROUP_H */
