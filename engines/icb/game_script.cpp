/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * Additional copyright for this file:
 * Copyright (C) 1999-2000 Revolution Software Ltd.
 * This code is based on source code created by Revolution Software,
 * used with permission.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "engines/icb/common/px_windows.h"
#include "res_man.h"
#include "game_script.h"
#include "debug.h"
#include "mission.h"
#include "p4.h"
#include "global_objects.h"
#include "global_switches.h"
#include "sound.h"
#include "global_vars.h"

// for checking we have correct disk on psx
#if _PSX
#include "engines/icb/gfx/psx_disc.h"
#include "credits_psx.h"
#else
#include "main_menu.h"
#endif

#include "common/events.h"
#include "common/textconsole.h"

namespace ICB {

void _game_script::Restart_game_script() {
	pc = 0;
	Init_globals(); // must reload the global vars for the new game!
}

bool8 _game_script::Running_from_gamescript() { return running_from_game_script; }

bool8 _game_script::Init_game_script() {
	// assume
	running_from_game_script = FALSE8;

#ifdef _PC
	// If we are running tt mode (translator mode) then ignore any game scripts
	if (tt)
		return FALSE8;
#endif //_PC

	// build name

	sprintf(fname, GAMESCRIPT_PATH);
	sprintf(cluster, GLOBAL_CLUSTER_PATH);
	fn_hash = HashString(fname);
	cluster_hash = HashString(cluster);

	warning("Init_gs::'%s'::'%s'", fname, cluster);
	if (private_session_resman->Test_file(fname, fn_hash, cluster, cluster_hash)) {
		// program counter in gameScript

		pc = 0;

		warning("Gamescript found");
		running_from_game_script = TRUE8;

		return TRUE8;
	}
	warning("Gamescript: %s %s not found", fname, cluster);

	return FALSE8;
}

void Init_play_movie(const char *param0, bool8 param1);

// runs the gamescript until a given bookmark then returns
// so the pc is set to the correct place for a mission....
void _game_script::Run_to_bookmark(const char *name) {
	if ((g_mission) && (g_mission->session)) {
#ifdef _PSX
		// session
		uint32 cluster_hash = MS->Fetch_session_cluster_hash();
		buf = private_session_resman->Res_open(fname, fn_hash, MS->Fetch_session_cluster(), cluster_hash);
#else
		// global
		buf = private_session_resman->Res_open(fname, fn_hash, cluster, cluster_hash);
#endif
	} else {
		// global
		buf = private_session_resman->Res_open(fname, fn_hash, cluster, cluster_hash);
	}

	char command;
	char p1[ENGINE_STRING_LEN];

	// reset program counter (but keep demo flag!)
	int demo = g_globalScriptVariables.GetVariable("demo");
	Restart_game_script();
	g_globalScriptVariables.SetVariable("demo", demo);

	// now loop through gamescript...
	while (1) {
		command = buf[pc];

		switch (command) {
		case 0:
			Fatal_error("Bookmark %s not found in gamescript", name);
			break;

		case 'B':
			pc += 2;
			Fetch_next_param(p1);
			Fetch_next_line();

			// if this is the bookmark then return, pc is in the right place...
			if (strcmp(p1, name) == 0)
				return;

			// otherwise we keep looking...
			break;

		default: // any other command
			Fetch_next_line();
			break;
		}
	}
}

void _game_script::Process_game_script() {
	// process the next command
	// this is a stub routine and so drops out at end and is then called again

	char command;
	char p1[ENGINE_STRING_LEN];
	char p2[ENGINE_STRING_LEN];
	char p3[ENGINE_STRING_LEN];

#ifdef _PSX
	if ((mission) && (mission->session)) {
		// session
		uint32 cluster_hash = MS->Fetch_session_cluster_hash();
		buf = private_session_resman->Res_open(fname, fn_hash, MS->Fetch_session_cluster(), cluster_hash);
	} else {
		// global
		buf = private_session_resman->Res_open(fname, fn_hash, cluster, cluster_hash);
	}
#else
	// global
	buf = private_session_resman->Res_open(fname, fn_hash, cluster, cluster_hash);
#endif

	// get next command
	command = buf[pc];

	switch (command) {
	case 0:
		Message_box("thank you for playing In Cold Blood (c) Revolution Software Ltd 1999");
#ifdef _PC
		{
			Common::Event event;
			event.type = Common::EVENT_QUIT;
			g_system->getEventManager()->pushEvent(event);
		}
#endif
		break;

	// bookmark
	// these are ignored
	case 'B':
		pc += 2;
		Fetch_next_param(p1);
		Fetch_next_line(); // ignored in normal gamescript
		warning("Hit bookmark mission %s", p1);
		break;

	case 'X': // yes, its the amazing X mode   - t h e  m a i n  m e n u -
		Fetch_next_line();
		stub.Push_stub_mode(__toe_on_door);
		break;

	case 'W':
		Fetch_next_line();
#if _PC
		Fatal_error("midWay legal screen not supported on PC!");
#else
		DisplayLegalScreen();
#endif
		break;

	case 'M':
		pc += 2;

		Fetch_next_param(p1);
		Fetch_next_param(p2);
		Fetch_next_line();

		if (Setup_new_mission(p1, p2)) { // mission_name, session_name
			// only do actor relative on pc
#if _PC
			MS->player.Set_control_mode(ACTOR_RELATIVE);
#else
			printf("mission/session loaded/started\n");
#endif
			stub.Push_stub_mode(__mission_and_console);
		} else {
			Fatal_error("no such mission-session [%s][%s]", p1, p2);
		}
		break;

	case 'L': // load a mission, but dont play it
		pc += 2;

		Fetch_next_param(p1);
		Fetch_next_param(p2);
		Fetch_next_line();

		if (!Setup_new_mission(p1, p2)) // mission_name, session_name
			Fatal_error("no such mission-session [%s][%s]", p1, p2);

		// disable sounds for now...
		PauseSounds();

		g_mission->Game_cycle();     // do a cycle - load the set graphics
		g_mission->Create_display(); // test
		break;

	case 'P':
		Fetch_next_line();
#if _PC
		MS->player.Set_control_mode(ACTOR_RELATIVE);
#else
		// enable sounds...
		UnpauseSounds();

		printf("play...\n");
#endif
		stub.Push_stub_mode(__mission_and_console);
		break;

	case 'T':
		pc += 2; // skip the T

		Fetch_next_param(p1);
		Fetch_next_param(p2);
		Fetch_next_param(p3);
		Fetch_next_line();

		warning("text scrolly %s over movie/screen %s starting frame %d", p1, p2, atoi(p3));

#if _PC
		InitisliaseScrollingText(p1, p2, atoi(p3));
		stub.Push_stub_mode(__scrolling_text);
#else
		DoScrollingText(p1, p2, atoi(p3));
#endif
		break;

	case 'G':
		pc += 2;

		Fetch_next_param(p1);
		Fetch_next_param(p2);
		Fetch_next_line();
		g_globalScriptVariables.SetVariable(p1, (atoi(p2)));
		break;

	case 'R': // restart
		Restart_game_script();
		break;

	case 'D': // debugging on again
		px.debugging_and_console = TRUE8;
		Fetch_next_line();
		break;

	case 'S': // play a movie sequence
		pc += 2;
		Fetch_next_param(p1);
		Fetch_next_param(p2);
		Fetch_next_line();
		Init_play_movie(p1, (bool8)atoi(p2));
		break;

	case 'C': // set cd number
		pc += 2;
		Fetch_next_param(p1);
		Fetch_next_line();
		px.current_cd = atoi(p1);

#if _PSX
		DiscCheckInserted();
#endif
		if ((!px.current_cd) || (px.current_cd > 3))
			Fatal_error("gamescript tried to set silly cd number %d", px.current_cd);
		break;

	case 'Z': // Signify that the game has been completed
		Fetch_next_line();
#if _PC
		GameCompleted();
#endif
		break;

	default:
		Fatal_error("unknown game script command '%s'", buf[pc]);
		break;
	}
}

void _game_script::Fetch_next_param(char *p) {
	uint8 c = 0;

	while ((buf[pc]) && (buf[pc] != 0x0d) && (buf[pc] != ' '))
		p[c++] = buf[pc++];

	p[c] = 0;
	Zdebug("%s", p);

	pc++;
}

void _game_script::Fetch_next_line() {
	// seek to next line

	// return false if current line is the last line

	do {
		while ((buf[pc]) && (buf[pc] != 0x0a))
			pc++;

		if (!buf[pc]) // end of file
			return;

		pc++; // past the 0x0a

	} while (buf[pc] == 0x0d); // keep skipping blank line
}

} // End of namespace ICB