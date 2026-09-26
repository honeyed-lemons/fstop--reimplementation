//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"
#include "photo.h"
#include "prop_portal.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC_NO_BASE( CaptureInfo_t )

	DEFINE_FIELD( hCapturedEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( nOldScaleLevel, FIELD_INTEGER ),
	DEFINE_FIELD( nPreviewScaleLevel, FIELD_INTEGER ),

	DEFINE_FIELD( bHasCustomScaleData, FIELD_BOOLEAN ),		
	DEFINE_EMBEDDED( customScaleData ),

	// fixed up in CWeaponPlacement::OnRestore
	//DEFINE_FIELD( pPlacementQuery, FIELD_BUGCAUSER ),

	DEFINE_FIELD( vecOldOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( vecOldAngles, FIELD_VECTOR ),
	DEFINE_FIELD( vecVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( vecAngVelocity, FIELD_VECTOR ),

END_DATADESC()

BEGIN_SIMPLE_DATADESC( CameraInfo_ScaleData_t )
	
	DEFINE_FIELD( nNumScaleUpLevels, FIELD_INTEGER ),
	DEFINE_FIELD( nNumScaleDownLevels, FIELD_INTEGER ),
	DEFINE_ARRAY( flScaleUpLevelMultipliers, FIELD_FLOAT, CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS ),
	DEFINE_ARRAY( flScaleDownLevelMultipliers, FIELD_FLOAT, CAPTURE_INFO_MAX_CUSTOM_SCALE_DOWN_MULTIPLIERS ),

END_DATADESC()

CameraInfo_ScaleData_t::CameraInfo_ScaleData_t( float *pOrderedScales, int iOrderedScaleCount )
{
	for( int i = 0; i != iOrderedScaleCount; ++i )
	{
		if( pOrderedScales[i] == 1.0f )
		{
			//found the center
			nNumScaleDownLevels = i;
			nNumScaleUpLevels = (iOrderedScaleCount - i) - 1;

			for( int j = i - 1; j >= 0; --j )
			{
				flScaleDownLevelMultipliers[i - (j + 1)] = pOrderedScales[j];					
			}

			for( int j = i + 1; j < iOrderedScaleCount; ++j )
			{
				flScaleUpLevelMultipliers[j - (i + 1)] = pOrderedScales[j];					
			}
			return;
		}
	}
}

// Handles casting/saftey checking for getting a scale value from a base entity
float UTIL_GetEntityScaleFactor( CBaseEntity* pEnt )
{
	Assert ( pEnt );	
	CBaseAnimating* pEntAnimating = pEnt->GetBaseAnimating();

	if ( pEntAnimating )
	{
		CaptureInfo_t cap;
		UTIL_InitCaptureInfo( cap, pEnt, NULL );
		pEntAnimating->Get_CPhotoPlacementQuery()->GetScaleForStep( pEntAnimating->GetObjectScaleLevel(), &cap );
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current scale step of an object and optionally the number of up and down steps it has available
// Input  : pEnt - the ent to check the current scale step of
//			pOutNumScaleDownSteps - out param, will contain number of scale down steps
//			pOutNumScaleUpSteps - out param, will contain number of scale up steps
// Output : int - current scale level
//-----------------------------------------------------------------------------
int UTIL_GetEntityScaleLevel( CBaseEntity* pEnt, int* pOutNumScaleDownSteps /*= NULL*/, int* pOutNumScaleUpSteps /*= NULL*/ )
{
	Assert ( pEnt );

	if ( pOutNumScaleDownSteps )
		*pOutNumScaleDownSteps = 0;

	if ( pOutNumScaleUpSteps )
		*pOutNumScaleUpSteps = 0;

	CBaseAnimating* pEntAnimating = dynamic_cast<CBaseAnimating*>(pEnt);

	if ( pEntAnimating )
	{		
		CaptureInfo_t cap;
		UTIL_InitCaptureInfo( cap, pEnt, NULL );

		if ( pOutNumScaleUpSteps )
		{
			*pOutNumScaleUpSteps = pEntAnimating->Get_CPhotoPlacementQuery()->GetNumScaleUpSteps( &cap );
		}

		if ( pOutNumScaleDownSteps )
		{
			*pOutNumScaleDownSteps = pEntAnimating->Get_CPhotoPlacementQuery()->GetNumScaleDownSteps( &cap );
		}

		return pEntAnimating->GetObjectScaleLevel();
	}

	return 0;
}




class CTraceFilterCollisionGroups : public CTraceFilterHitAll
{
public:
	int m_collisionGroup;
	ITraceFilter *m_pNextFilter;

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		return m_pNextFilter->ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

bool CBaseEntity::CPhotoPlacementQuery::CheckPlacement( CaptureInfo_t &captureInfo, int iScaleStep, const Vector &vPlacementOrigin, const Vector &vPlacementDirection, const QAngle &qPlacementAngles, Vector &positionOut, QAngle &anglesOut, CInfoPlacementHelper **pHelperOut, ITraceFilter *pTraceFilter )
{
	Assert( captureInfo.pPlacementQuery != NULL );

	CTraceFilterHitAll traceFilterHitEverything;
	CTraceFilterCollisionGroups traceFilterCollisionGroup;
	traceFilterCollisionGroup.m_pNextFilter = pTraceFilter ? pTraceFilter : &traceFilterHitEverything;
	ITraceFilter *pFinalTraceFilter = ModifyBaseTraceFilter( &traceFilterCollisionGroup );

	// First try to detect anything the camera should collide with
	Ray_t ray;
	ray.Init( vPlacementOrigin, vPlacementOrigin + ( vPlacementDirection * captureInfo.pPlacementQuery->GetMaxPlacementDistance() ) );
	trace_t tr;

	traceFilterCollisionGroup.m_collisionGroup = COLLISION_GROUP_PLACEMENT_SOLID;
	enginetrace->TraceRay( ray, MASK_SHOT, pFinalTraceFilter, &tr );

	// Only place on the world, or on portal collision simulators
	if ( tr.DidHitNonWorldEntity() && !FClassnameIs( tr.m_pEnt, "portalsimulator_collisionentity" ) && tr.fraction < 1.0f )
	{
		UTIL_FailurePlacement( tr.endpos, &positionOut, &anglesOut );
		return false;
	}

	// Now try camera solid blockers (xray fizzlers, etc)
	trace_t trTest;

	traceFilterCollisionGroup.m_collisionGroup = COLLISION_GROUP_CAMERA_SOLID;
	enginetrace->TraceRay( ray, MASK_SHOT, pFinalTraceFilter, &trTest );

	// use closest hit
	if ( trTest.fraction < tr.fraction )
		tr = trTest;

	// now try anything solid to shot
	traceFilterCollisionGroup.m_collisionGroup = COLLISION_GROUP_NONE;
	enginetrace->TraceRay( ray, MASK_SHOT, pFinalTraceFilter, &trTest );

	// use closest hit
	if ( trTest.fraction < tr.fraction )
		tr = trTest;

	// Photo erasers block us as do clip brushes
	if ( tr.DidHitNonWorldEntity() && ( FClassnameIs( tr.m_pEnt, "trigger_photo_eraser" ) || 
		FClassnameIs( tr.m_pEnt, "func_placement_clip" ) ) )
	{
		UTIL_FailurePlacement( tr.endpos, &positionOut, &anglesOut );
		return false;
	}

	CProp_Portal* pHitPortal = NULL;
	if ( UTIL_DidTraceTouchPortals( ray, tr, &pHitPortal ) && pHitPortal && pHitPortal->IsActivedAndLinked() )
	{
		//TODO: place through portals
	}


	if( !tr.DidHit() )
	{
		tr.plane.dist = 0.0f;
		tr.plane.normal = vec3_origin;
	}

	CheckPlacementData_t placementData;
	placementData.hPlacementHelper = UTIL_FindPlacementHelper( captureInfo, iScaleStep, tr.endpos );
	placementData.Trace = tr;
	placementData.vTraceOrigin = vPlacementOrigin;
	placementData.vTraceDirection = vPlacementDirection;
	placementData.qTraceAngles = qPlacementAngles;
	placementData.nScaleStep = iScaleStep;
	placementData.fScale = GetScaleForStep( iScaleStep, &captureInfo );
	placementData.pPlacedEntity = tr.m_pEnt;
	placementData.vPlacedPosition = tr.endpos;

	// Set us up with a valid failure position
	UTIL_FailurePlacement( tr.endpos, &positionOut, &anglesOut );

	bool bSucceeded = captureInfo.pPlacementQuery->GetPlacementPosition( captureInfo, placementData, positionOut, anglesOut );

	if( pHelperOut )
		*pHelperOut = placementData.hPlacementHelper.Get();

	return bSucceeded;
}

ITraceFilter *CBaseEntity::CPhotoPlacementQuery::ModifyBaseTraceFilter( ITraceFilter *pBaseFilter )
{
	return pBaseFilter;
}

bool CBaseEntity::CPhotoPlacementQuery::GetPlacementPosition( CaptureInfo_t &captureInfo,
															 CheckPlacementData_t &placementData,
															 Vector &positionOut,
															 QAngle &anglesOut )
{
	if ( placementData.hPlacementHelper && !placementData.hPlacementHelper->ShouldHideUntilPlaced() )
	{
		if ( placementData.hPlacementHelper->ShouldUseHelperAngles() )
			anglesOut = placementData.hPlacementHelper->GetTargetAngles();

		Vector vecDir;
		AngleVectors( placementData.hPlacementHelper->GetTargetAngles(), &vecDir );

		positionOut = placementData.hPlacementHelper->GetTargetOrigin();
		positionOut += vecDir * GetPlacementHelperOffset( captureInfo, placementData );
		
		// see if we were actually supposed to 
		CBaseEntity* pEnt = placementData.hPlacementHelper->GetTargetOverride();
		if( pEnt != NULL )
		{
			placementData.pPlacedEntity = pEnt;
		}

		return true;
	}

	return GetPlacementPosition_NoHelper( captureInfo, placementData, positionOut, anglesOut );
}

bool CBaseEntity::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																	  CheckPlacementData_t &placementData,
																	  Vector &positionOut,
																	  QAngle &anglesOut )
{
	return false;
}

float CBaseEntity::CPhotoPlacementQuery::GetPlacementHelperOffset( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData )
{
	CBaseEntity *pEnt = captureInfo.hCapturedEnt.Get();
	if( pEnt != NULL )
	{
		return -pEnt->CollisionProp()->OBBMins().z * placementData.fScale;
	}
	return 0.0f;
}

int CBaseEntity::CPhotoPlacementQuery::GetNumScaleUpSteps( const CaptureInfo_t* pCaptureInfo )
{
	if( pCaptureInfo && pCaptureInfo->bHasCustomScaleData )
	{
		return CustomDataGetNumScaleUpSteps( pCaptureInfo->customScaleData );
	}
	else
	{
		CameraInfo_ScaleData_t *pDefaultScales = GetSimpleScales();
		if( pDefaultScales )
			return CustomDataGetNumScaleUpSteps( *pDefaultScales );
	}
	return 0;
}

int CBaseEntity::CPhotoPlacementQuery::GetNumScaleDownSteps( const CaptureInfo_t* pCaptureInfo )
{
	if( pCaptureInfo && pCaptureInfo->bHasCustomScaleData )
	{
		return CustomDataGetNumScaleDownSteps( pCaptureInfo->customScaleData );
	}
	else
	{
		CameraInfo_ScaleData_t *pDefaultScales = GetSimpleScales();
		if( pDefaultScales )
			return CustomDataGetNumScaleDownSteps( *pDefaultScales );
	}
	return 0;
}

float CBaseEntity::CPhotoPlacementQuery::GetScaleForStep( int nScaleStep, const CaptureInfo_t* pCaptureInfo )
{
	if( pCaptureInfo && pCaptureInfo->bHasCustomScaleData )
	{
		return CustomDataGetScaleForStep( nScaleStep, pCaptureInfo->customScaleData );
	}
	else
	{
		CameraInfo_ScaleData_t *pDefaultScales = GetSimpleScales();
		if( pDefaultScales )
			return CustomDataGetScaleForStep( nScaleStep, *pDefaultScales );
	}
	return 1.0f;
}

float CBaseEntity::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	return 30.0f * 12.0f;
}

void CBaseEntity::CPhotoPlacementQuery::GetCentering( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &vExtentsOut, Vector &vCenterToOriginOut )
{
	CBaseEntity *pEnt = captureInfo.hCapturedEnt.Get();
	if( pEnt == NULL )
	{
		vExtentsOut = vec3_origin;
		vCenterToOriginOut = vec3_origin;
		return;
	}

	float fScaleRatio = placementData.fScale / GetScaleForStep( captureInfo.nOldScaleLevel, &captureInfo );

#if 1
	//OBB+AABB
	Vector vMaxs = pEnt->CollisionProp()->OBBMaxs();
	Vector vMins = pEnt->CollisionProp()->OBBMins();
	vExtentsOut = (vMaxs - vMins) * (0.5f * fScaleRatio);
	vCenterToOriginOut = (vMaxs + vMins) * ((-0.5f) * fScaleRatio);
#else
	//AABB only
	Vector vOldEntityMaxs;
	Vector vOldEntityMins;
	pEnt->CollisionProp()->WorldSpaceAABB( &vOldEntityMins, &vOldEntityMaxs );

	
	Vector vOldCenter = ((vOldEntityMins + vOldEntityMaxs) / 2.0f);
	vExtentsOut = (vOldEntityMaxs - vOldCenter) * fScaleRatio;
	vCenterToOriginOut = (pEnt->GetAbsOrigin() - vOldCenter) * fScaleRatio;
#endif
}

void CBaseEntity::CPhotoPlacementQuery::GetRotatedCentering( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, const VMatrix &matRotation, Vector &vExtentsOut, Vector &vCenterToOriginOut )
{
	GetCentering( captureInfo, placementData, vExtentsOut, vCenterToOriginOut );

	float fExtentDistribution[6] = { -vExtentsOut.z, // Z-
									-vExtentsOut.x,  // X-
									vExtentsOut.x,  // X+
									-vExtentsOut.y,  // Y-
									vExtentsOut.z,  // Z+
									vExtentsOut.y }; // Y+

	float *pXDistribution = &fExtentDistribution[1];
	float *pYDistribution = &fExtentDistribution[3];

	for( int i = 0; i != 8; ++i )
	{
		Vector vTest;
		vTest.x = pXDistribution[i & (1<<0)]; //fExtentDistribution[(0 or 1) + 1]
		vTest.y = pYDistribution[i & (1<<1)]; //fExtentDistribution[(0 or 2) + 3]
		vTest.z = fExtentDistribution[i & (1<<2)]; //fExtentDistribution[(0 or 4)]
		vTest = matRotation.ApplyRotation( vTest );

		if( vTest.x > vExtentsOut.x )
			vExtentsOut.x = vTest.x;
		if( vTest.y > vExtentsOut.y )
			vExtentsOut.y = vTest.y;
		if( vTest.z > vExtentsOut.z )
			vExtentsOut.z = vTest.z;
	}

	vCenterToOriginOut = matRotation.ApplyRotation( vCenterToOriginOut );

}

void CBaseEntity::CPhotoPlacementQuery::GetRotatedCentering( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, const QAngle &qAngles, Vector &vExtentsOut, Vector &vCenterToOriginOut )
{
	VMatrix matRotation;
	AngleMatrix( qAngles, matRotation.As3x4() );
	GetRotatedCentering( captureInfo, placementData, matRotation, vExtentsOut, vCenterToOriginOut );
}

CameraInfo_ScaleData_t *CBaseEntity::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

int CBaseEntity::CPhotoPlacementQuery::CustomDataGetNumScaleUpSteps( const CameraInfo_ScaleData_t &ScaleData )
{
	return ScaleData.nNumScaleUpLevels;
}

int CBaseEntity::CPhotoPlacementQuery::CustomDataGetNumScaleDownSteps( const CameraInfo_ScaleData_t &ScaleData )
{
	return ScaleData.nNumScaleDownLevels;
}

float CBaseEntity::CPhotoPlacementQuery::CustomDataGetScaleForStep( int nScaleStep, const CameraInfo_ScaleData_t &ScaleData )
{
	if ( nScaleStep > 0 )
	{
		Assert( (nScaleStep-1) < ScaleData.nNumScaleUpLevels );
		return ScaleData.flScaleUpLevelMultipliers[ nScaleStep-1 ];
	}
	else if ( nScaleStep < 0 )
	{
		Assert( ((-nScaleStep)-1) < ScaleData.nNumScaleDownLevels );
		return ScaleData.flScaleDownLevelMultipliers[ (-nScaleStep)-1 ];
	}
	else
	{	// step 0 is the default scale.
		return 1.0f;
	}
}


bool CBaseEntity::CPhotoPlacementQuery::WallPlacement( float fBumpLeftRightDist,
														float fBumpUpDownDist,
														float fBumpOffWall,
														CaptureInfo_t &captureInfo,
														CheckPlacementData_t &placementData,
														Vector &positionOut,
														QAngle &anglesOut )
{
	if ( placementData.Trace.fraction == 1.0f ) //can't place in midair
		return false;

	if ( (placementData.Trace.DidHitWorld() == false && !FClassnameIs( placementData.Trace.m_pEnt, "portalsimulator_collisionentity" )) || 
		placementData.Trace.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) || 
		StringHasPrefix( placementData.Trace.surface.name, "**displacement**" ) )
		return false;

	Assert( (placementData.Trace.plane.normal.LengthSqr() - 1.0f) < FLT_EPSILON );
	positionOut = placementData.Trace.endpos;

	// Get our other two directions
	Vector vecForward, vecRight, vecUp;
	vecForward = placementData.Trace.plane.normal;
	VectorVectors( vecForward, vecRight, vecUp );
	VectorAngles( vecForward, vecUp, anglesOut );

	vecForward *= fBumpOffWall;
	vecRight *= fBumpLeftRightDist;
	vecUp *= fBumpUpDownDist;

	for( int iTries = 0; iTries < 3; ++iTries )
	{
		bool bDoneBumping = true; //assume we're done and disprove

		// Right
		trace_t tr;
		UTIL_TraceLine( positionOut, positionOut + vecRight, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			bDoneBumping = false;
			positionOut -= vecRight * ( 1.0f - tr.fraction );
		}

		UTIL_TraceLine( positionOut, positionOut - vecRight, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			bDoneBumping = false;
			positionOut += vecRight * ( 1.0f - tr.fraction );
		}

		// Up
		UTIL_TraceLine( positionOut, positionOut + vecUp, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			bDoneBumping = false;
			positionOut -= vecUp * ( 1.0f - tr.fraction );
		}

		UTIL_TraceLine( positionOut, positionOut - vecUp, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f )
		{
			bDoneBumping = false;
			positionOut += vecUp * ( 1.0f - tr.fraction );
		}

		if( bDoneBumping )
		{
			positionOut += vecForward;

			// Must be able to fit here!
			Vector vecMaxs = vecForward;
			vecMaxs += vecRight;
			vecMaxs += vecUp;
			Vector vecMins = vec3_origin;
			vecMins -= vecRight;
			vecMins -= vecUp;

			UTIL_AlignBBox( vecMins, vecMaxs );
			UTIL_TraceHull( positionOut, positionOut + Vector( 0, 0, 1 ), vecMins, vecMaxs, CONTENTS_SOLID, NULL, COLLISION_GROUP_NONE, &tr );

			return !(tr.startsolid || tr.allsolid);
		}
	}

	return false;
}


bool CBaseEntity::CPhotoPlacementQuery::SpacePlacement( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, const QAngle &qPlacementAngleIN, int traceMask, Vector &positionOut )
{
	Vector vExtents, vCenterToOrigin;
	GetRotatedCentering( captureInfo, placementData, qPlacementAngleIN, vExtents, vCenterToOrigin );

	Vector vCenter = placementData.Trace.endpos;

	Vector vFit, vPush;
	if( placementData.Trace.plane.normal != vec3_origin )
	{
		//push off the plane
		vCenter.x += placementData.Trace.plane.normal.x * vExtents.x;
		vCenter.y += placementData.Trace.plane.normal.y * vExtents.y;
		vCenter.z += placementData.Trace.plane.normal.z * vExtents.z;

		vPush = placementData.Trace.plane.normal;
	}
	else
	{
		vPush = -placementData.vTraceDirection;
		vPush.NormalizeInPlace();
	}

	//assume the ground is a big threat
	{
		trace_t tr;
		UTIL_TraceLine( vCenter, vCenter + Vector( 0.0f, 0.0f, -vExtents.z ), traceMask, placementData.pTraceFilter, &tr );
		vCenter.z += vExtents.z * (1.0f - tr.fraction);
	}

	bool bFits = UTIL_FindClosestPassableSpace( vCenter, vExtents, vPush, placementData.pTraceFilter, traceMask, 20, vFit );

	positionOut = vFit + vCenterToOrigin;

	if( bFits )
	{
		//make sure we haven't placed on the other side of a wall or something terrible
		trace_t tr;
		//check from placement start to finish
		UTIL_TraceLine( placementData.Trace.endpos, vFit, traceMask, placementData.pTraceFilter, &tr );
		if( tr.fraction != 1.0f )
		{
			//try again from placement trace origin to finish
			UTIL_TraceLine( placementData.vTraceOrigin, vFit, traceMask, placementData.pTraceFilter, &tr );
			if( tr.fraction != 1.0f )
			{
				positionOut = placementData.Trace.endpos + vCenterToOrigin;
				bFits = false;
			}
		}
	}

	return bFits;
}
