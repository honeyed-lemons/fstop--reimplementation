//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Displays the camera's flash
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
#include "view_scene.h"

#include <vgui/ilocalize.h>
#include <vgui/isurface.h>
#include <vgui/ivgui.h>
#include "vgui_controls/animationcontroller.h"
#include <vgui_controls/editablepanel.h>

#include "materialsystem/imaterialsystem.h"

#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// FIXME: Needs to be shared or great disaster will befall us!
#define FLASH_SNAPSHOT	0
#define FLASH_REPLACE	1

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudPhotoFlash : public EditablePanel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudPhotoFlash, EditablePanel );

public:
	CHudPhotoFlash( const char *pElementName );

	void MsgFunc_Flash( bf_read &msg );

protected:
	virtual void Reset( void );
	virtual void Init( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void Paint( void );
	virtual bool ShouldDraw( void );

	void PaintGlow( void );

private:
	int		m_nFlashTexture;
	float	m_flDisplayTime;
	float	m_flStartTime;
	float	m_flFadeInTime;
	float	m_flFadeOutTime;
	int		m_nFlashType;
	Vector	m_vecPosition;
};

DECLARE_HUDELEMENT_DEPTH( CHudPhotoFlash, 100 );

DECLARE_HUD_MESSAGE( CHudPhotoFlash, Flash );

const float FADE_IN_DURATION = 0.5f;
const float FADE_OUT_DURATION = 0.25f;

// const char szFlashMaterial[] = "sprites/light_glow02_add_noz";
const char szFlashMaterial[] = "HUD/light_glow03";

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheHudPhotoFlashMaterials )
PRECACHE( MATERIAL, szFlashMaterial )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudPhotoFlash::CHudPhotoFlash( const char *pElementName ) : CHudElement(pElementName), BaseClass( NULL, "HudPhotoInventory" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	LoadControlSettings( "Resource/PhotoInventory.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudPhotoFlash::Init( void )
{
	static bool bHook = true;
	if( bHook ) //avoid repeatedly hooking the same message (so we don't process it a bunch)
	{
		HOOK_HUD_MESSAGE( CHudPhotoFlash, Flash );
		bHook = false;
	}

	m_nFlashTexture = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nFlashTexture, szFlashMaterial, true, false );

	m_flDisplayTime = 0.0f;
	m_flStartTime = 0.0f;
	m_flFadeInTime = 0.0f;
	m_flFadeOutTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudPhotoFlash::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudPhotoFlash::ApplySchemeSettings( vgui::IScheme *scheme )
{
	LoadControlSettings( "Resource/PhotoInventory.res" );
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	// SetPaintBackgroundType( 2 );

	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );

	// We want to take over the lower half of the screen, roughly
	SetBounds( 0, 0, screenWide, screenTall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudPhotoFlash::ShouldDraw( void )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>(C_BasePlayer::GetLocalPlayer());
	if ( pPlayer == NULL )
		return false;

	// Must have something in our inventory!
	if ( m_flDisplayTime < gpGlobals->curtime )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the flash effect
//-----------------------------------------------------------------------------
void CHudPhotoFlash::PaintGlow( void )
{
	Vector vecScreen;
	if ( ScreenTransform( m_vecPosition, vecScreen ) )
		return;

	float x = ScreenWidth()/2;
	float y = ScreenHeight()/2;

	x += 0.5 * vecScreen[0] * ScreenWidth() + 0.5;
	y -= 0.5 * vecScreen[1] * ScreenHeight() + 0.5;

	int nWidth = ScreenWidth() * 1.25;
	int nHeight = ScreenHeight() * 1.25;

	int xLeft = x - nWidth;
	int xRight = x + nWidth;
	int yTop = y - nHeight;
	int yBottom = y + nHeight;
	
	surface()->DrawSetTexture( m_nFlashTexture );

	Vertex_t vert[4];
	vert[0].Init( Vector2D( xLeft, yTop ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xRight,yTop ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xRight,yBottom), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft, yBottom ), Vector2D(0,1) );
	
	int nAlpha = RemapValClamped( gpGlobals->curtime, m_flStartTime, m_flDisplayTime, 255.0f, 0.0f );
	surface()->DrawSetColor( nAlpha, nAlpha, nAlpha, 255 );
	surface()->DrawTexturedPolygon( 4, vert );
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudPhotoFlash::Paint( void )
{
	// TEMP
	if ( m_flDisplayTime > gpGlobals->curtime )
	{
		PaintGlow();
	}

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPhotoFlash::MsgFunc_Flash( bf_read &msg )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *) C_BasePlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return;

	// Basic info
	m_flStartTime = gpGlobals->curtime;
	m_flFadeOutTime = msg.ReadFloat();
	m_flDisplayTime = gpGlobals->curtime + m_flFadeOutTime;

	m_vecPosition.x = msg.ReadFloat();
	m_vecPosition.y = msg.ReadFloat();
	m_vecPosition.z = msg.ReadFloat();
}
