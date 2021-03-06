#include "Proxy_Server.hpp"
#include "Proxy_Header.hpp"

ProxyServer_t server;

void Proxy_Server_Initialize_MemoryAddress(void)
{
	// ----------- SERVER

	// vars
	server.svs = (serverStatic_t*)var_svs_addr;
	server.sv = (server_t*)var_sv_addr;

	// cvars
	server.cvars.sv_fps = *(cvar_t**)cvar_sv_fps_addr;
	server.cvars.sv_gametype = *(cvar_t**)cvar_sv_gametype_addr;
	server.cvars.sv_hostname = *(cvar_t**)cvar_sv_hostname_addr;
	server.cvars.sv_mapname = *(cvar_t**)cvar_sv_mapname_addr;
	server.cvars.sv_maxclients = *(cvar_t**)cvar_sv_maxclients_addr;
	server.cvars.sv_privateClients = *(cvar_t**)cvar_sv_privateClients_addr;
	server.cvars.sv_pure = *(cvar_t**)cvar_sv_pure_addr;

	// functions
	server.functions.SV_ClientEnterWorld = (void (*)(client_t*, usercmd_t*))func_SV_ClientEnterWorld_addr;
	server.functions.SV_ClientThink = (void (*)(client_t*, usercmd_t*))func_SV_ClientThink_addr;
	server.functions.SV_ExecuteClientCommand = (void (*)(client_t*, const char*, qboolean))func_SV_ExecuteClientCommand_addr;
	server.functions.SV_DropClient = (void (*)(client_t*, const char*))func_SV_DropClient_addr;
	server.functions.SV_Netchan_Transmit = (void (*)(client_t*, msg_t*))func_SV_Netchan_Transmit_addr;
	server.functions.SV_RateMsec = (int (*)(client_t*, int))func_SV_RateMsec_addr;
	server.functions.SV_UpdateServerCommandsToClient = (void (*)(client_t*, msg_t*))func_SV_UpdateServerCommandsToClient_addr;

	// ----------- COMMON

	// vars
	server.common.vars.logfile = (fileHandle_t*)var_common_logfile_addr;
	server.common.vars.rd_buffer = (char**)var_common_rd_buffer_addr;
	server.common.vars.rd_buffersize = (int*)var_common_rd_buffersize_addr;

	// cvars
	server.common.cvars.com_dedicated = *(cvar_t**)cvar_common_com_dedicated_addr;
	server.common.cvars.com_sv_running = *(cvar_t**)cvar_common_com_sv_running_addr;
	server.common.cvars.com_logfile = *(cvar_t**)cvar_common_com_logfile_addr;
	server.common.cvars.fs_gamedirvar = *(cvar_t**)cvar_common_fs_gamedirvar_addr;

	// functions
	server.common.functions.Com_DPrintf = (void (QDECL*)(const char*, ...))func_Com_DPrintf_addr;
	server.common.functions.Com_HashKey = (int (*)(char*, int))func_Com_HashKey_addr;
	server.common.functions.Com_Printf = (void (QDECL *)(const char*, ...))func_Com_Printf_addr;
	server.common.functions.Cvar_VariableString = (char* (*)(const char*))func_Cvar_VariableString_addr;
	server.common.functions.FS_FOpenFileWrite = (fileHandle_t (*)(const char*))func_FS_FOpenFileWrite_addr;
	server.common.functions.FS_ForceFlush = (void (*)(fileHandle_t))func_FS_ForceFlush_addr;
	server.common.functions.FS_Initialized = (qboolean(*)())func_FS_Initialized_addr;
	server.common.functions.FS_Write = (int (*)(const void*, int, fileHandle_t))func_FS_Write_addr;
	server.common.functions.Netchan_TransmitNextFragment = (void (*)(netchan_t*))func_Netchan_TransmitNextFragment_addr;
	server.common.functions.NET_AdrToString = (const char* (*)(netadr_t))func_NET_AdrToString_addr;
	server.common.functions.MSG_Init = (void (*)(msg_t*, byte*, int))func_MSG_Init_addr;
	server.common.functions.MSG_ReadByte = (int (*)(msg_t*))func_MSG_ReadByte_addr;
	server.common.functions.MSG_ReadDeltaUsercmdKey = (void (*)(msg_t*, int, usercmd_t*, usercmd_t*))func_MSG_ReadDeltaUsercmdKey_addr;
	server.common.functions.MSG_WriteBigString = (void (*)(msg_t*, const char*))func_MSG_WriteBigString_addr;
	server.common.functions.MSG_WriteByte = (void (*)(msg_t*, int))func_MSG_WriteByte_addr;
	server.common.functions.MSG_WriteDeltaEntity = (void (*)(msg_t*, struct entityState_s*, struct entityState_s*, qboolean))func_MSG_WriteDeltaEntity_addr;
	server.common.functions.MSG_WriteLong = (void (*)(msg_t*, int))func_MSG_WriteLong_addr;
	server.common.functions.MSG_WriteShort = (void (*)(msg_t*, int))func_MSG_WriteShort_addr;
	server.common.functions.rd_flush = *(void (**)(char*))var_common_rd_flush_addr;
	server.common.functions.Sys_IsLANAddress = (qboolean(*)(netadr_t))func_Sys_IsLANAddress_addr;
	server.common.functions.Sys_Print = (void (*)(const char*))func_Sys_Print_addr;
}

// Update value of packets and FPS
// From JK2MV (updated version by fau)
void Proxy_Server_CalcPacketsAndFPS(int clientNum, int* packets, int* fps)
{
	int		lastCmdTime;
	int		lastPacketIndex = 0;
	int		i;

	lastCmdTime = proxy.clientData[clientNum].cmdStats[proxy.clientData[clientNum].cmdIndex & (CMD_MASK - 1)].serverTime;

	for (i = 0; i < CMD_MASK; i++)
	{
		ucmdStat_t* stat = &proxy.clientData[clientNum].cmdStats[i];

		if (stat->serverTime + 1000 >= lastCmdTime)
		{
			(*fps)++;

			if (lastPacketIndex != stat->packetIndex)
			{
				lastPacketIndex = stat->packetIndex;
				(*packets)++;
			}
		}
	}
}

void Proxy_Server_UpdateUcmdStats(int clientNum, usercmd_t* cmd, int packetIndex)
{
	proxy.clientData[clientNum].cmdIndex++;
	const int cmdIndex = (proxy.clientData[clientNum].cmdIndex & (CMD_MASK - 1));
	proxy.clientData[clientNum].cmdStats[cmdIndex].serverTime = cmd->serverTime;
	proxy.clientData[clientNum].cmdStats[cmdIndex].packetIndex = packetIndex;
}

static inline int Proxy_CalculateTimeNudge(int clientNum, float serverFrameTime)
{
	timenudgeData_t *timenudgeData = &proxy.clientData[clientNum].timenudgeData;
	int magicOffsetNumber = 19;

	if (server.cvars.sv_fps->integer >= 30)
		magicOffsetNumber = 11;//hack (DUMB), should just fix calculation so it doesn't rely on magic numbers like this

#if defined(_WIN32) && !defined(MING32)
	magicOffsetNumber += 2;
#endif

	return (
			(timenudgeData->delaySum / (float)timenudgeData->delayCount)
			+ (timenudgeData->pingSum / (float)timenudgeData->delayCount)
			- magicOffsetNumber // this magic number might be the instructions time until the calc
			+ serverFrameTime
			) * -1;
}

void Proxy_Server_UpdateTimenudge(client_t* client, usercmd_t* cmd, int _Milliseconds)
{
	const int clientNum = getClientNumFromAddr(client);

	proxy.clientData[clientNum].timenudgeData.delayCount++;
	proxy.clientData[clientNum].timenudgeData.delaySum += cmd->serverTime - server.svs->time;
	proxy.clientData[clientNum].timenudgeData.pingSum += client->ping;

	// Wait 1000 ms so we have enough data when we'll calcul an approximation of the timenudge
	if (_Milliseconds < proxy.clientData[clientNum].timenudgeData.lastTimeTimeNudgeCalculation + 1000) {
		return;
	}

	// ((serverTime - sv.time) + ping -18 + (1000/sv_fps)) * -1
	proxy.clientData[clientNum].timenudge = Proxy_CalculateTimeNudge(clientNum, (1000.0f / server.cvars.sv_fps->value)); //mayb should just check the time since last frame?

	proxy.clientData[clientNum].timenudgeData.delayCount = 0;
	proxy.clientData[clientNum].timenudgeData.delaySum = 0;
	proxy.clientData[clientNum].timenudgeData.pingSum = 0;

	proxy.clientData[clientNum].timenudgeData.lastTimeTimeNudgeCalculation = _Milliseconds;
}