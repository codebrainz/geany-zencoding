/*
 * zen-controller.h
 *
 * Copyright 2011 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA.
 *
 */

#ifndef ZEN_CONTROLLER_H
#define ZEN_CONTROLLER_H
#ifdef __cplusplus
extern "C" {
#endif


#include <Python.h>
#include <geanyplugin.h>


typedef struct _ZenController ZenController;

struct _ZenController
{
	PyObject *editor;
	PyObject *run_action;
	PyObject *set_context;
	PyObject *set_active_profile;
};


ZenController *zen_controller_new(const char *zendir, const char *profiles_dir);
void zen_controller_free(ZenController *zen);
void zen_controller_run_action(ZenController *zen, const char *action_name);
void zen_controller_set_active_profile(ZenController *zen, const char *profile);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ZEN_CONTROLLER_H */
