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
#include "collisionproperty.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar	showtriggers;

//-----------------------------------------------------------------------------
// Purpose: A magnetic surface, charge can be modified to push or pull.
//-----------------------------------------------------------------------------

//struct attached_objects_t
//{
//	IPhysicsConstraint *pConstraint;
//	EHANDLE			   hEntity;
//
//	DECLARE_SIMPLE_DATADESC();
//};

class CMonopoleField : public CBaseEntity
{
	DECLARE_CLASS( CMonopoleField, CBaseEntity );
public:
	DECLARE_DATADESC();

	CMonopoleField();
	~CMonopoleField();

	void	Spawn( void );
	void	Touch( CBaseEntity *pOther );
	void	DoMagnetSuck( CBaseEntity *pOther );

	bool	IsOn( void ) { return m_bActive; }

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputTurnOn( inputdata_t &inputdata );
	void	InputTurnOff( inputdata_t &inputdata );

	void	InputTogglePolarity( inputdata_t &inputdata );
	void	InputTurnPositive( inputdata_t &inputdata );
	void	InputTurnNegative( inputdata_t &inputdata );

	// think
	void	SuckThink( void );
	
protected:
	// Outputs
	COutputEvent	m_OnMagnetAttach;
	COutputEvent	m_OnMagnetDetach;

	// Keys
//	float			m_massScale;
//	string_t		m_iszOverrideScript;
//	float			m_forceLimit;
//	float			m_torqueLimit;

//	CUtlVector< attached_objects_t >	m_MagnettedEntities;
//	IPhysicsConstraintGroup				*m_pConstraintGroup;

	bool			m_bActive;
	bool			m_bHasHitSomething;
	int				m_iMaxObjectsAttached;
	bool			m_bPositive;

	Vector			m_vecHitboxPadding;
};

//============================================================================================================
// PHYS MAGNET
//============================================================================================================
LINK_ENTITY_TO_CLASS( func_monopole_field, CMonopoleField );

// BUGBUG: This won't work!  Right now you can't save physics pointers inside an embedded type!
//BEGIN_SIMPLE_DATADESC( attached_objects_t )
//
//	DEFINE_PHYSPTR( pConstraint ),
//	DEFINE_FIELD( hEntity,	FIELD_EHANDLE	),
//
//END_DATADESC()

BEGIN_DATADESC( CMonopoleField )
	// Outputs
	DEFINE_OUTPUT( m_OnMagnetAttach, "OnAttach" ),
	DEFINE_OUTPUT( m_OnMagnetDetach, "OnDetach" ),

	// Keys
	DEFINE_KEYFIELD( m_bActive, FIELD_BOOLEAN,	"StartActive" ),
	DEFINE_KEYFIELD( m_bPositive, FIELD_BOOLEAN, "StartPositive" ),

	DEFINE_KEYFIELD( m_vecHitboxPadding, FIELD_VECTOR, "HitboxPadding" ),

//	DEFINE_UTLVECTOR( m_MagnettedEntities, FIELD_EMBEDDED ),
//	DEFINE_PHYSPTR( m_pConstraintGroup ),

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
CMonopoleField::CMonopoleField( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMonopoleField::~CMonopoleField( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopoleField::Spawn( void )
{
//	m_pConstraintGroup = NULL;

	BaseClass::Spawn();

	SetModel( STRING( GetModelName() ) );    // set size and link into world
											 // This has nothing to do with a real model

	if ( !showtriggers.GetInt() )
	{
		AddEffects( EF_NODRAW );
	}

	SetThink( &CMonopoleField::SuckThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopoleField::Touch( CBaseEntity *pOther )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopoleField::SuckThink( void )
{
	#ifdef DEBUG
	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
	NDebugOverlay::Box( GetAbsOrigin(), vecMins, vecMaxs, 0, 255, 0, 32, 0.05f );
	#endif DEBUG

	DoMagnetSuck( NULL );

	SetThink( &CMonopoleField::SuckThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMonopoleField::DoMagnetSuck( CBaseEntity *pOther )
{
	if ( !m_bActive )
		return;

	// Look for physics objects underneath the magnet and suck them onto it
	Vector vecOrigin;

	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
	vecOrigin = 0.5f*(vecMins + vecMaxs);

	vecMins -= m_vecHitboxPadding;
	vecMaxs += m_vecHitboxPadding;

#ifdef DEBUG
	NDebugOverlay::Box( GetAbsOrigin(), vecMins, vecMaxs, 0, 0, 255, 32, 0.05f );
#endif DEBUG

	CBaseEntity *pEntities[20];
	int iNumEntities = UTIL_EntitiesInBox( pEntities, 20, vecMins, vecMaxs, 0 );

	for ( int i = 0; i < iNumEntities; i++ )
	{
		CBaseEntity *pEntity = pEntities[i];
		if ( !pEntity || pEntity == pOther || pEntity == this )
			continue;

		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if ( pPhys && 
			( pEntity->GetMoveType() == MOVETYPE_VPHYSICS ||
			pEntity->GetMoveType() == MOVETYPE_WALK ) && 
			pPhys->GetMass() < 5000 )
		{
			{
				// Pull it towards the magnet
				CCollisionProperty *pCollision = CollisionProp();
				Vector vecNearest, vecVelocity, vecEntOrigin, vecPushOrigin;
				
				// get our origin, or the center if it's the player
				vecEntOrigin = pEntity->GetAbsOrigin();
				vecPushOrigin = pEntity->GetAbsOrigin();

				if( pEntity->IsPlayer() )
				{
					Vector vecEntMins, vecEntMaxs;
					pEntity->CollisionProp()->WorldSpaceSurroundingBounds( &vecEntMins, &vecEntMaxs );
					
					// pull up just a little.
					vecEntOrigin.z += 16.f;
					vecPushOrigin = 0.5f*(vecEntMins + vecEntMaxs);

					if( pEntity->GetFlags() & FL_ONGROUND )
					{
						pEntity->SetGroundEntity( NULL );

						Vector newOrigin = pEntity->GetAbsOrigin();
						newOrigin.z += 4.0f;
						pEntity->SetAbsOrigin( newOrigin );
					}
				}

				
				// we try to push through the center of the object.
				pCollision->CalcNearestPoint( vecPushOrigin, &vecNearest );
				vecVelocity = vecPushOrigin - vecNearest;

				// we do out dist check from the lower position
				float flDist = pCollision->CalcDistanceFromPoint( vecEntOrigin );

				if ( flDist > 0.0f )
				{
					flDist = MAX( flDist, 50.f );
					
					float flDirection = 1.0f;
					if ( !m_bPositive )
						flDirection = -1.0f;
					
					VectorNormalize(vecVelocity);

					// nasty force equation.
					vecVelocity *= flDirection * (50.f + 0.25f*pPhys->GetMass())  / ( (0.05f + flDist/100.f) * (0.05f + flDist/100.f) );

					if( pEntity->IsPlayer() )
					{
						vecVelocity.z *= 4.0f;
						pEntity->SetBaseVelocity( 4.f*vecVelocity );
						pEntity->AddFlag( FL_BASEVELOCITY );
					}
					else
					{
						pPhys->AddVelocity( &vecVelocity, NULL );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make the magnet active
//-----------------------------------------------------------------------------
void CMonopoleField::InputTurnOn( inputdata_t &inputdata )
{
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: Make the magnet inactive. Drop everything it's got hooked on.
//-----------------------------------------------------------------------------
void CMonopoleField::InputTurnOff( inputdata_t &inputdata )
{
	m_bActive = false;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the magnet's active state
//-----------------------------------------------------------------------------
void CMonopoleField::InputToggle( inputdata_t &inputdata )
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
void CMonopoleField::InputTurnPositive( inputdata_t &inputdata )
{
	m_bPositive = true;
}

//-----------------------------------------------------------------------------
// Purpose: Make the polarity negative
//-----------------------------------------------------------------------------
void CMonopoleField::InputTurnNegative( inputdata_t &inputdata )
{
	m_bPositive = false;
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the magnet's polarity
//-----------------------------------------------------------------------------
void CMonopoleField::InputTogglePolarity( inputdata_t &inputdata )
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