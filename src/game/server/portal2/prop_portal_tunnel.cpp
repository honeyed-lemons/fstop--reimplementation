//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Portal tunnel
//
//===========================================================================//

#include "cbase.h"
#include "prop_portal.h"
#include "ai_utils.h"
#include "particle_system.h"
#include "entityblocker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const char* g_pszCreateTunnelContext = "CreateTunnelThinkContext";

#define PORTAL_TUNNEL_MODEL_NAME "models/props_gameplay/aperture_door_frame.mdl"
#define PORTAL_DOOR_FIZZLE_EFFECT "fizzler_field_scalable"
#define PORTAL_TUNNEL_MODEL_HALFWIDTH 32.0f //scales the portal to fit the model. Update these if the model changes
#define PORTAL_TUNNEL_MODEL_HALFHEIGHT 56.0f

ConVar sv_portal_door_use_legacy_placement_rules("sv_portal_door_use_legacy_placement_rules", "0");
ConVar sv_portal_door_allowscale("sv_portal_door_allowscale", "0");



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecMins - 
//			&vecMaxs - 
//-----------------------------------------------------------------------------
void UTIL_AlignBBox(Vector& vecMins, Vector& vecMaxs)
{
	for (int i = 0; i < 3; i++)
	{
		if (vecMins[i] > vecMaxs[i])
		{
			V_swap(vecMins[i], vecMaxs[i]);
		}
	}
}

//------------------------------------------------------------------------------
// Portal tunnel (temp)
//------------------------------------------------------------------------------

class CPropPortalTunnel : public CBaseAnimating
{
public:
	DECLARE_CLASS(CPropPortalTunnel, CBaseAnimating);

	CPropPortalTunnel(void);

	virtual void UpdateOnRemove(void);
	virtual void Precache(void);
	virtual void Spawn(void);
	virtual void Activate(void);

	virtual void OnCaptured(void);
	virtual void OnReleased(void);
	START_BRANCHING_SINGLETON_DEFINITION(CPhotoPlacementQuery)
	{
	public:
		virtual bool GetPlacementPosition(CaptureInfo_t & captureInfo, CheckPlacementData_t & placementData, Vector & positionOut, QAngle & anglesOut);
		virtual float GetMaxPlacementDistance(void);

	protected:
		virtual CameraInfo_ScaleData_t* GetSimpleScales(void);
	};
	END_BRANCHING_SINGLETON_DEFINITION(CPhotoPlacementQuery);

private:

	void CreateParticleField(void);
	bool PositionIsValid(const Vector& vecPosition, const QAngle& vecAngles, Vector* vecResultPosition, QAngle* vecResultAngles);
	bool FindPortalPoint(const char* lpszCandidateName, Vector* vecOriginOut, QAngle* vecAnglesOut);
	void CreateTunnel(void);
	void CreateDoor(void);
	void Destroy(void);

	inline void SetPartner(CPropPortalTunnel* pPartner) { m_hPartner = pPartner; }
	inline void SetPortalGroupID(int nID) { m_nPortalGroupID = nID; }

	DECLARE_DATADESC();

private:
	int							m_nPortalGroupID;			// GUID for our portal for linking
	CHandle<CProp_Portal>		m_hPortal;					// Actual portal
	string_t					m_strHorizontalFailTarget;	// Failed target
	string_t					m_strFloorFailTarget;		// Fail target for floor placed doors
	string_t					m_strCeilingFailTarget;		// Fail target for ceiling placed doors
	string_t					m_strSuccessTarget;			// Success target (if specified, does this instead of tunneling)
	CHandle<CPropPortalTunnel>	m_hPartner;					// The other side of the door (we're always linked to it!
	CHandle<CParticleSystem>	m_hParticleField;			// Particle effect
	CHandle<CEntityBlocker>		m_hPhysicsBlocker;			// Blocker for objects passing through

	const string_t& GetFailTargetString(void) const;
	void CreateTunnelThink(void);
};

LINK_ENTITY_TO_CLASS(prop_portal_tunnel, CPropPortalTunnel);

BEGIN_DATADESC(CPropPortalTunnel)
DEFINE_KEYFIELD(m_strHorizontalFailTarget, FIELD_STRING, "failtarget"),
DEFINE_KEYFIELD(m_strFloorFailTarget, FIELD_STRING, "failtarget_floor"),
DEFINE_KEYFIELD(m_strCeilingFailTarget, FIELD_STRING, "failtarget_ceiling"),
DEFINE_KEYFIELD(m_strSuccessTarget, FIELD_STRING, "successtarget"),

DEFINE_THINKFUNC(CreateTunnelThink),

DEFINE_FIELD(m_nPortalGroupID, FIELD_INTEGER),
DEFINE_FIELD(m_hPortal, FIELD_EHANDLE),
DEFINE_FIELD(m_hPartner, FIELD_EHANDLE),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropPortalTunnel::CPropPortalTunnel(void) : m_hPartner(NULL), m_hPortal(NULL)//, m_nPortalGroupID(PORTAL_LINKAGE_GROUP_INVALID)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::Destroy(void)
{
	if (m_hPartner)
	{
		UTIL_Remove(m_hPartner);
		m_hPartner = NULL;
	}

	if (m_hPortal)
	{
		UTIL_Remove(m_hPortal);
		m_hPortal = NULL;
	}

	if (m_hParticleField)
	{
		UTIL_Remove((CBaseEntity*)m_hParticleField.Get());
		m_hParticleField = NULL;
	}

	if (m_hPhysicsBlocker)
	{
		UTIL_Remove(m_hPhysicsBlocker);
		m_hPhysicsBlocker = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::UpdateOnRemove(void)
{
	Destroy();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::Precache(void)
{
	//PrecacheModel( "models/props_farm/doorframe001.mdl" );
	PrecacheModel(PORTAL_TUNNEL_MODEL_NAME);

	// FIXME: This should be parameterized
	// PrecacheModel( "models/props_farm/door.mdl" );

	PrecacheParticleSystem(PORTAL_DOOR_FIZZLE_EFFECT);
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
void CPropPortalTunnel::CreateDoor( void )
{
	CBaseEntity *pDoor = CreateEntityByName( "prop_door_rotating" );

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

	Vector vecOffset = GetAbsOrigin();
	vecOffset += ( vecRight * (DOOR_WIDTH/2) );
	vecOffset -= ( vecUp * (DOOR_HEIGHT/2) );

	UTIL_SetOrigin( pDoor, vecOffset );
	pDoor->SetAbsAngles( GetAbsAngles() );

	// Setup our door model name
	// FIXME: This should be parameterized
	pDoor->KeyValue( "model",  "models/props_farm/door.mdl" );
	DispatchSpawn( pDoor );

	// Save us for later
	m_hDoor = (CPropDoorRotating *) pDoor;
}
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::CreateParticleField(void)
{
	if (m_hParticleField != NULL)
	{
		UTIL_Remove(m_hParticleField);
		m_hParticleField = NULL;
	}

	m_hParticleField = (CParticleSystem*)CreateEntityByName("info_particle_system");
	if (m_hParticleField == NULL)
		return;

	// Setup our basic parameters
	m_hParticleField->KeyValue("start_active", "1");
	m_hParticleField->KeyValue("effect_name", PORTAL_DOOR_FIZZLE_EFFECT);

	m_hParticleField->SetControlPointValue(1, Vector(GetModelScale(), 0.0f, 0.0f));

	Vector vecForward;
	GetVectors(&vecForward, NULL, NULL);
	m_hParticleField->SetAbsOrigin(GetAbsOrigin() + (vecForward * 1.1f));

	m_hParticleField->SetAbsAngles(GetAbsAngles());
	DispatchSpawn(m_hParticleField);
	if (gpGlobals->curtime > 0.5f)
		m_hParticleField->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::Spawn(void)
{
	Precache();

	BaseClass::Spawn();

	// Attach ourselves to the nearest wall behind us
	Vector vecDir;
	AngleVectors(GetAbsAngles(), &vecDir);

	// If they have a success case, always use that
	if (m_hPartner && m_strSuccessTarget != NULL_STRING)
	{
		Vector vFinalPosition;
		QAngle qFinalAngles;
		if (FindPortalPoint(STRING(m_strSuccessTarget), &vFinalPosition, &qFinalAngles) == false)
			return;

		SetAbsOrigin(vFinalPosition);
		SetAbsAngles(qFinalAngles);
	}
	else
	{
		trace_t tr;
		UTIL_TraceLine(GetAbsOrigin() + (vecDir * 1.0f), GetAbsOrigin() - (vecDir * 17.0f), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if (m_hPartner == NULL)
		{
			// FIXME: Too many presentation problems right now!
			// CreateDoor();
		}

		// If we didn't hit something, we can't be placed
		if (tr.DidHit() && tr.startsolid == false && tr.allsolid == false && !enginetrace->PointOutsideWorld(tr.endpos + (tr.plane.normal * 4.0f)))
		{
			UTIL_SetOrigin(this, tr.endpos);
			QAngle vecAngles;
			//VectorAngles( tr.plane.normal, vecAngles );
			//SetAbsAngles( vecAngles );
		}
		else
		{
			if (m_hPartner)
			{
				Vector vFinalPosition;
				QAngle qFinalAngles;
				string_t strFail = GetFailTargetString();
				if (FindPortalPoint(STRING(strFail), &vFinalPosition, &qFinalAngles) == false)
				{
					Warning("prop_portal_door '%s' needs a fail target!\n", GetDebugName());
					return;
				}

				// Don't let them capture the other side of the failure door or they'll be trapped!
				m_bCanBeCaptured = false;

				SetAbsOrigin(vFinalPosition);
				SetAbsAngles(qFinalAngles);
			}
		}
	}

	CreateParticleField();

	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_OBB_YAW);
	SetCollisionGroup(COLLISION_GROUP_CAMERA_SOLID);

	SetModel(PORTAL_TUNNEL_MODEL_NAME);

	// Create a blocker internally
	Vector vecMins, vecMaxs;
	QAngle vecOldAngles = GetAbsAngles();

	CollisionProp()->WorldSpaceAABB(&vecMins, &vecMaxs);
	vecMins -= GetAbsOrigin();
	vecMaxs -= GetAbsOrigin();
	vecMins[0] *= 0.9f;
	vecMins[1] *= 0.9f;
	vecMaxs *= 0.9f;

	// Create an entity blocker
	m_hPhysicsBlocker = CEntityBlocker::Create(GetAbsOrigin(), vecMins, vecMaxs, UTIL_GetLocalPlayer(), true);

	// Don't cast shadows because we're going to be moving around
	AddEffects(EF_NOSHADOW);

	// Create our portals when the world starts simulating
	SetContextThink(&CPropPortalTunnel::CreateTunnelThink, gpGlobals->curtime, g_pszCreateTunnelContext);

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::Activate(void)
{
	BaseClass::Activate();
}

void CPropPortalTunnel::CreateTunnelThink(void)
{
	// Now, create our tunnel
	CreateTunnel();

	SetContextThink(NULL, TICK_NEVER_THINK, g_pszCreateTunnelContext);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropPortalTunnel::FindPortalPoint(const char* lpszCandidateName, Vector* vecOriginOut, QAngle* vecAnglesOut)
{
	// Find a failure point and use that!
	string_t strName = AllocPooledString(lpszCandidateName);
	if (strName == NULL_STRING)
		return false;

	CUtlVector<CBaseEntity*> vPortalPoints;
	CBaseEntity* pPoint = NULL;
	while ((pPoint = gEntList.FindEntityByName(pPoint, strName)) != NULL)
	{
		vPortalPoints.AddToTail(pPoint);
	}

	if (vPortalPoints.Count())
	{
		int nRandomPortal = random->RandomInt(0, vPortalPoints.Count() - 1);
		pPoint = vPortalPoints[nRandomPortal];

		Vector vecForward;
		AngleVectors(pPoint->GetAbsAngles(), &vecForward);

		trace_t tr;
		UTIL_TraceLine(pPoint->GetAbsOrigin(), pPoint->GetAbsOrigin() - (vecForward * 16.0f), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		QAngle vecAngles;
		VectorAngles(tr.plane.normal, vecAngles);

		*vecOriginOut = tr.endpos;
		*vecAnglesOut = vecAngles;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropPortalTunnel::PositionIsValid(const Vector& vecPosition, const QAngle& vecAngles, Vector* vecResultPosition, QAngle* vecResultAngles)
{
	// Assuming the frame wants to center itself around the point specified, check out extents
	Vector vecOffset = vecPosition;
	Vector vecForward, vecRight, vecUp;
	AngleVectors(vecAngles, &vecForward, &vecRight, &vecUp);

	float fScale = GetModelScale();

	// Trace to the sides
	Vector vecMaxs;
	vecMaxs = vecForward * 4.0f;
	vecMaxs += vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale);

	Vector vecMins = -vecMaxs;
	UTIL_AlignBBox(vecMins, vecMaxs);

	vecMaxs = vecForward * 4.0f;
	vecMaxs += vecUp * (PORTAL_TUNNEL_MODEL_HALFHEIGHT * fScale);
	vecMins = -vecMaxs;
	UTIL_AlignBBox(vecMins, vecMaxs);

	// Bail if center point is in solid
	if (enginetrace->PointOutsideWorld(vecPosition))
		return false;

	trace_t tr;
	UTIL_TraceHull(vecOffset, vecOffset + (vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale)), vecMins, vecMaxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if (tr.startsolid || tr.allsolid)
		return false;

	if (tr.fraction < 1.0f)
	{
		vecOffset -= vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale) * (1.0f - tr.fraction);
	}

	UTIL_TraceHull(vecOffset, vecOffset - (vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale)), vecMins, vecMaxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if (tr.startsolid || tr.allsolid)
		return false;

	if (tr.fraction < 1.0f)
	{
		vecOffset += vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale) * (1.0f - tr.fraction);
	}

	//FIXME: At this point we may be squashed!

	*vecResultPosition = vecOffset;
	*vecResultAngles = vecAngles;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::CreateTunnel(void)
{
	// FIXME: This is shady -- double allocation coming from Activate / OnReleased doing the same thing
	if (m_hPortal)
	{
		UTIL_Remove(m_hPortal);
		m_hPortal = NULL;
	}

	float fScale = GetModelScale();

	// First, test our placement position
	Vector vFinalPosition;
	QAngle qFinalAngles;
	// --------------------------------------------
	// Primary portal
	// --------------------------------------------

	Vector vecDir;
	AngleVectors(GetAbsAngles(), &vecDir);
	Vector vecTraceStart = GetAbsOrigin() + (vecDir * 1.0f);

	//if (m_nPortalGroupID == PORTAL_LINKAGE_GROUP_INVALID)
	//{
	//	SetPortalGroupID(UTIL_GetUnusedLinkageID());
	//}

	bool bSecondary = (m_hPartner != NULL);
	CProp_Portal* pPortal = CProp_Portal::FindPortal(m_nPortalGroupID, bSecondary, true);

	//pPortal->Resize(PORTAL_TUNNEL_MODEL_HALFWIDTH * fScale, PORTAL_TUNNEL_MODEL_HALFHEIGHT * fScale);

	//HACK: Remove the portal's microphone/speaker to fix sound bug in rooms with many portal doors.
	if (pPortal)
	{
		//pPortal->RemovePortalMicAndSpeaker();
		//pPortal->m_bHACKUseMicrophones = false;
	}

	vFinalPosition = GetAbsOrigin();
	qFinalAngles = GetAbsAngles();

	pPortal->PlacePortal(vFinalPosition, qFinalAngles, 1.0f, true);

	// NDebugOverlay::Cross3D( vFinalPosition, PORTAL_TUNNEL_MODEL_HALFWIDTH, 0, 255, 0, true, 20.0f );

	pPortal->SetContextThink(&CProp_Portal::DelayedPlacementThink, gpGlobals->curtime, s_pDelayedPlacementContext);
	pPortal->m_vDelayedPosition = vFinalPosition;
	pPortal->m_hPlacedBy = AI_GetSinglePlayer();

	m_hPortal = pPortal;
	m_hPortal->SetOwnerEntity(this);

	if (m_hPartner == NULL)
	{
		vFinalPosition = GetAbsOrigin() - (vecDir * 16.0f);

		Vector vecForward, vecRight, vecUp;
		GetVectors(&vecForward, &vecRight, &vecUp);

		// rotate 180 about modelspace up
		VMatrix matRot;
		MatrixBuildRotationAboutAxis(matRot, vecUp, 180);

		VMatrix matOut;
		matOut = matRot * EntityToWorldTransform();
		MatrixAngles(matOut.As3x4(), qFinalAngles);


		CPropPortalTunnel* pPartner = (CPropPortalTunnel*)CreateEntityByName("prop_portal_tunnel");
		if (pPartner)
		{
			pPartner->SetPartner(this);
			pPartner->SetAbsOrigin(vFinalPosition);
			pPartner->SetAbsAngles(qFinalAngles);
			pPartner->SetModelScale(fScale);

			pPartner->KeyValue("failtarget", STRING(m_strHorizontalFailTarget));
			pPartner->KeyValue("failtarget_ceiling", STRING(m_strCeilingFailTarget));
			pPartner->KeyValue("failtarget_floor", STRING(m_strFloorFailTarget));
			pPartner->KeyValue("successtarget", STRING(m_strSuccessTarget));
			pPartner->KeyValue("canbecaptured", m_bCanBeCaptured);

			// Make us link to the same portal group
			pPartner->SetPortalGroupID(m_nPortalGroupID);

			DispatchSpawn(pPartner);
			if (gpGlobals->curtime > 2.0f)
			{
				pPartner->Activate();
			}

			// Maintain it for ourselves
			m_hPartner = pPartner;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::OnCaptured(void)
{
	// We need to clear our partner's pointer to us so it doesn't try to remove us on its destroy call (due to stasis)
	if (m_hPartner)
	{
		m_hPartner->SetPartner(NULL);
	}

	Destroy();

	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalTunnel::OnReleased(void)
{
	CreateTunnel();

	BaseClass::OnReleased();
}

const string_t& CPropPortalTunnel::GetFailTargetString(void) const
{
	Vector vForward;
	GetVectors(&vForward, NULL, NULL);

	if ((vForward[ROLL] < -FLT_EPSILON) && m_strFloorFailTarget != NULL_STRING)
	{
		return m_strFloorFailTarget;
	}
	else if ((vForward[ROLL] > FLT_EPSILON) && m_strCeilingFailTarget != NULL_STRING)
	{
		return m_strCeilingFailTarget;
	}
	else
	{
		return m_strHorizontalFailTarget;
	}
}


bool CPropPortalTunnel::CPhotoPlacementQuery::GetPlacementPosition(CaptureInfo_t& captureInfo,
	CheckPlacementData_t& placementData,
	Vector& positionOut,
	QAngle& anglesOut)
{
	Vector vecNormal = (placementData.Trace.plane.normal == vec3_origin) ? -placementData.vTraceDirection : placementData.Trace.plane.normal;

	// If we fail, our origin and angles will default to a reasonable value
	positionOut = placementData.Trace.endpos;

	{
		matrix3x4_t matSurface;
		QAngle vecEndAngles;
		VectorAngles(vecNormal, vecEndAngles);
		AngleMatrix(vecEndAngles, placementData.Trace.endpos, matSurface);

		Vector vecDir = -placementData.vTraceDirection;

		anglesOut = TransformAnglesToWorldSpace(QAngle(90, 0, 0), matSurface);
		anglesOut[YAW] = UTIL_VecToYaw(vecDir);
	}

	bool bIsOnHorizontalSurf = false;

	// if in legacy mode, we only want to be placed on vertical surfaces only
	if ((vecNormal[2] > FLT_EPSILON || vecNormal[2] < -FLT_EPSILON))
	{
		if (sv_portal_door_use_legacy_placement_rules.GetBool())
		{
			// Move off the origin
			positionOut += vecNormal * (PORTAL_TUNNEL_MODEL_HALFHEIGHT * placementData.fScale);
			return false;
		}

		// different bumping rules (below) for horizontal surfaces
		bIsOnHorizontalSurf = true;
	}

	// Assuming the frame wants to center itself around the point specified, check out extents
	Vector vecOffset = placementData.Trace.endpos;
	Vector vecForward, vecRight, vecUp;
	VectorVectors(vecNormal, vecRight, vecUp);

	vecForward = vecNormal;


	trace_t tr;
	UTIL_TraceLine(vecOffset, vecOffset - Vector(0, 0, (PORTAL_TUNNEL_MODEL_HALFHEIGHT * placementData.fScale)), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);

	// if in legacy mode, only accept placement with the door bottom flush with the ground
	if (sv_portal_door_use_legacy_placement_rules.GetBool())
	{
		if (tr.fraction == 1.0f)
			return false;
	}

	// bump up half-height if we have a floor below us
	if (tr.fraction < 1.0f)
		vecOffset[2] = tr.endpos[2] + ((bIsOnHorizontalSurf) ? (0.0f) : ((PORTAL_TUNNEL_MODEL_HALFHEIGHT * placementData.fScale)));

	// Trace to the sides
	Vector vecMaxs = vecForward * 4.0f;
	vecMaxs += vecUp * ((bIsOnHorizontalSurf) ? (1.0f) : ((PORTAL_TUNNEL_MODEL_HALFHEIGHT * placementData.fScale)));
	Vector vecMins = -vecMaxs;
	vecMins += vecForward * 4.0f;

	UTIL_AlignBBox(vecMins, vecMaxs);

	UTIL_TraceHull(vecOffset, vecOffset + (vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * placementData.fScale)), vecMins, vecMaxs, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);
	if (tr.startsolid || tr.allsolid)
		return false;

	if (tr.fraction < 1.0f)
	{
		vecOffset -= vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * placementData.fScale) * (1.0f - tr.fraction);
	}

	UTIL_TraceHull(vecOffset, vecOffset - (vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * placementData.fScale)), vecMins, vecMaxs, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);
	if (tr.startsolid || tr.allsolid)
		return false;

	if (tr.fraction < 1.0f)
	{
		vecOffset += vecRight * (PORTAL_TUNNEL_MODEL_HALFWIDTH * placementData.fScale) * (1.0f - tr.fraction);
	}

	//FIXME: At this point we may be squashed!

	// move slightly off our resting surface to prevent z-fighting when placed on transparent surfs.
	vecOffset += vecForward * 0.2f;
	positionOut = vecOffset;


	{
		matrix3x4_t matSurface;
		QAngle vecEndAngles;
		VectorAngles(vecNormal, vecEndAngles);
		AngleMatrix(vecEndAngles, placementData.Trace.endpos, matSurface);

		anglesOut = TransformAnglesToWorldSpace(QAngle(0, 0, 0), matSurface);
	}

	return true;
}

float CPropPortalTunnel::CPhotoPlacementQuery::GetMaxPlacementDistance(void)
{
	return 1800.0f;
}


CameraInfo_ScaleData_t* CPropPortalTunnel::CPhotoPlacementQuery::GetSimpleScales(void)
{
	static float s_DefaultScales[] = { 0.2f, 0.6f, 1.0f, 1.5f, 2.0f };
	static CameraInfo_ScaleData_t simpleScales(s_DefaultScales, sizeof(s_DefaultScales) / sizeof(float));
	return &simpleScales;
}

