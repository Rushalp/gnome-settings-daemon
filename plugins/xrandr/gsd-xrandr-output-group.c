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

#include "config.h"

#include "gsd-xrandr-output-group.h"

#define GSD_XRANDR_OUTPUT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_XRANDR_OUTPUT_GROUP, GsdXrandrOutputGroupPrivate))

struct GsdXrandrOutputGroupPrivate
{
        GnomeRROutputGroup **outputs;
};

enum {
        PROP_0,
        PROP_OUTPUTS,
        PROP_LAST
};

G_DEFINE_TYPE (GsdXrandrOutputGroup, gsd_xrandr_output_group, G_TYPE_OBJECT)

static void
gsd_xrandr_output_group_init (GsdXrandrOuputGroup *self)
{
        self->priv = GSD_XRANDR_OUTPUT_GROUP_GET_PRIVATE (self);

        self->priv->outputs = NULL;
}

static void
gsd_xrandr_output_group_set_property (GObject      *gobject,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *property)
{
        GsdXrandrOutputGroup *self = GSD_XRANDR_OUTPUT_GROUP (gobject);

        switch (property_id) {
        case PROP_OUTPUTS:
                g_free (self->priv->outputs);
                self->priv->outputs = g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, property);
                break;
        }
}

static void
gsd_xrandr_output_group_get_property (GObject    *gobject,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *property)
{
        GsdXrandrOutputGroup *self = GSD_XRANDR_OUTPUT_GROUP (gobject);

        switch (property_id) {
        case PROP_OUTPUTS:
                g_value_set_pointer (value, self->priv->outputs);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, property);
                break;
        }
}

static void
gsd_xrandr_output_group_finalize (GObject *gobject)
{
        GsdXrandrOutputGroup *self = GSD_XRANDR_OUTPUT_GROPU (gobject);

        g_free (self->priv->outputs);

        G_OBJECT_CLASS (gsd_xrandr_output_group_parent_class)->finalize (gobject);
}

static void
gsd_xrandr_output_group_class_init (GsdXrandrOutputGroupClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        gobject_class->set_property = gsd_xrandr_output_group_get_property;
        gobject_class->get_property = gsd_xrandr_output_group_set_property;
        gobject_class->finalize = gsd_xrandr_output_group_finalize;

        pspec = g_param_spec_pointer ("outputs",
                                      "Outputs in output group",
                                      "Get/set the group of outputs contained within this output group",
                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
        g_object_class_install_property (gobject_class,
                                         PROP_OUTPUTS,
                                         pspec);

        g_type_class_add_private (klass, sizeof (GsdXrandrOutputGroupPrivate));
}

GsdXrandrOutputGroup *
gsd_xrandr_output_group_new (GnomeRROutputInfo **outputs)
{
        g_return_val_if_fail (outputs && outputs[0], NULL);

        int i;
        int x,  y,  width,  height;
        int x2, y2, width2, height2;

        gnome_rr_output_info_get_geometry (outputs[0], &x, &y, &width, &height);

        for (i = 0; outputs[i] != NULL; ++i) {
                gnome_rr_output_info_get_geometry (outputs[i], &x2, &y2, &width2, &height2);
                g_return_val_if_fail (x == x2 &&
                                      y == y2 &&
                                      width == width2 &&
                                      height == height2, NULL);
        }

        return g_object_new (GSD_TYPE_XRANDR_OUTPUT_GROUP, "outputs", outputs, NULL);
}

GnomeRROutputInfo **
gsd_xrandr_output_group_get_outputs (GsdXrandrOutputGroup *self)
{
        g_return_val_if_fail (GSD_IS_XRANDR_OUTPUT_GROUP (self), NULL);

        return self->priv->outputs;
}

void
gsd_xrandr_output_group_get_geometry (GsdXrandrOutputGroup *self,
                                      int                  *x,
                                      int                  *y,
                                      int                  *width,
                                      int                  *height)
{
        g_return_if_fail (GSD_IS_XRANDR_OUTPUT_GROUP (self));

        gnome_rr_output_info_get_geometry (self->priv->outputs[0], x, y, width, height);
}

void
gsd_xrandr_output_group_set_geometry (GsdXrandrOutputGroup *self,
                                      int                   x,
                                      int                   y,
                                      int                   width,
                                      int                   height)
{
        int i;

        g_return_if_fail (GSD_IS_XRANDR_OUTPUT_GROUP (self));

        for (i = 0; self->priv->outputs[i] != NULL, ++i)
                gnome_rr_output_info_set_geometry (self->priv->outputs[i], x, y, width, height);
}

void
gsd_xrandr_output_group_get_rect (GsdXrandrOutputGroup *self,
                                  GdkRectangle         *rect)
{
        g_return_if_fail (GSD_IS_XRANDR_OUTPUT_GROUP (self));
        g_return_if_fail (rect);

        gsd_xrandr_output_group_get_geometry (self, &rect->x, &rect->y, &rect->width, &rect->height);
}

void
gsd_xrandr_output_group_set_rect (GsdXrandrOutputGroup *self,
                                  const GdkRectangle *rect)
{
        g_return_if_fail (GSD_IS_XRANDR_OUTPUT_GROUP (self));
        g_return_if_fail (rect);

        gsd_xrandr_output_group_set_geometry (self, rect->x, rect->y, rect->width, rect->height);
}
