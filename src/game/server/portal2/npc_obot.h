//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: An offense-focused companion
//
//=============================================================================//

#ifndef	NPC_OBOT_H
#define	NPC_OBOT_H

#include "npc_companionbot.h"

class CNPC_OBot : public CNPC_CompanionBot
{
	DECLARE_CLASS( CNPC_OBot, CNPC_CompanionBot );

public:
					CNPC_OBot( void );

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Activate( void );

	virtual int		SelectSchedule();
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &set );

	//---------------------------------
	//	Bot Types
	//---------------------------------
	virtual int		GetBotType( void ) const { return m_iBotType; }
	virtual char	*GetBotString( void ) const { return "OBot"; }

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

#endif	//NPC_OBOT_H
