//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal mod render targets are specified by and accessable through this singleton
//
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#ifndef APERTURERENDERTARGETS_H_
#define APERTURERENDERTARGETS_H_
#ifdef _WIN32
#pragma once
#endif

#include "portal_render_targets.h"

#ifndef APERTURERENDERTARGETS_H_ 
#pragma message ( "This file should only be built with aperture builds" )
#endif

// externs
class IMaterialSystem;
class IMaterialSystemHardwareConfig;

class CApertureRenderTargets : public CPortalRenderTargets
{
	// no networked vars
	DECLARE_CLASS_GAMEROOT(CApertureRenderTargets, CPortalRenderTargets);
public:
	virtual void InitClientRenderTargets(IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig);
	virtual void ShutdownClientRenderTargets();

	ITexture *GetLargePhotoRenderTarget(int iIndex);
	//ITexture *GetSmallPhotoRenderTarget( int iIndex );

private:
	void InitLargePhotoTextures(IMaterialSystem* pMaterialSystem);
	//void InitSmallPhotoTextures( IMaterialSystem* pMaterialSystem );

	CTextureReference m_LargePhotoTextures[3];
	//CTextureReference m_SmallPhotoTextures[8];
};

extern CApertureRenderTargets* aperturerendertargets;


#endif //APERTURERENDERTARGETS_H_