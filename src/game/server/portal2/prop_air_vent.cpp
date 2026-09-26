//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Air vent
//
//===========================================================================//

#include "cbase.h"

#include "prop_air_vent.h"

#include "triggers.h"
#include "particle_system.h"
#include "vcollide_parse.h"
#include "physics.h"
#include "vphysics/constraints.h"
#include "phys_controller.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const char* g_pszSetupAirCurrentContext = "SetupAirCurrentContext";
//const char* g_pszHACKUpdatePhysicsIfParentedContext = "HACKUpdatePhysicsIfParentedContext";

BEGIN_SIMPLE_DATADESC( CPushController )
	DEFINE_FIELD( m_controlType,	FIELD_INTEGER ),
	DEFINE_FIELD( m_linear,		FIELD_VECTOR ),
	DEFINE_FIELD( m_linearSave,	FIELD_VECTOR ),
END_DATADESC()


void CPushController::SetConstantForce( const Vector &linear )
{
	m_linear = linear;
	// cache these for scaling later
	m_linearSave = linear;
}

void CPushController::ScaleConstantForce( float scale )
{
	m_linear = m_linearSave * scale;
}


IMotionEvent::simresult_e CPushController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = m_linear;

	return m_controlType;
}

BEGIN_DATADESC( CTriggerAirVentPush )
	DEFINE_FIELD( m_vecPushDir, FIELD_VECTOR ),
	DEFINE_FIELD( m_flAlternateTicksFix, FIELD_FLOAT ),
	DEFINE_FIELD( m_flPushSpeed, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_airvent_push, CTriggerAirVentPush );


bool CPropAirVent::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																	   CheckPlacementData_t &placementData,
																	   Vector &positionOut,
																	   QAngle &anglesOut )
{
	// first see if we actually are hitting a Physics Object.
	if ( placementData.Trace.m_pEnt && placementData.Trace.m_pEnt->VPhysicsGetObject() )
	{
		// see which angle best suits us. Let's just assume it's up. eventually do a dot product.
		Vector vecForward, vecRight, vecUp; 
		placementData.Trace.m_pEnt->GetVectors( &vecForward, &vecRight, &vecUp );

		Vector testVectors[6] = { vecForward, -vecForward, vecRight, -vecRight, vecUp, -vecUp };

		int bestIndex = -1;
		float bestMatch = 0.0f;

		// look at where we are placing on this object to calc a direction from the center of the object
		Vector placeDir = placementData.Trace.endpos - placementData.Trace.m_pEnt->GetAbsOrigin();
		placeDir.NormalizeInPlace();

		// use the center and the oriention of the object to find the best face.
		for ( int i = 0; i < 6; i++ )
		{
			// we don't want to be putting this in the ground, but there's an acceptable epsilon.
			if ( testVectors[i].z <= -0.05f )
				continue;

			float dot = testVectors[i].Dot( placeDir );
			if ( dot > bestMatch )
			{
				bestIndex = i;
				bestMatch = dot;
			}
		}

		// we got one! now try to determine where we attach ourselves.
		if ( bestIndex >= 0 )
		{
			Vector vecMaxs = placementData.Trace.m_pEnt->CollisionProp()->OBBMaxs();

			// FIXME we need to use the part to vecMaxs that matches the face.
			positionOut = placementData.Trace.m_pEnt->GetAbsOrigin() + 1.33f * vecMaxs.z * testVectors[bestIndex];

			// get a second vector that's not the best one to help with the orientation.
			Vector pseudoUp = vecForward;
			if ( testVectors[bestIndex] == vecForward )
			{
				pseudoUp = vecUp;
			}

			QAngle vecEndAngles;
			VectorAngles( testVectors[bestIndex], pseudoUp, vecEndAngles );

			anglesOut = vecEndAngles;

			return true;
		}
		return false;
	}

	return WallPlacement( FAN_MODEL_WIDTH * placementData.fScale, 
		FAN_MODEL_WIDTH * placementData.fScale, 
		FAN_MODEL_OFFSET * placementData.fScale, 
		captureInfo, placementData, positionOut, anglesOut );
}

float CPropAirVent::CPhotoPlacementQuery::GetPlacementHelperOffset(	CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData )
{
	return FAN_MODEL_OFFSET * placementData.fScale;
}

CameraInfo_ScaleData_t *CPropAirVent::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 1.0f, 1.5f, 2.25f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

float CPropAirVent::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	return ( 60.0f * 12.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerAirVentPush::Spawn( void )
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
void CTriggerAirVentPush::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || (pOther->GetMoveType() == MOVETYPE_PUSH || pOther->GetMoveType() == MOVETYPE_NONE ) )
		return;

	if (!PassesTriggerFilters(pOther))
		return;

	// FIXME: If something is hierarchically attached, should we try to push the parent?
	if (pOther->GetMoveParent())
		return;

	if ( ( (CPropAirVent*) GetParent() )->ShouldPushEntity( pOther ) == false )
		return;

	// Transform the push dir into global space
	Vector vecAbsDir;
	VectorRotate( m_vecPushDir, EntityToWorldTransform(), vecAbsDir );

	// We must be able to "hit" our mark and not be obstructed to push them
	Vector vecTestPos = GetAbsOrigin() + ( vecAbsDir * FAN_MODEL_OFFSET );
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
#if defined( HL2_DLL )
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
#endif

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
				Vector origin = pOther->GetAbsOrigin();
				origin.z += 4.0f;
				pOther->SetAbsOrigin( origin );
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

void UTIL_FailurePlacement( const Vector &vecEndPoint, Vector *pOriginOut, QAngle *pAnglesOut )
{
	// Out origin is simply the end point
	if ( pOriginOut )
	{
		*pOriginOut = vecEndPoint;
	}

	// Angles face up but 
	if ( pAnglesOut )
	{
		matrix3x4_t matSurface;
		QAngle vecEndAngles( 0, 0, 0 );
		AngleMatrix( vecEndAngles, vecEndPoint, matSurface );

		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		Vector vecDir = pPlayer->EyePosition() - vecEndPoint;

		*pAnglesOut = TransformAnglesToWorldSpace( QAngle( 0, 0, 0 ), matSurface );
		(*pAnglesOut)[1] = UTIL_VecToYaw( vecDir ); // FIXME: Not wanted in all cases
	}
}

extern void UTIL_AlignBBox( Vector &vecMins, Vector &vecMaxs );

const char gFanModel[] = "models/props_gameplay/fan.mdl";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::Precache( void )
{
	PrecacheModel( gFanModel );
	PrecacheParticleSystem( "airvent_small" );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::DestroyPhysicsHelpers( void )
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = NULL;
	}

	SetOwnerEntity( NULL );

	if ( m_pConstraint )
	{
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;
	}

	m_hAttachedEntity = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::Spawn( void )
{
	Precache();

	SetModel( gFanModel );
	int nSpinSequence = LookupSequence( "spin" );
	ResetSequence( nSpinSequence );

	BaseClass::Spawn();
	
	CreateVPhysics();
	//IPhysicsObject *pPhysObj = VPhysicsGetObject();
	//if ( pPhysObj )
	//{
	//	pPhysObj->EnableMotion(false);
	//}

	SetContextThink( &CPropAirVent::CurrentCreationThink, gpGlobals->curtime, g_pszSetupAirCurrentContext );
	SetThink( &CPropAirVent::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.02f );

	// if I have a parent, then override that and instead switch to a normal constraint.
	CBaseEntity *pEnt = GetParent();
	AttachToEntity( pEnt );
	SetParent(NULL);
}

bool CPropAirVent::CreateVPhysics( void )
{
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj == NULL )
	{
		pPhysObj = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	}

	if ( pPhysObj )
	{
		// Setup the integrator
		m_Integrator.Init( IMotionEvent::SIM_GLOBAL_ACCELERATION );

		Vector linear( 0.0f, 0.0f, 1.0f );

		m_Integrator.SetConstantForce( linear );
		m_pController = physenv->CreateMotionController( &m_Integrator );
		m_pController->AttachObject( pPhysObj, true );

		// Make sure the object is simulated
		pPhysObj->Wake();
		return true;
	}

	return false;
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::Activate( void )
{
	// base class activate sets up our size (if non-default) so call this before
	// creating our push volume based on our size.
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::CreateCurrent( void )
{
	if ( m_hPushTrigger == NULL )
	{
		m_hPushTrigger = (CTriggerAirVentPush *) CreateEntityByName( "trigger_airvent_push" );
		Assert( m_hPushTrigger != NULL );
		if ( m_hPushTrigger == NULL )
			return;
	}

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

	float flScale = GetModelScale();
	
	Vector vecTunnel;
	vecTunnel = vec3_origin;
	vecTunnel += ( vecRight* (4*12)*flScale );
	vecTunnel += ( vecUp * (4*12)*flScale );

	Vector vecMins = -vecTunnel;
	Vector vecMaxs = vecTunnel;

	float flUnitRemap = 1;
	if ( GetObjectScaleLevel() == 1 )
		flUnitRemap = 2;
	else if ( GetObjectScaleLevel() == 2 )
		flUnitRemap = 4;

	// Find our exact trigger volume we're trying to map to
	vecMaxs += vecForward * ( ( 128.0f * flUnitRemap ) - ( FAN_MODEL_OFFSET * flScale ) );

	for ( int i = 0 ; i < 3 ; i++ )
	{
		if ( vecMins[i] > vecMaxs[i] )
		{
			SwapDWord( vecMins[i], vecMaxs[i] );
		}
	}

	CBaseEntity *pPushEntity = (CBaseEntity *) m_hPushTrigger.Get();

	// Set our push trigger to these bounds
	UTIL_SetOrigin( pPushEntity, GetAbsOrigin() );

	// Setup our directions
	m_hPushTrigger->SetPushDirection( vecForward );
	m_hPushTrigger->SetPushSpeed( flUnitRemap );

	DispatchSpawn( pPushEntity );

	pPushEntity->SetParent( this );

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
	CreateParticles( (int) flUnitRemap );
}

//-----------------------------------------------------------------------------
// Purpose: Create the particle to accompany our air current
//-----------------------------------------------------------------------------
void CPropAirVent::CreateParticles( int nScale )
{
	// Create the dust effect in place
	m_hParticles = (CParticleSystem *) CreateEntityByName( "info_particle_system" );
	if ( m_hParticles == NULL )
		return;

	m_hParticles->SetParent( this );

	const char *szNames[] = { "airvent_small", "airvent_medium", "airvent_large" };
	int nSystemSize;
	switch( nScale )
	{
	default:
	case 1:
		nSystemSize = 0;
		break;

	case 2:
		nSystemSize = 1;
		break;

	case 4:
		nSystemSize = 2;
		break;
	}

	// Setup our basic parameters
	m_hParticles->KeyValue( "start_active", "1" );
	m_hParticles->KeyValue( "effect_name", szNames[nSystemSize] );
	m_hParticles->SetAbsOrigin( GetAbsOrigin() );
	m_hParticles->SetAbsAngles( GetAbsAngles() );
	DispatchSpawn( m_hParticles );
	if ( gpGlobals->curtime > 5.0f )
		m_hParticles->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::OnCaptured( void )
{
	if ( m_hPushTrigger )
	{
		UTIL_Remove( m_hParticles );
		m_hParticles = NULL;

		UTIL_Remove( (CBaseEntity *) m_hPushTrigger.Get() );
		m_hPushTrigger = NULL;
	}

	DestroyPhysicsHelpers();

	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::AttachToEntity( EHANDLE hEntity )
{
	if( hEntity && hEntity->VPhysicsGetObject() )
	{
		m_hAttachedEntity = hEntity;

		if ( m_hAttachedEntity )
		{
			SetThink( &CPropAirVent::PhysicsAnimateThink );
			SetNextThink( gpGlobals->curtime + 0.02f );

			SetMoveType( MOVETYPE_VPHYSICS );
			CreateConstraint( m_hAttachedEntity );
			SetOwnerEntity( m_hAttachedEntity );
		}
	}
	else
	{
		SetThink( &CPropAirVent::AnimateThink );
		SetNextThink( gpGlobals->curtime + 0.02f );

		SetMoveType( MOVETYPE_NONE );
		m_hAttachedEntity = NULL;
		SetOwnerEntity( NULL );
	}

	// make sure we move if we are attached to something.
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )
	{
		pPhysObj->EnableMotion( m_hAttachedEntity == NULL ? false : true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::OnReleased( void )
{	
	CreateVPhysics();
		
	AttachToEntity( g_placedEntity );

	CreateCurrent();

	BaseClass::OnReleased();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropAirVent::CreateConstraint( CBaseEntity *pTargetEntity )
{
	if ( pTargetEntity == NULL )
		return false;

	CBaseAnimating *pAnim = GetBaseAnimating();
	if ( pAnim == NULL )
		return false;

	Vector vecOffset;
	QAngle vecAngles;
	pAnim->GetAttachment( 1, vecOffset, vecAngles );

	IPhysicsObject *pTargetPhys = pTargetEntity->VPhysicsGetObject();
	IPhysicsObject *pPhys = VPhysicsGetObject();

	if ( pTargetPhys == NULL )
	{
		return false; // FIXME: For now just let us go!
		pTargetPhys = g_PhysWorldObject;
	}

	constraint_fixedparams_t fixedConstraint;
	fixedConstraint.Defaults();
	fixedConstraint.InitWithCurrentObjectState( pTargetPhys, pPhys );
	fixedConstraint.constraint.Defaults();
		
	// Create out actual constraint now
	m_pConstraint = physenv->CreateFixedConstraint( pTargetPhys, pPhys, NULL, fixedConstraint );

	pTargetPhys->Wake();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::PhysicsAnimateThink( void )
{
	// do our animation as usual.
	AnimateThink();

	// make sure that what we are attached to still exists
	if( m_hAttachedEntity == NULL )
	{
		DestroyPhysicsHelpers();
		
		IPhysicsObject *pPhys = VPhysicsGetObject();
		if( pPhys )
		{
			pPhys->Wake();
		}
	}
	else
	{
		// repeat
		SetThink( &CPropAirVent::PhysicsAnimateThink );
		SetNextThink( gpGlobals->curtime + 0.02f );

		// try to push what we are on.
		PushAttached();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	// Do it forever!
	SetThink( &CPropAirVent::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.02f );
	
	// Update our debug state
	if ( m_hPushTrigger )
	{
		if ( m_debugOverlays & OVERLAY_BBOX_BIT )
		{
			m_hPushTrigger->m_debugOverlays |= OVERLAY_BBOX_BIT;
		}
		else
		{
			m_hPushTrigger->m_debugOverlays &= ~OVERLAY_BBOX_BIT;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAirVent::PushAttached( void )
{
	//handle the case when what we are attached to get snapped up.
	if ( m_hAttachedEntity != NULL && m_hAttachedEntity->IsInStasis() )
	{
		DestroyPhysicsHelpers();
		m_hAttachedEntity = NULL;
		SetThink( &CPropAirVent::AnimateThink );
		SetNextThink( gpGlobals->curtime + 0.02f );
	
		IPhysicsObject *pPhys = VPhysicsGetObject();
		if( pPhys )
		{
			pPhys->Wake();
		}
	
			return;
		}

	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );

	// now try to push us around.
	m_Integrator.SetConstantForce( -vecForward );
	
	
	IPhysicsObject *pAttachedPhys = m_hAttachedEntity->VPhysicsGetObject();
	float flForceScale = 20.0f * ( pAttachedPhys->GetMass() + 55.f ) * GetFanForceScalar();
	pAttachedPhys->Wake();

	m_Integrator.ScaleConstantForce( flForceScale );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPropAirVent::GetFanForceScalar( void )
{
	float scalar = 1.0f;

	// Normaly I'd just use the model scale, but that's making all the ranges a little too extreme.
	// These are just custom values to try and get this working well. 
	if( GetObjectScaleLevel() == 1 )
		scalar = 1.5f;
	else if( GetObjectScaleLevel() == 2 )
		scalar = 2.25f;

	return scalar;
}

// Just call 'CreateCurrent' and turn off. Moves the call out of Activate.
void CPropAirVent::CurrentCreationThink( void )
{
	CreateCurrent();
	SetContextThink( NULL, TICK_NEVER_THINK, g_pszSetupAirCurrentContext );
}

LINK_ENTITY_TO_CLASS( prop_air_vent, CPropAirVent );

BEGIN_DATADESC( CPropAirVent )
	DEFINE_THINKFUNC( PhysicsAnimateThink ),
	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( CurrentCreationThink ),
	DEFINE_FIELD( m_hParticles, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPushTrigger, FIELD_EHANDLE ),
END_DATADESC()
