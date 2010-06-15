/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * heavily based on code from Rhythmbox and Gedit
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 *
 * Sunday 13th May 2007: Bastien Nocera: Add exception clause.
 * See license_change file for details.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Python.h>
#include <pygobject.h>
#include <pygtk/pygtk.h>

#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif /* PY_VERSION_HEX */

/* Exported by Totem Python module */
void pytotem_register_classes (PyObject *d);
void pytotem_add_constants (PyObject *module, const gchar *strip_prefix);
extern PyMethodDef pytotem_functions[];

/* We retrieve this to check for correct class hierarchy */
static PyTypeObject *PyTotemPlugin_Type;

DL_EXPORT (void)
init_totem (void)
{
	PyObject *pygtk, *mdict, *require;
	PyObject *totem, *gtk, *pygtk_version, *pygtk_required_version;
	PyObject *gettext, *install, *gettext_args;
//	PyObject *sys_path;
	struct sigaction old_sigint;
	gint res;
	char *argv[] = { "totem", NULL };
//	char **paths;
//	guint i;

	g_message ("inittotem is called");
#if 0
	if (Py_IsInitialized ()) {
		g_warning ("Python should only be initialized once, since it's in class_init");
		g_return_if_reached ();
	}

	/* Hack to make python not overwrite SIGINT: this is needed to avoid
	 * the crash reported on gedit bug #326191 */

	/* Save old handler */
	res = sigaction (SIGINT, NULL, &old_sigint);
	if (res != 0) {
		g_warning ("Error initializing Python interpreter: cannot get "
		           "handler to SIGINT signal (%s)",
		           strerror (errno));

		return;
	}

	/* Python initialization */
	Py_Initialize ();

	/* Restore old handler */
	res = sigaction (SIGINT, &old_sigint, NULL);
	if (res != 0) {
		g_warning ("Error initializing Python interpreter: cannot restore "
		           "handler to SIGINT signal (%s)",
		           strerror (errno));
		return;
	}

	PySys_SetArgv (1, argv);
#endif
	/* pygtk.require("2.8") */
	pygtk = PyImport_ImportModule ("pygtk");
	if (pygtk == NULL) {
		g_warning ("Could not import pygtk");
		PyErr_Print();
		return;
	}

	mdict = PyModule_GetDict (pygtk);
	require = PyDict_GetItemString (mdict, "require");
	PyObject_CallObject (require, Py_BuildValue ("(S)", PyString_FromString ("2.8")));

	/* import gobject */
	init_pygobject ();

	/* disable pyg* log hooks, since ours is more interesting */
#ifdef pyg_disable_warning_redirections
	pyg_disable_warning_redirections ();
#endif

	/* import gtk */
	init_pygtk ();

	pyg_enable_threads ();

	gtk = PyImport_ImportModule ("gtk");
	if (gtk == NULL) {
		g_warning ("Could not import gtk");
		PyErr_Print();
		return;
	}

	mdict = PyModule_GetDict (gtk);
	pygtk_version = PyDict_GetItemString (mdict, "pygtk_version");
	pygtk_required_version = Py_BuildValue ("(iii)", 2, 12, 0);
	if (PyObject_Compare (pygtk_version, pygtk_required_version) == -1) {
		g_warning("PyGTK %s required, but %s found.",
				  PyString_AsString (PyObject_Repr (pygtk_required_version)),
				  PyString_AsString (PyObject_Repr (pygtk_version)));
		Py_DECREF (pygtk_required_version);
		return;
	}
	Py_DECREF (pygtk_required_version);

#if 0
	/* import totem */
	paths = totem_get_plugin_paths ();
	sys_path = PySys_GetObject ("path");
	for (i = 0; paths[i] != NULL; i++) {
		PyObject *path;

		path = PyString_FromString (paths[i]);
		if (PySequence_Contains (sys_path, path) == 0) {
			PyList_Insert (sys_path, 0, path);
		}
		Py_DECREF (path);
	}
	g_strfreev (paths);
#endif
	PyObject *mdict2, *tuple;

	/* import gedit */
	totem = Py_InitModule ("totem._totem", pytotem_functions);
	mdict = PyModule_GetDict (totem);
	pytotem_register_classes (mdict);
	pytotem_add_constants (totem, "TOTEM_");

	/* Set version */
#if 0
	tuple = Py_BuildValue ("(iii)",
			       GEDIT_MAJOR_VERSION,
			       GEDIT_MINOR_VERSION,
			       GEDIT_MICRO_VERSION);
	PyDict_SetItemString (mdict, "version", tuple);
	Py_DECREF (tuple);
#endif
#if 0
	totem = PyImport_ImportModule ("totem");

	if (totem == NULL) {
		g_warning ("Could not import Python module 'totem'");
		PyErr_Print ();
		return;
	}

	/* add pytotem_functions */
	for (res = 0; pytotem_functions[res].ml_name != NULL; res++) {
		PyObject *func;

		func = PyCFunction_New (&pytotem_functions[res], totem);
		if (func == NULL) {
			g_warning ("unable object for function '%s' create", pytotem_functions[res].ml_name);
			PyErr_Print ();
			return;
		}
		if (PyModule_AddObject (totem, pytotem_functions[res].ml_name, func) < 0) {
			g_warning ("unable to insert function '%s' in 'totem' module", pytotem_functions[res].ml_name);
			PyErr_Print ();
			return;
		}
	}
	mdict = PyModule_GetDict (totem);

	pytotem_register_classes (mdict);
	pytotem_add_constants (totem, "TOTEM_");
#endif
	/* Retrieve the Python type for totem.Plugin */
	PyTotemPlugin_Type = (PyTypeObject *) PyDict_GetItemString (mdict, "Plugin");
	if (PyTotemPlugin_Type == NULL) {
		PyErr_Print ();
		return;
	}

	/* i18n support */
	gettext = PyImport_ImportModule ("gettext");
	if (gettext == NULL) {
		g_warning ("Could not import gettext");
		PyErr_Print();
		return;
	}

	mdict = PyModule_GetDict (gettext);
	install = PyDict_GetItemString (mdict, "install");
	gettext_args = Py_BuildValue ("ss", GETTEXT_PACKAGE, GNOMELOCALEDIR);
	PyObject_CallObject (install, gettext_args);
	Py_DECREF (gettext_args);

	/* ideally totem should clean up the python stuff again at some point,
	 * for which we'd need to save the result of SaveThread so we can then
	 * restore the state in a class_finalize or so, but since totem doesn't
	 * do any clean-up at this point, we'll just skip this as well */
//	PyEval_SaveThread();
}

