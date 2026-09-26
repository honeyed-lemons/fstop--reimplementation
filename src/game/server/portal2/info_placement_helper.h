//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photos
//
//=====================================================================================//

#ifndef INFO_PLACEMENT_HELPER_H_
#define INFO_PLACEMENT_HELPER_H_
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "photo.h"

class CInfoPlacementHelper : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoPlacementHelper, CPointEntity );
	DECLARE_DATADESC();

	CInfoPlacementHelper();
	virtual void Activate( void );
	virtual void UpdateOnRemove( void );

	const char	*GetTargetClassname( void ) { return STRING( m_strTargetClassname ); }
	float GetTargetRadius( void ) { return m_flRadius; }

	void OnObjectPlaced( CBaseEntity *pObject )
	{
		m_OnObjectPlaced.FireOutput( pObject, this );
	}

	inline bool LimitByTargetSize( void ) { return m_bUseSizeLimiting; }
	inline int GetTargetSize( void ) const { return m_nTargetSize; }

	// return an override target that we should be attaching to instead of just placing in mid air.
	inline CBaseEntity* GetTargetOverride( void )
	{
		if ( m_strTargetEntity == NULL_STRING )
			return NULL;

		return gEntList.FindEntityByName( NULL, STRING( m_strTargetEntity ) );
	}

	inline const Vector &GetTargetOrigin( void )
	{
		if ( m_strTargetProxy == NULL_STRING )
			return GetAbsOrigin();

		CBaseEntity *pProxy = gEntList.FindEntityByName( NULL, STRING( m_strTargetProxy ) );
		if ( pProxy == NULL )
		{
			Warning("Placement proxy entity %s not found!\n", STRING( m_strTargetProxy ) );
			return GetAbsOrigin();
		}

		return pProxy->GetAbsOrigin();
	}

	inline const QAngle &GetTargetAngles( void )
	{
		Assert( m_bSnapToHelperAngles );

		if ( m_strTargetProxy == NULL_STRING )
			return GetAbsAngles();

		CBaseEntity *pProxy = gEntList.FindEntityByName( NULL, STRING( m_strTargetProxy ) );
		if ( pProxy == NULL )
		{
			Warning("Placement proxy entity %s not found!\n", STRING( m_strTargetProxy ) );
			return GetAbsAngles();
		}

		return pProxy->GetAbsAngles();
	}

	bool ShouldHideUntilPlaced( void );
	bool ShouldUseHelperAngles( void );
	inline bool ShouldForcePlacement( void ) { return m_bForcePlacement; };

	void Enable( void ) { m_bDisabled = false; };
	void Disable( void ) { m_bDisabled = true; }
	bool IsEnabled( void ) const { return (m_bDisabled==false); }
	
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	COutputEvent	m_OnObjectPlaced;
	COutputFloat	m_ObjectPlacedSize;

private:
	string_t	m_strTargetClassname;
	string_t	m_strTargetProxy;		// Name of a proxy entity to use for our position
	string_t	m_strTargetEntity;		// Name of a specific entity we are supposed to attach to.
	float		m_flRadius;
	int			m_nTargetSize;			// Target object's size must match this
	bool		m_bUseSizeLimiting;
	bool		m_bHideUntilPlaced;		// Don't show the helper's position in placement preview, just place the item there when the player puts it down.
	bool		m_bSnapToHelperAngles;	// Use the helper's angles for placement
	bool		m_bForcePlacement;		// if set, placing at this helper will not use the normal placement validity rules
	bool		m_bDisabled;
};

struct CaptureInfo_t;
CInfoPlacementHelper* UTIL_FindPlacementHelper( CaptureInfo_t& captureInfo, int nSize, const Vector& vecEndPoint );
CInfoPlacementHelper* UTIL_FindPlacementHelper( const Vector& vecEndPoint );
void UTIL_ClearPlacementHelpers( void );


#endif // INFO_PLACEMENT_HELPER_H_
