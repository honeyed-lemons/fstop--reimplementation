//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photo placement tool
//
//=====================================================================================//

#include "cbase.h"
#include "player.h"
#include "basecombatweapon.h"
#include "ai_basenpc.h"
#include "portal_player.h"
#include "in_buttons.h"
#include "weapon_camera.h"
#include "particle_parse.h"

#include "props.h"

#include "photo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Vector g_placedPosition = vec3_invalid;
CBaseEntity *g_placedEntity = NULL;

ConVar camera_preview_transluceny("camera_preview_transluceny", "255");
ConVar camera_reverse_scaling_direction("camera_reverse_scaling_direction", "1");

extern ConVar sv_camera_use_legacy_capture;

//-----------------------------------------------------------------------------
// Photo preview model
//-----------------------------------------------------------------------------

class CPhotoPreview : public CBaseAnimating
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS(CPhotoPreview, CBaseAnimating);

	CPhotoPreview(void) : m_flSourceScale(1.0f) {}

	static CPhotoPreview *CreatePhotoPreview(const char *lpszModelName, const Vector &vecTargetPos, const QAngle &vecBaseAngles, CBasePlayer *pOwner)
	{
		CPhotoPreview *pPreview = (CPhotoPreview *)CreateEntityByName("photopreview");
		if (pPreview == NULL)
			return NULL;

		pPreview->SetModelName(MAKE_STRING(lpszModelName));
		pPreview->SetAbsOrigin(vecTargetPos);
		DispatchSpawn(pPreview);

		// Save this for later
		pPreview->SetTargetPosition(vecTargetPos, vecBaseAngles);
		pPreview->m_hOwner = pOwner;

		pPreview->SetEffects( /*EF_NOSHADOW |*/ EF_NORECEIVESHADOW);
		pPreview->SetShadowCastDistance((100 * 12));

		// Think once to get the ball rolling
		pPreview->MoveThink();

		return pPreview;
	}

	void SetTargetPosition(const Vector &vecNewTarget, const QAngle &vecNewAngles)
	{
		m_vecTargetPos = vecNewTarget;
		m_vecTargetAngles = vecNewAngles;
	}

	void Spawn(void)
	{
		SetModel(STRING(GetModelName()));
		SetSolid(SOLID_NONE);
		SetSolidFlags(FSOLID_NOT_SOLID);
		SetRenderColor(0, 0, 0);
		SetRenderAlpha(64);
		SetRenderMode(kRenderTransColor);

		m_flStartScaleTime = 0.0f;
		m_flTargetScale = 1.0f;
	}

	static void ComputePlayerMatrix(CBasePlayer *pPlayer, matrix3x4_t &out)
	{
		if (pPlayer == NULL)
			return;

		QAngle angles = pPlayer->EyeAngles();
		Vector origin = pPlayer->EyePosition();
		AngleMatrix(angles, origin, out);
	}

	void MoveThink(void)
	{
		// Set our new position and angles
		SetAbsOrigin(m_vecTargetPos);
		SetAbsAngles(m_vecTargetAngles);

		// Scale!
		float flScale = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartScaleTime, m_flStartScaleTime + 0.25f, m_flSourceScale, m_flTargetScale);
		SetModelScale(flScale);

		// Setup the next think
		SetThink(&CPhotoPreview::MoveThink);
		SetNextThink(gpGlobals->curtime);
	}

	void ShowAsValid(bool bState)
	{
		if (bState)
		{
			SetRenderColor(255, 255, 255);
			SetRenderAlpha(camera_preview_transluceny.GetInt());
			RemoveEffects(EF_ITEM_BLINK);
		}
		else
		{
			SetRenderColor(255, 0, 0);
			SetRenderAlpha(camera_preview_transluceny.GetInt() / 2);
			AddEffects(EF_ITEM_BLINK);
		}
	}

	void SetObjectScale(float flScale)
	{
		m_flTargetScale = flScale;
		m_flSourceScale = GetModelScale();
		m_flStartScaleTime = gpGlobals->curtime;
	}

private:
	Vector	m_vecTargetPos;		// Target position this object is moving towards
	QAngle	m_vecTargetAngles;	// Base angles (relative to the player's viewpoint)
	CHandle<CBasePlayer> m_hOwner;		// Who owns us

	CNetworkVar(float, m_flStartScaleTime);	// Time we began scaling up
	CNetworkVar(float, m_flTargetScale);	// Final scale size

	float	m_flSourceScale;	// Scale we began at when a size change occured
};

LINK_ENTITY_TO_CLASS(photopreview, CPhotoPreview);

BEGIN_DATADESC(CPhotoPreview)

DEFINE_THINKFUNC(MoveThink),

DEFINE_FIELD(m_vecTargetPos, FIELD_VECTOR),
DEFINE_FIELD(m_vecTargetAngles, FIELD_VECTOR),
DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
DEFINE_FIELD(m_flStartScaleTime, FIELD_TIME),
DEFINE_FIELD(m_flTargetScale, FIELD_FLOAT),
DEFINE_FIELD(m_flSourceScale, FIELD_FLOAT),

END_DATADESC()

//-----------------------------------------------------------------------------
// CWeaponPlacement
//-----------------------------------------------------------------------------

class CWeaponPlacement : public CBaseCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS(CWeaponPlacement, CBaseCombatWeapon);

	CWeaponPlacement(void);
	~CWeaponPlacement(void);

	// DECLARE_SERVERCLASS();

	virtual void	Precache(void);
	virtual void	OnRestore(void);
	virtual void	PrimaryAttack(void);
	virtual void	SecondaryAttack(void);
	virtual void	ItemPostFrame(void);
	virtual bool	Deploy(void);
	virtual bool	Holster(CBaseCombatWeapon *pSwitchingTo);
	virtual bool	GetPlacementPosition(Vector *pOriginOut, QAngle *pAnglesOut, CInfoPlacementHelper **pHelperOut = NULL);
	virtual void	OnMouseWheel(int nDirection);
	virtual bool	Reload(void);
	virtual void	WeaponIdle(void);

	virtual int	CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual int	GetMinBurst() { return 1; }
	virtual int	GetMaxBurst() { return 1; }
	virtual float GetFireRate(void) { return 3.0f; }
	virtual Activity GetPrimaryAttackActivity(void) { return ACT_VM_PRIMARYATTACK; }
	virtual Activity GetDrawActivity(void);

	virtual bool HasAnyAmmo(void) { return (Photo_Count() > 0); }

	DECLARE_ACTTABLE();

private:

	float	GetObjectScale(const CaptureInfo_t &captureInfo);

	// Aesthetics
	void	ReleaseEffect(const Vector &vecPosition);

	void	NotifyCompanions(void);

	// Object handling
	CBaseEntity	*ReleaseObject(const CaptureInfo_t &captureInfo, const Vector &vecPoint, const QAngle &vecAngles, CInfoPlacementHelper *pHelper);

	void	UpdateActiveItem(void);
	void	DestroyPhotoPreview(void);
	void	PlacePhoto(void);
	void	CancelPlacement(void);

	CHandle<CPhotoPreview>	m_hPhotoPreview;
	CaptureInfo_t			m_CaptureInfo;
	int						m_nObjectScaleLevel;
	Vector 					m_vecObjectSize;
	Vector					m_vecObjectOffset;
	bool					m_bObjectIsNPC;
	bool					m_bInPlacementMode;
};

// IMPLEMENT_SERVERCLASS_ST( CWeaponPlacement, DT_WeaponCaptivas )
// END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_placement, CWeaponPlacement);
PRECACHE_WEAPON_REGISTER(weapon_placement);

BEGIN_DATADESC(CWeaponPlacement)

DEFINE_FIELD(m_hPhotoPreview, FIELD_EHANDLE),
DEFINE_FIELD(m_nObjectScaleLevel, FIELD_INTEGER),
DEFINE_FIELD(m_vecObjectSize, FIELD_VECTOR),
DEFINE_FIELD(m_vecObjectOffset, FIELD_VECTOR),
DEFINE_FIELD(m_bObjectIsNPC, FIELD_BOOLEAN),

DEFINE_EMBEDDED(m_CaptureInfo),

END_DATADESC()

acttable_t	CWeaponPlacement::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};


IMPLEMENT_ACTTABLE(CWeaponPlacement);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPlacement::CWeaponPlacement(void) : m_vecObjectSize(vec3_origin)
{
	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bFiresUnderwater = true;
	m_bInPlacementMode = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponPlacement::~CWeaponPlacement(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlacement::Precache(void)
{
	PrecacheScriptSound("Weapon_Camera.Release");

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponPlacement::GetObjectScale(const CaptureInfo_t &captureInfo)
{
	// Call through our normal means
	float flObjectScale = 1.0f;
	if (captureInfo.pPlacementQuery)
	{
		flObjectScale = captureInfo.pPlacementQuery->GetScaleForStep(m_nObjectScaleLevel, &captureInfo);
	}

	return flObjectScale;
}

//-----------------------------------------------------------------------------
// Purpose: Replace the object in the world
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponPlacement::ReleaseObject(const CaptureInfo_t &captureInfo, const Vector &vecPoint, const QAngle &vecAngles, CInfoPlacementHelper *pHelper)
{
	CBaseEntity *pNewObject = UTIL_RestoreCapturedObject(captureInfo, vecPoint, vecAngles, m_nObjectScaleLevel, pHelper);
	if (pNewObject == NULL)
		return NULL;

	return pNewObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlacement::DestroyPhotoPreview(void)
{
	if (m_hPhotoPreview)
	{
		UTIL_Remove(m_hPhotoPreview);
		m_hPhotoPreview = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlacement::UpdateActiveItem(void)
{
	// Must have a photo to restore
	unsigned int nNumPhotos = Photo_Count();
	if (nNumPhotos == 0)
	{
		DestroyPhotoPreview();
		return;
	}

	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetLocalPlayer();
	if (pPlayer == NULL)
		return;

	int nIndex = pPlayer->GetSelectedPhoto();
	if (Photo_Get(nIndex, &m_CaptureInfo) == false)
	{
		DestroyPhotoPreview();
		return;
	}

	m_vecObjectSize = m_CaptureInfo.hCapturedEnt->CollisionProp()->OBBSize();//WorldAlignSize();
	m_vecObjectOffset = m_CaptureInfo.hCapturedEnt->WorldSpaceCenter();

	if (m_hPhotoPreview)
	{
		m_hPhotoPreview->SetModel(STRING(m_CaptureInfo.hCapturedEnt->GetModelName()));
	}
	else
	{
		// Create the preview
		Vector vecTargetPos;
		QAngle vecTargetAngles;
		GetPlacementPosition(&vecTargetPos, &vecTargetAngles);
		m_hPhotoPreview = CPhotoPreview::CreatePhotoPreview(STRING(m_CaptureInfo.hCapturedEnt->GetModelName()), vecTargetPos, vecTargetAngles, ToBasePlayer(GetOwner()));
		if (m_hPhotoPreview == NULL)
			return;
	}

	// Restore the scale of the object as we captured it
	m_nObjectScaleLevel = UTIL_GetEntityScaleLevel(m_CaptureInfo.hCapturedEnt.Get());
	m_hPhotoPreview->SetObjectScale(GetObjectScale(m_CaptureInfo));
}

//-----------------------------------------------------------------------------
// Purpose: Flash effect and noise
//-----------------------------------------------------------------------------
void CWeaponPlacement::ReleaseEffect(const Vector &vecPosition)
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		pPlayer->Flash(0.5f, vecPosition);
	}

	EmitSound("Weapon_Camera.Release");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlacement::NotifyCompanions(void)
{

}

//-----------------------------------------------------------------------------
// Purpose: Capture an object
//-----------------------------------------------------------------------------
void CWeaponPlacement::PlacePhoto(void)
{
	CPortal_Player *pPlayer = (CPortal_Player *)ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	// Must have a photo to restore
	unsigned int nNumPhotos = Photo_Count();
	if (nNumPhotos == 0)
		return;

	Vector vecTargetPos;
	QAngle vecTargetAngles;
	CInfoPlacementHelper *pHelper = NULL;
	if (GetPlacementPosition(&vecTargetPos, &vecTargetAngles, &pHelper) == false)
	{
		pPlayer->FlashDenyIndicator(1.0f, FLASH_INDICATOR_INVALID);
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
		return;
	}

	// Attempt to put it back
	CBaseEntity *pNewObject = ReleaseObject(m_CaptureInfo, vecTargetPos, vecTargetAngles, pHelper);
	if (pNewObject)
	{
		ReleaseEffect(pNewObject->WorldSpaceCenter());

		int nIndex = pPlayer->GetSelectedPhoto();
		Photo_Remove(nIndex);
	}

	// Alert our companions that we just replaced an object
	NotifyCompanions();

	// Switch weapons if we can
	if (pPlayer->SwitchToNextBestWeapon(this) == false)
	{
		if (Photo_Count() == 0)
		{
			// Put the weapon away
			Holster(NULL);

			// Clear capture info
			Q_memset(&(m_CaptureInfo), NULL, sizeof(CaptureInfo_t));
		}
		else
		{
			// Display something different now
			UpdateActiveItem();
		}
	}

	pPlayer->ControlHelperAnimate(CONTROL_STATE_NEUTRAL, true);
}

//-----------------------------------------------------------------------------
// Purpose: Capture an object
//-----------------------------------------------------------------------------
void CWeaponPlacement::CancelPlacement(void)
{
	CPortal_Player *pPlayer = (CPortal_Player *)ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	pPlayer->ControlHelperAnimate(CONTROL_STATE_NEUTRAL);
	pPlayer->SwitchToNextBestWeapon(this);
}

//-----------------------------------------------------------------------------
// Purpose: Capture an object
//-----------------------------------------------------------------------------
void CWeaponPlacement::PrimaryAttack(void)
{
	if (m_bInPlacementMode)
	{
		PlacePhoto();
	}
	else
	{
		// Go into the placement mode
		UpdateActiveItem();
		m_bInPlacementMode = true;

		// We need to reset the idle to make it change here
		SendWeaponAnim(ACT_VM_IDLE);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlacement::SecondaryAttack(void)
{
	// Cancel the placement and return to the neutral mode
	DestroyPhotoPreview();
	m_bInPlacementMode = false;
}

//like CTraceFilterSimple, but without the collision group check
class CTraceSkipEntity : public CTraceFilter
{
public:
	CTraceSkipEntity(IHandleEntity *pEntity) : m_PassEntity(pEntity) { };
	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		if (pHandleEntity == m_PassEntity)
			return false;

		return true;
	}

protected:
	IHandleEntity *m_PassEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPlacement::GetPlacementPosition(Vector *pOriginOut, QAngle *pAnglesOut, CInfoPlacementHelper **pHelperOut)
{
	if (!m_CaptureInfo.pPlacementQuery)
	{
		if (m_CaptureInfo.hCapturedEnt)
		{
			Assert(!"This should have been fixed up at restore! Investigate.");

			CBaseAnimating* pAni = dynamic_cast<CBaseAnimating*>(m_CaptureInfo.hCapturedEnt.Get());
			if (pAni)
			{
				m_CaptureInfo.pPlacementQuery = pAni->Get_CPhotoPlacementQuery();
			}
		}

		if (!m_CaptureInfo.pPlacementQuery)
		{
			Assert(0);
			return false;
		}
	}

	// Trace ahead and try to find somewhere to place this
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	Vector vecViewPos = pPlayer->EyePosition();
	Vector vecViewDir = pPlayer->EyeDirection3D();
	QAngle qAngles = pPlayer->EyeAngles();

	CTraceSkipEntity ownerFilter(GetOwner());

	Vector vRetPosition;
	QAngle qRetAngles;

	bool bSuccess = m_CaptureInfo.pPlacementQuery->CheckPlacement(m_CaptureInfo, m_nObjectScaleLevel, vecViewPos, vecViewDir, qAngles, vRetPosition, qRetAngles, pHelperOut, &ownerFilter);

	if (pOriginOut)
		*pOriginOut = vRetPosition;

	if (pAnglesOut)
		*pAnglesOut = qRetAngles;

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPlacement::Deploy(void)
{
	if (Photo_Count() == 0)
		return false;

	bool bDeployed = BaseClass::Deploy();
	if (bDeployed)
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());
		if (pOwner)
		{
			pOwner->SetNextAttack(gpGlobals->curtime);
		}
	}

	// If we're in this mode, we don't start placement right away
	m_bInPlacementMode = false;
	return bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPlacement::Holster(CBaseCombatWeapon *pSwitchingTo)
{
	DestroyPhotoPreview();

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Display and update the photo placement preview item (move to the client!)
//-----------------------------------------------------------------------------
void CWeaponPlacement::ItemPostFrame(void)
{
	// FIXME: Break out the bits we need so that we can override the button handling, but allow the base class
	//			to do other work that may be important to it -- jdw

	// BaseClass::ItemPostFrame();

	CPortal_Player *pOwner = (CPortal_Player *)ToBasePlayer(GetOwner());
	if (pOwner == NULL)
		return;

	bool bWeaponActed = false;
	if (m_flNextPrimaryAttack < gpGlobals->curtime)
	{
		// Override how we deal with our buttons and only act on the initial edge trigger (not held buttons)
		if ((pOwner->m_afButtonPressed & IN_ATTACK) && (pOwner->m_afButtonLast & IN_ATTACK) == false)
		{
			PrimaryAttack();
			bWeaponActed = true;
		}
	}

	if (m_flNextSecondaryAttack < gpGlobals->curtime)
	{
		if ((pOwner->m_afButtonPressed & IN_ATTACK2) && (pOwner->m_afButtonLast & IN_ATTACK2) == false)
		{
			SecondaryAttack();
			bWeaponActed = true;
		}
	}

	// Do nothing
	if (bWeaponActed == false)
	{
		WeaponIdle();
	}

	CPortal_Player *pPlayer = (CPortal_Player *)ToBasePlayer(GetOwner());
	if (pPlayer)
	{
		pPlayer->SetPlacingPhoto(m_bInPlacementMode);
	}

	// At this point, the primary attacks may have removed our photo preview
	if (m_hPhotoPreview == NULL)
		return;

	if (!m_CaptureInfo.pPlacementQuery)
	{
		Assert(0);
		return;
	}

	Vector vecTargetPos;
	QAngle vecTargetAngles;
	bool bPlaceable = GetPlacementPosition(&vecTargetPos, &vecTargetAngles);

	m_hPhotoPreview->SetTargetPosition(vecTargetPos, vecTargetAngles);
	m_hPhotoPreview->ShowAsValid(bPlaceable);

	int nLowestLevel = -m_CaptureInfo.pPlacementQuery->GetNumScaleDownSteps(&m_CaptureInfo);
	int nHighestLevel = m_CaptureInfo.pPlacementQuery->GetNumScaleUpSteps(&m_CaptureInfo);
	float flLowScale = m_CaptureInfo.pPlacementQuery->GetScaleForStep(nLowestLevel, &m_CaptureInfo);
	float flHighScale = m_CaptureInfo.pPlacementQuery->GetScaleForStep(nHighestLevel, &m_CaptureInfo);
	float flScale = RemapValClamped(m_hPhotoPreview->GetModelScale(), flLowScale, flHighScale, 1.0f, 0.0f);
	flScale = Bias(flScale, 0.05f);

	CBaseViewModel *vm = ToBasePlayer(GetOwner())->GetViewModel();
	if (vm != NULL)
	{
		vm->SetPoseParameter("photo_scale", flScale);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Mouse wheelin'
//-----------------------------------------------------------------------------
void CWeaponPlacement::OnMouseWheel(int nDirection)
{
	// Ignore the message if we're not placing
	if (m_bInPlacementMode == false)
		return;

	CPortal_Player *pPlayer = (CPortal_Player *)ToBasePlayer(GetOwner());
	Assert(pPlayer);
	if (pPlayer)
	{
		CWeaponCamera* pCamera = dynamic_cast<CWeaponCamera*> (pPlayer->Weapon_OwnsThisType("weapon_camera"));

		// if they have a weapon camera, it may restrict their ability to scale objects.
		if (pCamera)
		{
			if (!pCamera->CanScaleCapturedObjects())
				return;
		}
	}

	if (!m_CaptureInfo.hCapturedEnt.Get() || !m_CaptureInfo.pPlacementQuery)
	{
		Assert(0);
		return;
	}

	// See if we need to swap the scaling direction
	if (camera_reverse_scaling_direction.GetBool())
	{
		nDirection *= -1;
	}

	if (nDirection == MWHEEL_UP)
	{
		if (m_nObjectScaleLevel + 1 <= m_CaptureInfo.pPlacementQuery->GetNumScaleUpSteps(&m_CaptureInfo))
			m_nObjectScaleLevel++;
	}
	else if (nDirection == MWHEEL_DOWN)
	{
		if (m_nObjectScaleLevel - 1 >= -(m_CaptureInfo.pPlacementQuery->GetNumScaleDownSteps(&m_CaptureInfo)))
			m_nObjectScaleLevel--;
	}

	// Publish this back to the capture info so that we can cycle through objects and make them retain their sizes
	m_CaptureInfo.nPreviewScaleLevel = m_nObjectScaleLevel;

	int nIndex = pPlayer->GetSelectedPhoto();
	Photo_Update(nIndex, m_CaptureInfo);

	if (m_hPhotoPreview)
	{
		m_hPhotoPreview->SetObjectScale(GetObjectScale(m_CaptureInfo));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Choose the draw animation
//-----------------------------------------------------------------------------
Activity CWeaponPlacement::GetDrawActivity(void)
{
	/*
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer->HasNamedPlayerItem( "weapon_camera" ) == false )
		return ACT_VM_DEPLOY;
	*/

	return BaseClass::GetDrawActivity();
}

//-----------------------------------------------------------------------------
// Purpose: Choose our idle animation
//-----------------------------------------------------------------------------
void CWeaponPlacement::WeaponIdle(void)
{
	Activity nIdleActivity = (m_bInPlacementMode) ? ACT_VM_IDLE : ACT_VM_IDLE_LOWERED;

	//Idle again if we've finished
	if (HasWeaponIdleTimeElapsed())
	{
		SendWeaponAnim(nIdleActivity);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Reload!
//-----------------------------------------------------------------------------
bool CWeaponPlacement::Reload(void)
{
	SendWeaponAnim(ACT_VM_RELOAD);
	UpdateActiveItem();
	return true;
}

void CWeaponPlacement::OnRestore(void)
{
	// Fix up placement query
	if (m_CaptureInfo.hCapturedEnt)
	{
		CBaseAnimating* pEntAnimating = dynamic_cast<CBaseAnimating*>(m_CaptureInfo.hCapturedEnt.Get());
		Assert(pEntAnimating);
		if (pEntAnimating)
		{
			m_CaptureInfo.pPlacementQuery = pEntAnimating->Get_CPhotoPlacementQuery();
			Assert(m_CaptureInfo.pPlacementQuery);
		}
	}
}