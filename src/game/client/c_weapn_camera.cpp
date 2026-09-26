//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side camera weapon
//
//=====================================================================================//

#include "cbase.h"
#include "c_weapon_camera.h"
#include "c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// C_WeaponCamera
//-----------------------------------------------------------------------------

IMPLEMENT_CLIENTCLASS_DT(C_WeaponCamera, DT_WeaponCamera, CWeaponCamera)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

C_WeaponCamera::C_WeaponCamera()
{
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

C_WeaponCamera::~C_WeaponCamera()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool C_WeaponCamera::ShouldDraw()
{
	return BaseClass::ShouldDraw();
}