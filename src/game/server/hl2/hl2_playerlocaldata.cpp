//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2_playerlocaldata.h"
#include "hl2_player.h"
#include "mathlib/mathlib.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_SEND_TABLE_NOBASE( CHL2PlayerLocalData, DT_HL2Local )
	SendPropFloat( SENDINFO(m_flSuitPower), 10, SPROP_UNSIGNED | SPROP_ROUNDUP, 0.0, 100.0 ),
	SendPropInt( SENDINFO(m_bZooming), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_bitsActiveDevices), MAX_SUIT_DEVICES, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_iSquadMemberCount) ),
	SendPropInt( SENDINFO(m_iSquadMedicCount) ),
	SendPropBool( SENDINFO(m_fSquadInFollowMode) ),
	SendPropBool( SENDINFO(m_bWeaponLowered) ),
	SendPropEHandle( SENDINFO(m_hAutoAimTarget) ),
	SendPropVector( SENDINFO(m_vecAutoAimPoint) ),
	SendPropEHandle( SENDINFO(m_hLadder) ),
	SendPropBool( SENDINFO(m_bDisplayReticle) ),
	SendPropBool( SENDINFO(m_bStickyAutoAim) ),
	SendPropBool( SENDINFO(m_bAutoAimTarget) ),
#ifdef HL2_EPISODIC
	SendPropFloat( SENDINFO(m_flFlashBattery) ),
	SendPropVector( SENDINFO(m_vecLocatorOrigin) ),
#endif
#ifdef PORTAL2
	// SendPropArray3( SENDINFO_ARRAY3(m_bHasPhotoInInventory), SendPropBool( SENDINFO_ARRAY(m_bHasPhotoInInventory) ) ),
	SendPropArray3(SENDINFO_ARRAY3(m_nLocatorEntityIndices), SendPropInt(SENDINFO_ARRAY(m_nLocatorEntityIndices))),
	// SendPropInt( SENDINFO(m_nSelectedPhoto) ),
	SendPropBool(SENDINFO(m_bPlacingPhoto)),
#endif // PORTAL2

END_SEND_TABLE()

BEGIN_SIMPLE_DATADESC( CHL2PlayerLocalData )
	DEFINE_FIELD( m_flSuitPower, FIELD_FLOAT ),
	DEFINE_FIELD( m_bZooming, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bitsActiveDevices, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSquadMemberCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSquadMedicCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_fSquadInFollowMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bWeaponLowered, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDisplayReticle, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bStickyAutoAim, FIELD_BOOLEAN ),
#ifdef HL2_EPISODIC
	DEFINE_FIELD( m_flFlashBattery, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLocatorOrigin, FIELD_POSITION_VECTOR ),
#endif
	// Ladder related stuff
	DEFINE_FIELD( m_hLadder, FIELD_EHANDLE ),
	DEFINE_EMBEDDED( m_LadderMove ),
#ifdef PORTAL2
	// DEFINE_ARRAY( m_bHasPhotoInInventory, FIELD_BOOLEAN, 3 ),
	DEFINE_ARRAY(m_nLocatorEntityIndices, FIELD_INTEGER, 16),
	// DEFINE_FIELD( m_nSelectedPhoto, FIELD_INTEGER ),
	DEFINE_FIELD(m_bPlacingPhoto, FIELD_BOOLEAN),
#endif // PORTAL2

END_DATADESC()

CHL2PlayerLocalData::CHL2PlayerLocalData()
{
	m_flSuitPower = 0.0;
	m_bZooming = false;
	m_bWeaponLowered = false;
	m_hAutoAimTarget.Set(NULL);
	m_hLadder.Set(NULL);
	m_vecAutoAimPoint.GetForModify().Init();
	m_bDisplayReticle = false;
#ifdef HL2_EPISODIC
	m_flFlashBattery = 0.0f;
#endif

#ifdef PORTAL2
	// Clear this out!
	/*
	m_bHasPhotoInInventory.GetForModify(0) = false;
	m_bHasPhotoInInventory.GetForModify(1) = false;
	m_bHasPhotoInInventory.GetForModify(2) = false;
	*/

	for (int i = 0; i < 16; i++)
	{
		m_nLocatorEntityIndices.GetForModify(i) = -1;
	}

	// m_nSelectedPhoto = -1;
	m_bPlacingPhoto = false;
#endif // PORTAL2
}

