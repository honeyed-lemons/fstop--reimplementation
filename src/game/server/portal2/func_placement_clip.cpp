//
//
//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncPlacementClip : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPlacementClip, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn();
	bool CreateVPhysics( void );

	void InputEnable( inputdata_t &data );
	void InputDisable( inputdata_t &data );

private:
};

BEGIN_DATADESC( CFuncPlacementClip )
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_placement_clip, CFuncPlacementClip );

void CFuncPlacementClip::Spawn( void )
{

	SetLocalAngles( vec3_angle );
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetModel( STRING( GetModelName() ) );

	// It's part of the world
	AddFlag( FL_WORLDBRUSH );

	CreateVPhysics();

	AddEffects( EF_NODRAW );		// make entity invisible

	SetCollisionGroup( COLLISION_GROUP_PLACEMENT_SOLID );
}

bool CFuncPlacementClip::CreateVPhysics( void )
{
	SetSolid( SOLID_BSP );
	VPhysicsInitStatic();

	return true;
}

void CFuncPlacementClip::InputEnable( inputdata_t &data )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( true );
	}
	RemoveSolidFlags( FSOLID_NOT_SOLID );
}

void CFuncPlacementClip::InputDisable( inputdata_t &data )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableCollisions( false );
	}
	AddSolidFlags( FSOLID_NOT_SOLID );
}
