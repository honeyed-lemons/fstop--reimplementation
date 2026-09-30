//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Camera
//
//=====================================================================================//

#include "cbase.h"
#include "weapon_camera.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"
#include "saverestore.h"
#include "saverestoretypes.h"
#include "portal_player.h"
#include "particle_parse.h"

#include "vphysics_interface.h"
#include "studio.h"
#include "props.h"

#include "photo.h"
#include "env_dof_controller.h"

#include "util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IPhysicsCollision *s_pPhysCollision = NULL;

ConVar camera_capture_distance( "camera_capture_distance", "10000.0", FCVAR_CHEAT );
ConVar camera_allow_zoom( "camera_allow_zoom", "1", FCVAR_CHEAT );
ConVar sv_camera_debug_capture( "sv_camera_debug_capture", "0", FCVAR_CHEAT, "Highlights the camera's capture bounds and the potential capturable entities." );
ConVar sv_camera_capture_box_size( "sv_camera_capture_box_size", "15", FCVAR_CHEAT );

bool g_bAllOnCapturedChainedToBase;	// For catching errors in leaf classes


IMPLEMENT_SERVERCLASS_ST(CWeaponCamera, DT_WeaponCamera)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_camera, CWeaponCamera );
PRECACHE_WEAPON_REGISTER( weapon_camera );

BEGIN_DATADESC( CWeaponCamera )

	DEFINE_FIELD( m_hDOFController, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTargetDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDOFDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTargetBlur, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDOFBlur, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDOFRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTargetRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_CurIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_bInViewfinder, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bFirstPresentation, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_bCanZoom, FIELD_BOOLEAN, "canzoom" ),
	DEFINE_KEYFIELD( m_bCanScaleCapturedObjects, FIELD_BOOLEAN, "canscale" ),
	DEFINE_KEYFIELD( m_nNumCaptureSlots, FIELD_INTEGER, "captureslots" ),

	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetNumCaptureSlots", InputSetNumCaptureSlots ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetZoomAbility", InputSetZoomAbility ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetScaleAbility", InputSetScaleAbility ),

END_DATADESC()

acttable_t	CWeaponCamera::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE( CWeaponCamera );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponCamera::CWeaponCamera( void ) : m_CurIndex( -1 ), m_bInViewfinder( false ), m_bFirstPresentation( true )
{
	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;

	m_bFiresUnderwater	= true;

	m_nNumCaptureSlots			= g_CurMaxInvPhotos;
	m_bCanZoom					= false;
	m_bCanScaleCapturedObjects	= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCamera::~CWeaponCamera( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCamera::Precache( void )
{
	PrecacheScriptSound( "Weapon_Camera.Capture" );
	PrecacheScriptSound( "Weapon_Camera.Release" );

	PrecacheScriptSound( "Weapon_Portalgun.fire_blue" );
	PrecacheScriptSound( "Weapon_Portalgun.fire_red" );

	PrecacheParticleSystem( "portal_dematerialize" );
	PrecacheParticleSystem( "portal_rematerialize" );

	BaseClass::Precache();
}


void CWeaponCamera::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	string_t iszCacheName = GetEntityName();
	BaseClass::OnPickedUp( pNewOwner );
	//HACK: Base class weapons clear there name to avoid entity IO, but we don't want that
	//behavior. This avoids somewhat messier player proxy logic in the map.
	SetName( iszCacheName );

	// Borked for multiplayer
	Assert( !GameRules()->IsMultiplayer() );
	g_CurMaxInvPhotos = g_CurMaxInvPhotos; // m_nNumCaptureSlots; NOTE: Multiple slots have been removed!

	CPortal_Player* pPlayer = dynamic_cast<CPortal_Player*>(pNewOwner);
	if ( pPlayer && (pPlayer->HasNamedPlayerItem( "weapon_placement" ) == false) )
	{
		pPlayer->GiveNamedItem( "weapon_placement" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Stop any effects we're doing
//-----------------------------------------------------------------------------
bool CWeaponCamera::Holster( CBaseCombatWeapon *pNextWeapon )
{
	if ( BaseClass::Holster( pNextWeapon ) == false )
		return false;

	DOFControlSettings_t settings;
	settings.flFarBlurRadius = 0;
	settings.flNearBlurRadius = 0;
	settings.flNearBlurDepth = 0;
	settings.flNearFocusDistance = 0;
	settings.flFarBlurDepth = 0;
	settings.flFarFocusDistance = 0;

	m_flTargetRadius = 0;
	m_flDOFRadius = 0;

	m_flTargetBlur = 0;
	m_flDOFBlur = 0;

	m_flTargetDist = 0;
	m_flDOFDist = 0;

	// Set it!
	m_hDOFController->SetControllerState( settings );

	// Stop zooming!
	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->SetFOV( this, 0, 0.25f );
		pPlayer->StopZooming();
	}

	m_bInViewfinder = false;
	return true;
}


class CCaptureableEntityEnumerator: public IEntityEnumerator
{
public:
	CCaptureableEntityEnumerator( const Ray_t& ray ):
	m_ray( ray ),
	m_flLargestDot( -1.0f ),
	m_pBestEnt( NULL )
	{
		m_flClosestDist = ray.m_Delta.Length() + 1.0f;
	}

	// This gets called for each entity in a box traced along the crosshair, and 
	// tries to guess which one the player wants to pick up.
	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

		if ( UTIL_ObjectMayBeCaptured( pEnt ) && pEnt->CollisionProp() )
		{
			if ( sv_camera_debug_capture.GetBool() )
			{
				// Highlight as potential candidate
				NDebugOverlay::EntityBounds( pEnt, 0, 255, 0, 0, 0.1f );
			}

			// Use the closest point on their aabb
			Vector vHitPoint;
			Vector mins, maxs;
			pEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
			CalcClosestPointOnAABB( mins, maxs, m_ray.m_Start, vHitPoint );
			
			// setup for look direction culling
			Vector vDirToHitPoint = vHitPoint - m_ray.m_Start;
			float flDist = VectorNormalize( vDirToHitPoint );

			Vector vRayDir = m_ray.m_Delta;
			VectorNormalize( vRayDir );
			float flDot = vDirToHitPoint.Dot( vRayDir );

			// This means both are out of the crosshair, 
			// choose this candidate if it's closer to the center of the screen
			if ( flDot > m_flLargestDot )
			{
				// This is a better choice, keep it
				m_flClosestDist = flDist;
				m_flLargestDot = flDot;
				m_pBestEnt = pEnt;
				return true;
			}
		}

		return true;
	}

	inline CBaseEntity* GetBestCaptureEntity( void ) { return m_pBestEnt; }

private:
	CBaseEntity*	m_pBestEnt;		// Best capture candidate found 
	const Ray_t&	m_ray;			// Copy of the ray cast for the enumerator, used for tighter checks after finding candidates
	float			m_flClosestDist;	// The distance from the point hit on pBestEnt to the ray's origin
	float			m_flLargestDot;		// keep the one closest to the center of the crosshair
};

//-----------------------------------------------------------------------------
// Purpose: Find a object that can be captured along the specified ray
// Input  : &vecOrigin - start of ray
//			&vecDir - ray dir
//			iSweptBoxSize - size of the mins/maxes
// Output : CBaseEntity
//-------------------------------------\----------------------------------------
CBaseEntity * CWeaponCamera::FindFirstCapturableObject( const Vector &vecOrigin, const Vector &vecDir, const Vector& vecSweptBoxMins, const Vector& vecSweptBoxMaxs )
{
	Vector vecEnd = vecOrigin + vecDir.Normalized()*MAX_TRACE_LENGTH; //camera_capture_distance.GetFloat();

	// Make sure the path between the target ent and the camera is unobstructed
	trace_t tr;
	CTraceFilterEitherOfTwoCollisionGroups filter( GetOwner(), COLLISION_GROUP_NONE, COLLISION_GROUP_CAMERA_SOLID );
	UTIL_TraceLine( vecOrigin, vecEnd, MASK_SHOT, &filter, &tr );

	CBaseEntity* pCaptureEnt = NULL;

	// early out if there is nothing blocking our target, and it's directly under our crosshair
	if ( UTIL_ObjectMayBeCaptured( tr.m_pEnt ) )
	{
		pCaptureEnt = tr.m_pEnt;
	}
	else
	{
		// Try capturing through portals
		CProp_Portal* pHitPortal = NULL;
		Ray_t rayThroughPortals;
		rayThroughPortals.Init( vecOrigin, vecEnd );
		if ( UTIL_DidTraceTouchPortals( rayThroughPortals, tr, &pHitPortal ) && pHitPortal && pHitPortal->IsActivedAndLinked() )
		{
			// non-tunnel portals get traced through.
			trace_t trPortal;
			UTIL_Portal_TraceRay( rayThroughPortals, MASK_SHOT, &filter, &trPortal, false );

			// Test what we hit on the other side of the portal for capturability
			if ( UTIL_ObjectMayBeCaptured( trPortal.m_pEnt ) )
			{
				pCaptureEnt = trPortal.m_pEnt;
			}
			else
			{
				// If we didnt hit anything valid in the world, or through the portal
				// then take the actual portal we hit.
				pCaptureEnt = pHitPortal->GetOwnerEntity() ? pHitPortal->GetOwnerEntity() : pHitPortal;
			}

		}
	}

	// If we got something valid out of the direct point traces, take it.
	if ( pCaptureEnt )
	{
		if ( sv_camera_debug_capture.GetBool() )
		{
			NDebugOverlay::EntityBounds( pCaptureEnt, 255, 0, 0, 64, 0.1f );
		}
		
		return pCaptureEnt;
	}

	// Below here, we didn't get any valid entity under the crosshair. 
	// Make a guess as to what the player wants to pick up.
	if ( sv_camera_debug_capture.GetBool() )
	{
		// highlight the swept box
		QAngle vAngles;
		VectorAngles( vecDir, vAngles );
		NDebugOverlay::SweptBox( vecOrigin, vecEnd, vecSweptBoxMins, vecSweptBoxMaxs, vAngles, 255, 0, 0, 128, 0.1f );
	}

	// If there is no capturable entity directly under the crosshair, sweep a large box
	// down the crosshair and try to guess what the player is trying to pick up
	Ray_t ray;
	ray.Init( vecOrigin, vecEnd, vecSweptBoxMins, vecSweptBoxMaxs );
	CCaptureableEntityEnumerator CapEnum( ray );
	enginetrace->EnumerateEntities( ray, false, &CapEnum );
	CBaseEntity* pEnt = CapEnum.GetBestCaptureEntity();

	if ( !pEnt )
		return NULL; 

	// Make sure the best-guess candidate doesn't get picked up through walls.
	trace_t trCandidate;
	enginetrace->ClipRayToEntity( ray, MASK_ALL, pEnt, &trCandidate );
	if ( !tr.DidHit() )
	{
		Assert( 0 );
	}

	if ( tr.fraction < trCandidate.fraction )
		return NULL;

	if ( sv_camera_debug_capture.GetBool() )
	{
		// Highlight the best-guess candidate
		NDebugOverlay::EntityBounds( pEnt, 128, 128, 0, 64, 0.1f );		
	}

	return pEnt;
}



//----------------------------------------------------------------------------
// Purpose: LEGACY- Keeping this around for easier regression diagnosis
// Traces a box to test for any capturable object in front of the camera
//-----------------------------------------------------------------------------
CBaseEntity * CWeaponCamera::CameraTraceHull( const Vector& vecStart, const Vector& vecEnd, const Vector& vecMins, const Vector& vecMaxs, trace_t* pTrace )
{
	AssertMsg( 0, "LEGACY-- this function has fallen out of use. Step through it to make sure it's doing what you want." );

	if ( !pTrace )
	{
		Assert( 0 );
		return NULL;
	}

	// Kinda ugly: This ray is used conditionally if we're using non-fizzling portal doors
	// or if we're capturing doors on miss. 
	Ray_t ray;
	ray.Init( vecStart, vecEnd );

	CTraceFilterSimple filter( GetOwner(), COLLISION_GROUP_NONE );

	UTIL_TraceHull( vecStart, vecEnd, vecMins, vecMaxs, MASK_SHOT, &filter, pTrace );
	

	CBaseEntity* pHitEnt = pTrace->m_pEnt;

	// Photo erasers block us!
	if ( pHitEnt && FClassnameIs( pHitEnt, "trigger_photo_eraser" ) )
	{
		UTIL_ClearTrace( *pTrace );
		return NULL;
	}

	// Test if we can pick it up
	if ( UTIL_ObjectMayBeCaptured( pHitEnt ) )
	{
		return pHitEnt;
	}
	
	// FIXME: For now, if it's not on screen, we don't like it
	UTIL_ClearTrace( *pTrace );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Determine whether or not this object may be captured
// Input  : *pObject - Object to test
//			&vecViewPos - Where we're looking from (used for LOS calculations)
//-----------------------------------------------------------------------------
bool UTIL_ObjectMayBeCaptured( CBaseEntity *pObject )
{
	// Must exist!
	if ( pObject == NULL )
		return false;

	if ( pObject->IsWorld() )
		return false;

	if ( FClassnameIs( pObject, "portalsimulator_collisionentity" ) )
		return false;

	// Must be visible
	if ( pObject->GetEffects() & EF_NODRAW )
		return false;

	// Cannot be the player
	if ( pObject->IsPlayer() )
		return false;

	// Must have a model and be capturable
	CBaseAnimating *pAnimating = pObject->GetBaseAnimating();
	if ( pAnimating == NULL || pAnimating->MayBeCaptured() == false )
		return false;

	// special case for the android missile, currently they are based off the hunter flechette
	// so they might have VPhysics, or have their motion disabled
	if( FClassnameIs( pObject, "android_missile" ) )
		return true;

	// Must be physical
	// HACK: We're allowing portal doors through this check since they cant be scaled.
	// TODO: Once we start capturing more non-vphysics objects (like brush models) this will need to be revisited.
	IPhysicsObject *pPhysObject = pObject->VPhysicsGetObject();	
	if ( pPhysObject == NULL && !FClassnameIs( pObject, "prop_portal_tunnel" ) )
		return false;

	// Only grab "active" physical targets
	if ( pPhysObject && pPhysObject->IsCollisionEnabled() == false )
		return false;

	// Ragdolls cannot currently be picked up
	if ( FClassnameIs( pObject, "prop_ragdoll" ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
void CWeaponCamera::SaveEntityConnections( CBaseEntity *pTarget, CaptureInfo_t &info )
{
	datamap_t *dmap = pTarget->GetDataDescMap();
	while ( dmap )
	{
		int fields = dmap->dataNumFields;
		for ( int i = 0; i < fields; i++ )
		{
			typedescription_t *dataDesc = &dmap->dataDesc[i];
			if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
			{
				CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((int)this + (int)dataDesc->fieldOffset[0]);
				if ( pOutput->NumberOfElements() )
				{
					// FIXME: Save this, et al
				}
			}
		}

		dmap = dmap->baseMap;
	}
}
*/

// read space delimited floats from a string, store them in a float array
int CopyScaleValuesToArray( char* pszValueString, float* pflFloatArray, int iSize, const char* pszModelName )
{
	if ( !pszValueString )
		return 0;
	
	char* pTok = strtok( pszValueString, " " );

	if ( !pTok )
		return 0;

	// Fill the the array with the floats in pszValueString
	int i = 0;
	while ( pTok != NULL )
	{
		pflFloatArray[i++] = atof( pTok );
		if ( i >= iSize )
		{
			// If the QC specifies more than our max, just use the iSize first entries and ignore the rest.
			Assert( 0 );
			Warning( "Model %s specifies too many scale values in the 'camera_scale_values' section (max is %i up, %i down.)\n", pszModelName, CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS, CAPTURE_INFO_MAX_CUSTOM_SCALE_DOWN_MULTIPLIERS );
			return iSize-1;
		}
		pTok = strtok( NULL, " " );
	}
	
	// Fill the rest of the array with safety values.
	for ( int j = i; j < iSize; ++j )
	{
		pflFloatArray[j] = 1.0f;
	}
	
	Assert( i > 0 );
	return i;
}
 
//-----------------------------------------------------------------------------
// Purpose: Fill in CameraInfo_CustomScaleData_t with any custom scale values specified by the model.
// Input  : *pTarget - entity with a model that may have custom scale values specified in the QC
//			&info - CaptureInfo_t to who's CameraInfo_CustomScaleData_t is to be filled with any custom scale data found.
//-----------------------------------------------------------------------------
void ReadCustomScaleValues( CBaseEntity *pTarget, CaptureInfo_t &info )
{
	if ( !pTarget )
	{
		Assert( 0 );
		return;
	}

	info.bHasCustomScaleData = false;
	Q_memset( &(info.customScaleData), NULL, sizeof( CameraInfo_ScaleData_t ) );

	KeyValues *modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( pTarget->GetModel() ), modelinfo->GetModelKeyValueText( pTarget->GetModel() ) ) )
	{
		KeyValues *pkvCustomScaleValues = modelKeyValues->FindKey("camera_scale_values");
		if ( pkvCustomScaleValues )
		{
			// Found custom scale values, use this instead of the values in the placement query
			info.bHasCustomScaleData = true;
			
			char* pszScaleUpMultipliers		= (char*)pkvCustomScaleValues->GetString( "scale_up_multipliers" );
			char* pszScaleDownMultipliers	= (char*)pkvCustomScaleValues->GetString( "scale_down_multipliers" );

			// Read in space delimited floats for scale up/down values
			info.customScaleData.nNumScaleUpLevels = CopyScaleValuesToArray( pszScaleUpMultipliers, info.customScaleData.flScaleUpLevelMultipliers, CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS, STRING( pTarget->GetModelName() ) );
			info.customScaleData.nNumScaleDownLevels = CopyScaleValuesToArray( pszScaleDownMultipliers, info.customScaleData.flScaleDownLevelMultipliers, CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS, STRING( pTarget->GetModelName() ) );								
		}
	}

	modelKeyValues->deleteThis();
	modelKeyValues = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Fill in an empty CaptureInfo_t with required info to restore an entity.
// Input  : CaptureInfo - empty struct to fill in
//			pObject - the entity who's being captured
//			pCapturingPlayer - The player doing the capturing (if any). 
//-----------------------------------------------------------------------------
bool UTIL_InitCaptureInfo( CaptureInfo_t& CaptureInfo, CBaseEntity* pObject, CBasePlayer* pCapturingPlayer /*= NULL*/ )
{
	// Save our old position
	CaptureInfo.vecOldOrigin = pObject->GetAbsOrigin();
	CaptureInfo.vecOldAngles = pObject->GetAbsAngles();
	
	// Keep velocity after we restore
	IPhysicsObject* pPhysObj = pObject->VPhysicsGetObject();
	if ( pPhysObj )
	{
		pPhysObj->GetVelocity( &CaptureInfo.vecVelocity, &CaptureInfo.vecAngVelocity );
	}

	// FIXME: Yes?
	CBaseAnimating *pAnim = pObject->GetBaseAnimating();
	Assert( pAnim ); // We can't be here!
	if ( pAnim == NULL )
		return false;

	CaptureInfo.nOldScaleLevel = pAnim->GetObjectScaleLevel();
	CaptureInfo.pPlacementQuery = pObject->Get_CPhotoPlacementQuery();

	// Read Check QC for user specified scale values.
	ReadCustomScaleValues( pObject, CaptureInfo );

	// If we're keeping the real ent around, get a pointer to it here
	// so it's recorded in the photo inventory
	CaptureInfo.hCapturedEnt = pObject;

	return true;
}

//
//
//

class CAfterImage : public CBaseAnimating
{
public:
	DECLARE_CLASS( CAfterImage, CBaseAnimating );
	DECLARE_DATADESC();

	static CAfterImage *CreateAfterImage( CBaseAnimating *pSourceEntity )
	{
		CAfterImage *pImage = (CAfterImage *) CreateEntityByName( "_after_image" );
		if ( pImage )
		{
			pImage->SetAbsOrigin( pSourceEntity->GetAbsOrigin() );
			pImage->SetAbsAngles( pSourceEntity->GetAbsAngles() );
			pImage->SetModelName( pSourceEntity->GetModelName() );
			DispatchSpawn( pImage );
			pImage->SetModelScale( pSourceEntity->GetModelScale() );
			pImage->SetCycle( pSourceEntity->GetCycle() );
			pImage->SetSequence( pSourceEntity->GetSequence() );
		}

		return pImage;
	}

	void FadeThink( void )
	{
		float flPerc = RemapValClamped( gpGlobals->curtime, m_flStartTime, m_flFadeTime, 1.0f, 0.0f );
		int nAlpha = 164.0f * Bias( flPerc, 0.1f );

		SetRenderAlpha( nAlpha );

		if ( GetRenderAlpha() <= 1 )
		{
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 0.1f );
			return;
		}
		
		SetThink( &CAfterImage::FadeThink );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}

	virtual void Spawn( void )
	{
		Precache();

		SetModel( STRING( GetModelName() ) );
		SetSolid( SOLID_NONE );
		SetEffects( EF_NOSHADOW );

		SetRenderColor( 255, 255, 255 );
		SetRenderAlpha( 164 );
		SetRenderMode( kRenderTransColor );	

		m_flStartTime = gpGlobals->curtime;
		m_flFadeTime = gpGlobals->curtime + 1.0f;
		FadeThink();
		
		// Make us as small as the zombie was
		BaseClass::Spawn();
	}

private:
	float	m_flStartTime;
	float	m_flFadeTime;
};

LINK_ENTITY_TO_CLASS( _after_image, CAfterImage );

BEGIN_DATADESC( CAfterImage )
	DEFINE_THINKFUNC( FadeThink )
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Capture the object using the save/restore system to hold its data locally to ourselves
// Input  : *pObject - Object to capture
//-----------------------------------------------------------------------------
void CWeaponCamera::CaptureObject( CBaseEntity *pObject )
{
	CBaseAnimating* pAnim = pObject->GetBaseAnimating();
	Assert( pAnim );

	if ( pAnim && pAnim->TestPreCapture() == false )
		return;
	
	// Capture data relevant to recreating this entity
	CaptureInfo_t CaptureInfo;

	CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
	UTIL_InitCaptureInfo( CaptureInfo, pObject, pPlayer );

	pObject->SetStasis( true );
	
	// Don't move in heirarchy
	pObject->SetParent( NULL, -1 );

	// Add it to the inventory
	int nIndex = Photo_Add( &CaptureInfo );
	if ( pPlayer )
	{
		pPlayer->SetSelectedPhoto( nIndex );
		pPlayer->FlashInventory( 2.0f, FLASH_INVENTORY_ADDED );
	}

	// Send a callback
	if ( pAnim )
	{
		//
		CAfterImage::CreateAfterImage( pAnim );
		//

		g_bAllOnCapturedChainedToBase = false;
		pAnim->OnCaptured();

		// NOTE: If you're here, you forgot to chain to the base class in an OnCaptured implementation! 
		Assert( g_bAllOnCapturedChainedToBase );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Flash effect and noise
//-----------------------------------------------------------------------------
void CWeaponCamera::CaptureEffect( const Vector &vecPosition )
{
	CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->Flash( 0.5f, vecPosition );
	}

	EmitSound( "Weapon_Camera.Capture" );
}

unsigned int MAX_OBJECTS_TO_TEST = 128;									// Maximum number of objects to test
unsigned int MAX_OBJECT_DISTANCE = (150*12);							// Maximum distance away from the camera
unsigned int MAX_OBJECT_DISTANCE_SQR = Square( MAX_OBJECT_DISTANCE );	// Maximum distance away from the camera

float MAX_OBJECT_SKEW = cos(DEG2RAD(15.0f));						// Maximum skew away from our center the object may be

const int	CAMERA_FOV_START = 70;
const float CAMERA_FOV_RATE	= 0.2f;
const int	CAMERA_FOV_INCR = 15;
const int	CAMERA_FOV_MAX = (CAMERA_FOV_START+CAMERA_FOV_INCR*2);
const int	CAMERA_FOV_MIN = (CAMERA_FOV_START-CAMERA_FOV_INCR*2);

//-----------------------------------------------------------------------------
// Purpose: Mouse wheelin'
//-----------------------------------------------------------------------------
void CWeaponCamera::OnMouseWheel( int nDirection )
{
	if ( camera_allow_zoom.GetBool() == false || !m_bCanZoom )
		return;

	// We can only do this if we're in the viewfinder
	if ( m_bInViewfinder == false )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	float flTargetFOV = pPlayer->GetFOV();
	if ( nDirection == MWHEEL_UP )
	{
		flTargetFOV -= CAMERA_FOV_INCR;
	}
	else if ( nDirection == MWHEEL_DOWN )
	{
		flTargetFOV += CAMERA_FOV_INCR;
	}

	flTargetFOV = clamp( flTargetFOV, CAMERA_FOV_MIN, CAMERA_FOV_MAX );
	pPlayer->SetFOV( this, flTargetFOV, CAMERA_FOV_RATE );
}

//-----------------------------------------------------------------------------
// Purpose: Capture an object
//-----------------------------------------------------------------------------
void CWeaponCamera::PrimaryAttack( void )
{
	// Get our owner
	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// If we've taken a picture, go back to NULL
	if ( Photo_Count() >= 1 )
	{
		// Switch away to the photo placement mode
		pPlayer->SwitchToNextBestWeapon( this );
		pPlayer->ControlHelperAnimate( CONTROL_STATE_PICTURE );
		return;
	}

	if ( m_bInViewfinder == false )
	{
		pPlayer->ControlHelperAnimate( CONTROL_STATE_CAMERA );
		m_bInViewfinder = true;
		pPlayer->StartZooming();
		pPlayer->SetFOV( this, CAMERA_FOV_START, CAMERA_FOV_RATE );
		UpdateDOF( true );
		return;
	}

	// Always introduce some sort of pause
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;

	// Only allow this if we can add photographs
	if ( Photo_CanAdd() == false )
	{
		pPlayer->FlashDenyIndicator( 1.0f, FLASH_INDICATOR_FULL );
		pPlayer->FlashInventory( 1.0f, FLASH_INVENTORY_FULL );
		return;
	}

	// We look from our own eyes in the proper direction
	Vector vecViewPos = pPlayer->EyePosition();
	Vector vecViewDir = pPlayer->EyeDirection3D();

	Vector vecCaptureBoxMins( -sv_camera_capture_box_size.GetFloat(), -sv_camera_capture_box_size.GetFloat(), -sv_camera_capture_box_size.GetFloat() );
	Vector vecCaptureBoxMaxs( sv_camera_capture_box_size.GetFloat(), sv_camera_capture_box_size.GetFloat(), sv_camera_capture_box_size.GetFloat() );

	// Search for a capture target
	CBaseEntity *pBestEntity = FindFirstCapturableObject( vecViewPos, vecViewDir, vecCaptureBoxMins, vecCaptureBoxMaxs );
	if ( pBestEntity == NULL )
	{
		pPlayer->FlashDenyIndicator( 0.75f, FLASH_INDICATOR_INVALID );
		return;
	}

	// Do flashy effects
	CaptureEffect( pBestEntity->WorldSpaceCenter() );
	
	// Capture it
	CaptureObject( pBestEntity );
	
	pPlayer->StopZooming();
	pPlayer->SetFOV( this, 0, 0.0f );
	m_bInViewfinder = false;
	SendWeaponAnim( ACT_VM_RELOAD );

	pPlayer->ControlHelperAnimate( CONTROL_STATE_NEUTRAL, true );

	// Once we've captured, switch to another weapon
	pPlayer->ControlHelperAnimate( CONTROL_STATE_NEUTRAL );
	pPlayer->SwitchToNextBestWeapon( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCamera::SecondaryAttack( void )
{
	// Get our owner
	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	if ( pPlayer->IsZooming() == false )
	{
		// Delay our next attempt
		if ( Photo_Count() == 0 )
		{
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;
			((CBasePlayer *)pPlayer)->PlayUseDenySound();
			return;
		}

		pPlayer->SwitchToNextBestWeapon( this );
		pPlayer->ControlHelperAnimate( CONTROL_STATE_PICTURE );
	}
	else
	{
		// Stop zooming!
		pPlayer->SetFOV( this, 0, 0.25f );
		pPlayer->StopZooming();
		m_bInViewfinder = false;
		SendWeaponAnim( ACT_VM_DRAW );
		pPlayer->ControlHelperAnimate( CONTROL_STATE_NEUTRAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCamera::Deploy( void )
{
	bool bDeployed = BaseClass::Deploy();
	if ( bDeployed )
	{
		m_bInViewfinder = false;
		m_flNextPrimaryAttack = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		if ( pOwner )
		{
			pOwner->SetNextAttack( gpGlobals->curtime );
		}
	}
	
	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->SetPlacingPhoto( false );
	}

	return bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCamera::UpdateLocators( void )
{
	if ( m_bInViewfinder == false )
		return;

	int nCurrentIndex = 0;
	int pEntityIndices[16];
	memset( pEntityIndices, -1, sizeof(pEntityIndices) );

	CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( GetOwner() );

	// Update all chickens
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "npc_chicken" ) ) != NULL && nCurrentIndex < ARRAYSIZE( pEntityIndices ) )
	{
		if ( pEntity->IsInStasis() )
			continue;

		// Must have a line-of-sight to the target
		trace_t tr;
		UTIL_TraceLine( pPlayer->EyePosition(), pEntity->WorldSpaceCenter(), MASK_OPAQUE, pEntity, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction == 1.0f )
				continue;

		// Valid, take it
		pEntityIndices[nCurrentIndex++] = pEntity->entindex();
	}
	
	// Send it over the wire
	pPlayer->UpdateLocatorEntityIndices( pEntityIndices, ARRAYSIZE(pEntityIndices) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCamera::UpdateDOF( bool bImmediate /*= false*/ )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	// We look from our own eyes in the proper direction
	Vector vecViewPos = pOwner->EyePosition();
	Vector vecViewDir = pOwner->EyeDirection3D();

	// Find or create our DOF controller to start with
	if ( m_hDOFController == NULL )
	{
		m_hDOFController = (CEnvDOFController *) gEntList.FindEntityByClassname( NULL, "env_dof_controller" );
		if ( m_hDOFController == NULL )
		{
			// Create it
			m_hDOFController = (CEnvDOFController *) CreateEntityByName( "env_dof_controller" );
			Assert( m_hDOFController != NULL );
		}
	}
	// Must have one to bother with this
	if ( m_hDOFController == NULL )
		return;

	if ( m_bInViewfinder == false )
	{
		m_flTargetBlur = 0.0f;	// Only fade out the blur, not the distance
	}
	else
	{
		Vector vecCaptureBoxMins( -sv_camera_capture_box_size.GetFloat(), -sv_camera_capture_box_size.GetFloat(), -sv_camera_capture_box_size.GetFloat() );
		Vector vecCaptureBoxMaxs( sv_camera_capture_box_size.GetFloat(), sv_camera_capture_box_size.GetFloat(), sv_camera_capture_box_size.GetFloat() );

		CBaseEntity *pBestEntity = FindFirstCapturableObject( vecViewPos, vecViewDir, vecCaptureBoxMins, vecCaptureBoxMaxs );
		if ( pBestEntity == NULL  )
		{
			m_flTargetBlur = 4.0f;
			m_flTargetDist = 1.0f;
			m_flTargetRadius = 1.0f;
		}
		else
		{
			// Get the direction and distance
			m_flTargetDist = ( pBestEntity->WorldSpaceCenter() - vecViewPos ).Length();
			m_flTargetBlur = 10.0f;
			m_flTargetRadius = pBestEntity->CollisionProp()->BoundingRadius2D();
		}
	}

	// Decide if we're snapping to the new location or blending
	if ( bImmediate )
	{
		m_flDOFBlur = m_flTargetBlur;
		m_flDOFDist = m_flTargetDist;
		m_flDOFRadius = m_flTargetRadius;
	}
	else
	{
		m_flDOFBlur = ( m_flDOFBlur * 0.9f ) + ( m_flTargetBlur * 0.1f );
		m_flDOFDist = ( m_flDOFDist * 0.9f ) + ( m_flTargetDist * 0.1f );
		m_flDOFRadius = ( m_flDOFRadius * 0.9f ) + ( m_flTargetRadius * 0.1f );
	}

	DOFControlSettings_t settings;
	settings.flFarBlurRadius = m_flDOFBlur;
	settings.flNearBlurRadius = m_flDOFBlur * 0.75f;

	// Create a ramped focus around the object in question
	settings.flNearBlurDepth = m_flDOFDist - m_flDOFRadius * 8.0f;
	settings.flNearFocusDistance = m_flDOFDist;

	settings.flFarBlurDepth = m_flDOFDist + m_flDOFRadius * 8.0f;
	settings.flFarFocusDistance = m_flDOFDist;

	// Set it!
	m_hDOFController->SetControllerState( settings );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCamera::ItemPostFrame( void )
{
	// FIXME: Break out the bits we need so that we can override the button handling, but allow the base class
	//			to do other work that may be important to it -- jdw

	// BaseClass::ItemPostFrame();
	
	CPortal_Player *pOwner = (CPortal_Player *) ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	bool bWeaponActed = false;
	if ( m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		// Override how we deal with our buttons and only act on the initial edge trigger (not held buttons)
		if ( ( pOwner->m_afButtonPressed & IN_ATTACK ) && ( pOwner->m_afButtonLast & IN_ATTACK ) == false )
		{
			PrimaryAttack();
			bWeaponActed = true;
		}
	}
	
	if ( m_flNextSecondaryAttack < gpGlobals->curtime )
	{
		if ( ( pOwner->m_afButtonPressed & IN_ATTACK2 ) && ( pOwner->m_afButtonLast & IN_ATTACK2 ) == false )
		{
			SecondaryAttack();
			bWeaponActed = true;
		}
	}
	
	// Do nothing
	if ( bWeaponActed == false )
	{
		WeaponIdle();
	}

	// Update the locators (TEMP!)
	UpdateLocators();

	// Update our depth-of-field effect
	UpdateDOF( false );
}

void CWeaponCamera::InputSetNumCaptureSlots( inputdata_t& input )
{
	SetNumCaptureSlots( input.value.Int() );
}

void CWeaponCamera::SetNumCaptureSlots( int iNewSlotCount )
{
	//TODO: Inventory slots are not yet dynamic, just capping based on camera's number. 
	//if we revisit the inventory this should change.
	if ( (iNewSlotCount < 0) || (iNewSlotCount > g_CurMaxInvPhotos) )
	{
		Assert ( 0 );
		Warning( "weapon_camera %s received 'SetNumCaptureSlots' input with invalid max slot number (must be between 0 and %d, given %i).\n", GetDebugName(), g_CurMaxInvPhotos, iNewSlotCount );
		return;
	}
	
	// if we're downsizing, clear out any used camera slots safely
	for ( int i = g_CurMaxInvPhotos; i > iNewSlotCount; --i )
	{
		Photo_Remove( i-1 );
	}

	g_CurMaxInvPhotos = iNewSlotCount;
}

void CWeaponCamera::InputSetZoomAbility( inputdata_t& input )
{
	SetZoomAbility( input.value.Bool() );
}

void CWeaponCamera::SetZoomAbility( bool bCanZoom )
{
	m_bCanZoom = bCanZoom;
}

void CWeaponCamera::InputSetScaleAbility( inputdata_t& input )
{
	SetScaleAbility( input.value.Bool() );
}

void CWeaponCamera::SetScaleAbility( bool bCanScale )
{
	m_bCanScaleCapturedObjects = bCanScale;
}
