//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Scalable building!
//
//===========================================================================//

#include "cbase.h"
#include "prop_portal.h"
#include "ai_utils.h"

// Used for maintaining unique IDs to portals
extern int g_PortalGUID;

class CPropBuilding : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropBuilding, CBaseAnimating );

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	
	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

	virtual void UpdateOnRemove( void );

private:
	void	CreatePortal( void );

	string_t				m_strTargetPortal;
	CHandle<CProp_Portal>	m_hTargetPortals[2];

	DECLARE_DATADESC();
};

const char g_szModelName[] = "models/props_fstop/dollhouse04.mdl";

char g_szTargetPortalName[256];	// HACK

LINK_ENTITY_TO_CLASS( prop_building, CPropBuilding );

BEGIN_DATADESC( CPropBuilding )
	DEFINE_KEYFIELD( m_strTargetPortal, FIELD_STRING, "target_portal" ),
	DEFINE_ARRAY( m_hTargetPortals, FIELD_EHANDLE, 2 ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropBuilding::Precache( void )
{
	PrecacheModel( g_szModelName );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropBuilding::Spawn( void )
{
	Precache();

	SetModel( g_szModelName );
	
	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_NONE );
	CreateVPhysics();

	AddEffects( EF_NOSHADOW );

	BaseClass::Spawn();

	// FIXME: Only create the model at the "normal" scale for now
	if ( GetObjectScaleLevel() == 0 )
	{
		CreatePortal();
	}
}


bool CPropBuilding::CreateVPhysics( void )
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
void CPropBuilding::CreatePortal( void )
{
	if ( m_strTargetPortal == NULL_STRING )
	{
		m_strTargetPortal = AllocPooledString( g_szTargetPortalName );
	}
	else
	{
		Q_strncpy( g_szTargetPortalName, STRING(m_strTargetPortal), sizeof(g_szTargetPortalName) );
	}

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, STRING( m_strTargetPortal ) );
	if ( pTarget == NULL )
	{
		Assert( 0 );
		return;
	}

	Vector vecOrigin;
	QAngle vecAngles;
	GetAttachment( "door", vecOrigin, vecAngles );

	// The main portal is at the attachment point on the model
	m_hTargetPortals[0] = CProp_Portal::FindPortal( g_PortalGUID, false, true );
	if ( m_hTargetPortals[0] == NULL )
	{
		Assert( 0 );
		return;
	}

	// Set the portal up
	m_hTargetPortals[0]->PlacePortal( vecOrigin, vecAngles, 1.0f, false );
	
	// The secondary portal is at the target position
	m_hTargetPortals[1] = CProp_Portal::FindPortal( g_PortalGUID, true, true );
	if ( m_hTargetPortals[1] == NULL )
	{
		Assert( 0 );
		return;
	}

	g_PortalGUID++;

	// Set the portal up
	m_hTargetPortals[1]->PlacePortal( pTarget->GetAbsOrigin(), pTarget->GetAbsAngles(), 1.0f, false );
}


void CPropBuilding::UpdateOnRemove( void )
{
	if ( m_hTargetPortals[0] != NULL )
	{
		m_hTargetPortals[0]->Fizzle();
		m_hTargetPortals[0] = NULL;
	}
	
	if ( m_hTargetPortals[1] != NULL )
	{
		m_hTargetPortals[1]->Fizzle();
		m_hTargetPortals[1] = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//------------------------------------------------------------------------------
// Portal tunnel (temp)
//------------------------------------------------------------------------------



bool CPropBuilding::CPhotoPlacementQuery::GetPlacementPosition( CaptureInfo_t &captureInfo,
															   CheckPlacementData_t &placementData,
															   Vector &positionOut,
															   QAngle &anglesOut )
{
	Vector vecNormal = vecEndNormal;
	Vector vecPoint = vecEndPoint;
	float flScale = GetScaleForStep( nScaleStep, &captureInfo );

	// Let's extend our trace now
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
	{
		Vector vecEyeDir = pPlayer->EyeDirection3D();
		Vector vecEyePos = pPlayer->EyePosition();
		trace_t tr;
		Ray_t ray;
		ray.Init( vecEyePos, vecEyePos + ( vecEyeDir * ( (500*12) * flScale ) ) );
		UTIL_Portal_TraceRay( ray, CONTENTS_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			vecNormal = tr.plane.normal;
			vecPoint = tr.endpos;
		}
	}

	if ( pOriginOut )
	{
		if ( nScaleStep == 0 )
		{
			*pOriginOut = vecPoint - Vector( 0, 0, (3*12) * flScale );
		}
		else
		{
			*pOriginOut = vecPoint - Vector( 0, 0, 3.0f );
		}
	}

	if ( pAnglesOut )
	{
		/*
		matrix3x4_t matSurface;
		QAngle vecEndAngles;
		VectorAngles( vecNormal, vecEndAngles );
		AngleMatrix( vecEndAngles, vecEndPoint, matSurface );

		*pAnglesOut = TransformAnglesToWorldSpace( QAngle( 90, 0, 0 ), matSurface );
		*/
		
		*pAnglesOut = QAngle( 0, 0, 0 );
		Vector vecDir = pPlayer->EyePosition() - vecEndPoint;
		(*pAnglesOut)[1] = UTIL_VecToYaw( vecDir ) + 180;
	}

	return true;
}

CameraInfo_ScaleData_t *CPropBuilding::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 0.25f, 0.5f, 1.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, SIZEOF_ARRAY(s_DefaultScales) );
	return &simpleScales;
}


