//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photo inventory
//
//=====================================================================================//

#include "cbase.h"
#include "player.h"
#include "portal_player.h"

#include "photo.h"
#include "inv_photos.h"
#include "portal_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Photo inventory
//

int g_CurMaxInvPhotos = MAX_PHOTOS;	 //HACK: the max photos allowed by the player's current weapon_camera


bool CPhotoInventory::IsValidPhoto( int nIndex )
{
	// Make sure it's a valid index
	if ( nIndex < 0 || nIndex >= g_CurMaxInvPhotos )
		return false;

	return ( m_Photos[nIndex].hCapturedEnt.Get() != NULL );
}

void CPhotoInventory::CyclePhotos( bool bForward )
{
	CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
	if ( pPlayer == NULL )
		return;

	int nDirection = ( bForward ) ? 1 : -1;
	int nIndex = pPlayer->GetSelectedPhoto() + nDirection;

	// Figure out the next valid slot, including looping around
	for ( int i = 0; i < g_CurMaxInvPhotos; i++ )
	{
		CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();		
		if ( IsValidPhoto( nIndex % g_CurMaxInvPhotos ) )
		{
			pPlayer->SetSelectedPhoto( nIndex % g_CurMaxInvPhotos );
			return;
		}

		nIndex += nDirection;
	}

	// No photo to cycle to!
}

void CPhotoInventory::ReportPhotos( void )
{
	for ( int i = 0; i < NumPhotos(); i++ )
	{
		Msg( "Photo %d: %s\n", i, m_Photos[i].hCapturedEnt->GetClassname() );
	}
}

int CPhotoInventory::FirstAvailablePhoto( int nStartIndex )
{
	int nIndex = nStartIndex;

	// Find the first free slot
	for ( int i = 0; i < g_CurMaxInvPhotos; i++ )
	{
		// See if this is a free slot
		if ( IsValidPhoto( nIndex % g_CurMaxInvPhotos ) )
			return nIndex;

		nIndex++;
	}

	return -1;
}

int	CPhotoInventory::AddPhoto( CaptureInfo_t *pInfo )
{
	// Must have room
	if ( NumPhotos() >= g_CurMaxInvPhotos )
		return -1;

	// Find the first free slot
	for ( int i = 0; i < g_CurMaxInvPhotos; i++ )
	{
		// See if this is a free slot
		if ( IsValidPhoto( i ) == false )
		{
			// Copy it
			m_Photos[i] = *pInfo;
			m_nNumPhotos++;
			return i;
		}
	}

	// No valid slot found
	return -1;
}

bool CPhotoInventory::GetPhoto( int nIndex, CaptureInfo_t *pOut )
{
	// Must be valid
	if ( IsValidPhoto( nIndex ) == false )
		return false;

	*pOut = m_Photos[nIndex];
	return true;
}

bool CPhotoInventory::RemovePhoto( int nIndex )
{
	// Make sure it's a valid index
	if ( IsValidPhoto( nIndex ) == false )
		return false;

	// Clear it
	Q_memset( &(m_Photos[nIndex]), NULL, sizeof( CaptureInfo_t ) );
	m_nNumPhotos--;

	return true;
}

void CPhotoInventory::PurgePhotos( void )
{
	m_nNumPhotos = 0;
	Q_memset( m_Photos, NULL, sizeof( CaptureInfo_t ) * g_CurMaxInvPhotos );
}

bool CPhotoInventory::UpdatePhoto( int nIndex, CaptureInfo_t &pIn )
{
	// Make sure it's a valid index
	if ( IsValidPhoto( nIndex ) == false )
		return false;

	m_Photos[nIndex] = pIn;
	return true;
}

void CPhotoInventory::OnRestore( void )
{
	// Fix up placement query pointers on restore.
	for ( int i = 0; i < m_nNumPhotos; ++i )
	{
		CBaseAnimating* pEntAnimating = dynamic_cast<CBaseAnimating*>(m_Photos[i].hCapturedEnt.Get());
		if ( pEntAnimating )
		{
			m_Photos[i].pPlacementQuery = pEntAnimating->Get_CPhotoPlacementQuery();
		}
	}
}

BEGIN_DATADESC_NO_BASE( CPhotoInventory )

	DEFINE_FIELD( m_nNumPhotos, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( m_Photos, MAX_PHOTOS ),

END_DATADESC()




//-----------------------------------------------------------------------------
// Purpose: Whether or not a photo may be added to the inventory
//-----------------------------------------------------------------------------
bool Photo_CanAdd( void )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( pPortalPlayer )
		return pPortalPlayer->m_PhotoInventory.CanAddPhoto();
	else
		return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add a photograph to the player's inventory
// Input  : *pInfo - Information to add
// Output : Index into the list of photos for later reference
//-----------------------------------------------------------------------------
int Photo_Add( CaptureInfo_t *pInfo )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return -1;

	// See if we're even able to add the photograph
	if ( pPortalPlayer->m_PhotoInventory.CanAddPhoto() == false )
		return -1;	

	int nIndex = pPortalPlayer->m_PhotoInventory.AddPhoto( pInfo );
	pPortalPlayer->OnPhotoAdded( nIndex );

	CSingleUserRecipientFilter user( pPortalPlayer );
	user.MakeReliable();
	UserMessageBegin( user, "TakePhoto" );
	WRITE_EHANDLE( pInfo->hCapturedEnt );
	WRITE_BYTE( nIndex );
	MessageEnd();

	CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->OnPhotoAdded( nIndex );
	}

	Msg("Photo added to inventory at index %d\n", nIndex);
	return nIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Get the photo by the index
// Input  : nIndex - Index of the photo we want
//-----------------------------------------------------------------------------
bool Photo_Get( int nIndex, CaptureInfo_t *pOut )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return false;
	else
		return pPortalPlayer->m_PhotoInventory.GetPhoto( nIndex, pOut );
}

//-----------------------------------------------------------------------------
// Purpose: Removes a photo, by index
// Input  : nIndex - Photo index
//-----------------------------------------------------------------------------
bool Photo_Remove( int nIndex )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return false;

	bool bSuccess = pPortalPlayer->m_PhotoInventory.RemovePhoto( nIndex );
	
	pPortalPlayer->OnPhotoRemoved( nIndex );
	
	// Find our next best
	Photo_Cycle( true );

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of photos in the collection
//-----------------------------------------------------------------------------
unsigned int Photo_Count( void )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return 0;
	else
		return pPortalPlayer->m_PhotoInventory.NumPhotos();
}

//-----------------------------------------------------------------------------
// Purpose: Purge all photos in the inventory
//-----------------------------------------------------------------------------
void Photo_Purge( void )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return;

	pPortalPlayer->m_PhotoInventory.PurgePhotos();
}

//-----------------------------------------------------------------------------
// Purpose: Cycle the photo inventory in one direction or another
//-----------------------------------------------------------------------------
void Photo_Cycle( bool bForward )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return;

	pPortalPlayer->m_PhotoInventory.CyclePhotos( bForward );
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether there is a valid photograph in the inventory slot
//-----------------------------------------------------------------------------
bool Photo_IsValid( int nIndex )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return false;
	else
		return pPortalPlayer->m_PhotoInventory.IsValidPhoto( nIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Update the photo data in the inventory
//-----------------------------------------------------------------------------
bool Photo_Update( int nIndex, CaptureInfo_t &pIn )
{
	CPortal_Player* pPortalPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
	Assert( pPortalPlayer );
	if ( !pPortalPlayer )
		return false;
	else
		return pPortalPlayer->m_PhotoInventory.UpdatePhoto( nIndex, pIn );
}
\
