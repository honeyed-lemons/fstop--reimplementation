//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
//  Purpose: Spouting geyser
//
//============================================================================//

#include "cbase.h"
#include "baseanimating.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "triggers.h"

// FIXME: Generalize this into something that multiple different classes can use!

//-----------------------------------------------------------------------------
// Purpose: A trigger that pushes the player, NPCs, or objects.
//-----------------------------------------------------------------------------
class CTriggerAirPush : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerAirPush, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Untouch( CBaseEntity *pOther );

	void SetPushDirection ( const Vector &vecPushDir ) { m_vecPushDir = vecPushDir; }
	void SetPushSpeed( float flSpeed ) { m_flPushSpeed = flSpeed; }

private:

	Vector m_vecPushDir;
	float m_flAlternateTicksFix; // Scale factor to apply to the push speed when running with alternate ticks
	float m_flPushSpeed;
};

BEGIN_DATADESC( CTriggerAirPush )
	DEFINE_FIELD( m_vecPushDir, FIELD_VECTOR ),
	DEFINE_FIELD( m_flAlternateTicksFix, FIELD_FLOAT ),
	DEFINE_FIELD( m_flPushSpeed, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_air_push, CTriggerAirPush );

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerAirPush::Spawn( void )
{
	// Convert pushdir from angles to a vector
	Vector vecAbsDir = m_vecPushDir;

	// Transform the vector into entity space
	VectorIRotate( vecAbsDir, EntityToWorldTransform(), m_vecPushDir );

	BaseClass::Spawn();

	AddSpawnFlags( SF_TRIGGER_ALLOW_ALL );
	InitTrigger();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CTriggerAirPush::Touch( CBaseEntity *pOther )
{
	if ( pOther->IsSolid() == false || (pOther->GetMoveType() == MOVETYPE_PUSH || pOther->GetMoveType() == MOVETYPE_NONE ) )
		return;

	if ( PassesTriggerFilters( pOther ) == false )
		return;

	// FIXME: If something is hierarchically attached, should we try to push the parent?
	if ( pOther->GetMoveParent() )
		return;

	// Transform the push dir into global space
	Vector vecAbsDir;
	VectorRotate( m_vecPushDir, EntityToWorldTransform(), vecAbsDir );

	// We must be able to "hit" our mark and not be obstructed to push them
	Vector vecTestPos = GetAbsOrigin() + ( vecAbsDir * 24.0f );
	trace_t tr;
	UTIL_TraceLine( vecTestPos, pOther->GetAbsOrigin(), CONTENTS_SOLID, pOther, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f )
	{
		if ( m_debugOverlays & OVERLAY_BBOX_BIT )
		{
			NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, true, 0.05f );
		}
		return;
	}

	if ( m_debugOverlays & OVERLAY_BBOX_BIT )
	{
		NDebugOverlay::Line( tr.startpos, tr.endpos, 0, 255, 0, true, 0.05f );
	}

	switch( pOther->GetMoveType() )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
		break;

	case MOVETYPE_VPHYSICS:
		{
			IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
			if ( pPhys )
			{
				// Always wake us up!
				pPhys->Wake();

				Vector vecVelocity;
				pPhys->GetVelocity( &vecVelocity, NULL );

				float flVelAdd = ( vecVelocity.z < 0 ) ? fabs( vecVelocity.z * 2.0f ) : 0.0f; // FIXME: This is assuming up...
				float flUpDot = DotProduct( Vector( 0, 0, 1 ), vecAbsDir );
				m_flPushSpeed = 800.0f + ( flVelAdd * flUpDot );
				pPhys->ApplyForceCenter( m_flPushSpeed * vecAbsDir * pPhys->GetMass() * gpGlobals->frametime );

				AngularImpulse angImpulse = RandomAngularImpulse( -32.0f, 32.0f );
				pPhys->ApplyTorqueCenter( angImpulse );

				return;
			}
		}
		break;

	default:
		{
			// HACK HACK  HL2 players on ladders will only be disengaged if the sf is set, otherwise no push occurs.
			if ( pOther->IsPlayer() && 
				pOther->GetMoveType() == MOVETYPE_LADDER )
			{
				if ( !HasSpawnFlags(SF_TRIG_PUSH_AFFECT_PLAYER_ON_LADDER) )
				{
					// Ignore the push
					return;
				}
			}
			
			Vector vecVelocity = pOther->GetAbsVelocity();
			float flVelAdd = ( vecVelocity.z < 0 ) ? fabs( vecVelocity.z * 2.0f ) : 0.0f; // FIXME: This is assuming up...
			float flUpDot = DotProduct( Vector( 0, 0, 1 ), vecAbsDir );
			Vector vecPush = vecAbsDir * ( 600.0f + ( flVelAdd * flUpDot ) );

			if ( pOther->GetFlags() & FL_BASEVELOCITY )
			{
				vecPush = vecPush + pOther->GetBaseVelocity();
			}

			if ( vecPush.z > 0 && (pOther->GetFlags() & FL_ONGROUND) )
			{
				pOther->SetGroundEntity( NULL );
				pOther->SetAbsOrigin( pOther->GetAbsOrigin() + Vector(0,0,4) );
				pOther->SetGroundChangeTime( gpGlobals->curtime + 0.5f );
			}
			else
			{
				vecPush += vecAbsDir * 320.0f;
			}

			pOther->SetBaseVelocity( vecPush );
			pOther->AddFlag( FL_BASEVELOCITY );
		}
		break;
	}
}

class CPropGeyser : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropGeyser, CBaseAnimating );
	DECLARE_DATADESC();

				CPropGeyser( void ) {}
				~CPropGeyser( void ){}

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void OnCaptured( void );
	virtual void OnReleased( void );
	virtual void UpdateOnRemove( void );

	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

private:

	void IdleThink( void );
	void PreEruptThink( void );
	void EruptThink( void );

	void CreateParticleSystems( void );
	void DestroyParticleSystems( void );
	
	void StartEruptionPush( void );
	void StopEruptionPush( void );

	CHandle< CParticleSystem >	m_hPreEruptionParticles;
	CHandle< CParticleSystem >	m_hEruptionParticles;
	CHandle< CTriggerAirPush >	m_hPushTrigger;

	float	m_flEruptionTime;	// Time until the next eruption
};             

LINK_ENTITY_TO_CLASS( prop_geyser, CPropGeyser );

BEGIN_DATADESC( CPropGeyser )
	DEFINE_FUNCTION( IdleThink ),
	DEFINE_FUNCTION( PreEruptThink ),
	DEFINE_FUNCTION( EruptThink ),

	DEFINE_FIELD( m_hPreEruptionParticles, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hEruptionParticles, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPushTrigger, FIELD_EHANDLE ),


	DEFINE_FIELD( m_flEruptionTime, FIELD_FLOAT ),
END_DATADESC()

const char szGeyserModel[] = "models/props_gameplay/geyser.mdl";

ConVar geyser_idle_time( "geyser_idle_time", "5.0" );
ConVar geyser_pre_eruption_time( "geyser_pre_eruption_time", "2.0" );
ConVar geyser_eruption_time( "geyser_eruption_time", "2.0" );



//------------------------------------------------------------------------------
// Placement query
//------------------------------------------------------------------------------

bool CPropGeyser::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																	  CheckPlacementData_t &placementData,
																	  Vector &positionOut,
																	  QAngle &anglesOut )
{
	if ( placementData.Trace.fraction == 1.0f ) //can't place in midair
		return false;
	
	matrix3x4_t matSurface;
	QAngle qEndAngles;
	VectorAngles( placementData.Trace.plane.normal, qEndAngles );
	AngleMatrix( qEndAngles, placementData.Trace.endpos, matSurface );

	anglesOut = TransformAnglesToWorldSpace( QAngle( 0, 0, 0 ), matSurface );
	positionOut = placementData.Trace.endpos;

	return true;
}

CameraInfo_ScaleData_t *CPropGeyser::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 1.0f, 1.5f, 2.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::Precache( void )
{
	PrecacheModel( szGeyserModel );

	PrecacheParticleSystem( "geyser_spout_pre_small" );
	PrecacheParticleSystem( "geyser_spout_pre_medium" );
	PrecacheParticleSystem( "geyser_spout_pre_large" );
	PrecacheParticleSystem( "geyser_spout_small" );
	PrecacheParticleSystem( "geyser_spout_medium" );
	PrecacheParticleSystem( "geyser_spout_large" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::Spawn( void )
{
	Precache();

	SetModel( szGeyserModel );

	/*
	int nSpinSequence = LookupSequence( "spin" );
	ResetSequence( nSpinSequence );
	*/

	// Don't cast shadows because we're going to be moving around
	AddEffects( EF_NOSHADOW );
	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_NONE );

	BaseClass::Spawn();
	CreateVPhysics();

	SetThink( &CPropGeyser::PreEruptThink );
	SetNextThink( gpGlobals->curtime + geyser_idle_time.GetFloat() );
}

bool CPropGeyser::CreateVPhysics( void )
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
void CPropGeyser::UpdateOnRemove( void )
{
	DestroyParticleSystems();
	
	if ( m_hPushTrigger )
	{
		UTIL_Remove( m_hPushTrigger );
		m_hPushTrigger = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::OnCaptured( void )
{
	DestroyParticleSystems();

	if ( m_hPushTrigger )
	{
		UTIL_Remove( m_hPushTrigger );
		m_hPushTrigger = NULL;
	}

	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::OnReleased( void )
{
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )
	{
		pPhysObj->EnableMotion( false );
	}

	// CreateCurrent();

	BaseClass::OnReleased();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::CreateParticleSystems( void )
{
	// Make sure the effect is created
	if ( m_hPreEruptionParticles == NULL )
	{
		// Create the dust effect in place
		m_hPreEruptionParticles = (CParticleSystem *) CreateEntityByName( "info_particle_system" );
		if ( m_hPreEruptionParticles == NULL )
			return;

		// Setup our basic parameters
		m_hPreEruptionParticles->KeyValue( "start_active", "0" );
		if ( m_nObjectScaleLevel == 0 )
		{
			m_hPreEruptionParticles->KeyValue( "effect_name", "geyser_spout_pre_small" );
		}
		else if ( m_nObjectScaleLevel == 1 )
		{
			m_hPreEruptionParticles->KeyValue( "effect_name", "geyser_spout_pre_medium" );
		}
		else if ( m_nObjectScaleLevel == 2 )
		{
			m_hPreEruptionParticles->KeyValue( "effect_name", "geyser_spout_pre_large" );
		}

		m_hPreEruptionParticles->SetParent( this );
		m_hPreEruptionParticles->SetLocalOrigin( vec3_origin );
		m_hPreEruptionParticles->SetLocalAngles( vec3_angle );
		DispatchSpawn( m_hPreEruptionParticles );
		m_hPreEruptionParticles->Activate();
	}

	// Make sure the effect is created
	if ( m_hEruptionParticles == NULL )
	{
		// Create the dust effect in place
		m_hEruptionParticles = (CParticleSystem *) CreateEntityByName( "info_particle_system" );
		if ( m_hEruptionParticles == NULL )
			return;

		// Setup our basic parameters
		m_hEruptionParticles->KeyValue( "start_active", "0" );
		if ( m_nObjectScaleLevel == 0 )
		{
			m_hEruptionParticles->KeyValue( "effect_name", "geyser_spout_small" );
		}
		else if ( m_nObjectScaleLevel == 1 )
		{
			m_hEruptionParticles->KeyValue( "effect_name", "geyser_spout_medium" );
		}
		else if ( m_nObjectScaleLevel == 2 )
		{
			m_hEruptionParticles->KeyValue( "effect_name", "geyser_spout_large" );
		}
		m_hEruptionParticles->SetParent( this );
		m_hEruptionParticles->SetLocalOrigin( vec3_origin );
		m_hEruptionParticles->SetLocalAngles( vec3_angle );
		DispatchSpawn( m_hEruptionParticles );
		m_hEruptionParticles->Activate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::DestroyParticleSystems( void )
{
	if ( m_hPreEruptionParticles )
	{
		m_hPreEruptionParticles->StopParticleSystem();
		UTIL_Remove( m_hPreEruptionParticles );
		m_hPreEruptionParticles = NULL;
	}

	if ( m_hEruptionParticles )
	{
		m_hEruptionParticles->StopParticleSystem();
		UTIL_Remove( m_hEruptionParticles );
		m_hEruptionParticles = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::PreEruptThink( void )
{
	// Make sure these are working
	CreateParticleSystems();

	if ( m_hPreEruptionParticles )
	{
		m_hPreEruptionParticles->StartParticleSystem();
	}

	if ( m_hEruptionParticles )
	{
		m_hEruptionParticles->StopParticleSystem();
	}

	// Shake nearby players
	// UTIL_ScreenShake( GetAbsOrigin(), 8.0f, 1.0f, geyser_pre_eruption_time.GetFloat(), (60.0f*12.0f), SHAKE_START );

	// Setup for the next phase
	SetThink( &CPropGeyser::EruptThink );
	SetNextThink( gpGlobals->curtime + geyser_pre_eruption_time.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::EruptThink( void )
{
	// Make sure these are working
	CreateParticleSystems();

	if ( m_hPreEruptionParticles )
	{
		m_hPreEruptionParticles->StopParticleSystem();
	}

	if ( m_hEruptionParticles )
	{
		m_hEruptionParticles->StartParticleSystem();
	}

	// Shake nearby players
	// UTIL_ScreenShake( GetAbsOrigin(), 32.0f, 1.0f, geyser_eruption_time.GetFloat(), (60.0f*12.0f), SHAKE_START );

	StartEruptionPush();

	SetThink( &CPropGeyser::IdleThink );
	SetNextThink( gpGlobals->curtime + geyser_eruption_time.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::IdleThink( void )
{
	StopEruptionPush();

	if ( m_hPreEruptionParticles )
	{
		m_hPreEruptionParticles->StopParticleSystem();
	}

	if ( m_hEruptionParticles )
	{
		m_hEruptionParticles->StopParticleSystem();
	}

	SetThink( &CPropGeyser::PreEruptThink );
	SetNextThink( gpGlobals->curtime + geyser_idle_time.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::StartEruptionPush( void )
{
	if ( m_hPushTrigger == NULL )
	{
		m_hPushTrigger = (CTriggerAirPush *) CreateEntityByName( "trigger_air_push" );
		Assert( m_hPushTrigger != NULL );
		if ( m_hPushTrigger == NULL )
			return;

		Vector vecForward, vecRight, vecUp;
		AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

		float flScale = GetModelScale();

		Vector vecTunnel;
		vecTunnel = vec3_origin;
		vecTunnel += ( vecRight * (1*12)*flScale );
		vecTunnel += ( vecUp * (2*12)*flScale );

		Vector vecMins = -vecTunnel;
		Vector vecMaxs = vecTunnel;

		float flUnitRemap = 1;
		if ( flScale == 1.5f )
			flUnitRemap = 2;
		else if ( flScale == 2.25f )
			flUnitRemap = 4;

		// Find our exact trigger volume we're trying to map to
		vecMaxs += vecForward * ( ( 128.0f * flUnitRemap ) - ( 24.0f * flScale ) );

		for ( int i = 0 ; i < 3 ; i++ )
		{
			if ( vecMins[i] > vecMaxs[i] )
			{
				V_swap( vecMins[i], vecMaxs[i] );
			}
		}

		CBaseEntity *pPushEntity = (CBaseEntity *) m_hPushTrigger.Get();

		// Set our push trigger to these bounds
		UTIL_SetOrigin( pPushEntity, GetAbsOrigin() );

		// Setup our directions
		m_hPushTrigger->SetPushDirection( vecForward );
		m_hPushTrigger->SetPushSpeed( flUnitRemap );

		DispatchSpawn( pPushEntity );

		// Hit our activation!
		if ( gpGlobals->curtime > 2.0f )
		{
			pPushEntity->Activate();
		}

		// Reset the size after the spawn has occurred
		UTIL_SetSize( pPushEntity, vecMins, vecMaxs );
		pPushEntity->SetSolid( SOLID_BBOX );
		pPushEntity->AddSolidFlags( FSOLID_TRIGGER );
		pPushEntity->AddSpawnFlags( SF_TRIGGER_ALLOW_ALL );

		// Create a particle system for it
		// CreateParticles( (int) flUnitRemap );
	}

	m_hPushTrigger->Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropGeyser::StopEruptionPush( void )
{
	if ( m_hPushTrigger )
	{
		m_hPushTrigger->Disable();
	}
}