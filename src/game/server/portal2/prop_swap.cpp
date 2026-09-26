//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Swap
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "triggers.h"
#include "props.h"
#include "photo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
//	Swap
//

class CPropSwap : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropSwap, CPhysicsProp );
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );

	virtual bool TestPreCapture( void );

protected:
	virtual void OnCaptured( void );
};

BEGIN_DATADESC( CPropSwap )
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_swap, CPropSwap );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropSwap::Precache( void )
{
	PrecacheModel( "models/props/metal_box.mdl" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropSwap::Spawn( void )
{
//	m_bDoReflection = true;

	Precache();
	SetModel( "models/props/metal_box.mdl" );

	BaseClass::Spawn();

	// make this big enough to prevent us getting stuck
	UTIL_CreateScaledPhysObject( this, 1.0f );
	SetModelScale(1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropSwap::TestPreCapture( void )
{
	Vector playerOrigin, swapOrigin;
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	// find out where both of these are at
	playerOrigin = pPlayer->GetAbsOrigin();
	swapOrigin = GetAbsOrigin();

	// technically we want to swap our floor poistion, so do some math to shift things about.
	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
	
	playerOrigin.z += ( swapOrigin.z - vecMins.z );
	swapOrigin.z -= ( swapOrigin.z - vecMins.z );
	swapOrigin.z += 16.f;

	// swap our position with the player.
	pPlayer->Teleport( &swapOrigin, NULL, NULL );
	Teleport( &playerOrigin, NULL, NULL );

	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->Wake();
	}

	// we already handled our event, so, uh, just don't do anything!
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropSwap::OnCaptured( void )
{
	// Do we need to handle this case? I don't think so...
	BaseClass::OnCaptured();
}