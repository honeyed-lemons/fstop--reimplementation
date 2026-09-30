//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A companion that can heal the player
//
//=============================================================================//

#include "cbase.h"
#include "npc_medicbot.h"
#include "hl2_player.h"
#include "npcevent.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	eoz_medicbot_health( "eoz_medicbot_health", "200", FCVAR_ARCHIVE, "How much health the medicbot has." );

//---------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_medicbot, CNPC_MedicBot );

BEGIN_DATADESC( CNPC_MedicBot )
END_DATADESC();

AI_BEGIN_CUSTOM_NPC( npc_medicbot, CNPC_MedicBot )
AI_END_CUSTOM_NPC()

namespace EOZ_Hacks
{
	extern EHANDLE g_pMbot, g_pObot;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CNPC_MedicBot::CNPC_MedicBot( void )
{
	m_iBotType = CBOT_DEFENSIVE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_MedicBot::Activate( void )
{
	BaseClass::Activate();
	if (EOZ_Hacks::g_pMbot.Get())
	{
		Warning("Created a medicbot when one already exists.");
	}
	EOZ_Hacks::g_pMbot = this;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_MedicBot::Precache( void )
{
	SetModelName( AllocPooledString( "models/companionBot/companionBot.mdl" ) );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_MedicBot::Spawn( void )
{
	BaseClass::Spawn();

	m_iHealth = m_iMaxHealth = eoz_medicbot_health.GetFloat();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CNPC_MedicBot::SelectSchedule( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Tack on extra criteria for responses
//-----------------------------------------------------------------------------
void CNPC_MedicBot::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// set.AppendCriteria( "total_healed", UTIL_VarArgs( "%u", m_nTotalHealingDispensed ) ) ;
}
