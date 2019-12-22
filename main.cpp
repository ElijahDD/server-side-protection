#include "framework.h"

player_s players[MAX_CLIENTS];

bool UTIL_TraceHull(Vector vecSrc, Vector vecEnd, Vector vecMins, Vector vecMaxs, edict_t* pSkip)
{

	TraceResult trace;
	float* minmaxs[2] = { vecMins, vecMaxs };
	Vector vecTemp;
	int i, j, k;

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				vecTemp.x = vecEnd.x + minmaxs[i][0];
				vecTemp.y = vecEnd.y + minmaxs[j][1];
				vecTemp.z = vecEnd.z + minmaxs[k][2];

				TRACE_LINE(vecSrc, vecTemp, ignore_monsters | 0x100, pSkip, &trace); // ignore glass

				if (trace.flFraction == 1.0f)
					return true;

				if (trace.pHit)
				{
					auto pTextureName = TRACE_TEXTURE(trace.pHit, vecSrc, vecTemp);

					if (pTextureName)
					{
						//gpMetaUtilFuncs->pfnLogConsole(PLID, "classname: %s | %s %i %i %i %i", STRING(VARS(trace.pHit)->classname), pTextureName, trace.pHit->v.solid, trace.pHit->v.renderamt, trace.pHit->v.renderfx, trace.pHit->v.rendermode);

						if (pTextureName[0] == '{' || strstr(pTextureName, "glass"))
							return true;
					}
				}
			}
		}
	}

	return false;
}

BOOL AddToFullPack_Pre(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, BOOL player, unsigned char* pSet)
{
	if (player && ent != host)
	{
		auto attacker_index = ENTINDEX(host);
		auto enemy_index = e;

		if (players[attacker_index - 1].state[enemy_index - 1] && ((pcv_ssp_reversed_visibility->value && players[attacker_index - 1].state[enemy_index - 1]) || !pcv_ssp_reversed_visibility->value))
			RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

BOOL AddToFullPack_Post(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, BOOL player, unsigned char* pSet)
{
	if (player && ent != host && pcv_ssp_remove_players_solid->value)
	{
		CBasePlayer* pAttackerPlayer = (CBasePlayer*)GET_PRIVATE(host);
		CBasePlayer* pEnemyPlayer = (CBasePlayer*)GET_PRIVATE(ent);

		if (pEnemyPlayer && pAttackerPlayer)
		{
			if ((pcv_ssp_teammates->value && pAttackerPlayer->m_iTeam == pEnemyPlayer->m_iTeam) || pAttackerPlayer->m_iTeam != pEnemyPlayer->m_iTeam)
			{
				auto distance = Vector(host->v.origin - ent->v.origin).Length();

				if (distance > 128.f) // For stuck
				{
					state->solid = SOLID_NOT;
					//gpMetaUtilFuncs->pfnLogConsole(PLID, "SOLID_NOT for %i", ENTINDEX(ent));
				}
			}
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void StartFrame_Post()
{
	for (int attacker_index = 1; attacker_index <= gpGlobals->maxClients; attacker_index++)
	{
		auto attacker_edict = INDEXENT(attacker_index);

		if (!attacker_edict)
			continue;

		if (attacker_edict->v.health <= 0.f || attacker_edict->v.deadflag != DEAD_NO)
			continue;

		CBasePlayer* pAttackerPlayer = (CBasePlayer*)GET_PRIVATE(attacker_edict);

		if (!pAttackerPlayer)
			continue;

		Vector vecSrc(attacker_edict->v.origin + attacker_edict->v.view_ofs);

		for (int enemy_index = 1; enemy_index <= gpGlobals->maxClients; enemy_index++)
		{
			players[attacker_index - 1].state[enemy_index - 1] = false;

			if (enemy_index == attacker_index)
				continue;

			auto enemy_edict = INDEXENT(enemy_index);

			if (!enemy_edict)
				continue;

			if (enemy_edict->v.health <= 0.f || enemy_edict->v.deadflag != DEAD_NO)
				continue;

			if (enemy_edict->v.effects & EF_NODRAW)
				continue;

			CBasePlayer* pEnemyPlayer = (CBasePlayer*)GET_PRIVATE(enemy_edict);

			if (!pEnemyPlayer)
				continue;

			//gpMetaUtilFuncs->pfnLogConsole(PLID, "%i %i %f", pEnemyPlayer->m_iTeam, pAttackerPlayer->m_iTeam, pcv_ssp_teammates->value);

			if (!pcv_ssp_teammates->value && pEnemyPlayer->m_iTeam == pAttackerPlayer->m_iTeam)
				continue;

			players[attacker_index - 1].state[enemy_index - 1] = true;

			if (UTIL_TraceHull(vecSrc, enemy_edict->v.origin, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
			{
				players[attacker_index - 1].state[enemy_index - 1] = false;
				continue;
			}

			auto weapon_model_index = MODEL_INDEX(STRING(enemy_edict->v.weaponmodel));

			if (weapon_model_index)
			{
				Vector vecAngles(enemy_edict->v.angles), vecForward;

				g_engfuncs.pfnAngleVectors(vecAngles, vecForward, NULL, NULL);

				TraceResult trace;

				TRACE_LINE(enemy_edict->v.origin + enemy_edict->v.view_ofs, (enemy_edict->v.origin + enemy_edict->v.view_ofs) + vecForward * 64.f, ignore_monsters, attacker_edict, &trace); // mb cuustom for weapons

				TRACE_LINE(vecSrc, trace.vecEndPos, ignore_monsters, attacker_edict, &trace); // maybe for no bugs => - endpos - vecdir * 4.f

				if (trace.flFraction == 1.f)
				{
					//gpMetaUtilFuncs->pfnLogConsole(PLID, "weapon => attacker_index: %i, enemy_index: %i", attacker_index, enemy_index);
					players[attacker_index - 1].state[enemy_index - 1] = false;
					continue;
				}
			}

			if (!enemy_edict->v.velocity.IsZero())
			{
				Vector vecEnd = enemy_edict->v.origin;

				if (pcv_ssp_predict_origin->value)
					vecEnd = vecEnd + enemy_edict->v.velocity * gpGlobals->frametime * pcv_ssp_predict_origin->value;

				if (UTIL_TraceHull(vecSrc, vecEnd, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
				{
					//gpMetaUtilFuncs->pfnLogConsole(PLID, "velocity => attacker_index: %i, enemy_index: %i", attacker_index, enemy_index);
					players[attacker_index - 1].state[enemy_index - 1] = false;
					continue;
				}

				if (UTIL_TraceHull(vecSrc, vecEnd, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
				{
					//gpMetaUtilFuncs->pfnLogConsole(PLID, "inversed velocity => attacker_index: %i, enemy_index: %i", attacker_index, enemy_index);
					players[attacker_index - 1].state[enemy_index - 1] = false;
					continue;
				}
			}
		}
	}

	RETURN_META(MRES_IGNORED);
}