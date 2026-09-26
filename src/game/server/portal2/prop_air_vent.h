//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Air vent
//
//===========================================================================//

#ifndef PROP_AIR_VENT_H
#define PROP_AIR_VENT_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"
#include "particle_system.h"
#include "vcollide_parse.h"
#include "physics.h"
#include "vphysics/constraints.h"
#include "phys_controller.h"

#define FAN_MODEL_OFFSET 6.0f
#define FAN_MODEL_WIDTH	 48.0f

//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CPushController : public IMotionEvent
{
	DECLARE_CLASS( CPushController, CBaseTrigger );
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

//-----------------------------------------------------------------------------
// Purpose: A trigger that pushes the player, NPCs, or objects.
//-----------------------------------------------------------------------------
class CTriggerAirVentPush : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerAirVentPush, CBaseTrigger );
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

//------------------------------------------------------------------------------
// Air vent
//------------------------------------------------------------------------------

class CPropAirVent : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropAirVent, CBaseAnimating );
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual bool CreateVPhysics( void );
	virtual void OnCaptured( void );
	virtual void OnReleased( void );
	
	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );
		virtual float GetMaxPlacementDistance( void );
		virtual float GetPlacementHelperOffset(	CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

	bool ShouldPushEntity( CBaseEntity *pEntity ) { return ( m_hAttachedEntity == NULL || pEntity != m_hAttachedEntity.Get() ); }

private:
	void DestroyPhysicsHelpers( void );
	bool CreateConstraint( CBaseEntity *pTargetEntity );
	void CreateCurrent( void );
	void CreateParticles( int nScale );
	void AnimateThink( void );
	void PhysicsAnimateThink( void );

	void AttachToEntity( EHANDLE hEntity );
	void PushAttached( void );
	float GetFanForceScalar( void );

	CHandle<CParticleSystem>	m_hParticles;

private:
	CHandle<CTriggerAirVentPush>	m_hPushTrigger;
	EHANDLE							m_hAttachedEntity;
	
	IPhysicsConstraint				*m_pConstraint;
	CPushController					m_Integrator;
	IPhysicsMotionController		*m_pController;

	void CurrentCreationThink( void );
//	void UpdatePhysicsIfParented( void );
};

#endif // PROP_AIR_VENT_H