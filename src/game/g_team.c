/*
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012 Jan Simek <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 */
/**
 * @file g_team.c
 */

#include <limits.h>

#include "g_local.h"

#ifdef FEATURE_OMNIBOT
#include "g_etbot_interface.h"
#endif

int OtherTeam(int team)
{
	if (team == TEAM_AXIS)
	{
		return TEAM_ALLIES;
	}
	else if (team == TEAM_ALLIES)
	{
		return TEAM_AXIS;
	}
	return team;
}

const char *TeamName(int team)
{
	if (team == TEAM_AXIS)
	{
		return "RED";
	}
	else if (team == TEAM_ALLIES)
	{
		return "BLUE";
	}
	else if (team == TEAM_SPECTATOR)
	{
		return "SPECTATOR";
	}
	return "FREE";
}

const char *TeamColorString(int team)
{
	if (team == TEAM_AXIS)
	{
		return S_COLOR_RED;
	}
	else if (team == TEAM_ALLIES)
	{
		return S_COLOR_BLUE;
	}
	else if (team == TEAM_SPECTATOR)
	{
		return S_COLOR_YELLOW;
	}
	return S_COLOR_WHITE;
}

// NULL for everyone
void QDECL PrintMsg(gentity_t *ent, const char *fmt, ...)
{
	char    msg[1024];
	va_list argptr;
	char    *p;

	// NOTE: if buffer overflow, it's more likely to corrupt stack and crash than do a proper G_Error?
	va_start(argptr, fmt);
	if (Q_vsnprintf(msg, sizeof(msg), fmt, argptr) > sizeof(msg))
	{
		G_Error("PrintMsg overrun\n");
	}
	va_end(argptr);

	// double quotes are bad
	while ((p = strchr(msg, '"')) != NULL)
	{
		*p = '\'';
	}

	trap_SendServerCommand(((ent == NULL) ? -1 : ent - g_entities), va("print \"%s\"", msg));
}

/*
==============
OnSameTeam
==============
*/
qboolean OnSameTeam(gentity_t *ent1, gentity_t *ent2)
{
	if (!ent1 || !ent1->client || !ent2 || !ent2->client)
	{
		return qfalse;
	}

	if (ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam)
	{
		return qtrue;
	}

	return qfalse;
}

#define WCP_ANIM_NOFLAG             0
#define WCP_ANIM_RAISE_AXIS         1
#define WCP_ANIM_RAISE_AMERICAN     2
#define WCP_ANIM_AXIS_RAISED        3
#define WCP_ANIM_AMERICAN_RAISED    4
#define WCP_ANIM_AXIS_TO_AMERICAN   5
#define WCP_ANIM_AMERICAN_TO_AXIS   6
#define WCP_ANIM_AXIS_FALLING       7
#define WCP_ANIM_AMERICAN_FALLING   8

/*
================
Team_FragBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumlative.  You get one, they are in importance
order.
================
*/
void Team_FragBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker)
{
	int       flag_pw, enemy_flag_pw;
	int       otherteam;
	gentity_t *flag, *carrier = NULL;
	char      *c;
	vec3_t    v1, v2;
	int       team;

	// no bonus for fragging yourself
	if (!targ->client || !attacker->client || targ == attacker)
	{
		return;
	}

	team      = targ->client->sess.sessionTeam;
	otherteam = OtherTeam(targ->client->sess.sessionTeam);
	if (otherteam < 0)
	{
		return; // whoever died isn't on a team

	}
	// no bonuses for fragging friendlies, penalties scored elsewhere
	if (team == attacker->client->sess.sessionTeam)
	{
		return;
	}

	// same team, if the flag at base, check to he has the enemy flag
	if (team == TEAM_AXIS)
	{
		flag_pw       = PW_REDFLAG;
		enemy_flag_pw = PW_BLUEFLAG;
	}
	else
	{
		flag_pw       = PW_BLUEFLAG;
		enemy_flag_pw = PW_REDFLAG;
	}

	// did the attacker frag the flag carrier?
	if (targ->client->ps.powerups[enemy_flag_pw])
	{
		AddScore(attacker, WOLF_FRAG_CARRIER_BONUS);

		return;
	}
	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->client->sess.sessionTeam)
	{
	case TEAM_AXIS:
		c = "team_CTF_redflag";
		break;
	case TEAM_ALLIES:
		c = "team_CTF_blueflag";
		break;
	default:
		return;
	}

	flag = NULL;
	while ((flag = G_Find(flag, FOFS(classname), c)) != NULL)
	{
		if (!(flag->flags & FL_DROPPED_ITEM))
		{
			break;
		}
	}

	if (flag)     // added some more stuff after this fn
	{ //      return; // can't find attacker's flag
		int i;

		// find attacker's team's flag carrier
		for (i = 0; i < g_maxclients.integer; i++)
		{
			carrier = g_entities + i;
			if (carrier->inuse && carrier->client->ps.powerups[flag_pw])
			{
				break;
			}
			carrier = NULL;
		}

		// ok we have the attackers flag and a pointer to the carrier

		// check to see if we are defending the base's flag
		VectorSubtract(targ->client->ps.origin, flag->s.origin, v1);
		VectorSubtract(attacker->client->ps.origin, flag->s.origin, v2);

		if ((VectorLengthSquared(v1) < Square(CTF_TARGET_PROTECT_RADIUS) ||
		     VectorLengthSquared(v2) < Square(CTF_TARGET_PROTECT_RADIUS) ||
		     CanDamage(flag, targ->client->ps.origin) || CanDamage(flag, attacker->client->ps.origin)) &&
		    attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam)
		{
			// we defended the base flag
			// FIXME -- don't report flag defense messages, change to gooder message
			AddScore(attacker, WOLF_FLAG_DEFENSE_BONUS);
			return;
		}
	}

	// look for nearby checkpoints and spawnpoints
	flag = NULL;
	while ((flag = G_Find(flag, FOFS(classname), "team_WOLF_checkpoint")) != NULL)
	{
		VectorSubtract(targ->client->ps.origin, flag->s.origin, v1);
		if ((flag->s.frame != WCP_ANIM_NOFLAG) && (flag->count == attacker->client->sess.sessionTeam))
		{
			if (VectorLengthSquared(v1) < Square(WOLF_CP_PROTECT_RADIUS))
			{
				if (flag->spawnflags & 1)                         // protected spawnpoint
				{
					AddScore(attacker, WOLF_SP_PROTECT_BONUS);
				}
				else
				{
					AddScore(attacker, WOLF_CP_PROTECT_BONUS);    // protected checkpoint
				}
			}
		}
	}
}

void Team_ResetFlag(gentity_t *ent)
{
	if (!ent)
	{
		G_Printf("Warning: NULL passed to Team_ResetFlag\n");
		return;
	}

	if (ent->flags & FL_DROPPED_ITEM)
	{
		Team_ResetFlag(&g_entities[ent->s.otherEntityNum]);
		G_FreeEntity(ent);
	}
	else
	{
		ent->s.density++;

		// do we need to respawn?
		if (ent->s.density == 1)
		{
			RespawnItem(ent);
#ifdef FEATURE_OMNIBOT

			Bot_Util_SendTrigger(ent, NULL, va("Flag returned %s!", _GetEntityName(ent)), "returned");
#endif
		}
	}
}

void Team_ReturnFlagSound(gentity_t *ent, int team)
{
	// play powerup spawn sound to all clients
	gentity_t *pm;

	if (ent == NULL)
	{
		G_Printf("Warning: NULL passed to Team_ReturnFlagSound\n");
		return;
	}

	pm                = G_PopupMessage(PM_OBJECTIVE);
	pm->s.effect3Time = G_StringIndex(ent->message);
	pm->s.effect2Time = team;
	pm->s.density     = 1; // 1 = returned
}

void Team_ReturnFlag(gentity_t *ent)
{
	int team = ent->item->giTag == PW_REDFLAG ? TEAM_AXIS : TEAM_ALLIES;

	Team_ReturnFlagSound(ent, team);
	Team_ResetFlag(ent);
	PrintMsg(NULL, "The %s flag has returned!\n", TeamName(team)); // FIXME: returns RED/BLUE flag ... change to Axis/Allies?
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
==============
*/
void Team_DroppedFlagThink(gentity_t *ent)
{
	if (ent->item->giTag == PW_REDFLAG)
	{
		G_Script_ScriptEvent(&g_entities[ent->s.otherEntityNum], "trigger", "returned");

		Team_ReturnFlagSound(ent, TEAM_AXIS);
		Team_ResetFlag(ent);

		if (level.gameManager)
		{
			G_Script_ScriptEvent(level.gameManager, "trigger", "axis_object_returned");
		}
	}
	else if (ent->item->giTag == PW_BLUEFLAG)
	{
		G_Script_ScriptEvent(&g_entities[ent->s.otherEntityNum], "trigger", "returned");

		Team_ReturnFlagSound(ent, TEAM_ALLIES);
		Team_ResetFlag(ent);

		if (level.gameManager)
		{
			G_Script_ScriptEvent(level.gameManager, "trigger", "allied_object_returned");
		}
	}
	// Reset Flag will delete this entity
}

int Team_TouchOurFlag(gentity_t *ent, gentity_t *other, int team)
{
	gclient_t *cl = other->client;

	if (ent->flags & FL_DROPPED_ITEM)
	{
		// hey, its not home.  return it by teleporting it back
		AddScore(other, WOLF_SECURE_OBJ_BONUS);

		if (cl->sess.sessionTeam == TEAM_AXIS)
		{
			if (level.gameManager)
			{
				G_Script_ScriptEvent(level.gameManager, "trigger", "axis_object_returned");
			}
			G_Script_ScriptEvent(&g_entities[ent->s.otherEntityNum], "trigger", "returned");
#ifdef FEATURE_OMNIBOT
			{
				const char *pName = ent->message ? ent->message : _GetEntityName(ent);
				Bot_Util_SendTrigger(ent, NULL, va("Axis have returned %s!", pName ? pName : ""), "returned");
			}
#endif
		}
		else
		{
			if (level.gameManager)
			{
				G_Script_ScriptEvent(level.gameManager, "trigger", "allied_object_returned");
			}
			G_Script_ScriptEvent(&g_entities[ent->s.otherEntityNum], "trigger", "returned");
#ifdef FEATURE_OMNIBOT
			{
				const char *pName = ent->message ? ent->message : _GetEntityName(ent);
				Bot_Util_SendTrigger(ent, NULL, va("Allies have returned %s!", pName ? pName : ""), "returned");
			}
#endif
		}

		//ResetFlag will remove this entity!  We must return zero
		Team_ReturnFlagSound(ent, team);
		Team_ResetFlag(ent);
		return 0;
	}

	// GT_WOLF doesn't support capturing the flag
	return 0;
}

int Team_TouchEnemyFlag(gentity_t *ent, gentity_t *other, int team)
{
	gclient_t *cl = other->client;
	gentity_t *tmp;

	ent->s.density--;

	// hey, its not our flag, pick it up
	AddScore(other, WOLF_STEAL_OBJ_BONUS);

	tmp         = ent->parent;
	ent->parent = other;

	if (cl->sess.sessionTeam == TEAM_AXIS)
	{
		gentity_t *pm = G_PopupMessage(PM_OBJECTIVE);

		pm->s.effect3Time = G_StringIndex(ent->message);
		pm->s.effect2Time = TEAM_AXIS;
		pm->s.density     = 0; // 0 = stolen

		if (level.gameManager)
		{
			G_Script_ScriptEvent(level.gameManager, "trigger", "allied_object_stolen");
		}
		G_Script_ScriptEvent(ent, "trigger", "stolen");
#ifdef FEATURE_OMNIBOT
		Bot_Util_SendTrigger(ent, NULL, va("Axis have stolen %s!", ent->message), "stolen");
#endif
	}
	else
	{
		gentity_t *pm = G_PopupMessage(PM_OBJECTIVE);

		pm->s.effect3Time = G_StringIndex(ent->message);
		pm->s.effect2Time = TEAM_ALLIES;
		pm->s.density     = 0; // 0 = stolen

		if (level.gameManager)
		{
			G_Script_ScriptEvent(level.gameManager, "trigger", "axis_object_stolen");
		}
		G_Script_ScriptEvent(ent, "trigger", "stolen");
#ifdef FEATURE_OMNIBOT
		Bot_Util_SendTrigger(ent, NULL, va("Allies have stolen %s!", ent->message), "stolen");
#endif
	}

	ent->parent = tmp;

	// reset player disguise on stealing docs
	other->client->ps.powerups[PW_OPS_DISGUISED] = 0;

	if (team == TEAM_AXIS)
	{
		cl->ps.powerups[PW_REDFLAG] = INT_MAX;
	}
	else
	{
		cl->ps.powerups[PW_BLUEFLAG] = INT_MAX;
	} // flags never expire

	// store the entitynum of our original flag spawner
	if (ent->flags & FL_DROPPED_ITEM)
	{
		cl->flagParent = ent->s.otherEntityNum;
	}
	else
	{
		cl->flagParent = ent->s.number;
	}

	other->client->speedScale = ent->splashDamage; // Alter player speed

	if (ent->s.density > 0)
	{
		return 1; // We have more flags to give out, spawn back quickly
	}
	else
	{
		return -1; // Do not respawn this automatically, but do delete it if it was FL_DROPPED
	}
}

int Pickup_Team(gentity_t *ent, gentity_t *other)
{
	int       team;
	gclient_t *cl = other->client;

	// figure out what team this flag is
	if (strcmp(ent->classname, "team_CTF_redflag") == 0)
	{
		team = TEAM_AXIS;
	}
	else if (strcmp(ent->classname, "team_CTF_blueflag") == 0)
	{
		team = TEAM_ALLIES;
	}
	else
	{
		PrintMsg(other, "Don't know what team the flag is on.\n");
		return 0;
	}

	// set flag model in carrying entity if multiplayer and flagmodel is set
	other->message           = ent->message;
	other->s.otherEntityNum2 = ent->s.modelindex2;

	return ((team == cl->sess.sessionTeam) ?
	        Team_TouchOurFlag : Team_TouchEnemyFlag)
	           (ent, other, team);
}

/*---------------------------------------------------------------------------*/

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define MAX_TEAM_SPAWN_POINTS   256
gentity_t *SelectRandomTeamSpawnPoint(int teamstate, team_t team, int spawnObjective)
{
	gentity_t *spot;
	gentity_t *spots[MAX_TEAM_SPAWN_POINTS];
	int       count, closest;
	int       i = 0;
	char      *classname;
	float     shortest, tmp;
	vec3_t    target;
	vec3_t    farthest;

	if (team == TEAM_AXIS)
	{
		classname = "team_CTF_redspawn";
	}
	else if (team == TEAM_ALLIES)
	{
		classname = "team_CTF_bluespawn";
	}
	else
	{
		return NULL;
	}

	count = 0;

	spot = NULL;

	while ((spot = G_Find(spot, FOFS(classname), classname)) != NULL)
	{
		if (SpotWouldTelefrag(spot))
		{
			continue;
		}

		// modified to allow initial spawnpoints to be disabled at gamestart
		if (!(spot->spawnflags & 2))
		{
			continue;
		}

		// invisible entities can't be used for spawning
		if (spot->entstate == STATE_INVISIBLE || spot->entstate == STATE_UNDERCONSTRUCTION)
		{
			continue;
		}

		spots[count] = spot;
		if (++count == MAX_TEAM_SPAWN_POINTS)
		{
			break;
		}
	}

	if (!count)     // no spots that won't telefrag
	{
		spot = NULL;
		while ((spot = G_Find(spot, FOFS(classname), classname)) != NULL)
		{
			// modified to allow initial spawnpoints to be disabled at gamestart
			if (!(spot->spawnflags & 2))
			{
				continue;
			}

			// invisible entities can't be used for spawning
			if (spot->entstate == STATE_INVISIBLE || spot->entstate == STATE_UNDERCONSTRUCTION)
			{
				continue;
			}

			return spot;
		}

		return G_Find(NULL, FOFS(classname), classname);
	}

	if ((!level.numspawntargets))
	{
		G_Error("No spawnpoints found\n");
		return NULL;
	}
	else
	{
		// adding ability to set autospawn
		if (!spawnObjective)
		{
			switch (team)
			{
			case TEAM_AXIS:
				spawnObjective = level.axisAutoSpawn + 1;
				break;
			case TEAM_ALLIES:
				spawnObjective = level.alliesAutoSpawn + 1;
				break;
			default:
				break;
			}
		}

		i = spawnObjective - 1;

		VectorCopy(level.spawntargets[i], farthest);

		// now that we've got farthest vector, figure closest spawnpoint to it
		VectorSubtract(farthest, spots[0]->s.origin, target);
		shortest = VectorLength(target);
		closest  = 0;
		for (i = 0; i < count; i++)
		{
			VectorSubtract(farthest, spots[i]->s.origin, target);
			tmp = VectorLength(target);

			if (tmp < shortest)
			{
				shortest = tmp;
				closest  = i;
			}
		}
		return spots[closest];
	}
}

/*
===========
SelectCTFSpawnPoint
============
*/
gentity_t *SelectCTFSpawnPoint(team_t team, int teamstate, vec3_t origin, vec3_t angles, int spawnObjective)
{
	gentity_t *spot;

	spot = SelectRandomTeamSpawnPoint(teamstate, team, spawnObjective);

	if (!spot)
	{
		return SelectSpawnPoint(vec3_origin, origin, angles);
	}

	VectorCopy(spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy(spot->s.angles, angles);

	return spot;
}

/*---------------------------------------------------------------------------*/

/*
==================
TeamplayLocationsMessage

Format:
    clientNum location health armor weapon powerups
==================
*/

void TeamplayInfoMessage(team_t team)
{
	char      entry[1024];
	char      string[1400];
	int       stringlength = 0;
	int       i, j;
	gentity_t *player;
	int       cnt;
	int       h;
	char      *bufferedData;
	char      *tinfo;

	// send the latest information on all clients
	string[0] = 0;

	for (i = 0, cnt = 0; i < level.numConnectedClients; i++)
	{
		player = g_entities + level.sortedClients[i];
		if (player->inuse && player->client->sess.sessionTeam == team)
		{
			// If in LIMBO, don't show followee's health
			if (player->client->ps.pm_flags & PMF_LIMBO)
			{
				h = -1;
			}
			else
			{
				h = player->client->ps.stats[STAT_HEALTH];
				if (h < 0)
				{
					h = 0;
				}
			}

			Com_sprintf(entry, sizeof(entry), " %i %i %i %i %i", level.sortedClients[i], player->client->pers.teamState.location[0], player->client->pers.teamState.location[1], h, player->s.powerups);

			j = strlen(entry);
			if (stringlength + j > sizeof(string))
			{
				break;
			}
			strcpy(string + stringlength, entry);
			stringlength += j;
			cnt++;
		}
	}

	bufferedData = team == TEAM_AXIS ? level.tinfoAxis : level.tinfoAllies;

	tinfo = va("tinfo %i%s", cnt, string);
	if (!Q_stricmp(bufferedData, tinfo))       // no change so just return
	{
		return;
	}

	Q_strncpyz(bufferedData, tinfo, 1400);

	for (i = 0; i < level.numConnectedClients; i++)
	{
		player = g_entities + level.sortedClients[i];
		if (player->inuse && player->client->sess.sessionTeam == team)
		{
			if (player->client->pers.connected == CON_CONNECTED)
			{
				trap_SendServerCommand(player - g_entities, tinfo);
			}
		}
	}
}

void CheckTeamStatus(void)
{
	gentity_t *ent;

	if (level.time - level.lastTeamLocationTime > TEAM_LOCATION_UPDATE_TIME)
	{
		int i;

		level.lastTeamLocationTime = level.time;
		for (i = 0; i < level.numConnectedClients; i++)
		{
			ent = g_entities + level.sortedClients[i];
			if (ent->inuse && (ent->client->sess.sessionTeam == TEAM_AXIS || ent->client->sess.sessionTeam == TEAM_ALLIES))
			{
				ent->client->pers.teamState.location[0] = (int)ent->r.currentOrigin[0];
				ent->client->pers.teamState.location[1] = (int)ent->r.currentOrigin[1];
			}
		}

		TeamplayInfoMessage(TEAM_AXIS);
		TeamplayInfoMessage(TEAM_ALLIES);
	}
}

/*-----------------------------------------------------------------*/

void Use_Team_Spawnpoint(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	if (ent->spawnflags & 2)
	{
		ent->spawnflags &= ~2;

		if (g_developer.integer)
		{
			G_Printf("setting %s %s inactive\n", ent->classname, ent->targetname);
		}
	}
	else
	{
		ent->spawnflags |= 2;

		if (g_developer.integer)
		{
			G_Printf("setting %s %s active\n", ent->classname, ent->targetname);
		}
	}
}

void DropToFloor(gentity_t *ent);


// edited quaked def
/*QUAKED team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32) ? INVULNERABLE STARTACTIVE
potential spawning position for axis team in wolfdm games.

TODO: SelectRandomTeamSpawnPoint() will choose team_CTF_redspawn point that:

1) has been activated (FL_SPAWNPOINT_ACTIVE)
2) isn't occupied and
3) is closest to team_WOLF_objective

This allows spawnpoints to advance across the battlefield as new ones are
placed and/or activated.

If target is set, point spawnpoint toward target activation
*/
void SP_team_CTF_redspawn(gentity_t *ent)
{
	ent->enemy = G_PickTarget(ent->target);
	if (ent->enemy)
	{
		vec3_t dir;

		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
		vectoangles(dir, ent->s.angles);
	}

	ent->use = Use_Team_Spawnpoint;

	VectorSet(ent->r.mins, -16, -16, -24);
	VectorSet(ent->r.maxs, 16, 16, 32);

	ent->think = DropToFloor;
}

// edited quaked def
/*QUAKED team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32) ? INVULNERABLE STARTACTIVE
potential spawning position for allied team in wolfdm games.

TODO: SelectRandomTeamSpawnPoint() will choose team_CTF_bluespawn point that:

1) has been activated (active)
2) isn't occupied and
3) is closest to selected team_WOLF_objective

This allows spawnpoints to advance across the battlefield as new ones are
placed and/or activated.

If target is set, point spawnpoint toward target activation
*/
void SP_team_CTF_bluespawn(gentity_t *ent)
{
	ent->enemy = G_PickTarget(ent->target);
	if (ent->enemy)
	{
		vec3_t dir;

		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
		vectoangles(dir, ent->s.angles);
	}

	ent->use = Use_Team_Spawnpoint;

	VectorSet(ent->r.mins, -16, -16, -24);
	VectorSet(ent->r.maxs, 16, 16, 32);

	ent->think = DropToFloor;
}

/*QUAKED team_WOLF_objective (1 1 0.3) (-16 -16 -24) (16 16 32) DEFAULT_AXIS DEFAULT_ALLIES
marker for objective

This marker will be used for computing effective radius for
dynamite damage, as well as generating a list of objectives
that players can elect to spawn near to in the limbo spawn
screen.

    "description"   short text key for objective name that will appear in objective selection in limbo UI.

DEFAULT_AXIS - This spawn region belongs to the Axis at the start of the map
DEFAULT_ALLIES - This spawn region belongs to the Alles at the start of the map
*/
static int numobjectives = 0;

void reset_numobjectives(void)
{
	numobjectives = 0;
}

// swaps the team
void team_wolf_objective_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	char cs[MAX_STRING_CHARS];

	// 256 is a disabled flag
	if ((self->count2 & ~256) == TEAM_AXIS)
	{
		self->count2 = (self->count2 & 256) + TEAM_ALLIES;
	}
	else if ((self->count2 & ~256) == TEAM_ALLIES)
	{
		self->count2 = (self->count2 & 256) + TEAM_AXIS;
	}

	// And update configstring
	trap_GetConfigstring(self->count, cs, sizeof(cs));
	Info_SetValueForKey(cs, "spawn_targ", self->message);
	Info_SetValueForKey(cs, "x", va("%i", (int)self->s.origin[0]));
	Info_SetValueForKey(cs, "y", va("%i", (int)self->s.origin[1]));
	Info_SetValueForKey(cs, "t", va("%i", self->count2));
	trap_SetConfigstring(self->count, cs);
}

void objective_Register(gentity_t *self)
{
	char numspawntargets[128];
	int  cs_obj = CS_MULTI_SPAWNTARGETS;
	char cs[MAX_STRING_CHARS];

	if (numobjectives == MAX_MULTI_SPAWNTARGETS)
	{
		G_Error("SP_team_WOLF_objective: exceeded MAX_MULTI_SPAWNTARGETS (%d)\n", MAX_MULTI_SPAWNTARGETS);
	}
	else     // Set config strings
	{
		cs_obj += numobjectives;
		trap_GetConfigstring(cs_obj, cs, sizeof(cs));
		Info_SetValueForKey(cs, "spawn_targ", self->message);
		Info_SetValueForKey(cs, "x", va("%i", (int)self->s.origin[0]));
		Info_SetValueForKey(cs, "y", va("%i", (int)self->s.origin[1]));
		if (level.ccLayers)
		{
			Info_SetValueForKey(cs, "z", va("%i", (int)self->s.origin[2]));
		}
		Info_SetValueForKey(cs, "t", va("%i", self->count2));
		self->use   = team_wolf_objective_use;
		self->count = cs_obj;
		trap_SetConfigstring(cs_obj, cs);
		VectorCopy(self->s.origin, level.spawntargets[numobjectives]);
	}

	numobjectives++;

	// set current # spawntargets
	level.numspawntargets = numobjectives;
	trap_GetConfigstring(CS_MULTI_INFO, cs, sizeof(cs));
	Com_sprintf(numspawntargets, 128, "%d", numobjectives);
	Info_SetValueForKey(cs, "s", numspawntargets); // numspawntargets
	trap_SetConfigstring(CS_MULTI_INFO, cs);
}

void SP_team_WOLF_objective(gentity_t *ent)
{
	char *desc;

	G_SpawnString("description", "WARNING: No objective description set", &desc);

	// wtf is this g_alloced? just use a static buffer fgs...
	ent->message = G_Alloc(strlen(desc) + 1);
	Q_strncpyz(ent->message, desc, strlen(desc) + 1);

	ent->nextthink = level.time + FRAMETIME;
	ent->think     = objective_Register;
	ent->s.eType   = ET_WOLF_OBJECTIVE;

	if (ent->spawnflags & 1)
	{
		ent->count2 = TEAM_AXIS;
	}
	else if (ent->spawnflags & 2)
	{
		ent->count2 = TEAM_ALLIES;
	}
}

// Capture and Hold Checkpoint flag
#define SPAWNPOINT  1
#define CP_HOLD     2
#define AXIS_ONLY   4
#define ALLIED_ONLY 8

void checkpoint_touch(gentity_t *self, gentity_t *other, trace_t *trace);

void checkpoint_use_think(gentity_t *self)
{
	self->count2 = -1;

	if (self->count == TEAM_AXIS)
	{
		self->health = 0;
	}
	else
	{
		self->health = 10;
	}
}

void checkpoint_use(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	int holderteam;
	int time;

	if (!activator->client)
	{
		return;
	}

	if (ent->count < 0)
	{
		checkpoint_touch(ent, activator, NULL);
	}

	holderteam = activator->client->sess.sessionTeam;

	if (ent->count == holderteam)
	{
		return;
	}

	if (ent->count2 == level.time)
	{
		if (holderteam == TEAM_AXIS)
		{
			time = ent->health / 2;
			time++;
			trap_SendServerCommand(activator - g_entities, va("cp \"Flag will be held in %i seconds!\"", time));
		}
		else
		{
			time = (10 - ent->health) / 2;
			time++;
			trap_SendServerCommand(activator - g_entities, va("cp \"Flag will be held in %i seconds!\"", time));
		}
		return;
	}

	if (holderteam == TEAM_AXIS)
	{
		ent->health--;
		if (ent->health < 0)
		{
			checkpoint_touch(ent, activator, NULL);
			return;
		}

		time = ent->health / 2;
		time++;
		trap_SendServerCommand(activator - g_entities, va("cp \"Flag will be held in %i seconds!\"", time));
	}
	else
	{
		ent->health++;
		if (ent->health > 10)
		{
			checkpoint_touch(ent, activator, NULL);
			return;
		}

		time = (10 - ent->health) / 2;
		time++;
		trap_SendServerCommand(activator - g_entities, va("cp \"Flag will be held in %i seconds!\"", time));
	}

	ent->count2    = level.time;
	ent->think     = checkpoint_use_think;
	ent->nextthink = level.time + 2000;

	// reset player disguise on touching flag
	other->client->ps.powerups[PW_OPS_DISGUISED] = 0;
}

void checkpoint_spawntouch(gentity_t *self, gentity_t *other, trace_t *trace);

void checkpoint_hold_think(gentity_t *self)
{
	self->nextthink = level.time + 5000;
}

void checkpoint_think(gentity_t *self)
{
	switch (self->s.frame)
	{
	case WCP_ANIM_NOFLAG:
		break;
	case WCP_ANIM_RAISE_AXIS:
		self->s.frame = WCP_ANIM_AXIS_RAISED;
		break;
	case WCP_ANIM_RAISE_AMERICAN:
		self->s.frame = WCP_ANIM_AMERICAN_RAISED;
		break;
	case WCP_ANIM_AXIS_RAISED:
		break;
	case WCP_ANIM_AMERICAN_RAISED:
		break;
	case WCP_ANIM_AXIS_TO_AMERICAN:
		self->s.frame = WCP_ANIM_AMERICAN_RAISED;
		break;
	case WCP_ANIM_AMERICAN_TO_AXIS:
		self->s.frame = WCP_ANIM_AXIS_RAISED;
		break;
	case WCP_ANIM_AXIS_FALLING:
		self->s.frame = WCP_ANIM_NOFLAG;
		break;
	case WCP_ANIM_AMERICAN_FALLING:
		self->s.frame = WCP_ANIM_NOFLAG;
		break;
	default:
		break;
	}

	if (self->spawnflags & SPAWNPOINT)
	{
		self->touch = checkpoint_spawntouch;
	}
	else if (!(self->spawnflags & CP_HOLD))
	{
		self->touch = checkpoint_touch;
	}
	self->nextthink = 0;
}

void checkpoint_touch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	if (self->count == other->client->sess.sessionTeam)
	{
		return;
	}

	if (self->s.frame == WCP_ANIM_NOFLAG)
	{
		AddScore(other, WOLF_CP_CAPTURE);
	}
	else
	{
		AddScore(other, WOLF_CP_RECOVER);
	}

	// Set controlling team
	self->count = other->client->sess.sessionTeam;

	// Set animation
	if (self->count == TEAM_AXIS)
	{
		if (self->s.frame == WCP_ANIM_NOFLAG)
		{
			self->s.frame = WCP_ANIM_RAISE_AXIS;
		}
		else if (self->s.frame == WCP_ANIM_AMERICAN_RAISED)
		{
			self->s.frame = WCP_ANIM_AMERICAN_TO_AXIS;
		}
		else
		{
			self->s.frame = WCP_ANIM_AXIS_RAISED;
		}
	}
	else
	{
		if (self->s.frame == WCP_ANIM_NOFLAG)
		{
			self->s.frame = WCP_ANIM_RAISE_AMERICAN;
		}
		else if (self->s.frame == WCP_ANIM_AXIS_RAISED)
		{
			self->s.frame = WCP_ANIM_AXIS_TO_AMERICAN;
		}
		else
		{
			self->s.frame = WCP_ANIM_AMERICAN_RAISED;
		}
	}

	self->parent = other;

	// reset player disguise on touching flag
	other->client->ps.powerups[PW_OPS_DISGUISED] = 0;
	// Run script trigger
	if (self->count == TEAM_AXIS)
	{
		self->health = 0;
		G_Script_ScriptEvent(self, "trigger", "axis_capture");
	}
	else
	{
		self->health = 10;
		G_Script_ScriptEvent(self, "trigger", "allied_capture");
	}

	// Play a sound
	G_AddEvent(self, EV_GENERAL_SOUND, self->soundPos1);

	// Don't allow touch again until animation is finished
	self->touch = NULL;

	self->think     = checkpoint_think;
	self->nextthink = level.time + 1000;
}

// if spawn flag is set, use this touch fn instead to turn on/off targeted spawnpoints
void checkpoint_spawntouch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	gentity_t *ent      = NULL;
	qboolean  playsound = qtrue;
	qboolean  firsttime = qfalse;
#ifdef FEATURE_OMNIBOT
	char *flagAction = "touch";
#endif

	if (self->count == other->client->sess.sessionTeam)
	{
		return;
	}

	if (self->s.frame == WCP_ANIM_NOFLAG)
	{
		AddScore(other, WOLF_SP_CAPTURE);
	}
	else
	{
		AddScore(other, WOLF_SP_RECOVER);
	}

	if (self->count < 0)
	{
		firsttime = qtrue;
	}

	// Set controlling team
	self->count = other->client->sess.sessionTeam;

	// Set animation
	if (self->count == TEAM_AXIS)
	{
		if (self->s.frame == WCP_ANIM_NOFLAG && !(self->spawnflags & ALLIED_ONLY))
		{
			self->s.frame = WCP_ANIM_RAISE_AXIS;
#ifdef FEATURE_OMNIBOT
			flagAction = "capture";
#endif
		}
		else if (self->s.frame == WCP_ANIM_NOFLAG)
		{
			self->s.frame = WCP_ANIM_NOFLAG;
			playsound     = qfalse;
		}
		else if (self->s.frame == WCP_ANIM_AMERICAN_RAISED && !(self->spawnflags & ALLIED_ONLY))
		{
			self->s.frame = WCP_ANIM_AMERICAN_TO_AXIS;
#ifdef FEATURE_OMNIBOT
			flagAction = "reclaims";
#endif
		}
		else if (self->s.frame == WCP_ANIM_AMERICAN_RAISED)
		{
			self->s.frame = WCP_ANIM_AMERICAN_FALLING;
#ifdef FEATURE_OMNIBOT
			flagAction = "neutralized";
#endif
		}
		else
		{
			self->s.frame = WCP_ANIM_AXIS_RAISED;
		}
	}
	else
	{
		if (self->s.frame == WCP_ANIM_NOFLAG && !(self->spawnflags & AXIS_ONLY))
		{
			self->s.frame = WCP_ANIM_RAISE_AMERICAN;
#ifdef FEATURE_OMNIBOT
			flagAction = "capture";
#endif
		}
		else if (self->s.frame == WCP_ANIM_NOFLAG)
		{
			self->s.frame = WCP_ANIM_NOFLAG;
			playsound     = qfalse;
		}
		else if (self->s.frame == WCP_ANIM_AXIS_RAISED && !(self->spawnflags & AXIS_ONLY))
		{
			self->s.frame = WCP_ANIM_AXIS_TO_AMERICAN;
#ifdef FEATURE_OMNIBOT
			flagAction = "reclaims";
#endif
		}
		else if (self->s.frame == WCP_ANIM_AXIS_RAISED)
		{
			self->s.frame = WCP_ANIM_AXIS_FALLING;
#ifdef FEATURE_OMNIBOT
			flagAction = "neutralized";
#endif
		}
		else
		{
			self->s.frame = WCP_ANIM_AMERICAN_RAISED;
		}
	}

	// If this is the first time it's being touched, and it was the opposing team
	// touching a single-team reinforcement flag... don't do anything.
	if (firsttime && !playsound)
	{
		return;
	}

	// Play a sound
	if (playsound)
	{
		G_AddEvent(self, EV_GENERAL_SOUND, self->soundPos1);
	}

	self->parent = other;

	// reset player disguise on touching flag
	other->client->ps.powerups[PW_OPS_DISGUISED] = 0;
	// Run script trigger
	if (self->count == TEAM_AXIS)
	{
		G_Script_ScriptEvent(self, "trigger", "axis_capture");
#ifdef FEATURE_OMNIBOT
		Bot_Util_SendTrigger(self, NULL, va("axis_%s_%s", flagAction, _GetEntityName(self)), flagAction);
#endif
	}
	else
	{
		G_Script_ScriptEvent(self, "trigger", "allied_capture");
#ifdef FEATURE_OMNIBOT
		Bot_Util_SendTrigger(self, NULL, va("allies_%s_%s", flagAction, _GetEntityName(self)), flagAction);
#endif
	}

	// Don't allow touch again until animation is finished
	self->touch = NULL;

	self->think     = checkpoint_think;
	self->nextthink = level.time + 1000;

	// activate all targets
	// updated this to allow toggling of initial spawnpoints as well, plus now it only
	// toggles spawnflags 2 for spawnpoint entities
	if (self->target)
	{
		while (1)
		{
			ent = G_FindByTargetname(ent, self->target);
			if (!ent)
			{
				break;
			}
			if (other->client->sess.sessionTeam == TEAM_AXIS)
			{
				if (!strcmp(ent->classname, "team_CTF_redspawn"))
				{
					ent->spawnflags |= 2;
				}
				else if (!strcmp(ent->classname, "team_CTF_bluespawn"))
				{
					ent->spawnflags &= ~2;
				}
			}
			else
			{
				if (!strcmp(ent->classname, "team_CTF_bluespawn"))
				{
					ent->spawnflags |= 2;
				}
				else if (!strcmp(ent->classname, "team_CTF_redspawn"))
				{
					ent->spawnflags &= ~2;
				}
			}
		}
	}
}

/*QUAKED team_WOLF_checkpoint (.9 .3 .9) (-16 -16 0) (16 16 128) SPAWNPOINT CP_HOLD AXIS_ONLY ALLIED_ONLY
This is the flagpole players touch in Capture and Hold game scenarios.

It will call specific trigger funtions in the map script for this object.
When allies capture, it will call "allied_capture".
When axis capture, it will call "axis_capture".

if spawnpoint flag is set, think will turn on spawnpoints (specified as targets)
for capture team and turn *off* targeted spawnpoints for opposing team
*/
void SP_team_WOLF_checkpoint(gentity_t *ent)
{
	char *capture_sound;

	if (!ent->scriptName)
	{
		G_Error("team_WOLF_checkpoint must have a \"scriptname\"\n");
	}

	// Make sure the ET_TRAP entity type stays valid
	ent->s.eType = ET_TRAP;

	// Model is user assignable, but it will always try and use the animations for flagpole.md3
	if (ent->model)
	{
		ent->s.modelindex = G_ModelIndex(ent->model);
	}
	else
	{
		ent->s.modelindex = G_ModelIndex("models/multiplayer/flagpole/flagpole.md3");
	}

	G_SpawnString("noise", "sound/movers/doors/door6_open.wav", &capture_sound);
	ent->soundPos1 = G_SoundIndex(capture_sound);

	ent->clipmask   = CONTENTS_SOLID;
	ent->r.contents = CONTENTS_SOLID;

	VectorSet(ent->r.mins, -8, -8, 0);
	VectorSet(ent->r.maxs, 8, 8, 128);

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngle(ent, ent->s.angles);

	// s.frame is the animation number
	ent->s.frame = WCP_ANIM_NOFLAG;

	// s.teamNum is which set of animations to use ( only 1 right now )
	ent->s.teamNum = 1;

	// Used later to set animations (and delay between captures)
	ent->nextthink = 0;

	// Used to time how long it must be "held" to switch
	ent->health = -1;
	ent->count2 = -1;

	// 'count' signifies which team holds the checkpoint
	ent->count = -1;

	if (ent->spawnflags & SPAWNPOINT)
	{
		ent->touch = checkpoint_spawntouch;
	}
	else
	{
		if (ent->spawnflags & CP_HOLD)
		{
			ent->use = checkpoint_use;
		}
		else
		{
			ent->touch = checkpoint_touch;
		}
	}

	trap_LinkEntity(ent);
}

/**
 * @note Unused
 */
int Team_ClassForString(char *string)
{
	if (!Q_stricmp(string, "soldier"))
	{
		return PC_SOLDIER;
	}
	else if (!Q_stricmp(string, "medic"))
	{
		return PC_MEDIC;
	}
	else if (!Q_stricmp(string, "engineer"))
	{
		return PC_ENGINEER;
	}
	else if (!Q_stricmp(string, "fieldops"))
	{
		return PC_FIELDOPS;
	}
	else if (!Q_stricmp(string, "covertops"))
	{
		return PC_COVERTOPS;
	}
	return -1;
}

char      *aTeams[TEAM_NUM_TEAMS] = { "FFA", "^1Axis^7", "^4Allies^7", "^2Spectators^7" };
team_info teamInfo[TEAM_NUM_TEAMS];

// Resets a team's settings
void G_teamReset(int team_num, qboolean fClearSpecLock)
{
	teamInfo[team_num].team_lock    = (match_latejoin.integer == 0 && g_gamestate.integer == GS_PLAYING);
	teamInfo[team_num].team_name[0] = 0;
	teamInfo[team_num].team_score   = 0;
	teamInfo[team_num].timeouts     = match_timeoutcount.integer;

	if (fClearSpecLock)
	{
		teamInfo[team_num].spec_lock = qfalse;
	}
}

// Swaps active players on teams
void G_swapTeams(void)
{
	int       i;
	gclient_t *cl;

	for (i = TEAM_AXIS; i <= TEAM_ALLIES; i++)
	{
		G_teamReset(i, qtrue);
	}

	for (i = 0; i < level.numConnectedClients; i++)
	{
		cl = level.clients + level.sortedClients[i];

		if (cl->sess.sessionTeam == TEAM_AXIS)
		{
			cl->sess.sessionTeam = TEAM_ALLIES;
		}
		else if (cl->sess.sessionTeam == TEAM_ALLIES)
		{
			cl->sess.sessionTeam = TEAM_AXIS;
		}
		else
		{
			continue;
		}

		G_UpdateCharacter(cl);
		ClientUserinfoChanged(level.sortedClients[i]);
		ClientBegin(level.sortedClients[i]);
	}

	AP("cp \"^1Teams have been swapped!\n\"");
}

int QDECL G_SortPlayersByXP(const void *a, const void *b)
{
	gclient_t *cla = &level.clients[*((int *)a)];
	gclient_t *clb = &level.clients[*((int *)b)];

	if (cla->ps.persistant[PERS_SCORE] > clb->ps.persistant[PERS_SCORE])
	{
		return -1;
	}
	if (clb->ps.persistant[PERS_SCORE] > cla->ps.persistant[PERS_SCORE])
	{
		return 1;
	}

	return 0;
}

// Shuffle active players onto teams
void G_shuffleTeams(void)
{
	int       i, cTeam; //, cMedian = level.numNonSpectatorClients / 2;
	int       cnt = 0;
	int       sortClients[MAX_CLIENTS];
	gclient_t *cl;

	G_teamReset(TEAM_AXIS, qtrue);
	G_teamReset(TEAM_ALLIES, qtrue);

	for (i = 0; i < level.numConnectedClients; i++)
	{
		cl = level.clients + level.sortedClients[i];

		if (cl->sess.sessionTeam != TEAM_AXIS && cl->sess.sessionTeam != TEAM_ALLIES)
		{
			continue;
		}

		sortClients[cnt++] = level.sortedClients[i];
	}

	qsort(sortClients, cnt, sizeof(int), G_SortPlayersByXP);

	for (i = 0; i < cnt; i++)
	{
		cl = level.clients + sortClients[i];

		//	cTeam = (i % 2) + TEAM_AXIS;
		cTeam = (((i + 1) % 4) - ((i + 1) % 2)) / 2 + TEAM_AXIS;

		if (cl->sess.sessionTeam != cTeam)
		{
			G_LeaveTank(g_entities + sortClients[i], qfalse);
			G_RemoveClientFromFireteams(sortClients[i], qtrue, qfalse);
			if (g_landminetimeout.integer)
			{
				G_ExplodeMines(g_entities + sortClients[i]);
			}
			G_FadeItems(g_entities + sortClients[i], MOD_SATCHEL);
		}

		cl->sess.sessionTeam = cTeam;

		G_UpdateCharacter(cl);
		ClientUserinfoChanged(sortClients[i]);
		ClientBegin(sortClients[i]);
	}

	AP("cp \"^1Teams have been shuffled!\n\"");
}

// Returns player's "real" team.
int G_teamID(gentity_t *ent)
{
	if (ent->client->sess.coach_team)
	{
		return(ent->client->sess.coach_team);
	}
	return(ent->client->sess.sessionTeam);
}

// Determine if the "ready" player threshold has been reached.
qboolean G_checkReady(void)
{
	int       ready = 0, notReady = match_minplayers.integer;
	gclient_t *cl;

	if (0 == g_doWarmup.integer)
	{
		return qtrue;
	}

	// Ensure we have enough real players
	if (level.numNonSpectatorClients >= match_minplayers.integer && level.voteInfo.numVotingClients > 0)
	{
		int i;

		// Step through all active clients
		notReady = 0;
		for (i = 0; i < level.numConnectedClients; i++)
		{
			cl = level.clients + level.sortedClients[i];

			if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR)
			{
				continue;
			}
			else if (cl->pers.ready || (g_entities[level.sortedClients[i]].r.svFlags & SVF_BOT))
			{
				ready++;
			}
			else
			{
				notReady++;
			}
		}
	}

	notReady = (notReady > 0 || ready > 0) ? notReady : match_minplayers.integer;
	if (g_minGameClients.integer != notReady)
	{
		trap_Cvar_Set("g_minGameClients", va("%d", notReady));
	}

	// Do we have enough "ready" players?
	return(level.ref_allready || ((ready + notReady > 0) && 100 * ready / (ready + notReady) >= match_readypercent.integer));
}

// Checks ready states to start/stop the sequence to get the match rolling.
qboolean G_readyMatchState(void)
{
	if ((g_doWarmup.integer ||
	     (g_gametype.integer == GT_WOLF_LMS && g_lms_lockTeams.integer) ||
	     level.warmupTime > (level.time + 10 * 1000)) &&
	    g_gamestate.integer == GS_WARMUP && G_checkReady())
	{
		level.ref_allready = qfalse;
		if (g_doWarmup.integer > 0 || (g_gametype.integer == GT_WOLF_LMS && g_lms_lockTeams.integer))
		{
			teamInfo[TEAM_AXIS].team_lock   = qtrue;
			teamInfo[TEAM_ALLIES].team_lock = qtrue;
		}

		return qtrue;

	}
	else if (!G_checkReady())
	{
		if (g_gamestate.integer == GS_WARMUP_COUNTDOWN)
		{
			AP("cp \"^1COUNTDOWN STOPPED!^7  Back to warmup...\n\"");
		}
		level.lastRestartTime = level.time;
		trap_SendConsoleCommand(EXEC_APPEND, va("map_restart 0 %i\n", GS_WARMUP));
	}

	return qfalse;
}

// Check if we need to reset the game state due to an empty team
void G_verifyMatchState(int nTeam)
{
	if ((level.lastRestartTime + 1000) < level.time && (nTeam == TEAM_ALLIES || nTeam == TEAM_AXIS) &&
	    (g_gamestate.integer == GS_PLAYING || g_gamestate.integer == GS_WARMUP_COUNTDOWN || g_gamestate.integer == GS_INTERMISSION))
	{
		if (TeamCount(-1, nTeam) == 0)
		{
			if (g_doWarmup.integer > 0)
			{
				level.lastRestartTime = level.time;
				if (g_gametype.integer == GT_WOLF_STOPWATCH)
				{
					trap_Cvar_Set("g_currentRound", "0");
					trap_Cvar_Set("g_nextTimeLimit", "0");
				}

				trap_SendConsoleCommand(EXEC_APPEND, va("map_restart 0 %i\n", GS_WARMUP));

			}
			else
			{
				teamInfo[nTeam].team_lock = qfalse;
			}

			G_teamReset(nTeam, qtrue);
		}
	}

	// Cleanup of ready count
	G_checkReady();
}

// Checks to see if a specified team is allowing players to join.
qboolean G_teamJoinCheck(int team_num, gentity_t *ent)
{
	int cnt = TeamCount(-1, team_num);

	// Sanity check
	if (cnt == 0)
	{
		G_teamReset(team_num, qtrue);
		teamInfo[team_num].team_lock = qfalse;
	}

	// Check for locked teams
	if ((team_num == TEAM_AXIS || team_num == TEAM_ALLIES))
	{
		if (ent->client->sess.sessionTeam == team_num)
		{
			return qtrue;
		}

		if (g_gametype.integer != GT_WOLF_LMS)
		{
			// Check for full teams
			if (team_maxplayers.integer > 0 && team_maxplayers.integer <= cnt)
			{
				G_printFull(va("The %s team is full!", aTeams[team_num]), ent);
				return qfalse;

				// Check for locked teams
			}
			else if (teamInfo[team_num].team_lock && (!(ent->client->pers.invite & team_num)))
			{
				G_printFull(va("The %s team is LOCKED!", aTeams[team_num]), ent);
				return qfalse;
			}
		}
		else
		{
			if (team_maxplayers.integer > 0 && team_maxplayers.integer <= cnt)
			{
				G_printFull(va("The %s team is full!", aTeams[team_num]), ent);
				return qfalse;
			}
			else if (g_gamestate.integer == GS_PLAYING && g_lms_lockTeams.integer && (!(ent->client->pers.invite & team_num)))
			{
				G_printFull(va("The %s team is LOCKED!", aTeams[team_num]), ent);
				return qfalse;
			}
		}
	}

	return qtrue;
}

// Update specs for blackout, as needed
void G_updateSpecLock(int nTeam, qboolean fLock)
{
	int       i;
	gentity_t *ent;

	teamInfo[nTeam].spec_lock = fLock;
	for (i = 0; i < level.numConnectedClients; i++)
	{
		ent = g_entities + level.sortedClients[i];

		if (ent->client->sess.referee)
		{
			continue;
		}
		if (ent->client->sess.coach_team)
		{
			continue;
		}

		ent->client->sess.spec_invite &= ~nTeam;

		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			continue;
		}

		if (!fLock)
		{
			continue;
		}

#ifdef FEATURE_MULTIVIEW
		if (ent->client->pers.mvCount > 0)
		{
			G_smvRemoveInvalidClients(ent, nTeam);
		}
		else
#endif
		if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW)
		{
			StopFollowing(ent);
			ent->client->sess.spec_team &= ~nTeam;
		}

#ifdef FEATURE_MULTIVIEW
		// ClientBegin sets blackout
		if (ent->client->pers.mvCount < 1)
		{
#endif
		SetTeam(ent, "s", qtrue, -1, -1, qfalse);
#ifdef FEATURE_MULTIVIEW
	}
#endif
	}
}

// Swap team speclocks
void G_swapTeamLocks(void)
{
	qboolean fLock = teamInfo[TEAM_AXIS].spec_lock;
	teamInfo[TEAM_AXIS].spec_lock   = teamInfo[TEAM_ALLIES].spec_lock;
	teamInfo[TEAM_ALLIES].spec_lock = fLock;

	fLock                           = teamInfo[TEAM_AXIS].team_lock;
	teamInfo[TEAM_AXIS].team_lock   = teamInfo[TEAM_ALLIES].team_lock;
	teamInfo[TEAM_ALLIES].team_lock = fLock;
}

// Removes everyone's specinvite for a particular team.
void G_removeSpecInvite(int team)
{
	int i;
	gentity_t *cl;

	for (i = 0; i < level.numConnectedClients; i++)
	{
		cl = g_entities + level.sortedClients[i];
		if (!cl->inuse || cl->client->sess.referee || cl->client->sess.coach_team == team)
		{
			continue;
		}

		cl->client->sess.spec_invite &= ~team;  // none = 0, red = 1, blue = 2
	}
}

// Return blockout status for a player
int G_blockoutTeam(gentity_t *ent, int nTeam)
{
	return(!G_allowFollow(ent, nTeam));
}

// Figure out if we are allowed/want to follow a given player
qboolean G_allowFollow(gentity_t *ent, int nTeam)
{
	if (g_gametype.integer == GT_WOLF_LMS && g_lms_followTeamOnly.integer)
	{
		if ((ent->client->sess.spec_invite & nTeam) == nTeam)
		{
			return qtrue;
		}
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
		    ent->client->sess.sessionTeam != nTeam)
		{
			return qfalse;
		}
	}

	if (level.time - level.startTime > 2500)
	{
		if (TeamCount(-1, TEAM_AXIS) == 0)
		{
			teamInfo[TEAM_AXIS].spec_lock = qfalse;
		}
		if (TeamCount(-1, TEAM_ALLIES) == 0)
		{
			teamInfo[TEAM_ALLIES].spec_lock = qfalse;
		}
	}

	return((!teamInfo[nTeam].spec_lock || ent->client->sess.sessionTeam != TEAM_SPECTATOR || (ent->client->sess.spec_invite & nTeam) == nTeam));
}

// Figure out if we are allowed/want to follow a given player
qboolean G_desiredFollow(gentity_t *ent, int nTeam)
{
	if (G_allowFollow(ent, nTeam) &&
	    (ent->client->sess.spec_team == 0 || ent->client->sess.spec_team == nTeam))
	{
		return qtrue;
	}

	return qfalse;
}
