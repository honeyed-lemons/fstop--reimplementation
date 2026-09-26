//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Reflect
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
//	Reflect
//

class CPropReflect : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropReflect, CPhysicsProp );
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );

	virtual bool TestPreCapture( void );

protected:
	virtual void OnCaptured( void );
};

BEGIN_DATADESC( CPropReflect )
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_reflect, CPropReflect );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropReflect::Precache( void )
{
	PrecacheModel( "models/props/metal_box.mdl" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropReflect::Spawn( void )
{

	Precache();
	SetModel( "models/props/metal_box.mdl" );

	BaseClass::Spawn();

	// make this big enough to prevent us getting stuck
	UTIL_CreateScaledPhysObject( this, 1.0f );
	SetModelScale(1.0f);

	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )	
	{
		pPhysObj->EnableMotion(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropReflect::TestPreCapture( void )
{
	Vector swapOrigin;
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	// find out where our mirror is at.
	swapOrigin = GetAbsOrigin();

	// technically we want to swap our floor poistion, so do some math to shift things about.
	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );

	// Get our up angle, this is what we are going to use to reflect ourselves.
	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

	// put ourselves at little above the object we are refelcting off.
	swapOrigin.z -= ( swapOrigin.z - vecMins.z );
	swapOrigin += vecUp*64.f;

	// Do our reflection based on the 
	// I' = I-2 * dot(N,I) * N;
	Vector playerVelocity = pPlayer->GetAbsVelocity();
	playerVelocity = playerVelocity - 2.0f * vecUp.Dot( playerVelocity ) * vecUp;

	// swap our position with the player.
	pPlayer->Teleport( &swapOrigin, NULL, &playerVelocity );

	// we already handled our event, so, uh, just don't do anything!
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropReflect::OnCaptured( void )
{
	// Do we need to handle this case? I don't think so...
	BaseClass::OnCaptured();
}