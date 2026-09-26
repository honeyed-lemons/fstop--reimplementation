//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose:
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "triggers.h"
#include "props.h"
#include "photo.h"
#include "phys_controller.h"
#include "physics.h"
#include "vphysics/constraints.h"
#include "saverestore_utlvector.h"
#include "physics_saverestore.h"
#include "decals.h"
#include "bone_setup.h"

#define	POSITIVE_SKIN	0
#define	NEGATIVE_SKIN	1

#define MONOPOLE_MODEL_WIDTH 24
#define MONOPOLE_MODEL_OFFSET 12

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: A magnet that creates constraints between itself and anything it touches 
//-----------------------------------------------------------------------------

struct attached_objects_t
{
	IPhysicsConstraint *pConstraint;
	EHANDLE			   hEntity;

	DECLARE_SIMPLE_DATADESC();
};

class CMonopole : public CBaseAnimating, public IPhysicsConstraintEvent
{
	DECLARE_CLASS( CMonopole, CBaseAnimating );
public:
	DECLARE_DATADESC();

	CMonopole();
	~CMonopole();

	void	Spawn( void );
	void	Precache( void );
	void	Touch( CBaseEntity *pOther );
	void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void	DoMagnetSuck( CBaseEntity *pOther );
	void	SetConstraintGroup( IPhysicsConstraintGroup *pGroup );

	bool	IsOn( void ) { return m_bActive; }
	int		GetNumAttachedObjects( void );
	float	GetTotalMassAttachedObjects( void );
	CBaseEntity *GetAttachedObject( int iIndex );

	// Checking for hitting something
	void	ResetHasHitSomething( void ) { m_bHasHitSomething = false; }
	bool	HasHitSomething( void ) { return m_bHasHitSomething; }

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputTurnOn( inputdata_t &inputdata );
	void	InputTurnOff( inputdata_t &inputdata );

	void	InputTogglePolarity( inputdata_t &inputdata );
	void	InputTurnPositive( inputdata_t &inputdata );
	void	InputTurnNegative( inputdata_t &inputdata );

	void	InputConstraintBroken( inputdata_t &inputdata );

	void	DetachAll( void );
	void	ValidateConstraints( void );

	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );
		virtual float GetMaxPlacementDistance( void );
		virtual float GetPlacementHelperOffset(	CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

	virtual void	OnCaptured( void );
	virtual void	OnReleased( void );

	// think
	void	SuckThink( void );


// IPhysicsConstraintEvent
public:
	void	ConstraintBroken( IPhysicsConstraint *pConstraint );
	
protected:
	// Outputs
	COutputEvent	m_OnMagnetAttach;
	COutputEvent	m_OnMagnetDetach;

	// Keys
	float			m_massScale;
	string_t		m_iszOverrideScript;
	float			m_forceLimit;
	float			m_torqueLimit;

	CUtlVector< attached_objects_t >	m_MagnettedEntities;
	IPhysicsConstraintGroup				*m_pConstraintGroup;

	bool			m_bActive;
	bool			m_bHasHitSomething;
	float			m_flTotalMass;
	int				m_iMaxObjectsAttached;
	bool			m_bPositive;
};

//============================================================================================================
// PHYS MAGNET
//============================================================================================================
LINK_ENTITY_TO_CLASS( prop_monopole, CMonopole );

const char MAGNET_MODEL_NAME[] = "models/flag/briefcase.mdl";

// BUGBUG: This won't work!  Right now you can't save physics pointers inside an embedded type!
BEGIN_SIMPLE_DATADESC( attached_objects_t )

	DEFINE_PHYSPTR( pConstraint ),
	DEFINE_FIELD( hEntity,	FIELD_EHANDLE	),

END_DATADESC()

BEGIN_DATADESC( CMonopole )
	// Outputs
	DEFINE_OUTPUT( m_OnMagnetAttach, "OnAttach" ),
	DEFINE_OUTPUT( m_OnMagnetDetach, "OnDetach" ),

	// Keys
	DEFINE_KEYFIELD( m_massScale, FIELD_FLOAT, "massScale" ),
	DEFINE_KEYFIELD( m_iszOverrideScript, FIELD_STRING, "overridescript" ),
	DEFINE_KEYFIELD( m_iMaxObjectsAttached, FIELD_INTEGER, "maxobjects" ),
	DEFINE_KEYFIELD( m_forceLimit, FIELD_FLOAT, "forcelimit" ),
	DEFINE_KEYFIELD( m_torqueLimit, FIELD_FLOAT, "torquelimit" ),

	DEFINE_KEYFIELD( m_bActive, FIELD_BOOLEAN,	"StartActive" ),
	DEFINE_KEYFIELD( m_bPositive, FIELD_BOOLEAN, "StartPositive" ),

	DEFINE_UTLVECTOR( m_MagnettedEntities, FIELD_EMBEDDED ),
	DEFINE_PHYSPTR( m_pConstraintGroup ),

	DEFINE_FIELD( m_bHasHitSomething, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flTotalMass, FIELD_FLOAT ),

	DEFINE_THINKFUNC( SuckThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TogglePolarity", InputTogglePolarity ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnPositive", InputTurnPositive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnNegative", InputTurnNegative ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMonopole::CMonopole( void )
{
	m_forceLimit = 0;
	m_torqueLimit = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMonopole::~CMonopole( void )
{
	DetachAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	SetModel( MAGNET_MODEL_NAME );

	solid_t tmpSolid;
	PhysModelParseSolid( tmpSolid, this, GetModelIndex() );
	if ( m_massScale > 0 )
	{
		tmpSolid.params.mass *= m_massScale;
	}
	PhysSolidOverride( tmpSolid, m_iszOverrideScript );
	VPhysicsInitNormal( GetSolid(), GetSolidFlags(), true, &tmpSolid );

	VPhysicsGetObject()->Wake();
	VPhysicsGetObject()->EnableMotion( false );

	m_bPositive = true;
	m_bActive = true;
	m_pConstraintGroup = NULL;
	m_flTotalMass = 0;

	BaseClass::Spawn();

	SetThink( &CMonopole::SuckThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::Precache( void )
{
	PrecacheModel( MAGNET_MODEL_NAME );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::Touch( CBaseEntity *pOther )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::SuckThink( void )
{
	DoMagnetSuck( NULL );

	SetThink( &CMonopole::SuckThink );
	SetNextThink( gpGlobals->curtime + 0.05f );

	ValidateConstraints();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];

	// Ignore triggers
	if ( pOther->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
		return;

	m_bHasHitSomething = true;
	DoMagnetSuck( pEvent->pEntities[!index] );

	// Don't pickup if we're not active
	if ( !m_bActive )
		return;

	// Hit our maximum?
	if ( m_iMaxObjectsAttached && m_iMaxObjectsAttached <= GetNumAttachedObjects() )
		return;

	// Make sure it's made of metal
	const surfacedata_t *phit = physprops->GetSurfaceData( pEvent->surfaceProps[otherIndex] );
	char cTexType = phit->game.material;
	if ( cTexType != CHAR_TEX_METAL && cTexType != CHAR_TEX_COMPUTER )
	{
		// If we don't have a model, we're done. The texture we hit wasn't metal.
		if ( !pOther->GetBaseAnimating() )
			return;

		// If we have a model that wants to be metal, even though we hit a non-metal texture, we'll stick to it
		if ( Q_strncmp( Studio_GetDefaultSurfaceProps( pOther->GetBaseAnimating()->GetModelPtr() ), "metal", 5 ) )
			return;
	}

	IPhysicsObject *pPhysics = pOther->VPhysicsGetObject();
	if ( pPhysics && pOther->GetMoveType() == MOVETYPE_VPHYSICS && pPhysics->IsMoveable() )
	{
		// Make sure we haven't already got this sucker on the magnet
		int iCount = m_MagnettedEntities.Count();
		for ( int i = 0; i < iCount; i++ )
		{
			if ( m_MagnettedEntities[i].hEntity == pOther )
				return;
		}

		// Create a constraint between the magnet and this sucker
		IPhysicsObject *pMagnetPhysObject = VPhysicsGetObject();
		Assert( pMagnetPhysObject );

		attached_objects_t newEntityOnMagnet;
		newEntityOnMagnet.hEntity = pOther;

		{
			constraint_fixedparams_t fixed;
			fixed.Defaults();
			fixed.InitWithCurrentObjectState( pMagnetPhysObject, pPhysics );
			fixed.constraint.Defaults();
			fixed.constraint.forceLimit = lbs2kg(m_forceLimit);
			fixed.constraint.torqueLimit = lbs2kg(m_torqueLimit);

			newEntityOnMagnet.pConstraint = physenv->CreateFixedConstraint( pMagnetPhysObject, pPhysics, NULL, fixed );
		}

		newEntityOnMagnet.pConstraint->SetGameData( (void *) this );
		m_MagnettedEntities.AddToTail( newEntityOnMagnet );

		m_flTotalMass += pPhysics->GetMass();
	}

	DoMagnetSuck( pOther );

	m_OnMagnetAttach.FireOutput( this, this );

	BaseClass::VPhysicsCollision( index, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::DoMagnetSuck( CBaseEntity *pOther )
{
	if ( !m_bActive )
		return;

	// Look for physics objects underneath the magnet and suck them onto it
	Vector vecCheckPos, vecSuckPoint;
	VectorTransform( Vector(0,0,0), EntityToWorldTransform(), vecCheckPos );
	VectorTransform( Vector(0,0,0), EntityToWorldTransform(), vecSuckPoint );

	CBaseEntity *pEntities[20];
	int iNumEntities = UTIL_EntitiesInSphere( pEntities, 20, vecCheckPos, 150.0 * GetModelScale(), 0 );
	for ( int i = 0; i < iNumEntities; i++ )
	{
		CBaseEntity *pEntity = pEntities[i];
		if ( !pEntity || pEntity == pOther || pEntity == this )
			continue;

		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if ( pPhys && pEntity->GetMoveType() == MOVETYPE_VPHYSICS && pPhys->GetMass() < 5000 )
		{
			// Make sure it's made of metal
			const surfacedata_t *phit = physprops->GetSurfaceData( pPhys->GetMaterialIndex() );
			char cTexType = phit->game.material;
			if ( cTexType != CHAR_TEX_METAL && cTexType != CHAR_TEX_COMPUTER )
			{
				// If we don't have a model, we're done. The texture we hit wasn't metal.
				if ( !pEntity->GetBaseAnimating() )
					continue;

				// If we have a model that wants to be metal, even though we hit a non-metal texture, we'll stick to it
				if ( Q_strncmp( Studio_GetDefaultSurfaceProps( pEntity->GetBaseAnimating()->GetModelPtr() ), "metal", 5 ) )
					continue;
			}

			{
				// Pull it towards the magnet
				Vector vecVelocity = (GetAbsOrigin() - pEntity->GetAbsOrigin());
				float flDist = vecVelocity.Length();
				
				if ( flDist > 0.0f )
				{
					float flDirection = 1.0f;
					
					// FIXME: need a better way to tag an item with it's polarity.
					bool bEntityNegative = Q_strcmp( STRING( pEntity->GetModelName() ), "models/props/metal_box.mdl" ) == 0;

					if ( bEntityNegative && !m_bPositive )
						flDirection = -1.0f;
					
					VectorNormalize(vecVelocity);
					// nasty force equation. This is just about looks, not about accuracy.
					vecVelocity *= flDirection * (50.f + 0.25f*pPhys->GetMass()) * GetModelScale() / ( (0.05f + flDist/100.f) * (0.05f + flDist/100.f) );
					pPhys->AddVelocity( &vecVelocity, NULL );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::SetConstraintGroup( IPhysicsConstraintGroup *pGroup )
{
	m_pConstraintGroup = pGroup;
}

//-----------------------------------------------------------------------------
// Purpose: Make the magnet active
//-----------------------------------------------------------------------------
void CMonopole::InputTurnOn( inputdata_t &inputdata )
{
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: Make the magnet inactive. Drop everything it's got hooked on.
//-----------------------------------------------------------------------------
void CMonopole::InputTurnOff( inputdata_t &inputdata )
{
	m_bActive = false;
	DetachAll();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the magnet's active state
//-----------------------------------------------------------------------------
void CMonopole::InputToggle( inputdata_t &inputdata )
{
	if ( m_bActive )
	{
		InputTurnOff( inputdata );
	}
	else
	{
		InputTurnOn( inputdata );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make to polarity positive
//-----------------------------------------------------------------------------
void CMonopole::InputTurnPositive( inputdata_t &inputdata )
{
	m_nSkin = POSITIVE_SKIN;
	m_bPositive = true;
	DetachAll();
}

//-----------------------------------------------------------------------------
// Purpose: Make the polarity negative
//-----------------------------------------------------------------------------
void CMonopole::InputTurnNegative( inputdata_t &inputdata )
{
	m_nSkin = NEGATIVE_SKIN;
	m_bPositive = false;
	DetachAll();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the magnet's polarity
//-----------------------------------------------------------------------------
void CMonopole::InputTogglePolarity( inputdata_t &inputdata )
{
	if ( m_bPositive )
	{
		InputTurnNegative( inputdata );
	}
	else
	{
		InputTurnPositive( inputdata );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make sure none of our attached objects are in stasis
//-----------------------------------------------------------------------------
void CMonopole::ValidateConstraints( void )
{
	// Find the entity that was constrained and release it
	int iCount = m_MagnettedEntities.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_MagnettedEntities[i].hEntity.Get() != NULL && m_MagnettedEntities[i].hEntity->IsInStasis() )
		{
			IPhysicsObject *pPhysObject = m_MagnettedEntities[i].hEntity->VPhysicsGetObject();

			if( pPhysObject != NULL )
			{
				m_flTotalMass -= pPhysObject->GetMass();
			}

			// kill this constraint.
			physenv->DestroyConstraint( m_MagnettedEntities[i].pConstraint );

			m_OnMagnetDetach.FireOutput( this, this );
			m_MagnettedEntities.Remove(i);

			// we deleted this element, so check here again.
			i--; iCount--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: One of our magnet constraints broke
//-----------------------------------------------------------------------------
void CMonopole::ConstraintBroken( IPhysicsConstraint *pConstraint )
{
	// Find the entity that was constrained and release it
	int iCount = m_MagnettedEntities.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_MagnettedEntities[i].hEntity.Get() != NULL && m_MagnettedEntities[i].pConstraint == pConstraint )
		{
			IPhysicsObject *pPhysObject = m_MagnettedEntities[i].hEntity->VPhysicsGetObject();

			if( pPhysObject != NULL )
			{
				m_flTotalMass -= pPhysObject->GetMass();
			}

			m_MagnettedEntities.Remove(i);
			break;
		}
	}

	m_OnMagnetDetach.FireOutput( this, this );

	physenv->DestroyConstraint( pConstraint  );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopole::DetachAll( void )
{
	// Make sure we haven't already got this sucker on the magnet
	int iCount = m_MagnettedEntities.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		physenv->DestroyConstraint( m_MagnettedEntities[i].pConstraint  );
	}

	m_MagnettedEntities.Purge();
	m_flTotalMass = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMonopole::GetNumAttachedObjects( void )
{
	return m_MagnettedEntities.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CMonopole::GetTotalMassAttachedObjects( void )
{
	return m_flTotalMass;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CMonopole::GetAttachedObject( int iIndex )
{
	Assert( iIndex < GetNumAttachedObjects() );

	return m_MagnettedEntities[iIndex].hEntity;
}

//------------------------------------------------------------------------------
// Placement query
//------------------------------------------------------------------------------

bool CMonopole::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																	CheckPlacementData_t &placementData,
																	Vector &positionOut,
																	QAngle &anglesOut )
{
	return WallPlacement( MONOPOLE_MODEL_WIDTH * placementData.fScale, 
		MONOPOLE_MODEL_WIDTH * placementData.fScale, 
		MONOPOLE_MODEL_OFFSET * placementData.fScale, 
		captureInfo, placementData, positionOut, anglesOut );
}

float CMonopole::CPhotoPlacementQuery::GetPlacementHelperOffset( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData )
{
	return MONOPOLE_MODEL_OFFSET * placementData.fScale;
}

CameraInfo_ScaleData_t *CMonopole::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 0.75f, 1.0f, 1.33f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

float CMonopole::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	return 20.0f * 12.0f;
}


void CMonopole::OnCaptured( void )
{
	DetachAll();
	BaseClass::OnCaptured();
}

void CMonopole::OnReleased( void )
{
	BaseClass::OnReleased();
}