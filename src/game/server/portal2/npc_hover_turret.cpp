//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "soundenvelope.h"
#include "ai_default.h"
#include "ai_node.h"
#include "ai_navigator.h"
#include "ai_pathfinder.h"
#include "ai_moveprobe.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ai_route.h"
#include "explode.h"
#include "basegrenade_shared.h"
#include "ndebugoverlay.h"
#include "decals.h"
#include "gib.h"
#include "game.h"			
#include "ai_interactions.h"
#include "ieffects.h"
#include "vstdlib/random.h"
#include "engine/ienginesound.h"
#include "movevars_shared.h"
#include "npcevent.h"
#include "props.h"
#include "te_effect_dispatch.h"
#include "ai_squadslot.h"
#include "world.h"
#include "smoke_trail.h"
#include "func_break.h"
#include "physics_impact_damage.h"
#include "weapon_physcannon.h"
#include "physics_prop_ragdoll.h"
#include "soundent.h"
#include "ammodef.h"
#include "ai_basenpc_physicsflyer.h"
#include "sprite.h"
#include "spritetrail.h"
#include "player_pickup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// When the engine is running and the manhack is operating under power
// we don't let gravity affect him.
#define HOVER_TURRET_GRAVITY			0.000

#define HOVER_TURRET_GIB_COUNT			5 
#define HOVER_TURRET_INGORE_WATER_DIST	384

// Sound stuff
#define HOVER_TURRET_PITCH_DIST1		512
#define HOVER_TURRET_MIN_PITCH1			100
#define HOVER_TURRET_MAX_PITCH1			160
#define HOVER_TURRET_WATER_PITCH1		85
#define HOVER_TURRET_VOLUME1			0.55
#define HOVER_MIN_HEIGHT				52

#define HOVER_TURRET_PITCH_DIST2		400
#define HOVER_TURRET_MIN_PITCH2			85
#define HOVER_TURRET_MAX_PITCH2			190
#define HOVER_TURRET_WATER_PITCH2		90

#define HOVER_TURRET_NOISEMOD_HIDE		5000

#define HOVER_TURRET_BODYGROUP_BLADE	1
#define HOVER_TURRET_BODYGROUP_BLUR		2
#define HOVER_TURRET_BODYGROUP_OFF		0
#define HOVER_TURRET_BODYGROUP_ON		1

#define HOVER_TURRET_SHOT_KNOCKBACK		100.0f
#define HOVER_TURRET_SHOT_CYCLE			0.20f

#define HOVER_TURRET_DAMAGE_MULTIPLIER	3.0f
#define HOVER_TURRET_BULLET_FORCE_MULTIPLIER 0.1f
//#define HOVER_TURRET_PHYSICAL_FORCE_MULTIPLIER 135.0f

#define HOVER_TURRET_LASER_EFFECT	"effects/bluelaser1.vmt"
#define HOVER_TURRET_GLOW_SPRITE	"sprites/light_glow03.vmt"

#define	HOVER_TURRET_CHARGE_MIN_DIST	200

ConVar	sk_hover_turret_health( "sk_hover_turret_health","150");
ConVar	sk_hover_turret_melee_dmg( "sk_hover_turret_melee_dmg","0");
ConVar	sk_hover_turret_v2( "sk_hover_turret_v2","1");

extern void		SpawnBlood(Vector vecSpot, const Vector &vAttackDir, int bloodColor, float flDamage);
extern float	GetFloorZ(const Vector &origin);

// Start with the engine off and folded up.
#define SF_HOVER_TURRET_PACKED_UP			(1 << 16)
#define SF_HOVER_TURRET_NO_DAMAGE_EFFECTS	(1 << 17)
#define SF_HOVER_TURRET_USE_AIR_NODES		(1 << 18)
#define SF_HOVER_TURRET_CARRIED				(1 << 19)	// Being carried by a metrocop
#define SF_HOVER_TURRET_NO_DANGER_SOUNDS		(1 << 20)

//-----------------------------------------------------------------------------
// Attachment points.
//-----------------------------------------------------------------------------
#define	HOVER_TURRET_GIB_HEALTH				30
#define	HOVER_TURRET_INACTIVE_HEALTH			25
#define	HOVER_TURRET_MAX_SPEED				500
#define HOVER_TURRET_BURST_SPEED				650
#define HOVER_TURRET_NPC_BURST_SPEED			800

#define HOVER_TURRET_MODEL	"models/npcs/hover_turret.mdl"

//-----------------------------------------------------------------------------
// Manhack 
//-----------------------------------------------------------------------------
class CNPC_HoverTurret : public CNPCBaseInteractive<CAI_BasePhysicsFlyingBot>, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CNPC_HoverTurret, CNPCBaseInteractive<CAI_BasePhysicsFlyingBot> );
	DECLARE_SERVERCLASS();

public:
	CNPC_HoverTurret();
	~CNPC_HoverTurret();

	Class_T			Classify( void ) { return CLASS_COMBINE; }

	// Attacking
	void			Event_Killed( const CTakeDamageInfo &info );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);

	void			Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy );
	const char		*GetTracerType( void ) { return "AR2Tracer"; }

	float			GetAttackDamageScale( CBaseEntity *pVictim );

	void			ShootThink( void );
	void			ShootBullet();

	void			UpdateOnRemove( void );

	bool			OverrideMove(float flInterval);
	void			MoveToTarget(float flInterval, const Vector &MoveTarget);
	void			MoveExecute_Alive(float flInterval);

	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true ) { return WorldSpaceCenter(); }

	virtual float	GetHeadTurnRate( void ) { return 720.0f; } // Degrees per second

	void			Precache(void);
	void			RunTask( const Task_t *pTask );
	void			Spawn(void);
	void			StartTask( const Task_t *pTask );

	void			GatherConditions();
	void			PrescheduleThink( void );
	void			Explode( void );

	void			VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	virtual void	ClampMotorForces( Vector &linear, AngularImpulse &angular );

	// Player pickup
	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );

	// Use functions
	void	ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int ObjectCaps() 
	{ 
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
		{
			pPlayer->PickupObject( this, false );
		}
	}

	CBasePlayer *HasPhysicsAttacker( float dt );

	float GetMaxEnginePower();

	DEFINE_CUSTOM_AI;

	DECLARE_DATADESC();

private:

	void MaintainGroundHeight( void );
	void TakeDamageFromPhysicsImpact( int index, gamevcollisionevent_t *pEvent );

	virtual bool	AllowedToIgnite( void ) { return true; }
	bool			OnBurning( void );

	// Weapon variables
	int				m_iAmmoType;
	bool			m_bAimingAtTarget;

	// networked vars
	CNetworkVar( bool, m_bLaserOn );
	CNetworkVar( float, m_flAimStartTime );
	CNetworkVar( int, m_sLaserHaloSprite );

	// Player pickup
	bool			m_bCarriedByPlayer;
	bool			m_bUseCarryAngles;
	float			m_flPlayerDropTime;

	// Movement variables
	Vector			m_vForceVelocity;		// Someone forced me to move

	Vector			m_vTargetBanking;

	Vector			m_vForceMoveTarget;		// Will fly here
	float			m_fForceMoveTime;		// If time is less than this
	Vector			m_vSwarmMoveTarget;		// Will fly here
	float			m_fSwarmMoveTime;		// If time is less than this
	float			m_fEnginePowerScale;	// scale all thrust by this amount (usually 1.0!)

	float			m_flNextEngineSoundTime;
	float			m_flEngineStallTime;

	float			m_flNextBurstTime;
	float			m_flBurstDuration;
	Vector			m_vecBurstDirection;

	float			m_flWaterSuspendTime;
	int				m_nLastSpinSound;

	// physics influence
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	// Death
	float			m_fSparkTime;
	float			m_fSmokeTime;

	bool			m_bDirtyPitch; // indicates whether we want the sound pitch updated.(sjb)
	bool			m_bShowingHostile;

	bool			m_bBladesActive;
	bool			m_bIgnoreClipbrushes;

	float			m_flBladeSpeed;

	CSprite			*m_pEyeGlow;
	CSprite			*m_pLightGlow;

	CHandle<SmokeTrail>	m_hSmokeTrail;

	int				m_nLastWaterLevel;
	bool			m_bDoSwarmBehavior;
	bool			m_bGib;

	bool			m_bHeld;
	bool			m_bInitialPositionSet;

	Vector			m_vecLoiterPosition;
	float			m_fTimeNextLoiterPulse;

	float			m_flBumpSuppressTime;

	COutputEvent	m_OnPhysGunPickup;
	COutputEvent	m_OnPhysGunDrop;
};

//-----------------------------------------------------------------------------
// HoverTurret Conditions
//-----------------------------------------------------------------------------
// enum HoverTurretConditions
// {
// 	LAST_SHARED_CONDITION,
// };

//-----------------------------------------------------------------------------
// HoverTurret schedules.
//-----------------------------------------------------------------------------
enum HoverTurretSchedules
{
	SCHED_HOVER_TURRET_IDLE = LAST_SHARED_SCHEDULE,
};


//-----------------------------------------------------------------------------
// HoverTurret tasks.
//-----------------------------------------------------------------------------
enum HoverTurretTasks
{
	TASK_HOVER_TURRET_HOVER = LAST_SHARED_TASK,
};

LINK_ENTITY_TO_CLASS( npc_hover_turret, CNPC_HoverTurret );

BEGIN_DATADESC( CNPC_HoverTurret )

DEFINE_FIELD( m_iAmmoType,				FIELD_INTEGER ),

DEFINE_FIELD( m_vForceVelocity,			FIELD_VECTOR),

DEFINE_FIELD( m_vTargetBanking,			FIELD_VECTOR),
DEFINE_FIELD( m_vForceMoveTarget,			FIELD_POSITION_VECTOR),
DEFINE_FIELD( m_fForceMoveTime,			FIELD_TIME),
DEFINE_FIELD( m_vSwarmMoveTarget,			FIELD_POSITION_VECTOR),
DEFINE_FIELD( m_fSwarmMoveTime,			FIELD_TIME),
DEFINE_FIELD( m_fEnginePowerScale,		FIELD_FLOAT),

DEFINE_FIELD( m_flNextEngineSoundTime,	FIELD_TIME),
DEFINE_FIELD( m_flEngineStallTime,		FIELD_TIME),
DEFINE_FIELD( m_flNextBurstTime,			FIELD_TIME ),
DEFINE_FIELD( m_flWaterSuspendTime,		FIELD_TIME),
DEFINE_FIELD( m_nLastSpinSound,			FIELD_INTEGER ),

// Player pickup
DEFINE_FIELD( m_bCarriedByPlayer, FIELD_BOOLEAN ),
DEFINE_FIELD( m_bUseCarryAngles, FIELD_BOOLEAN ),
DEFINE_FIELD( m_flPlayerDropTime, FIELD_TIME ),

// Death
DEFINE_FIELD( m_fSparkTime,				FIELD_TIME),
DEFINE_FIELD( m_fSmokeTime,				FIELD_TIME),

DEFINE_FIELD( m_bDirtyPitch,			FIELD_BOOLEAN ),
DEFINE_FIELD( m_bGib,					FIELD_BOOLEAN),
DEFINE_FIELD( m_bHeld,					FIELD_BOOLEAN),

DEFINE_FIELD( m_vecLoiterPosition,		FIELD_POSITION_VECTOR),
DEFINE_FIELD( m_fTimeNextLoiterPulse,	FIELD_TIME),

DEFINE_FIELD( m_flBumpSuppressTime,		FIELD_TIME ),

DEFINE_FIELD( m_bBladesActive,			FIELD_BOOLEAN),
DEFINE_FIELD( m_flBladeSpeed,				FIELD_FLOAT),
DEFINE_KEYFIELD( m_bIgnoreClipbrushes,	FIELD_BOOLEAN, "ignoreclipbrushes" ),
DEFINE_FIELD( m_hSmokeTrail,				FIELD_EHANDLE),

DEFINE_FIELD( m_nLastWaterLevel,			FIELD_INTEGER ),
DEFINE_FIELD( m_bDoSwarmBehavior,			FIELD_BOOLEAN ),

// Physics Influence
DEFINE_FIELD( m_hPhysicsAttacker, FIELD_EHANDLE ),
DEFINE_FIELD( m_flLastPhysicsInfluenceTime, FIELD_TIME ),

DEFINE_FIELD( m_flBurstDuration,	FIELD_FLOAT ),
DEFINE_FIELD( m_vecBurstDirection,	FIELD_VECTOR ),
DEFINE_FIELD( m_bShowingHostile,	FIELD_BOOLEAN ),

DEFINE_OUTPUT( m_OnPhysGunPickup, "OnPhysGunPickup" ),
DEFINE_OUTPUT( m_OnPhysGunDrop, "OnPhysGunDrop" ),

// Function Pointers
DEFINE_BASENPCINTERACTABLE_DATADESC(),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_HoverTurret, DT_NPC_HoverTurret)

	SendPropFloat( SENDINFO( m_flAimStartTime ) ),
	SendPropBool( SENDINFO( m_bLaserOn ) ),
	SendPropInt( SENDINFO( m_sLaserHaloSprite ) ),

END_SEND_TABLE()

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CNPC_HoverTurret::CNPC_HoverTurret()
{
#ifdef _DEBUG
	m_vForceMoveTarget.Init();
	m_vSwarmMoveTarget.Init();
	m_vTargetBanking.Init();
	m_vForceVelocity.Init();
#endif
	m_bDirtyPitch = true;
	m_nLastWaterLevel = 0;
	m_bDoSwarmBehavior = true;
	m_flBumpSuppressTime = 0;
	m_bUseCarryAngles = true;
	m_bLaserOn = false;
	m_flAimStartTime = 0.0f;
	m_sLaserHaloSprite = 0;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CNPC_HoverTurret::~CNPC_HoverTurret()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::GatherConditions()
{
	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::PrescheduleThink( void )
{
	if ( IsOnFire() )
	{
		OnBurning();
	}
	else
	{
		ShootThink();
	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::ShootThink( void )
{
	// have we aimed long enough?
	if ( HasCondition( COND_SEE_ENEMY ) && (m_flEngineStallTime <= gpGlobals->curtime) && !m_bHeld )
	{
		if ( !m_bAimingAtTarget )
		{
			m_bAimingAtTarget = true;
			m_flAimStartTime.GetForModify() = gpGlobals->curtime;
			m_bLaserOn.GetForModify() = true;
		}
		else if( (m_flAimStartTime + 2.0f) < gpGlobals->curtime )
		{
			// set us to shoot again quickly.
			m_flAimStartTime.GetForModify() = gpGlobals->curtime - (2.0f - HOVER_TURRET_SHOT_CYCLE);
			
			// Take a shot
			ShootBullet();
		}
	}
	else
	{
		m_bLaserOn.GetForModify() = false;
		m_bAimingAtTarget = false;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::ShootBullet()
{
	CBaseEntity *pEnemy = GetEnemy();
	if ( !pEnemy )
		return;

	//Get our shot positions
	Vector vecMid = EyePosition();
	Vector vecMidEnemy = pEnemy->BodyTarget( vecMid );

	//Calculate dir and dist to enemy
	Vector	vecDirToEnemy = vecMidEnemy - vecMid;

	//We want to look at the enemy's eyes so we don't jitter
	Vector	vecDirToEnemyEyes = vecMidEnemy - vecMid;
	VectorNormalize( vecDirToEnemyEyes );

	QAngle vecAnglesToEnemy;
	VectorAngles( vecDirToEnemyEyes, vecAnglesToEnemy );

	//Fire the weapon
#if !DISABLE_SHOT
	Shoot( vecMid, vecDirToEnemy );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CNPC_HoverTurret::GetAttackDamageScale( CBaseEntity *pVictim )
{
	CBaseCombatCharacter *pBCC = pVictim->MyCombatCharacterPointer();

	if ( pBCC )
	{
		if ( pBCC->Classify() == CLASS_PLAYER )
		{
			return HOVER_TURRET_DAMAGE_MULTIPLIER;
		}
	}

	return BaseClass::GetAttackDamageScale( pVictim );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// void CNPC_HoverTurret::DeathSound( const CTakeDamageInfo &info )
// {
// 	BaesClass::DeathSound( info );
// }

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	Explode();
}

//-----------------------------------------------------------------------------
// Take damage from combine ball
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::TakeDamageFromPhysicsImpact( int index, gamevcollisionevent_t *pEvent )
{
	CBaseEntity *pHitEntity = pEvent->pEntities[!index];

	// NOTE: Bypass the normal impact energy scale here.
	float flDamageScale = PlayerHasMegaPhysCannon() ? 10.0f : 1.0f;
	int damageType = 0;
	float damage = CalculateDefaultPhysicsDamage( index, pEvent, flDamageScale, true, damageType );
	if ( damage == 0 )
		return;

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	if ( damageForce == vec3_origin )
	{
		// This can happen if this entity is motion disabled, and can't move.
		// Use the velocity of the entity that hit us instead.
		damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
	}

	// FIXME: this doesn't pass in who is responsible if some other entity "caused" this collision
	PhysCallbackDamage( this, CTakeDamageInfo( pHitEntity, pHitEntity, damageForce, damagePos, damage, damageType ), *pEvent, index );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	int otherIndex = !index;
	CBaseEntity *pHitEntity = pEvent->pEntities[otherIndex];

	if ( pHitEntity )
	{
		// It can take physics damage if it rams into a vehicle
		if ( pHitEntity->HasPhysicsAttacker( 0.5f ) )
		{
			// It also can take physics damage from things thrown by the player.
			TakeDamageFromPhysicsImpact( index, pEvent );
		}
		else if ( FClassnameIs( pHitEntity, "prop_combine_ball" ) )
		{
			// It also can take physics damage from a combine ball.
			TakeDamageFromPhysicsImpact( index, pEvent );
		}
		else if ( m_iHealth <= 0 )
		{
			TakeDamageFromPhysicsImpact( index, pEvent );
		}
	}
	
	// Stall out for a short time
	m_flEngineStallTime = gpGlobals->curtime + 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::MaintainGroundHeight( void )
{
	float zSpeed = GetCurrentVelocity().z;

	if ( zSpeed > 32.0f )
		return;

	trace_t	tr;
	AI_TraceHull(	GetAbsOrigin(), 
		GetAbsOrigin() - Vector( 0, 0, HOVER_MIN_HEIGHT ), 
		GetHullMins(), 
		GetHullMaxs(), 
		(GetAITraceMask_BrushOnly()), 
		this, 
		COLLISION_GROUP_NONE, 
		&tr );

	if ( tr.fraction != 1.0f )
	{
		float speedAdj = MAX( 16, (-zSpeed*0.5f) );

		m_vForceVelocity += Vector(0,0,1) * ( speedAdj * ( 1.0f - tr.fraction ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles movement towards the last move target.
// Input  : flInterval - 
//-----------------------------------------------------------------------------
bool CNPC_HoverTurret::OverrideMove( float flInterval )
{
	if ( !m_bInitialPositionSet )
	{
		m_vForceMoveTarget = GetAbsOrigin();
		m_vForceMoveTarget.z += HOVER_MIN_HEIGHT;
		m_bInitialPositionSet = true;
	}
	
	// If we're stalled, do nothing
	if ( m_flEngineStallTime > gpGlobals->curtime )
		return false;

	MaintainGroundHeight();

	if ( GetEnemy() )
	{
		TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );
	}

	if ( ( GetAbsOrigin() - m_vForceMoveTarget ).Length() > 12 )
	{
		MoveToTarget( flInterval, m_vForceMoveTarget );
	}
	else
	{
		const float myDecay	= 9.5f;
		Decelerate( flInterval, myDecay );
	}

	MoveExecute_Alive( flInterval );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::MoveExecute_Alive( float flInterval )
{
	PhysicsCheckWaterTransition();

	Vector vCurrentVelocity = GetCurrentVelocity();

	// FIXME: move this
	if ( VPhysicsGetObject() )
		VPhysicsGetObject()->Wake();

	if( m_fEnginePowerScale < GetMaxEnginePower() && gpGlobals->curtime > m_flWaterSuspendTime )
	{
		// Power is low, and we're no longer stuck in water, so bring power up.
		m_fEnginePowerScale += 0.05;
	}

	if ( IsFrozen() )
	{
		m_fEnginePowerScale = 0;
		vCurrentVelocity.z -= flInterval * sv_gravity.GetFloat();
	}

	// ----------------------------------------------------------------------------------------
	// Add in any forced velocity
	// ----------------------------------------------------------------------------------------
	SetCurrentVelocity( vCurrentVelocity + m_vForceVelocity );
	m_vForceVelocity = vec3_origin;

	AddNoiseToVelocity( 1 );

	LimitSpeed( 200, 100 );

	// CheckCollisions( flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//			reason - 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	{
		// Suppress collisions between the manhack and the player; we're currently bumping
		// almost certainly because it's not purely a physics object.
		SetOwnerEntity( pPhysGunUser );
		m_bHeld = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysGunUser - 
//			Reason - 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason )
{
	// Stop suppressing collisions between the manhack and the player
	SetOwnerEntity( NULL );

	m_bHeld = false;

	{
		m_hPhysicsAttacker = NULL;
		m_flLastPhysicsInfluenceTime = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Whether this should return carry angles
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_HoverTurret::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return m_bUseCarryAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const QAngle
//-----------------------------------------------------------------------------
QAngle CNPC_HoverTurret::PreferredCarryAngles( void )
{
	// FIXME: Embed this into the class
	static QAngle g_prefAngles;

	Vector vecUserForward;
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	pPlayer->EyeVectors( &vecUserForward );

	// If we're looking up, then face directly forward
	if ( vecUserForward.z >= 0.0f )
		return vec3_angle;

	// Otherwise, stay "upright"
	g_prefAngles.Init();
	g_prefAngles.x = -pPlayer->EyeAngles().x;
	
	return g_prefAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::Precache( void )
{
	PrecacheModel( HOVER_TURRET_MODEL );

	PrecacheModel(HOVER_TURRET_LASER_EFFECT);
	m_sLaserHaloSprite.GetForModify() = PrecacheModel(HOVER_TURRET_GLOW_SPRITE);

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// Override this task so we go for the enemy at eye level
	case TASK_HOVER_TURRET_HOVER:
		break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::Spawn(void)
{
	Precache();

	SetModel( HOVER_TURRET_MODEL );
	SetHullType( HULL_TINY_CENTERED );
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_VPHYSICS );

	m_iHealth			= sk_hover_turret_health.GetFloat();
	SetViewOffset( Vector(0, 0, 10) );		// Position of the eyes relative to NPC's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;
	m_NPCState			= NPC_STATE_NONE;

	SetNavType(NAV_FLY);

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL );
	AddEffects( EF_NOSHADOW );

	SetBloodColor( DONT_BLEED );
	SetCurrentVelocity( vec3_origin );
	m_vForceVelocity.Init();
	m_vCurrentBanking.Init();
	m_vTargetBanking.Init();

	m_flNextBurstTime	= gpGlobals->curtime;

	CapabilitiesAdd( bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_MOVE_FLY | bits_CAP_SQUAD );

	m_flNextEngineSoundTime		= gpGlobals->curtime;
	m_flWaterSuspendTime		= gpGlobals->curtime;
	m_flEngineStallTime			= gpGlobals->curtime;
	m_fForceMoveTime			= gpGlobals->curtime;
	m_vForceMoveTarget			= vec3_origin;
	m_fSwarmMoveTime			= gpGlobals->curtime;
	m_vSwarmMoveTarget			= vec3_origin;
	m_nLastSpinSound			= -1;

	m_fSmokeTime		= 0;
	m_fSparkTime		= 0;

	// Noise modifier
	Vector	bobAmount;
	bobAmount.x = random->RandomFloat( -1.0f, 1.0f );
	bobAmount.y = random->RandomFloat( -1.0f, 1.0f );
	bobAmount.z = random->RandomFloat( -1.0f, 1.0f );

	SetNoiseMod( bobAmount );

	m_iAmmoType = GetAmmoDef()->Index( "Pistol" );

	// Start out with full power! 
	m_fEnginePowerScale = GetMaxEnginePower();

	m_fHeadYaw = 0;

	NPCInit();

	// HoverTurrets are designed to slam into things, so don't take much damage from it!
	SetImpactEnergyScale( 0.001 );

	// HoverTurrets get 30 seconds worth of free knowledge.
	GetEnemies()->SetFreeKnowledgeDuration( 30.0 );

	// don't be an NPC, we want to collide with debris stuff
	SetCollisionGroup( COLLISION_GROUP_NONE );

	m_bHeld = false;

	if ( vec3_origin == GetAbsOrigin() )
	{
		m_bInitialPositionSet = false;
	}
	else
	{
		m_bInitialPositionSet = true;
		m_vForceMoveTarget = GetAbsOrigin();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{	
	case TASK_HOVER_TURRET_HOVER:
		break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output :
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::ClampMotorForces( Vector &linear, AngularImpulse &angular )
{
	// Msg("%.0f %.0f %.0f\n", linear.x, linear.y, linear.z );

	float fscale = 3000;

	if ( m_flEngineStallTime > gpGlobals->curtime )
	{
		linear.x = 0.0f;
		linear.y = 0.0f;
		// linear.z = clamp( linear.z, -fscale, fscale < 1200 ? 1200 : fscale );
		linear.z = 0.0f;
	}
	else
	{
		// limit reaction forces
		linear.x = clamp( linear.x, -fscale, fscale );
		linear.y = clamp( linear.y, -fscale, fscale );
		linear.z = clamp( linear.z, -fscale, fscale < 1200 ? 1200 : fscale );
	}
}

CBasePlayer *CNPC_HoverTurret::HasPhysicsAttacker( float dt )
{
	// If the player is holding me now, or I've been recently thrown
	// then return a pointer to that player
	/*
	if ( IsHeldByPhyscannon() || (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime) )
	{
		return m_hPhysicsAttacker;
	}
	*/
	return NULL;
}

//-----------------------------------------------------------------------------
// HoverTurrets that have been hacked by Alyx get more engine power (fly faster)
//-----------------------------------------------------------------------------
float CNPC_HoverTurret::GetMaxEnginePower()
{
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::MoveToTarget(float flInterval, const Vector &vMoveTarget)
{
	if ( flInterval <= 0 )
		return;

	if ( GetEnemy() != NULL )
	{
		TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );
	}
	else
	{
		TurnHeadToTarget( flInterval, vMoveTarget );
	}

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float	myAccel;
	float	myZAccel = 300.0f;
	float	myDecay	 = 0.3f;

	Vector targetDir;
	float flDist;

	Vector vecCurrentDir = GetCurrentVelocity();
	VectorNormalize( vecCurrentDir );

	targetDir = vMoveTarget - GetAbsOrigin();
	flDist = VectorNormalize( targetDir );

	float flDot = DotProduct( targetDir, vecCurrentDir );

	// Otherwise we should steer towards our goal
	if( flDot > 0.25 )
	{
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 300;
	}
	else
	{
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 200;
	}

	// Clamp lateral acceleration
	if ( myAccel > ( flDist / flInterval ) )
	{
		myAccel = flDist / flInterval;
	}

	// Clamp vertical movement
	if ( myZAccel > flDist / flInterval )
	{
		myZAccel = flDist / flInterval;
	}

	// Scale by our engine force
	myAccel *= m_fEnginePowerScale;
	myZAccel *= m_fEnginePowerScale;

	MoveInDirection( flInterval, targetDir, myAccel, myZAccel, myDecay );

	// calc relative banking targets
	Vector forward, right;
	GetVectors( &forward, &right, NULL );
	m_vTargetBanking.x	= 40 * DotProduct( forward, targetDir );
	m_vTargetBanking.z	= 40 * DotProduct( right, targetDir );
	m_vTargetBanking.y	= 0.0;
}

////-----------------------------------------------------------------------------
//// Purpose: Try to aim at the player and if we have aimed long enough then take the shot.
////-----------------------------------------------------------------------------
//void CNPC_HoverTurret::ShootThink()
//{
//	
//}

//-----------------------------------------------------------------------------
// Purpose: Fire!
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy )
{
	FireBulletsInfo_t info;

	CBaseEntity *pEnemy = GetEnemy();
	if( pEnemy )
	{
		Vector vecDir = vecDirToEnemy;

		info.m_vecSrc = vecSrc;
		info.m_vecDirShooting = vecDir;
		info.m_iTracerFreq = 1;
		info.m_iShots = 1;
		info.m_pAttacker = this;
		info.m_vecSpread = vec3_origin;
		info.m_flDistance = MAX_COORD_RANGE;
		info.m_iAmmoType = m_iAmmoType;
	}

	info.m_flDamageForceScale = HOVER_TURRET_BULLET_FORCE_MULTIPLIER;

	// Shoot out of the left barrel if there's nothing solid between the turret's center and the muzzle
	trace_t tr;
	Vector vecCenter = GetAbsOrigin();
	UTIL_TraceLine( vecCenter, info.m_vecSrc, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.m_pEnt || !tr.m_pEnt->IsWorld() )
	{
		FireBullets( info );
	}

	EmitSound( "NPC_FloorTurret.ShotSounds" );
	DoMuzzleFlash();

	{
//		m_pMotionController->Suspend( 2.0f );
//
		IPhysicsObject *pTurretPhys = VPhysicsGetObject();
		info.m_vecDirShooting.NormalizeInPlace();
		Vector vVelocityImpulse = info.m_vecDirShooting * -1.f * HOVER_TURRET_SHOT_KNOCKBACK;
		pTurretPhys->AddVelocity( &vVelocityImpulse, &vVelocityImpulse );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles turrets being set on fire by lasers
//-----------------------------------------------------------------------------
bool CNPC_HoverTurret::OnBurning( void )
{
	// Tick down
	m_iHealth -= 1;

//	if ( gpGlobals->curtime > m_fNextTalk )
//	{
//		EmitSound( "Portal.Glados_core.Death" );
//		m_fNextTalk = gpGlobals->curtime + 1.75f;
//	}

	if ( m_iHealth <= 0 )
	{
		Explode();
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles turrets being set on fire by lasers
//-----------------------------------------------------------------------------
void CNPC_HoverTurret::Explode( void )
{
	ExplosionCreate( WorldSpaceCenter(), vec3_angle, this, 1000, 500.0f, 
		SF_ENVEXPLOSION_NODAMAGE | SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS	|
		SF_ENVEXPLOSION_NOSMOKE  | SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );

	UTIL_ScreenShake( WorldSpaceCenter(), 25.0, 150.0, 1.0, 750.0f, SHAKE_START );

	CPVSFilter filter( WorldSpaceCenter() );
	Vector gibVelocity = RandomVector(-150,150);
	int iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( "MetalChunks" ) );	
	for ( int i = 0; i < 32; i++ )
	{
		te->BreakModel( filter, 0.0, WorldSpaceCenter(), vec3_angle, Vector(16,16,72), gibVelocity, iModelIndex, 400, 1, 2.5, BREAK_METAL );
	}

	AddEffects( EF_NODRAW );
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_hover_turret, CNPC_HoverTurret )

DECLARE_TASK( TASK_HOVER_TURRET_HOVER );

//=========================================================
// > SCHED_HOVER_TURRET_ATTACK_HOVER
//=========================================================
DEFINE_SCHEDULE
(
 SCHED_HOVER_TURRET_IDLE,

 "	Tasks"
 "		TASK_SET_ACTIVITY		ACTIVITY:ACT_FLY"
 "		TASK_HOVER_TURRET_HOVER		0"
 "	"
 "	Interrupts"
 "		COND_NEW_ENEMY"
 "		COND_ENEMY_DEAD"
 "		COND_LIGHT_DAMAGE"
 "		COND_HEAVY_DAMAGE"
 );

AI_END_CUSTOM_NPC()
