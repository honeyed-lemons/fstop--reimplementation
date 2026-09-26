//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. =====//
//
//  Purpose: 
//
//==========================================================================//

#include "cbase.h"

#include "doors.h"

#include "ai_basenpc.h"
#include "npcevent.h"
#include "particle_parse.h"
#include "npc_android.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNPC_Android_Basic : public CAI_BlendingHost<CNPC_Android>
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CNPC_Android_Basic, CAI_BlendingHost<CNPC_Android> );

public:
	virtual const char* GetBotModel( void );
	virtual bool	IsValidEnemy( CBaseEntity *pEnemy );
	virtual void BuildScheduleTestBits( void );

			void GatherConditions( void );
			int	 SelectSchedule ( void );
			void RunTask( const Task_t *pTask );

			Activity SelectDoorBash();
	virtual Class_T	Classify( void ) { return CLASS_ZOMBIE; }


private:

	CHandle< CBaseDoor > m_hBlockingDoor;
	float				 m_flDoorBashYaw;
	
	CRandSimTimer 		 m_DurationDoorBash;
	CSimTimer 	  		 m_NextTimeToStartDoorBash;

	Vector				 m_vPositionCharged;

	DEFINE_CUSTOM_AI;
};

LINK_ENTITY_TO_CLASS( npc_android_basic, CNPC_Android_Basic );

BEGIN_DATADESC( CNPC_Android_Basic )
	DEFINE_FIELD( m_hBlockingDoor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDoorBashYaw, FIELD_FLOAT ),

	DEFINE_EMBEDDED( m_DurationDoorBash ),
	DEFINE_EMBEDDED( m_NextTimeToStartDoorBash ),
	DEFINE_FIELD( m_vPositionCharged, FIELD_POSITION_VECTOR ),
END_DATADESC()

//=========================================================
// Conditions
//=========================================================
enum
{
	COND_BLOCKED_BY_DOOR = LAST_BASE_ANDROID_CONDITION,
	COND_DOOR_OPENED,
	COND_ANDROID_CHARGE_TARGET_MOVED,
};

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_ANDROID_BASH_DOOR = LAST_BASE_ANDROID_SCHEDULE,
	SCHED_ANDROID_WANDER_ANGRILY,
	SCHED_ANDROID_CHARGE_ENEMY,
	SCHED_ANDROID_FAIL,
};
enum
{
	TASK_ANDROID_EXPRESS_ANGER = LAST_BASE_ANDROID_TASK,
	TASK_ANDROID_YAW_TO_DOOR,
	TASK_ANDROID_ATTACK_DOOR,
	TASK_ANDROID_CHARGE_ENEMY,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_Android_Basic::GetBotModel( void )
{
	return "models/bot_male/bot_male.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Android_Basic::IsValidEnemy( CBaseEntity *pEnemy )
{
	return BaseClass::IsValidEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Android_Basic::SelectSchedule ( void )
{
	if( HasCondition( COND_PHYSICS_DAMAGE ) && !m_ActBusyBehavior.IsActive() )
	{
		return SCHED_FLINCH_PHYSICS;
	}

	switch ( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		if ( HasCondition( COND_NEW_ENEMY ) && GetEnemy() )
		{
			// if you have a new enemy anywhere on the map, try to hunt it down.
			if ( MustCloseToAttack() )
			{
				return SCHED_CHASE_ENEMY;
			}
		}

		if ( HasCondition( COND_LOST_ENEMY ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) && MustCloseToAttack() ) )
		{
			return SCHED_ANDROID_WANDER_MEDIUM;
		}

		if( HasCondition( COND_ANDROID_CAN_SWAT_ATTACK ) )
		{
			return SCHED_ANDROID_SWATITEM;
		}
		break;

	case NPC_STATE_ALERT:
		if ( HasCondition( COND_LOST_ENEMY ) || HasCondition( COND_ENEMY_DEAD ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) && MustCloseToAttack() ) )
		{
			ClearCondition( COND_LOST_ENEMY );
			ClearCondition( COND_ENEMY_UNREACHABLE );

			// Just lost track of our enemy. 
			// Wander around a bit so we don't look like a dingus.
			return SCHED_ANDROID_WANDER_MEDIUM;
		}
		break;
	}

	return BaseClass::SelectSchedule();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android_Basic::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if( !IsCurSchedule( SCHED_FLINCH_PHYSICS ) && !m_ActBusyBehavior.IsActive() )
	{
		SetCustomInterruptCondition( COND_PHYSICS_DAMAGE );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android_Basic::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ANDROID_ATTACK_DOOR:
		{
			if ( IsActivityFinished() )
			{
				if ( m_DurationDoorBash.Expired() )
				{
					TaskComplete();
					m_NextTimeToStartDoorBash.Reset();
				}
				else
					ResetIdealActivity( SelectDoorBash() );
			}
			break;
		}
	case TASK_ANDROID_CHARGE_ENEMY:
		{
			break;
		}

	case TASK_ANDROID_EXPRESS_ANGER:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}


	default:
		BaseClass::RunTask( pTask );
		break;
	}

}

//---------------------------------------------------------
//---------------------------------------------------------

Activity CNPC_Android_Basic::SelectDoorBash()
{
//	if ( random->RandomInt( 1, 3 ) == 1 )
	return ACT_MELEE_ATTACK1;
//	return (Activity)ACT_ANDROID_WALLPOUND;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android_Basic::GatherConditions( void )
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] = 
	{
		COND_BLOCKED_BY_DOOR,
		COND_DOOR_OPENED,
		COND_ANDROID_CHARGE_TARGET_MOVED,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	if ( m_hBlockingDoor == NULL || 
		 ( m_hBlockingDoor->m_toggle_state == TS_AT_TOP || 
		   m_hBlockingDoor->m_toggle_state == TS_GOING_UP )  )
	{
		ClearCondition( COND_BLOCKED_BY_DOOR );
		if ( m_hBlockingDoor != NULL )
		{
			SetCondition( COND_DOOR_OPENED );
			m_hBlockingDoor = NULL;
		}
	}
	else
		SetCondition( COND_BLOCKED_BY_DOOR );

	if ( ConditionInterruptsCurSchedule( COND_ANDROID_CHARGE_TARGET_MOVED ) )
	{
		if ( GetNavigator()->IsGoalActive() )
		{
			const float CHARGE_RESET_TOLERANCE = 60.0;
			if ( !GetEnemy() ||
				 ( m_vPositionCharged - GetEnemyLKP()  ).Length() > CHARGE_RESET_TOLERANCE )
			{
				SetCondition( COND_ANDROID_CHARGE_TARGET_MOVED );
			}
				 
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_android_basic, CNPC_Android_Basic )

	DECLARE_CONDITION( COND_BLOCKED_BY_DOOR )
	DECLARE_CONDITION( COND_DOOR_OPENED )
	DECLARE_CONDITION( COND_ANDROID_CHARGE_TARGET_MOVED )

	DECLARE_TASK( TASK_ANDROID_EXPRESS_ANGER )
	DECLARE_TASK( TASK_ANDROID_YAW_TO_DOOR )
	DECLARE_TASK( TASK_ANDROID_ATTACK_DOOR )
	DECLARE_TASK( TASK_ANDROID_CHARGE_ENEMY )
	
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_WANDER_ANGRILY,

		"	Tasks"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			4"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_DOOR_OPENED"
	)

AI_END_CUSTOM_NPC()