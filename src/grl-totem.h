/*
 * Copyright (C) 2013 Bastien Nocera
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef _GRL_TOTEM_SOURCE_H_
#define _GRL_TOTEM_SOURCE_H_

#include <grilo.h>

#define GRL_TOTEM_SOURCE_TYPE            (grl_totem_source_get_type ())
#define GRL_TOTEM_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GRL_TOTEM_SOURCE_TYPE,  GrlTotemSource))
#define GRL_IS_TOTEM_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GRL_TOTEM_SOURCE_TYPE))
#define GRL_TOTEM_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GRL_TOTEM_SOURCE_TYPE, GrlTotemSourceClass))
#define GRL_IS_TOTEM_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) GRL_TOTEM_SOURCE_TYPE))
#define GRL_TOTEM_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GRL_TOTEM_SOURCE_TYPE, GrlTotemSourceClass))

typedef struct _GrlTotemSource GrlTotemSource;
typedef struct _GrlTotemSourcePrivate GrlTotemSourcePrivate;

struct _GrlTotemSource {

	GrlSource parent;

	/*< private >*/
	GrlTotemSourcePrivate *priv;
};

typedef struct _GrlTotemSourceClass GrlTotemSourceClass;

struct _GrlTotemSourceClass {

	GrlSourceClass parent_class;

};

GrlPluginDescriptor *grl_totem_plugin_get_descriptor (void);
gboolean             grl_totem_plugin_wraps_source   (GrlSource *source);
GType                grl_totem_source_get_type       (void);

#endif /* _GRL_TOTEM_SOURCE_H_ */
