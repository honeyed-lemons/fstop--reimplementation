//==== Copyright © 1996-2007, Valve Corporation, All rights reserved. =========
//
// Missile shooting version of the android. Used for area denial like the turrets in Portal.
//
//=============================================================================

#include "cbase.h"

#include "ai_basenpc.h"
#include "npcevent.h"
#include "props.h"
#include "smoke_trail.h"
#include "func_break.h"
#include "datacache/imdlcache.h"

#include "doors.h"

#include "particle_parse.h"
#include "npc_android.h"

#include "ieffects.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *ANDROID_MISSILE_MODEL = "models/weapons/w_missile.mdl";
//static const char *ANDROID_MISSILE_MODEL = "models/weapons/hunter_flechette.mdl";

ConVar android_missile_speed( "android_missile_speed", "250" );
ConVar sk_android_missile_dmg( "sk_android_missile_dmg", "0.0" );
ConVar sk_android_missile_explode_dmg( "sk_android_missile_explode_dmg", "20.0" );
ConVar sk_android_missile_explode_radius( "sk_android_missile_explode_radius", "128.0" );
ConVar android_cheap_explosions( "android_cheap_explosions", "1" );

#define ANDROID_MISSILE_WARN_TIME		1.0f

//-----------------------------------------------------------------------------
// Animation events
//-----------------------------------------------------------------------------
int AE_ANDROID_STARTSHOOT;
int AE_ANDROID_SHOOT;

//-----------------------------------------------------------------------------
// The android can fire a volley of explosive missiles.
//-----------------------------------------------------------------------------
static const char *s_szAndroidMissileSeekThink = "AndroidMissileSeekThink";
static const char *s_szAndroidMissileDangerSoundThink = "AndroidMissileDangerSoundThink";
static const char *s_szAndroidMissileSpriteTrail = "sprites/bluelaser1.vmt";
static int s_nAndroidMissileImpact = -2;
static int s_nMissileFuseAttach = -1;


class CAndroidMissile : public CPhysicsProp, public IParentPropInteraction
{
	DECLARE_CLASS( CAndroidMissile, CPhysicsProp );

public:

	CAndroidMissile();
	~CAndroidMissile();

	Class_T Classify() { return CLASS_NONE; }
	
	bool WasThrownBack()
	{
		return m_bThrownBack;
	}

public:

	void Spawn();
	void Activate();
	void Precache();
	void Shoot( Vector &vecVelocity, bool bBright );
	void SetSeekTarget( CBaseEntity *pTargetEntity );
	void Explode();
	float GetScaleDamageScalar( void );

	START_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery )
	{
	public:
		virtual bool GetPlacementPosition( CaptureInfo_t &captureInfo, CheckPlacementData_t &placementData, Vector &positionOut, QAngle &anglesOut );
		virtual float GetMaxPlacementDistance( void );

	protected:
		virtual CameraInfo_ScaleData_t *GetSimpleScales( void );
	};
	END_BRANCHING_SINGLETON_DEFINITION( CPhotoPlacementQuery );

	virtual void OnReleased( void );
	virtual void OnCaptured( void );
	void AdjustTragectoryForSize( int nSize );

	bool CreateVPhysics();

	unsigned int PhysicsSolidMaskForEntity() const;
	static CAndroidMissile *MissileCreate( const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner = NULL );

	// IParentPropInteraction
	void OnParentCollisionInteraction( parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent );
	void OnParentPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

protected:

	void SetupGlobalModelData();

	void StickTo( CBaseEntity *pOther, trace_t &tr );

	void DangerSoundThink();
	void ExplodeThink();
	void DopplerThink();
	void OrientThink();
	void SeekThink();

	void CreateSmokeTrail( void );

	void MissileTouch( CBaseEntity *pOther );
	
	Vector m_vecShootPosition;
	EHANDLE m_hSeekTarget;
	bool m_bThrownBack;
	CHandle<RocketTrail>	m_hRocketTrail;


	DECLARE_DATADESC();
	//DECLARE_SERVERCLASS();
};

LINK_ENTITY_TO_CLASS( android_missile, CAndroidMissile );

BEGIN_DATADESC( CAndroidMissile )

	DEFINE_THINKFUNC( DangerSoundThink ),
	DEFINE_THINKFUNC( ExplodeThink ),
	DEFINE_THINKFUNC( DopplerThink ),
	DEFINE_THINKFUNC( OrientThink ),
	DEFINE_THINKFUNC( SeekThink ),
	
	DEFINE_ENTITYFUNC( MissileTouch ),

	DEFINE_FIELD( m_hRocketTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecShootPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hSeekTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bThrownBack, FIELD_BOOLEAN ),

END_DATADESC()


bool CAndroidMissile::CPhotoPlacementQuery::GetPlacementPosition( CaptureInfo_t &captureInfo,
																 CheckPlacementData_t &placementData,
																 Vector &positionOut,
																 QAngle &anglesOut )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( placementData.qTraceAngles, &vecForward, &vecRight, &vecUp );

	Vector vDirection = vecForward + (vecUp * 0.2f);
	vDirection.NormalizeInPlace();
	VectorAngles( vDirection, vecUp, anglesOut );
	positionOut = placementData.Trace.endpos;
	return true;
}

float CAndroidMissile::CPhotoPlacementQuery::GetMaxPlacementDistance( void )
{
	return 5.0f * 12.0f;
}


CameraInfo_ScaleData_t *CAndroidMissile::CPhotoPlacementQuery::GetSimpleScales( void )
{
	static float s_DefaultScales[] = { 0.5f, 1.0f, 2.0f };
	static CameraInfo_ScaleData_t simpleScales( s_DefaultScales, sizeof(s_DefaultScales)/sizeof(float) );
	return &simpleScales;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CAndroidMissile *CAndroidMissile::MissileCreate( const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner )
{
	// Create a new entity with CAndroidMissile private data
	CAndroidMissile *pMissile = (CAndroidMissile *)CreateEntityByName( "android_missile" );
	((CAndroidMissile *)pMissile)->SetObjectScaleLevel( ((CBaseAnimating *)pentOwner)->GetObjectScaleLevel() );

	UTIL_SetOrigin( pMissile, vecOrigin );
	pMissile->SetAbsAngles( angAngles );
	pMissile->Spawn();
	pMissile->Activate();
	pMissile->SetOwnerEntity( pentOwner );

	return pMissile;
}


void ShootMissiles( Vector vOrigin, QAngle angles, CBaseEntity *pOwner )
{
	MDLCACHE_CRITICAL_SECTION();

	CBasePlayer *pPlayer = (CBasePlayer*)pOwner;

	CAndroidMissile *entity = CAndroidMissile::MissileCreate( vOrigin, angles, pPlayer );
	if ( entity )
	{
		entity->Precache();
		DispatchSpawn( entity );

		// Shoot the missile.		
		Vector forward;
		pPlayer->EyeVectors( &forward );
		entity->Shoot( forward * 250.0f, false );

		QAngle angles;

		VectorAngles( forward, angles );

		entity->SetAbsAngles( angles );
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_Android_Shoot_Missile( const CCommand& args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	QAngle angEye = pPlayer->EyeAngles();
	Vector vOrigin = pPlayer->EyePosition();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	ShootMissiles( vOrigin, angEye, pPlayer );

	CBaseEntity::SetAllowPrecache( allowPrecache );
}

static ConCommand ent_create("android_shoot_missile", CC_Android_Shoot_Missile, "Fires a android missile where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CAndroidMissile::CAndroidMissile()
{
	UseClientSideAnimation();
	m_hRocketTrail = NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CAndroidMissile::~CAndroidMissile()
{
}


//-----------------------------------------------------------------------------
// If set, the missile will seek unerringly toward the target as it flies.
//-----------------------------------------------------------------------------
void CAndroidMissile::SetSeekTarget( CBaseEntity *pTargetEntity )
{
	if ( pTargetEntity )
	{
		m_hSeekTarget = pTargetEntity;
		SetContextThink( &CAndroidMissile::SeekThink, gpGlobals->curtime, s_szAndroidMissileSeekThink );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAndroidMissile::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CAndroidMissile::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}


//-----------------------------------------------------------------------------
// Called from CPropPhysics code when we're attached to a physics object.
//-----------------------------------------------------------------------------
void CAndroidMissile::OnParentCollisionInteraction( parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent )
{
	if ( eType == COLLISIONINTER_PARENT_FIRST_IMPACT )
	{
		m_bThrownBack = true;
		Explode();
	}
}

void CAndroidMissile::OnParentPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason )
{
	m_bThrownBack = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::OnReleased( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	assert( pPlayer );

	// release like normal
	BaseClass::OnReleased();

	// then shoot based on where we are looking
	Vector vecViewDir = pPlayer->EyeDirection3D();

	vecViewDir *= android_missile_speed.GetFloat();

	AdjustTragectoryForSize(GetObjectScaleLevel());
	Shoot( vecViewDir, false );
	SetOwnerEntity( pPlayer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::OnCaptured( void )
{
	if( m_hRocketTrail )
	{
		m_hRocketTrail->SetLifetime(0.1f);
		m_hRocketTrail = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::AdjustTragectoryForSize( int nSize )
{
	SetMoveType( nSize == 1 ? MOVETYPE_FLYGRAVITY : MOVETYPE_FLY, MOVECOLLIDE_FLY_CUSTOM );
	SetGravity( nSize == 1 ? 0.25f : 0.0f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::Spawn()
{
	Precache( );

	AdjustTragectoryForSize(GetObjectScaleLevel());

	SetModel( ANDROID_MISSILE_MODEL );
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	
	SetTouch( &CAndroidMissile::MissileTouch );

	// Make us glow until we've hit the wall
	m_nSkin = 1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::Activate()
{
	BaseClass::Activate();
	SetupGlobalModelData();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::SetupGlobalModelData()
{
	if ( s_nAndroidMissileImpact == -2 )
	{
		s_nAndroidMissileImpact = LookupSequence( "impact" );
		s_nMissileFuseAttach = LookupAttachment( "attach_fuse" );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::Precache()
{
	PrecacheModel( ANDROID_MISSILE_MODEL );
	PrecacheModel( "sprites/light_glow02_noz.vmt" );

	PrecacheScriptSound( "NPC_Android.MissileNearmiss" );
	PrecacheScriptSound( "NPC_Android.MissileHitBody" );
	PrecacheScriptSound( "NPC_Android.MissileHitWorld" );
	PrecacheScriptSound( "NPC_Android.MissilePreExplode" );
	PrecacheScriptSound( "NPC_Android.MissileExplode" );

	PrecacheParticleSystem( "hunter_flechette_trail_striderbuster" );
	PrecacheParticleSystem( "hunter_flechette_trail" );
	PrecacheParticleSystem( "hunter_projectile_explosion_1" );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::StickTo( CBaseEntity *pOther, trace_t &tr )
{
	EmitSound( "NPC_Android.MissileHitWorld" );

	// Bullseyes don't have valid bones so don't attach to them because it causes
	// a crash in bone setup.
	if ( !pOther->IsWorld() && ( pOther->Classify() != CLASS_BULLSEYE ) )
	{
		SetParent( pOther );
		SetSolid( SOLID_NONE );
		SetSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_NONE );
	}
	else
	{
		CreateVPhysics();

		if ( VPhysicsGetObject() )
		{
			SetMoveType( MOVETYPE_VPHYSICS );
			VPhysicsGetObject()->EnableMotion( false );
			AddSpawnFlags( SF_PHYSPROP_ENABLE_ON_PHYSCANNON );
			UTIL_SetSize( this, -Vector(8,8,8), Vector(8,8,8) );
		}
	}
	
	Vector vecVelocity = GetAbsVelocity();

	SetTouch( NULL );

	// Stop seeking.
	m_hSeekTarget = NULL;
	SetContextThink( NULL, 0, s_szAndroidMissileSeekThink );

	// Get ready to explode.
	SetThink( &CAndroidMissile::ExplodeThink );
	SetNextThink( gpGlobals->curtime );

	// Play our impact animation.
	ResetSequence( s_nAndroidMissileImpact );

	UTIL_ImpactTrace( &tr, DMG_BULLET );
	
	// Shoot some sparks
	if (UTIL_PointContents(GetAbsOrigin(), MASK_WATER) & CONTENTS_WATER)
	{
		g_pEffects->Sparks( GetAbsOrigin() );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::MissileTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	if ( FClassnameIs( pOther, "android_missile" ) )
		return;

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize( vecNormalizedVel );

		float flDamage = sk_android_missile_dmg.GetFloat();
		CBreakable *pBreak = dynamic_cast <CBreakable *>(pOther);
		if ( pBreak && ( pBreak->GetMaterialType() == matGlass ) )
		{
			flDamage = MAX( pOther->GetHealth(), flDamage );
		}

		CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), flDamage, DMG_DISSOLVE | DMG_NEVERGIB );
		CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
		dmgInfo.SetDamagePosition( tr.endpos );
		pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );

		ApplyMultiDamage();

		// Keep going through breakable glass.
		if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			 return;
			 
		SetAbsVelocity( Vector( 0, 0, 0 ) );

		// play body "thwack" sound
		EmitSound( "NPC_Android.MissileHitBody" );

		StopParticleEffects( this );

		Vector vForward;
		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		trace_t	tr2;
		UTIL_TraceLine( GetAbsOrigin(),	GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2 );

		if ( tr2.fraction != 1.0f )
		{
			//NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
			//NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			
				DispatchEffect( "BoltImpact", data );
			}
		}

		Explode();

		if ( ( ( pOther->GetMoveType() == MOVETYPE_VPHYSICS ) || ( pOther->GetMoveType() == MOVETYPE_PUSH ) ) && ( ( pOther->GetHealth() > 0 ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) ) )
		{
			CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>( pOther );
			if ( pProp )
			{
				pProp->SetInteraction( PROPINTER_PHYSGUN_NOTIFY_CHILDREN );
			}
		
			// We hit a physics object that survived the impact. Stick to it.
			StickTo( pOther, tr );
		}
		else
		{
			SetTouch( NULL );
			SetThink( NULL );

			UTIL_Remove( this );
		}
	}
	else
	{
		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			// We hit a physics object that survived the impact. Stick to it.
			StickTo( pOther, tr );
		}
		else if( pOther->GetMoveType() == MOVETYPE_PUSH && FClassnameIs(pOther, "func_breakable") )
		{
			// We hit a func_breakable, stick to it.
			// The MOVETYPE_PUSH is a micro-optimization to cut down on the classname checks.
			StickTo( pOther, tr );
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}

			UTIL_Remove( this );
		}
	}
}


//-----------------------------------------------------------------------------
// Fixup missile position when seeking towards a striderbuster.
//-----------------------------------------------------------------------------
void CAndroidMissile::SeekThink()
{
	if ( m_hSeekTarget )
	{
		Vector vecBodyTarget = m_hSeekTarget->BodyTarget( GetAbsOrigin() );

		Vector vecClosest;
		CalcClosestPointOnLineSegment( GetAbsOrigin(), m_vecShootPosition, vecBodyTarget, vecClosest, NULL );

		Vector vecDelta = vecBodyTarget - m_vecShootPosition;
		VectorNormalize( vecDelta );

		QAngle angShoot;
		VectorAngles( vecDelta, angShoot );

		float flSpeed = android_missile_speed.GetFloat();
		if ( !flSpeed )
		{
			flSpeed = 250.0f;
		}

		Vector vecVelocity = vecDelta * flSpeed;
		Teleport( &vecClosest, &angShoot, &vecVelocity );

		SetNextThink( gpGlobals->curtime, s_szAndroidMissileSeekThink );
	}
}


//-----------------------------------------------------------------------------
// Play a near miss sound as we travel past the player.
//-----------------------------------------------------------------------------
void CAndroidMissile::DopplerThink()
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( !pPlayer )
		return;

	Vector vecVelocity = GetAbsVelocity();
	VectorNormalize( vecVelocity );
	
	float flMyDot = DotProduct( vecVelocity, GetAbsOrigin() );
	float flPlayerDot = DotProduct( vecVelocity, pPlayer->GetAbsOrigin() );

	if ( flPlayerDot <= flMyDot )
	{
		EmitSound( "NPC_Android.MissileNearMiss" );
		
		// We've played the near miss sound and we're not seeking. Stop thinking.
		SetThink( NULL );
	}
	else
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Play a near miss sound as we travel past the player.
//-----------------------------------------------------------------------------
void CAndroidMissile::OrientThink()
{
	QAngle	finalAngles;
	VectorAngles( GetAbsVelocity(), finalAngles );
	SetAbsAngles( finalAngles );
	
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::Shoot( Vector &vecVelocity, bool bBrightFX )
{
//	CreateSprites( bBrightFX );

	m_vecShootPosition = GetAbsOrigin();

	SetAbsVelocity( vecVelocity );

	SetThink( &CAndroidMissile::OrientThink );
	SetNextThink( gpGlobals->curtime );

	CreateSmokeTrail();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::DangerSoundThink()
{
	EmitSound( "NPC_Android.MissilePreExplode" );

	SetThink( &CAndroidMissile::ExplodeThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::ExplodeThink()
{
	Explode();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAndroidMissile::CreateSmokeTrail( void )
{
	if ( m_hRocketTrail )
		return;

	// Smoke trail.
	if ( (m_hRocketTrail = RocketTrail::CreateRocketTrail()) != NULL )
	{
		float fScale = GetModelScale();

		// we want to scale down the particles in a cubed fashion to match with the volume of the item. 
		fScale *= fScale;

		// we are just using this effect for the flare.
		m_hRocketTrail->m_SpawnRate = 0;
		m_hRocketTrail->m_flFlareScale = 1.0 * fScale;

		m_hRocketTrail->SetLifetime( 999 );
		m_hRocketTrail->FollowEntity( this, "0" );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAndroidMissile::Explode()
{
	SetSolid( SOLID_NONE );

	// Don't catch self in own explosion!
	m_takedamage = DAMAGE_NO;

	EmitSound( "NPC_Android.MissileExplode" );
	
	// Move the explosion effect to the tip to reduce intersection with the world.
	Vector vecFuse;
	GetAttachment( s_nMissileFuseAttach, vecFuse );
	DispatchParticleEffect( "hunter_projectile_explosion_1", vecFuse, GetAbsAngles(), NULL );

	int nDamageType = DMG_DISSOLVE;

	if( m_hRocketTrail )
	{
		m_hRocketTrail->SetLifetime(0.1f);
		m_hRocketTrail = NULL;
	}

	RadiusDamage( CTakeDamageInfo( this, GetOwnerEntity(), GetScaleDamageScalar()*sk_android_missile_explode_dmg.GetFloat(), nDamageType ), GetAbsOrigin(), sk_android_missile_explode_radius.GetFloat(), CLASS_NONE, NULL );
		
    AddEffects( EF_NODRAW );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CAndroidMissile::GetScaleDamageScalar( void )
{
	// normal takes 3 to kill
	float scalar = 1.0f;

	// the big size does just enough to kill an android in one hit.
	if( GetObjectScaleLevel() == 1 )
		scalar = 2.6f;
	// little ones need some punch too, it takes 5 to kill an android
	else if( GetObjectScaleLevel() == -1 )
		scalar = 0.5f;

	return scalar;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CNPC_Android_Missile : public CAI_BlendingHost<CNPC_Android>
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CNPC_Android_Missile, CAI_BlendingHost<CNPC_Android> );

public:
	virtual const char* GetBotModel( void );
	virtual bool	IsValidEnemy( CBaseEntity *pEnemy );
	virtual void BuildScheduleTestBits( void );
			void GatherConditions( void );
			int	 SelectSchedule ( void );
			int	 TranslateSchedule( int scheduleType );
			void RunTask( const Task_t *pTask );
	virtual void HandleAnimEvent( animevent_t *pEvent );
			bool ShootMissile( CBaseEntity *pTargetEntity, bool bSingleShot );

	virtual void Spawn( void );
	
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

LINK_ENTITY_TO_CLASS( npc_android_missile, CNPC_Android_Missile );

BEGIN_DATADESC( CNPC_Android_Missile )
	DEFINE_FIELD( m_hBlockingDoor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDoorBashYaw, FIELD_FLOAT ),

	DEFINE_EMBEDDED( m_DurationDoorBash ),
	DEFINE_EMBEDDED( m_NextTimeToStartDoorBash ),
	DEFINE_FIELD( m_vPositionCharged, FIELD_POSITION_VECTOR ),
END_DATADESC()

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_ANDROID_RANGE_ATTACK1 = LAST_BASE_ANDROID_SCHEDULE,
};

int ACT_ANDROID_RANGED_ATTACK1;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android_Missile::Spawn( void )
{
	BaseClass::Spawn();
	
	CapabilitiesRemove( bits_CAP_INNATE_MELEE_ATTACK1 );
	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_Android_Missile::GetBotModel( void )
{
	return "models/bot_male/bot_male.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Android_Missile::IsValidEnemy( CBaseEntity *pEnemy )
{
	return BaseClass::IsValidEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Android_Missile::HandleAnimEvent( animevent_t *pEvent )
{
	/// need to do some sort of "Bad stuff is comming" effect
	if ( pEvent->Event() == AE_ANDROID_STARTSHOOT)
	{
		return;
	}

	if ( pEvent->Event() == AE_ANDROID_SHOOT)
	{
		CBaseEntity* pEnemy = GetEnemy();
		if( pEnemy )
		{
			ShootMissile( pEnemy, true );
		}
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Android_Missile::ShootMissile( CBaseEntity *pTargetEntity, bool bSingleShot )
{
	if ( !pTargetEntity )
	{
		Assert( false );
		return false;
	}

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

	Vector vecDir;
	vecDir = pTargetEntity->GetAbsOrigin() - GetAbsOrigin();
	vecDir.NormalizeInPlace();

	QAngle angShoot;
	VectorAngles( vecDir, angShoot );

	CAndroidMissile *pMissile = CAndroidMissile::MissileCreate( GetAbsOrigin() + GetModelScale() * ( 36*vecUp + 10*vecForward ), angShoot, this );

	pMissile->AddEffects( EF_NOSHADOW );

	vecDir *= android_missile_speed.GetFloat();

	pMissile->Shoot( vecDir, false );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Android_Missile::SelectSchedule ( void )
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
void CNPC_Android_Missile::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if( !IsCurSchedule( SCHED_FLINCH_PHYSICS ) && !m_ActBusyBehavior.IsActive() )
	{
		SetCustomInterruptCondition( COND_PHYSICS_DAMAGE );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android_Missile::RunTask( const Task_t *pTask )
{
	BaseClass::RunTask( pTask );
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_Android_Missile::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_RANGE_ATTACK1:
		return SCHED_ANDROID_RANGE_ATTACK1;
	}
	return BaseClass::TranslateSchedule( scheduleType );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_Android_Missile::GatherConditions( void )
{
	BaseClass::GatherConditions();
}
//
////---------------------------------------------------------
////---------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_android_missile, CNPC_Android_Missile )

	DECLARE_ANIMEVENT( AE_ANDROID_STARTSHOOT )
	DECLARE_ANIMEVENT( AE_ANDROID_SHOOT )

	DECLARE_ACTIVITY( ACT_ANDROID_RANGED_ATTACK1 );

	DEFINE_SCHEDULE
	(
		SCHED_ANDROID_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_ANDROID_RANGED_ATTACK1"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_HEAR_DANGER"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_WEAPON_SIGHT_OCCLUDED"
	)

AI_END_CUSTOM_NPC()