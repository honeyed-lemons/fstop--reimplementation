//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Spawn and use functions for editor-placed triggers.
//
//===========================================================================//

#include "cbase.h"
#include "triggers.h"
#include "portal_player.h"
#include "ai_basenpc.h"
#include "props.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "physics_saverestore.h"

bool g_bAllOnReleasedChainedToBase;

bool UTIL_FizzlePlayerPhotos( CPortal_Player *pPlayer )
{
	// Must have photos to bother
	if ( Photo_Count() == 0 )
		return false;

	CaptureInfo_t captureInfo;

	bool bFizzleOccurred = false;
	// FIXME: Need better accessor
	for ( int i = 0; i < 3; i++ )
	{
		if ( Photo_Get( i, &captureInfo ) )
		{
			bFizzleOccurred = true;

			// Recreate the object
			CBaseEntity *pFizzledObject = UTIL_RestoreCapturedObject( captureInfo, captureInfo.vecOldOrigin, captureInfo.vecOldAngles, captureInfo.nOldScaleLevel );
			if ( pFizzledObject )
			{
				CBaseAnimating *pAnim = pFizzledObject->GetBaseAnimating();
				if ( pAnim )
				{
					pAnim->OnFizzled();
				}
			}
		}
	}

	// FIXME: Temp masking effect
	color32 white = { 255, 255, 255, 255 };
	UTIL_ScreenFade( pPlayer, white, 0.25f, 0.0f, FFADE_IN );

	// Make them all go away!
	pPlayer->StripPhotos();

	return bFizzleOccurred;
}

class CTriggerPhotoEraser : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerPhotoEraser, CBaseTrigger );
	virtual int	ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual void Spawn( void )
	{
		BaseClass::Spawn();

		// Don't let the camera shoot through us!
		InitTrigger();

		if ( m_bDisabled == false )
		{
			// We want to hit camera traces!
			SetCollisionGroup( COLLISION_GROUP_CAMERA_SOLID );
			RemoveSolidFlags( FSOLID_NOT_SOLID ); // HACK: We want camera traces to hit this!
		}
	}

	virtual void Touch( CBaseEntity *pOther )
	{
		// We only touch players
		if ( pOther == NULL || pOther->IsPlayer() == false )
			return;

		// Must be enabled
		if ( m_bDisabled )
			return;

		CPortal_Player *pPlayer = (CPortal_Player *) ToBasePlayer( pOther );
		
		if ( UTIL_FizzlePlayerPhotos( pPlayer ) )
		{
			// Fire off the output
			m_OnObjectsFizzled.FireOutput( pPlayer, this );
		}
	}
	
	virtual void Enable( void )
	{
		SetCollisionGroup( COLLISION_GROUP_CAMERA_SOLID );
		RemoveSolidFlags( FSOLID_NOT_SOLID ); // HACK: We want camera traces to hit this!
	}

	virtual void Disable( void )
	{
		SetCollisionGroup( COLLISION_GROUP_NONE );
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	DECLARE_DATADESC();

protected:
	COutputEvent	m_OnObjectsFizzled;
};

BEGIN_DATADESC( CTriggerPhotoEraser )
	DEFINE_OUTPUT( m_OnObjectsFizzled, "OnObjectsFizzled" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_photo_eraser, CTriggerPhotoEraser );



//-----------------------------------------------------------------------------
// Purpose: Replace the object in the world
//-----------------------------------------------------------------------------

CBaseEntity *UTIL_RestoreCapturedObject( CaptureInfo_t captureInfo, const Vector &vecPoint, const QAngle &vecAngles, int nScaleLevel, CInfoPlacementHelper *pHelper )
{
	CBaseEntity* pEnt = captureInfo.hCapturedEnt;
	if ( pEnt == NULL )
	{
		Assert( pEnt != NULL );
		Warning( "Tried to restore NULL object.  Object was most likely destroyed when in the camera's inventory!\n" );
		return NULL;
	}

	CBaseAnimating* pEntAnimating = (CBaseAnimating*)pEnt;
	Assert ( pEntAnimating );

	if ( !pEntAnimating )
		return NULL;

	Vector vecPlacementOrigin = vecPoint;
	QAngle vecPlacementAngles = vecAngles;

	if ( pHelper )
	{
		vecPlacementOrigin = pHelper->GetTargetOrigin();

		if ( pHelper->ShouldUseHelperAngles() )
		{
			vecPlacementAngles = pHelper->GetTargetAngles();
		}

		pHelper->m_OnObjectPlaced.FireOutput( captureInfo.hCapturedEnt, UTIL_GetLocalPlayer() );
		pHelper->m_ObjectPlacedSize.Set( nScaleLevel, captureInfo.hCapturedEnt, UTIL_GetLocalPlayer()  );
	}

	pEnt->SetStasis( false );
	pEnt->Teleport( &vecPlacementOrigin, &vecPlacementAngles, &captureInfo.vecVelocity );

	

	float flModelScale = 1.0f;
	if ( captureInfo.pPlacementQuery )
	{
		flModelScale = captureInfo.pPlacementQuery->GetScaleForStep( nScaleLevel, &captureInfo );
	}

	CAI_BaseNPC* pNPCPointer = dynamic_cast<CAI_BaseNPC*>(pEnt->MyCombatCharacterPointer());
	if ( pNPCPointer )
	{
		pNPCPointer->SetHullSizeNormal( true );
	}

	// Restore the angular and spatial velocity
	IPhysicsObject *pObject = pEntAnimating->VPhysicsGetObject();
	if ( pObject && pObject->IsMoveable() )
	{
		pObject->SetVelocityInstantaneous( &captureInfo.vecVelocity, &captureInfo.vecAngVelocity );
		pObject->Wake();
	}

	// Find the amount we need to scale to reach our new desired size, starting from our old one
	Assert( pEntAnimating->m_flModelScale > 0 );
	if ( flModelScale != pEntAnimating->GetModelScale() )
	{
		UTIL_CreateScaledPhysObject( pEntAnimating, flModelScale );

		// Let the object know how large it is now
		pEntAnimating->SetModelScale( flModelScale );
		pEntAnimating->SetObjectScaleLevel( nScaleLevel );
	}

	if ( pNPCPointer )
	{
		pNPCPointer->SetHullSizeNormal( true );
	}
	
	g_bAllOnReleasedChainedToBase = false;
	pEntAnimating->OnReleased();

	// NOTE: If you're here, you forgot to chain to the base class in an OnReleased implementation! 
	Assert( g_bAllOnReleasedChainedToBase );
	
	return pEnt;
}



//-----------------------------------------------------------------------------
// Purpose: Scale the object to a new size, taking its render verts and physical verts into account
//-----------------------------------------------------------------------------
bool UTIL_CreateScaledPhysObject( CBaseAnimating *pInstance, float flScale )
{
	// Don't scale NPCs
	if ( pInstance->MyCombatCharacterPointer() )
		return false;

	// FIXME: This needs to work for ragdolls!

	// Get our object
	IPhysicsObject *pObject = pInstance->VPhysicsGetObject();
	if ( pObject == NULL )
	{
		AssertMsg( 0, "UTIL_CreateScaledPhysObject: Failed to scale physics for object-- It has no physics." );
		return false;
	}

	// See if our current physics object is motion disabled
	bool bWasMotionDisabled = ( pObject->IsMotionEnabled() == false );
	bool bWasStatic			= ( pObject->IsStatic() );

	vcollide_t *pCollide = modelinfo->GetVCollide( pInstance->GetModelIndex() );
	if ( pCollide == NULL || pCollide->solidCount == 0 )
		return NULL;

	CPhysCollide *pNewCollide = pCollide->solids[0];	// FIXME: Needs to iterate over the solids

	if ( flScale != 1.0f )
	{
		// Create a query to get more information from the collision object
		ICollisionQuery *pQuery = physcollision->CreateQueryModel( pCollide->solids[0] );	// FIXME: This should iterate over all solids!
		if ( pQuery == NULL )
			return false;

		// Create a container to hold all the convexes we'll create
		const int nNumConvex = pQuery->ConvexCount();
		CPhysConvex **pConvexes = (CPhysConvex **) stackalloc( sizeof(CPhysConvex *) * nNumConvex );

		// For each convex, collect the verts and create a convex from it we'll retain for later
		for ( int i = 0; i < nNumConvex; i++ )
		{
			int nNumTris = pQuery->TriangleCount( i );
			int nNumVerts = nNumTris * 3;
			// FIXME: Really?  stackalloc?
			Vector *pVerts = (Vector *) stackalloc( sizeof(Vector) * nNumVerts );
			Vector **ppVerts = (Vector **) stackalloc( sizeof(Vector *) * nNumVerts );
			for ( int j = 0; j < nNumTris; j++ )
			{
				// Get all the verts for this triangle and scale them up
				pQuery->GetTriangleVerts( i, j, pVerts+(j*3) );
				*(pVerts+(j*3)) *= flScale;
				*(pVerts+(j*3)+1) *= flScale;
				*(pVerts+(j*3)+2) *= flScale;

				// Setup our pointers (blech!)
				*(ppVerts+(j*3)) = pVerts+(j*3);
				*(ppVerts+(j*3)+1) = pVerts+(j*3)+1;
				*(ppVerts+(j*3)+2) = pVerts+(j*3)+2;
			}

			// Convert it back to a convex
			pConvexes[i] = physcollision->ConvexFromVerts( ppVerts, nNumVerts );
			Assert( pConvexes[i] != NULL );
			if ( pConvexes[i] == NULL )
				return false;
		}

		// Clean up
		physcollision->DestroyQueryModel( pQuery );

		// Create a collision model from all the convexes
		pNewCollide = physcollision->ConvertConvexToCollide( pConvexes, nNumConvex );
		if ( pNewCollide == NULL )
			return false;
	}

	// Get our solid info
	solid_t tmpSolid;
	if ( !PhysModelParseSolidByIndex( tmpSolid, pInstance, pInstance->GetModelIndex(), -1 ) )
		return false;

	// Physprops get keyvalues that effect the mass, this block is to respect those fields when we scale
	CPhysicsProp *pPhysInstance = dynamic_cast<CPhysicsProp*>( pInstance );
	if ( pPhysInstance )
	{
		if ( pPhysInstance->GetMassScale() > 0 )
		{
			tmpSolid.params.mass *= pPhysInstance->GetMassScale();
		}

		PhysSolidOverride( tmpSolid, pPhysInstance->GetPhysOverrideScript() );
	}			

	// Scale our mass up as well
	tmpSolid.params.mass *= flScale;
	tmpSolid.params.volume = physcollision->CollideVolume( pNewCollide );

	// Get our surface prop info
	int surfaceProp = -1;
	if ( tmpSolid.surfaceprop[0] )
	{
		surfaceProp = physprops->GetSurfaceIndex( tmpSolid.surfaceprop );
	}

	// Now put it all back (phew!)
	IPhysicsObject *pNewObject = NULL;
	if ( bWasStatic )
	{
		pNewObject = physenv->CreatePolyObjectStatic( pNewCollide, surfaceProp, pInstance->GetAbsOrigin(), pInstance->GetAbsAngles(), &tmpSolid.params );
	}
	else
	{
		pNewObject = physenv->CreatePolyObject( pNewCollide, surfaceProp, pInstance->GetAbsOrigin(), pInstance->GetAbsAngles(), &tmpSolid.params );
	}
	Assert( pNewObject );

	pInstance->VPhysicsDestroyObject();
	pInstance->VPhysicsSetObject( pNewObject );

	// Increase our model bounds
	const model_t *pModel = modelinfo->GetModel( pInstance->GetModelIndex() );
	if ( pModel )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( pModel, mins, maxs );
		pInstance->SetCollisionBounds( mins*flScale, maxs*flScale );
	}

	// Scale the base model as well
	pInstance->SetModelScale( flScale );

	if ( pInstance->GetParent() )
	{
		pNewObject->SetShadow( 1e4, 1e4, false, false );
		pNewObject->UpdateShadow( pInstance->GetAbsOrigin(), pInstance->GetAbsAngles(), false, 0 );
	}

	if ( bWasMotionDisabled )
	{
		pNewObject->EnableMotion( false );
	}
	else
	{
		// Make sure we start awake!
		pNewObject->Wake();
	}

	// Blargh
	pInstance->SetScaledPhysics( ( flScale != 1.0f ) ? pNewObject : NULL );

	return true;
}
