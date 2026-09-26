//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Displays the player's photo inventory
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "c_basehlplayer.h"

#include <vgui/ilocalize.h>
#include <vgui/isurface.h>
#include <vgui/ivgui.h>
#include "vgui_controls/animationcontroller.h"
#include <vgui_controls/editablepanel.h>

#include "aperture_render_targets.h"
#include "materialsystem/imaterialsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// FIXME: Needs to be shared or great disaster will befall us!
#define FLASH_INVENTORY_BASIC		0
#define FLASH_INVENTORY_FULL		1
#define FLASH_INVENTORY_ADDED		2
#define FLASH_INVENTORY_STRIPPED	3

ConVar cl_force_draw_photo_inventory("cl_force_draw_photo_inventory", "0", FCVAR_CHEAT);

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudPhotoInventory : public EditablePanel, public CHudElement
{
	DECLARE_CLASS_SIMPLE(CHudPhotoInventory, EditablePanel);

public:
	CHudPhotoInventory(const char *pElementName);

	void MsgFunc_InventoryFlash(bf_read &msg);
	void MsgFunc_TakePhoto(bf_read &msg);

protected:
	virtual void Reset(void);
	virtual void Init(void);
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint(void);
	virtual bool ShouldDraw(void);

	void PaintSlot(int nSlot, bool bExists, bool bSelected);
	void GetGlobalsStates(float &flScale, float &flAlpha);
	void UpdatePlacingTransition(void);

private:
	int		m_nPhotoTexture[3];
	int		m_nPhotoNumberTexture[3];
	float	m_flDisplayTime;
	float	m_flStartTime;
	float	m_flFadeInTime;
	float	m_flFadeOutTime;
	int		m_nFlashType;
	bool	m_bWasPlacing;

	int		m_nSelectedWidth;
	int		m_nSelectedHeight;
	int		m_nIdleWidth;
	int		m_nIdleHeight;

	bool	m_bLastState[3];	// The last state the photos we in, we need this for edge triggers on removing photos
};

DECLARE_HUDELEMENT_DEPTH(CHudPhotoInventory, 100);

DECLARE_HUD_MESSAGE(CHudPhotoInventory, InventoryFlash);
DECLARE_HUD_MESSAGE(CHudPhotoInventory, TakePhoto);

ConVar cl_camera_use_photos("cl_camera_use_photos", "1", 0, "Use snapshots of objects for spirit camera inventory views");


const float FADE_IN_DURATION = 0.5f;
const float FADE_OUT_DURATION = 0.25f;


PRECACHE_REGISTER_BEGIN(GLOBAL, PrecacheHudPhotoMaterials)
PRECACHE(MATERIAL, "photos/photo_background")
PRECACHE(MATERIAL, "photos/photo_foreground")
PRECACHE(MATERIAL, "HUD/inv_photo_numbers1")
PRECACHE(MATERIAL, "HUD/inv_photo_numbers2")
PRECACHE(MATERIAL, "HUD/inv_photo_numbers3")
PRECACHE(MATERIAL, "HUD/inv_photo1")
PRECACHE(MATERIAL, "HUD/inv_photo2")
PRECACHE(MATERIAL, "HUD/inv_photo3")
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudPhotoInventory::CHudPhotoInventory(const char *pElementName) : CHudElement(pElementName), BaseClass(NULL, "HudPhotoInventory")
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_PLAYERDEAD);

	LoadControlSettings("Resource/PhotoInventory.res");

	m_bWasPlacing = false;
	m_nFlashType = FLASH_INVENTORY_BASIC;
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudPhotoInventory::Init(void)
{
	static bool bHook = true;
	if (bHook) //avoid repeatedly hooking the same message (so we don't process it a bunch)
	{
		HOOK_HUD_MESSAGE(CHudPhotoInventory, InventoryFlash);
		HOOK_HUD_MESSAGE(CHudPhotoInventory, TakePhoto);
		bHook = false;
	}

	//numbers
	m_nPhotoNumberTexture[0] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoNumberTexture[0], "HUD/inv_photo_numbers1", true, false);

	m_nPhotoNumberTexture[1] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoNumberTexture[1], "HUD/inv_photo_numbers2", true, false);

	m_nPhotoNumberTexture[2] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoNumberTexture[2], "HUD/inv_photo_numbers3", true, false);

	//snapshots
	m_nPhotoTexture[0] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoTexture[0], "HUD/inv_photo1", true, false);

	m_nPhotoTexture[1] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoTexture[1], "HUD/inv_photo2", true, false);

	m_nPhotoTexture[2] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_nPhotoTexture[2], "HUD/inv_photo3", true, false);

	m_bWasPlacing = false;
	m_nFlashType = FLASH_INVENTORY_BASIC;

	m_flDisplayTime = 0.0f;
	m_flStartTime = 0.0f;
	m_flFadeInTime = 0.0f;
	m_flFadeOutTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudPhotoInventory::Reset(void)
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudPhotoInventory::ApplySchemeSettings(vgui::IScheme *scheme)
{
	LoadControlSettings("Resource/PhotoInventory.res");
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
	// SetPaintBackgroundType( 2 );

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);

	m_nSelectedWidth = screenWide / 22;
	m_nSelectedHeight = screenWide / 22;
	m_nIdleWidth = screenWide / 26;
	m_nIdleHeight = screenWide / 26;

	const int nPadding = (m_nSelectedHeight / 3); // Upper and lower padding

	int nLowerY = screenTall - (m_nSelectedHeight * 2 + (nPadding * 2));

	// We want to take over the lower half of the screen, roughly
	SetBounds(0, nLowerY, screenWide, screenTall - nLowerY);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudPhotoInventory::ShouldDraw(void)
{
	if (!cl_force_draw_photo_inventory.GetBool())
		return false; // FIXME: This hud element is redundant to the photo being carried 

	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>(C_BasePlayer::GetLocalPlayer());
	if (pPlayer == NULL)
		return false;

	// Must have something in our inventory!
	if (m_flDisplayTime < gpGlobals->curtime /*&&
		 pPlayer->m_HL2Local.m_bHasPhotoInInventory[0] == false &&
		 pPlayer->m_HL2Local.m_bHasPhotoInInventory[1] == false &&
		 pPlayer->m_HL2Local.m_bHasPhotoInInventory[2] == false*/)
		return false;

	// We're being told to display
#if 0
	if (m_flDisplayTime > (gpGlobals->curtime + 0.1f)) // FIXME: Huh?
	{
		return true;
	}
	else
	{
		m_nFlashType = FLASH_INVENTORY_BASIC;
	}

	// FIXME: We don't need to be out if the placement mode isn't active at this point
	if (pPlayer->m_HL2Local.m_bPlacingPhoto)
		return true;
#endif // 0

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Setup our long fades above all other animations
//-----------------------------------------------------------------------------
void CHudPhotoInventory::GetGlobalsStates(float &flScale, float &flAlpha)
{
	flScale = 1.0f;
	flAlpha = 1.0f;

	if (m_flDisplayTime >= gpGlobals->curtime)
	{
		if (gpGlobals->curtime <= m_flFadeInTime)
		{
			flScale = flAlpha = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeInTime, 0.0f, 1.0f);
		}
		else if (gpGlobals->curtime >= m_flFadeOutTime)
		{
			flAlpha = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flFadeOutTime, m_flDisplayTime, 1.0f, 0.0f);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudPhotoInventory::PaintSlot(int nSlot, bool bExists, bool bSelected)
{
	float flGlobalScale;
	float flGlobalAlpha;
	GetGlobalsStates(flGlobalScale, flGlobalAlpha);

	float flAnimScale = 0.0f;
	float flAnimOffsetX = 0.0f;
	float flAnimOffsetY = 0.0f;

	switch (m_nFlashType)
	{
	default:
	case FLASH_INVENTORY_BASIC:
		// NOTE: Do nothing
		break;

	case FLASH_INVENTORY_ADDED:
	{
		if (bSelected == false)
		{
			flGlobalScale = 1.0f;
		}
	}
	break;

	case FLASH_INVENTORY_FULL:
	{
		float flScale = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeOutTime, 1.0f, 0.0f);
		flAnimScale = sinf(gpGlobals->curtime * 2.0f) + sinf((gpGlobals->curtime * 10.0f) + nSlot) * cosf(gpGlobals->curtime * 2.0f) * 24.0f * flScale;
		flAnimOffsetX = flAnimOffsetY = sinf(gpGlobals->curtime * 4.0f) + sinf((gpGlobals->curtime * 15.0f) + nSlot) * cosf(gpGlobals->curtime) * 8.0f * flScale;
		flGlobalScale = 1.0f;
	}
	break;

	case FLASH_INVENTORY_STRIPPED:
	{
		float flScale = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeOutTime - ((m_flFadeOutTime - m_flStartTime) * 0.5), 1.0f, 0.0f);
		flAnimOffsetX = sinf(gpGlobals->curtime * (24.0f * flScale)) * 32.0f * flScale;
		flAnimOffsetY = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeOutTime, 0.0f, 48.0f);
		flGlobalScale = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeOutTime, 1.0f, 0.75f);
		flGlobalAlpha = SimpleSplineRemapValClamped(gpGlobals->curtime, m_flStartTime, m_flFadeOutTime, 1.0f, 0.0f);
	}
	break;
	}

	// Figure out our scale
	// float flScale = ( bSelected ) ? 64.0f : 48.0f;
	float flScale = (bSelected) ? m_nSelectedWidth : m_nIdleWidth;
	flScale += flAnimScale;
	flScale *= flGlobalScale;

	// Center
	float xLeft = (GetWide() / 2) - (flScale + (flScale / 2));
	float xRight = (GetWide() / 2) + (flScale + (flScale / 2));
	float yTop = (GetTall() / 2) - flScale;
	float yBottom = (GetTall() / 2) + flScale;

	flAnimOffsetX *= flGlobalScale;
	flAnimOffsetY *= flGlobalScale;

	// Offsets
	xLeft += flAnimOffsetX;
	xRight += flAnimOffsetX;
	yTop += flAnimOffsetY;
	yBottom += flAnimOffsetY;

	switch (nSlot)
	{
	case 0:
	{
		xLeft -= m_nSelectedWidth * 3;
		xRight -= m_nSelectedWidth * 3;
	}
	break;

	case 2:
	{
		xLeft += m_nSelectedWidth * 3;
		xRight += m_nSelectedWidth * 3;
	}
	break;
	}

	// Bleh
	if (xLeft > xRight)
	{
		float temp_oldleft;
		float temp_oldright;
		temp_oldleft = xLeft;
		temp_oldright = xRight;
		xLeft = temp_oldright;
		xRight = temp_oldleft;
	}

	if (bExists)
	{
		const int nDropHeight = 8;
		if (bSelected)
		{
			surface()->DrawSetColor(0, 0, 0, 128 * flGlobalAlpha);
		}
		else
		{
			surface()->DrawSetColor(0, 0, 0, 16 * flGlobalAlpha);
		}

		surface()->DrawFilledRect(xLeft + nDropHeight, yTop + nDropHeight, xRight + nDropHeight, yBottom + nDropHeight);

		bool bUseCameraPhotos = cl_camera_use_photos.GetBool();

		if (bSelected)
		{
			surface()->DrawSetColor(255, 255, 255, 255 * flGlobalAlpha);
		}
		else
		{
			surface()->DrawSetColor(128, 128, 128, 128 * flGlobalAlpha);
		}

		if (bUseCameraPhotos)
		{
			surface()->DrawSetTexture(m_nPhotoTexture[nSlot]);
		}
		else
		{
			surface()->DrawSetTexture(m_nPhotoNumberTexture[nSlot]);
		}

		Vertex_t vert[4];
		vert[0].Init(Vector2D(xLeft, yTop), Vector2D(0, 0));
		vert[1].Init(Vector2D(xRight, yTop), Vector2D(1, 0));
		vert[2].Init(Vector2D(xRight, yBottom), Vector2D(1, 1));
		vert[3].Init(Vector2D(xLeft, yBottom), Vector2D(0, 1));
		surface()->DrawTexturedPolygon(4, vert);

		if (bUseCameraPhotos)
		{
			surface()->DrawSetTexture(m_nPhotoNumberTexture[nSlot]);
			if (bSelected)
			{
				surface()->DrawSetColor(255, 255, 255, 255 * flGlobalAlpha);
			}
			else
			{
				surface()->DrawSetColor(255, 255, 255, 128 * flGlobalAlpha);
			}
			surface()->DrawTexturedPolygon(4, vert);
		}
	}
	else
	{
		surface()->DrawSetColor(0, 0, 0, 64);
		surface()->DrawFilledRect(xLeft, yTop, xRight, yBottom);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the transition from placing to not
//-----------------------------------------------------------------------------
void CHudPhotoInventory::UpdatePlacingTransition(void)
{
	switch (m_nFlashType)
	{
	case FLASH_INVENTORY_FULL:
		if (m_bWasPlacing)
		{
			// Don't fade in, but fade out
			m_flFadeInTime = 0.0f;
			m_flFadeOutTime = m_flDisplayTime - 0.25f;
		}
		else
		{
			// Fade in, don't fade out
			m_flFadeOutTime = m_flDisplayTime;
		}
		break;

	case FLASH_INVENTORY_ADDED:
		if (m_bWasPlacing)
		{
			m_flFadeInTime = 0.0f;
			m_flFadeOutTime = m_flDisplayTime - FADE_OUT_DURATION;
		}
		else
		{
			// Fade in, don't fade out
			m_flFadeOutTime = m_flDisplayTime;
		}
		break;

	default:
		if (m_bWasPlacing)
		{
			// No longer placing, so fade out
			m_flDisplayTime = gpGlobals->curtime + FADE_OUT_DURATION;
			m_flFadeInTime = 0.0f;
			m_flFadeOutTime = gpGlobals->curtime;
			m_flStartTime = gpGlobals->curtime;
		}
		else
		{
			m_flDisplayTime = gpGlobals->curtime + FADE_IN_DURATION;
			m_flFadeInTime = gpGlobals->curtime + FADE_IN_DURATION;
			m_flStartTime = gpGlobals->curtime;
			m_flFadeOutTime = 0.0f;
		}

		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudPhotoInventory::Paint(void)
{
	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (pPlayer == NULL)
		return;

	// Catch transitions to and from placing photographs
	if (m_bWasPlacing != pPlayer->m_HL2Local.m_bPlacingPhoto)
	{
		// Reset our state
		UpdatePlacingTransition();
		m_bWasPlacing = pPlayer->m_HL2Local.m_bPlacingPhoto;
	}

	// If the inventory isn't up, display all the slots equally
	if ((m_flDisplayTime > gpGlobals->curtime) && m_nFlashType == FLASH_INVENTORY_FULL && pPlayer->m_HL2Local.m_bPlacingPhoto == false)
	{
		// PaintSlot( 0, pPlayer->m_HL2Local.m_bHasPhotoInInventory[0], false );
		// PaintSlot( 1, pPlayer->m_HL2Local.m_bHasPhotoInInventory[1], false );
		// PaintSlot( 2, pPlayer->m_HL2Local.m_bHasPhotoInInventory[2], false );
	}
	else if ((m_flDisplayTime > gpGlobals->curtime) && m_nFlashType == FLASH_INVENTORY_STRIPPED)
	{
		// Draw all the slots
		PaintSlot(0, m_bLastState[0], false);
		PaintSlot(1, m_bLastState[1], false);
		PaintSlot(2, m_bLastState[2], false);
	}
	else
	{
		// Draw all the slots
		// PaintSlot( 0, pPlayer->m_HL2Local.m_bHasPhotoInInventory[0], (pPlayer->m_HL2Local.m_nSelectedPhoto == 0) );
		// PaintSlot( 1, pPlayer->m_HL2Local.m_bHasPhotoInInventory[1], (pPlayer->m_HL2Local.m_nSelectedPhoto == 1) );
		// PaintSlot( 2, pPlayer->m_HL2Local.m_bHasPhotoInInventory[2], (pPlayer->m_HL2Local.m_nSelectedPhoto == 2) );
	}

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPhotoInventory::MsgFunc_InventoryFlash(bf_read &msg)
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (pPlayer == NULL)
		return;

	// Basic info
	m_flStartTime = gpGlobals->curtime;
	m_flDisplayTime = gpGlobals->curtime + msg.ReadFloat();
	m_nFlashType = msg.ReadByte();

	switch (m_nFlashType)
	{
	case FLASH_INVENTORY_FULL:
		m_flFadeInTime = gpGlobals->curtime + 0.1f;
		m_flFadeOutTime = m_flDisplayTime - 0.25f;
		break;

	case FLASH_INVENTORY_ADDED:
		m_flFadeInTime = gpGlobals->curtime + FADE_IN_DURATION;
		m_flFadeOutTime = m_flDisplayTime - FADE_OUT_DURATION;
		break;

	case FLASH_INVENTORY_STRIPPED:
		// Save off the last state here
		m_bLastState[0] = msg.ReadByte();
		m_bLastState[1] = msg.ReadByte();
		m_bLastState[2] = msg.ReadByte();

		m_flFadeInTime = gpGlobals->curtime + 0.1f;
		m_flFadeOutTime = m_flDisplayTime - 0.5f;

		break;

	default:
		m_flFadeInTime = gpGlobals->curtime + FADE_IN_DURATION;
		m_flFadeOutTime = m_flDisplayTime - 0.25f;
		break;
	}

	// Don't fade if we're placing a picture
	if (pPlayer->m_HL2Local.m_bPlacingPhoto && m_bWasPlacing)
	{
		m_flFadeOutTime = m_flDisplayTime;
	}
	else
	{
		m_bWasPlacing = false;
	}
}

void Aperture_QueuePhotoView(EHANDLE hPhotoEntity, ITexture *pRenderTarget); //lives in viewrender.cpp

void CHudPhotoInventory::MsgFunc_TakePhoto(bf_read &msg)
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (pPlayer == NULL)
		return;

	//grab photo entity EHANDLE
	long iEncodedEHandle;
	int iEntity, iSerialNum;
	iEncodedEHandle = msg.ReadLong();

	if (iEncodedEHandle == INVALID_NETWORKED_EHANDLE_VALUE)
		return;

	iEntity = iEncodedEHandle & ((1 << MAX_EDICT_BITS) - 1);
	iSerialNum = iEncodedEHandle >> MAX_EDICT_BITS;

	EHANDLE hPhotoEntity(iEntity, iSerialNum);

	int iSlot = msg.ReadByte();

	//copy to texture
	//CMatRenderContextPtr pRenderContext( materials );
	ITexture *pPhotoTexture = aperturerendertargets->GetLargePhotoRenderTarget(iSlot);
	Assert(pPhotoTexture);
	if (pPhotoTexture)
	{
		Aperture_QueuePhotoView(hPhotoEntity, pPhotoTexture);
	}
}
