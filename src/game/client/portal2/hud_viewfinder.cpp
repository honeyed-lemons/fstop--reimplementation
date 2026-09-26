//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "c_basehlplayer.h"
#include "c_portal_player.h"
#include "debugoverlay_shared.h"
#include "view.h"

#include <vgui/ischeme.h>
#include <vgui/isurface.h>
#include <keyvalues.h>
#include <vgui_controls/animationcontroller.h>

//for screenfade
#include "ivieweffects.h"
#include "shake.h"
#include "view_scene.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudViewfinder : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudViewfinder, vgui::Panel );

public:
	CHudViewfinder( const char *pElementName );

	void	Init( void );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );
	virtual bool ShouldDraw( void );

private:
	void PaintLocator( C_BaseEntity *pTarget );
	void PaintLocators( void );

	int m_iScopeTexture[4];
	int m_nChickenIcon;
	int m_nArrowIcon;
};

DECLARE_HUDELEMENT_DEPTH( CHudViewfinder, 100 );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudViewfinder::CHudViewfinder( const char *pElementName ) : CHudElement(pElementName), BaseClass(NULL, "HudViewfinder")
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudViewfinder::Init( void )
{
	int i;
	for( i=0;i<4;i++ )
	{
		m_iScopeTexture[i] = surface()->CreateNewTextureID();
	}

	surface()->DrawSetTextureFile( m_iScopeTexture[0], "HUD/camera_viewfinder_ul", true, false );
	surface()->DrawSetTextureFile( m_iScopeTexture[1], "HUD/camera_viewfinder_halfcircle", true, false );
	
	m_nChickenIcon = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nChickenIcon, "HUD/hud_loc_chicken", true, false );
	
	m_nArrowIcon = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nArrowIcon, "HUD/hud_loc_arrow", true, false );

	// remove ourselves from the global group so the scoreboard doesn't hide us
	UnregisterForRenderGroup( "global" );
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudViewfinder::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);
	SetBounds(0, 0, screenWide, screenTall);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudViewfinder::ShouldDraw( void )
{
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer *>(C_BasePlayer::GetLocalPlayer());
	if ( pPlayer == NULL )
		return false;

	return ( pPlayer->m_HL2Local.m_bZooming && CHudElement::ShouldDraw() );
}

void UTIL_WorldToScreenCoords( const Vector &vecWorld, int *pScreenX, int *pScreenY )
{
	*pScreenX = 0;
	*pScreenY = 0;

	Vector vecTransform;
	if ( ScreenTransform( vecWorld, vecTransform ) )
		return;

	*pScreenX = (ScreenWidth()/2)  + 0.5f * vecTransform.x * ScreenWidth()  + 0.5f;
	*pScreenY = (ScreenHeight()/2) - 0.5f * vecTransform.y * ScreenHeight() + 0.5f;
}

void UTIL_GenerateBoxVertices( const Vector &vOrigin, const Vector &vMins, const Vector &vMaxs, Vector pVerts[8] )
{
	Vector vecPos;
	for ( int i = 0; i < 8; ++i )
	{
		vecPos[0] = ( i & 0x1 ) ? vMaxs[0] : vMins[0];
		vecPos[1] = ( i & 0x2 ) ? vMaxs[1] : vMins[1];
		vecPos[2] = ( i & 0x4 ) ? vMaxs[2] : vMins[2];
		pVerts[i] = vecPos + vOrigin;
	}
}

bool UTIL_EntityBoundsToSizes( C_BaseEntity *pTarget, int *pMinX, int *pMinY, int *pMaxX, int *pMaxY )
{
	Vector vecBoxVerts[8];
	UTIL_GenerateBoxVertices( pTarget->GetAbsOrigin(), pTarget->WorldAlignMins(), pTarget->WorldAlignMaxs(), vecBoxVerts );

	int nMaxX = 0;
	int nMinX = ScreenWidth();
	int nMaxY = 0;
	int nMinY = ScreenHeight();

	int nX, nY;
	for ( int i = 0; i < 8; i++ )
	{
		UTIL_WorldToScreenCoords( vecBoxVerts[i], &nX, &nY );

		/*
		surface()->DrawSetColor( 255, 128, 0, 255 );
		surface()->DrawOutlinedCircle( nX, nY, 4, 4 );
		*/

		if ( nX > nMaxX )
		{
			nMaxX = nX;
		}
		else if ( nX < nMinX )
		{
			nMinX = nX;
		}
		
		if ( nY > nMaxY )
		{
			nMaxY = nY;
		}
		else if ( nY < nMinY )
		{
			nMinY = nY;
		}
	}

	/*
	surface()->DrawSetColor( 255, 0, 0, 255 );
	surface()->DrawOutlinedCircle( nMaxX, nMaxY, 8, 32 );

	surface()->DrawSetColor( 0, 255, 0, 255 );
	surface()->DrawOutlinedCircle( nMinX, nMinY, 8, 32 );
	*/

	*pMaxX = nMaxX;
	*pMinX = nMinX;
	*pMaxY = nMaxY;
	*pMinY = nMinY;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draws object locators on the screen
//-----------------------------------------------------------------------------
void CHudViewfinder::PaintLocator( C_BaseEntity *pTarget )
{
	// Center	
	Vector vecScreen;
	if ( ScreenTransform( pTarget->WorldSpaceCenter(), vecScreen ) )
		return;

	float centerX = ScreenWidth()/2;
	float centerY = ScreenHeight()/2;

	centerX += 0.5 * vecScreen[0] * ScreenWidth() + 0.5;
	centerY -= 0.5 * vecScreen[1] * ScreenHeight() + 0.5;
	
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	float flZDiff = pTarget->WorldSpaceCenter().z - pLocalPlayer->EyePosition().z;
	float flDistSqr = ( pTarget->WorldSpaceCenter() - pLocalPlayer->EyePosition() ).LengthSqr();
	float flDistScale = RemapValClamped( flDistSqr, Square(25*12), Square(300*12), 1.0f, 0.25f );

	float screenMax = 48.0f * flDistScale;

	surface()->DrawSetTexture( m_nChickenIcon );
	
	const int nDropShadowDepth = 4;

	// First, draw the drop-shadow
	surface()->DrawSetColor( 0, 0, 0, 64 * flDistScale );
	surface()->DrawTexturedRect( ( centerX - screenMax ) + nDropShadowDepth, ( centerY - screenMax ) + nDropShadowDepth, 
								   centerX + screenMax + nDropShadowDepth, centerY + screenMax + nDropShadowDepth );

	// Now draw the real deal
	surface()->DrawSetColor( 255, 128, 0, 164 * flDistScale );
	surface()->DrawTexturedRect( centerX - screenMax, centerY - screenMax, centerX + screenMax, centerY + screenMax );

	// If we're a story up or down, report that difference
	if ( fabs( flZDiff ) > (10*12) )
	{
		surface()->DrawSetTexture( m_nArrowIcon );

		int nWidth = 8 * flDistScale;
		int nHeight = 8 * flDistScale;

		if ( flZDiff > 0.0f )
		{
			surface()->DrawTexturedRect( centerX - nWidth, centerY - ( nHeight + screenMax ), 
										 centerX + nWidth, ( centerY - screenMax ) );
		}
		else
		{
			vgui::Vertex_t vert[4];	

			//upper left
			vert[0].Init( Vector2D( centerX - nWidth, centerY + screenMax ), Vector2D(0,1) );
			vert[1].Init( Vector2D( centerX + nWidth, centerY + screenMax ), Vector2D(1,1) );
			vert[2].Init( Vector2D( centerX + nWidth, centerY + nHeight + screenMax ), Vector2D(1,0) );
			vert[3].Init( Vector2D( centerX - nWidth, centerY + nHeight + screenMax ), Vector2D(0,0) );

			surface()->DrawTexturedPolygon( 4, vert );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws object locators on the screen
//-----------------------------------------------------------------------------
void CHudViewfinder::PaintLocators( void )
{
	C_Portal_Player *pPlayer = (C_Portal_Player *) C_BasePlayer::GetLocalPlayer();
	
	// Now, paint them all in relation to their locations
	for ( int i = 0; i < 16; i++ )
	{
		int nEntityIndex =	pPlayer->m_HL2Local.m_nLocatorEntityIndices[i];
		if ( nEntityIndex < 0 )
			continue;

		C_BaseEntity *pEntity = C_BaseEntity::Instance( nEntityIndex );
		if ( pEntity )
		{
			PaintLocator( pEntity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudViewfinder::Paint( void )
{
	// Draw locators as well
	PaintLocators();

	// We need to update the refraction texture so the scope can refract it
	UpdateRefractTexture();

	int screenWide, screenTall;
	GetHudSize( screenWide, screenTall );

	// calculate the bounds in which we should draw the scope
	int xMid = screenWide / 2;
	int yMid = screenTall / 2;

	// width of the drawn scope. in widescreen, we draw the sides with primitives
	int wide = screenWide;

	int xLeft = xMid - wide/2;
	int xRight = xMid + wide/2;
	int yTop = 0;
	int yBottom = screenTall;

	float uv1 = 0.0f, uv2 = 1.0f;

	vgui::Vertex_t vert[4];	

	Vector2D uv11( uv1, uv1 );
	Vector2D uv12( uv1, uv2 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );

	vgui::surface()->DrawSetColor(0,0,0,255);

	//upper left
	vgui::surface()->DrawSetTexture( m_iScopeTexture[0] );
	vert[0].Init( Vector2D( xLeft, yTop ), Vector2D(0,0) );
	vert[1].Init( Vector2D( xMid,  yTop ), Vector2D(1,0) );
	vert[2].Init( Vector2D( xMid,  yMid ), Vector2D(1,1) );
	vert[3].Init( Vector2D( xLeft, yMid ), Vector2D(0,1) );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	// top right
	vert[0].Init( Vector2D( xMid, yTop ), Vector2D(1,0) );
	vert[1].Init( Vector2D( xRight,   yTop ), Vector2D(0,0) );
	vert[2].Init( Vector2D( xRight,   yMid ), Vector2D(0,1) );
	vert[3].Init( Vector2D( xMid, yMid ), Vector2D(1,1) );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	// bottom right
	vert[0].Init( Vector2D( xMid,   yMid ), Vector2D(1,1) );
	vert[1].Init( Vector2D( xRight, yMid ), Vector2D(0,1) );
	vert[2].Init( Vector2D( xRight, yBottom ), Vector2D(0,0) );
	vert[3].Init( Vector2D( xMid,   yBottom ), Vector2D(1,0) );
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// bottom left
	vert[0].Init( Vector2D( xLeft, yMid ), Vector2D(0,1) );
	vert[1].Init( Vector2D( xMid,  yMid ), Vector2D(1,1) );
	vert[2].Init( Vector2D( xMid,  yBottom ), Vector2D(1,0) );
	vert[3].Init( Vector2D( xLeft, yBottom), Vector2D(0,0) );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	/*
	if ( wide < screenWide )
	{
		// Left block
		vgui::surface()->DrawFilledRect( 0, 0, xLeft, screenTall );

		// Right block
		vgui::surface()->DrawFilledRect( xRight, 0, screenWide, screenTall );
	}
	*/

	// draw zoom circles
	int tall;
	GetSize(wide, tall);

	int nInnerCircle = ( screenTall * 0.15f );
	int nOuterCircle = nInnerCircle * 1.25f;
	surface()->DrawOutlinedCircle( xMid, yMid, nInnerCircle, 64 );
	surface()->DrawOutlinedCircle( xMid, yMid, nOuterCircle, 64 );

	// Draw inner reticle
	vgui::surface()->DrawSetColor( 0,0,0,64 );
	vgui::surface()->DrawSetTexture(m_iScopeTexture[1]);
	const float circleScale = ( screenTall * 0.05f );
	vert[0].Init( Vector2D( xMid-circleScale,  yMid ), uv12 );
	vert[1].Init( Vector2D( xMid+circleScale,  yMid ), uv22 );
	vert[2].Init( Vector2D( xMid+circleScale,  yMid+circleScale ), uv21 );
	vert[3].Init( Vector2D( xMid-circleScale,  yMid+circleScale ), uv11 );
	vgui::surface()->DrawTexturedPolygon( 4, vert );
	
	vgui::surface()->DrawSetColor( 0,0,0,32 );

	vert[0].Init( Vector2D( xMid-circleScale,  yMid-circleScale ), uv21 );
	vert[1].Init( Vector2D( xMid+circleScale,  yMid-circleScale ), uv11 );
	vert[2].Init( Vector2D( xMid+circleScale,  yMid ), uv12 );
	vert[3].Init( Vector2D( xMid-circleScale,  yMid ), uv22 );
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// Corner registers
	const float flRegisterWeight = 1.0f;
	const float flRegisterOffset = screenWide * 0.125f;
	const float flRegisterLength = screenWide * 0.1f;
	
	vgui::surface()->DrawSetColor( 0,0,0,255 );

	// Upper left
	vgui::surface()->DrawFilledRect( flRegisterOffset, flRegisterOffset, flRegisterOffset + flRegisterLength, flRegisterOffset + flRegisterWeight );
	vgui::surface()->DrawFilledRect( flRegisterOffset, flRegisterOffset, flRegisterOffset + flRegisterWeight, flRegisterOffset + flRegisterLength );
	
	// Lower left
	vgui::surface()->DrawFilledRect( flRegisterOffset, screenTall - flRegisterOffset, flRegisterOffset + flRegisterLength, screenTall - flRegisterOffset + flRegisterWeight );
	vgui::surface()->DrawFilledRect( flRegisterOffset, screenTall - ( flRegisterOffset + flRegisterLength ), flRegisterOffset + flRegisterWeight, screenTall - flRegisterOffset );

	// Upper right
	vgui::surface()->DrawFilledRect( screenWide - ( flRegisterOffset + flRegisterLength ), flRegisterOffset, screenWide - flRegisterOffset, flRegisterOffset + flRegisterWeight );
	vgui::surface()->DrawFilledRect( screenWide - ( flRegisterOffset + flRegisterWeight ), flRegisterOffset, screenWide - flRegisterOffset, flRegisterOffset + flRegisterLength );

	// Lower right
	vgui::surface()->DrawFilledRect( screenWide - ( flRegisterOffset + flRegisterLength ), screenTall - ( flRegisterOffset + flRegisterWeight ), screenWide - flRegisterOffset, screenTall - flRegisterOffset );
	vgui::surface()->DrawFilledRect( screenWide - ( flRegisterOffset + flRegisterWeight ), screenTall - ( flRegisterOffset + flRegisterLength ), screenWide - flRegisterOffset, screenTall - flRegisterOffset );
}