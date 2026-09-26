//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal mod render targets are specified by and accessable through this singleton
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "aperture_render_targets.h"
#include "materialsystem\imaterialsystem.h"
#include "rendertexture.h"

extern CApertureRenderTargets* aperturerendertargets;

void CApertureRenderTargets::InitLargePhotoTextures(IMaterialSystem* pMaterialSystem)
{
	Msg("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
	for (int i = 0; i != ARRAYSIZE(m_LargePhotoTextures); ++i)
	{
		char szName[256];
		if (i = 1)
			Q_snprintf(szName, sizeof(szName), "_rt_LargePhoto%1");
		if (i = 2)
			Q_snprintf(szName, sizeof(szName), "_rt_LargePhoto%2");
		if (i = 3)
			Q_snprintf(szName, sizeof(szName), "_rt_LargePhoto%3");

		m_LargePhotoTextures[i].Init(pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			szName,
			256, 256, RT_SIZE_DEFAULT,
			IMAGE_FORMAT_RGB888,
			//pMaterialSystem->GetBackBufferFormat(),
			MATERIAL_RT_DEPTH_SHARED,
			0,
			CREATERENDERTARGETFLAGS_HDR));
	}
}

/*void CApertureRenderTargets::InitSmallPhotoTextures( IMaterialSystem* pMaterialSystem )
{
	for( int i = 0; i != ARRAYSIZE( m_SmallPhotoTextures ); ++i )
	{
		char szName[256];
		sprintf( szName, "_rt_SmallPhoto%d", i + 1 );

		m_SmallPhotoTextures[i].Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2(
												szName,
												32, 32, RT_SIZE_DEFAULT,
												IMAGE_FORMAT_RGB888, //pMaterialSystem->GetBackBufferFormat(),
												MATERIAL_RT_DEPTH_SHARED,
												0,
												CREATERENDERTARGETFLAGS_HDR ) );
	}
}*/

ITexture *CApertureRenderTargets::GetLargePhotoRenderTarget(int iIndex)
{
	if ((iIndex < 0) || (iIndex >= ARRAYSIZE(m_LargePhotoTextures)))
		return NULL;

	return m_LargePhotoTextures[iIndex];
}

/*ITexture *CApertureRenderTargets::GetSmallPhotoRenderTarget( int iIndex )
{
	if( (iIndex < 0) || (iIndex >= ARRAYSIZE( m_SmallPhotoTextures )) )
		return NULL;

	return m_SmallPhotoTextures[iIndex];
}*/


//-----------------------------------------------------------------------------
// Purpose: InitClientRenderTargets, interface called by the engine at material system init in the engine
// Input  : pMaterialSystem - the interface to the material system from the engine (our singleton hasn't been set up yet)
//			pHardwareConfig - the user's hardware config, useful for conditional render targets setup
//-----------------------------------------------------------------------------
void CApertureRenderTargets::InitClientRenderTargets(IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig)
{
	InitLargePhotoTextures(pMaterialSystem);
	//InitSmallPhotoTextures( pMaterialSystem );

	BaseClass::InitClientRenderTargets(pMaterialSystem, pHardwareConfig);
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown client render targets. This gets called during shutdown in the engine
// Input  :  - 
//-----------------------------------------------------------------------------
void CApertureRenderTargets::ShutdownClientRenderTargets()
{
	for (int i = 0; i != ARRAYSIZE(m_LargePhotoTextures); ++i)
	{
		m_LargePhotoTextures[i].Shutdown();
	}

	/*for( int i = 0; i != ARRAYSIZE( m_SmallPhotoTextures ); ++i )
	{
		m_SmallPhotoTextures[i].Shutdown();
	}*/

	BaseClass::ShutdownClientRenderTargets();
}


static CApertureRenderTargets g_ApertureRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CApertureRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION, g_ApertureRenderTargets);
CApertureRenderTargets* aperturerendertargets = &g_ApertureRenderTargets;

static CPortalRenderTargets g_PortalRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CPortalRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION, g_PortalRenderTargets);
CPortalRenderTargets* portalrendertargets = &g_ApertureRenderTargets;
//CPortalRenderTargets* portalrendertargets = &g_ApertureRenderTargets;