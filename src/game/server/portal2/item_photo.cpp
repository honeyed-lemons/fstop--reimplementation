//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photograph that sits in the world and may be picked up
//
//====================================================================================//

#include "cbase.h"
#include "items.h"
#include "ai_utils.h"
#include "photo.h"
#include "portal_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_PHOTOGRAPH_IS_MOTION_DISABLED	( 1 << 0 )

//-----------------------------------------------------------------------------
// Small health kit. Heals the player when picked up.
//-----------------------------------------------------------------------------
class CPhotograph : public CItem
{
public:
	DECLARE_CLASS( CPhotograph, CItem );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );
	
	bool MyTouch( CBasePlayer *pPlayer );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CaptureThink( void );

private:
	string_t		m_strEntityName;
	string_t		m_strMaterialName;
	bool			m_bMotionDisabled;
	CaptureInfo_t	m_captureInfo;

	COutputEvent	m_OnPickedUp;

public:
	CNetworkString( m_szTextureName, MAX_PATH );
};

LINK_ENTITY_TO_CLASS( item_photo, CPhotograph );
PRECACHE_REGISTER( item_photo );

BEGIN_DATADESC( CPhotograph )
	DEFINE_KEYFIELD( m_strEntityName, FIELD_STRING, "target_entity" ),
	DEFINE_KEYFIELD( m_strMaterialName, FIELD_STRING, "target_material" ),
	DEFINE_KEYFIELD( m_bMotionDisabled, FIELD_BOOLEAN, "target_motiondisabled" ),

	DEFINE_EMBEDDED( m_captureInfo ),

	DEFINE_THINKFUNC( CaptureThink ),

	DEFINE_ARRAY( m_szTextureName, FIELD_CHARACTER, MAX_PATH ),

	DEFINE_OUTPUT( m_OnPickedUp, "OnPickedUp" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPhotograph, DT_Photograph )
	SendPropString( SENDINFO( m_szTextureName ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhotograph::Spawn( void )
{
	Precache();
	SetModel( "models/items/photograph.mdl" );

	Q_strncpy( m_szTextureName.GetForModify(), STRING( m_strMaterialName ), MAX_PATH );
	BaseClass::Spawn();

	// See if we should be motion disabled
	if ( HasSpawnFlags( SF_PHOTOGRAPH_IS_MOTION_DISABLED ) )
	{
		IPhysicsObject *pObject = VPhysicsGetObject();
		if ( pObject )
		{
			pObject->EnableMotion( false );
		}

		AddEffects( EF_NOSHADOW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhotograph::Activate( void )
{
	SetThink( &CPhotograph::CaptureThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhotograph::CaptureThink( void )
{
	// Find our target
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, STRING( m_strEntityName ) );
	if ( pTarget == NULL )
	{
		Assert( pTarget != NULL );
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;
	}

	// Setup this item as if captured by the camera
	UTIL_InitCaptureInfo( m_captureInfo, pTarget );

	CBaseAnimating *pAnim = pTarget->GetBaseAnimating();
	if ( pAnim )
	{
		pAnim->OnCaptured();
		if ( !pAnim->IsInStasis() )
		{
			pAnim->SetStasis( true );
		}

		pAnim->SetParent( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhotograph::Precache( void )
{
	PrecacheModel("models/items/photograph.mdl");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhotograph::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( pActivator );
	if ( pPlayer == NULL )
		return;

	// Must have room
	if ( Photo_CanAdd() == false )
	{
		pPlayer->FlashDenyIndicator( 0.75f, FLASH_INDICATOR_FULL );
		pPlayer->FlashInventory( 1.0f, FLASH_INVENTORY_FULL );
		return;
	}

	// Put it into our inventory
	int nIndex = Photo_Add( &m_captureInfo );

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();
	UserMessageBegin( user, "TakePhoto" );
	WRITE_EHANDLE( this );
	WRITE_BYTE( nIndex );
	MessageEnd();

	if ( pPlayer )
	{
		pPlayer->FlashInventory( 2.0f, FLASH_INVENTORY_ADDED );
		pPlayer->SetSelectedPhoto( nIndex );
	}

	// Make sure they have this weapon (it will be rejected if they do)
	if ( pPlayer->HasNamedPlayerItem( "weapon_placement" ) == false )
	{
		pPlayer->GiveNamedItem( "weapon_placement" );
	}
	else
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
		if ( pWeapon )
		{
			// If we're cycling but not in placement mode, then swap over to it but don't cycle
			if ( FClassnameIs( pWeapon, "weapon_camera" ) )
			{
				// Swap to placement mode
				pPlayer->SwitchToNextBestWeapon( pWeapon );
			}
			else
			{
				// Re-animate to show the addition
				pPlayer->GetActiveWeapon()->Deploy();
			}
		}
	}

	// Fire the output
	m_OnPickedUp.FireOutput( pActivator, this );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );

	SetSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: Allow the player to pick us up and hold us
//-----------------------------------------------------------------------------
bool CPhotograph::MyTouch( CBasePlayer *pOther )
{
	// Never pick up via a bump
	return false;
}
