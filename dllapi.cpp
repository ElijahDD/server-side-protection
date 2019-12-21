#include <extdll.h>
#include <meta_api.h>

cvar_t ssp_version = { "ssp_version", (char*)Plugin_info.version, FCVAR_SERVER | FCVAR_EXTDLL, 0, nullptr };
cvar_t ssp_predict_origin = { "ssp_predict_origin", "16.0", FCVAR_SERVER | FCVAR_EXTDLL, 16.f, nullptr };
cvar_t ssp_reversed_visibility = { "ssp_reversed_visibility", "1.0", FCVAR_SERVER | FCVAR_EXTDLL, 1.f, nullptr };

class player_s {
public:
	bool state[MAX_CLIENTS];
};

player_s players[MAX_CLIENTS];

bool MyTraceHull(Vector vecSrc, Vector vecEnd, Vector mins, Vector maxs, edict_t* pSkip)
{
	TraceResult trace;
	float* minmaxs[2] = { mins, maxs };
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

		if (players[attacker_index - 1].state[enemy_index - 1] && ((ssp_reversed_visibility.value && players[attacker_index - 1].state[enemy_index - 1]) || !ssp_reversed_visibility.value))
			RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
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

		Vector vecSrc(attacker_edict->v.origin + attacker_edict->v.view_ofs);

		for (int enemy_index = 1; enemy_index <= gpGlobals->maxClients; enemy_index++)
		{
			if (enemy_index == attacker_index)
			{
				players[attacker_index - 1].state[enemy_index - 1] = false;
				continue;
			}

			players[attacker_index - 1].state[enemy_index - 1] = true;

			auto enemy_edict = INDEXENT(enemy_index);

			if (!enemy_edict)
				continue;

			if (enemy_edict->v.health <= 0.f || enemy_edict->v.deadflag != DEAD_NO)
				continue;

			if (enemy_edict->v.effects & EF_NODRAW)
				continue;

			if (MyTraceHull(vecSrc, enemy_edict->v.origin, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
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

				if (ssp_predict_origin.value)
					vecEnd = vecEnd + enemy_edict->v.velocity * gpGlobals->frametime * ssp_predict_origin.value;

				if (MyTraceHull(vecSrc, vecEnd, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
				{
					//gpMetaUtilFuncs->pfnLogConsole(PLID, "velocity => attacker_index: %i, enemy_index: %i", attacker_index, enemy_index);
					players[attacker_index - 1].state[enemy_index - 1] = false;
					continue;
				}

				if (MyTraceHull(vecSrc, vecEnd, enemy_edict->v.mins, enemy_edict->v.maxs, attacker_edict))
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

DLL_FUNCTIONS g_DllFunctionTable =
{
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,					// pfnTouch
	NULL,					// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox
	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields
	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState
	NULL,					// pfnClientConnect
	NULL,					// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer
	NULL,					// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate
	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink
	NULL,					// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel
	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization
	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink
	NULL,					// pfnSys_Error
	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType
	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	&AddToFullPack_Pre,		// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

DLL_FUNCTIONS g_DllFunctionTable_Post =
{
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,					// pfnTouch
	NULL,					// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox
	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields
	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState
	NULL,					// pfnClientConnect
	NULL,					// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer
	NULL,					// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate
	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink
	&StartFrame_Post,		// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel
	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization
	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink
	NULL,					// pfnSys_Error
	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType
	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

NEW_DLL_FUNCTIONS g_NewDllFunctionTable =
{
	NULL,					//! pfnOnFreeEntPrivateData()	Called right before the object's memory is freed.  Calls its destructor.
	NULL,					//! pfnGameShutdown()
	NULL,					//! pfnShouldCollide()
	NULL,					//! pfnCvarValue()
	NULL,					//! pfnCvarValue2()
};

NEW_DLL_FUNCTIONS g_NewDllFunctionTable_Post =
{
	NULL,					//! pfnOnFreeEntPrivateData()	Called right before the object's memory is freed.  Calls its destructor.
	NULL,					//! pfnGameShutdown()
	NULL,					//! pfnShouldCollide()
	NULL,					//! pfnCvarValue()
	NULL,					//! pfnCvarValue2()
};

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable) {
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pFunctionTable, &g_DllFunctionTable, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable) {
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pFunctionTable, &g_DllFunctionTable_Post, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetNewDLLFunctions(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable) {
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable, sizeof(NEW_DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetNewDLLFunctions_Post(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable) {
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable_Post, sizeof(NEW_DLL_FUNCTIONS));
	return TRUE;
}
