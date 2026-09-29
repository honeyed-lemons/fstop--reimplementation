//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
//  Purpose: One of the two ends of a portal pair which can be picked up and placed by weapon_camera
//
//===========================================================================//

#include "cbase.h"
#include "prop_portal.h"
#include "vcollide_parse.h"
#include "eventqueue.h"

#include "datacache/imdlcache.h"	// for the con command below, it precaches models for dynamic created doors

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PORTAL_LINKED_DOOR_MODEL_NAME "models/props/portaldoor.mdl"
#define PORTAL_LINKED_DOOR_RESTING_SURFACE_TRACE_DIST 1.5f


class CPropPortalLinkedDoor : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropPortalLinkedDoor, CBaseAnimating );
	DECLARE_DATADESC();

	CPropPortalLinkedDoor( void );

	virtual void UpdateOnRemove( void );
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool CreateVPhysics( void );
	virtual void Activate( void );
	
	void AnimateThink( void );
	void DisableLinkageThink( void );

	inline bool IsPortal2( void ) { return m_bIsPortal2; }
	inline CProp_Portal* GetPortal( void ) { return m_hPortal.Get(); }
	
private:
	// Creation/removal of internal members 
	void Init( void );
	void Destroy( void );

	void Open( CBaseEntity *pActivator );
	void Close( CBaseEntity *pActivator );

	void OnFullyOpened( void );
	void OnFullyClosed( void );

	// Prevents/allows portal functionality
	void DisableLinkage( void );
	void EnableLinkage( void );

	void PinPhysics( void );

	// Tests placement validity rules
	bool CheckRestingSurface( void );

	void SetPartner( CPropPortalLinkedDoor *pPartner );
	void SetPartnerByName( string_t iszentityname );
	
	void InputSetPartner( inputdata_t &input );
	void InputOpen( inputdata_t &input );
	void InputClose( inputdata_t &input );

	string_t						m_iszPartnerName;			// name of our linked CPropPortalLinkedDoor partner
	bool							m_bIsOnValidSurface;		// If all four corners and center are on a valid wall. Will reject linkage attempts if this is false.
	bool							m_bIsLinkedToPartner;		// if true, door links to partner and does not move physically
	bool							m_bIsInitialized;			// has found a partner ent and created it's member portal
	bool							m_bIsPortal2;				// Portal's color, it is required that a '1' portal be linked to a '2'.
	CHandle<CProp_Portal>			m_hPortal;					// Actual portal
	CHandle<CPropPortalLinkedDoor>	m_hPartner;					// Other door entity we link to
};

LINK_ENTITY_TO_CLASS( prop_portal_linked_door, CPropPortalLinkedDoor );

BEGIN_DATADESC( CPropPortalLinkedDoor )
	
	DEFINE_INPUTFUNC( FIELD_STRING, "SetPartner", InputSetPartner ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Open", InputOpen ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Close", InputClose ),

	DEFINE_KEYFIELD( m_iszPartnerName, FIELD_STRING, "partnername" ),
	DEFINE_KEYFIELD( m_bIsPortal2, FIELD_BOOLEAN, "IsPortal2" ),

	DEFINE_FIELD( m_bIsOnValidSurface, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsLinkedToPartner, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsInitialized, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPartner, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( DisableLinkageThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropPortalLinkedDoor::CPropPortalLinkedDoor( void ) : 
m_hPartner( NULL ), 
m_hPortal( NULL ),  
m_iszPartnerName( NULL_STRING ),
m_bIsLinkedToPartner( false ),
m_bIsInitialized( false ),
m_bIsPortal2( false ),
m_bIsOnValidSurface( false )
{
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::UpdateOnRemove( void )
{
	Destroy();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::Precache( void )
{
	PrecacheModel( PORTAL_LINKED_DOOR_MODEL_NAME );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );

	// HACK: Player gets stuck in this model a lot, no need for collision really at this time.
	// if the model changes we might want to re address this.
	SetModel( PORTAL_LINKED_DOOR_MODEL_NAME );
	CreateVPhysics();
	Init();

	// See if we spawned on something placeable
	CheckRestingSurface();
}


bool CPropPortalLinkedDoor::CreateVPhysics( void )
{
	// Create our normal physical bounds
	IPhysicsObject* pObj = VPhysicsInitStatic(  );
	if ( pObj )
		return true;
	
	Assert( 0 );
	return false;
}


void CPropPortalLinkedDoor::Activate( void )
{
	BaseClass::Activate();

	SetThink( &CPropPortalLinkedDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( CheckRestingSurface() )
	{
		// EnableLinkage();
		PinPhysics();
	}	

	// Start closed
	ResetSequence( 0 );

	// Link to our initial partner (if any)
	SetPartnerByName( m_iszPartnerName );
}

void CPropPortalLinkedDoor::Init( void )
{
	Assert( !m_bIsInitialized && !m_hPartner.Get() && !m_hPortal.Get() );

	// create and spawn without a linkage group (we'll fix up later)
	m_hPortal = (CProp_Portal*)CreateEntityByName( "prop_portal" );
	Assert( m_hPortal );
	if ( m_hPortal )
	{
		//m_hPortal->ChangeLinkageGroup( PORTAL_LINKAGE_GROUP_INVALID );
		m_hPortal->m_bIsPortal2 = m_bIsPortal2;
		m_hPortal->SetOwnerEntity( this );
		DispatchSpawn( m_hPortal );
		m_hPortal->Fizzle();
	}
	
	m_bIsInitialized = true;
}

void CPropPortalLinkedDoor::Destroy( void )
{
	DisableLinkage();

	m_hPartner = NULL;

	m_bIsInitialized = false;

	if ( m_hPortal.Get() )
	{
		UTIL_Remove( m_hPortal );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Disables portal functionality.
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::DisableLinkage( void )
{
	if ( m_hPortal )
	{
		m_hPortal->Fizzle();
	}

	m_bIsLinkedToPartner = false;
}

//-----------------------------------------------------------------------------
// Purpose: Enables portal functionality
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::EnableLinkage( void )
{
	Assert( m_hPortal );

	if ( m_hPartner )
	{
		Assert( m_hPartner->GetEntityName() == m_iszPartnerName );
	}
	else if ( m_iszPartnerName != NULL_STRING )
	{
		SetPartnerByName( m_iszPartnerName );
	}
	else
	{
		// Is this valid?
		Assert( 0 );
	}

	if ( m_hPartner && m_hPartner->GetPortal() )
	{
		Assert( m_bIsPortal2 ? (!m_hPartner->IsPortal2()) : (m_hPartner->IsPortal2()) );

		// Make sure this portal and the partner's portal share the same linkage id
		unsigned char iPortalID = m_hPortal->GetLinkageGroup();
		unsigned char iPartnerID = m_hPartner->GetPortal()->GetLinkageGroup();
		//unsigned char iLinkageID = PORTAL_LINKAGE_GROUP_INVALID;
		//// Update linkage id to match if they don't match, or if both are invalid
		//if ( (iPortalID != iPartnerID) || 
		//	 (iPortalID == PORTAL_LINKAGE_GROUP_INVALID) )
		//{
		//	// choose one thats valid, as long as both portals agree
		//	iLinkageID = (iPortalID != PORTAL_LINKAGE_GROUP_INVALID) ? (iPortalID) : (iPartnerID);

		//	// Neither was valid, get an unused one and assign it to both
		//	if ( iLinkageID == PORTAL_LINKAGE_GROUP_INVALID )
		//	{
		//		iLinkageID = UTIL_GetUnusedLinkageID();
		//	}

		//	// Assuming the above worked..
		//	Assert( iLinkageID != PORTAL_LINKAGE_GROUP_INVALID );

		//	m_hPortal->ChangeLinkageGroup( iLinkageID );
		//	m_hPartner->GetPortal()->ChangeLinkageGroup( iLinkageID );
		//}
		
		if ( !m_hPortal->IsActivedAndLinked() )
		{
			Vector vecForward, vecUp;
			AngleVectors( GetAbsAngles(), &vecForward, NULL, &vecUp );
			Vector vecPortalPos = GetAbsOrigin() - ( vecForward * 12.0f ) + ( vecUp * 48.0f );
			
			trace_t tr;
			UTIL_TraceLine( vecPortalPos, vecPortalPos - ( vecForward * 24.0f ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			// m_hPortal->Resize( 56, 50 ); // FIXME: Need to get these dimensions from somewhere else!
			//m_hPortal->Resize( 78, 82 ); // FIXME: Need to get these dimensions from somewhere else!
			m_hPortal->m_bActivated = true;
			m_hPortal->PlacePortal( tr.endpos, GetAbsAngles(), 1.0f, true );
			m_hPortal->SetContextThink( &CProp_Portal::DelayedPlacementThink, gpGlobals->curtime, s_pDelayedPlacementContext ); 
			m_hPortal->m_vDelayedPosition = tr.endpos;
		}		
		m_bIsLinkedToPartner = true;
	}
}

bool CPropPortalLinkedDoor::CheckRestingSurface( void )
{
	return true;

	/*
	// Run placement rules
	Vector vUp, vRight, vForward;
	GetVectors( &vForward, &vRight, &vUp );
	Vector vTraceOrigins[5];
	vTraceOrigins[0] = GetLocalOrigin();
	
	for ( int i = 1; i < 5; ++i )
	{
		vTraceOrigins[i] = vTraceOrigins[0];
		vTraceOrigins[i] += vRight * (((i-1) & (1<<0))?(32):(-32));
		vTraceOrigins[i] += vUp * (((i-1) & (1<<1))?(56):(-56));
	}

	// Trace all four corners and center and make sure they're all placed on solid
	bool bIsValid = true;
	for ( int i = 0; i < 5; ++i )
	{
		trace_t tr;
		UTIL_TraceLine( vTraceOrigins[i], vTraceOrigins[i] - vForward * PORTAL_LINKED_DOOR_RESTING_SURFACE_TRACE_DIST, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		if ( !tr.DidHit() || tr.IsDispSurface() || (tr.surface.flags & SURF_SKY) )
		{
			bIsValid = false;
			break;
		}
	}

	// record success or failure of the last polled surface check
	m_bIsOnValidSurface = bIsValid;

	return bIsValid;
	*/
}

void CPropPortalLinkedDoor::SetPartner( CPropPortalLinkedDoor *pPartner )
{
	// Don't bother if we've done this already
	if ( pPartner == m_hPartner )
		return;

	if ( pPartner == NULL )
	{
		DisableLinkage();
		return;
	}
	else if ( m_hPartner != pPartner )
	{
		// TODO: Unlink from our current partner!
		DisableLinkage();
	}

	m_iszPartnerName = pPartner->GetEntityName();
	m_hPartner = pPartner;

	// This must be reciprocal!
	m_hPartner->SetPartner( this );
}

void CPropPortalLinkedDoor::SetPartnerByName( string_t iszentityname )
{
	m_iszPartnerName = iszentityname;
	CBaseEntity* pEnt = gEntList.FindEntityByName( NULL, STRING(m_iszPartnerName), this, NULL, NULL, NULL );
	Assert( pEnt && FClassnameIs( pEnt, "prop_portal_linked_door" ) );
	SetPartner( dynamic_cast<CPropPortalLinkedDoor*>(pEnt) );
}

void CPropPortalLinkedDoor::InputSetPartner( inputdata_t &input )
{
	SetPartnerByName( input.value.StringID() );
}

void CPropPortalLinkedDoor::PinPhysics( void )
{
	IPhysicsObject* pObj = VPhysicsGetObject();
	Assert( pObj );

	if ( pObj )
	{
		pObj->EnableMotion( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Animate and catch edge cases for us stopping / starting our animation
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::AnimateThink( void )
{
	// Update our animation
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	if ( IsSequenceFinished() )
	{
		int nSequence = GetSequence();
		int nOpenSequence = LookupSequence( "open" );
		int nCloseSequence = LookupSequence( "close" );
		if ( nSequence == nOpenSequence )
		{
			int nIdleSequence = LookupSequence( "idleopen" );
			ResetSequence( nIdleSequence );
		
			OnFullyOpened();
		}
		else if ( nSequence == nCloseSequence )
		{
			int nIdleSequence = LookupSequence( "idleclose" );
			ResetSequence( nIdleSequence );

			OnFullyClosed();
		}
	}

	SetThink( &CPropPortalLinkedDoor::AnimateThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Open the door
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::Open( CBaseEntity *pActivator )
{
	int nSequence = GetSequence();
	int nTargetSequence = LookupSequence( "open" );
	if ( nSequence != nTargetSequence )
	{
		ResetSequence( nTargetSequence );
		SetPlaybackRate( 0.75f );

		// Get our partner to open
		m_hPartner->Open( pActivator );

		// Open the portal between the spaces
		EnableLinkage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Open the door2
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::InputOpen( inputdata_t &input )
{
	Open( input.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::Close( CBaseEntity *pActivator )
{
	int nSequence = GetSequence();
	int nTargetSequence = LookupSequence( "close" );
	if ( nSequence != nTargetSequence )
	{
		ResetSequence( nTargetSequence );
		SetPlaybackRate( 0.75f );

		// Get our partner to open
		m_hPartner->Close( pActivator );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::InputClose( inputdata_t &input )
{
	Close( input.pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::OnFullyOpened( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Close the door
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::OnFullyClosed( void )
{
	SetContextThink( &CPropPortalLinkedDoor::DisableLinkageThink, gpGlobals->curtime + 0.5f, "DisableLinkageThink" );
}

//-----------------------------------------------------------------------------
// Purpose: Close the portal down
//-----------------------------------------------------------------------------
void CPropPortalLinkedDoor::DisableLinkageThink( void )
{
	DisableLinkage();
}
