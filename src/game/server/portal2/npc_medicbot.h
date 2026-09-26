//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A companion that can heal the player
//
//=============================================================================//

#ifndef	NPC_MEDICBOT_H
#define	NPC_MEDICBOT_H

#include "npc_companionbot.h"

class CNPC_MedicBot : public CNPC_CompanionBot
{
	DECLARE_CLASS( CNPC_MedicBot, CNPC_CompanionBot );

public:
					CNPC_MedicBot( void );

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Activate( void );

	virtual int		SelectSchedule( void );
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &set );

	//---------------------------------
	//	Bot Types
	//---------------------------------
	virtual int		GetBotType( void ) const { return m_iBotType; }
	virtual char	*GetBotString( void ) const { return "MBot"; }

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

#endif	//NPC_MEDICBOT_H
