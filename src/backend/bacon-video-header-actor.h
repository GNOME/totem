/*
 * Overlaid header
 *
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef BACON_VIDEO_HEADER_ACTOR_H
#define BACON_VIDEO_HEADER_ACTOR_H

#include <glib-object.h>
#include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OVERLAY_OPACITY 220

#define BACON_TYPE_VIDEO_HEADER_ACTOR            (bacon_video_header_actor_get_type ())
#define BACON_VIDEO_HEADER_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  BACON_TYPE_VIDEO_HEADER_ACTOR, BaconVideoHeaderActor))
#define BACON_VIDEO_HEADER_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   BACON_TYPE_VIDEO_HEADER_ACTOR, BaconVideoHeaderActorClass))
#define BACON_IS_VIDEO_HEADER_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  BACON_TYPE_VIDEO_HEADER_ACTOR))
#define BACON_IS_VIDEO_HEADER_ACTOR_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), BACON_TYPE_VIDEO_HEADER_ACTOR))
#define BACON_VIDEO_HEADER_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BACON_TYPE_VIDEO_HEADER_ACTOR, BaconVideoHeaderActorClass))

typedef struct BaconVideoHeaderActor                   BaconVideoHeaderActor;
typedef struct BaconVideoHeaderActorClass              BaconVideoHeaderActorClass;
typedef struct BaconVideoHeaderActorPrivate            BaconVideoHeaderActorPrivate;

struct BaconVideoHeaderActor {
        GtkClutterActor                  parent;

        BaconVideoHeaderActorPrivate  *priv;
};

struct BaconVideoHeaderActorClass {
        GtkClutterActorClass parent_class;
};

GType                 bacon_video_header_actor_get_type          (void);

ClutterActor *        bacon_video_header_actor_new               (void);

G_END_DECLS

#endif
