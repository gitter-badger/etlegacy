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
 * @file cg_fireteamoverlay.c
 */

#include "cg_local.h"

static int sortedFireTeamClients[MAX_CLIENTS];

int QDECL CG_SortFireTeam(const void *a, const void *b)
{
	clientInfo_t *ca, *cb;
	int          cna, cnb;

	cna = *(int *)a;
	cnb = *(int *)b;

	ca = &cgs.clientinfo[cna];
	cb = &cgs.clientinfo[cnb];

	// not on our team, so shove back
	if (!CG_IsOnSameFireteam(cnb, cg.clientNum))
	{
		return -1;
	}
	if (!CG_IsOnSameFireteam(cna, cg.clientNum))
	{
		return 1;
	}

	// leader comes first
	if (CG_IsFireTeamLeader(cna))
	{
		return -1;
	}
	if (CG_IsFireTeamLeader(cnb))
	{
		return 1;
	}

	// then higher ranks
	if (ca->rank > cb->rank)
	{
		return -1;
	}
	if (cb->rank > ca->rank)
	{
		return 1;
	}

	// then score
	//if ( ca->score > cb->score ) {
	//  return -1;
	//}
	//if ( cb->score > ca->score ) {
	//  return 1;
	//}                                                                                                                       // not atm

	return 0;
}

// sorts client's fireteam by leader then rank
void CG_SortClientFireteam()
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		sortedFireTeamClients[i] = i;
	}

	qsort(sortedFireTeamClients, MAX_CLIENTS, sizeof(sortedFireTeamClients[0]), CG_SortFireTeam);

	// debug
	//for(i = 0; i < MAX_CLIENTS; i++) {
	//	CG_Printf( "%i ", sortedFireTeamClients[i] );
	//}
	//C/G_Printf( "\n" );
}

// parses fireteam servercommand
void CG_ParseFireteams()
{
	int        i, j;
	char       *s;
	const char *p;
	int        clnts[2];

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		cgs.clientinfo[i].fireteamData = NULL;
	}

	for (i = 0; i < MAX_FIRETEAMS; i++)
	{
		char hexbuffer[11] = "0x00000000";

		p = CG_ConfigString(CS_FIRETEAMS + i);

		/*      s = Info_ValueForKey(p, "n");
		        if(!s || !*s) {
		            cg.fireTeams[i].inuse = qfalse;
		            continue;
		        } else {
		            cg.fireTeams[i].inuse = qtrue;
		        }*/

		//Q_strncpyz(cg.fireTeams[i].name, s, 32);
		//CG_Printf("Fireteam: %s\n", cg.fireTeams[i].name);

		j = atoi(Info_ValueForKey(p, "id"));
		if (j == -1)
		{
			cg.fireTeams[i].inuse = qfalse;
			continue;
		}
		else
		{
			cg.fireTeams[i].inuse = qtrue;
			cg.fireTeams[i].ident = j;
		}

		s                      = Info_ValueForKey(p, "l");
		cg.fireTeams[i].leader = atoi(s);

		s                    = Info_ValueForKey(p, "p");
		cg.fireTeams[i].priv = atoi(s);

		s = Info_ValueForKey(p, "c");
		Q_strncpyz(hexbuffer + 2, s, 9);
		sscanf(hexbuffer, "%x", &clnts[1]);
		Q_strncpyz(hexbuffer + 2, s + 8, 9);
		sscanf(hexbuffer, "%x", &clnts[0]);

		for (j = 0; j < cgs.maxclients; j++)
		{
			if (COM_BitCheck(clnts, j))
			{
				cg.fireTeams[i].joinOrder[j]   = qtrue;
				cgs.clientinfo[j].fireteamData = &cg.fireTeams[i];
				//CG_Printf("%s\n", cgs.clientinfo[j].name);
			}
			else
			{
				cg.fireTeams[i].joinOrder[j] = qfalse;
			}
		}
	}

	CG_SortClientFireteam();
}

// Fireteam that both specified clients are on, if they both are on the same team
fireteamData_t *CG_IsOnSameFireteam(int clientNum, int clientNum2)
{
	if (CG_IsOnFireteam(clientNum) == CG_IsOnFireteam(clientNum2))
	{
		return CG_IsOnFireteam(clientNum);
	}

	return NULL;
}

// Fireteam that specified client is leader of, or NULL if none
fireteamData_t *CG_IsFireTeamLeader(int clientNum)
{
	fireteamData_t *f;

	if (!(f = CG_IsOnFireteam(clientNum)))
	{
		return NULL;
	}

	if (f->leader != clientNum)
	{
		return NULL;
	}

	return f ;
}

// Client, not on a fireteam, not sorted, but on your team
clientInfo_t *CG_ClientInfoForPosition(int pos, int max)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_CLIENTS && cnt < max; i++)
	{
		if (cg.clientNum != i && cgs.clientinfo[i].infoValid && !CG_IsOnFireteam(i) && cgs.clientinfo[cg.clientNum].team == cgs.clientinfo[i].team)
		{
			if (cnt == pos)
			{
				return &cgs.clientinfo[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Fireteam, that's on your same team
fireteamData_t *CG_FireTeamForPosition(int pos, int max)
{
	int i, cnt = 0;

	for (i = 0; i < MAX_FIRETEAMS && cnt < max; i++)
	{
		if (cg.fireTeams[i].inuse && cgs.clientinfo[cg.fireTeams[i].leader].team == cgs.clientinfo[cg.clientNum].team)
		{
			if (cnt == pos)
			{
				return &cg.fireTeams[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Client, not sorted by rank, on CLIENT'S fireteam
clientInfo_t *CG_FireTeamPlayerForPosition(int pos, int max)
{
	int            i, cnt = 0;
	fireteamData_t *f = CG_IsOnFireteam(cg.clientNum);

	if (!f)
	{
		return NULL;
	}

	for (i = 0; i < MAX_CLIENTS && cnt < max; i++)
	{
		if (cgs.clientinfo[i].infoValid && cgs.clientinfo[cg.clientNum].team == cgs.clientinfo[i].team)
		{
			if (!(f == CG_IsOnFireteam(i)))
			{
				continue;
			}

			if (cnt == pos)
			{
				return &cgs.clientinfo[i];
			}
			cnt++;
		}
	}

	return NULL;
}

// Client, sorted by rank, on CLIENT'S fireteam
clientInfo_t *CG_SortedFireTeamPlayerForPosition(int pos)
{
	int            i;
	int            cnt = 0;
	fireteamData_t *f  = CG_IsOnFireteam(cg.clientNum);

	if (!f)
	{
		return NULL;
	}

	for (i = 0; i < cgs.maxclients && cnt < MAX_FIRETEAM_MEMBERS; i++)
	{
		if (!(f == CG_IsOnFireteam(sortedFireTeamClients[i])))
		{
			return NULL;
		}

		if (cnt == pos)
		{
			return &cgs.clientinfo[sortedFireTeamClients[i]];
		}
		cnt++;
	}

	return NULL;
}

// Main Functions

#define FT_BAR_YSPACING 2.f
#define FT_BAR_HEIGHT   10.f

/*
CG_DrawFireTeamOverlay
    based on NQ CG_DrawFireTeamOverlay
*/

static vec4_t clr1 = { 0.0f, 0.0f, 0.0f, 0.8f };                // box itselfs - { 0.16f,	0.2f,	0.17f,	0.8f }
static vec4_t clr2 = { 0.0f, 0.0f, 0.0f, 0.2f };                // not selected
static vec4_t clr3 = { 0.25f, 0.0f, 0.0f, 0.6f };               // selected member
static vec4_t tclr = { 0.6f, 0.6f, 0.6f, 1.0f };
static vec4_t bgColor = { 0.0f, 0.0f, 0.0f, 0.6f };       // window
static vec4_t borderColor = { 0.5f, 0.5f, 0.5f, 0.5f };   // window

#define MIN_BORDER_DISTANCE 10 //Currently this if is used for X axis only

// FIXME: add more options to shorten this box
void CG_DrawFireTeamOverlay(rectDef_t *rect)
{
	int            x = rect->x;
	int            y = rect->y + 1;             // +1, jitter it into place in 1024 :)
	int            i, locwidth, namewidth, puwidth, lineX;
	int            boxWidth      = 90;
	int            bestNameWidth = -1;
	int            bestLocWidth  = -1;
	char           buffer[64];
	float          h   = 16;                    // 12 + 2 + 2
	clientInfo_t   *ci = NULL;
	fireteamData_t *f  = NULL;
	char           *locStr[MAX_FIRETEAM_MEMBERS];
	vec3_t         origin;

	int curWeap;

	// assign fireteam data, and early out if not on one
	if (!(f = CG_IsOnFireteam(cg.clientNum)))
	{
		return;
	}

	memset(locStr, 0, sizeof(char *) * MAX_FIRETEAM_MEMBERS);

	// First get name and location width, also store location names
	for (i = 0; i < MAX_FIRETEAM_MEMBERS; i++)
	{
		ci = CG_SortedFireTeamPlayerForPosition(i);

		// Make sure it's valid
		if (!ci)
		{
			break;
		}

		if (cg_locations.integer & LOC_FTEAM)
		{
			origin[0] = ci->location[0];
			origin[1] = ci->location[1];

			locStr[i] = CG_BuildLocationString(ci->clientNum, origin, LOC_FTEAM);

			if (!locStr[i][1] || !*locStr[i])
			{
				locStr[i] = "";
			}

			locwidth = CG_Text_Width_Ext(locStr[i], 0.2f, 0, &cgs.media.limboFont2);
		}
		else
		{
			locwidth = 0;
		}

		namewidth = CG_Text_Width_Ext(ci->name, 0.2f, 0, &cgs.media.limboFont2);

		if (ci->powerups & ((1 << PW_REDFLAG) | (1 << PW_BLUEFLAG) | (1 << PW_OPS_DISGUISED)))
		{
			namewidth += 14;
		}

		if (namewidth > bestNameWidth)
		{
			bestNameWidth = namewidth;
		}

		if (locwidth > bestLocWidth)
		{
			bestLocWidth = locwidth;
		}

		h += 12.f;
	}

	boxWidth += bestLocWidth + bestNameWidth;

	if (cg_fireteamLatchedClass.integer)
	{
		boxWidth += 28;
	}

	if ((Ccg_WideX(640) - MIN_BORDER_DISTANCE) < (x + boxWidth))
	{
		float realw = Ccg_WideX(640);
		x = x - ((x + boxWidth) - realw) - MIN_BORDER_DISTANCE;
	}
	else if (x < MIN_BORDER_DISTANCE)
	{
		x = MIN_BORDER_DISTANCE;
	}

	CG_DrawRect(x, y, boxWidth, h, 1, borderColor);
	CG_FillRect(x + 1, y + 1, boxWidth - 2, h - 2, bgColor);

	x += 2;
	y += 2;

	CG_FillRect(x, y, boxWidth - 4, 12, clr1);

	if (f->priv)
	{
		Com_sprintf(buffer, 64, CG_TranslateString("Private Fireteam: %s"), bg_fireteamNames[f->ident]);
	}
	else
	{
		Com_sprintf(buffer, 64, CG_TranslateString("Fireteam: %s"), bg_fireteamNames[f->ident]);
	}

	Q_strupr(buffer);
	CG_Text_Paint_Ext(x + 3, y + FT_BAR_HEIGHT, .19f, .19f, tclr, buffer, 0, 0, 0, &cgs.media.limboFont1);
	x    += 2;
	lineX = x;
	for (i = 0; i < MAX_FIRETEAM_MEMBERS; i++)
	{
		x  = lineX;
		y += FT_BAR_HEIGHT + FT_BAR_YSPACING;
		// grab a pointer to the current player
		ci = CG_SortedFireTeamPlayerForPosition(i);

		// make sure it's valid
		if (!ci)
		{
			break;
		}

		// hilight selected players
		if (ci->selected)
		{
			CG_FillRect(x, y + FT_BAR_YSPACING, boxWidth - 6, FT_BAR_HEIGHT, clr3);
		}
		else
		{
			CG_FillRect(x, y + FT_BAR_YSPACING, boxWidth - 6, FT_BAR_HEIGHT, clr2);
		}

		x += 4;

		// draw class icon in fireteam overlay
		CG_DrawPic(x, y, 12, 12, cgs.media.skillPics[SkillNumForClass(ci->cls)]);
		x += 14;

		if (cg_fireteamLatchedClass.integer && ci->cls != ci->latchedcls)
		{
			// draw the yellow arrow
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, tclr, "^3->", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
			x += 14;
			// draw latched class icon in fireteam overlay
			CG_DrawPic(x, y, 12, 12, cgs.media.skillPics[SkillNumForClass(ci->latchedcls)]);
			x += 14;
		}
		else if (cg_fireteamLatchedClass.integer)
		{
			x += 28;
		}
		// draw the mute-icon in the fireteam overlay..
		//if ( ci->muted ) {
		//	CG_DrawPic( x, y, 12, 12, cgs.media.muteIcon );
		//	x += 14;
		//} else if

		// draw objective icon (if they are carrying one) in fireteam overlay
		if (ci->powerups & ((1 << PW_REDFLAG) | (1 << PW_BLUEFLAG)))
		{
			CG_DrawPic(x, y, 12, 12, cgs.media.objectiveShader);
			x      += 14;
			puwidth = 14;
		}
		// or else draw the disguised icon in fireteam overlay
		else if (ci->powerups & (1 << PW_OPS_DISGUISED))
		{
			CG_DrawPic(x, y, 12, 12, ci->team == TEAM_AXIS ? cgs.media.alliedUniformShader : cgs.media.axisUniformShader);
			x      += 14;
			puwidth = 14;
		}
		// otherwise draw rank icon in fireteam overlay
		else
		{
			//if (ci->rank > 0) CG_DrawPic( x, y, 12, 12, rankicons[ ci->rank ][  ci->team == TEAM_AXIS ? 1 : 0 ][0].shader );
			//x += 14;
			puwidth = 0;
		}

		// draw the player's name
		CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, colorWhite, ci->name, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);

		// add space
		x += 14 + bestNameWidth - puwidth;

		// draw the player's weapon icon
		if (cg.predictedPlayerEntity.currentState.eFlags & EF_MOUNTEDTANK)
		{
			if (cg_entities[cg_entities[cg_entities[cg.snap->ps.clientNum].tagParent].tankparent].currentState.density & 8)
			{
				curWeap = WP_MOBILE_BROWNING;
			}
			else
			{
				curWeap = WP_MOBILE_MG42;
			}
		}
		else if (cg.predictedPlayerEntity.currentState.eFlags & EF_MG42_ACTIVE)
		{
			curWeap = WP_MOBILE_MG42;
		}
		else
		{
			curWeap = cg_entities[ci->clientNum].currentState.weapon;
		}

		// note: WP_NONE is excluded
		if (IS_VALID_WEAPON(curWeap) && cg_weapons[curWeap].weaponIcon[0])     // do not try to draw nothing
		{
			CG_DrawPic(x, y, cg_weapons[curWeap].weaponIconScale * 10, 10, cg_weapons[curWeap].weaponIcon[0]);
		}
		else if (IS_VALID_WEAPON(curWeap) && cg_weapons[curWeap].weaponIcon[1])
		{
			CG_DrawPic(x, y, cg_weapons[curWeap].weaponIconScale * 10, 10, cg_weapons[curWeap].weaponIcon[1]);
		}

		x += 24;

		if (ci->health >= 100)
		{
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, colorGreen, va("%i", ci->health < 0 ? 0 : ci->health), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
			x += 12;
		}
		else if (ci->health >= 10)
		{
			x += 6;
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, ci->health > 80 ? colorGreen : colorYellow, va("%i", ci->health < 0 ? 0 : ci->health), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
			x += 6;
		}
		else if (ci->health > 0)
		{
			x += 12;
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, colorRed, va("%i", ci->health < 0 ? 0 : ci->health), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
		}
		else if (ci->health == 0)
		{
			x += 6;
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, ((cg.time % 500) > 250)  ? colorWhite : colorRed, "*", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
			x += 6;
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, ((cg.time % 500) > 250)  ? colorRed : colorWhite, "0", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
		}
		else
		{
			x += 12;
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, colorRed, "0", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
		}
		// set hard limit on width
		x += 12;
		if (cg_locations.integer & LOC_FTEAM)
		{
			CG_Text_Paint_Ext(x, y + FT_BAR_HEIGHT, .2f, .2f, tclr, locStr[i], 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
		}
	}
}

/* unused
qboolean CG_FireteamGetBoxNeedsButtons(void)
{
    if (cgs.applicationEndTime > cg.time)
    {
        if (cgs.applicationClient < 0)
        {
            return qfalse;
        }
        return qtrue;
    }

    if (cgs.invitationEndTime > cg.time)
    {
        if (cgs.invitationClient < 0)
        {
            return qfalse;
        }
        return qtrue;
    }

    if (cgs.propositionEndTime > cg.time)
    {
        if (cgs.propositionClient < 0)
        {
            return qfalse;
        }
        return qtrue;
    }

    return qfalse;
}
*/

/* unused
const char *CG_FireteamGetBoxText(void)
{
	if (cgs.applicationEndTime > cg.time)
	{
		if (cgs.applicationClient == -1)
		{
			return "Sent";
		}

		if (cgs.applicationClient == -2)
		{
			return "Failed";
		}

		if (cgs.applicationClient == -3)
		{
			return "Accepted";
		}

		if (cgs.applicationClient == -4)
		{
			return "Sent";
		}

		if (cgs.applicationClient < 0)
		{
			return NULL;
		}

		return va("Accept application from %s?", cgs.clientinfo[cgs.applicationClient].name);
	}

	if (cgs.invitationEndTime > cg.time)
	{
		if (cgs.invitationClient == -1)
		{
			return "Sent";
		}

		if (cgs.invitationClient == -2)
		{
			return "Failed";
		}

		if (cgs.invitationClient == -3)
		{
			return "Accepted";
		}

		if (cgs.invitationClient == -4)
		{
			return "Sent";
		}

		if (cgs.invitationClient < 0)
		{
			return NULL;
		}

		return va("Accept invitiation from %s?", cgs.clientinfo[cgs.invitationClient].name);
	}

	if (cgs.propositionEndTime > cg.time)
	{
		if (cgs.propositionClient == -1)
		{
			return "Sent";
		}

		if (cgs.propositionClient == -2)
		{
			return "Failed";
		}

		if (cgs.propositionClient == -3)
		{
			return "Accepted";
		}

		if (cgs.propositionClient == -4)
		{
			return "Sent";
		}

		if (cgs.propositionClient < 0)
		{
			return NULL;
		}

		return va("Accept %s's proposition to invite %s to join your fireteam?", cgs.clientinfo[cgs.propositionClient2].name, cgs.clientinfo[cgs.propositionClient].name);
	}

	return NULL;
}
*/

qboolean CG_FireteamHasClass(int classnum, qboolean selectedonly)
{
	fireteamData_t *ft;
	int            i;

	if (!(ft = CG_IsOnFireteam(cg.clientNum)))
	{
		return qfalse;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		/*      if( i == cgs.clientinfo ) {
		            continue;
		        }*/

		if (!cgs.clientinfo[i].infoValid)
		{
			continue;
		}

		if (ft != CG_IsOnFireteam(i))
		{
			continue;
		}

		if (cgs.clientinfo[i].cls != classnum)
		{
			continue;
		}

		if (selectedonly && !cgs.clientinfo[i].selected)
		{
			continue;
		}

		return qtrue;
	}

	return qfalse;
}

const char *CG_BuildSelectedFirteamString(void)
{
	char         buffer[256];
	clientInfo_t *ci;
	int          cnt = 0;
	int          i;

	*buffer = '\0';
	for (i = 0; i < MAX_FIRETEAM_MEMBERS; i++)
	{
		ci = CG_SortedFireTeamPlayerForPosition(i);
		if (!ci)
		{
			break;
		}

		if (!ci->selected)
		{
			continue;
		}

		cnt++;

		Q_strcat(buffer, sizeof(buffer), va("%i ", ci->clientNum));
	}

	if (cnt == 0)
	{
		return "0";
	}

	if (!cgs.clientinfo[cg.clientNum].selected)
	{
		Q_strcat(buffer, sizeof(buffer), va("%i ", cg.clientNum));
		cnt++;
	}

	return va("%i %s", cnt, buffer);
}
