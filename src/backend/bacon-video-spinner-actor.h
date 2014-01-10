/*
 * Overlaid spinner
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
 * Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef BACON_VIDEO_SPINNER_ACTOR_H
#define BACON_VIDEO_SPINNER_ACTOR_H

#include <glib-object.h>
#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define OVERLAY_OPACITY 220

#define BACON_TYPE_VIDEO_SPINNER_ACTOR            (bacon_video_spinner_actor_get_type ())
#define BACON_VIDEO_SPINNER_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  BACON_TYPE_VIDEO_SPINNER_ACTOR, BaconVideoSpinnerActor))
#define BACON_VIDEO_SPINNER_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   BACON_TYPE_VIDEO_SPINNER_ACTOR, BaconVideoSpinnerActorClass))
#define BACON_IS_VIDEO_SPINNER_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  BACON_TYPE_VIDEO_SPINNER_ACTOR))
#define BACON_IS_VIDEO_SPINNER_ACTOR_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), BACON_TYPE_VIDEO_SPINNER_ACTOR))
#define BACON_VIDEO_SPINNER_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BACON_TYPE_VIDEO_SPINNER_ACTOR, BaconVideoSpinnerActorClass))

typedef struct BaconVideoSpinnerActor                   BaconVideoSpinnerActor;
typedef struct BaconVideoSpinnerActorClass              BaconVideoSpinnerActorClass;
typedef struct BaconVideoSpinnerActorPrivate            BaconVideoSpinnerActorPrivate;

struct BaconVideoSpinnerActor {
        ClutterActor                    parent;

        BaconVideoSpinnerActorPrivate  *priv;
};

struct BaconVideoSpinnerActorClass {
        ClutterActorClass parent_class;
};

GType                 bacon_video_spinner_actor_get_type          (void);

ClutterActor *        bacon_video_spinner_actor_new               (void);

G_END_DECLS

#endif
