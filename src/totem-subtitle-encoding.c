/* 
 * Copyright (C) 2001-2006 Bastien Nocera <hadess@hadess.net>
 *
 * encoding list copied from gnome-terminal/encoding.c
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
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"
#include <glib/gi18n.h>
#include "totem-subtitle-encoding.h"
#include <string.h>

typedef enum
{
  SUBTITLE_ENCODING_CURRENT_LOCALE,

  SUBTITLE_ENCODING_ISO_8859_6,
  SUBTITLE_ENCODING_IBM_864,
  SUBTITLE_ENCODING_MAC_ARABIC,
  SUBTITLE_ENCODING_WINDOWS_1256,

  SUBTITLE_ENCODING_ARMSCII_8,

  SUBTITLE_ENCODING_ISO_8859_4,
  SUBTITLE_ENCODING_ISO_8859_13,
  SUBTITLE_ENCODING_WINDOWS_1257,

  SUBTITLE_ENCODING_ISO_8859_14,

  SUBTITLE_ENCODING_ISO_8859_2,
  SUBTITLE_ENCODING_IBM_852,
  SUBTITLE_ENCODING_MAC_CE,
  SUBTITLE_ENCODING_WINDOWS_1250,

  SUBTITLE_ENCODING_GB18030,
  SUBTITLE_ENCODING_GB2312,
  SUBTITLE_ENCODING_GBK,
  SUBTITLE_ENCODING_HZ,

  SUBTITLE_ENCODING_BIG5,
  SUBTITLE_ENCODING_BIG5_HKSCS,
  SUBTITLE_ENCODING_EUC_TW,

  SUBTITLE_ENCODING_MAC_CROATIAN,

  SUBTITLE_ENCODING_ISO_8859_5,
  SUBTITLE_ENCODING_IBM_855,
  SUBTITLE_ENCODING_ISO_IR_111,
  SUBTITLE_ENCODING_KOI8_R,
  SUBTITLE_ENCODING_MAC_CYRILLIC,
  SUBTITLE_ENCODING_WINDOWS_1251,

  SUBTITLE_ENCODING_CP_866,

  SUBTITLE_ENCODING_MAC_UKRAINIAN,
  SUBTITLE_ENCODING_KOI8_U,

  SUBTITLE_ENCODING_GEOSTD8,

  SUBTITLE_ENCODING_ISO_8859_7,
  SUBTITLE_ENCODING_MAC_GREEK,
  SUBTITLE_ENCODING_WINDOWS_1253,

  SUBTITLE_ENCODING_MAC_GUJARATI,

  SUBTITLE_ENCODING_MAC_GURMUKHI,

  SUBTITLE_ENCODING_ISO_8859_8_I,
  SUBTITLE_ENCODING_IBM_862,
  SUBTITLE_ENCODING_MAC_HEBREW,
  SUBTITLE_ENCODING_WINDOWS_1255,

  SUBTITLE_ENCODING_ISO_8859_8,

  SUBTITLE_ENCODING_MAC_DEVANAGARI,

  SUBTITLE_ENCODING_MAC_ICELANDIC,

  SUBTITLE_ENCODING_EUC_JP,
  SUBTITLE_ENCODING_ISO_2022_JP,
  SUBTITLE_ENCODING_SHIFT_JIS,

  SUBTITLE_ENCODING_EUC_KR,
  SUBTITLE_ENCODING_ISO_2022_KR,
  SUBTITLE_ENCODING_JOHAB,
  SUBTITLE_ENCODING_UHC,

  SUBTITLE_ENCODING_ISO_8859_10,

  SUBTITLE_ENCODING_MAC_FARSI,

  SUBTITLE_ENCODING_ISO_8859_16,
  SUBTITLE_ENCODING_MAC_ROMANIAN,

  SUBTITLE_ENCODING_ISO_8859_3,

  SUBTITLE_ENCODING_TIS_620,

  SUBTITLE_ENCODING_ISO_8859_9,
  SUBTITLE_ENCODING_IBM_857,
  SUBTITLE_ENCODING_MAC_TURKISH,
  SUBTITLE_ENCODING_WINDOWS_1254,

  SUBTITLE_ENCODING_UTF_7,
  SUBTITLE_ENCODING_UTF_8,
  SUBTITLE_ENCODING_UTF_16,
  SUBTITLE_ENCODING_UCS_2,
  SUBTITLE_ENCODING_UCS_4,

  SUBTITLE_ENCODING_ISO_8859_1,
  SUBTITLE_ENCODING_ISO_8859_15,
  SUBTITLE_ENCODING_IBM_850,
  SUBTITLE_ENCODING_MAC_ROMAN,
  SUBTITLE_ENCODING_WINDOWS_1252,

  SUBTITLE_ENCODING_TCVN,
  SUBTITLE_ENCODING_VISCII,
  SUBTITLE_ENCODING_WINDOWS_1258,

  SUBTITLE_ENCODING_LAST
} SubtitleEncodingIndex;


typedef struct
{
  int index;
  gboolean valid;
  char *charset;
  char *name;
} SubtitleEncoding;


static SubtitleEncoding encodings[] = {

  {SUBTITLE_ENCODING_CURRENT_LOCALE, TRUE,
      NULL, N_("Current Locale")},

  {SUBTITLE_ENCODING_ISO_8859_6, FALSE,
      "ISO-8859-6", N_("Arabic")},
  {SUBTITLE_ENCODING_IBM_864, FALSE,
      "IBM864", N_("Arabic")},
  {SUBTITLE_ENCODING_MAC_ARABIC, FALSE,
      "MAC_ARABIC", N_("Arabic")},
  {SUBTITLE_ENCODING_WINDOWS_1256, FALSE,
      "WINDOWS-1256", N_("Arabic")},

  {SUBTITLE_ENCODING_ARMSCII_8, FALSE,
      "ARMSCII-8", N_("Armenian")},

  {SUBTITLE_ENCODING_ISO_8859_4, FALSE,
      "ISO-8859-4", N_("Baltic")},
  {SUBTITLE_ENCODING_ISO_8859_13, FALSE,
      "ISO-8859-13", N_("Baltic")},
  {SUBTITLE_ENCODING_WINDOWS_1257, FALSE,
      "WINDOWS-1257", N_("Baltic")},

  {SUBTITLE_ENCODING_ISO_8859_14, FALSE,
      "ISO-8859-14", N_("Celtic")},

  {SUBTITLE_ENCODING_ISO_8859_2, FALSE,
      "ISO-8859-2", N_("Central European")},
  {SUBTITLE_ENCODING_IBM_852, FALSE,
      "IBM852", N_("Central European")},
  {SUBTITLE_ENCODING_MAC_CE, FALSE,
      "MAC_CE", N_("Central European")},
  {SUBTITLE_ENCODING_WINDOWS_1250, FALSE,
      "WINDOWS-1250", N_("Central European")},

  {SUBTITLE_ENCODING_GB18030, FALSE,
      "GB18030", N_("Chinese Simplified")},
  {SUBTITLE_ENCODING_GB2312, FALSE,
      "GB2312", N_("Chinese Simplified")},
  {SUBTITLE_ENCODING_GBK, FALSE,
      "GBK", N_("Chinese Simplified")},
  {SUBTITLE_ENCODING_HZ, FALSE,
      "HZ", N_("Chinese Simplified")},

  {SUBTITLE_ENCODING_BIG5, FALSE,
      "BIG5", N_("Chinese Traditional")},
  {SUBTITLE_ENCODING_BIG5_HKSCS, FALSE,
      "BIG5-HKSCS", N_("Chinese Traditional")},
  {SUBTITLE_ENCODING_EUC_TW, FALSE,
      "EUC-TW", N_("Chinese Traditional")},

  {SUBTITLE_ENCODING_MAC_CROATIAN, FALSE,
      "MAC_CROATIAN", N_("Croatian")},

  {SUBTITLE_ENCODING_ISO_8859_5, FALSE,
      "ISO-8859-5", N_("Cyrillic")},
  {SUBTITLE_ENCODING_IBM_855, FALSE,
      "IBM855", N_("Cyrillic")},
  {SUBTITLE_ENCODING_ISO_IR_111, FALSE,
      "ISO-IR-111", N_("Cyrillic")},
  {SUBTITLE_ENCODING_KOI8_R, FALSE,
      "KOI8-R", N_("Cyrillic")},
  {SUBTITLE_ENCODING_MAC_CYRILLIC, FALSE,
      "MAC-CYRILLIC", N_("Cyrillic")},
  {SUBTITLE_ENCODING_WINDOWS_1251, FALSE,
      "WINDOWS-1251", N_("Cyrillic")},

  {SUBTITLE_ENCODING_CP_866, FALSE,
      "CP866", N_("Cyrillic/Russian")},

  {SUBTITLE_ENCODING_MAC_UKRAINIAN, FALSE,
      "MAC_UKRAINIAN", N_("Cyrillic/Ukrainian")},
  {SUBTITLE_ENCODING_KOI8_U, FALSE,
      "KOI8-U", N_("Cyrillic/Ukrainian")},

  {SUBTITLE_ENCODING_GEOSTD8, FALSE,
      "GEORGIAN-PS", N_("Georgian")},

  {SUBTITLE_ENCODING_ISO_8859_7, FALSE,
      "ISO-8859-7", N_("Greek")},
  {SUBTITLE_ENCODING_MAC_GREEK, FALSE,
      "MAC_GREEK", N_("Greek")},
  {SUBTITLE_ENCODING_WINDOWS_1253, FALSE,
      "WINDOWS-1253", N_("Greek")},

  {SUBTITLE_ENCODING_MAC_GUJARATI, FALSE,
      "MAC_GUJARATI", N_("Gujarati")},

  {SUBTITLE_ENCODING_MAC_GURMUKHI, FALSE,
      "MAC_GURMUKHI", N_("Gurmukhi")},

  {SUBTITLE_ENCODING_ISO_8859_8_I, FALSE,
      "ISO-8859-8-I", N_("Hebrew")},
  {SUBTITLE_ENCODING_IBM_862, FALSE,
      "IBM862", N_("Hebrew")},
  {SUBTITLE_ENCODING_MAC_HEBREW, FALSE,
      "MAC_HEBREW", N_("Hebrew")},
  {SUBTITLE_ENCODING_WINDOWS_1255, FALSE,
      "WINDOWS-1255", N_("Hebrew")},

  {SUBTITLE_ENCODING_ISO_8859_8, FALSE,
      "ISO-8859-8", N_("Hebrew Visual")},

  {SUBTITLE_ENCODING_MAC_DEVANAGARI, FALSE,
      "MAC_DEVANAGARI", N_("Hindi")},

  {SUBTITLE_ENCODING_MAC_ICELANDIC, FALSE,
      "MAC_ICELANDIC", N_("Icelandic")},

  {SUBTITLE_ENCODING_EUC_JP, FALSE,
      "EUC-JP", N_("Japanese")},
  {SUBTITLE_ENCODING_ISO_2022_JP, FALSE,
      "ISO2022JP", N_("Japanese")},
  {SUBTITLE_ENCODING_SHIFT_JIS, FALSE,
      "SHIFT-JIS", N_("Japanese")},

  {SUBTITLE_ENCODING_EUC_KR, FALSE,
      "EUC-KR", N_("Korean")},
  {SUBTITLE_ENCODING_ISO_2022_KR, FALSE,
      "ISO2022KR", N_("Korean")},
  {SUBTITLE_ENCODING_JOHAB, FALSE,
      "JOHAB", N_("Korean")},
  {SUBTITLE_ENCODING_UHC, FALSE,
      "UHC", N_("Korean")},

  {SUBTITLE_ENCODING_ISO_8859_10, FALSE,
      "ISO-8859-10", N_("Nordic")},

  {SUBTITLE_ENCODING_MAC_FARSI, FALSE,
      "MAC_FARSI", N_("Persian")},

  {SUBTITLE_ENCODING_ISO_8859_16, FALSE,
      "ISO-8859-16", N_("Romanian")},
  {SUBTITLE_ENCODING_MAC_ROMANIAN, FALSE,
      "MAC_ROMANIAN", N_("Romanian")},

  {SUBTITLE_ENCODING_ISO_8859_3, FALSE,
      "ISO-8859-3", N_("South European")},

  {SUBTITLE_ENCODING_TIS_620, FALSE,
      "TIS-620", N_("Thai")},

  {SUBTITLE_ENCODING_ISO_8859_9, FALSE,
      "ISO-8859-9", N_("Turkish")},
  {SUBTITLE_ENCODING_IBM_857, FALSE,
      "IBM857", N_("Turkish")},
  {SUBTITLE_ENCODING_MAC_TURKISH, FALSE,
      "MAC_TURKISH", N_("Turkish")},
  {SUBTITLE_ENCODING_WINDOWS_1254, FALSE,
      "WINDOWS-1254", N_("Turkish")},

  {SUBTITLE_ENCODING_UTF_7, FALSE,
      "UTF-7", N_("Unicode")},
  {SUBTITLE_ENCODING_UTF_8, FALSE,
      "UTF-8", N_("Unicode")},
  {SUBTITLE_ENCODING_UTF_16, FALSE,
      "UTF-16", N_("Unicode")},
  {SUBTITLE_ENCODING_UCS_2, FALSE,
      "UCS-2", N_("Unicode")},
  {SUBTITLE_ENCODING_UCS_4, FALSE,
      "UCS-4", N_("Unicode")},

  {SUBTITLE_ENCODING_ISO_8859_1, FALSE,
      "ISO-8859-1", N_("Western")},
  {SUBTITLE_ENCODING_ISO_8859_15, FALSE,
      "ISO-8859-15", N_("Western")},
  {SUBTITLE_ENCODING_IBM_850, FALSE,
      "IBM850", N_("Western")},
  {SUBTITLE_ENCODING_MAC_ROMAN, FALSE,
      "MAC_ROMAN", N_("Western")},
  {SUBTITLE_ENCODING_WINDOWS_1252, FALSE,
      "WINDOWS-1252", N_("Western")},

  {SUBTITLE_ENCODING_TCVN, FALSE,
      "TCVN", N_("Vietnamese")},
  {SUBTITLE_ENCODING_VISCII, FALSE,
      "VISCII", N_("Vietnamese")},
  {SUBTITLE_ENCODING_WINDOWS_1258, FALSE,
      "WINDOWS-1258", N_("Vietnamese")}
};

static const SubtitleEncoding *
find_encoding_by_charset (const char *charset)
{
  int i;

  i = 1;                        /* skip current locale */
  while (i < SUBTITLE_ENCODING_LAST) {
    if (strcasecmp (charset, encodings[i].charset) == 0)
      return &encodings[i];

    ++i;
  }

  if (strcasecmp (charset,
          encodings[SUBTITLE_ENCODING_CURRENT_LOCALE].charset) == 0)
    return &encodings[SUBTITLE_ENCODING_CURRENT_LOCALE];

  return NULL;
}

static void
subtitle_encoding_init (void)
{
  int i;
  gsize bytes_read, bytes_written;
  gchar *converted;
  gchar ascii_sample[96];

  g_get_charset ((const char **)
      &encodings[SUBTITLE_ENCODING_CURRENT_LOCALE].charset);

  g_assert (G_N_ELEMENTS (encodings) == SUBTITLE_ENCODING_LAST);

  /* Initialize the sample text with all of the printing ASCII characters
   * from space (32) to the tilde (126), 95 in all. */
  for (i = 0; i < (int) sizeof (ascii_sample); i++)
    ascii_sample[i] = i + 32;

  ascii_sample[sizeof (ascii_sample) - 1] = '\0';

  i = 0;
  while (i < SUBTITLE_ENCODING_LAST) {
    bytes_read = 0;
    bytes_written = 0;

    g_assert (encodings[i].index == i);

    /* Translate the names */
    encodings[i].name = _(encodings[i].name);

    /* Test that the encoding is a proper superset of ASCII (which naive
     * apps are going to use anyway) by attempting to validate the text
     * using the current encoding.  This also flushes out any encodings
     * which the underlying GIConv implementation can't support.
     */
    converted = g_convert (ascii_sample, sizeof (ascii_sample) - 1,
        encodings[i].charset, encodings[i].charset,
        &bytes_read, &bytes_written, NULL);

    /* The encoding is only valid if ASCII passes through cleanly. */
    if (i == SUBTITLE_ENCODING_CURRENT_LOCALE)
      encodings[i].valid = TRUE;
    else
      encodings[i].valid =
          (bytes_read == (sizeof (ascii_sample) - 1)) &&
          (converted != NULL) && (strcmp (converted, ascii_sample) == 0);

#ifdef DEBUG_ENCODINGS
    if (!encodings[i].valid) {
      g_print ("Rejecting encoding %s as invalid:\n", encodings[i].charset);
      g_print (" input  \"%s\"\n", ascii_sample);
      g_print (" output \"%s\"\n\n", converted ? converted : "(null)");
    }
#endif

    /* Discard the converted string. */
    g_free (converted);

    ++i;
  }
}

static int
subtitle_encoding_get_index (const char *charset)
{
  const SubtitleEncoding *e;

  e = find_encoding_by_charset (charset);
  if (e != NULL)
    return e->index;
  else
    return SUBTITLE_ENCODING_CURRENT_LOCALE;
}

static const char *
subtitle_encoding_get_charset (int index)
{
  const SubtitleEncoding *e;

  if (index >= SUBTITLE_ENCODING_LAST)
    e = &encodings[SUBTITLE_ENCODING_CURRENT_LOCALE];
  else if (index < SUBTITLE_ENCODING_CURRENT_LOCALE)
    e = &encodings[SUBTITLE_ENCODING_CURRENT_LOCALE];
  else if (!encodings[index].valid)
    e = &encodings[SUBTITLE_ENCODING_CURRENT_LOCALE];
  else
    e = &encodings[index];
  return e->charset;
}

enum
{
  INDEX_COL,
  NAME_COL
};

static gint
compare (GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, gpointer data)
{
  gchar *str_a, *str_b;
  gint result;

  gtk_tree_model_get (model, a, NAME_COL, &str_a, -1);
  gtk_tree_model_get (model, b, NAME_COL, &str_b, -1);

  result = strcmp (str_a, str_b);

  g_free (str_a);
  g_free (str_b);

  return result;
}

static void
is_encoding_sensitive (GtkCellLayout * cell_layout,
    GtkCellRenderer * cell,
    GtkTreeModel * tree_model, GtkTreeIter * iter, gpointer data)
{

  gboolean sensitive;

  sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);
  g_object_set (cell, "sensitive", sensitive, NULL);
}

static GtkTreeModel *
subtitle_encoding_create_store (void)
{
  gchar *label;
  gchar *lastlang = "";
  GtkTreeIter iter, iter2;
  GtkTreeStore *store;
  int i;

  store = gtk_tree_store_new (2, G_TYPE_INT, G_TYPE_STRING);

  for (i = 0; i < SUBTITLE_ENCODING_LAST; i++) {
    if (encodings[i].valid) {
      if (strcmp (lastlang, encodings[i].name)) {
        lastlang = encodings[i].name;
        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, INDEX_COL,
            -1, NAME_COL, lastlang, -1);
      }
      label = g_strdup_printf("%s (%s)", lastlang, encodings[i].charset);
      gtk_tree_store_append (store, &iter2, &iter);
      gtk_tree_store_set (store, &iter2, INDEX_COL,
          encodings[i].index, NAME_COL, label, -1);
      g_free(label);
    }
  }
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
      compare, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
      NAME_COL, GTK_SORT_ASCENDING);
  return GTK_TREE_MODEL (store);
}

static void
subtitle_encoding_combo_render (GtkComboBox * combo)
{
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
      "text", NAME_COL, NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
      renderer, is_encoding_sensitive, NULL, NULL);
}

const char *
totem_subtitle_encoding_get_selected (GtkComboBox * combo)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint index = -1;

  model = gtk_combo_box_get_model (combo);
  if (gtk_combo_box_get_active_iter (combo, &iter)) {
    gtk_tree_model_get (model, &iter, INDEX_COL, &index, -1);
  }
  if (index == -1)
    return NULL;
  return subtitle_encoding_get_charset (index);
}

void
totem_subtitle_encoding_set (GtkComboBox * combo, const char *encoding)
{
  GtkTreeModel *model;
  GtkTreeIter iter, iter2;
  gint index, i;

  g_return_if_fail (encoding != NULL);

  model = gtk_combo_box_get_model (combo);
  index = subtitle_encoding_get_index (encoding);
  gtk_tree_model_get_iter_first (model, &iter);
  do {
    if (!gtk_tree_model_iter_has_child (model, &iter))
      continue;
    if (!gtk_tree_model_iter_children (model, &iter2, &iter))
      continue;
    do {
      gtk_tree_model_get (model, &iter2, INDEX_COL, &i, -1);
      if (i == index)
        break;
    } while (gtk_tree_model_iter_next (model, &iter2));
    if (i == index)
      break;
  } while (gtk_tree_model_iter_next (model, &iter));
  gtk_combo_box_set_active_iter (combo, &iter2);
}

void
totem_subtitle_encoding_init (GtkComboBox *combo)
{
  GtkTreeModel *model;
  subtitle_encoding_init ();
  model = subtitle_encoding_create_store ();
  gtk_combo_box_set_model (combo, model);
  g_object_unref (model);
  subtitle_encoding_combo_render (combo);
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
