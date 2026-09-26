//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "c_basehlcombatweapon.h"
#include "iviewrender_beams.h"
#include "beam_shared.h"
#include "c_weapon__stubs.h"
#include "materialsystem/imaterial.h"
#include "precache_register.h"
#include "beamdraw.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_WeaponCamera : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(C_WeaponCamera, CBaseHLCombatWeapon);
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
};