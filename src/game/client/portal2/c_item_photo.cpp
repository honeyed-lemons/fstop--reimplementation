//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side photo with texture bindings
//
//=============================================================================//

#include "cbase.h"
#include "iviewrender.h"
#include "proxyentity.h"
#include "materialsystem/imaterialvar.h"
#include "c_portal_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_Photograph : public C_BaseAnimating
{
	DECLARE_CLASS( C_Photograph, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

public:
	const char *GetTextureName( void ) { return m_szTextureName; }
	char m_szTextureName[MAX_PATH];

};

IMPLEMENT_CLIENTCLASS_DT( C_Photograph, DT_Photograph, CPhotograph )
	RecvPropString( RECVINFO( m_szTextureName ) ),
END_RECV_TABLE()

//------------------------------------------------------------------------------
// A material proxy that resets the texture to use the original surface texture
//------------------------------------------------------------------------------
class CPhotoMaterialProxy : public CEntityMaterialProxy
{
public:
	CPhotoMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar* m_BaseTextureVar;
};

CPhotoMaterialProxy::CPhotoMaterialProxy()
{
	m_BaseTextureVar = NULL;
}

bool CPhotoMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	return foundVar;
}

void CPhotoMaterialProxy::OnBind( C_BaseEntity *pC_BaseEntity )
{
	C_Photograph *pPhoto = dynamic_cast<C_Photograph *>(pC_BaseEntity);
	if( pPhoto == NULL )
		return;

	// Use the current base texture specified by the suface
	ITexture *pTexture = materials->FindTexture( pPhoto->GetTextureName(), TEXTURE_GROUP_MODEL, true );
	m_BaseTextureVar->SetTextureValue( pTexture );
}

IMaterial *CPhotoMaterialProxy::GetMaterial( void )
{
	if ( !m_BaseTextureVar )
		return NULL;

	return m_BaseTextureVar->GetOwningMaterial();
}

EXPOSE_INTERFACE( CPhotoMaterialProxy, IMaterialProxy, "PhotoMaterial" IMATERIAL_PROXY_INTERFACE_VERSION );

//------------------------------------------------------------------------------
// A material proxy that resets the texture to use the original surface texture
//------------------------------------------------------------------------------
class CPlacementPhotoMaterialProxy : public CEntityMaterialProxy
{
public:
	CPlacementPhotoMaterialProxy();
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual IMaterial *GetMaterial() { return m_pMaterial; }

private:
	float	GetAlphaFade( void );

	IMaterial		*m_pMaterial;
	IMaterialVar	*m_pAlphaVar;
	bool			m_bLastState;
	float			m_flStartAlpha;
	float			m_flTargetAlpha;
	float			m_flFadeStartTime;
	float			m_flFadeDuration;
};

CPlacementPhotoMaterialProxy::CPlacementPhotoMaterialProxy()
{
	m_pMaterial = NULL;
	m_pAlphaVar = NULL;

	m_flStartAlpha = 1.0f;
	m_flTargetAlpha = 1.0f;
	m_flFadeDuration = 1.0f;
	m_flFadeStartTime = 0.0f;
	m_bLastState = false;
}

bool CPlacementPhotoMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool found;
	m_pMaterial = pMaterial;
	m_pAlphaVar = pMaterial->FindVar( "$alpha", &found );
	if ( found == NULL )
	{
		m_pAlphaVar = NULL;
		return false;
	}

	m_bLastState = false;
	return true;
}

float CPlacementPhotoMaterialProxy::GetAlphaFade( void )
{
	return SimpleSplineRemapValClamped( gpGlobals->curtime, m_flFadeStartTime, m_flFadeStartTime + m_flFadeDuration, m_flStartAlpha, m_flTargetAlpha );
}

void CPlacementPhotoMaterialProxy::OnBind( C_BaseEntity *pC_BaseEntity )
{
	C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player *>(C_BasePlayer::GetLocalPlayer());
	if ( pPlayer == NULL )
		return;

	// Detect a state change
	if ( m_bLastState != pPlayer->m_HL2Local.m_bPlacingPhoto )
	{
		m_flStartAlpha = GetAlphaFade();
		m_flFadeStartTime = gpGlobals->curtime;
		m_flFadeDuration = 0.4f;

		if ( m_bLastState )
		{
			m_flTargetAlpha = 1.0f;
		}
		else
		{
			m_flTargetAlpha = 0.0f;
		}

		m_bLastState = pPlayer->m_HL2Local.m_bPlacingPhoto;
	}

	m_pAlphaVar->SetFloatValue( GetAlphaFade() );
}

EXPOSE_INTERFACE( CPlacementPhotoMaterialProxy, IMaterialProxy, "PlacementPhoto" IMATERIAL_PROXY_INTERFACE_VERSION );
