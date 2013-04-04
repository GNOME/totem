/*
 * Copyright © 2013 Red Hat, Inc.
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
 * Author: Joaquim Rocha <me@joaquimrocha.com>
 */

#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <glib.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define CLOCK_TYPE            (clock_get_type ())
#define CLOCK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLOCK_TYPE, Clock))
#define CLOCK_IS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLOCK_TYPE))
#define CLOCK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLOCK_TYPE, ClockClass))
#define CLOCK_IS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLOCK_TYPE))
#define CLOCK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLOCK_TYPE, ClockClass))

typedef struct _Clock      Clock;
typedef struct _ClockClass ClockClass;

struct _Clock
{
  ClutterActor parent_instance;

  /*< private >*/
  gfloat angle;
};

struct _ClockClass
{
  ClutterActorClass parent_class;
};

ClutterActor * clock_new (void);

GType clock_get_type (void);

#endif /* __CLOCK_H__ */
