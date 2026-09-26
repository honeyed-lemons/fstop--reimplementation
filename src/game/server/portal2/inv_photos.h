//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photo inventory
//
//=====================================================================================//


#ifndef INV_PHOTOS_H
#define INV_PHOTOS_H
#ifdef _WIN32
#pragma once
#endif

#include "photo.h"


const int MAX_PHOTOS = 1;
extern int g_CurMaxInvPhotos;

//
// Photo inventory
//

class CPhotoInventory
{
public:
	DECLARE_CLASS_NOBASE( CPhotoInventory );
	DECLARE_DATADESC();

	CPhotoInventory()
	{
		// Make sure we start clean
		PurgePhotos();
	}

	int NumPhotos( void ) { return m_nNumPhotos; }
	bool CanAddPhoto( void ) { return (NumPhotos() < g_CurMaxInvPhotos); }
	bool IsValidPhoto( int nIndex );
	void CyclePhotos( bool bForward );
	void ReportPhotos( void );
	int FirstAvailablePhoto( int nStartIndex );
	int AddPhoto( CaptureInfo_t *pInfo );
	bool GetPhoto( int nIndex, CaptureInfo_t *pOut );
	bool RemovePhoto( int nIndex );
	void PurgePhotos( void );
	bool UpdatePhoto( int nIndex, CaptureInfo_t &pIn );
	void OnRestore( void );
private:
	CaptureInfo_t			m_Photos[MAX_PHOTOS];
	int						m_nNumPhotos;
};

bool Photo_CanAdd( void );
int Photo_Add( CaptureInfo_t *pInfo );
bool Photo_Get( int nIndex, CaptureInfo_t *pOut );
bool Photo_Remove( int nIndex );
unsigned int Photo_Count( void );
void Photo_Purge( void );
void Photo_Cycle( bool bForward );
bool Photo_IsValid( int nIndex );


#endif //INV_PHOTOS_H