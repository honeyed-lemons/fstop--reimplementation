//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Zombie spawning tombstone!
//
//===========================================================================//

#include "cbase.h"
#include "monstermaker.h"
#include "ai_basenpc.h"

class CPropTombstone : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropTombstone, CBaseAnimating );
	
			CPropTombstone( void );

	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Precache( void );
	virtual ICapturedObjectPlacementQuery *GetPlacementQuery( void );
	virtual void OnReleased( void );
	virtual void DeathNotice( CBaseEntity *pVictim );

private:
	DECLARE_DATADESC();

	void	SpawnThink( void );
	void	TestValidGround( void );
	void	SetupNextSpawn( void );

	int		m_nNumLiveChildren;
	bool	m_bOnValidGround;
};

const char g_szModelName[] = "models/props_fstop/tombstone001.mdl";

const float RESPAWN_DELAY = 2.0f;

LINK_ENTITY_TO_CLASS( prop_tombstone, CPropTombstone );

BEGIN_DATADESC( CPropTombstone )
	DEFINE_THINKFUNC( SpawnThink ),
	DEFINE_FIELD( m_nNumLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( m_bOnValidGround, FIELD_BOOLEAN ),
END_DATADESC()

CPropTombstone::CPropTombstone( void ) : m_nNumLiveChildren( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::Precache( void )
{
	PrecacheModel( g_szModelName );
	UTIL_PrecacheOther( "npc_zombie" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::Spawn( void )
{
	m_nNumLiveChildren = 0;

	Precache();

	SetModel( g_szModelName );

	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_NONE );

	CreateVPhysics();

	// AddEffects( EF_NOSHADOW );

	BaseClass::Spawn();

	// See if we're on the ground properly
	TestValidGround();

	// If we're on valid ground, spawn a zombie
	// FIXME: This needs to check where the zombie is going to come from as well
	if ( m_bOnValidGround )
	{
		SetNextThink( gpGlobals->curtime + RESPAWN_DELAY );
		SetThink( &CPropTombstone::SpawnThink );
	}
	else
	{
		// TODO: Otherwise we'll need to play an effect and generally just complain
	}
}

bool CPropTombstone::CreateVPhysics( void )
{
	IPhysicsObject *pPhysObj = VPhysicsInitStatic();
	if ( pPhysObj )
	{
		pPhysObj->EnableMotion( false );
		return true;
	}

	// failed to create, probably not exected behavior
	Assert ( 0 );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::SpawnThink( void )
{
	// If we've already spawned a child, then we're done
	if ( m_nNumLiveChildren )
		return;

	// Make an instance of a zombie
	CAI_BaseNPC *pNPC = (CAI_BaseNPC *) CreateEntityByName( "npc_zombie" );
	if ( pNPC == NULL )
		return;

	Vector vOrigin;
	if ( CAI_BaseNPC::FindSpotForNPCInRadius( &vOrigin, GetAbsOrigin(), pNPC, (5.0f*12.0f) ) == false )
	{
		// Remove the entity
		UTIL_RemoveImmediate( pNPC );

		// Try again next round
		SetNextThink( gpGlobals->curtime + RESPAWN_DELAY );
		SetThink( &CPropTombstone::SpawnThink );
		return;
	}

	// Start in that position
	pNPC->SetAbsOrigin( vOrigin );
	
	// Give us a death notice
	pNPC->SetOwnerEntity( this );

	// Spawn properly
	DispatchSpawn( pNPC );

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer == NULL )
		return;

	// FIXME: Know where our enemy is to start with
	pNPC->SetEnemy( pPlayer );
	pNPC->UpdateEnemyMemory( pPlayer, pPlayer->GetAbsOrigin() );

	// Add another live child to the list
	m_nNumLiveChildren++;

	// HACK: Rise out of the ground 
	variant_t emptyVariant;
	pNPC->AcceptInput( "RiseFromGround", this, this, emptyVariant, USE_ON );

	// Wait around until that entity goes
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::TestValidGround()
{
	// Trace underneath us and see if it's a valid place to make zombies
	m_bOnValidGround = false;

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,128), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f )
	{
		surfacedata_t *pSurfaceProp = physprops->GetSurfaceData( tr.surface.surfaceProps );
		char cCurrGameMaterial = pSurfaceProp->game.material;
		if ( cCurrGameMaterial == 'D' )
		{
			m_bOnValidGround = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::SetupNextSpawn( void )
{
	if ( m_nNumLiveChildren <= 0 && m_bOnValidGround )
	{
		m_nNumLiveChildren = 0;
		SetNextThink( gpGlobals->curtime + RESPAWN_DELAY );
		SetThink( &CPropTombstone::SpawnThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::DeathNotice( CBaseEntity *pVictim )
{
	// Decrement our count of active children
	m_nNumLiveChildren--;

	if ( IsInStasis() == false )
	{
		// Start up again if all our children are gone
		SetupNextSpawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTombstone::OnReleased( void )
{
	TestValidGround();
	SetupNextSpawn();

	BaseClass::OnReleased();
}

//------------------------------------------------------------------------------
// Portal tunnel (temp)
//------------------------------------------------------------------------------

class CPropTombstone_PlacementQuery : public CBaseCapturedObjectPlacementQuery
{
	bool GetPlacementPosition( const Vector &vecEndPoint, const Vector &vecEndNormal, CaptureInfo_t &captureInfo, int nScaleStep, Vector *pOriginOut, QAngle *pAnglesOut )
	{
		if ( pOriginOut )
		{
			*pOriginOut = vecEndPoint;
		}

		if ( pAnglesOut )
		{
			*pAnglesOut = QAngle( 0, 0, 0 );
		}

		return true;
	}

	int GetNumScaleUpSteps( const CaptureInfo_t* pCaptureInfo ) { return 2; }
	int GetNumScaleDownSteps( const CaptureInfo_t* pCaptureInfo ) { return 2; }

	float GetScaleForStep( int nScaleStep, const CaptureInfo_t* pCaptureInfo ) 
	{
		switch ( nScaleStep )
		{
		default:
		case 0:
			return 1.0f;
			break;
		case 1:
			return 1.5f;
			break;
		case 2:
			return 2.0f;
			break;
		case -1:
			return 0.75f;
			break;
		case -2:
			return 0.5f;
			break;
		}

		return 1.0f;
	}
};

// Query singleton
CPropTombstone_PlacementQuery g_PropTombstone_PlacementQuery;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

ICapturedObjectPlacementQuery *CPropTombstone::GetPlacementQuery( void ) 
{ 
	return &g_PropTombstone_PlacementQuery; 
}
