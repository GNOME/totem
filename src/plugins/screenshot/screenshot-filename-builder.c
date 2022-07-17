/* screenshot-filename-builder.c - Builds a filename suitable for a screenshot
 *
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 * 28th June 2012: Bastien Nocera: Add exception clause.
 * See license_change file for details.
 */

#include <config.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pwd.h>
#include <string.h>

#include "screenshot-filename-builder.h"
#include "totem-uri.h"

typedef enum
{
  TEST_SAVED_DIR = 0,
  TEST_DEFAULT,
  TEST_FALLBACK,
  NUM_TESTS
} TestType;

typedef struct
{
  char *base_paths[NUM_TESTS];
  char *screenshot_origin;
  int iteration;
  TestType type;
} AsyncExistenceJob;

/* Taken from gnome-vfs-utils.c */
static char *
expand_initial_tilde (const char *path)
{
  g_autofree gchar *user_name = NULL;
  char *slash_after_user_name;
  struct passwd *passwd_file_entry;

  if (path[1] == '/' || path[1] == '\0') {
    return g_build_filename (g_get_home_dir (), &path[1], NULL);
  }

  slash_after_user_name = strchr (&path[1], '/');
  if (slash_after_user_name == NULL) {
    user_name = g_strdup (&path[1]);
  } else {
    user_name = g_strndup (&path[1],
                           slash_after_user_name - &path[1]);
  }
  passwd_file_entry = getpwnam (user_name);

  if (passwd_file_entry == NULL || passwd_file_entry->pw_dir == NULL) {
    return g_strdup (path);
  }

  return g_strconcat (passwd_file_entry->pw_dir,
                      slash_after_user_name,
                      NULL);
}

gchar *
get_fallback_screenshot_dir (void)
{
  return g_strdup (g_get_home_dir ());
}

gchar *
get_default_screenshot_dir (void)
{
  const char *special_dir = NULL;
  char *screenshots_dir = NULL;
  g_autoptr(GFile) file = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  special_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (special_dir == NULL)
    return NULL;

  /* Translators: "Screenshots" is the name of the folder under ~/Pictures for screenshots,
   * same as used in GNOME Shell:
   * https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/gnome-42/js/ui/screenshot.js#L2072 */
  screenshots_dir = g_build_filename (special_dir, _("Screenshots"), NULL);

  /* ensure that the "Screenshots" folder exists */
  file = g_file_new_for_path (screenshots_dir);
  ret = g_file_make_directory_with_parents (file, NULL, &error);
  if (!ret && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    g_clear_pointer (&screenshots_dir, g_free);

  return screenshots_dir;
}

static gchar *
sanitize_save_directory (const gchar *save_dir)
{
  if (save_dir == NULL)
    return NULL;

  if (save_dir[0] == '~')
    return expand_initial_tilde (save_dir);

  if (strstr (save_dir, "://") != NULL)
    {
      g_autoptr(GFile) file = g_file_new_for_uri (save_dir);
      return g_file_get_path (file);
    }

  return g_strdup (save_dir);
}

static char *
build_path (AsyncExistenceJob *job)
{
  g_autofree gchar *file_name = NULL, *origin = NULL;
  const gchar *base_path;

  base_path = job->base_paths[job->type];

  if (base_path == NULL ||
      base_path[0] == '\0')
    return NULL;

  if (job->screenshot_origin == NULL)
    {
      g_autoptr(GDateTime) d = g_date_time_new_now_local ();
      origin = g_date_time_format (d, "%Y-%m-%d %H-%M-%S");
    }
  else
    origin = g_strdup (job->screenshot_origin);

  if (job->iteration == 0)
    {
      /* translators: this is the name of the file that gets made up
       * with the screenshot if the entire screen is taken */
      file_name = g_strdup_printf (_("Screenshot from %s.png"), origin);
    }
  else
    {
      /* translators: this is the name of the file that gets
       * made up with the screenshot if the entire screen is
       * taken */
      file_name = g_strdup_printf (_("Screenshot from %s - %d.png"), origin, job->iteration);
    }

  return g_build_filename (base_path, file_name, NULL);
}

static void
async_existence_job_free (AsyncExistenceJob *job)
{
  gint idx;

  for (idx = 0; idx < NUM_TESTS; idx++)
    g_free (job->base_paths[idx]);

  g_free (job->screenshot_origin);

  g_slice_free (AsyncExistenceJob, job);
}

static gboolean
prepare_next_cycle (AsyncExistenceJob *job)
{
  gboolean res = FALSE;

  if (job->type != (NUM_TESTS - 1))
    {
      (job->type)++;
      job->iteration = 0;

      res = TRUE;
    }

  return res;
}

static void
try_check_file (GTask *task,
                gpointer source_object,
                gpointer data,
                GCancellable *cancellable)
{
  AsyncExistenceJob *job = data;

  while (TRUE)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file = NULL;
      g_autoptr(GFileInfo) info = NULL;
      g_autofree gchar *path = build_path (job);

      if (path == NULL)
        {
          (job->type)++;
          continue;
        }

      file = g_file_new_for_path (path);
      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                G_FILE_QUERY_INFO_NONE, cancellable, &error);
      if (info != NULL)
        {
          /* file already exists, iterate again */
          (job->iteration)++;
          continue;
        }

      /* see the error to check whether the location is not accessible
       * or the file does not exist.
       */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_autoptr(GFile) parent = g_file_get_parent (file);

          /* if the parent directory doesn't exist as well, we'll forget the saved
           * directory and treat this as a generic error.
           */
          if (g_file_query_exists (parent, NULL))
            {
              g_task_return_pointer (task, g_steal_pointer (&path), NULL);
              return;
            }
        }

      if (!prepare_next_cycle (job))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "%s", "Failed to find a valid place to save");
          return;
        }
    }
}

void
screenshot_build_filename_async (const char *save_dir,
                                 const char *screenshot_origin,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  AsyncExistenceJob *job;
  g_autoptr(GTask) task = NULL;

  job = g_slice_new0 (AsyncExistenceJob);

  job->base_paths[TEST_SAVED_DIR] = sanitize_save_directory (save_dir);
  job->base_paths[TEST_DEFAULT] = get_default_screenshot_dir ();
  job->base_paths[TEST_FALLBACK] = get_fallback_screenshot_dir ();
  job->iteration = 0;
  job->type = TEST_SAVED_DIR;

  job->screenshot_origin = g_strdup (screenshot_origin);

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_task_data (task, job, (GDestroyNotify) async_existence_job_free);

  g_task_run_in_thread (task, try_check_file);
}

gchar *
screenshot_build_filename_finish (GAsyncResult *result,
                                  GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}
