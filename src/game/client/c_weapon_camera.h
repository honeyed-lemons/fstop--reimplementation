//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side camera weapon
//
//=====================================================================================//

#ifndef C_WEAPON_CAMERA_H
#define C_WEAPON_CAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlcombatweapon.h"

//-----------------------------------------------------------------------------
// C_WeaponCamera
//-----------------------------------------------------------------------------

class C_WeaponCamera : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS(C_WeaponCamera, C_BaseHLCombatWeapon);
	DECLARE_CLIENTCLASS();

	C_WeaponCamera();
	~C_WeaponCamera();

	virtual bool ShouldDraw();
};

#endif // C_WEAPON_CAMERA_H
