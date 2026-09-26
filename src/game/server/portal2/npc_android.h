//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. =====//
//
//  Purpose: 
//
//==========================================================================//

#ifndef NPC_ANDROID_H
#define NPC_ANDROID_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_blended_movement.h"
#include "soundenvelope.h"
#include "ai_behavior_actbusy.h"
#include "ai_baseactor.h"

#define ANDROID_MELEE_REACH	55

extern int AE_ANDROID_STEP_LEFT;
extern int AE_ANDROID_STEP_RIGHT;

//
// Custom schedules.
//

// Pass these to claw attack so we know where to draw the blood.
#define ANDROID_BLOOD_LEFT_HAND		0
#define ANDROID_BLOOD_RIGHT_HAND		1
#define ANDROID_BLOOD_BOTH_HANDS		2
#define ANDROID_BLOOD_BITE			3


enum
{
	SCHED_ANDROID_IDLE_WALK = LAST_SHARED_SCHEDULE,

	SCHED_ANDROID_WALK_AWAY,
	SCHED_ANDROID_RUN_AWAY,
	SCHED_ANDROID_CHASE_ENEMY,
	SCHED_ANDROID_SQUAWK,
	SCHED_ANDROID_IDLE_STAND,
	SCHED_ANDROID_FALL,
	SCHED_ANDROID_HIT_GROUND,
	SCHED_ANDROID_STUMBLE,
	SCHED_ANDROID_RISE,

	SCHED_ANDROID_MOVE_TO_AMBUSH,
	SCHED_ANDROID_WAIT_AMBUSH,
	SCHED_ANDROID_WANDER_MEDIUM,	// medium range wandering behavior.
	SCHED_ANDROID_SWATITEM,

	SCHED_ANDROID_MOVE_SWATITEM,
	SCHED_ANDROID_ATTACKITEM,
	SCHED_ANDROID_WANDER_FAIL,
	SCHED_ANDROID_WANDER_STANDOFF,
	SCHED_ANDROID_MELEE_ATTACK1,
	SCHED_ANDROID_POST_MELEE_WAIT,

	LAST_BASE_ANDROID_SCHEDULE,
};

enum 
{
	TASK_ANDROID_PICK_RANDOM_GOAL = LAST_SHARED_TASK,
	TASK_ANDROID_PICK_EVADE_GOAL,
	TASK_ANDROID_SWAT_ITEM,
	TASK_ANDROID_DELAY_SWAT,
	TASK_ANDROID_GET_PATH_TO_PHYSOBJ,
	TASK_ANDROID_WAIT_POST_MELEE,

	LAST_BASE_ANDROID_TASK,
};

enum
{
	COND_ANDROID_ENEMY_TOO_CLOSE = LAST_SHARED_CONDITION,
	COND_ANDROID_ENEMY_WAY_TOO_CLOSE,
	COND_ANDROID_RELEASED,
	COND_ANDROID_OFF_GROUND,
	COND_ANDROID_HIT_GROUND,
	COND_ANDROID_RISE_FROM_GROUND,
	COND_ANDROID_CAN_SWAT_ATTACK,
	
	COND_ANDROID_LOCAL_MELEE_OBSTRUCTION,


	LAST_BASE_ANDROID_CONDITION
};


class CNPC_Android : public CAI_BaseActor
{
public:
	DECLARE_CLASS( CNPC_Android, CAI_BaseActor  );
	DECLARE_DATADESC();

	virtual void	BuildScheduleTestBits( void );
	virtual void	OnScheduleChange( void );
	virtual int		SelectSchedule( void );
	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	StartTask( const Task_t *pTask );
	virtual void	HandleAnimEvent( animevent_t *pEvent );

	virtual void	RunTask( const Task_t *pTask );

	// Swatting physics objects
	int GetSwatActivity( void );
	bool FindNearestPhysicsObject( int iMaxMass );
	float DistToPhysicsEnt( void );
	virtual bool CanSwatPhysicsObjects( void ) { return true; }
	
	// default zombies are melee
	virtual bool	MustCloseToAttack(void) { return true; }

			int		GetAttackActivity( void );
	virtual int		MeleeAttack1Conditions( float flDot, float flDist );
	virtual float	GetClawAttackRange() { return (ANDROID_MELEE_REACH * GetScaleRangeScalar() ); }
			float	GetScaleRangeScalar();
	virtual CBaseEntity *ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch, int BloodOrigin );
	virtual bool	IsValidEnemy( CBaseEntity *pEnemy );
	virtual bool	ShouldPlayIdleSound( void );
	virtual void	IdleSound( void );
	virtual void	AttackSound( void );
	virtual void	AttackMissSound( void );
	virtual void	AttackHitSound( void );
	virtual void	MoanSound( void );
	virtual void	MoveStartSound( void );
	virtual int		TranslateSchedule( int scheduleType );
	virtual void	GatherConditions( void );
	virtual void	PoundSound();
			void	FootstepSound( bool fRightFoot );

	int OnTakeDamage_Alive( const CTakeDamageInfo &info );

	// Custom damage/death 
	bool ShouldIgnite( const CTakeDamageInfo &info );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	void CopyRenderColorTo( CBaseEntity *pOther );
			bool	AllowedToIgnite( void ) { return true; }

	virtual void	StartTouch( CBaseEntity *pOther );
			void	HeavyFootstep( bool isRightFoot );

			void	InputRiseFromGround( inputdata_t &inputdata );

	virtual void	SetAndroidModel( void );

	virtual const char*		GetBotModel( void );

	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );

	virtual void OnCaptured( void );
	virtual void OnReleased( void );

	virtual Class_T	Classify( void ) { return CLASS_CITIZEN_PASSIVE; }	// FIXME: JDWFIXME

	CAI_ActBusyBehavior		m_ActBusyBehavior;

protected:
	DEFINE_CUSTOM_AI;

	bool	m_bLongFall;
	float	m_flBurnDamage;				// Keeps track of how much burn damage we've incurred in the last few seconds.
	float	m_flBurnDamageResetTime;	// Time at which we reset the burn damage.

	EHANDLE m_hPhysicsEnt;

	float m_flNextMoanSound;
	float m_flNextSwat;
	float m_flNextSwatScan;

	float	m_flNextFlinch;

	float	m_flNextMoanTime;

	static int ACT_DROID_MELEE_CHOP;
	static int ACT_DROID_JUMPSWIPE;
	static int ACT_DROID_SWATRIGHTMID;
	static int ACT_DROID_SWATRIGHTLOW;
	static int ACT_DROID_SWATLEFTMID;
	static int ACT_DROID_SWATLEFTLOW;
	static int ACT_ANDROID_RISE_FROM_GROUND;
	
};

#endif // NPC_ANDROID_H