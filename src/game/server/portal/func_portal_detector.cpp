//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "func_portal_detector.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Spawnflags
#define SF_START_INACTIVE			0x01

// Global list of portal detectors. These are iterated and refreshed 
CEntityClassList<CFuncPortalDetector> g_FuncNoPortalVolumeList;
template <> CFuncPortalDetector* CEntityClassList<CFuncPortalDetector>::m_pClassList = NULL;

CFuncPortalDetector* GetPortalDetectorList()
{
	return g_FuncNoPortalVolumeList.m_pClassList;
}


LINK_ENTITY_TO_CLASS(func_portal_detector, CFuncPortalDetector);

BEGIN_DATADESC(CFuncPortalDetector)

DEFINE_FIELD(m_bActive, FIELD_BOOLEAN),
DEFINE_ARRAY(m_phTouchingPortals, FIELD_EHANDLE, 2),
DEFINE_FIELD(m_iTouchingPortalCount, FIELD_INTEGER),
DEFINE_KEYFIELD(m_iLinkageGroupID, FIELD_INTEGER, "LinkageGroupID"),

// Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
DEFINE_INPUTFUNC(FIELD_VOID, "Toggle", InputToggle),

DEFINE_OUTPUT(m_OnStartTouchPortal, "OnStartTouchPortal"),
DEFINE_OUTPUT(m_OnStartTouchPortal1, "OnStartTouchPortal1"),
DEFINE_OUTPUT(m_OnStartTouchPortal2, "OnStartTouchPortal2"),
DEFINE_OUTPUT(m_OnStartTouchLinkedPortal, "OnStartTouchLinkedPortal"),
DEFINE_OUTPUT(m_OnStartTouchBothLinkedPortals, "OnStartTouchBothLinkedPortals"),

DEFINE_OUTPUT(m_OnEndTouchPortal, "OnEndTouchPortal"),
DEFINE_OUTPUT(m_OnEndTouchPortal1, "OnEndTouchPortal1"),
DEFINE_OUTPUT(m_OnEndTouchPortal2, "OnEndTouchPortal2"),
DEFINE_OUTPUT(m_OnEndTouchLinkedPortal, "OnEndTouchLinkedPortal"),
DEFINE_OUTPUT(m_OnEndTouchBothLinkedPortals, "OnEndTouchBothLinkedPortals"),

DEFINE_FUNCTION(IsActive),

END_DATADESC()



CFuncPortalDetector::CFuncPortalDetector() :m_iTouchingPortalCount(0), m_bActive(false), m_iLinkageGroupID(0)
{
	// Add me to the global list
	g_FuncNoPortalVolumeList.Insert(this);
}

CFuncPortalDetector::~CFuncPortalDetector()
{
	g_FuncNoPortalVolumeList.Remove(this);
}

void CFuncPortalDetector::Spawn()
{
	BaseClass::Spawn();

	if (m_spawnflags & SF_START_INACTIVE)
	{
		m_bActive = false;
	}
	else
	{
		m_bActive = true;
	}

	// Bind to our model, cause we need the extents for bounds checking
	SetModel(STRING(GetModelName()));
	SetRenderMode(kRenderNone);	// Don't draw
	SetSolid(SOLID_VPHYSICS);		// we may want slanted walls, so we'll use OBB
	AddSolidFlags(FSOLID_NOT_SOLID);
}

void CFuncPortalDetector::SetActive(bool bActive)
{
	m_bActive = bActive;

	Vector vMin, vMax;
	CollisionProp()->WorldSpaceAABB(&vMin, &vMax);

	Vector vBoxCenter = (vMin + vMax) * 0.5f;
	Vector vBoxExtents = (vMax - vMin) * 0.5f;

	bool bTouchedPortal1 = false;
	bool bTouchedPortal2 = false;

	// Send start touches if we're just waking up, send end touches if we're being turned off
	COutputEvent& TouchingPortal = bActive ? m_OnStartTouchPortal : m_OnEndTouchPortal;
	COutputEvent& TouchingPortal2 = bActive ? m_OnStartTouchPortal2 : m_OnEndTouchPortal2;
	COutputEvent& TouchingPortal1 = bActive ? m_OnStartTouchPortal1 : m_OnEndTouchPortal1;
	COutputEvent& TouchingLinked = bActive ? m_OnStartTouchLinkedPortal : m_OnEndTouchLinkedPortal;
	COutputEvent& TouchingBothLinked = bActive ? m_OnStartTouchBothLinkedPortals : m_OnEndTouchBothLinkedPortals;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if (iPortalCount != 0)
	{
		CProp_Portal** pPortals = CProp_Portal_Shared::AllPortals.Base();
		for (int i = 0; i != iPortalCount; ++i)
		{
			CProp_Portal* pTempPortal = pPortals[i];

			//require that it's active and/or linked?
			bool bTouchedAtLeastOnePortal = false;
			if (pTempPortal->GetLinkageGroup() == m_iLinkageGroupID && UTIL_IsBoxIntersectingPortal(vBoxCenter, vBoxExtents, pTempPortal))
			{
				if (!bTouchedAtLeastOnePortal)
				{
					TouchingPortal.FireOutput(pTempPortal, this);
					bTouchedAtLeastOnePortal = true;
				}


				if (pTempPortal->IsPortal2())
				{
					TouchingPortal2.FireOutput(pTempPortal, this);

					if (pTempPortal->IsActivedAndLinked())
					{
						bTouchedPortal2 = true;
						TouchingLinked.FireOutput(pTempPortal, this);
					}
				}
				else
				{
					TouchingPortal1.FireOutput(pTempPortal, this);

					if (pTempPortal->IsActivedAndLinked())
					{
						bTouchedPortal1 = true;
						TouchingLinked.FireOutput(pTempPortal, this);
					}
				}
			}
		}
	}

	if (bTouchedPortal1 && bTouchedPortal2)
	{
		TouchingBothLinked.FireOutput(this, this);
	}
}

void CFuncPortalDetector::InputDisable(inputdata_t& inputdata)
{
	SetActive(false);
}

void CFuncPortalDetector::InputEnable(inputdata_t& inputdata)
{
	SetActive(true);
}

void CFuncPortalDetector::InputToggle(inputdata_t& inputdata)
{
	m_bActive = !m_bActive;
	SetActive(m_bActive);

}

void CFuncPortalDetector::UpdateOnPortalMoved(CProp_Portal* pPortal)
{
	if (m_bActive == false)
		return;

	Assert(m_iTouchingPortalCount >= 0 && m_iTouchingPortalCount <= 2);

	// Is this portal in our touching list?
	bool bWasTouchingPortalDetector = IsPortalTouchingDetector(pPortal);

	// Test if the portal is now (after movement) touching this detector
	bool bIsTouchingPortalDetector = false;
	if (GetLinkageGroupID() == pPortal->GetLinkageGroup())
	{
		// It's detecting this portal's group
		Vector vMin, vMax;
		CollisionProp()->WorldSpaceAABB(&vMin, &vMax);

		Vector vBoxCenter = (vMin + vMax) * 0.5f;
		Vector vBoxExtents = (vMax - vMin) * 0.5f;

		if (UTIL_IsBoxIntersectingPortal(vBoxCenter, vBoxExtents, pPortal))
		{
			bIsTouchingPortalDetector = true;
		}
	}

	// Was touching, no longer touching 
	if (bIsTouchingPortalDetector == false && bWasTouchingPortalDetector == true)
	{
		Assert(m_iTouchingPortalCount > 0 && m_iTouchingPortalCount <= 2);
		m_phTouchingPortals[(pPortal->IsPortal2()) ? (1) : (0)] = NULL;
		m_iTouchingPortalCount--;
		PortalRemovedFromInsideBounds(pPortal);
	}

	// Newly touching this detector
	if (bIsTouchingPortalDetector == true && bWasTouchingPortalDetector == false)
	{
		Assert(m_iTouchingPortalCount >= 0 && m_iTouchingPortalCount < 2);
		m_phTouchingPortals[(pPortal->IsPortal2()) ? (1) : (0)] = pPortal;
		m_iTouchingPortalCount++;
		PortalPlacedInsideBounds(pPortal);
	}
}

void CFuncPortalDetector::PortalPlacedInsideBounds(CProp_Portal* pPortal)
{
	m_OnStartTouchPortal.FireOutput(pPortal, this);

	if (pPortal->IsPortal2())
		m_OnStartTouchPortal2.FireOutput(pPortal, this);
	else
		m_OnStartTouchPortal1.FireOutput(pPortal, this);

	if (pPortal->IsActivedAndLinked())
	{
		m_OnStartTouchLinkedPortal.FireOutput(pPortal, this);

		if (m_iTouchingPortalCount == 2)
		{
			m_OnStartTouchBothLinkedPortals.FireOutput(pPortal, this);
		}
	}
}

void CFuncPortalDetector::PortalRemovedFromInsideBounds(CProp_Portal* pPortal)
{
	m_OnEndTouchPortal.FireOutput(pPortal, this);

	// It's intersecting this portal
	if (pPortal->IsPortal2())
		m_OnEndTouchPortal2.FireOutput(pPortal, this);
	else
		m_OnEndTouchPortal1.FireOutput(pPortal, this);

	if (pPortal->IsActivedAndLinked())
	{
		m_OnEndTouchLinkedPortal.FireOutput(pPortal, this);

		if (m_iTouchingPortalCount == 0)
		{
			m_OnEndTouchBothLinkedPortals.FireOutput(pPortal, this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Test if the specified portal is already touching this detector
//-----------------------------------------------------------------------------
bool CFuncPortalDetector::IsPortalTouchingDetector(const CProp_Portal* pPortal)
{
	if (pPortal == NULL)
		return false;

	Assert(m_iTouchingPortalCount >= 0 && m_iTouchingPortalCount <= 2);

	for (int i = 0; i < 2; ++i)
	{
		const CProp_Portal* pCurPortal = dynamic_cast<const CProp_Portal*>(m_phTouchingPortals[i].Get());

		if (pCurPortal == pPortal)
		{
			return true;
		}
	}

	return false;
}