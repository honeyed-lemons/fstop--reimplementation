//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. =====//
//
//  Purpose: 
//
//==========================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "npcevent.h"
#include "particle_parse.h"
#include "entityflame.h"
#include "props.h"


#include "npc_android.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// if the zombie doesn't find anything closer than this, it doesn't swat.
#define ANDROID_FARTHEST_PHYSICS_OBJECT	40.0*12.0
#define ANDROID_PHYSICS_SEARCH_DEPTH	100

// Don't swat objects unless player is closer than this.
#define ANDROID_PLAYER_MAX_SWAT_DIST		1000

//
// The heaviest physics object that a zombie should try to swat. (kg)
#define ANDROID_MAX_PHYSOBJ_MASS		60

//
// Zombie tries to get this close to a physics object's origin to swat it
#define ANDROID_PHYSOBJ_SWATDIST		80

//
// Because movement code sometimes doesn't get us QUITE where we
// want to go, the zombie tries to get this close to a physics object
// Zombie will end up somewhere between PHYSOBJ_MOVE_TO_DIST & PHYSOBJ_SWATDIST
#define ANDROID_PHYSOBJ_MOVE_TO_DIST	48

//
// How long between physics swat attacks (in seconds). 
#define ANDROID_SWAT_DELAY			5


//
// After taking damage, ignore further damage for n seconds. This keeps the zombie
// from being interrupted while.f
//
#define ANDROID_FLINCH_DELAY			3


#define ANDROID_BURN_TIME		10 // If ignited, burn for this many seconds
#define ANDROID_BURN_TIME_NOISE	2  // Give or take this many seconds.

ConVar	android_dmg_normal( "android_dmg_normal","25");
ConVar	android_dmg_big( "android_dmg_big","50");

int AE_ANDROID_PECK;
int AE_ANDROID_STEP_LEFT;
int AE_ANDROID_STEP_RIGHT;
int AE_ANDROID_MOVESTART_LEFT;
int AE_ANDROID_MOVESTART_RIGHT;
int AE_ANDROID_ATTACK_RIGHT;
int AE_ANDROID_ATTACK_LEFT;
int AE_ANDROID_ATTACK_BOTH;
int AE_ANDROID_SWATITEM;
int AE_ANDROID_STARTSWAT;
int AE_ANDROID_WIFF;
int AE_ANDROID_SCUFF_LEFT;
int AE_ANDROID_SCUFF_RIGHT;
int AE_ANDROID_ATTACK_SCREAM;
int AE_ANDROID_GET_UP;
int AE_ANDROID_POUND;
int AE_ANDROID_ALERTSOUND;

// Private activities
int CNPC_Android::ACT_DROID_MELEE_CHOP;
int CNPC_Android::ACT_DROID_JUMPSWIPE;
int CNPC_Android::ACT_DROID_SWATRIGHTMID;
int CNPC_Android::ACT_DROID_SWATRIGHTLOW;
int CNPC_Android::ACT_DROID_SWATLEFTMID;
int CNPC_Android::ACT_DROID_SWATLEFTLOW;
int CNPC_Android::ACT_ANDROID_RISE_FROM_GROUND;

LINK_ENTITY_TO_CLASS( npc_android, CNPC_Android );

BEGIN_DATADESC( CNPC_Android )
	DEFINE_INPUTFUNC( FIELD_VOID, "RiseFromGround", InputRiseFromGround ),

	DEFINE_FIELD( m_bLongFall, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flBurnDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBurnDamageResetTime, FIELD_FLOAT ),

	DEFINE_FIELD( m_hPhysicsEnt, FIELD_EHANDLE ),

	DEFINE_FIELD( m_flNextMoanSound, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextSwat, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextSwatScan, FIELD_FLOAT ),

	DEFINE_FIELD( m_flNextFlinch, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextMoanTime, FIELD_FLOAT ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_Android::GetBotModel( void )
{
	string_t name = GetModelName();
	if( !name )
	{
		return "models/bot_male/bot_male.mdl";
	}
	
	return STRING( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::Precache( void )
{
	PrecacheModel( GetBotModel() );

	PrecacheParticleSystem( "zombie_stomp_heavy" );
	PrecacheParticleSystem( "GrubBlood" );

	PrecacheScriptSound( "NPC_Strider.Footstep" );
	PrecacheScriptSound( "NPC_BaseZombie.Swat" );

	PrecacheScriptSound( "Zombie.FootstepRight" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );

	PrecacheScriptSound( "Zombie.Attack" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombie.AttackHit" );

//	PrecacheScriptSound( "NPC_Android.FootstepLeft" );
//	PrecacheScriptSound( "NPC_Android.FootstepRight" );

	PrecacheScriptSound( "NPC_Android.Squash" );
	PrecacheScriptSound( "NPC_Android.Servo" );
//	PrecacheScriptSound( "NPC_Android.Windup" );
	PrecacheScriptSound( "NPC_Android.Pain" );
	
	UTIL_PrecacheOther( "_squashed_zombie" );
}

////-----------------------------------------------------------------------------
//// Purpose: 
////-----------------------------------------------------------------------------
//void CNPC_Android::IdleSound( void )
//{
//}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Android::IsValidEnemy( CBaseEntity *pEnemy )
{
////	if ( pEnemy && pEnemy->IsPlayer() )
//	return false;
//
	return BaseClass::IsValidEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: Choose the right model for our android
//-----------------------------------------------------------------------------
void CNPC_Android::SetAndroidModel( void )
{
	SetModel( GetBotModel()  );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::Spawn( void )
{
	Precache();

	SetAndroidModel();

	SetHullType( HULL_HUMAN );

	SetHullSizeNormal();
	SetDefaultEyeOffset();

	SetNavType( NAV_GROUND );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_STEP );

	m_flNextSwat = gpGlobals->curtime;
	m_flNextSwatScan = gpGlobals->curtime;

	m_flNextMoanTime = gpGlobals->curtime + 9999;

	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 );

	NPCInit();

	SetHealth( 50 );

	BaseClass::Spawn();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	// If we're tiny and the player touched us...
	if ( pOther->IsPlayer() && GetObjectScaleLevel() == -1 )
	{
		Vector vecOffset = GetAbsOrigin();

		/*
		// FIXME: Need a new squashed model!
		CBaseEntity *pSquashedModel = CreateEntityByName( "_squashed_zombie" );
		
		Vector vecForward;
		GetVectors( &vecForward, NULL, NULL );

		UTIL_SetOrigin( pSquashedModel, vecOffset );
		
		QAngle vecAngles = GetAbsAngles();
		pSquashedModel->SetAbsAngles( vecAngles );
		DispatchSpawn( pSquashedModel );
		*/

		// Make us immaterial while we wait to vanish
		SetEffects( EF_NODRAW );
		SetSolidFlags( FSOLID_NOT_SOLID );

		// Cover the transition
		DispatchParticleEffect( "GrubBlood", GetAbsOrigin() + Vector( 0, 0, 4 ), QAngle(-90,0,0) );

		// Splat on the ground
		trace_t tr;
		UTIL_TraceLine( vecOffset + Vector( 0, 0, 8), vecOffset - Vector(0,0,64), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 )
		{
			UTIL_DecalTrace( &tr, "Antlion.Splat" );
		}

		// Splat sound
		EmitSound( "NPC_Android.Squash" );

		// FIXME: This is wrong
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::GetAttackActivity( void )
{
	// if our enemy is higher then we want to try and jump at it instead of our normal attack
	if( GetEnemy() )
	{
		if( GetEnemy()->Classify() == CLASS_BULLSEYE )
		{
			return ACT_DROID_MELEE_CHOP;
		}

		CBaseAnimating *pOtherAnim = GetEnemy()->GetBaseAnimating();
		
		if( pOtherAnim && pOtherAnim->GetObjectScaleLevel() > GetObjectScaleLevel() )
		{
			return ACT_DROID_JUMPSWIPE;
		}
		else if( GetObjectScaleLevel() == 0 && GetEnemy()->Classify() == CLASS_PLAYER )
		{
			return ACT_DROID_MELEE_CHOP;
		}
		
	}

	return ACT_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ANDROID_GET_PATH_TO_PHYSOBJ:
		{
			Vector vecGoalPos;
			Vector vecDir;

			vecDir = GetLocalOrigin() - m_hPhysicsEnt->GetLocalOrigin();
			VectorNormalize(vecDir);
			vecDir.z = 0;

			AI_NavGoal_t goal( m_hPhysicsEnt->WorldSpaceCenter() );
			goal.pTarget = m_hPhysicsEnt;
			GetNavigator()->SetGoal( goal );

			TaskComplete();
		}
		break;
	case TASK_MELEE_ATTACK1:
		{
			SetIdealActivity( (Activity)GetAttackActivity() );
			break;
		}
	case TASK_ANDROID_SWAT_ITEM:
		{
			if( m_hPhysicsEnt == NULL )
			{
				// Physics Object is gone! Probably was an explosive 
				// or something else broke it.
				TaskFail("Physics ent NULL");
			}
			else if ( DistToPhysicsEnt() > ANDROID_PHYSOBJ_SWATDIST )
			{
				// Physics ent is no longer in range! Probably another android swatted it or it moved
				// for some other reason.
				TaskFail( "Physics swat item has moved" );
			}
			else
			{
				SetIdealActivity( (Activity)GetSwatActivity() );
			}
			break;
		}
		break;
	case TASK_ANDROID_DELAY_SWAT:
		m_flNextSwat = gpGlobals->curtime + pTask->flTaskData;
		TaskComplete();
		break;

	case TASK_ANDROID_WAIT_POST_MELEE:
		{
			// Don't wait when attacking the player
			if ( GetEnemy() && GetEnemy()->IsPlayer() )
			{
				TaskComplete();
				return;
			}

			// Wait a single think
			SetWait( 0.1 );
		}
		break;
		
	default:
		{
			BaseClass::StartTask( pTask );
			break;
		}
	}
//
//	BaseClass::StartTask( pTask );
}

//---------------------------------------------------------
//---------------------------------------------------------

void CNPC_Android::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ANDROID_SWAT_ITEM:
		if( IsActivityFinished() )
		{
			TaskComplete();
		}
		break;

	case TASK_ANDROID_WAIT_POST_MELEE:
		{
			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
		}
		break;
	default:
		BaseClass::RunTask( pTask );
		break;

	}
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Android::GetSwatActivity( void )
{
	// Hafta figure out whether to swat with left or right arm.
	// Also hafta figure out whether to swat high or low. (later)
	float		flDot;
	Vector		vecRight, vecDirToObj;

	AngleVectors( GetLocalAngles(), NULL, &vecRight, NULL );
	
	vecDirToObj = m_hPhysicsEnt->GetLocalOrigin() - GetLocalOrigin();
	VectorNormalize(vecDirToObj);

	// compare in 2D.
	vecRight.z = 0.0;
	vecDirToObj.z = 0.0;

	flDot = DotProduct( vecRight, vecDirToObj );

	Vector vecMyCenter;
	Vector vecObjCenter;

	vecMyCenter = WorldSpaceCenter();
	vecObjCenter = m_hPhysicsEnt->WorldSpaceCenter();
	float flZDiff;

	flZDiff = vecMyCenter.z - vecObjCenter.z;

	if( flDot >= 0 )
	{
		// Right
		if( flZDiff < 0 )
		{
			return ACT_DROID_SWATRIGHTMID;
		}

		return ACT_DROID_SWATRIGHTLOW;
	}
	else
	{
		// Left
		if( flZDiff < 0 )
		{
			return ACT_DROID_SWATLEFTMID;
		}

		return ACT_DROID_SWATLEFTLOW;
	}
}

//---------------------------------------------------------
// The closest physics object is chosen that is:
// <= MaxMass in Mass
// Between the zombie and the enemy
// not too far from a direct line to the enemy.
//---------------------------------------------------------
bool CNPC_Android::FindNearestPhysicsObject( int iMaxMass )
{
	CBaseEntity		*pList[ ANDROID_PHYSICS_SEARCH_DEPTH ];
	CBaseEntity		*pNearest = NULL;
	float			flDist;
	IPhysicsObject	*pPhysObj;
	int				i;
	Vector			vecDirToEnemy;
	Vector			vecDirToObject;

	if ( !CanSwatPhysicsObjects() || !GetEnemy() )
	{
		// Can't swat, or no enemy, so no swat.
		m_hPhysicsEnt = NULL;
		return false;
	}

	vecDirToEnemy = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	float dist = VectorNormalize(vecDirToEnemy);
	vecDirToEnemy.z = 0;

	if( dist > ANDROID_PLAYER_MAX_SWAT_DIST )
	{
		// Player is too far away. Don't bother 
		// trying to swat anything at them until
		// they are closer.
		return false;
	}

	float flNearestDist = min( dist, ANDROID_FARTHEST_PHYSICS_OBJECT * 0.5 );
	Vector vecDelta( flNearestDist, flNearestDist, GetHullHeight() * 2.0 );

	class CAndroidSwatEntitiesEnum : public CFlaggedEntitiesEnum
	{
	public:
		CAndroidSwatEntitiesEnum( CBaseEntity **pList, int listMax, int iMaxMass )
		 :	CFlaggedEntitiesEnum( pList, listMax, 0 ),
			m_iMaxMass( iMaxMass )
		{
		}

		virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
		{
			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
			if ( pEntity && 
				 pEntity->VPhysicsGetObject() && 
				 pEntity->VPhysicsGetObject()->GetMass() <= m_iMaxMass && 
				 pEntity->VPhysicsGetObject()->IsAsleep() && 
				 pEntity->VPhysicsGetObject()->IsMoveable() )
			{
				return CFlaggedEntitiesEnum::EnumElement( pHandleEntity );
			}
			return ITERATION_CONTINUE;
		}

		int m_iMaxMass;
	};

	CAndroidSwatEntitiesEnum swatEnum( pList, ANDROID_PHYSICS_SEARCH_DEPTH, iMaxMass );

	int count = UTIL_EntitiesInBox( GetAbsOrigin() - vecDelta, GetAbsOrigin() + vecDelta, &swatEnum );

	// magically know where they are
	Vector vecAndroidKnees;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.25f ), &vecAndroidKnees );

	for( i = 0 ; i < count ; i++ )
	{
		pPhysObj = pList[ i ]->VPhysicsGetObject();

		Assert( !( !pPhysObj || pPhysObj->GetMass() > iMaxMass || !pPhysObj->IsAsleep() ) );

		Vector center = pList[ i ]->WorldSpaceCenter();
		flDist = UTIL_DistApprox2D( GetAbsOrigin(), center );

		if( flDist >= flNearestDist )
			continue;

		// This object is closer... but is it between the player and the zombie?
		vecDirToObject = pList[ i ]->WorldSpaceCenter() - GetAbsOrigin();
		VectorNormalize(vecDirToObject);
		vecDirToObject.z = 0;

		if( DotProduct( vecDirToEnemy, vecDirToObject ) < 0.8 )
			continue;

		if( flDist >= UTIL_DistApprox2D( center, GetEnemy()->GetAbsOrigin() ) )
			continue;

		// don't swat things where the highest point is under my knees
		// NOTE: This is a rough test; a more exact test is going to occur below
		if ( (center.z + pList[i]->BoundingRadius()) < vecAndroidKnees.z )
			continue;

		// don't swat things that are over my head.
		if( center.z > EyePosition().z )
			continue;

		vcollide_t *pCollide = modelinfo->GetVCollide( pList[i]->GetModelIndex() );
		if ( pCollide == NULL )
			continue;

		Vector objMins, objMaxs;
		physcollision->CollideGetAABB( &objMins, &objMaxs, pCollide->solids[0], pList[i]->GetAbsOrigin(), pList[i]->GetAbsAngles() );

		if ( objMaxs.z < vecAndroidKnees.z )
			continue;

		if ( !FVisible( pList[i] ) )
			continue;

		if ( hl2_episodic.GetBool() )
		{
			// Skip things that the enemy can't see. Do we want this as a general thing? 
			// The case for this feature is that zombies who are pursuing the player will
			// stop along the way to swat objects at the player who is around the corner or 
			// otherwise not in a place that the object has a hope of hitting. This diversion
			// makes the zombies very late (in a random fashion) getting where they are going. (sjb 1/2/06)
			if( !GetEnemy()->FVisible( pList[i] ) )
				continue;
		}

		// Make this the last check, since it makes a string.
		// Don't swat server ragdolls!
		if ( FClassnameIs( pList[ i ], "physics_prop_ragdoll" ) )
			continue;
			
		if ( FClassnameIs( pList[ i ], "prop_ragdoll" ) )
			continue;

		// The object must also be closer to the zombie than it is to the enemy
		pNearest = pList[ i ];
		flNearestDist = flDist;
	}

	m_hPhysicsEnt = pNearest;

	if( m_hPhysicsEnt == NULL )
	{
		return false;
	}
	else
	{
		return true;
	}
}

//---------------------------------------------------------
// Provides a standard way for the zombie to get the 
// distance to a physics ent. Since the code to find physics 
// objects uses a fast dis approx, we have to use that here
// as well.
//---------------------------------------------------------
float CNPC_Android::DistToPhysicsEnt( void )
{
	//return ( GetLocalOrigin() - m_hPhysicsEnt->GetLocalOrigin() ).Length();
	if ( m_hPhysicsEnt != NULL )
		return UTIL_DistApprox2D( GetAbsOrigin(), m_hPhysicsEnt->WorldSpaceCenter() );
	return ANDROID_PHYSOBJ_SWATDIST + 1;
}

#define ANDROID_SCORCH_RATE		8
#define ANDROID_MIN_RENDERCOLOR	50

int CNPC_Android::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	if( inputInfo.GetDamageType() & DMG_BURN )
	{
		// If a zombie is on fire it only takes damage from the fire that's attached to it. (DMG_DIRECT)
		// This is to stop zombies from burning to death 10x faster when they're standing around
		// 10 fire entities.
		if( IsOnFire() && !(inputInfo.GetDamageType() & DMG_DIRECT) )
		{
			return 0;
		}
		
		Scorch( ANDROID_SCORCH_RATE, ANDROID_MIN_RENDERCOLOR );
	}

	if ( ShouldIgnite( info ) )
	{
		Ignite( 100.0f );
		m_flNextMoanTime = gpGlobals->curtime + 0.25f;
	}

	int tookDamage = BaseClass::OnTakeDamage_Alive( info );

	if( tookDamage > 0 && (info.GetDamageType() & (DMG_BURN|DMG_DIRECT)) && m_ActBusyBehavior.IsActive() ) 
	{
		//!!!HACKHACK- Stuff a light_damage condition if an actbusying zombie takes direct burn damage. This will cause an
		// ignited zombie to 'wake up' and rise out of its actbusy slump. (sjb)
		SetCondition( COND_LIGHT_DAMAGE );
	}

	// IMPORTANT: always clear the headshot flag after applying damage. No early outs!
//	m_bHeadShot = false;

	return tookDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::SelectSchedule( void )
{
	if ( HasCondition( COND_ANDROID_RISE_FROM_GROUND ) )
	{
		ClearCondition( COND_ANDROID_RISE_FROM_GROUND );
		return SCHED_ANDROID_RISE;
	}

	// If we hit the ground, then bounce!
	if ( HasCondition( COND_ANDROID_HIT_GROUND ) )
	{
		return ( m_bLongFall ) ? SCHED_ANDROID_HIT_GROUND : SCHED_ANDROID_STUMBLE;
	}

	// Get released
	if ( HasCondition( COND_ANDROID_RELEASED ) )
	{
		ClearCondition( COND_ANDROID_RELEASED );

		// Find about how high we are off the ground and use that to determine how we fall
		trace_t tr;
		const float flCheckDist = 10.0f * GetModelScale() * 12.0f;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,flCheckDist), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

		// If this is a decent fall, then play an animation
		if ( tr.fraction > 0.05f )
		{
//			SetCondition( COND_ANDROID_OFF_GROUND );
			SetIdealActivity( (Activity)ACT_GLIDE );

			// Remember if this is a small or large fall
			m_bLongFall = ( tr.fraction == 1.0f );

			return SCHED_ANDROID_FALL;
		}
	}

	if( GetIdealActivity() == ACT_GLIDE )
		return SCHED_ANDROID_FALL;

	// If we can swat physics objects, see if we can swat our obstructor
	if ( CanSwatPhysicsObjects() )
	{
		if ( m_hPhysicsEnt != NULL && 
			 m_hPhysicsEnt->VPhysicsGetObject()->GetMass() < 100 )
		{
			return SCHED_ANDROID_ATTACKITEM;
		}
	}

	if ( BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Android::BuildScheduleTestBits( void )
{
	// Ignore damage if we were recently damaged or we're attacking.
//	if ( GetActivity() == ACT_MELEE_ATTACK1 )
//	{
//		ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );
//		ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
//	}

	BaseClass::BuildScheduleTestBits();
}


//-----------------------------------------------------------------------------
// Purpose: Called when we change schedules.
//-----------------------------------------------------------------------------
void CNPC_Android::OnScheduleChange( void )
{
	//
	// If we took damage and changed schedules, ignore further damage for a few seconds.
	//
	if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ))
	{
		m_flNextFlinch = gpGlobals->curtime + ANDROID_FLINCH_DELAY;
	} 

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CNPC_Android::IRelationType( CBaseEntity *pTarget )
{
	return BaseClass::IRelationType( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: damage has been done. Should the zombie ignite?
//-----------------------------------------------------------------------------
bool CNPC_Android::ShouldIgnite( const CTakeDamageInfo &info )
{
 	if ( IsOnFire() )
	{
		// Already burning!
		return false;
	}

	if ( info.GetDamageType() & DMG_BURN )
	{
#ifdef APERTURE
		return true;
#endif // APERTURE

		//
		// If we take more than ten percent of our health in burn damage within a five
		// second interval, we should catch on fire.
		//
		m_flBurnDamage += info.GetDamage();
		m_flBurnDamageResetTime = gpGlobals->curtime + 5;

		if ( m_flBurnDamage >= m_iMaxHealth * 0.1f )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sufficient fire damage has been done. Zombie ignites!
//-----------------------------------------------------------------------------
void CNPC_Android::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );

	// Set the zombie up to burn to death in about ten seconds.
	SetHealth( min( m_iHealth, FLAME_DIRECT_DAMAGE_PER_SEC * (ANDROID_BURN_TIME + random->RandomFloat( -ANDROID_BURN_TIME_NOISE, ANDROID_BURN_TIME_NOISE)) ) );

	// FIXME: use overlays when they come online
	//AddOverlay( ACT_ZOM_WALK_ON_FIRE, false );
	if( !m_ActBusyBehavior.IsActive() )
	{
		Activity activity = GetActivity();
		Activity burningActivity = activity;

		if ( activity == ACT_WALK )
		{
			burningActivity = ACT_WALK_ON_FIRE;
		}
		else if ( activity == ACT_RUN )
		{
			burningActivity = ACT_RUN_ON_FIRE;
		}
		else if ( activity == ACT_IDLE )
		{
			burningActivity = ACT_IDLE_ON_FIRE;
		}

		if( HaveSequenceForActivity(burningActivity) )
		{
			// Make sure we have a sequence for this activity (torsos don't have any, for instance) 
			// to prevent the baseNPC & baseAnimating code from throwing red level errors.
			SetActivity( burningActivity );
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android::CopyRenderColorTo( CBaseEntity *pOther )
{
	color24 color = GetRenderColor();
	pOther->SetRenderColor( color.r, color.g, color.b, GetRenderAlpha() );
}

//-----------------------------------------------------------------------------
// Purpose: Look in front and see if the claw hit anything.
//
// Input  :	flDist				distance to trace		
//			iDamage				damage to do if attack hits
//			vecViewPunch		camera punch (if attack hits player)
//			vecVelocityPunch	velocity punch (if attack hits player)
//
// Output : The entity hit by claws. NULL if nothing.
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Android::ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch, int BloodOrigin  )
{
	//
	// Trace out a cubic section of our hull and see what we hit.
	//
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	if( GetObjectScaleLevel() == 1 )
	{
		vecMins *= 1.0f;
		vecMaxs *= 1.0f;

		vecMins.z *= 6.0f; // we need this thing to be able to hit us on the ground.
		vecMaxs.z *= 6.0f;
	}

	CBaseEntity *pHurt = NULL;
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
	{ 
		// We always hit bullseyes we're targeting
		pHurt = GetEnemy();
		CTakeDamageInfo info( this, this, vec3_origin, GetAbsOrigin(), iDamage, DMG_SLASH );
		pHurt->TakeDamage( info );
	}
	else 
	{
		// Try to hit them with a trace
		pHurt = CheckTraceHullAttack( flDist, vecMins, vecMaxs, iDamage, DMG_SLASH );
	}

	if ( !pHurt && m_hPhysicsEnt != NULL && IsCurSchedule(SCHED_ANDROID_ATTACKITEM) )
	{
		pHurt = m_hPhysicsEnt;

		Vector vForce = pHurt->WorldSpaceCenter() - WorldSpaceCenter(); 
		VectorNormalize( vForce );

		vForce *= 5 * 24;

		CTakeDamageInfo info( this, this, vForce, GetAbsOrigin(), iDamage, DMG_SLASH );
		pHurt->TakeDamage( info );

		pHurt = m_hPhysicsEnt;
	}

	if ( pHurt )
	{
		AttackHitSound();

		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer != NULL && !(pPlayer->GetFlags() & FL_GODMODE ) )
		{
			pPlayer->ViewPunch( qaViewPunch );
			
			pPlayer->VelocityPunch( vecVelocityPunch );
		}
	}
	else 
	{
		AttackMissSound();
	}

	if ( pHurt == m_hPhysicsEnt && IsCurSchedule(SCHED_ANDROID_ATTACKITEM) )
	{
		m_hPhysicsEnt = NULL;
		m_flNextSwat = gpGlobals->curtime + random->RandomFloat( 2, 4 );
	}

	return pHurt;
}

//-----------------------------------------------------------------------------
// Purpose: The zombie is frustrated and pounding walls/doors. Make an appropriate noise
// Input  : 
//-----------------------------------------------------------------------------
void CNPC_Android::PoundSound()
{
	trace_t		tr;
	Vector		forward;

	GetVectors( &forward, NULL, NULL );

	AI_TraceLine( EyePosition(), EyePosition() + forward * 128, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if( tr.fraction == 1.0 )
	{
		// Didn't hit anything!
		return;
	}

	if( tr.fraction < 1.0 && tr.m_pEnt )
	{
		const surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if( psurf )
		{
			EmitSound( physprops->GetString(psurf->sounds.impactHard) );
			return;
		}
	}

	// Otherwise fall through to the default sound.
	CPASAttenuationFilter filter( this,"NPC_BaseZombie.PoundDoor" );
	EmitSound( filter, entindex(),"NPC_BaseZombie.PoundDoor" );
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CNPC_Android::FootstepSound( bool fRightFoot )
{
	if( fRightFoot )
	{
		EmitSound(  "Zombie.FootstepRight" );
	}
	else
	{
		EmitSound( "Zombie.FootstepLeft" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Android::ShouldPlayIdleSound( void )
{
	return ( m_flNextMoanTime < gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::IdleSound( void )
{
	EmitSound( "NPC_Android.Pain" );
	m_flNextMoanTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 1.5f );
//	EmitSound( "NPC_Android.Servo" );
//	m_flNextServoTime = gpGlobals->curtime + random->RandomFloat( 0.8f, 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::MoanSound( void )
{
	EmitSound( "NPC_Android.Pain" );
	m_flNextMoanTime = gpGlobals->curtime + random->RandomFloat( 0.7f, 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_Android::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_Android::AttackMissSound( void )
{
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::AttackSound( void )
{
//	EmitSound( "Zombie.Attack" );
//	EmitSound( "NPC_Android.Windup" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::MoveStartSound( void )
{
	EmitSound( "NPC_Android.Servo" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_NPC_ATTACK_BROADCAST )
	{
		if( GetEnemy() && GetEnemy()->IsNPC() )
		{
			if( HasCondition(COND_CAN_MELEE_ATTACK1) )
			{
				// This animation is sometimes played by code that doesn't intend to attack the enemy
				// (For instance, code that makes a zombie take a frustrated swipe at an obstacle). 
				// Try not to trigger a reaction from our enemy unless we're really attacking. 
//				GetEnemy()->MyNPCPointer()->DispatchInteraction( g_interactionZombieMeleeWarning, NULL, this );
			}
		}
		return;
	}

	if ( pEvent->event == AE_ANDROID_POUND )
	{
		PoundSound();
		return;
	}

	if ( pEvent->event == AE_ANDROID_ALERTSOUND )
	{
		AlertSound();
		return;
	}
	
	if ( pEvent->event == AE_ANDROID_MOVESTART_LEFT ||
		 pEvent->event == AE_ANDROID_MOVESTART_RIGHT )
	{
		MoveStartSound();
		return;
	}

	if ( pEvent->event == AE_ANDROID_STEP_LEFT )
	{
		MakeAIFootstepSound( 180.0f );
		FootstepSound( false );
		
		if ( GetObjectScaleLevel() == 1 )
		{
			HeavyFootstep( false );
		}

		return;
	}
	
	if ( pEvent->event == AE_ANDROID_STEP_RIGHT )
	{
		MakeAIFootstepSound( 180.0f );
		FootstepSound( true );

		if ( GetObjectScaleLevel() == 1 )
		{
			HeavyFootstep( true );
		}

		return;
	}

	if ( pEvent->event == AE_ANDROID_GET_UP )
	{
		MakeAIFootstepSound( 180.0f, 3.0f );
		if( !IsOnFire() )
		{
			// If you let this code run while a zombie is burning, it will stop wailing. 
//			m_flNextMoanSound = gpGlobals->curtime;
//			MoanSound( envDefaultZombieMoanVolumeFast, ARRAYSIZE( envDefaultZombieMoanVolumeFast ) );
		}
		return;
	}

//	if ( pEvent->event == AE_ANDROID_SCUFF_LEFT )
//	{
//		MakeAIFootstepSound( 180.0f );
////		FootscuffSound( false );
//		return;
//	}
//
//	if ( pEvent->event == AE_ANDROID_SCUFF_RIGHT )
//	{
//		MakeAIFootstepSound( 180.0f );
////		FootscuffSound( true );
//		return;
//	}

	if ( pEvent->event == AE_ANDROID_WIFF )
	{
		AttackMissSound();
		return;
	}

	// all swat animations are handled as a single case.
	if ( pEvent->event == AE_ANDROID_STARTSWAT )
	{
		MakeAIFootstepSound( 180.0f );
		AttackSound();
		return;
	}

	if ( pEvent->event == AE_ANDROID_ATTACK_SCREAM )
	{
//		AttackSound();
		return;
	}

	if ( pEvent->event == AE_ANDROID_SWATITEM )
	{
		CBaseEntity *pEnemy = GetEnemy();
		if ( pEnemy )
		{
			Vector v;
			CBaseEntity *pPhysicsEntity = m_hPhysicsEnt;
			if( !pPhysicsEntity )
			{
				DevMsg( "**Zombie: Missing my physics ent!!" );
				return;
			}
			
			IPhysicsObject *pPhysObj = pPhysicsEntity->VPhysicsGetObject();

			if( !pPhysObj )
			{
				DevMsg( "**Zombie: No Physics Object for physics Ent!" );
				return;
			}

			EmitSound( "NPC_BaseZombie.Swat" );
			PhysicsImpactSound( pEnemy, pPhysObj, CHAN_BODY, pPhysObj->GetMaterialIndex(), physprops->GetSurfaceIndex("flesh"), 0.5, 800 );

			Vector physicsCenter = pPhysicsEntity->WorldSpaceCenter();
			v = pEnemy->WorldSpaceCenter() - physicsCenter;
			VectorNormalize(v);

			// Send the object at 800 in/sec toward the enemy.  Add 200 in/sec up velocity to keep it
			// in the air for a second or so.
			v = v * 800;
			v.z += 200;

			// add some spin so the object doesn't appear to just fly in a straight line
			// Also this spin will move the object slightly as it will press on whatever the object
			// is resting on.
			AngularImpulse angVelocity( random->RandomFloat(-180, 180), 20, random->RandomFloat(-360, 360) );

			pPhysObj->AddVelocity( &v, &angVelocity );

			// If we don't put the object scan time well into the future, the zombie
			// will re-select the object he just hit as it is flying away from him.
			// It will likely always be the nearest object because the zombie moved
			// close enough to it to hit it.
			m_hPhysicsEnt = NULL;

			m_flNextSwatScan = gpGlobals->curtime + ANDROID_SWAT_DELAY;

			return;
		}
	}

	if( GetEnemy() && GetEnemy()->m_takedamage == DAMAGE_NO )
		return;
	
	float damage = GetObjectScaleLevel() == 1 ? android_dmg_big.GetFloat() : android_dmg_normal.GetFloat();
	float scalar = GetModelScale();

	if ( pEvent->event == AE_ANDROID_ATTACK_RIGHT )
	{
		Vector right, forward;
		AngleVectors( GetLocalAngles(), &forward, &right, NULL );
		
		right = right * 100;
		forward = forward * 200;

		ClawAttack( GetClawAttackRange(), damage, QAngle( -15, -20, -10 ), scalar * (right + forward), ANDROID_BLOOD_RIGHT_HAND );
		return;
	}

	if ( pEvent->event == AE_ANDROID_ATTACK_LEFT )
	{
		Vector right, forward;
		AngleVectors( GetLocalAngles(), &forward, &right, NULL );

		right = right * -100;
		forward = forward * 200;

		ClawAttack( GetClawAttackRange(), damage, QAngle( -15, 20, -10 ), scalar * (right + forward), ANDROID_BLOOD_LEFT_HAND );
		return;
	}

	if ( pEvent->event == AE_ANDROID_ATTACK_BOTH )
	{
		Vector right, forward;
		int rightPunch = random->RandomInt(-10,10);

		QAngle qaPunch( 45, rightPunch, random->RandomInt(-5,5) );

		AngleVectors( GetLocalAngles(), &forward, &right, NULL );
		forward = forward * 200;
		right = right * 15 * rightPunch;

		ClawAttack( GetClawAttackRange(), damage, qaPunch, scalar * (right + forward), ANDROID_BLOOD_BOTH_HANDS );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Display heavy foot step effects for huge zombie!
//-----------------------------------------------------------------------------
void CNPC_Android::HeavyFootstep( bool isRightFoot )
{
	Vector vecFootPos;
	QAngle vecFootAngles;
	GetAttachment( ((isRightFoot) ? "foot_right" : "foot_left" ), vecFootPos, vecFootAngles );
	DispatchParticleEffect( "zombie_stomp_heavy", vecFootPos, vec3_angle );

	// Shake nearby players
	UTIL_ScreenShake( GetAbsOrigin(), 64.0f, 1.0f, 1.0f, (60.0f*12.0f), SHAKE_START );

	CPASAttenuationFilter filter( this, "NPC_Strider.Footstep" );
	EmitSound( filter, 0, "NPC_Strider.Footstep", &vecFootPos, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Android::GetScaleRangeScalar( void )
{
	float scalar = 1.0f;

	// Normaly I'd just use the model scale, but that's making all the ranges a little too extreme.
	// These are just custom values to try and get this working well. 
	if( GetObjectScaleLevel() == 1 )
		scalar = 2.75f;
	else if( GetObjectScaleLevel() == -1 )
		scalar = 0.8f;

	return scalar;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Android::MeleeAttack1Conditions( float flDot, float flDist )
{
	float range = GetClawAttackRange() * GetScaleRangeScalar();

	if (flDist > range )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	// Build a cube-shaped hull, the same hull that ClawAttack() is going to use.
	Vector vecMins = GetHullMins();
	Vector vecMaxs = GetHullMaxs();
	vecMins.z = vecMins.x;
	vecMaxs.z = vecMaxs.x;

	if( GetObjectScaleLevel() == 1 )
	{
		vecMins *= 1.0f;
		vecMaxs *= 1.0f;

		vecMins.z *= 6.0f; // we need this thing to be able to hit us on the ground.
		vecMaxs.z *= 6.0f;
	}
	else if( GetObjectScaleLevel() == -1 )
	{
		vecMins *= GetModelScale();
		vecMaxs *= GetModelScale();
	}

	Vector forward;
	GetVectors( &forward, NULL, NULL );

	trace_t	tr;
	CTraceFilterNav traceFilter( this, false, this, COLLISION_GROUP_NONE );
	AI_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + forward * GetClawAttackRange(), vecMins, vecMaxs, GetAITraceMask(), &traceFilter, &tr );

	if( tr.fraction == 1.0 || !tr.m_pEnt )
	{
		// If our trace was unobstructed but we were shooting 
		if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
			return COND_CAN_MELEE_ATTACK1;

		// This attack would miss completely. Trick the zombie into moving around some more.
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( tr.m_pEnt == GetEnemy() || 
		tr.m_pEnt->IsNPC() || 
		( tr.m_pEnt->m_takedamage == DAMAGE_YES && (dynamic_cast<CBreakableProp*>(tr.m_pEnt) ) ) )
	{
		// -Let the zombie swipe at his enemy if he's going to hit them.
		// -Also let him swipe at NPC's that happen to be between the zombie and the enemy. 
		//  This makes mobs of zombies seem more rowdy since it doesn't leave guys in the back row standing around.
		// -Also let him swipe at things that takedamage, under the assumptions that they can be broken.
		return COND_CAN_MELEE_ATTACK1;
	}

	Vector vecTrace = tr.endpos - tr.startpos;
	float lenTraceSq = vecTrace.Length2DSqr();

	if( tr.m_pEnt->IsBSPModel() )
	{
		// The trace hit something solid, but it's not the enemy. If this item is closer to the zombie than
		// the enemy is, treat this as an obstruction.
		Vector vecToEnemy = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();

		if( lenTraceSq < vecToEnemy.Length2DSqr() )
		{
			return COND_ANDROID_LOCAL_MELEE_OBSTRUCTION;
		}
	}

	if ( !tr.m_pEnt->IsWorld() && GetEnemy() && GetEnemy()->GetGroundEntity() == tr.m_pEnt )
	{
		//Try to swat whatever the player is standing on instead of acting like a dill.
		return COND_CAN_MELEE_ATTACK1;
	}

	// Bullseyes are given some grace on if they can be hit
	if ( GetEnemy() && GetEnemy()->Classify() == CLASS_BULLSEYE )
		return COND_CAN_MELEE_ATTACK1;

	// Move around some more
	return COND_TOO_FAR_TO_ATTACK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::OnCaptured( void )
{
	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::OnReleased( void )
{
	GetNavigator()->ClearGoal();
	ClearSchedule( "Released from camera" );

	SetCondition( COND_ANDROID_RELEASED );

	SetSchedule( GetNewSchedule() );

	BaseClass::OnReleased();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CNPC_Android::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_CHASE_ENEMY:
		if ( HasCondition( COND_ANDROID_LOCAL_MELEE_OBSTRUCTION ) && !HasCondition(COND_TASK_FAILED) && IsCurSchedule( SCHED_CHASE_ENEMY, false ) )
		{
			return SCHED_COMBAT_PATROL;
		}
		return SCHED_ANDROID_CHASE_ENEMY;
		break;

	case SCHED_ANDROID_ATTACKITEM:
	case SCHED_ANDROID_SWATITEM:
		// If the object is far away, move and swat it. If it's close, just swat it.
		if( DistToPhysicsEnt() > ANDROID_PHYSOBJ_SWATDIST )
		{
			return SCHED_ANDROID_MOVE_SWATITEM;
		}
		else
		{
			return SCHED_ANDROID_SWATITEM;
		}
		break;

	case SCHED_STANDOFF:
		return SCHED_ANDROID_WANDER_STANDOFF;

	case SCHED_MELEE_ATTACK1:
		return SCHED_ANDROID_MELEE_ATTACK1;
	}
	
	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::GatherConditions( void )
{
	ClearCondition( COND_ANDROID_LOCAL_MELEE_OBSTRUCTION );

	if ( IsCurSchedule( SCHED_ANDROID_FALL ) ||
		 IsCurSchedule( SCHED_FALL_TO_GROUND ) )
	{
		// there needs to be a better way to determine this.

		trace_t tr;
		const float flCheckDist = 10.0f * GetModelScale() * 12.0f;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,flCheckDist), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

		// If this is a decent fall, then play an animation
		if ( tr.fraction <= 0.1f )
		{
			SetCondition( COND_ANDROID_HIT_GROUND );
		}
	}

	if( m_NPCState == NPC_STATE_COMBAT )
	{
		// This check for !m_pPhysicsEnt prevents a crashing bug, but also
		// eliminates the zombie picking a better physics object if one happens to fall
		// between him and the object he's heading for already. 
		if( gpGlobals->curtime >= m_flNextSwatScan && (m_hPhysicsEnt == NULL) )
		{
			FindNearestPhysicsObject( ANDROID_MAX_PHYSOBJ_MASS * GetModelScale() );
			m_flNextSwatScan = gpGlobals->curtime + 2.0;
		}
	}


	//bool bOffGround = ( ( GetFlags() & (FL_ONGROUND|FL_FLY) ) != 0 );
	//if ( bOffGround == false )
	////if ( GetGroundEntity() == NULL )
	//{
	//	// Mark ourselves as being off the ground
	//	SetCondition( COND_ANDROID_OFF_GROUND );
	//}
	//else
	//{
	//	// If we were off the ground on the last frame, we were in the air!
	//	if ( HasCondition( COND_ANDROID_OFF_GROUND ) )
	//	{
	//		SetCondition( COND_ANDROID_HIT_GROUND );
	//	}

	//	ClearCondition( COND_ANDROID_OFF_GROUND );
	//}

	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android::InputRiseFromGround( inputdata_t &inputdata )
{
	SetCondition( COND_ANDROID_RISE_FROM_GROUND );
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_mannequin, CNPC_Android )

DECLARE_TASK( TASK_ANDROID_SWAT_ITEM )
DECLARE_TASK( TASK_ANDROID_DELAY_SWAT )
DECLARE_TASK( TASK_ANDROID_GET_PATH_TO_PHYSOBJ )
DECLARE_TASK( TASK_ANDROID_WAIT_POST_MELEE )


DECLARE_CONDITION( COND_ANDROID_RELEASED )
DECLARE_CONDITION( COND_ANDROID_OFF_GROUND )
DECLARE_CONDITION( COND_ANDROID_HIT_GROUND )
DECLARE_CONDITION( COND_ANDROID_RISE_FROM_GROUND )
DECLARE_CONDITION( COND_ANDROID_CAN_SWAT_ATTACK )
DECLARE_CONDITION( COND_ANDROID_LOCAL_MELEE_OBSTRUCTION )

DECLARE_ANIMEVENT( AE_ANDROID_ATTACK_RIGHT )
DECLARE_ANIMEVENT( AE_ANDROID_ATTACK_LEFT )
DECLARE_ANIMEVENT( AE_ANDROID_ATTACK_BOTH )
DECLARE_ANIMEVENT( AE_ANDROID_SWATITEM )
DECLARE_ANIMEVENT( AE_ANDROID_STARTSWAT )
DECLARE_ANIMEVENT( AE_ANDROID_WIFF )
DECLARE_ANIMEVENT( AE_ANDROID_STEP_LEFT )
DECLARE_ANIMEVENT( AE_ANDROID_STEP_RIGHT )
DECLARE_ANIMEVENT( AE_ANDROID_MOVESTART_LEFT )
DECLARE_ANIMEVENT( AE_ANDROID_MOVESTART_RIGHT )
DECLARE_ANIMEVENT( AE_ANDROID_SCUFF_LEFT )
DECLARE_ANIMEVENT( AE_ANDROID_SCUFF_RIGHT )
DECLARE_ANIMEVENT( AE_ANDROID_ATTACK_SCREAM )
DECLARE_ANIMEVENT( AE_ANDROID_GET_UP )
DECLARE_ANIMEVENT( AE_ANDROID_POUND )
DECLARE_ANIMEVENT( AE_ANDROID_ALERTSOUND )

DECLARE_ACTIVITY( ACT_DROID_MELEE_CHOP )
DECLARE_ACTIVITY( ACT_DROID_JUMPSWIPE )
DECLARE_ACTIVITY( ACT_DROID_SWATRIGHTMID )
DECLARE_ACTIVITY( ACT_DROID_SWATRIGHTLOW )
DECLARE_ACTIVITY( ACT_DROID_SWATLEFTMID )
DECLARE_ACTIVITY( ACT_DROID_SWATLEFTLOW )

DECLARE_ACTIVITY( ACT_ANDROID_RISE_FROM_GROUND );

	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_MOVE_SWATITEM,

		"	Tasks"
		"		TASK_ANDROID_DELAY_SWAT			3"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_ANDROID_GET_PATH_TO_PHYSOBJ	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_ANDROID_SWAT_ITEM			0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	// SwatItem
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_SWATITEM,

		"	Tasks"
		"		TASK_ANDROID_DELAY_SWAT			3"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_FACE_ENEMY					0"
		"		TASK_ANDROID_SWAT_ITEM			0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_ATTACKITEM,

		"	Tasks"
		"		TASK_FACE_ENEMY					0"
		"		TASK_MELEE_ATTACK1				0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	// ChaseEnemy
	//=========================================================

	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_CHASE_ENEMY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_ANDROID_CAN_SWAT_ATTACK"
		"		COND_ANDROID_OFF_GROUND"

	)

	//=========================================================
	// Wander around for a while so we don't look stupid. 
	// this is done if we ever lose track of our enemy.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_WANDER_MEDIUM,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						480384" // 4 feet to 32 feet
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT_PVS					0" // if the player left my PVS, just wait.
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_ANDROID_WANDER_MEDIUM" // keep doing it
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ANDROID_OFF_GROUND"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_WANDER_STANDOFF,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						480384" // 4 feet to 32 feet
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT_PVS					0" // if the player left my PVS, just wait.
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	// If you fail to wander, wait just a bit and try again.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_WANDER_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_WAIT				1"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ANDROID_WANDER_MEDIUM"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	// Like the base class, only don't stop in the middle of 
	// swinging if the enemy is killed, hides, or new enemy.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ANDROID_POST_MELEE_WAIT"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ANDROID_OFF_GROUND"
	)

	//=========================================================
	// Make the zombie wait a frame after a melee attack, to
	// allow itself & it's enemy to test for dynamic scripted sequences.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_POST_MELEE_WAIT,

		"	Tasks"
		"		TASK_ANDROID_WAIT_POST_MELEE		0"
	)

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_MOVE_TO_AMBUSH,

		"	Tasks"
		"		TASK_WAIT						0.0" // don't react as soon as you see the player.
		"		TASK_FIND_COVER_FROM_ENEMY		0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_TURN_LEFT					180"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_ANDROID_WAIT_AMBUSH"
		"	"
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_NEW_ENEMY"
		"		COND_ANDROID_OFF_GROUND"
		"		COND_ANDROID_HIT_GROUND"
	)


	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_WAIT_AMBUSH,

		"	Tasks"
		"		TASK_WAIT_FACE_ENEMY	99999"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_ANDROID_OFF_GROUND"
	)

 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_ANDROID_RISE,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_ANDROID_RISE_FROM_GROUND"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ANDROID_WANDER_MEDIUM"
		""
		"	Interrupts"
		"		COND_ANDROID_OFF_GROUND"
		"		COND_ANDROID_HIT_GROUND"
 )

//=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_ANDROID_FALL,

	 "	Tasks"
	 "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_GLIDE"
	 "		TASK_WAIT_INDEFINITE			0"
	 ""
	 "	Interrupts"
	 "		COND_ANDROID_HIT_GROUND"
 )

 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_ANDROID_HIT_GROUND,

	 "	Tasks"
	 "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_LAND"
	 "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_STAND"
	 ""
	 "	Interrupts"
 	 "		COND_ANDROID_OFF_GROUND"
)


 //=========================================================
 DEFINE_SCHEDULE
 (
	 SCHED_ANDROID_STUMBLE,

	 "	Tasks"
	 "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_SIGNAL1"
	 ""
	 "	Interrupts"
	 "		COND_ANDROID_OFF_GROUND"
 )

 AI_END_CUSTOM_NPC()
