//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Core of the GlaDOS computer.
//
//=====================================================================================//

#include "cbase.h"
#include "baseentity.h"
#include "portal_player.h"
#include "te_effect_dispatch.h"	// sprite effect
#include "props.h"				// cphysicsprop base class
#include "saverestore_utlvector.h"
#include "fmtstr.h"

#define GLADOS_CORE_MODEL_NAME "models/props_bts/glados_ball_reference.mdl" 

static const char *s_pAnimateThinkContext = "Animate";

#define DEFAULT_LOOK_ANINAME			"look_01"
#define CURIOUS_LOOK_ANINAME			"look_02"
#define AGGRESSIVE_LOOK_ANINAME			"look_03"
#define CRAZY_LOOK_ANINAME				"look_04"
#define RICHARD_LOOK_ANINAME			"look_02"
#define AQUARIUM_LOOK_ANINAME			"look_02"

#define RICHARD_VOICE_NAME				"sphere01"
#define AQUARIUM_VOICE_NAME				"sphere02"

#define DEFAULT_SKIN					0
#define CURIOUS_SKIN					1
#define AGGRESSIVE_SKIN					2
#define	CRAZY_SKIN						3
#define	RICHARD_SKIN					1
#define	AQUARIUM_SKIN					2

enum SphereEvent_e
{
	SPHERE_EVENT_INFO_TARGET,
	SPHERE_EVENT_WAITING,
	SPHERE_EVENT_FIRST_PICKUP,
	SPHERE_EVENT_PICKUP,
	SPHERE_EVENT_DROPPED,
	SPHERE_EVENT_THROWN,
	SPHERE_EVENT_HURT,
	SPHERE_EVENT_HURT_REPEATEDLY,
	SPHERE_EVENT_PORTALED,
	SPHERE_EVENT_MOVEMENT_FORWARD,
	SPHERE_EVENT_MOVEMENT_BACKWARD,
	SPHERE_EVENT_MOVEMENT_STOPPED,
	SPHERE_EVENT_MOVEMENT_TURN_LEFT,
	SPHERE_EVENT_MOVEMENT_TURN_RIGHT,
	SPHERE_EVENT_MOVEMENT_TURN_UP,
	SPHERE_EVENT_MOVEMENT_TURN_DOWN,
	SPHERE_EVENT_MOVEMENT_SHAKING,
	SPHERE_EVENT_VELOCITY_UP_LOW,
	SPHERE_EVENT_VELOCITY_UP_HIGH,
	SPHERE_EVENT_VELOCITY_DOWN_HIGH,
	SPHERE_EVENT_VELOCITY_LATERAL_LOW,
	SPHERE_EVENT_VELOCITY_LATERAL_HIGH,
	SPHERE_EVENT_MOVEMENT_LAND_HARD,
	SPHERE_EVENT_MOVEMENT_DUCK,
	SPHERE_EVENT_SLOWTIME,
	SPHERE_EVENT_ABANDONED,
	SPHERE_EVENT_IDLE,

	SPHERE_EVENT_TOTAL,
};

const char *g_LineNames[ SPHERE_EVENT_TOTAL ] = 
{
	"InfoTarget",
	"ABANDONED",
	"PICKUPFIRST",
	"PICKUP",
	"DROP",
	"DROP",
	"PAIN",
	"BASHING",
	"SURPRISE",
	"FORWARD",
	"BACKINGUP",
	"Stopped",
	"LEFT",
	"RIGHT",
	"UP",
	"DOWN",
	"Shaking",
	"JUMPING",
	"LAUGH",
	"WHOOP",
	"LAUGH",
	"WHOOP",
	"LandHard",
	"Ducked",
	"UHOH",
	"ABANDONED",
	"IDLE",
};

class CInfoTargetPersonalitySphere : public CPointEntity
{
	DECLARE_CLASS( CInfoTargetPersonalitySphere, CPointEntity );
public:
	float GetTargetRadius( void ) { return m_flRadius; }
	string_t GetSphereLine( void ) { return m_strSphereLine; }

	DECLARE_DATADESC();	
private:

	string_t		m_strSphereLine;
	float			m_flRadius;
};


LINK_ENTITY_TO_CLASS( info_target_personality_sphere, CInfoTargetPersonalitySphere );
BEGIN_DATADESC( CInfoTargetPersonalitySphere )

	DEFINE_KEYFIELD( m_strSphereLine,		FIELD_STRING,	"sphereLine" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),

	END_DATADESC()

class CInfoTargetEntityEnumerator: public IEntityEnumerator
{
public:
	CInfoTargetEntityEnumerator( const Ray_t& ray ):
	m_ray( ray ),
	m_flLargestDot( -1.0f ),
	m_pBestEnt( NULL )
	{
		m_flClosestDist = ray.m_Delta.Length() + 1.0f;
	}

	// This gets called for each entity in a box traced along the crosshair, and 
	// tries to guess which one the player wants to look at.
	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

		if ( FClassnameIs( pEnt, "info_target_personality_sphere" ) )
		{
			{
				// Highlight as potential candidate
				NDebugOverlay::Sphere( pEnt->GetAbsOrigin(), vec3_angle, ( ( CInfoTargetPersonalitySphere * ) pEnt )->GetTargetRadius(), 255, 0, 0, 0, true, 0.05f );
			}
			
			// setup for look direction culling
			Vector vDirToEnt = pEnt->GetAbsOrigin() - m_ray.m_Start;
			float flDist = VectorNormalize( vDirToEnt );

			Vector vRayDir = m_ray.m_Delta;
			VectorNormalize( vRayDir );
			float flDot = vDirToEnt.Dot( vRayDir );

			float flToleranceAngle = atan2( ( ( CInfoTargetPersonalitySphere * ) pEnt )->GetTargetRadius(), flDist ); // angle of the max deflection
			float flTargetAngle = acos( flDot );

			// This means both are out of the crosshair, 
			// choose this candidate if it's closer to the center of the screen
			if ( flDot > m_flLargestDot && flToleranceAngle > flTargetAngle )
			{
				// This is a better choice, keep it
				m_flClosestDist = flDist;
				m_flLargestDot = flDot;
				m_pBestEnt = pEnt;
				return true;
			}
		}

		return true;
	}

	inline CBaseEntity* GetBestInfoTargetEntity( void ) { return m_pBestEnt; }

private:
	CBaseEntity*	m_pBestEnt;		// Best capture candidate found 
	const Ray_t&	m_ray;			// Copy of the ray cast for the enumerator, used for tighter checks after finding candidates
	float			m_flClosestDist;	// The distance from the point hit on pBestEnt to the ray's origin
	float			m_flLargestDot;		// keep the one closest to the center of the crosshair
};


class CPropPersonalitySphere : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropPersonalitySphere, CPhysicsProp );
	DECLARE_DATADESC();

	CPropPersonalitySphere();
	~CPropPersonalitySphere();

	typedef enum 
	{
		CORETYPE_CURIOUS,
		CORETYPE_AGGRESSIVE,
		CORETYPE_CRAZY,
		CORETYPE_RICHARD,
		CORETYPE_AQUARIUM,
		CORETYPE_NONE,
		CORETYPE_TOTAL,

	} CORETYPE;

	virtual void Spawn( void );
	virtual void Precache( void );
	void RegisterSoundEvent( SphereEvent_e eventName );

	virtual QAngle	PreferredCarryAngles( void ) { return QAngle( 180, -90, 180 ); }
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }

	void	StartTalking ( float flDelay );

	void	CheckForInfoTargets( void );
	void	UpdatePositions( void );

	void	TalkingThink ( void );
	void	AnimateThink ( void );

	void	SetupVOList ( void );

	void	TrySpeakLine( int line, int priority );
	
	void	OnPhysGunPickup( CBasePlayer* pPhysGunUser, PhysGunPickup_t reason );
	void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );

	void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

	void	NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	int ObjectCaps() 
	{ 
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

private:
//	int m_iTotalLines;
	bool m_bHasEvent[SPHERE_EVENT_TOTAL];
	int m_iEyeballAttachment;
	float m_flBetweenVOPadding;		// Spacing (in seconds) between VOs
	bool m_bFirstPickup;
	bool m_bHeld;
	float m_flLineCompletionTime;
	float m_flInfoTargetTime;		// we can't play the same info target line until this time has passed.
	float m_flIdleWaitTime;

	float m_flLastHeldTime;

	float m_flAbuseLevel;			// have we been abusing this poor sphere?

	CUtlVector<Vector> m_pastPositions;
	CUtlVector<Vector> m_pastOrient;

	int m_iCurrentLine;
	int m_iCurrentPriority;

	int m_iPendingLine;

	string_t	m_iszInfoTargetScriptName;
	string_t	m_iszLastLinePlayed;

	string_t	m_iszDeathSoundScriptName;
	string_t	m_iszLookAnimationName;		// Different animations for each personality
	string_t	m_iszVoiceName;

	CORETYPE	m_iCoreType;
};

LINK_ENTITY_TO_CLASS( prop_personality_sphere, CPropPersonalitySphere );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CPropPersonalitySphere )

	DEFINE_FIELD( m_iEyeballAttachment,						FIELD_INTEGER ),
	DEFINE_FIELD( m_iCurrentLine,							FIELD_INTEGER ),
	DEFINE_FIELD( m_iCurrentPriority,						FIELD_INTEGER ),
	DEFINE_FIELD( m_iszLastLinePlayed,						FIELD_STRING ),
	DEFINE_FIELD( m_iszInfoTargetScriptName,				FIELD_STRING ),
	DEFINE_FIELD( m_iszDeathSoundScriptName,				FIELD_STRING ),
	DEFINE_FIELD( m_iszLookAnimationName,					FIELD_STRING ),
	DEFINE_FIELD( m_iszVoiceName,							FIELD_STRING ),
	DEFINE_FIELD( m_flLineCompletionTime,					FIELD_FLOAT ),
	DEFINE_FIELD( m_flIdleWaitTime,							FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastHeldTime,							FIELD_FLOAT ),
	DEFINE_FIELD( m_flAbuseLevel,							FIELD_FLOAT ),
	

	DEFINE_UTLVECTOR( m_pastPositions,						FIELD_VECTOR ),
	DEFINE_UTLVECTOR( m_pastOrient,							FIELD_VECTOR ),

	DEFINE_FIELD( m_bFirstPickup,							FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHeld,									FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_iCoreType,			FIELD_INTEGER, "CoreType" ),
	DEFINE_KEYFIELD( m_flBetweenVOPadding,  FIELD_FLOAT, "DelayBetweenLines" ),

	DEFINE_THINKFUNC( TalkingThink ),
	DEFINE_THINKFUNC( AnimateThink ),
	
END_DATADESC()

CPropPersonalitySphere::CPropPersonalitySphere()
{
	m_iCurrentLine = m_iCurrentPriority = 0;
	m_iszLookAnimationName = m_iszDeathSoundScriptName = m_iszVoiceName = m_iszInfoTargetScriptName = m_iszLastLinePlayed = NULL_STRING;
	m_flBetweenVOPadding = 2.5f;
	m_flIdleWaitTime = 4.0f;
	m_flAbuseLevel = 0.0f;
	m_bFirstPickup = true;
	m_bHeld = false;
}

CPropPersonalitySphere::~CPropPersonalitySphere()
{
//	m_speechEvents.Purge();
}

void CPropPersonalitySphere::Spawn( void )
{
	Precache();
	KeyValue( "model", GLADOS_CORE_MODEL_NAME );
	BaseClass::Spawn();

	//Default to 'dropped' animation
	ResetSequence(LookupSequence("drop"));
	SetCycle( 1.0f );

	DisableAutoFade();
	m_iEyeballAttachment = LookupAttachment( "eyeball" );

	SetContextThink( &CPropPersonalitySphere::AnimateThink, gpGlobals->curtime + 0.1f, s_pAnimateThinkContext );

	if ( m_iCoreType == CORETYPE_AQUARIUM )
	{
		StartTalking( 0.0f );
	}
}

void CPropPersonalitySphere::RegisterSoundEvent( SphereEvent_e eventName )
{
	m_bHasEvent[eventName] = true;
	PrecacheScriptSound( CFmtStr( "%s.%s", STRING(m_iszVoiceName), g_LineNames[eventName] ) );
}

void CPropPersonalitySphere::Precache( void )
{
	BaseClass::Precache();

	SetupVOList();
	for( unsigned int i = 0; i < SPHERE_EVENT_TOTAL; i++ )
	{
		m_bHasEvent[i] = false;
	}

	switch ( m_iCoreType )
	{
	case CORETYPE_RICHARD:
		RegisterSoundEvent( SPHERE_EVENT_ABANDONED );
		RegisterSoundEvent( SPHERE_EVENT_FIRST_PICKUP );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_TURN_LEFT );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_TURN_RIGHT );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_TURN_UP );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_TURN_DOWN );
		RegisterSoundEvent( SPHERE_EVENT_PICKUP );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_FORWARD );
		RegisterSoundEvent( SPHERE_EVENT_MOVEMENT_BACKWARD );
		RegisterSoundEvent( SPHERE_EVENT_ABANDONED );
		RegisterSoundEvent( SPHERE_EVENT_HURT );
		RegisterSoundEvent( SPHERE_EVENT_HURT_REPEATEDLY );
		RegisterSoundEvent( SPHERE_EVENT_DROPPED );
		RegisterSoundEvent( SPHERE_EVENT_THROWN );
		RegisterSoundEvent( SPHERE_EVENT_VELOCITY_UP_LOW );
		RegisterSoundEvent( SPHERE_EVENT_VELOCITY_UP_HIGH );
		RegisterSoundEvent( SPHERE_EVENT_VELOCITY_DOWN_HIGH );
		RegisterSoundEvent( SPHERE_EVENT_VELOCITY_LATERAL_LOW );
		RegisterSoundEvent( SPHERE_EVENT_VELOCITY_LATERAL_HIGH );
		RegisterSoundEvent( SPHERE_EVENT_PORTALED );	
		RegisterSoundEvent( SPHERE_EVENT_SLOWTIME );
		break;
	case CORETYPE_AQUARIUM:
		RegisterSoundEvent( SPHERE_EVENT_WAITING );
		RegisterSoundEvent( SPHERE_EVENT_ABANDONED );
		RegisterSoundEvent( SPHERE_EVENT_IDLE );
		RegisterSoundEvent( SPHERE_EVENT_HURT );
		break;
	}

	PrecacheModel( GLADOS_CORE_MODEL_NAME );
}

void CPropPersonalitySphere::StartTalking( float flDelay )
{
	SetThink( &CPropPersonalitySphere::TalkingThink );
	SetNextThink( gpGlobals->curtime + m_flBetweenVOPadding + flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: Start playing personality VO list
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::TalkingThink( void )
{
	// Loop the 'look around' animation after the first line.
	int iCurSequence = GetSequence();
	int iLookSequence = LookupSequence( STRING(m_iszLookAnimationName) );
	if ( iCurSequence != iLookSequence /*&& m_iSpeechIter > 0*/ )
	{
		ResetSequence( iLookSequence );
	}

	SetNextThink( gpGlobals->curtime + 0.05f );

	if ( m_bFirstPickup )
	{
		TrySpeakLine( SPHERE_EVENT_WAITING, 0 );
		return;
	}

	float flInterval = gpGlobals->curtime - GetLastThink();
	m_flAbuseLevel *= (1.0f - 0.5*flInterval);
//	Msg("Abuse level %f\n", m_flAbuseLevel);

	
	CheckForInfoTargets();
	UpdatePositions();

	// if we've got nothing else then do some idle talk.
	if ( gpGlobals->curtime > m_flLineCompletionTime + m_flIdleWaitTime )
	{
		TrySpeakLine( SPHERE_EVENT_IDLE, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::CheckForInfoTargets( void )
{
	CPortal_Player *pPlayer = (CPortal_Player *) AI_GetSinglePlayer();
	if( !pPlayer || !m_bHeld )
	{
		return;
	}

	// fire a ray where the player is looking
	Vector vecStart = pPlayer->EyePosition();
	Vector vecForward, vecRight, vecUp;
	pPlayer->EyeVectors( &vecForward, &vecRight, &vecUp );

	{
		Ray_t ray;
		ray.Init( vecStart, vecStart + (400.0f * vecForward) );

		CInfoTargetEntityEnumerator InfoTargetEnum( ray );
		
		// get all of the entities where we are looking.
		trace_t tr;
		UTIL_TraceRay( ray, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

		CBaseEntity *ppEnts[256];
		int nEntCount = UTIL_EntitiesInSphere( ppEnts, 256, tr.endpos, 100.f + tr.fraction*200.f, 0 );

		int i;
		for ( i = 0; i < nEntCount; i++ )
		{
			if ( ppEnts[i] == NULL )
				continue;

			InfoTargetEnum.EnumEntity( ppEnts[i] );
		}

		CInfoTargetPersonalitySphere* pEnt = ( CInfoTargetPersonalitySphere* ) InfoTargetEnum.GetBestInfoTargetEntity();

		if ( !pEnt )
			return; 

		m_iszInfoTargetScriptName = pEnt->GetSphereLine();
		TrySpeakLine ( SPHERE_EVENT_INFO_TARGET, 8 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::UpdatePositions( void )
{
	CPortal_Player *pPlayer = (CPortal_Player *) AI_GetSinglePlayer();
	if( !pPlayer )
	{
		return;
	}
	// we need to do better then this to trigger a movement based comment
	float flBestFit = 0.5f;
	int iBestLine = -1;
	Vector vecForward = vec3_origin;

	// record our motion, we are going to use this to see if we are moving about.
	m_pastPositions.AddToTail( GetAbsOrigin() );
	if ( m_pastPositions.Count() > 40 )
	{
		m_pastPositions.RemoveMultipleFromHead( 1 );
	}

	if ( m_bHeld )
	{
		pPlayer->EyeVectors( &vecForward );
		m_pastOrient.AddToTail( vecForward );
		if ( m_pastOrient.Count() > 20 )
		{
			m_pastOrient.RemoveMultipleFromHead( 1 );
		}
	}
	else
	{
		m_pastOrient.RemoveAll();
		if ( gpGlobals->curtime > m_flLastHeldTime + 10.f )
		{
			TrySpeakLine ( SPHERE_EVENT_ABANDONED, 1 );
		}
	}

	
	// get our average velocity
	Vector vecVelocity = vec3_origin;
	for ( int i = 1; i < m_pastPositions.Count(); i++ )
	{
		vecVelocity += ( m_pastPositions[i] - m_pastPositions[i - 1] );
	}
	if ( m_pastPositions.Count() > 1 )
	{
		vecVelocity /= (m_pastPositions.Count() - 1);
	}

	if ( !m_bHeld )
	{
//		Msg("%f - %f\n",vecVelocity.Length(), 400.0f*(gpGlobals->curtime - GetLastThink() ) );
		if( vecVelocity.Length() > 400.0f*(gpGlobals->curtime - GetLastThink() ) )
		{
			TrySpeakLine ( SPHERE_EVENT_VELOCITY_DOWN_HIGH, 5 );		
		}
		return;
	}

	// check ducking first, this is likely to get overwritten
	if( ( pPlayer->GetFlags() & FL_DUCKING ) && ( pPlayer->GetFlags() & FL_ONGROUND ) )
	{
		TrySpeakLine ( SPHERE_EVENT_MOVEMENT_DUCK, 2 );
	}

	// get our average velocity
	Vector vecFacing = vec3_origin;
	for ( int i = 1; i < m_pastOrient.Count(); i++ )
	{
		vecFacing += m_pastOrient[i];
	}
	if ( m_pastOrient.Count() > 1 )
	{
		vecFacing /= (m_pastOrient.Count() - 1);
	}

	// start be checking if we are looking up or down.
	{
		if( vecFacing.z > flBestFit )
		{
			iBestLine = SPHERE_EVENT_MOVEMENT_TURN_UP;
			flBestFit = vecFacing.z;

//			Msg( "Looking up %f\n", flBestFit );
		}

		if( -vecFacing.z > flBestFit )
		{
			iBestLine = SPHERE_EVENT_MOVEMENT_TURN_DOWN;
			flBestFit = -vecFacing.z;

//			Msg( "Looking down %f\n", flBestFit );
		}
	}

//	Msg( "Velocity = %f %f %f --- %f\n", vecVelocity.x, vecVelocity.y, vecVelocity.z, vecVelocity.Length() );
//	Msg( "Forward  = %f %f %f\n", vecForward.x, vecForward.y, vecForward.z );

//	Msg( "PV = %f %f %f\n", pPlayer->GetAbsVelocity().x, pPlayer->GetAbsVelocity().y, pPlayer->GetAbsVelocity().z );
	
//	if ( pPlayer->IsSlowingTime() )
//	{
//		TrySpeakLine ( SPHERE_EVENT_SLOWTIME, 5 );
//		Msg("Slowtime!\n");
//	}


	if( pPlayer->GetAbsVelocity().z < -400.0f )
	{
		flBestFit = 1;
		iBestLine = SPHERE_EVENT_VELOCITY_DOWN_HIGH;		
	}
	else if( pPlayer->GetAbsVelocity().z > 400.0f )
	{
		flBestFit = 1;
		iBestLine = SPHERE_EVENT_VELOCITY_UP_HIGH;
	}
	else if( pPlayer->GetAbsVelocity().z > 100.0f )
	{
		// jumping
		flBestFit = 1;
		iBestLine = SPHERE_EVENT_VELOCITY_UP_LOW;
	}
	else if( pPlayer->GetAbsVelocity().Length() > 400.0f )
	{
		// Moving fast in some weird direction
		flBestFit = 1;
		iBestLine = SPHERE_EVENT_VELOCITY_LATERAL_HIGH;
	}
	else if( abs( vecVelocity.z ) > 20.f && abs( pPlayer->GetAbsVelocity().z ) < 0.1f )
	{
		TrySpeakLine ( SPHERE_EVENT_MOVEMENT_LAND_HARD, 4 );

//		Msg("Hard Landing\n");
	}
	else if( vecVelocity.Length() < 14.f )
	{
		if ( vecVelocity.Length() > 7.0f )
		{
			vecVelocity.NormalizeInPlace();
			vecVelocity.z = 0;
			vecForward.z = 0;
			float flFit = DotProduct( vecVelocity, vecForward );
			if( flBestFit < flFit )
			{
				flBestFit = flFit;
				iBestLine = SPHERE_EVENT_MOVEMENT_FORWARD;
//				Msg("I'm moving forward! %f\n", flFit);
			}
			else if( flBestFit < -flFit )
			{
				flBestFit = flFit;
				iBestLine = SPHERE_EVENT_MOVEMENT_BACKWARD;
//				Msg("I'm moving backwards! %f\n", flFit);
			}

		}
		else if( vecVelocity.Length() > 4.0f )
		{
			float flInterval = gpGlobals->curtime - GetLastThink();
//			Msg( "PV = %f %f %f --- %f\n", pPlayer->GetAbsVelocity().x, pPlayer->GetAbsVelocity().y, pPlayer->GetAbsVelocity().z, flInterval*pPlayer->GetAbsVelocity().Length() );
			if ( flInterval*pPlayer->GetAbsVelocity().Length() < 2.f )
			{
				flBestFit = 1;
				iBestLine = SPHERE_EVENT_MOVEMENT_STOPPED;
//				Msg("I'm stopped!\n");
			}
		}
	}
	
	if ( iBestLine >= 0 )
	{
		TrySpeakLine ( iBestLine, 3 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::TrySpeakLine( int line, int priority )
{
	if ( !m_bHasEvent[line] )
	{
		return;
	}
	
	// if our new priority is higher then interupt what we are doing
	// make sure that we have been talking for a little while before this 
	// happens, we don't want to stutter.
	if ( priority > m_iCurrentPriority )
	{
//		Msg( "stopping sound %s\n", STRING( m_iszLastLinePlayed ) );
		StopSound( STRING(m_iszLastLinePlayed) );
		m_flLineCompletionTime = gpGlobals->curtime;
	}
	
	// if we are still talking then we wont do anything.
	if ( m_flLineCompletionTime > gpGlobals->curtime || 
		( m_flAbuseLevel > 1.0f && priority <= 3 ) )
	{		
		return;
	}

	float flAdditionalPadding = 1.5f;

	if ( line == SPHERE_EVENT_HURT && m_bHasEvent[SPHERE_EVENT_HURT_REPEATEDLY ] )
	{
		m_flAbuseLevel += 1.f;
		if ( m_flAbuseLevel > 2.f )
		{
			line = SPHERE_EVENT_HURT_REPEATEDLY;
		}
	}

	if ( line == SPHERE_EVENT_HURT ||
		 line == SPHERE_EVENT_MOVEMENT_BACKWARD )
	{
		flAdditionalPadding = 0.f;
	}
	
	if ( m_iCoreType == CORETYPE_AQUARIUM )
	{
		flAdditionalPadding = 0.5f;
	}

//	Msg("%d - %d\n",line, priority);

	// We aren't currently playing anything so we are safe to play!
	const char* lineName = ( line == SPHERE_EVENT_INFO_TARGET ) ? STRING(m_iszInfoTargetScriptName) : g_LineNames[line];
	
	m_iszLastLinePlayed = AllocPooledString( ( CFmtStr( "%s.%s", STRING(m_iszVoiceName), lineName ) ) );

//	Msg( "Playing sound %s\n", STRING( m_iszLastLinePlayed ) );

	EmitSound( STRING( m_iszLastLinePlayed ) );

	m_iCurrentLine = line;
	m_iCurrentPriority = priority;
	

	float flCurDuration = GetSoundDuration( STRING(m_iszLastLinePlayed), GLADOS_CORE_MODEL_NAME ); //CFmtStr( "%s.%s", m_iszVoiceName, g_LineNames[line] ), GLADOS_CORE_MODEL_NAME );				
	m_flLineCompletionTime = gpGlobals->curtime + m_flBetweenVOPadding + flCurDuration + flAdditionalPadding;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::AnimateThink()
{
	StudioFrameAdvance();
	SetContextThink( &CPropPersonalitySphere::AnimateThink, gpGlobals->curtime + 0.1f, s_pAnimateThinkContext );
}

//-----------------------------------------------------------------------------
// Purpose: Setup list of lines based on core personality
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::SetupVOList( void )
{
//	m_speechEvents.RemoveAll();

//	m_iCoreType = CORETYPE_RICHARD;

	switch ( m_iCoreType )
	{
	case CORETYPE_RICHARD:
		{
			m_iszLookAnimationName = AllocPooledString( RICHARD_LOOK_ANINAME );
			m_iszVoiceName = AllocPooledString( RICHARD_VOICE_NAME );
			m_nSkin = RICHARD_SKIN;
			m_flBetweenVOPadding = 0.5f;
		}
		break;
	case CORETYPE_AQUARIUM:
		{
			m_iszLookAnimationName = AllocPooledString( AQUARIUM_LOOK_ANINAME );
			m_iszVoiceName = AllocPooledString( AQUARIUM_VOICE_NAME );
			m_nSkin = AQUARIUM_SKIN;
			m_flBetweenVOPadding = 0.1f;
			m_flIdleWaitTime = -0.1f; //!!
		}
		break;
	case CORETYPE_CURIOUS:
		{
//			m_iszVoiceName = AllocPooledString( SHPERE_VOICE_NAME );
			m_iszLookAnimationName = AllocPooledString( CURIOUS_LOOK_ANINAME );
			m_nSkin = CURIOUS_SKIN;
			
		}
		break;
	case CORETYPE_AGGRESSIVE:
		{
			m_iszLookAnimationName = AllocPooledString( AGGRESSIVE_LOOK_ANINAME );
			m_nSkin = AGGRESSIVE_SKIN;
		}
		break;
	case CORETYPE_CRAZY:
		{
			m_iszLookAnimationName = AllocPooledString( CRAZY_LOOK_ANINAME );
			m_nSkin = CRAZY_SKIN;
		}
		break;
	default:
		{
			m_iszLookAnimationName = AllocPooledString( DEFAULT_LOOK_ANINAME );
			m_nSkin = DEFAULT_SKIN;
		}
		break;
	};

	m_iszDeathSoundScriptName =  AllocPooledString( "Portal.Glados_core.Death" );
}

//-----------------------------------------------------------------------------
// Purpose: Cores play a special animation when picked up and dropped
// Input  : pPhysGunUser - player picking up object
//			reason - type of pickup
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::OnPhysGunPickup( CBasePlayer* pPhysGunUser, PhysGunPickup_t reason )
{
	if ( m_bFirstPickup )
	{
		StartTalking ( 0.f );
		TrySpeakLine ( SPHERE_EVENT_FIRST_PICKUP, 10 );
	}
	else
	{
		TrySpeakLine ( SPHERE_EVENT_PICKUP, 3 );
	}

	m_bFirstPickup = false;
	m_bHeld = true;
	ResetSequence(LookupSequence("turn"));

	// +use always enables motion on these props
	EnableMotion();

	BaseClass::OnPhysGunPickup ( pPhysGunUser, reason );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::OnPhysGunDrop( CBasePlayer* pPhysGunUser, PhysGunDrop_t reason )
{
	TrySpeakLine ( SPHERE_EVENT_DROPPED, 4 );

	m_bHeld = false;
	m_flLastHeldTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// We hit something, try and talk about it.
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );
	CBaseEntity *pHitEntity = pEvent->pEntities[!index];

	if ( pHitEntity->IsWorld() )
	{
		if( m_bHeld )
		{
			TrySpeakLine ( SPHERE_EVENT_HURT, 6 );
		}
		else
		{
			Vector vecVelocity = pEvent->preVelocity[index];// GetAbsVelocity();
//			Msg("Impacted with speed %f\n", vecVelocity.Length() );
			if( vecVelocity.Length() > 100.f )
			{
				TrySpeakLine ( SPHERE_EVENT_HURT, 6 );
			}
		}
	}
	else if ( pHitEntity->IsPlayer() )
	{
		// this just gets confusing. Maybe add in a check if you are jumping on the sphere?
	}
	else
	{
		TrySpeakLine ( SPHERE_EVENT_HURT, 6 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPropPersonalitySphere::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	if ( eventType == NOTIFY_EVENT_TELEPORT )
	{
		m_pastPositions.RemoveAll();
		m_pastOrient.RemoveAll();
		TrySpeakLine ( SPHERE_EVENT_PORTALED, 4 );
	}
}
