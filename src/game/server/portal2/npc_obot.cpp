//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: An offense-focused companion
//
//=============================================================================//

#include "cbase.h"
#include "npc_obot.h"
#include "hl2_player.h"
#include "npcevent.h"
#include "ammodef.h"
#include "basehlcombatweapon_shared.h"
// #include "te_effect_dispatch.h"
#include "particle_parse.h"
#include "beam_shared.h"
#include "weapon_rpg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	eoz_obot_health					( "eoz_obot_health", "300", FCVAR_ARCHIVE, "How much health the obot has." );

//---------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_obot, CNPC_OBot );

//-----------------------------------------------------------------------------
// obot data table
//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_OBot )
END_DATADESC();

namespace EOZ_Hacks
{
	extern EHANDLE g_pMbot, g_pObot;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CNPC_OBot::CNPC_OBot( void )
{
	m_iBotType = CBOT_OFFENSIVE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_OBot::Activate( void )
{
	BaseClass::Activate();
	if (EOZ_Hacks::g_pObot.Get())
	{
		Warning("Created an obot when one already exists.");
	}
	EOZ_Hacks::g_pObot = this;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_OBot::Precache( void )
{
	SetModelName( AllocPooledString( "models/companionBot/offenseBot.mdl" ) );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_OBot::Spawn( void )
{
	BaseClass::Spawn();

	m_iHealth = m_iMaxHealth = eoz_obot_health.GetFloat();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_OBot::SelectSchedule()
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Tack on extra criteria for responses
//-----------------------------------------------------------------------------
void CNPC_OBot::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// set.AppendCriteria( "has_ammo", UTIL_VarArgs("%d", m_iAmmoToGive) ) ;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_obot, CNPC_OBot )
AI_END_CUSTOM_NPC()
