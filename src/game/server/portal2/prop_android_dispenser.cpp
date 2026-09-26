//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Dispenser to pop out androids
//
//===========================================================================//
#include "cbase.h"
#include "monstermaker.h"
#include "ai_basenpc.h"


// this is just a passthrough to get around some of the automatic behavior associated with ownership and collision
class CDeathWatcher : public CBaseEntity
{
public:
	DECLARE_CLASS( CDeathWatcher, CBaseEntity );
	virtual void DeathNotice( CBaseEntity *pVictim );
			void LinkToSpawner( CHandle<CBaseEntity> hSpawner );
private:
	DECLARE_DATADESC();

	CHandle<CBaseEntity>		m_hAndroidDispenser;
};

LINK_ENTITY_TO_CLASS( ent_death_watcher, CDeathWatcher );

BEGIN_DATADESC( CDeathWatcher )
	DEFINE_FIELD( m_hAndroidDispenser, FIELD_EHANDLE ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Link to our spawner so we can send back our Death Notices
//-----------------------------------------------------------------------------
void CDeathWatcher::LinkToSpawner( CHandle<CBaseEntity> hSpawner )
{
	m_hAndroidDispenser = hSpawner;
}

//-----------------------------------------------------------------------------
// Purpose: Link to our spawner so we can send back our Death Notices
//-----------------------------------------------------------------------------
void CDeathWatcher::DeathNotice( CBaseEntity *pVictim )
{
	m_hAndroidDispenser->DeathNotice( pVictim );
}

class CPropAndroidDispenser : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropAndroidDispenser, CBaseAnimating );
	
			CPropAndroidDispenser( void );
			~CPropAndroidDispenser( void );

	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Precache( void );
	virtual bool MayBeCaptured( void ) { return false; }
	virtual void DeathNotice( CBaseEntity *pVictim );
			void AnimateThink( void );
			bool IsDispenserBlocked( void );

			void InputEnable( inputdata_t &inputdata );
			void InputDisable( inputdata_t &inputdata );
private:
	DECLARE_DATADESC();

	void	SpawnThink( void );
	void	SetupNextSpawn( void );

	int		m_nNumLiveChildren;
	float	m_flBlockedTime;
	bool	m_bClosePending;

	bool	m_bDisabled;

	COutputEvent				m_OnSpawnNPC;
	COutputEvent				m_OnChildKilled;

	CHandle<CBaseEntity>		m_hDeathWatcher;
	CHandle<CBaseEntity>		m_hSpawnedZombie;
};

const char g_szModelName[] = "models/props_gameplay/bot_spawn.mdl";

const float RESPAWN_DELAY = 2.0f;
const float BLOCKED_DELAY = 4.0f;

LINK_ENTITY_TO_CLASS( prop_android_dispenser, CPropAndroidDispenser );

BEGIN_DATADESC( CPropAndroidDispenser )
	DEFINE_KEYFIELD( m_bDisabled,			FIELD_BOOLEAN,	"StartDisabled" ),

	DEFINE_INPUTFUNC( FIELD_VOID,	"Enable",	InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Disable",	InputDisable ),

	DEFINE_OUTPUT( m_OnSpawnNPC,		"OnSpawnNPC" ),
	DEFINE_OUTPUT( m_OnChildKilled,		"OnChildKilled" ),
	
	DEFINE_THINKFUNC( SpawnThink ),
	DEFINE_THINKFUNC( AnimateThink ),

	DEFINE_FIELD( m_nNumLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( m_flBlockedTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bClosePending, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hDeathWatcher, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpawnedZombie, FIELD_EHANDLE ),
END_DATADESC()

CPropAndroidDispenser::CPropAndroidDispenser( void ) : m_nNumLiveChildren( 0 ), m_flBlockedTime( 0.f ), m_bClosePending( false )
{
}

CPropAndroidDispenser::~CPropAndroidDispenser( void )
{
	if ( m_hDeathWatcher )
	{
		UTIL_Remove( m_hDeathWatcher );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Input hander that starts the spawner
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
	if( m_nNumLiveChildren <= 0 )
	{
		if( !m_bClosePending )
		{
			SetupNextSpawn();
		}
		else
		{
			SetNextThink( gpGlobals->curtime + 0.1f );
			SetThink( &CPropAndroidDispenser::AnimateThink );
		}
	}
	}


//-----------------------------------------------------------------------------
// Purpose: Input hander that stops the spawner
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::Precache( void )
{
	PrecacheModel( g_szModelName );
	UTIL_PrecacheOther( "npc_android" );
	PrecacheScriptSound( "AndroidDispenser.Blocked" );
	PrecacheScriptSound( "AndroidDispenser.Dispense" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::Spawn( void )
{
	m_nNumLiveChildren = 0;

	Precache();

	SetModel( g_szModelName );

	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_NONE );

	CreateVPhysics();

	// AddEffects( EF_NOSHADOW );

	BaseClass::Spawn();
	SetNextThink( gpGlobals->curtime + RESPAWN_DELAY );
	SetThink( &CPropAndroidDispenser::SpawnThink );

	// create something to watch the things we spawn
	if ( m_hDeathWatcher )
	{
		UTIL_Remove( m_hDeathWatcher );
	}

	m_hDeathWatcher = CreateEntityByName( "ent_death_watcher" );
	m_hDeathWatcher->SetAbsOrigin( GetAbsOrigin() );
	CDeathWatcher *pDeathWatcher = (CDeathWatcher *) m_hDeathWatcher.Get();
	pDeathWatcher->LinkToSpawner( this );
	DispatchSpawn( m_hDeathWatcher );
}

bool CPropAndroidDispenser::CreateVPhysics( void )
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
// Purpose: Determine if there are any objects above the spawner that shouldn't be there
//-----------------------------------------------------------------------------
bool CPropAndroidDispenser::IsDispenserBlocked( void )
{
	// see if we should play a blocked animation instead of spawning.
	Vector vecMins( -40.0f, -40.0f, 0 );
	Vector vecMaxs( 40.0f, 40.0f, 40.f );
	CBaseEntity *list[64];

//	NDebugOverlay::Box( GetAbsOrigin(), vecMins, vecMaxs, 0, 255, 0, 32, 0.1f );	

	vecMins += GetAbsOrigin();
	vecMaxs += GetAbsOrigin();

	int count = UTIL_EntitiesInBox( list, 64, vecMins, vecMaxs, 0 );
	for ( int i = 0; i < count; i++ )
	{
		if ( !list[i]->IsInStasis() &&
			 ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS || list[i]->IsNPC() || list[i]->IsPlayer() ) )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::SpawnThink( void )
{
	Assert( m_hDeathWatcher );

	// If we've already spawned a child, then we're done
	if ( m_nNumLiveChildren )
		return;

	if( m_bDisabled )
		return;

	// is anything blocking the way out?
	if ( IsDispenserBlocked() )
	{
		// play the blocked animation
		int nBlockedSequence = LookupSequence( "blocked" );
		ResetSequence( nBlockedSequence );	
		
		// and make sure we actually animate
		SetThink( &CPropAndroidDispenser::AnimateThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
		
		// come back after the delay
		m_flBlockedTime = BLOCKED_DELAY;
		
		EmitSound( "AndroidDispenser.Blocked" );
		return;
	}

	// Make an instance of a zombie
	m_hSpawnedZombie = CreateEntityByName( "npc_android_basic" );
	if ( m_hSpawnedZombie == NULL )
		return;

	CAI_BaseNPC *pNPC = (CAI_BaseNPC *) m_hSpawnedZombie.Get();
//	m_OnSpawnNPC.Set( pNPC, pNPC, this );
	m_OnSpawnNPC.FireOutput( pNPC, this );

	// Start in that positions
	pNPC->SetAbsOrigin( GetAbsOrigin() + Vector(0, 0, 10.0f) );
	pNPC->SetAbsAngles( GetAbsAngles() );

	// Give us a death notice
	pNPC->SetOwnerEntity( m_hDeathWatcher );

	// Spawn properly
	DispatchSpawn( pNPC );

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer == NULL )
		return;

	// play our spawn animation
	int nRiseSequence = LookupSequence( "open" );
	ResetSequence( nRiseSequence );
	m_bClosePending = true;

	// FIXME: Know where our enemy is to start with
	pNPC->SetEnemy( pPlayer );
	pNPC->UpdateEnemyMemory( pPlayer, pPlayer->GetAbsOrigin() );

	// Add another live child to the list
	m_nNumLiveChildren++;

	// Play our dispense sound
	EmitSound( "AndroidDispenser.Dispense", gpGlobals->curtime + 0.20f );
	
	// HACK: Rise out of the ground 
	variant_t emptyVariant;
	pNPC->AcceptInput( "RiseFromGround", this, this, emptyVariant, USE_ON );
	pNPC->AddEffects( EF_NODRAW );

	SetThink( &CPropAndroidDispenser::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::SetupNextSpawn( void )
{
	if ( m_nNumLiveChildren <= 0 )
	{
		m_nNumLiveChildren = 0;
		SetNextThink( gpGlobals->curtime + RESPAWN_DELAY );
		SetThink( &CPropAndroidDispenser::SpawnThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle when our child dies. Note: this requires that we start a think, or have another death notice pending that can restart a think
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::DeathNotice( CBaseEntity *pVictim )
{
	m_OnChildKilled.FireOutput( pVictim, this );
	
	// Decrement our count of active children
	m_nNumLiveChildren--;

	// check if we are ready to spawn again, if not we will be when our animations finish.
	if( !m_bClosePending && IsSequenceFinished() )
	{
		SetupNextSpawn();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
		SetThink( &CPropAndroidDispenser::AnimateThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAndroidDispenser::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	// Do it forever!
	SetNextThink( gpGlobals->curtime + 0.1f );
	SetThink( &CPropAndroidDispenser::AnimateThink );

	if( IsSequenceFinished() )
	{
		if( !m_bClosePending )
		{
			SetupNextSpawn();
		}
		else if( m_bClosePending && !IsDispenserBlocked() )
		{
			// play our closing animation
			int nCloseSequence = LookupSequence( "close" );
			ResetSequence( nCloseSequence );
			m_bClosePending = false;
		}
		return;
	}

	// set our guy visible at the first oportunity
	if( m_hSpawnedZombie )
	{
		CAI_BaseNPC *pNPC = (CAI_BaseNPC *) m_hSpawnedZombie.Get();
		pNPC->RemoveEffects( EF_NODRAW );
		m_hSpawnedZombie = NULL;
	}

	// if we are blocked then count down to when we should spawn again.
	if ( m_flBlockedTime > 0.0f )
	{
		m_flBlockedTime -= 0.1f;
		if ( m_flBlockedTime <= 0.0f )
		{
			SetNextThink( gpGlobals->curtime + 0.1f );
			SetThink( &CPropAndroidDispenser::SpawnThink );
		}
	}
}
