//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Displays various indicators to the player
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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudIndicator : public EditablePanel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudIndicator, EditablePanel );

public:
	CHudIndicator( const char *pElementName );

	void MsgFunc_IndicatorFlash( bf_read &msg );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );
	virtual void Init( void );
	virtual bool ShouldDraw( void );
	virtual void Reset( void );

private:
	int		m_nTexture[2];	// Invalid and full indicators
	int		m_nIndicatorType;

	float	m_flDisplayTime;
	float	m_flStartTime;
	float	m_flFadeInTime;
	float	m_flFadeOutTime;
};

DECLARE_HUDELEMENT_DEPTH( CHudIndicator, 150 );
DECLARE_HUD_MESSAGE( CHudIndicator, IndicatorFlash );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudIndicator::CHudIndicator( const char *pElementName ) : CHudElement(pElementName), BaseClass( NULL, "HudIndicator" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	LoadControlSettings( "Resource/Indicator.res" );
	m_nTexture[0] = -1;
	m_nTexture[1] = -1;
	m_nIndicatorType = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudIndicator::Init( void )
{
	HOOK_HUD_MESSAGE( CHudIndicator, IndicatorFlash );
	
	m_nTexture[0] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTexture[0], "HUD/invalid", true, false );

	m_nTexture[1] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTexture[1], "HUD/inv_full", true, false );

	m_flDisplayTime = 0.0f;
	m_flStartTime = 0.0f;
	m_flFadeInTime = 0.0f;
	m_flFadeOutTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudIndicator::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudIndicator::ApplySchemeSettings( vgui::IScheme *scheme )
{
	LoadControlSettings( "Resource/Indicator.res" );
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );

	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );

	int nCenterX = screenWide / 2;
	int nCenterY = screenTall / 2;

	// We want to take over the lower half of the screen, roughly
	SetBounds( nCenterX-128, nCenterY-128, 256, 256 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudIndicator::ShouldDraw( void )
{
	if ( m_flDisplayTime > gpGlobals->curtime )
		return true;

	// FIXME: Only when we need to!
	return false;
}

const float FADE_IN_DURATION = 0.2f;
const float FADE_OUT_DURATION = 0.2f;

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudIndicator::Paint( void )
{
	float flGlobalScale = 1.0f;
	float flGlobalAlpha = 1.0f;
	
	if ( gpGlobals->curtime < m_flFadeInTime )
	{
		flGlobalScale = flGlobalAlpha = SimpleSplineRemapValClamped( gpGlobals->curtime, m_flStartTime, m_flFadeInTime, 0.0f, 1.0f );
	}
	else if ( gpGlobals->curtime > m_flFadeOutTime )
	{
		flGlobalScale = flGlobalAlpha = SimpleSplineRemapValClamped( gpGlobals->curtime, m_flFadeOutTime, m_flDisplayTime, 1.0f, 0.0f );
	}

	float flBump = sinf( gpGlobals->curtime * 10.0f ) * 16.0f * flGlobalScale;
	float flScale = 128.0f + flBump * flGlobalScale;
	const float flOffset = flScale * 0.5f;

	// Set the texture
	Assert( m_nIndicatorType < ARRAYSIZE(m_nTexture) );
	surface()->DrawSetTexture( m_nTexture[m_nIndicatorType] );

	// Center
	float xLeft = ( GetWide() / 2 ) - flOffset;
	float xRight = ( GetWide() / 2 ) + flOffset;
	float yTop = ( GetTall() / 2 ) - flOffset;
	float yBottom = ( GetTall() / 2 ) + flOffset;

	// Drop-shadow
	surface()->DrawSetColor( 0, 0, 0, 128.0f * flGlobalAlpha );
	const int nShadowDepth = 4;
	Vertex_t vert[4];
	vert[0].Init( Vector2D( xLeft+nShadowDepth, yTop+nShadowDepth ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xRight+nShadowDepth,yTop+nShadowDepth ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xRight+nShadowDepth,yBottom+nShadowDepth), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft+nShadowDepth, yBottom+nShadowDepth ), Vector2D(0,1) );
	surface()->DrawTexturedPolygon( 4, vert );

	// Main icon
	surface()->DrawSetColor( 255, 255, 255, 255.0f * flGlobalAlpha );
	vert[0].Init( Vector2D( xLeft, yTop ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xRight,yTop ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xRight,yBottom), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft, yBottom ), Vector2D(0,1) );
	surface()->DrawTexturedPolygon( 4, vert );

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudIndicator::MsgFunc_IndicatorFlash( bf_read &msg )
{
	// Read the type
	m_nIndicatorType = msg.ReadByte();

	// Setup the display time
	m_flStartTime = gpGlobals->curtime;
	m_flDisplayTime = gpGlobals->curtime + msg.ReadFloat();
	m_flFadeInTime = gpGlobals->curtime + FADE_IN_DURATION;
	m_flFadeOutTime = m_flDisplayTime - FADE_OUT_DURATION;
}


ConVar hud_show_control_helper( "hud_show_control_helper", "0" );

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudControlHelper : public EditablePanel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudControlHelper, EditablePanel );

public:
	CHudControlHelper( const char *pElementName );

	void MsgFunc_ControlHelperAnimate( bf_read &msg );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void Paint( void );
	virtual void Init( void );
	virtual bool ShouldDraw( void );
	virtual void Reset( void );

private:
	void	DrawIcon( int x, int y, float flScale, float flAlpha, int nIconID, int nDepth );

	int		m_nTexture[3];	// Left, right and center indicators
	int		m_nFrontIcon;

	float	m_flDisplayTime;
	float	m_flStartTime;
	float	m_flFadeInTime;
	float	m_flFadeOutTime;
	bool	m_bHoldIndefinitely;
};

DECLARE_HUDELEMENT_DEPTH( CHudControlHelper, 150 );
DECLARE_HUD_MESSAGE( CHudControlHelper, ControlHelperAnimate );

const int HELPER_ICON_WIDTH = 48;
const int HELPER_ICON_HEIGHT = 48;
const int HELPER_ICON_PAD = 20;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudControlHelper::CHudControlHelper( const char *pElementName ) : CHudElement(pElementName), BaseClass( NULL, "HudControlHelper" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	LoadControlSettings( "Resource/ControlHelper.res" );
	m_nTexture[0] = -1;
	m_nTexture[1] = -1;
	m_nTexture[2] = -1;

	m_nFrontIcon = 0;

	m_bHoldIndefinitely = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudControlHelper::Init( void )
{
	HOOK_HUD_MESSAGE( CHudControlHelper, ControlHelperAnimate );

	m_nTexture[0] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTexture[0], "HUD/hud_icon_camera", true, false );

	m_nTexture[1] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTexture[1], "HUD/hud_icon_neutral", true, false );

	m_nTexture[2] = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTexture[2], "HUD/hud_icon_picture", true, false );

	m_flDisplayTime = 0.0f;
	m_flStartTime = 0.0f;
	m_flFadeInTime = 0.0f;
	m_flFadeOutTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Init
//-----------------------------------------------------------------------------
void CHudControlHelper::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudControlHelper::ApplySchemeSettings( vgui::IScheme *scheme )
{
	LoadControlSettings( "Resource/ControlHelper.res" );
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );

	int nCenterY = (float) ScreenHeight() * 0.8f;

	// We want to take over the lower half of the screen, roughly
	SetBounds( 0, nCenterY-HELPER_ICON_HEIGHT, ScreenWidth(), HELPER_ICON_HEIGHT*2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudControlHelper::ShouldDraw( void )
{
	if ( hud_show_control_helper.GetBool() == false )
		return false;

	if ( m_flDisplayTime > gpGlobals->curtime || m_bHoldIndefinitely )
		return true;

	// FIXME: Only when we need to!
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the actual helper icon
//-----------------------------------------------------------------------------
void CHudControlHelper::DrawIcon( int x, int y, float flScale, float flAlpha, int nIconID, int nDepth )
{
	// Select the image
	Assert( nIconID < ARRAYSIZE(m_nTexture) );
	surface()->DrawSetTexture( m_nTexture[nIconID] );
	
	const float flOffset = flScale * 0.5f;
	
	// Center
	float xLeft = x - flOffset;
	float xRight = x + HELPER_ICON_WIDTH + flOffset;
	float yTop = y - flOffset;
	float yBottom = y + HELPER_ICON_HEIGHT + flOffset;

	// Drop-shadow
	surface()->DrawSetColor( 0, 0, 0, 127 * flAlpha );
	const int nShadowDepth = 6;
	Vertex_t vert[4];
	vert[0].Init( Vector2D( xLeft+nShadowDepth, yTop+nShadowDepth ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xRight+nShadowDepth,yTop+nShadowDepth ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xRight+nShadowDepth,yBottom+nShadowDepth), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft+nShadowDepth, yBottom+nShadowDepth ), Vector2D(0,1) );
	surface()->DrawTexturedPolygon( 4, vert );

	// Main icon
	float flColor = 255 / nDepth;
	surface()->DrawSetColor( flColor, flColor, flColor, 255.0f * flAlpha );
	vert[0].Init( Vector2D( xLeft, yTop ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xRight,yTop ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xRight,yBottom), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft, yBottom ), Vector2D(0,1) );
	surface()->DrawTexturedPolygon( 4, vert );
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudControlHelper::Paint( void )
{
	float flGlobalScale = 1.0f;
	float flGlobalAlpha = 1.0f;

	bool bScalingUp = false;
	bool bScalingDown = false;

	if ( gpGlobals->curtime < m_flFadeInTime )
	{
		flGlobalScale = SimpleSplineRemapValClamped( gpGlobals->curtime, m_flStartTime, m_flFadeInTime, 0.0f, 1.0f );
		bScalingUp = true;
	}
	else if ( m_bHoldIndefinitely == false && gpGlobals->curtime > m_flFadeOutTime )
	{
		flGlobalScale = flGlobalAlpha = SimpleSplineRemapValClamped( gpGlobals->curtime, m_flFadeOutTime, m_flDisplayTime, 1.0f, 0.0f );
		bScalingDown = true;
	}

	int iconCenterX = ( GetWide() / 2.0f );
	int iconY = ((float) GetTall() * 0.8f) - HELPER_ICON_HEIGHT;
	
	// Don't move when scaling up
	float flMoveScale = ( bScalingUp ) ? 1.0f : flGlobalScale;

	int nBaseOffset = iconCenterX-(HELPER_ICON_WIDTH*0.5f);
	int xOffset[ARRAYSIZE(m_nTexture)] = 
	{ 
		nBaseOffset-((HELPER_ICON_WIDTH+HELPER_ICON_PAD)*flMoveScale),
		nBaseOffset,
		nBaseOffset+((HELPER_ICON_WIDTH+HELPER_ICON_PAD)*flMoveScale),
	};

	int nCurIcon = (m_nFrontIcon+1)%ARRAYSIZE(m_nTexture);
	for ( int i = 0; i < ARRAYSIZE(m_nTexture); i++ )
	{
		float flScaleFactor = (m_nFrontIcon==nCurIcon && m_nFrontIcon != 1) ? 2.0f : 1.0f;
		
		int nDepth = (nCurIcon==m_nFrontIcon) ? 1 : 2;

		int xPos = xOffset[nCurIcon];
		
		// Scale the padding up as well to keep things proportional
		if ( nCurIcon == m_nFrontIcon )
		{
			if ( nCurIcon == 0 )
			{
				xPos -= ( HELPER_ICON_PAD / 2 ) * flGlobalScale;
			}
			else if ( nCurIcon == 2 )
			{
				xPos += ( HELPER_ICON_PAD / 2 ) * flGlobalScale;
			}
		}

		float flScale = HELPER_ICON_WIDTH;
		if ( bScalingDown )
		{
			flScale *= flGlobalScale;
		}
		else if ( m_nFrontIcon == nCurIcon )
		{
			if ( bScalingUp )
			{
				flScale *= flGlobalScale;
			}
		}

		DrawIcon( xPos, iconY, flScale*flScaleFactor, flGlobalAlpha, nCurIcon, nDepth );
		nCurIcon = (nCurIcon+1)%(ARRAYSIZE(m_nTexture));
	}

	BaseClass::Paint();
}

const float HELPER_FADE_IN_DURATION = 0.2f;
const float HELPER_FADE_OUT_DURATION = 0.1f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudControlHelper::MsgFunc_ControlHelperAnimate( bf_read &msg )
{
	bool bClear = msg.ReadByte();
	if ( bClear )
	{
		m_flDisplayTime = gpGlobals->curtime + HELPER_FADE_OUT_DURATION;
		m_flStartTime = m_flFadeInTime = gpGlobals->curtime;
		m_flFadeOutTime = m_flDisplayTime;
		m_bHoldIndefinitely = false;
		return;
	}

	m_nFrontIcon = msg.ReadByte();

	// Setup the display time
	if ( m_nFrontIcon == 1 )
	{
		m_flDisplayTime = gpGlobals->curtime + 0.5f;
		m_bHoldIndefinitely = false;
	}
	else
	{
		m_bHoldIndefinitely = true;
	}

	m_flStartTime = gpGlobals->curtime;
	
	if ( bClear == false )
	{
		m_flFadeInTime = gpGlobals->curtime + HELPER_FADE_IN_DURATION;
	}

	m_flFadeOutTime = m_flDisplayTime - HELPER_FADE_OUT_DURATION;
}
