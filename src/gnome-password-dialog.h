/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-password-dialog.h - A use password prompting dialog widget.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef GNOME_PASSWORD_DIALOG_H
#define GNOME_PASSWORD_DIALOG_H

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PASSWORD_DIALOG            (gnome_password_dialog_get_type ())
#define GNOME_PASSWORD_DIALOG(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_PASSWORD_DIALOG, GnomePasswordDialog))
#define GNOME_PASSWORD_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_PASSWORD_DIALOG, GnomePasswordDialogClass))
#define GNOME_IS_PASSWORD_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_PASSWORD_DIALOG))
#define GNOME_IS_PASSWORD_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_PASSWORD_DIALOG))

typedef struct GnomePasswordDialog        GnomePasswordDialog;
typedef struct GnomePasswordDialogClass   GnomePasswordDialogClass;
typedef struct GnomePasswordDialogDetails GnomePasswordDialogDetails;

struct GnomePasswordDialog
{
	GtkDialog gtk_dialog;

	GnomePasswordDialogDetails *details;
};

struct GnomePasswordDialogClass
{
	GtkDialogClass parent_class;
};

GtkType    gnome_password_dialog_get_type                (void);
GtkWidget* gnome_password_dialog_new                     (const char        *dialog_title,
							const char        *message,
							const char        *username,
							const char        *password,
							gboolean           readonly_username);
gboolean   gnome_password_dialog_run_and_block           (GnomePasswordDialog *password_dialog);

/* Attribute mutators */
void       gnome_password_dialog_set_username            (GnomePasswordDialog *password_dialog,
							const char        *username);
void       gnome_password_dialog_set_password            (GnomePasswordDialog *password_dialog,
							const char        *password);
void       gnome_password_dialog_set_readonly_username   (GnomePasswordDialog *password_dialog,
							gboolean           readonly);
void       gnome_password_dialog_set_remember            (GnomePasswordDialog *password_dialog,
							gboolean           remember);
void       gnome_password_dialog_set_remember_label_text (GnomePasswordDialog *password_dialog,
							const char        *remember_label_text);

/* Attribute accessors */
char *     gnome_password_dialog_get_username            (GnomePasswordDialog *password_dialog);
char *     gnome_password_dialog_get_password            (GnomePasswordDialog *password_dialog);
gboolean   gnome_password_dialog_get_remember            (GnomePasswordDialog *password_dialog);

G_END_DECLS

#endif /* GNOME_PASSWORD_DIALOG_H */
