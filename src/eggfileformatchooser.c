#include "eggfileformatchooser.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>

typedef struct _EggFileFormatSearch EggFileFormatSearch;

enum 
{
  MODEL_COLUMN_ID,
  MODEL_COLUMN_NAME,
  MODEL_COLUMN_ICON,
  MODEL_COLUMN_EXTENSIONS,
  MODEL_COLUMN_DATA,
  MODEL_COLUMN_DESTROY
};

enum 
{
  SIGNAL_SELECTION_CHANGED,
  SIGNAL_LAST
};

struct _EggFileFormatChooserPrivate
{
  GtkTreeStore *model;
  GtkTreeSelection *selection;
  guint idle_hack;
  guint last_id;
};

struct _EggFileFormatSearch
{
  gboolean success;
  GtkTreeIter iter;

  guint format;
  const gchar *extension;
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (EggFileFormatChooser, 
	       egg_file_format_chooser,
               GTK_TYPE_EXPANDER);

static void
selection_changed (GtkTreeSelection     *selection,
                   EggFileFormatChooser *self)
{
  gchar *label;
  gchar *name;

  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) 
    {
      gtk_tree_model_get (model, &iter, MODEL_COLUMN_NAME, &name, -1);

      label = g_strdup_printf (_("File Format: %s"), name);
      gtk_expander_set_label (GTK_EXPANDER (self), label);

      g_free (name);
      g_free (label);

      g_signal_emit (self, signals[SIGNAL_SELECTION_CHANGED], 0);
    }
}

/* XXX This hack is needed, as gtk_expander_set_label seems 
 * not to work from egg_file_format_chooser_init */
static gboolean
select_default_file_format (gpointer data)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (data);
  egg_file_format_chooser_set_format (self, 0);
  self->priv->idle_hack = 0;
  return FALSE;
}

static gboolean
find_by_format (GtkTreeModel *model,
                GtkTreePath  *path G_GNUC_UNUSED,
                GtkTreeIter  *iter,
                gpointer      data)
{
  EggFileFormatSearch *search = data;
  guint id;

  gtk_tree_model_get (model, iter, MODEL_COLUMN_ID, &id, -1);

  if (id == search->format)
    {
      search->success = TRUE;
      search->iter = *iter;
    }

  return search->success;
}

static gboolean
find_in_list (gchar       *list,
              const gchar *needle)
{
  gchar *saveptr;
  gchar *token;

  for(token = strtok_r (list, ",", &saveptr); NULL != token;
      token = strtok_r (NULL, ",", &saveptr))
    {
      token = g_strstrip (token);

      if (g_str_equal (needle, token))
        return TRUE;
    }

  return FALSE;
}

static gboolean
find_by_extension (GtkTreeModel *model,
                   GtkTreePath  *path G_GNUC_UNUSED,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  EggFileFormatSearch *search = data;

  gchar *extensions = NULL;
  guint format = 0;

  gtk_tree_model_get (model, iter,
                      MODEL_COLUMN_EXTENSIONS, &extensions,
                      MODEL_COLUMN_ID, &format,
                      -1);

  if (extensions && find_in_list (extensions, search->extension))
    {
      search->format = format;
      search->success = TRUE;
      search->iter = *iter;
    }

  g_free (extensions);
  return search->success;
}

static void
egg_file_format_chooser_init (EggFileFormatChooser *self)
{
  GtkWidget *scroller;
  GtkWidget *view;

  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkTreeIter iter;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EGG_TYPE_FILE_FORMAT_CHOOSER, 
                                            EggFileFormatChooserPrivate);

/* tree model */

  self->priv->model = gtk_tree_store_new (6, G_TYPE_UINT, G_TYPE_STRING,
                                             G_TYPE_STRING, G_TYPE_STRING,
                                             G_TYPE_POINTER, G_TYPE_POINTER);

  gtk_tree_store_append (self->priv->model, &iter, NULL);
  gtk_tree_store_set (self->priv->model, &iter,
                      MODEL_COLUMN_NAME, _("By Extension"),
                      MODEL_COLUMN_ID, 0,
                      -1);

/* tree view */

  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->priv->model));
  self->priv->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);

/* file format column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_title (column, _("File Format"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "icon-name", MODEL_COLUMN_ICON,
                                       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "text", MODEL_COLUMN_NAME,
                                       NULL);

/* extensions column */

  column = gtk_tree_view_column_new_with_attributes (
    _("Extension(s)"), gtk_cell_renderer_text_new (),
    "text", MODEL_COLUMN_EXTENSIONS, NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

/* selection */

  gtk_tree_selection_set_mode (self->priv->selection, GTK_SELECTION_BROWSE);
  g_signal_connect (self->priv->selection, "changed", G_CALLBACK (selection_changed), self);
  self->priv->idle_hack = g_idle_add (select_default_file_format, self);

/* scroller */

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_widget_set_size_request (scroller, -1, 150);
  gtk_container_add (GTK_CONTAINER (scroller), view);
  gtk_widget_show_all (scroller);

  gtk_container_add (GTK_CONTAINER (self), scroller);
}

static void
reset_model (EggFileFormatChooser *self)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->priv->model);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          GDestroyNotify destroy = NULL;
          gpointer data = NULL;

          gtk_tree_model_get (model, &iter,
                              MODEL_COLUMN_DESTROY, &destroy,
                              MODEL_COLUMN_DATA, &data,
                              -1);

          if (destroy)
            destroy (data);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_tree_store_clear (self->priv->model);
}

static void
egg_file_format_chooser_dispose (GObject *obj)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (obj);

  if (NULL != self)
    {
      if (self->priv->idle_hack)
        {
          g_source_remove (self->priv->idle_hack);
          self->priv->idle_hack = 0;
        }
    }

  G_OBJECT_CLASS (egg_file_format_chooser_parent_class)->dispose (obj);
}

static void
egg_file_format_chooser_finalize (GObject *obj)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (obj);

  if (NULL != self)
    {
      if (self->priv->model)
        {
          reset_model (self);
          g_object_unref (self->priv->model);
          self->priv->model = NULL;
        }
    }

  G_OBJECT_CLASS (egg_file_format_chooser_parent_class)->finalize (obj);
}

static void
egg_file_format_chooser_class_init (EggFileFormatChooserClass *cls)
{
  g_type_class_add_private (cls, sizeof (EggFileFormatChooserPrivate));

  G_OBJECT_CLASS (cls)->dispose = egg_file_format_chooser_dispose;
  G_OBJECT_CLASS (cls)->finalize = egg_file_format_chooser_finalize;

  signals[SIGNAL_SELECTION_CHANGED] = g_signal_new (
    "selection-changed", EGG_TYPE_FILE_FORMAT_CHOOSER, G_SIGNAL_RUN_FIRST,
    G_STRUCT_OFFSET (EggFileFormatChooserClass, selection_changed),
    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GtkWidget*
egg_file_format_chooser_new (void)
{
  return g_object_new (EGG_TYPE_FILE_FORMAT_CHOOSER, NULL);
}

static guint
egg_file_format_chooser_add_format_impl (EggFileFormatChooser *self,
                                         guint                 parent,
                                         const gchar          *name,
                                         const gchar          *icon,
                                         const gchar          *extensions)
{
  EggFileFormatSearch search;
  GtkTreeIter iter;

  search.success = FALSE;
  search.format = parent;

  if (parent > 0)
    gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->model),
                            find_by_format, &search);

  gtk_tree_store_append (self->priv->model, &iter, 
                         search.success ? &search.iter : NULL);

  gtk_tree_store_set (self->priv->model, &iter,
                      MODEL_COLUMN_ID, ++self->priv->last_id,
                      MODEL_COLUMN_EXTENSIONS, extensions,
                      MODEL_COLUMN_NAME, name,
                      MODEL_COLUMN_ICON, icon,
                      -1);

  return self->priv->last_id;
}

guint
egg_file_format_chooser_add_format (EggFileFormatChooser *self,
                                    guint                 parent,
                                    const gchar          *name,
                                    const gchar          *icon,
                                    ...)
{
  GString *buffer = NULL;
  const gchar* extptr;
  va_list extensions;
  guint id;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), 0);
  g_return_val_if_fail (NULL != name, 0);

  va_start (extensions, icon);

  while (NULL != (extptr = va_arg (extensions, const gchar*)))
    {
      if (NULL == buffer)
        buffer = g_string_new (NULL);
      else
        g_string_append (buffer, ", ");

      g_string_append (buffer, extptr);
    }

  va_end (extensions);

  id = egg_file_format_chooser_add_format_impl (self, parent, name, icon,
                                                buffer ? buffer->str : NULL);

  if (buffer)
    g_string_free (buffer, TRUE);

  return id;
}

static gchar*
get_icon_name (const gchar *mime_type)
{
  gchar *name = NULL;
  gchar *s;

  if (mime_type)
    {
      name = g_strconcat ("gnome-mime-", mime_type, NULL);

      for(s = name; *s; ++s)
        {
          if (!isalpha (*s) || !isascii (*s))
            *s = '-';
        }
    }

  if (!name ||
      !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name))
    {
      g_free (name);
      name = g_strdup ("gnome-mime-image");
    }

  return name;
}

void           
egg_file_format_chooser_add_pixbuf_formats (EggFileFormatChooser *self,
                                            guint                 parent G_GNUC_UNUSED,
                                            guint               **formats)
{
  GSList *pixbuf_formats = NULL;
  GSList *iter;
  gint i;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  pixbuf_formats = gdk_pixbuf_get_formats ();

  if (formats)
    *formats = g_new0 (guint, g_slist_length (pixbuf_formats) + 1);

  for(iter = pixbuf_formats, i = 0; iter; iter = iter->next, ++i)
    {
      GdkPixbufFormat *format = iter->data;

      gchar *description, *name, *extensions, *icon;
      gchar **mime_types, **extension_list;
      guint id;

      if (gdk_pixbuf_format_is_disabled (format) ||
         !gdk_pixbuf_format_is_writable (format))
        continue;

      mime_types = gdk_pixbuf_format_get_mime_types (format);
      icon = get_icon_name (mime_types[0]);
      g_strfreev (mime_types);

      extension_list = gdk_pixbuf_format_get_extensions (format);
      extensions = g_strjoinv (", ", extension_list);
      g_strfreev (extension_list);

      description = gdk_pixbuf_format_get_description (format);
      name = gdk_pixbuf_format_get_name (format);

      id = egg_file_format_chooser_add_format_impl (self, parent, description, 
                                                    icon, extensions);

      g_free (description);
      g_free (extensions);
      g_free (icon);

      egg_file_format_chooser_set_format_data (self, id, name, g_free);

      if (formats)
        *formats[i] = id;
    }

  g_slist_free (pixbuf_formats);
}

void
egg_file_format_chooser_remove_format (EggFileFormatChooser *self,
                                       guint                 format)
{
  GDestroyNotify destroy = NULL;
  gpointer data = NULL;

  EggFileFormatSearch search;
  GtkTreeModel *model;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_if_fail (search.success);

  gtk_tree_model_get (model, &search.iter,
                      MODEL_COLUMN_DESTROY, &destroy,
                      MODEL_COLUMN_DATA, &data,
                      -1);

  if (destroy)
    destroy (data);

  gtk_tree_store_remove (self->priv->model, &search.iter);
}

void            
egg_file_format_chooser_set_format (EggFileFormatChooser *self,
                                    guint                 format)
{
  EggFileFormatSearch search;

  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeView *view;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_if_fail (search.success);

  path = gtk_tree_model_get_path (model, &search.iter);
  view = gtk_tree_selection_get_tree_view (self->priv->selection);

  gtk_tree_view_expand_to_path (view, path);
  gtk_tree_selection_unselect_all (self->priv->selection);
  gtk_tree_selection_select_path (self->priv->selection, path);

  gtk_tree_path_free (path);

  if (self->priv->idle_hack > 0)
    {
      g_source_remove (self->priv->idle_hack);
      self->priv->idle_hack = 0;
    }
}

guint
egg_file_format_chooser_get_format (EggFileFormatChooser *self,
                                    const gchar          *filename)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint format = 0;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), -1);

  if (gtk_tree_selection_get_selected (self->priv->selection, &model, &iter))
    gtk_tree_model_get (model, &iter, MODEL_COLUMN_ID, &format, -1);

  if (0 == format && NULL != filename)
    {
      EggFileFormatSearch search;

      search.extension = strrchr(filename, '.');
      search.success = FALSE;

      if (search.extension++)
        gtk_tree_model_foreach (model, find_by_extension, &search);
      if (search.success)
        format = search.format;
    }

  return format;
}

void            
egg_file_format_chooser_set_format_data (EggFileFormatChooser *self,
                                         guint                 format,
                                         gpointer              data,
                                         GDestroyNotify        destroy)
{
  EggFileFormatSearch search;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->model),
                          find_by_format, &search);

  g_return_if_fail (search.success);

  gtk_tree_store_set (self->priv->model, &search.iter,
                      MODEL_COLUMN_DESTROY, destroy,
                      MODEL_COLUMN_DATA, data,
                      -1);
}

gpointer
egg_file_format_chooser_get_format_data (EggFileFormatChooser *self,
                                         guint                 format)
{
  EggFileFormatSearch search;
  gpointer data = NULL;
  GtkTreeModel *model;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), NULL);

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_val_if_fail (search.success, NULL);

  gtk_tree_model_get (model, &search.iter,
                      MODEL_COLUMN_DATA, &data,
                      -1);
  return data;
}

gchar*
egg_file_format_chooser_append_extension (EggFileFormatChooser *self,
                                          const gchar          *filename,
                                          guint                 format)
{
  EggFileFormatSearch search;
  GtkTreeModel *model;
  GtkTreeIter child;

  gchar *extensions;
  gchar *tmpstr;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), NULL);
  g_return_val_if_fail (NULL != filename, NULL);

  if (0 == format)
    format = egg_file_format_chooser_get_format (self, filename);

  if (0 == format)
    {
      g_warning ("%s: No file format selected. Cannot append extension.", G_STRFUNC);
      return NULL;
    }

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_val_if_fail (search.success, NULL);

  gtk_tree_model_get (model, &search.iter, 
                      MODEL_COLUMN_EXTENSIONS, &extensions,
                      -1);

  if (NULL == extensions && 
      gtk_tree_model_iter_nth_child (model, &child, &search.iter, 0))
    {
      gtk_tree_model_get (model, &child, 
                          MODEL_COLUMN_EXTENSIONS, &extensions,
                          -1);
    }

  if (NULL == extensions)
    {
      g_warning ("%s: File format %d doesn't provide file extensions. "
                 "Cannot append extension.", G_STRFUNC, format);
      return NULL;
    }

  if (NULL != (tmpstr = strchr(extensions, ',')))
    *tmpstr = '\0';

  tmpstr = g_strconcat (filename, ".", extensions, NULL);
  g_free (extensions);

  return tmpstr;
}

/* vim: set sw=2 sta et: */
