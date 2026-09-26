//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: Mousetrap
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "triggers.h"
#include "props.h"
#include "photo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mousetrap_forward_velocity( "mousetrap_forward_velocity", "250", FCVAR_NONE );
ConVar mousetrap_upward_velocity( "mousetrap_upward_velocity", "250", FCVAR_NONE );

//
//	Mousetrap
//

class CPropMousetrap : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropMousetrap, CBaseAnimating );
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void OnRestore( void );

	void CreateTriggers();

	virtual void UpdateOnRemove( void );

	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );
		virtual float GetMaxPlacementDistance( void );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

protected:
	void CheeseTouch( CBaseEntity *pOther );
	void ArmTouch( CBaseEntity *pOther );
	void DestroyTriggers( void );

	void OnCaptured( void );
	void OnReleased( void );

	void AnimateThink( void );

	void DoSnapPresentation( CBaseEntity *pTarget );
	void LaunchVictim( CBaseEntity *pOther );
	void CrushVictim(CBaseEntity *pOther );

	void Init_NormalSized( void );
	void Init_MidSized( void );
	void Init_FullSized( void );
	void CreatePlacementHelper( const Vector &vecOrigin, const QAngle &vecAngles );

	void InputSnap( inputdata_t &inputData );

	CHandle<CTriggerCallback>	m_hCheeseTrigger;
	CHandle<CTriggerCallback>	m_hArmTrigger;
	CHandle<CBaseEntity>		m_hHelper;
};

BEGIN_DATADESC( CPropMousetrap )
	DEFINE_ENTITYFUNC( CheeseTouch ),
	DEFINE_ENTITYFUNC( ArmTouch ),
	DEFINE_THINKFUNC( AnimateThink ),

	DEFINE_FIELD( m_hCheeseTrigger, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hArmTrigger, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hHelper, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Snap", InputSnap ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_mousetrap, CPropMousetrap );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::Precache( void )
{
	PrecacheModel( "models/props_farm/mousetrap.mdl" );
	PrecacheScriptSound( "Mousetrap.Snap" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::DestroyTriggers( void )
{
	if ( m_hCheeseTrigger )
	{
		UTIL_Remove( m_hCheeseTrigger );
		m_hCheeseTrigger = NULL;
	}

	if ( m_hArmTrigger )
	{
		UTIL_Remove( m_hArmTrigger );
		m_hArmTrigger = NULL;
	}

	if ( m_hHelper )
	{
		UTIL_Remove( m_hHelper );
		m_hHelper = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove our helpers as well
//-----------------------------------------------------------------------------
void CPropMousetrap::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
	DestroyTriggers();
}

//-----------------------------------------------------------------------------
// Purpose: Create a placement helper
//-----------------------------------------------------------------------------
void CPropMousetrap::CreatePlacementHelper( const Vector &vecOrigin, const QAngle &vecAngles )
{
	if ( m_hHelper )
	{
		UTIL_Remove( m_hHelper );
	}

	m_hHelper = CreateEntityByName( "info_placement_helper" );
	m_hHelper->SetAbsOrigin( vecOrigin );
	m_hHelper->SetAbsAngles( vecAngles );
	m_hHelper->KeyValue( "radius", "24" );
	m_hHelper->KeyValue( "hide_until_placed", "0" );
	DispatchSpawn( m_hHelper );
	m_hHelper->Activate();
	m_hHelper->SetParent( this );
}

//-----------------------------------------------------------------------------
// Purpose: Mousetrap doesn't do much other than snap when touched and fly into the air
//-----------------------------------------------------------------------------
void CPropMousetrap::Init_NormalSized( void )
{
	// Create our helper trigger
	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
	vecMins -= GetAbsOrigin();
	vecMaxs -= GetAbsOrigin();
	vecMaxs *= 2.0f;
	vecMins *= 2.0f;
	m_hCheeseTrigger = CTriggerCallback::Create( GetAbsOrigin(), GetAbsAngles(), vecMins, vecMaxs, this, static_cast <void (CBaseEntity::*)(CBaseEntity *)> (&CPropMousetrap::CheeseTouch) );
}

//-----------------------------------------------------------------------------
// Purpose: Able to kill zombies!
//-----------------------------------------------------------------------------
void CPropMousetrap::Init_MidSized( void )
{
	// We want to know when things touch us
	Init_FullSized(); // FIXME: For now there's no difference
}

//-----------------------------------------------------------------------------
// Purpose: Catapults the player into the air!
//-----------------------------------------------------------------------------
void CPropMousetrap::Init_FullSized( void )
{
	Vector vecForward, vecRight, vecUp;
	GetVectors( &vecForward, &vecRight, &vecUp );

	float flScale = GetModelScale();
	Vector vecOrigin = GetAbsOrigin();
	vecOrigin += vecForward * 3.5f * flScale;
	vecOrigin += vecUp * 0.75f * flScale;

	Vector vecMins( -2.0f * flScale, -2.0f * flScale, 0 );
	Vector vecMaxs( 2.0f * flScale, 2.0f * flScale, 2.5f * flScale );

	// Create our helper trigger
	m_hCheeseTrigger = CTriggerCallback::Create(	vecOrigin, 
													GetAbsAngles(), 
													vecMins, vecMaxs, 
													this, 
													static_cast <void (CBaseEntity::*)(CBaseEntity *)> (&CPropMousetrap::CheeseTouch) );
	
	vecOrigin = GetAbsOrigin();
	vecOrigin -= vecForward * 3.5f * flScale;
	vecOrigin += vecUp * 0.75f * flScale;

	vecMins.Init( -4.0f * flScale, -4.0f * flScale, 0 );
	vecMaxs.Init( 4.0f * flScale, 4.0f * flScale, 2.0f * flScale );

	// Create our helper trigger
	m_hArmTrigger = CTriggerCallback::Create(	vecOrigin, 
												GetAbsAngles(), 
												vecMins, vecMaxs, 
												this, 
												static_cast <void (CBaseEntity::*)(CBaseEntity *)> (&CPropMousetrap::ArmTouch) );

	// Create the helper
	QAngle vecAngles;
	VectorAngles( vecForward, vecAngles );
	CreatePlacementHelper( vecOrigin + ( -vecForward * 2.5f * flScale ) + ( vecUp * 20.0f ), vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::Spawn( void )
{
	Precache();
	SetModel( "models/props_farm/mousetrap.mdl" );

	BaseClass::Spawn();

	CreateVPhysics();
	CreateTriggers();
	SetCollisionGroup( COLLISION_GROUP_PASSABLE_DOOR );


	SetThink( &CPropMousetrap::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

bool CPropMousetrap::CreateVPhysics( void )
{
	// Create our normal physical bounds
	solid_t tmpSolid;
	PhysModelParseSolid( tmpSolid, this, GetModelIndex() );
	PhysGetMassCenterOverride( this, modelinfo->GetVCollide( GetModelIndex() ), tmpSolid );
	IPhysicsObject* pObj = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, &tmpSolid );

	if ( pObj )
	{
		return true;
	}
	else
	{
		Assert( 0 );
		return false;
	}
}

const float TRAP_HEIGHT = 4.0f;
const float TRAP_LENGTH = 12.0f;
const float TRAP_WIDTH = 4.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::LaunchVictim( CBaseEntity *pOther )
{
	Vector vecForward, vecRight, vecUp;
	GetVectors( &vecForward, &vecRight, &vecUp );

	float flScale = GetObjectScaleLevel();

	// Affect certain things in certain ways
	if ( pOther->MyCombatCharacterPointer() )
	{
		// Handle a player
		if ( pOther->IsPlayer() )
		{
			// Simply push us forward
			float flSpeed = mousetrap_forward_velocity.GetFloat() * flScale;
			if ( GetObjectScaleLevel() == 1 )
				flSpeed *= 0.75f;

			Vector vecPush = vecForward * flSpeed;

			flSpeed = mousetrap_upward_velocity.GetFloat() * flScale;
			if ( GetObjectScaleLevel() == 1 )
				flSpeed *= 1.5f;
			
			vecPush[2] = flSpeed;

			// Send us flying
			if ( pOther->GetFlags() & FL_ONGROUND )
			{
				pOther->SetGroundEntity( NULL );
				pOther->SetGroundChangeTime( gpGlobals->curtime + 0.5f );
			}

			pOther->SetAbsVelocity( vecPush );
		}
		else // NPCs
		{
			if ( FClassnameIs( pOther, "npc_chicken" ) )
			{
				CBaseAnimating *pOtherAnim = pOther->GetBaseAnimating();
				int nScaleLevel = pOtherAnim->GetObjectScaleLevel();
				Vector vecPush = vecForward * 50.0f * GetModelScale();
				vecPush[2] += 50.0f * GetModelScale();

				if ( nScaleLevel )
				{
					vecPush /= nScaleLevel;
				}

				// Send us flying
				if ( pOther->GetFlags() & FL_ONGROUND )
				{
					pOther->SetGroundEntity( NULL );
					pOther->SetGroundChangeTime( gpGlobals->curtime + 0.5f );
				}

				pOther->SetAbsVelocity( vecPush );

				QAngle angles;
				VectorAngles( vecPush, angles );
				angles.x = angles.z = 0.0f;
				pOther->SetAbsAngles( angles );
			}
			else
			{
				// Do slashing damage to the target (to hopefully cut them up)
				CTakeDamageInfo info( this, this, 200.0f, DMG_CRUSH );
				float flMass = 100.0f;
				info.SetDamageForce( ( vecForward * 250.0f * flMass * flScale ) + ( vecUp * 250.0f * flMass * flScale ) );
				info.SetDamagePosition( GetAbsOrigin() );
				pOther->TakeDamage( info );
			}
		}
	}
	else
	{
		if ( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			// Launch!
			IPhysicsObject *pPhysObject = pOther->VPhysicsGetObject();
			if ( pPhysObject )
			{
				Vector vecVelocity = vecForward * 475.0f * flScale;
				vecVelocity[2] = 475.0f * flScale;
				pPhysObject->ApplyForceCenter( vecVelocity );
				
				AngularImpulse angImpulse = RandomAngularImpulse( -400.0f, 400.0f );

				pPhysObject->SetVelocity( &vecVelocity, &angImpulse );

				CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pOther);
				if ( pProp != NULL )
				{
					//HACK!
					pProp->OnPhysGunDrop( UTIL_GetLocalPlayer(), LAUNCHED_BY_CANNON );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::CrushVictim( CBaseEntity *pOther )
{
	Vector vecForward, vecRight, vecUp;
	GetVectors( &vecForward, &vecRight, &vecUp );

	float flScale = GetObjectScaleLevel();

	// Affect certain things in certain ways
	if ( pOther->MyCombatCharacterPointer() )
	{
		// Handle a player
		if ( pOther->IsPlayer() )
		{
			//Kick the player angles
			CBasePlayer *pPlayer = ToBasePlayer( pOther );
			pPlayer->ViewPunch( QAngle( -16*flScale, 0, 0 ) );	

			Vector vecPush = vecForward * 275.0f * flScale;
			vecPush[2] += 64.0f * flScale;
			
			pOther->SetAbsVelocity( vecPush );
			pOther->SetBaseVelocity( vecPush );
			pOther->AddFlag( FL_BASEVELOCITY );
		}
		else // NPCs
		{
			// Do slashing damage to the target (to hopefully cut them up)
			CTakeDamageInfo info( this, this, 200.0f, DMG_CRUSH );
			float flMass = 100.0f;
			info.SetDamageForce( ( vecForward * 250.0f * flMass * flScale ) + ( vecUp * 250.0f * flMass * flScale ) );
			info.SetDamagePosition( GetAbsOrigin() );
			pOther->TakeDamage( info );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::DoSnapPresentation( CBaseEntity *pTarget )
{
	// Play our animation
	int nSequence = LookupSequence( "snap" );
	ResetSequence( nSequence );

	// Play a sound
	EmitSound( "Mousetrap.Snap" );

	// Only do this if we're small or something other than the player has stepped on us
	bool bShouldJump = ( pTarget == NULL || ( FClassnameIs( pTarget, "npc_chicken" ) == false && pTarget->IsPlayer() == false ) || GetObjectScaleLevel() == 0 );
	if ( bShouldJump )
	{
		// Leap into the air
		IPhysicsObject *pPhyObj = VPhysicsGetObject();
		if ( pPhyObj )
		{
			Vector vecUp;
			GetVectors( NULL, NULL, &vecUp );

			float flModifier = ( GetObjectScaleLevel() == 0 ) ? 4.0f : 1.0f;

			// Fly up into the air a bit
			float flMass = pPhyObj->GetMass();
			pPhyObj->ApplyForceCenter( vecUp * random->RandomFloat( 25.0f, 30.0f ) * flModifier * flMass * GetModelScale() );
			pPhyObj->ApplyTorqueCenter( RandomAngularImpulse( 5, 10 ) * flModifier * flMass * GetModelScale() );
		}
	}

	// We're now sprung, so don't re-fire
	DestroyTriggers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::ArmTouch( CBaseEntity *pOther )
{
	DoSnapPresentation( pOther );

	if ( GetObjectScaleLevel() )
	{
		LaunchVictim( pOther );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::CheeseTouch( CBaseEntity *pOther )
{
	DoSnapPresentation( pOther );

	if ( GetObjectScaleLevel() )
	{
		CrushVictim( pOther );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	// Do it forever!
	SetThink( &CPropMousetrap::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( m_debugOverlays & OVERLAY_BBOX_BIT )
	{
		if ( m_hArmTrigger )
		{
			m_hArmTrigger->m_debugOverlays |= OVERLAY_BBOX_BIT;
		}

		if ( m_hCheeseTrigger )
		{
			m_hCheeseTrigger->m_debugOverlays |= OVERLAY_BBOX_BIT;
		}
	}
	else
	{
		if ( m_hArmTrigger )
		{
			m_hArmTrigger->m_debugOverlays &= ~OVERLAY_BBOX_BIT;
		}

		if ( m_hCheeseTrigger )
		{
			m_hCheeseTrigger->m_debugOverlays &= ~OVERLAY_BBOX_BIT;
		}
	}
}

void VisualizeTestTrace( const trace_t &tr )
{
#ifdef DEBUG
	NDebugOverlay::Box( tr.startpos, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, 32, 0.05f );
	NDebugOverlay::Line( tr.startpos, tr.endpos, 0, 255, 0, true, 0.05f );
	NDebugOverlay::Box( tr.endpos, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, 32, 0.05f );
#endif // DEBUG
}

//------------------------------------------------------------------------------
// Placement query
//------------------------------------------------------------------------------

bool CPropMousetrap::CPhotoPlacementQuery::GetPlacementPosition_NoHelper( CaptureInfo_t &captureInfo,
																		 CheckPlacementData_t &placementData,
																		 Vector &positionOut,
																		 QAngle &anglesOut )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	Vector vecForward = placementData.Trace.endpos - pPlayer->EyePosition();

	VectorNormalize( vecForward );
	vecForward.z = 0.0f;
	Vector vecRight, vecUp;
	VectorVectors( vecForward, vecRight, vecUp );
	
	float flBestFraction = 0.0f;
	Vector vecTestPos = placementData.Trace.endpos + ( vecUp * 4.0f * placementData.fScale );

	//
	//

	trace_t tr;
	UTIL_TraceLine( vecTestPos, vecTestPos + ( vecUp * -8.0f * placementData.fScale ), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction > flBestFraction )
	{
		flBestFraction = tr.fraction;
	}
	VisualizeTestTrace( tr );

	vecTestPos = placementData.Trace.endpos + ( vecUp * 4.0f * placementData.fScale );
	vecTestPos += ( vecForward * 8.0f * placementData.fScale ) + ( vecRight * 3.5f * placementData.fScale );
	UTIL_TraceLine( vecTestPos, vecTestPos + ( vecUp * -8.0f * placementData.fScale ), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < flBestFraction )
	{
		flBestFraction = tr.fraction;
	}
	VisualizeTestTrace( tr );

	vecTestPos = placementData.Trace.endpos + ( vecUp * 4.0f * placementData.fScale );
	vecTestPos += ( vecForward * 8.0f * placementData.fScale ) + ( vecRight * -3.5f * placementData.fScale );
	UTIL_TraceLine( vecTestPos, vecTestPos + ( vecUp * -8.0f * placementData.fScale ), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < flBestFraction )
	{
		flBestFraction = tr.fraction;
	}
	VisualizeTestTrace( tr );

	vecTestPos = placementData.Trace.endpos + ( vecUp * 4.0f * placementData.fScale );
	vecTestPos += ( vecForward * -8.0f * placementData.fScale ) + ( vecRight * 3.5f * placementData.fScale );
	UTIL_TraceLine( vecTestPos, vecTestPos + ( vecUp * -8.0f * placementData.fScale ), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < flBestFraction )
	{
		flBestFraction = tr.fraction;
	}
	VisualizeTestTrace( tr );

	vecTestPos = placementData.Trace.endpos + ( vecUp * 4.0f * placementData.fScale );
	vecTestPos += ( vecForward * -8.0f * placementData.fScale ) + ( vecRight * -3.5f * placementData.fScale );
	UTIL_TraceLine( vecTestPos, vecTestPos + ( vecUp * -8.0f * placementData.fScale ), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < flBestFraction )
	{
		flBestFraction = tr.fraction;
	}
	VisualizeTestTrace( tr );

	
	anglesOut = vec3_angle;	
	anglesOut[YAW] = UTIL_VecToYaw( vecForward ); // Face the player so that the "cheese" is facing away

	positionOut = placementData.Trace.endpos + (vecUp * ((3.0f - (6.0f * flBestFraction)) * placementData.fScale));
		
	return true;
}

CameraInfo_ScaleData_t *CPropMousetrap::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 1.0f, 6.0f, 12.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

float CPropMousetrap::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	const float flBaseDist = (20.0f * 12.0f);
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		QAngle vecAngle = pPlayer->GetAbsAngles();

		// We only care about the quadrant under the horizon line to straight down
		if ( vecAngle.x > 90.0f || vecAngle.x < 0.0f )
			return flBaseDist;

		float flCos = cosf( DEG2RAD(vecAngle.x) );
		if ( flCos == 0.0f )
			return flBaseDist;
		
		// Find the opposite side of the triangle to find the distance forward to keep us base distance away
		return fabs( (1.0f/flCos) * flBaseDist );
	}

	return flBaseDist;
}


void CPropMousetrap::OnCaptured( void )
{
	int nSequence = LookupSequence( "idle" );
	ResetSequence( nSequence );

	DestroyTriggers();

	BaseClass::OnCaptured();
}

void CPropMousetrap::OnReleased( void )
{
	DestroyTriggers();
	CreateTriggers();

	BaseClass::OnReleased();
}

void CPropMousetrap::OnRestore( void )
{
	//HACK: Re-create triggers, they have save load issues
	UTIL_Remove( m_hArmTrigger );
	UTIL_Remove( m_hCheeseTrigger );

	m_hArmTrigger = m_hCheeseTrigger = NULL;
	CreateTriggers();

	BaseClass::OnRestore();
}

void CPropMousetrap::CreateTriggers()
{
	// Don't stomp handles
	Assert ( !m_hArmTrigger.Get() && !m_hCheeseTrigger.Get() );

	switch ( GetObjectScaleLevel() )
	{
	default:
	case 0:
		Init_NormalSized();
		break;

	case 1:
		Init_MidSized();
		break;

	case 2:
		Init_FullSized();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropMousetrap::InputSnap( inputdata_t &inputData )
{
	DoSnapPresentation( NULL );
}