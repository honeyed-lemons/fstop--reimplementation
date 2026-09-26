//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
//  Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "npcevent.h"
#include "particle_parse.h"
#include "ai_hint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Custom schedules.
//
enum
{
	SCHED_CHICKEN_IDLE_WALK = LAST_SHARED_SCHEDULE,

	SCHED_CHICKEN_WALK_AWAY,
	SCHED_CHICKEN_RUN_AWAY,
//	SCHED_CHICKEN_CHASE_ENEMY,
	SCHED_CHICKEN_SQUAWK,
	SCHED_CHICKEN_IDLE_STAND,
	SCHED_CHICKEN_FALL,
	SCHED_CHICKEN_HIT_GROUND,
	SCHED_CHICKEN_ROOST
};

enum 
{
	TASK_CHICKEN_PICK_RANDOM_GOAL = LAST_SHARED_TASK,
	TASK_CHICKEN_PICK_EVADE_GOAL,
	TASK_CHICKEN_FIND_PATH_TO_NEST,
};

enum
{
	COND_CHICKEN_ENEMY_TOO_CLOSE = LAST_SHARED_CONDITION,
	COND_CHICKEN_ENEMY_WAY_TOO_CLOSE,
	COND_CHICKEN_RELEASED,
	COND_CHICKEN_OFF_GROUND,
	COND_CHICKEN_HIT_GROUND
};

int AE_CHICKEN_PECK;
int AE_CHICKEN_FOOTSTEP_RIGHT;
int AE_CHICKEN_FOOTSTEP_LEFT;

class CNPC_Chicken : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CNPC_Chicken, CAI_BaseNPC  );
	DECLARE_DATADESC();

	virtual int		SelectSchedule( void );
	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	StartTask( const Task_t *pTask );
	virtual void	GatherEnemyConditions( CBaseEntity *pEnemy );
	virtual void	HandleAnimEvent( animevent_t *pEvent );
	virtual int		MeleeAttack1Conditions( float flDot, float flDist );
	virtual bool	IsValidEnemy( CBaseEntity *pEnemy );
	virtual float	MaxYawSpeed( void );
	virtual float	GetIdealAccel( void ) const;
	virtual bool	ShouldPlayIdleSound( void );
	virtual void	IdleSound( void );
	virtual void	PrescheduleThink( void );
	virtual bool	FInViewCone( CBaseEntity *pEntity );
	virtual int		TranslateSchedule( int scheduleType );
	virtual void	GatherConditions( void );
	virtual bool	IsInterruptable( void );
	virtual void	OnScheduleChange( void );

	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );
	virtual int				IRelationPriority( CBaseEntity *pTarget );

	virtual void OnCaptured( void );
	virtual void OnReleased( void );

	virtual Class_T	Classify( void ) { return CLASS_EARTH_FAUNA; }
	virtual bool	FValidateHintType( CAI_Hint *pHint );

private:	
	bool	FindNest( void );
	void	FootstepSound( bool bRightFoot );

	CHandle< CAI_Hint >		m_hNestNode;

	DEFINE_CUSTOM_AI;

	float	m_flNextCluckTime;
};

LINK_ENTITY_TO_CLASS( npc_chicken, CNPC_Chicken );

BEGIN_DATADESC( CNPC_Chicken )
	DEFINE_FIELD( m_flNextCluckTime, FIELD_TIME ),
END_DATADESC()

#define MODEL_CHICKEN	"models/chicken/chicken.mdl"	// FIXME: Change

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::Precache( void )
{
	PrecacheModel( MODEL_CHICKEN );
	PrecacheParticleSystem( "feathers_single" );
	PrecacheParticleSystem( "feathers_small" );
	PrecacheParticleSystem( "feathers_large" );
	PrecacheParticleSystem( "zombie_stomp_heavy" );
	
	PrecacheScriptSound( "NPC_BaseZombie.Swat" );
	PrecacheScriptSound( "NPC_Chicken.Clucks" );
	PrecacheScriptSound( "NPC_Chicken.Squawk" );
	PrecacheScriptSound( "NPC_Chicken.Startle" );

	PrecacheScriptSound( "NPC_Strider.Footstep" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::ShouldPlayIdleSound( void )
{
	return ( m_flNextCluckTime < gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::IdleSound( void )
{
	EmitSound( "NPC_Chicken.Clucks" );
	m_flNextCluckTime = gpGlobals->curtime + random->RandomFloat( 2.5f, 4.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::IsValidEnemy( CBaseEntity *pEnemy )
{
	// if ( pEnemy && pEnemy->IsPlayer() )
	// 	return false;
	
	if ( IRelationType( pEnemy ) == D_FR )
	{
		if ( ( pEnemy->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr() > Square(20*12) )
			return false;
	}

	return BaseClass::IsValidEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Chicken::MaxYawSpeed( void )
{
	return 60;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Chicken::GetIdealAccel( void ) const
{
	// return GetIdealSpeed() * 2.0f;
	return BaseClass::GetIdealAccel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::Spawn( void )
{
	Precache();

	SetModel( MODEL_CHICKEN );

	SetHullType( HULL_TINY );
	
	SetHullSizeNormal();
	SetDefaultEyeOffset();

	SetNavType( NAV_GROUND );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	
	SetMoveType( MOVETYPE_STEP );

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 );

	NPCInit();

	BaseClass::Spawn();

	// Never die!
	m_takedamage = DAMAGE_NO;
	SetHealth( 100 );
	SetMaxHealth( 100 );

	m_hNestNode = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_CHICKEN_PICK_EVADE_GOAL:
		{
			if ( GetEnemy() != NULL )
			{
				// Get our enemy's position in x/y.
				Vector vecEnemyOrigin = GetEnemy()->GetAbsOrigin();
				vecEnemyOrigin.z = GetAbsOrigin().z;

				// Pick a hop goal a random distance along a vector away from our enemy.
				m_vSavePosition = GetAbsOrigin() - vecEnemyOrigin;
				VectorNormalize( m_vSavePosition );
				m_vSavePosition = GetAbsOrigin() + m_vSavePosition * ( 128 + random->RandomInt( 0, 64 ) );

				GetMotor()->SetIdealYawToTarget( m_vSavePosition );
				TaskComplete();
			}
			else
			{
				TaskFail( "No enemy" );
			}
			break;
		}

		case TASK_CHICKEN_PICK_RANDOM_GOAL:
		{
			m_vSavePosition = GetLocalOrigin() + Vector( random->RandomFloat( -48.0f, 48.0f ), random->RandomFloat( -48.0f, 48.0f ), 0 );
			TaskComplete();
			break;
		}

		case TASK_MELEE_ATTACK1:
		{
			SetIdealActivity( (Activity)ACT_MELEE_ATTACK1 );
			break;
		}

		case TASK_CHICKEN_FIND_PATH_TO_NEST:
			{
				if ( m_hNestNode == NULL )
				{
					TaskFail( "Nest hint node missing!\n" );
					return;
				}

				m_vSavePosition = m_hNestNode->GetAbsOrigin();
				ChainStartTask( TASK_GET_PATH_TO_SAVEPOSITION );
				//TaskComplete();
			}
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEnemy - 
//-----------------------------------------------------------------------------
void CNPC_Chicken::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	ClearCondition( COND_CHICKEN_ENEMY_WAY_TOO_CLOSE );
	ClearCondition( COND_CHICKEN_ENEMY_TOO_CLOSE );
	ClearCondition( COND_SEE_HATE );

	Disposition_t relationType = IRelationType( pEnemy );
	float flEnemyDistSqr = (GetAbsOrigin() - pEnemy->GetAbsOrigin()).LengthSqr();

	if ( relationType == D_FR )
	{
		if ( flEnemyDistSqr < Square(10.0f*12.0f) )
		{
			SetCondition( COND_CHICKEN_ENEMY_WAY_TOO_CLOSE );
		}

		if ( flEnemyDistSqr < Square(20.0f*12.0f) )
		{
			SetCondition( COND_CHICKEN_ENEMY_TOO_CLOSE );
		}
	}
	else if ( relationType == D_HT )
	{
		if ( flEnemyDistSqr < Square(20.0f*12.0f) )
		{
			SetCondition( COND_SEE_HATE ); // FIXME: Questionable
		}
	}

	BaseClass::GatherEnemyConditions( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::FindNest( void )
{
	m_hNestNode = CAI_HintManager::FindHint( this, HINT_PORTAL2_NEST, bits_HINT_NODE_NEAREST, (40*12) );
	if ( m_hNestNode )
	{
		m_hNestNode->Lock( this );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::OnScheduleChange( void )
{
	if ( IsCurSchedule( SCHED_CHICKEN_ROOST ) == false )
	{
		if ( m_hNestNode )
		{
			m_hNestNode->Unlock();
			m_hNestNode = NULL;
		}
	}

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Chicken::SelectSchedule( void )
{
	// If we hit the ground, then bounce!
	if ( HasCondition( COND_CHICKEN_HIT_GROUND ) )
	{
		DispatchParticleEffect( "feathers_small", WorldSpaceCenter(), WorldSpaceCenter(), GetAbsAngles() );
		return SCHED_CHICKEN_HIT_GROUND;
	}

	// Squawk and complain if we were just released from a picture
	if ( HasCondition( COND_CHICKEN_RELEASED ) )
	{
		SetIdealActivity( (Activity)ACT_GLIDE );

		ClearCondition( COND_CHICKEN_RELEASED );

		// If we're off the ground, we need to flap and fall!
		int nSchedule = SCHED_CHICKEN_FALL;

		// Squawk and wait to cluck
		EmitSound( "NPC_Chicken.Startle" );
		m_flNextCluckTime = gpGlobals->curtime + random->RandomFloat( 2.5f, 4.0f );
		
		return nSchedule;
	}

	// Run if an enemy is too close to us
	if ( HasCondition( COND_CHICKEN_ENEMY_WAY_TOO_CLOSE ) )
		return SCHED_CHICKEN_RUN_AWAY;

	// Walk if they're just getting near
	if ( HasCondition( COND_CHICKEN_ENEMY_TOO_CLOSE ) )
		return SCHED_CHICKEN_WALK_AWAY;

	// Look for a nest if we're not in one already
	bool bRoosting = IsCurSchedule( SCHED_CHICKEN_ROOST );
	if ( bRoosting == false )
	{
		if ( FindNest() )
			return SCHED_CHICKEN_ROOST;
	}

	// Randomly patrol around
	if ( random->RandomInt( 0, 4 ) == 0 )
		return SCHED_CHICKEN_IDLE_WALK;

	// Chill
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CNPC_Chicken::IRelationType( CBaseEntity *pTarget )
{
	// For now, don't hate the player
	if ( pTarget && pTarget->IsPlayer() )
	{
		if ( GetObjectScaleLevel() == 1 )
			return D_HT;

		return D_NU;
	}

	// Don't worry about things we're already neutral to
	if ( BaseClass::IRelationType( pTarget ) == D_NU )
		return D_NU;

	CBaseAnimating *pTargetAnim = pTarget->GetBaseAnimating();
	int nEnemyObjectScale = 0;
	if ( pTargetAnim )
	{
		nEnemyObjectScale = pTargetAnim->GetObjectScaleLevel();
	}

	// Find the difference in scale between the objects
	int nScaleDiff = ( nEnemyObjectScale - GetObjectScaleLevel() );

	// They're larger, so we're scared
	if ( nScaleDiff >= 0 )
		return D_FR;
	
	// We're larger, let's eat them!
	if ( nScaleDiff < 0 )
		return D_HT;

	return D_NU;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNPC_Chicken::IRelationPriority( CBaseEntity *pTarget )
{
	// At this size, we're neutral to everything
	if ( GetObjectScaleLevel() == 1 || pTarget->IsPlayer() )
		return 1;

	// Find our distance
	float flDistSqr = ( pTarget->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
	float flPriority = clamp( flDistSqr / Square( 50*12 ), 0.0f, 1.0f );
	
	// Break this into some amount of granularity and prioritize for things closest to us
	return ( floor( 1.0f - flPriority ) * 10 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::FootstepSound( bool bRightFoot )
{
	Vector vecFootPos;
	QAngle vecFootAngles;
	GetAttachment( ((bRightFoot) ? "rfoot" : "lfoot" ), vecFootPos, vecFootAngles );
	DispatchParticleEffect( "zombie_stomp_heavy", vecFootPos, vec3_angle );

	// Shake nearby players
	UTIL_ScreenShake( GetAbsOrigin(), 8.0f, 1.0f, 1.0f, (60.0f*12.0f), SHAKE_START );

	CPASAttenuationFilter filter( this, "NPC_Strider.Footstep" );
	EmitSound( filter, 0, "NPC_Strider.Footstep", &vecFootPos, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::HandleAnimEvent( animevent_t *pEvent )
{
	// Peck!
	if ( pEvent->Event() == AE_CHICKEN_PECK)
	{
		// Get our beak's position
		Vector vecPeckPos;
		QAngle vecPeckAngles;
		GetAttachment( "beak", vecPeckPos, vecPeckAngles );
		
		// Damage direction
		Vector vecDamageForce;
		AngleVectors( vecPeckAngles, &vecDamageForce );
		vecDamageForce *= 250.0f;

		// Deal the damage in a small radius around our beak
		RadiusDamage( CTakeDamageInfo( this, this, vecDamageForce, vecPeckPos, 50.0f, DMG_CRUSH ), vecPeckPos, (8*12.0f), CLASS_EARTH_FAUNA, this );

		// FIXME: Temp
		EmitSound( "NPC_BaseZombie.Swat" );
	}

	if ( pEvent->Event() == AE_CHICKEN_FOOTSTEP_RIGHT || pEvent->Event() == AE_CHICKEN_FOOTSTEP_LEFT )
	{
		// Only bother if we're big!
		if ( GetObjectScaleLevel() == 1 )
		{
			FootstepSound( pEvent->Event() == AE_CHICKEN_FOOTSTEP_RIGHT );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Chicken::MeleeAttack1Conditions( float flDot, float flDist )
{
	// If we fear our enemy, then don't bother to attack this way!
	if ( IRelationType( GetEnemy() ) == D_FR )
		return 0;

	if ( flDot < cosf(DEG2RAD(45)) )// Yes I'm too lazy to look this up
									// Yes, I realize that typing this has taken longer than looking it up
									//
									// ... now I'm just being willful.
		return COND_NOT_FACING_ATTACK;

	if ( flDist > (2.5f*12)*GetModelScale() )
		return COND_TOO_FAR_TO_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::OnCaptured( void )
{
	DispatchParticleEffect( "feathers_small", WorldSpaceCenter(), WorldSpaceCenter(), GetAbsAngles() );

	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::OnReleased( void )
{
	DispatchParticleEffect( "feathers_small", WorldSpaceCenter(), WorldSpaceCenter(), GetAbsAngles() );
	SetCondition( COND_CHICKEN_RELEASED );

	BaseClass::OnReleased();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::FInViewCone( CBaseEntity *pEntity )
{
	// HACK
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	if ( GetActivity() == ACT_RUN || GetActivity() == ACT_GLIDE )
	{
		if ( random->RandomInt( 0, 1 ) == 0 )
		{
			DispatchParticleEffect( "feathers_single", WorldSpaceCenter(), WorldSpaceCenter(), GetAbsAngles() );
		}
	}

	// Fix-up our nest node
	if ( IsCurSchedule( SCHED_CHICKEN_ROOST ) == false )
	{
		if ( m_hNestNode != NULL )
		{
			m_hNestNode->Unlock();
			m_hNestNode = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNPC_Chicken::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_IDLE_STAND )
		return SCHED_CHICKEN_IDLE_STAND;

	return BaseClass::TranslateSchedule( scheduleType );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Chicken::GatherConditions( void )
{
	if ( GetGroundEntity() == NULL )
	{
		// Push out our next idle 
		m_flNextCluckTime = gpGlobals->curtime + random->RandomFloat( 2.5f, 4.0f );
		SetCondition( COND_CHICKEN_OFF_GROUND );
	}
	else
	{
		// If we were off the ground on the last frame, we were in the air!
		if ( HasCondition( COND_CHICKEN_OFF_GROUND ) )
		{
			SetCondition( COND_CHICKEN_HIT_GROUND );
		}

		ClearCondition( COND_CHICKEN_OFF_GROUND );
	}

	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::FValidateHintType( CAI_Hint *pHint )
{
	if ( pHint->HintType() == HINT_PORTAL2_NEST )
		return true;

	return BaseClass::FValidateHintType( pHint );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Chicken::IsInterruptable( void )
{
	// If we're roosting, we only interrupt if an enemy is too close to us
	bool bIsRoosting = IsCurSchedule( SCHED_CHICKEN_ROOST );
	bool bEnemyTooClose = HasCondition( COND_CHICKEN_ENEMY_TOO_CLOSE ) || HasCondition( COND_CHICKEN_ENEMY_WAY_TOO_CLOSE );
	if ( bIsRoosting && HasCondition( COND_NEW_ENEMY ) && bEnemyTooClose == false )
		return false;

	return BaseClass::IsInterruptable();
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_chicken, CNPC_Chicken )

	DECLARE_TASK( TASK_CHICKEN_PICK_RANDOM_GOAL )
	DECLARE_TASK( TASK_CHICKEN_PICK_EVADE_GOAL )
	DECLARE_TASK( TASK_CHICKEN_FIND_PATH_TO_NEST )
	
	DECLARE_ANIMEVENT( AE_CHICKEN_PECK )
	DECLARE_ANIMEVENT( AE_CHICKEN_FOOTSTEP_RIGHT )
	DECLARE_ANIMEVENT( AE_CHICKEN_FOOTSTEP_LEFT )

	DECLARE_CONDITION( COND_CHICKEN_ENEMY_TOO_CLOSE )
	DECLARE_CONDITION( COND_CHICKEN_ENEMY_WAY_TOO_CLOSE )
	DECLARE_CONDITION( COND_CHICKEN_RELEASED )
	DECLARE_CONDITION( COND_CHICKEN_OFF_GROUND )
	DECLARE_CONDITION( COND_CHICKEN_HIT_GROUND )

	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_IDLE_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING		1"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT				5"
		"		TASK_WAIT_PVS			0"
		""
		"	Interrupts"
		"		COND_CHICKEN_ENEMY_TOO_CLOSE"
		"		COND_CHICKEN_ENEMY_WAY_TOO_CLOSE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_IDLE_INTERRUPT"
		"		COND_CHICKEN_OFF_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_IDLE_WALK,
		
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
		"		TASK_CHICKEN_PICK_RANDOM_GOAL	0"
		"		TASK_GET_PATH_TO_SAVEPOSITION	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0"
		"		"
		"	Interrupts"
		"		COND_CHICKEN_ENEMY_TOO_CLOSE"
		"		COND_CHICKEN_ENEMY_WAY_TOO_CLOSE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_HATE"
		"		COND_CHICKEN_OFF_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_WALK_AWAY,
		
		"	Tasks"
		"		TASK_CHICKEN_PICK_EVADE_GOAL	0"
		"		TASK_GET_PATH_TO_SAVEPOSITION	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		"
		"	Interrupts"
		"		COND_CHICKEN_ENEMY_WAY_TOO_CLOSE"
		"		COND_HEAVY_DAMAGE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_CHICKEN_OFF_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_RUN_AWAY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_CHICKEN_PICK_EVADE_GOAL	0"
		"		TASK_GET_PATH_TO_SAVEPOSITION	0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CHICKEN_OFF_GROUND"
	)

	////=========================================================
	//DEFINE_SCHEDULE
	//(
	//	SCHED_CHICKEN_CHASE_ENEMY,

	//	"	Tasks"
	//	"		TASK_STOP_MOVING				0"
	//	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
	//	"		TASK_SET_TOLERANCE_DISTANCE		24"
	//	"		TASK_GET_CHASE_PATH_TO_ENEMY	400"
	//	"		TASK_WALK_PATH					0"
	//	"		TASK_WAIT_FOR_MOVEMENT			0"
	//	"		TASK_FACE_ENEMY					0"
	//	""
	//	"	Interrupts"
	//	"		COND_NEW_ENEMY"
	//	"		COND_ENEMY_DEAD"
	//	"		COND_ENEMY_UNREACHABLE"
	//	"		COND_CAN_MELEE_ATTACK1"
	//	"		COND_TOO_CLOSE_TO_ATTACK"
	//	"		COND_TASK_FAILED"
	//	"		COND_LOST_ENEMY"
	//	"		COND_CHICKEN_OFF_GROUND"
	//)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_SQUAWK,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_JUMP"
		""
		"	Interrupts"
		"		COND_CHICKEN_ENEMY_WAY_TOO_CLOSE"
		"		COND_CHICKEN_ENEMY_TOO_CLOSE"
		"		COND_CHICKEN_OFF_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_FALL,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_GLIDE"
		""
		"	Interrupts"
		"		COND_CHICKEN_HIT_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_HIT_GROUND,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_LAND"
		""
		"	Interrupts"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CHICKEN_ROOST,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_CHICKEN_FIND_PATH_TO_NEST	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CROUCH"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CROUCHIDLE"
		"		TASK_WAIT_INDEFINITE			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_HATE"
		"		COND_CHICKEN_ENEMY_TOO_CLOSE"
		"		COND_CHICKEN_ENEMY_WAY_TOO_CLOSE"
	)

AI_END_CUSTOM_NPC()
