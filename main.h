// main.h: include file for standart system include files or include files for specific project.

#pragma once

#include <extdll.h>
#include <meta_api.h>
#include <stdio.h>
#include <string.h>
#include <rehlds_api.h>
#include "ex_rehlds_api.h"
#include "cvars.h"
#include "entity_state.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "enginecallback.h"

class player_s
{
public:
	bool state[MAX_CLIENTS];
};
extern player_s players[MAX_CLIENTS];

bool UTIL_TraceHull(Vector vecSrc, Vector vecEnd, Vector vecMins, Vector vecMaxs, edict_t* pSkip);
BOOL AddToFullPack_Pre(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, BOOL player, unsigned char* pSet);
BOOL AddToFullPack_Post(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, BOOL player, unsigned char* pSet);
void StartFrame_Post();