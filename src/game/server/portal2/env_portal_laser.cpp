//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "envlaser.h"
#include "sprite.h"
#include "decals.h"
#include "prop_portal.h"
#include "prop_portal_shared.h"
#include "soundenvelope.h"			// for looping sound effects
#include "ai_utils.h"
#include "ai_network.h"
#include "ieffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MAX_CHILD_BEAMS	16

class CPortalLaser : public CPointEntity
{
	DECLARE_CLASS( CPortalLaser, CPointEntity );

public:
	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	Activate( void );
	virtual void	UpdateOnRemove( void );

	void InputTurnOn( inputdata_t &inputdata );
	void InputTurnOff( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	void	TurnOn( void );
	void	TurnOff( void );
	bool	IsOn( void );

	void	FireAtPoint( trace_t &tr, bool bImpact =true );
	void	StrikeThink( void );
	void	BeamDamage( trace_t *ptr );

	void	HideChildBeams( void );
	void	FireLaser( int nNumBeams, const Vector &vecStart, const Vector &vecDirection );
	void	UpdateSoundPosition( const Vector &vecStart, const Vector &vecEnd );

	Vector					m_vecNearestSoundSource;
	CBaseEntity				*m_pSoundProxy;
	CSoundPatch				*m_pAmbientSound;
	CInfoPlacementHelper	*m_pPlacementHelper;

	CBeam			*m_pChildBeams[MAX_CHILD_BEAMS];
};

LINK_ENTITY_TO_CLASS( env_portal_laser, CPortalLaser );

BEGIN_DATADESC( CPortalLaser )

	// Function Pointers
	DEFINE_FUNCTION( StrikeThink ),

	// Input functions
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),

END_DATADESC()

const char LASER_LOOPING_SOUND[] = { "Laser.BeamLoop" };
const float LASER_DAMAGE_AMOUNT = 1500.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::Spawn( void )
{
	BaseClass::Spawn();

	// We don't have a model, but we still want to think
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	
	// Setup all child lasers
	for ( int i = 0; i < MAX_CHILD_BEAMS; i++ )
	{
		if ( m_pChildBeams[i] == NULL )
		{
			m_pChildBeams[i] = (CBeam *) CBeam::BeamCreate( "sprites/purplelaser1.vmt", 2 );
		}
	}

	SetSolid( SOLID_NONE );	// Remove model & collisions
	Precache();

	// Setup all child lasers
	for ( int i = 0; i < MAX_CHILD_BEAMS; i++ )
	{
		DispatchSpawn( m_pChildBeams[i] );
		m_pChildBeams[i]->Activate();
	}

	m_pPlacementHelper = (CInfoPlacementHelper *) CreateEntityByName( "info_placement_helper" );
	m_pPlacementHelper->SetAbsOrigin( GetAbsOrigin() );
	m_pPlacementHelper->SetAbsAngles( GetAbsAngles() );
	m_pPlacementHelper->KeyValue( "radius", "16" );
	m_pPlacementHelper->KeyValue( "hide_until_placed", "0" );
	DispatchSpawn( m_pPlacementHelper );
	m_pPlacementHelper->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::Activate( void )
{
	BaseClass::Activate();

	if ( m_pSoundProxy == NULL )
	{
		m_pSoundProxy = CreateEntityByName( "info_target" );
		m_pSoundProxy->SetAbsOrigin( GetAbsOrigin() );
		m_pSoundProxy->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	}

	// Create a looping sound we'll track
	if ( m_pAmbientSound == NULL )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( m_pSoundProxy );

		m_pAmbientSound = controller.SoundCreate( filter, m_pSoundProxy->entindex(), LASER_LOOPING_SOUND );
		controller.Play( m_pAmbientSound, 1.0, 100 );
	}	

	// Start up
	TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::UpdateOnRemove( void )
{
	if ( m_pPlacementHelper != NULL )
	{
		UTIL_Remove( m_pPlacementHelper );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::Precache( void )
{
	// Grab the sound name
	PrecacheScriptSound( LASER_LOOPING_SOUND );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the laser is currently active.
//-----------------------------------------------------------------------------
bool CPortalLaser::IsOn( void )
{
	return ( m_pfnThink != NULL );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::InputTurnOn( inputdata_t &inputdata )
{
	if ( IsOn() == false )
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::InputTurnOff( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::InputToggle( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::TurnOff( void )
{
	HideChildBeams();

	SetNextThink( TICK_NEVER_THINK );
	SetThink( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::TurnOn( void )
{
	SetThink( &CPortalLaser::StrikeThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}


void CPortalLaser::BeamDamage( trace_t *ptr )
{
	if ( ptr->fraction != 1.0 && ptr->m_pEnt != NULL )
	{
		CBaseEntity *pHit = ptr->m_pEnt;
		if ( pHit )
		{
			ClearMultiDamage();
			Vector dir = ptr->endpos - GetAbsOrigin();
			VectorNormalize( dir );
			int nDamageType = DMG_BURN;

			CTakeDamageInfo info( this, this, LASER_DAMAGE_AMOUNT * gpGlobals->frametime, nDamageType );
			CalculateMeleeDamageForce( &info, dir, ptr->endpos );
			pHit->DispatchTraceAttack( info, dir, ptr );
			ApplyMultiDamage();
			UTIL_DecalTrace( ptr, "RedGlowFade" );

			CBaseAnimating *pAnim = pHit->GetBaseAnimating();
			if ( pAnim && ( FClassnameIs( pAnim, "npc_portal_turret_floor" ) || 
							FClassnameIs( pAnim, "npc_hover_turret" ) ) )
			{
				// Burn!
				pAnim->Ignite( 30.0f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::FireAtPoint( trace_t &tr, bool bImpact /*=true*/ )
{
	BeamDamage( &tr );	// FIXME: Return!

	if ( bImpact )
	{
		g_pEffects->Sparks( tr.endpos );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::HideChildBeams( void )
{
	for ( int i = 0; i < MAX_CHILD_BEAMS; i++ )
	{
		m_pChildBeams[i]->TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::UpdateSoundPosition( const Vector &vecStart, const Vector &vecEnd )
{
	Vector vecPlayer = AI_GetSinglePlayer()->EyePosition();
	Vector vecNearestPoint;
	CalcClosestPointOnLineSegment( vecPlayer, vecStart, vecEnd, vecNearestPoint );

	float flDistToPlayerSqr = ( vecPlayer - vecNearestPoint ).LengthSqr();
	float flNearestToPlayerSqr = ( vecPlayer - m_vecNearestSoundSource ).LengthSqr();

	// Updated
	if ( flDistToPlayerSqr < flNearestToPlayerSqr || m_vecNearestSoundSource == vec3_invalid )
	{
		m_vecNearestSoundSource = vecNearestPoint;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::FireLaser( int nNumBeams, const Vector &vecStart, const Vector &vecDirection )
{
	// Done
	if ( nNumBeams >= MAX_CHILD_BEAMS )
		return;

	Ray_t ray;
	ray.Init( vecStart, vecStart + ( vecDirection * MAX_TRACE_LENGTH ) );

	trace_t tr;
	UTIL_TraceRay( ray, (MASK_SHOT & ~(CONTENTS_WINDOW|CONTENTS_GRATE)), NULL, COLLISION_GROUP_NONE, &tr );

	// Move our laser sound
	UpdateSoundPosition( tr.startpos, tr.endpos );

	CProp_Portal *pFirstPortal = NULL;
	if ( UTIL_DidTraceTouchPortals( ray, tr, &pFirstPortal ) && pFirstPortal && pFirstPortal->IsActivedAndLinked() )
	{
		// Set our laser to go from our point here to the portal
		m_pChildBeams[nNumBeams]->TurnOff();
		m_pChildBeams[nNumBeams]->PointsInit( vecStart, tr.endpos );
		m_pChildBeams[nNumBeams]->SetAbsOrigin( vecStart );

		// Do damage along the line
		FireAtPoint( tr, false );

		// Transform the ray into the new portal's space
		Ray_t rayTransformed;
		UTIL_Portal_RayTransform( pFirstPortal->MatrixThisToLinked(), ray, rayTransformed );
		Vector vecDirection = rayTransformed.m_Delta;
		VectorNormalize( vecDirection );

		Vector vecStartPos;
		UTIL_Portal_PointTransform( pFirstPortal->MatrixThisToLinked(), tr.endpos, vecStartPos );

		FireLaser( nNumBeams+1, vecStartPos, vecDirection );
		return;
	}

	m_pChildBeams[nNumBeams]->TurnOff();

	// Draw the current beam
	m_pChildBeams[nNumBeams]->PointsInit( vecStart, tr.endpos );
	m_pChildBeams[nNumBeams]->SetAbsOrigin( vecStart );

	bool bReflected = false;

	bool bHitTurret = ( tr.m_pEnt && FClassnameIs( tr.m_pEnt, "npc_portal_turret_floor" ) );
	surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
	if ( pSurfaceData && pSurfaceData->game.material == CHAR_TEX_METAL && bHitTurret == false )
	{
		Vector vecDir = ( tr.endpos - tr.startpos );
		VectorNormalize( vecDir );

		// Bounce!
		Vector vecBounce = -2.0f * tr.plane.normal * DotProduct( vecDir, tr.plane.normal ) + vecDir;
		VectorNormalize( vecBounce );

		FireLaser( (nNumBeams+1), tr.endpos, vecBounce );

		bReflected = true;
	}

	// Put our placement helper wherever we end up
	// NOTE: This might not be that useful if this isn't a portalable surface!
	if ( bReflected == false )
	{
		UTIL_SetOrigin( m_pPlacementHelper, tr.endpos );
	}

	// Do damage at that point
	FireAtPoint( tr, ( bReflected == false ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalLaser::StrikeThink( void )
{
	// First, shut down all beams from our last update
	HideChildBeams();

	// Mark this as needing a new position
	m_vecNearestSoundSource = vec3_invalid;

	// Get the direction we're shooting
	Vector vecDir;
	AngleVectors( GetAbsAngles(), &vecDir );
	FireLaser( 0, GetAbsOrigin(), vecDir );

	// Update where our ambient sound is playing from
	UTIL_SetOrigin( m_pSoundProxy, m_vecNearestSoundSource );
	// NDebugOverlay::Box( m_vecNearestSoundSource, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, 0, 0.05f );

	// Think as soon as we can
	SetNextThink( gpGlobals->curtime );
}


