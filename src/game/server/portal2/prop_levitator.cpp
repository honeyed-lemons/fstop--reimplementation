//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose:
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "triggers.h"
#include "props.h"
#include "photo.h"
#include "phys_controller.h"
#include "physics.h"
#include "vphysics/constraints.h"
#include "rope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_gravity;

//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CLevitationController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	void Init( IMotionEvent::simresult_e controlType ) 
	{ 
		m_controlType = controlType;
	}

	void SetConstantForce( const Vector &linear );
	void ScaleConstantForce( float scale );

	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	IMotionEvent::simresult_e	m_controlType;
	Vector			m_linear;
	Vector			m_linearSave;
};

BEGIN_SIMPLE_DATADESC( CLevitationController )
DEFINE_FIELD( m_controlType,	FIELD_INTEGER ),
DEFINE_FIELD( m_linear,		FIELD_VECTOR ),
DEFINE_FIELD( m_linearSave,	FIELD_VECTOR ),
END_DATADESC()


void CLevitationController::SetConstantForce( const Vector &linear )
{
	m_linear = linear;
	// cache these for scaling later
	m_linearSave = linear;
}

void CLevitationController::ScaleConstantForce( float scale )
{
	m_linear = m_linearSave * scale;
}


IMotionEvent::simresult_e CLevitationController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = m_linear;

	return m_controlType;
}

//
//	Levitator
//

class CPropLevitator : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropLevitator, CBaseAnimating );
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void OnRestore( void );

	void CreateTriggers();

	virtual void UpdateOnRemove( void );

	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );
		virtual float GetMaxPlacementDistance( void );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

	virtual void	OnCaptured( void );
	virtual void	OnReleased( void );

protected:
	bool	CreateConstraint( CBaseEntity *pTargetEntity, const Vector &vecTargetOffset );
	void	DestroyPhysicsHelpers( void );
	void	FloatThink( void );

	EHANDLE						m_hAttachedEntity;
	EHANDLE						m_hUprightController;
	CLevitationController		m_integrator;
	IPhysicsMotionController	*m_pController;
	IPhysicsConstraint			*m_pConstraint;
	CHandle< CRopeKeyframe >	m_hRope;
	CHandle< CBaseEntity >		m_hRopeHelper;
};

BEGIN_DATADESC( CPropLevitator )
DEFINE_THINKFUNC( FloatThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_levitator, CPropLevitator );

const char LEVITATOR_MODEL_NAME[] = "models/props_gameplay/balloon.mdl";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLevitator::Precache( void )
{
	PrecacheModel( LEVITATOR_MODEL_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLevitator::DestroyPhysicsHelpers( void )
{
	if ( m_hUprightController )
	{
		UTIL_Remove( m_hUprightController );
		m_hUprightController = NULL;
	}

	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = NULL;
	}

	if ( m_pConstraint )
	{
		physenv->DestroyConstraint( m_pConstraint );
		m_pConstraint = NULL;
	}

	if ( m_hRope )
	{
		UTIL_Remove( m_hRope );
	}

	if ( m_hRopeHelper )
	{
		UTIL_Remove( m_hRopeHelper );
	}

	m_hAttachedEntity = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Remove our helpers as well
//-----------------------------------------------------------------------------
void CPropLevitator::UpdateOnRemove( void )
{
	DestroyPhysicsHelpers();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLevitator::Spawn( void )
{
	Precache();
	SetModel( LEVITATOR_MODEL_NAME );
	m_nSkin = 1;

	BaseClass::Spawn();

	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropLevitator::CreateVPhysics( void )
{
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj == NULL )
	{
		pPhysObj = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	}

	if ( pPhysObj )
	{
		pPhysObj->EnableGravity( false );
		
		// FIXME: Roll this into the levitation integrator!
		m_hUprightController = CreateKeepUpright( GetAbsOrigin(), GetAbsAngles(), this, 60.0f, true );

		// Setup the integrator
		m_integrator.Init( IMotionEvent::SIM_GLOBAL_ACCELERATION );

		Vector linear( 0.0f, 0.0f, 200.0f*GetModelScale() );

		m_integrator.SetConstantForce( linear );
		m_pController = physenv->CreateMotionController( &m_integrator );
		m_pController->AttachObject( pPhysObj, true );
		
		// Make sure the object is simulated
		pPhysObj->Wake();

		return true;
	}

	return false;
}

extern void DrawConstraintObjectsAxes( CBaseEntity *pConstraintEntity, IPhysicsConstraint *pConstraint );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropLevitator::FloatThink( void )
{
	if ( m_hAttachedEntity )
	{
		// Determine if the entity has gone into stasis while attached to us
		if ( m_hAttachedEntity->IsInStasis() )
		{
			DestroyPhysicsHelpers();
			m_hAttachedEntity = NULL;
			SetThink( NULL );
			return;
		}

		// Now, try to float a certain distance off of the ground
		const float flTraceDist = (3*12) + ( m_hAttachedEntity->CollisionProp()->BoundingRadius2D() / 2.0f );
		
		trace_t tr;
		UTIL_TraceLine( m_hAttachedEntity->GetAbsOrigin(), m_hAttachedEntity->GetAbsOrigin() - Vector(0,0,flTraceDist), MASK_SOLID, m_hAttachedEntity, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1.0f )
		{
#if 0
			NDebugOverlay::Box( tr.startpos, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
			NDebugOverlay::Box( tr.endpos, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
			NDebugOverlay::Line( tr.startpos, tr.endpos, 0, 255, 0, false, 0.05f );

			NDebugOverlay::Box( m_hAttachedEntity->GetAbsOrigin() - Vector(0,0,flTraceDist), -Vector(1,1,1), Vector(1,1,1), 255, 255, 0, 0, 0.05f );
			NDebugOverlay::Line( tr.startpos, m_hAttachedEntity->GetAbsOrigin() - Vector(0,0,flTraceDist), 255, 255, 0, true, 0.05f );
#endif			
			float flForceBias = Bias( ( 1.0f - tr.fraction ), 0.85f );
			float flForceScale = 1.0f + (flForceBias / 4);
			m_integrator.ScaleConstantForce( flForceScale );
			
#if 0
			char szText[MAX_PATH];
			Q_snprintf( szText, sizeof(szText), "Force: %.02f", flForceScale );
			NDebugOverlay::Text( tr.startpos, szText, false, 0.05f );
#endif
			}
		else
		{
			m_integrator.ScaleConstantForce( 1.0f );
		}
	}
	else
	{
		DestroyPhysicsHelpers();
		SetThink( NULL );
		return;
	}

	/*
	// Draw our attachment info
	matrix3x4_t matReference, matAttached;
	m_pConstraint->GetConstraintTransform( &matReference, &matAttached );

	IPhysicsObject *pRef = m_pConstraint->GetReferenceObject();
	IPhysicsObject *pAtt = m_pConstraint->GetAttachedObject();

	Vector vecReferencePoint, vecAttachPoint;
	MatrixGetColumn( matReference, 3, vecReferencePoint );
	MatrixGetColumn( matAttached, 3, vecAttachPoint );

	Vector vecAttWorld, vecRefWorld;
	pRef->LocalToWorld( &vecRefWorld, vecReferencePoint );
	pAtt->LocalToWorld( &vecAttWorld, vecAttachPoint );

	NDebugOverlay::Box( vecRefWorld, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
	NDebugOverlay::Box( vecAttWorld, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
	NDebugOverlay::Line( vecAttWorld, vecRefWorld, 0, 255, 0, false, 0.05f );
	*/
	
	// Think again
	SetThink( &CPropLevitator::FloatThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropLevitator::CreateConstraint( CBaseEntity *pTargetEntity, const Vector &vecTargetOffset )
{
	if ( pTargetEntity == NULL )
		return false;

	CBaseAnimating *pAnim = GetBaseAnimating();
	if ( pAnim == NULL )
		return false;

	Vector vecOffset;
	QAngle vecAngles;
	pAnim->GetAttachment( 1, vecOffset, vecAngles );

	constraint_ballsocketparams_t ballsocket;
	ballsocket.Defaults();

	IPhysicsObject *pTargetPhys = pTargetEntity->VPhysicsGetObject();
	IPhysicsObject *pPhys = VPhysicsGetObject();

	if ( pTargetPhys == NULL )
	{
		//return false; // FIXME: For now just let us go!
		pTargetPhys = g_PhysWorldObject;
	}

	// FIXME: What do these refer to exactly?
	pTargetPhys->WorldToLocal( &ballsocket.constraintPosition[0], GetAbsOrigin() );
	pPhys->WorldToLocal( &ballsocket.constraintPosition[1], GetAbsOrigin() );

	// Create out actual constraint now
	m_pConstraint = physenv->CreateBallsocketConstraint( pTargetPhys, pPhys, NULL, ballsocket );

	m_hRopeHelper = CreateEntityByName( "info_target" );
	m_hRopeHelper->SetAbsOrigin( vecTargetOffset );
	m_hRopeHelper->SetParent( pTargetEntity );
	DispatchSpawn( m_hRopeHelper );

	// Create the visual rope between the two
	m_hRope = CRopeKeyframe::Create( m_hRopeHelper, this, 0, 1, 1, "cable/rope.vmt", 8 );

	SetThink( &CPropLevitator::FloatThink );
	SetNextThink( gpGlobals->curtime + 0.05f );

	return true;
}


//------------------------------------------------------------------------------
// Placement query
//------------------------------------------------------------------------------

bool CPropLevitator::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																		 CheckPlacementData_t &placementData,
																		 Vector &positionOut,
																		 QAngle &anglesOut )
{
	anglesOut = vec3_angle;

	positionOut = placementData.Trace.endpos + Vector( 0, 0, 32 );
	NDebugOverlay::Box( placementData.Trace.endpos, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
	NDebugOverlay::Box( positionOut, -Vector(1,1,1), Vector(1,1,1), 0, 255, 0, 0, 0.05f );
	NDebugOverlay::Line( positionOut, placementData.Trace.endpos, 0, 255, 0, false, 0.05f );

	return true;
}

CameraInfo_ScaleData_t *CPropLevitator::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 0.5f, 1.0f, 2.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

float CPropLevitator::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	return 20.0f * 12.0f;
}


void CPropLevitator::OnCaptured( void )
{
	DestroyPhysicsHelpers();
	BaseClass::OnCaptured();
}

void CPropLevitator::OnReleased( void )
{
	m_hAttachedEntity = g_placedEntity;

	if ( m_hAttachedEntity )
	{
		// FIXME: This is a crappy way to do this!
		m_hAttachedEntity->SetOwnerEntity( this );
	}

	// FIXME: Clean up access to this information!
	CreateVPhysics();
	CreateConstraint( m_hAttachedEntity, g_placedPosition );
	
	BaseClass::OnReleased();
}

void CPropLevitator::OnRestore( void )
{
	BaseClass::OnRestore();
}
