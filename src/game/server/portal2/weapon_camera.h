//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Camera
//
//=====================================================================================//

#ifndef WEAPON_CAMERA_H
#define WEAPON_CAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatweapon_shared.h"
#include "env_dof_controller.h"
//-----------------------------------------------------------------------------
// CWeaponCamera
//-----------------------------------------------------------------------------

class CWeaponCamera : public CBaseCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponCamera, CBaseCombatWeapon );

	CWeaponCamera( void );
	~CWeaponCamera( void );

	DECLARE_SERVERCLASS();

	virtual void	Precache( void );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual bool	Deploy( void );
	virtual void	OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual	void	OnMouseWheel( int nDirection );
	virtual int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual int		GetMinBurst() { return 1; }
	virtual int		GetMaxBurst() { return 1; }
	virtual float	GetFireRate( void ) { return 3.0f; }
	virtual bool	HasAnyAmmo( void ) { return true; }
	virtual void	ItemPostFrame( void );
	virtual bool	Holster( CBaseCombatWeapon *pNextWeapon );
	virtual bool	CanScaleCapturedObjects( void ) const { return m_bCanScaleCapturedObjects; }

	virtual void	InputSetNumCaptureSlots( inputdata_t& input );
	virtual void	SetNumCaptureSlots( int iNumCaptureSlots );
	virtual void	InputSetZoomAbility( inputdata_t& input );
	virtual void	SetZoomAbility( bool bCanZoom );
	virtual void	InputSetScaleAbility( inputdata_t& input );
	virtual void	SetScaleAbility( bool bCanScale );
	
	// Our first time picking up the camera causes an "admire" animation to kick off
	virtual Activity GetDrawActivity( void ) 
	{ 
		/*
		// FIXME: Ultimatley we'll want to do something like this for the player, but it's annoying for development for now
		if ( m_bFirstPresentation ) 
		{ 
			m_bFirstPresentation = false; 
			return ACT_VM_DEPLOY;
		}
		*/
		
		return BaseClass::GetDrawActivity();
	}

	virtual Activity GetPrimaryAttackActivity( void ) { return ACT_VM_PRIMARYATTACK; }

	DECLARE_ACTTABLE();

private:
	// Aesthetics
	void	CaptureEffect( const Vector &vecPosition );
	// void	SaveEntityConnections( CBaseEntity *pTarget, CaptureInfo_t &info );

	void	UpdateLocators( void );
	void	UpdateDOF( bool bImmediate = false );
		
	// Object handling
	void	CaptureObject( CBaseEntity *pObject );

	CBaseEntity *CameraTraceHull( const Vector& vecStart, const Vector& vecEnd, const Vector& vecMins, const Vector& vecMaxs, trace_t* pTrace );
	CBaseEntity *FindFirstCapturableObject( const Vector &vecOrigin, const Vector &vecDir, const Vector& vecSweptBoxMins, const Vector& vecSweptBoxMaxs );

	CHandle<CEnvDOFController>	m_hDOFController;
	float						m_flTargetDist;
	float						m_flDOFDist;
	float						m_flTargetBlur;
	float						m_flDOFBlur;
	float						m_flDOFRadius;
	float						m_flTargetRadius;

	int							m_CurIndex;
	bool						m_bInViewfinder;
	bool						m_bCanZoom;
	bool						m_bCanScaleCapturedObjects;
	int							m_nNumCaptureSlots;

	bool						m_bFirstPresentation;	// Whether this is the first time we've been presented to the player
};

// 
class CTraceFilterEitherOfTwoCollisionGroups : public CTraceFilter
{
public:

	CTraceFilterEitherOfTwoCollisionGroups( const IHandleEntity *passentity, int collisionGroup1, int collisionGroup2 ):
	m_colGroup1( collisionGroup1 ),
	m_colGroup2 ( collisionGroup2 ),
	m_pPassEnt( passentity )
	{}

	virtual bool ShouldHitEntity( IHandleEntity* passentity, int contentsMask )
	{
		if ( !StandardFilterRules( passentity, contentsMask ) )
			return false;

		if ( m_pPassEnt )
		{
			if ( !PassServerEntityFilter( passentity, m_pPassEnt ) )
			{
				return false;
			}
		}

		CBaseEntity *pEntity = EntityFromEntityHandle( passentity );
		if ( !pEntity )
			return false;
		if ( !pEntity->ShouldCollide( m_colGroup1, contentsMask ) && 
			 !pEntity->ShouldCollide( m_colGroup2, contentsMask ) )
			return false;
		if ( !g_pGameRules->ShouldCollide( m_colGroup1, pEntity->GetCollisionGroup() ) && 
			 !g_pGameRules->ShouldCollide( m_colGroup2, pEntity->GetCollisionGroup() ) )
			return false;

		return true;
	}

private:
	int m_colGroup1;
	int m_colGroup2;
	const IHandleEntity* m_pPassEnt;

};

bool UTIL_ObjectMayBeCaptured( CBaseEntity *pObject );

#endif //WEAPON_CAMERA_H