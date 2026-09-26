//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "beam_shared.h"
#include "prop_portal_shared.h"


#define HOVER_TURRET_PORTAL_EYE_ATTACHMENT 1
#define HOVER_TURRET_PORTAL_LASER_ATTACHMENT 2
#define HOVER_TURRET_PORTAL_LASER_RANGE 8192

//#define FLOOR_TURRET_PORTAL_END_POINT_PULSE_SCALE 4.0f


class C_NPC_Hover_Turret : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_Hover_Turret, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	virtual ~C_NPC_Hover_Turret( void );

	virtual void	Spawn( void );
	virtual void	ClientThink( void );

	bool	IsLaserOn( void ) { return m_pBeam[0] != NULL; }
	void	LaserOff( void );
	void	LaserOn( void );
	float	LaserEndPointSize( void );

private:
	CBeam	*m_pBeam[4];

	bool	m_bOutOfAmmo;
	bool	m_bLaserOn;
	int		m_sLaserHaloSprite;
	float	m_fPulseOffset;

	float	m_flAimStartTime;

	float	m_bBeamFlickerOff;
	float	m_fBeamFlickerTime;

};


IMPLEMENT_CLIENTCLASS_DT( C_NPC_Hover_Turret, DT_NPC_HoverTurret, CNPC_HoverTurret )

	RecvPropFloat( RECVINFO( m_flAimStartTime ) ),
	RecvPropBool( RECVINFO( m_bLaserOn ) ),
	RecvPropInt( RECVINFO( m_sLaserHaloSprite ) ),

END_RECV_TABLE()


C_NPC_Hover_Turret::~C_NPC_Hover_Turret( void )
{
	LaserOff();
	for ( int i = 0; i < 4; i++ )
	{
		if ( m_pBeam[i] )
		{
			m_pBeam[i]->Remove();
		}
	}
}


void C_NPC_Hover_Turret::Spawn( void )
{
	SetThink( &C_NPC_Hover_Turret::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	for ( int i = 0; i < 4; i++ )
	{
		m_pBeam[i] = NULL;
	}

	m_fPulseOffset = RandomFloat( 0.0f, 2.0f * M_PI );

	m_bBeamFlickerOff = false;
	m_fBeamFlickerTime = 0.0f;

	BaseClass::Spawn();
}

void C_NPC_Hover_Turret::ClientThink( void )
{
	if ( m_bOutOfAmmo && m_fBeamFlickerTime < gpGlobals->curtime )
	{
		m_fBeamFlickerTime = gpGlobals->curtime + RandomFloat( 0.05f, 0.3f );
		m_bBeamFlickerOff = !m_bBeamFlickerOff;
	}

	if ( m_bLaserOn && !m_bBeamFlickerOff )
		LaserOn();
	else
		LaserOff();
}

void C_NPC_Hover_Turret::LaserOff( void )
{
	for ( int i = 0; i < 4; i++ )
	{
		if( m_pBeam[i] )
		{
			m_pBeam[i]->AddEffects( EF_NODRAW );
		}
	}
}

void C_NPC_Hover_Turret::LaserOn( void )
{
	if ( !IsBoneAccessAllowed() )
	{
		LaserOff();
		return;
	}

	Vector vecBody;
	QAngle angBodyDir;
	GetAttachment( HOVER_TURRET_PORTAL_LASER_ATTACHMENT, vecBody, angBodyDir );

	for ( int i = 0; i < 4; i++ )
	{
		Vector vecMuzzle;
		QAngle angMuzzleDir;
		GetAttachment( HOVER_TURRET_PORTAL_LASER_ATTACHMENT, vecMuzzle, angMuzzleDir );

		Vector vecEye;
		QAngle angEyeDir;
		GetAttachment( HOVER_TURRET_PORTAL_EYE_ATTACHMENT, vecEye, angEyeDir );

		Vector vecMuzzleDir;
		Vector vecMuzzleRight;
		Vector vecMuzzleUp;

		AngleVectors( GetAbsAngles(), &vecMuzzleDir, &vecMuzzleRight, &vecMuzzleUp );

		if ( i < 2 )
			vecMuzzle += 10.f*vecMuzzleRight;
		else
			vecMuzzle += -10.f*vecMuzzleRight;
		
		if ( i == 0 || i == 3 )
			vecMuzzle += 10.f*vecMuzzleUp;
		else
			vecMuzzle += -10.f*vecMuzzleUp;		

		vecMuzzle += 14.f*vecMuzzleDir;

		float spreadRatio = 0.5f*( gpGlobals->curtime - m_flAimStartTime );
		if( spreadRatio > 1.0f )
			spreadRatio = 1.0f;
		
		// as our aim dials in point more towards the player.
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if( pPlayer )
		{
			Vector vecToPlayer = pPlayer->EyePosition() - vecMuzzle;
			vecToPlayer.NormalizeInPlace();
			vecMuzzleDir = spreadRatio*vecToPlayer + (1.0f - spreadRatio)*vecMuzzleDir;
		}

		//void VectorRotate( const Vector &in1, const Quaternion &in2, Vector &out );
		Quaternion q;
		AxisAngleQuaternion( vecMuzzleDir, 200.f*( gpGlobals->curtime - m_flAimStartTime ) + 90.f*i, q );
		
		Vector laserDir = vecMuzzleDir + pow((1.0 - spreadRatio),2.5)*(1.5f*vecMuzzleUp + 1.5f*vecMuzzleRight);
		laserDir.NormalizeInPlace();
		VectorRotate( laserDir, q, vecMuzzleDir );

		if (!m_pBeam[i])
		{
			m_pBeam[i] = CBeam::BeamCreate( "effects/bluelaser1.vmt", 3.0f );
			m_pBeam[i]->SetColor( 30, 30, 255 );
			m_pBeam[i]->SetBrightness( 255 );
			m_pBeam[i]->SetNoise( 0 );
			m_pBeam[i]->SetWidth( 3.0f );
			m_pBeam[i]->SetEndWidth( 0 );
			m_pBeam[i]->SetScrollRate( 0 );
			m_pBeam[i]->SetFadeLength( 0 );
			m_pBeam[i]->SetHaloTexture( m_sLaserHaloSprite );
			m_pBeam[i]->SetHaloScale( 4.0f );
			m_pBeam[i]->SetCollisionGroup( COLLISION_GROUP_NONE );
			m_pBeam[i]->PointsInit( vecMuzzle + vecMuzzleDir, vecMuzzle );
			m_pBeam[i]->SetBeamFlag( FBEAM_REVERSED );
			m_pBeam[i]->SetStartEntity( this );
		}
		else
		{
			m_pBeam[i]->RemoveEffects( EF_NODRAW );
		}

		// Trace to find an endpoint
		Vector vEndPoint;
		float fEndFraction;
		Ray_t rayPath;

//		Vector vStartPoint = /*spreadRatio*vecBody + (1.0f - spreadRatio)**/vecMuzzle;

		rayPath.Init( vecMuzzle, vecMuzzle + vecMuzzleDir * HOVER_TURRET_PORTAL_LASER_RANGE );

		CTraceFilterSkipClassname traceFilter( this, "prop_energy_ball", COLLISION_GROUP_NONE );

		if ( UTIL_Portal_TraceRay_Beam( rayPath, MASK_SHOT, &traceFilter, &fEndFraction ) )
			vEndPoint = vecMuzzle + vecMuzzleDir * HOVER_TURRET_PORTAL_LASER_RANGE;	// Trace went through portal and endpoint is unknown
		else
			vEndPoint = vecMuzzle + vecMuzzleDir * HOVER_TURRET_PORTAL_LASER_RANGE * fEndFraction;	// Trace hit a wall

		// The beam is backwards, sort of. The endpoint is the sniper. This is
		// so that the beam can be tapered to very thin where it emits from the turret.
		m_pBeam[i]->PointsInit( vEndPoint, vecMuzzle );

		m_pBeam[i]->SetHaloScale( LaserEndPointSize() );
	}
}

float C_NPC_Hover_Turret::LaserEndPointSize( void )
{
	return 5.0f;//( ( max( 0.0f, sinf( gpGlobals->curtime * M_PI + m_fPulseOffset ) ) ) * FLOOR_TURRET_PORTAL_END_POINT_PULSE_SCALE + 3.0f ) * ( IsX360() ? ( 3.0f ) : ( 1.5f ) );
}