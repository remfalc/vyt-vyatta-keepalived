/*
 * Soft:        Keepalived is a failover program for the LVS project
 *              <www.linuxvirtualserver.org>. It monitor & manipulate
 *              a loadbalanced server pool using multi-layer checks.
 *
 * Part:        VRRP state transition notification scripts handling.
 *
 * Version:     $Id: vrrp_notify.c,v 1.1.10 2005/02/15 01:15:22 acassen Exp $
 *
 * Author:      Alexandre Cassen, <acassen@linux-vs.org>
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2001-2005 Alexandre Cassen, <acassen@linux-vs.org>
 */

/* system include */
#include <ctype.h>

/* local include */
#include "vrrp_notify.h"
#include "memory.h"
#include "notify.h"

static char *
get_iscript(vrrp_rt * vrrp, int state)
{
	if (!vrrp->notify_exec)
		return NULL;
	if (state == VRRP_STATE_BACK)
		return vrrp->script_backup;
	if (state == VRRP_STATE_MAST)
		return vrrp->script_master;
	if (state == VRRP_STATE_FAULT)
		return vrrp->script_fault;
	return NULL;
}

static char *
get_igscript(vrrp_rt * vrrp)
{
	return vrrp->script;
}

static char *
get_gscript(vrrp_sgroup * vgroup, int state)
{
	if (!vgroup->notify_exec)
		return NULL;
	if (state == VRRP_STATE_BACK)
		return vgroup->script_backup;
	if (state == VRRP_STATE_MAST)
		return vgroup->script_master;
	if (state == VRRP_STATE_FAULT)
		return vgroup->script_fault;
	return NULL;
}

static char *
get_ggscript(vrrp_sgroup * vgroup)
{
	return vgroup->script;
}

static char *
notify_script_name(char *cmdline)
{
	char *cp = cmdline;
	char *script;
	int str_len;

	if (!cmdline)
		return NULL;
	while (!isspace((int) *cp) && *cp != '\0')
		cp++;
	str_len = cp - cmdline;
	script = MALLOC(str_len + 1);
	memcpy(script, cmdline, str_len);
	*(script + str_len) = '\0';

	return script;
}

static int
script_open_litteral(char *script)
{
	FILE *fOut = fopen(script, "r");;
	if (!fOut) {
		syslog(LOG_INFO, "Can't open %s (errno %d %s)", script,
		       errno, strerror(errno));
		return 0;
	}
	fclose(fOut);
	return 1;
}

static int
script_open(char *script)
{
	char *name = notify_script_name(script);
	int ret = name ? script_open_litteral(name) : 0;
	if (name)
		FREE(name);
	return ret;
}

static int
notify_script_exec(char* script, char *type, int state_num, char* name)
{
	char *state = "{UNKNOWN}";
	char *command_line = NULL;
	int size = 0;

	/*
	 * Determine the length of the buffer that we'll need to generate the command
	 * to run:
	 *
	 * "script" {GROUP|INSTANCE} "NAME" {MASTER|BACKUP|FAULT}
	 *
	 * Thus, the length of the buffer will be:
	 *
	 *     ( strlen(script) + 3 ) + ( strlen(type) + 1 ) + ( strlen(state) + 1 ) + ( strlen(name) + 2 ) + 1
	 *
	 * Which is:
	 *     - The length of the script plus two enclosing quotes plus adjacent space
	 *     - The length of the type string plus the adjacent space
	 *     - The length of the state string plus the adjacent space
	 *     - The length of the name of the instance or group, plus two enclosing
	 *       quotes (just in case)
	 *     - The null-terminator
	 *
	 * Which results in:
	 *
	 *     strlen(script) + strlen(type) + strlen(state) + strlen(name) + 8
	 */
	switch (state_num) {
		case VRRP_STATE_MAST  : state = "MASTER" ; break;
		case VRRP_STATE_BACK  : state = "BACKUP" ; break;
		case VRRP_STATE_FAULT : state = "FAULT" ; break;
	}

	size = strlen(script) + strlen(type) + strlen(state) + strlen(name) + 8;
	command_line = MALLOC(size);
	if (!command_line)
		return 0;

	/* Launch the script */
	snprintf(command_line, size, "\"%s\" %s \"%s\" %s",script, type, name, state);
	notify_exec(command_line);
	FREE(command_line);
	return 1;
}

int
notify_instance_exec(vrrp_rt * vrrp, int state)
{
	char *script = get_iscript(vrrp, state);
	char *gscript = get_igscript(vrrp);
	int ret = 0;

	/* Launch the notify_* script */
	if (script && script_open(script)) {
		notify_exec(script);
		ret = 1;
	}

	/* Launch the generic notify script */
	if (gscript && script_open_litteral(gscript)) {
		notify_script_exec(gscript, "INSTANCE", state, vrrp->iname);
		ret = 1;
	}

	return ret;
}

int
notify_group_exec(vrrp_sgroup * vgroup, int state)
{
	char *script = get_gscript(vgroup, state);
	char *gscript = get_ggscript(vgroup);
	int ret = 0;

	/* Launch the notify_* script */
	if (script && script_open(script)) {
		notify_exec(script);
		ret = 1;
	}

	/* Launch the generic notify script */
	if (gscript && script_open_litteral(gscript)) {
		notify_script_exec(gscript, "GROUP", state, vgroup->gname);
		ret = 1;
	}

	return ret;
}
