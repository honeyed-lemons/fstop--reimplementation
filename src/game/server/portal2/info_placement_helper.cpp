//
//
//

#include "info_placement_helper.h"
#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_INFO_PLACEMENT_LIMIT_BY_SIZE		(1<<0)	// NOTE: This is now obsolete - use the keyvalue instead!

ConVar sv_show_placement_help_in_preview( "sv_show_placement_help_in_preview", "0", FCVAR_NONE, "Forces the placement preview to show any help in placement given from info_placement_helper entities.\n" );

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( info_placement_helper, CInfoPlacementHelper );

BEGIN_DATADESC( CInfoPlacementHelper )
	DEFINE_KEYFIELD( m_strTargetClassname, FIELD_STRING, "target_classname" ),
	DEFINE_KEYFIELD( m_strTargetProxy, FIELD_STRING, "proxy_name" ),
	DEFINE_KEYFIELD( m_strTargetEntity, FIELD_STRING, "attach_target_name" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_nTargetSize, FIELD_INTEGER, "target_size" ),
	DEFINE_KEYFIELD( m_bUseSizeLimiting, FIELD_BOOLEAN, "usesizelimit" ),
	DEFINE_KEYFIELD( m_bHideUntilPlaced, FIELD_BOOLEAN, "hide_until_placed" ),
	DEFINE_KEYFIELD( m_bSnapToHelperAngles, FIELD_BOOLEAN, "snap_to_helper_angles" ),
	DEFINE_KEYFIELD( m_bForcePlacement, FIELD_BOOLEAN, "force_placement" ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	DEFINE_OUTPUT( m_OnObjectPlaced, "OnObjectPlaced" ),
	DEFINE_OUTPUT( m_ObjectPlacedSize, "OnObjectPlacedSize" )
END_DATADESC()

//-----------------------------------------------------------------------------

class CInfoPlacementManager
{
public:
	
	void Purge( void )
	{
		m_PlacementHelpers.Purge();
	}

	void AddPlacementHelper( CInfoPlacementHelper *pHelper )
	{
		// If we already have this, don't bother adding it again
		if ( m_PlacementHelpers.Find( pHelper ) != m_PlacementHelpers.InvalidIndex() )
			return;

		m_PlacementHelpers.AddToTail( pHelper );
	}
	
	void RemovePlacementHelper( CInfoPlacementHelper *pHelper )
	{
		int nIndex = m_PlacementHelpers.Find( pHelper );
		if ( nIndex == m_PlacementHelpers.InvalidIndex() )
			return;

		m_PlacementHelpers.Remove( nIndex );
	}

	CInfoPlacementHelper *FindPlacementHelper( const Vector &vecOrigin )
	{
		// This query only makes sense with a viewer (ie. the player)
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer == NULL )
			return NULL;

		Vector vecEyeDir = pPlayer->EyeDirection3D();
		Vector vecEyePos = pPlayer->EyePosition();

		CInfoPlacementHelper *pBestHelper = NULL;
		float flBestDist  = 1e18f;

		// while( nStartIndex != m_PlacementHelpers.InvalidIndex() )
		for ( int i = 0; i < m_PlacementHelpers.Count(); i++ )
		{
			// First, see if we can access this member
			CInfoPlacementHelper *pHelper = m_PlacementHelpers[i];
			if ( pHelper == NULL )
				continue;

			// Don't bother if they're currently disabled
			if ( pHelper->IsEnabled() == false )
				continue;

			Vector vecTargetDir = ( pHelper->GetAbsOrigin() - vecEyePos );
			float flTargetDist = VectorNormalize( vecTargetDir );

			float flToleranceAngle = atan2( pHelper->GetTargetRadius(), flTargetDist ); // angle of the max deflection
			float flTargetAngle = acos( DotProduct( vecEyeDir, vecTargetDir ) ); // Angle whose cosine is eye's deflection

			// If the angle of deflection of the eye is greater than our tolerance, we're outside the influence of this helper
			if ( flTargetAngle > flToleranceAngle )
				continue;

			// Choose the closest placement helper if multiple pass the above tests.
			if ( flTargetDist > flBestDist )
				continue;

			// Everything passed, so take this
			pBestHelper = pHelper;
			flBestDist = flTargetDist;
		}

		return pBestHelper;
	}

	CInfoPlacementHelper *FindPlacementHelper( const char *lpszClassname, const char *lpszEntityName, int nSize, const Vector &vecOrigin )
	{
		// This query only makes sense with a viewer (ie. the player)
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer == NULL )
			return NULL;

		Vector vecEyeDir = pPlayer->EyeDirection3D();
		Vector vecEyePos = pPlayer->EyePosition();

		CInfoPlacementHelper *pBestHelper = NULL;
		float flBestDist  = 1e18f;

		// while( nStartIndex != m_PlacementHelpers.InvalidIndex() )
		for ( int i = 0; i < m_PlacementHelpers.Count(); i++ )
		{
			// First, see if we can access this member
			CInfoPlacementHelper *pHelper = m_PlacementHelpers[i];
			if ( pHelper == NULL )
				continue;

			// Don't bother if they're currently disabled
			if ( pHelper->IsEnabled() == false )
				continue;

			// If we've specified a targetname, then it must match
			const char *lpszTargetName = pHelper->GetTargetClassname();
			if ( lpszClassname != NULL && lpszClassname[0] != NULL && lpszTargetName != NULL && lpszTargetName[0] != NULL )
			{
				// First check for a classname match (most typical use)
				if ( Q_stricmp( lpszClassname, lpszTargetName ) )
				{
					// Next, check to see if we were using our entity name
					if ( Q_stricmp( lpszEntityName, lpszTargetName ) )
						continue;
				}
			}

			// If we're limiting by size, then screen by it
			if ( pHelper->LimitByTargetSize() && pHelper->GetTargetSize() != nSize )
				continue;

			Vector vecTargetDir = ( pHelper->GetAbsOrigin() - vecEyePos );
			float flTargetDist = VectorNormalize( vecTargetDir );

			float flToleranceAngle = atan2( pHelper->GetTargetRadius(), flTargetDist ); // angle of the max deflection
			float flTargetAngle = acos( DotProduct( vecEyeDir, vecTargetDir ) ); // Angle whose cosine is eye's deflection

			// If the angle of deflection of the eye is greater than our tolerance, we're outside the influence of this helper
			if ( flTargetAngle > flToleranceAngle )
				continue;
			
			// Choose the closest placement helper if multiple pass the above tests.
			if ( flTargetDist > flBestDist )
				continue;

			// don't let placement helpers pull objects through solid things
			if ( !pHelper->ShouldForcePlacement() )
			{
				Ray_t ray;
				ray.Init( pPlayer->EyePosition(), pHelper->GetTargetOrigin() );
				trace_t tr;
				UTIL_Portal_TraceRay( ray, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr, false );

				// Bail if we hit something that blocks placement
				if ( tr.DidHit() )
				{
					float flHitDist = tr.fraction * ray.m_Delta.Length();

					if ( flHitDist < flTargetDist )
						continue;
				}
			}

			// Everything passed, so take this
			pBestHelper = pHelper;
			flBestDist = flTargetDist;
		}

		return pBestHelper;
	}

private:
	CUtlVector <CInfoPlacementHelper *>	m_PlacementHelpers;
};

// Global singleton
static CInfoPlacementManager g_PlacementManager;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CInfoPlacementHelper* UTIL_FindPlacementHelper( CaptureInfo_t& captureInfo, int nSize, const Vector &vecOrigin )
{
	return g_PlacementManager.FindPlacementHelper( captureInfo.hCapturedEnt->GetClassname(), STRING( captureInfo.hCapturedEnt->GetEntityName() ), nSize, vecOrigin );
}

CInfoPlacementHelper* UTIL_FindPlacementHelper( const Vector& vecEndPoint )
{
	return g_PlacementManager.FindPlacementHelper( vecEndPoint );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void UTIL_ClearPlacementHelpers( void )
{
	g_PlacementManager.Purge();
}

CInfoPlacementHelper::CInfoPlacementHelper():
m_bHideUntilPlaced( true ),
m_bSnapToHelperAngles( true ),
m_bForcePlacement ( false )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CInfoPlacementHelper::Activate( void )
{
	BaseClass::Activate();

	// Bring the old legacy flag forward into the better keyvalue way
	if ( HasSpawnFlags( SF_INFO_PLACEMENT_LIMIT_BY_SIZE ) )
	{
		m_bUseSizeLimiting = true;
	}

	g_PlacementManager.AddPlacementHelper( this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CInfoPlacementHelper::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	g_PlacementManager.RemovePlacementHelper( this );
}

bool CInfoPlacementHelper::ShouldHideUntilPlaced( void )
{
	if ( sv_show_placement_help_in_preview.GetBool() )
	{
		return false;
	}
	else
	{
		return m_bHideUntilPlaced;
	}
}

bool CInfoPlacementHelper::ShouldUseHelperAngles( void )
{
	return m_bSnapToHelperAngles;
}

void CInfoPlacementHelper::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CInfoPlacementHelper::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}
