//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for player companions
//
//=============================================================================//

#ifndef	NPC_COMPANIONBOT_H
#define	NPC_COMPANIONBOT_H

//--------------------------------------------------------------------------
// OBJECTS
//--------------------------------------------------------------------------
typedef enum
{
	CBOT_DEFENSIVE=0,
	CBOT_OFFENSIVE,

	CBOT_LAST,
} CompanionBotRole_t;

#include "npc_playercompanion.h"
#include "npc_companionbot_upgrades.h"
#include "weapon_targetgun.h"

struct SquadCandidate_t;
typedef CBaseEntity CBaseHat;

//-----------------------------------------------------------------------------
//
// CLASS: CNPC_CompanionBot
//
//-----------------------------------------------------------------------------

//-------------------------------------
// Spawnflags
//-------------------------------------

#define SF_CITIZEN_FOLLOW			( 1 << 16 )	//65536 follow the player as soon as I spawn.
#define	SF_CITIZEN_MEDIC			( 1 << 17 )	//131072
#define SF_CITIZEN_AMMORESUPPLIER	( 1 << 19 )	//524288
#define SF_CITIZEN_NOT_COMMANDABLE	( 1 << 20 ) //1048576
#define SF_CITIZEN_IGNORE_SEMAPHORE ( 1 << 21 ) //2097152		Work outside the speech semaphore system
#define SF_CITIZEN_USE_RENDER_BOUNDS ( 1 << 24 )//16777216

//-------------------------------------

class CNPC_CompanionBot : public CNPC_PlayerCompanion
{
	DECLARE_CLASS( CNPC_CompanionBot, CNPC_PlayerCompanion );
public:

	// DECLARE_SERVERCLASS();

	CNPC_CompanionBot();

	//---------------------------------
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &set );
	virtual void	Precache();
	virtual void	Spawn();
	virtual void	PostNPCInit();
	virtual void	Activate();
	virtual void	OnRestore();
	
	// Camera interactions
	virtual void	OnCaptured( void );
	virtual void	OnReleased( void );

	//---------------------------------
	virtual Class_T Classify();
	virtual bool 	ShouldAlwaysThink();

	//---------------------------------
	// Health accessors
	//---------------------------------
	virtual void	TakeDamage( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );

	//---------------------------------
	// Behavior
	//---------------------------------
	virtual void	PredictPlayerPush();
	virtual void	TestPlayerPushing( CBaseEntity *pPlayer );
	virtual void 	GatherConditions();
	virtual void 	PrescheduleThink();
	virtual void	BuildScheduleTestBits();

	virtual int		SelectSchedule();
	virtual int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	int				SelectScheduleRetrieveItem();
	virtual int 	TranslateSchedule( int scheduleType );

	virtual void 	StartTask( const Task_t *pTask );
	virtual void 	RunTask( const Task_t *pTask );
	
	virtual Activity	NPC_TranslateActivity( Activity eNewActivity );
	virtual void 		HandleAnimEvent( animevent_t *pEvent );
	virtual void		TaskFail( AI_TaskFailureCode_t code );

	bool			ShouldLookForHat();
	CBaseEntity		*FindHat( const Vector &vecPosition, const Vector &range );
	void			PickupHat( CBaseHat *pItem );
	void 			SimpleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	bool			IgnorePlayerPushing( void );

	virtual int		DrawDebugTextOverlays( void );
	virtual void	Event_Killed( const CTakeDamageInfo &info );

	//---------------------------------
	// Speech
	//---------------------------------
	virtual bool	SelectIdleSpeech( AISpeechSelection_t *pSelection );
	void			TrackEntitySeen( CBaseEntity *pEntity );
	bool			SpeakIfAllowed( AIConcept_t concept, const char *modifiers = NULL, bool bRespondingToPlayer = false, char *pszOutResponseChosen = NULL, size_t bufsize = 0 );
	bool			SpeakIfAllowed( AIConceptHandle_t handle, const char *modifiers = NULL, bool bRespondingToPlayer = false, char *pszOutResponseChosen = NULL, size_t bufsize = 0 );
	bool			IsAllowedToSpeak( AIConceptHandle_t handle, bool bRespondingToPlayer = false );
	void			ForceNextLookTarget( CBaseEntity *pEntity );
	void			SetConversationTopic( CBaseEntity *pEntity );
	void			SetLastSpeaker( CBaseEntity *pEntity );
	virtual bool	IsOkToSpeak( ConceptCategory_t category, bool fRespondingToPlayer = false );
	virtual CAI_Expresser * CreateExpresser(); 
	CNPC_CompanionBot *		GetOtherBot(); // HACKHACK: get the robot that isn't me of the player has him
	virtual bool	IsValidSpeechTarget( int flags, CBaseEntity *pEntity );
	bool			SpeakForce(const char *ResponseConcept, bool bCancelScene = false);
	bool			UseSemaphore( void );


	//---------------------------------
	// Script
	//---------------------------------

	// Start the specifics of an scene event
	virtual bool		StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	virtual void		RemoveChoreoScene( CChoreoScene *scene, bool canceled = false );

	//---------------------------------
	// Commander mode
	//---------------------------------
	virtual bool	CanJoinPlayerSquad();
	bool			WasInPlayerSquad();
	void 			CommanderUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	AddToPlayerSquad();
	virtual void	RemoveFromPlayerSquad();
	void 			TogglePlayerSquadState();
	void			UpdatePlayerSquad();
	void 			FixupPlayerSquad();
	void 			ClearFollowTarget();
	void			SetFollowTarget( CBaseEntity *pLeader );
	CAI_BaseNPC *	GetSquadCommandRepresentative();
	void			SetSquad( CAI_Squad *pSquad );

	//---------------------------------
	// Commander mode, bot specific
	//---------------------------------
	void			OnAllyKilled( CBaseEntity *pDoomedBot );

	//---------------------------------
	// Hints
	//---------------------------------
	virtual bool	PickTacticalLookTarget( AILookTargetArgs_t *pArgs );
	virtual void	OnSelectedLookTarget( AILookTargetArgs_t *pArgs );
	virtual bool	FValidateHintType( CAI_Hint *pHint );

	//---------------------------------
	// Inputs
	//---------------------------------
	void			InputRemoveFromPlayerSquad( inputdata_t &inputdata ) { RemoveFromPlayerSquad(); }
	void			InputSetLooktarget( inputdata_t &inputdata );
	void			InputSetCommandable( inputdata_t &inputdata );
	void			InputSpeakIdleResponse( inputdata_t &inputdata );
	
	//---------------------------------
	//	Sounds 
	//---------------------------------
	void			FearSound( void );
	void			DeathSound( const CTakeDamageInfo &info );


	//---------------------------------
	//	Bot Types
	//---------------------------------
	CNetworkVarForDerived( int, m_iBotType );
	virtual int		GetBotType( void ) const { return 1; }
	virtual char	*GetBotString( void ) const { return "CompanionBot"; }
	virtual bool	IsCompanionBot( void ) const { return true; }
	static CNPC_CompanionBot *GetBotInstance( CompanionBotRole_t role ); // returns the bot of the given type, or NULL if none exists.
	
protected:
	// targeting
	virtual void		SetLastInteraction( int type );
	CBaseEntity			*FindNamedEntity( const char *pszName, IEntityFindFilter *pFilter );

public:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		COND_HAT_AVAILABLE = BaseClass::NEXT_CONDITION,
		COND_COMPANION_OFF_GROUND,
		COND_COMPANION_HIT_GROUND,
		NEXT_CONDITION,
		
		SCHED_GET_HAT = BaseClass::NEXT_SCHEDULE,
		SCHED_COMPANION_FALL,
		SCHED_COMPANION_HIT_GROUND,
		NEXT_SCHEDULE,
		
		TASK_DROP_HAT = BaseClass::NEXT_TASK,
		TASK_COMPANION_IMPACT_EFFECT,
		NEXT_TASK,
	};

	//-----------------------------------------------------
	// Player/Companion Interactions
	//-----------------------------------------------------
	enum
	{
		INTERACT_NONE,
		INTERACT_PLANT,
		INTERACT_UNPLANT,
		INTERACT_HEAL,
		INTERACT_COMMAND,
		INTERACT_WRENCH_HIT,
		INTERACT_GIVEAMMO,
	};

	//-----------------------------------------------------
	// Personality combinations
	//-----------------------------------------------------
	enum
	{
		PERSONALITY_OPTIMISM,
		PERSONALITY_AGGRESSION,
		PERSONALITY_INTELLIGENCE,
		PERSONALITY_COUNT,
	};

	int	m_nPersonalityValues[PERSONALITY_COUNT];

	//-----------------------------------------------------

protected:
	
	int				m_nInspectActivity;
	float			m_flNextFearSoundTime;
	string_t		m_iszOriginalSquad;
	float			m_flTimeJoinedPlayerSquad;
	bool			m_bWasInPlayerSquad;
	float			m_flTimeLastCloseToPlayer;
	EHANDLE			m_hLookTarget;
	EHANDLE			m_hForcedLookTarget;
	EHANDLE			m_hConversationTopic;
	EHANDLE			m_hLastSpeaker;
	CAI_Hint		*m_pSelectedHint;
	bool			m_bOverrideLooktargetCvar;

	// Camera interactions
	float			m_flReleasedTime; // Duration of time to carry the "recently released from camera" concept

private:
	CSimpleSimTimer	m_AutoSummonTimer;
	Vector			m_vAutoSummonAnchor;
	int				m_iHead;
	float			m_fCombatStartTime;
	float			m_fCombatEndTime;
	float			m_fTotalCombatTime;
	float			m_fLastInteractionTime;
	int				m_iLastInteraction;


	//-----------------------------------------------------
	// hat system
	//-----------------------------------------------------
	EHANDLE			m_hHat;
	unsigned short  m_nHats; // number of hats worn.
public:
	inline unsigned short  GetNumHats() { return m_nHats; }
private:

	static CSimpleSimTimer gm_PlayerSquadEvaluateTimer;


	//-----------------------------------------------------
	//	Outputs
	//-----------------------------------------------------
	COutputEvent		m_OnJoinedPlayerSquad;
	COutputEvent		m_OnLeftPlayerSquad;
	COutputEvent		m_OnFollowOrder;
	COutputEvent		m_OnStationOrder; 
	COutputEvent		m_OnPlayerUse;
	COutputEvent		m_OnNavFailBlocked;
protected:
	COutputEvent		m_OnPlanted;
	COutputEvent		m_OnUnplanted;
private:
	COutputInt			m_iHealthOutput;

	//-----------------------------------------------------

	CHandle<CAI_FollowGoal>	m_hSavedFollowGoalEnt;

	bool					m_bNotifyNavFailBlocked;
	bool					m_bNeverLeavePlayerSquad; // Don't leave the player squad unless killed, or removed via Entity I/O. 

protected:
	Vector					m_vecHeardSound;
	bool					m_bHasHeardSound;

	float					m_flNextAcknowledgeTime;	// Next time an antlion can make an acknowledgement noise
	float					m_flObeyFollowTime;			// A range of time the antlions must be obedient
	float					m_flNextHatSearchTime;

	float					m_flTimePlayerStare;	// The game time at which the player started staring at me.
	float					m_flTimeNextAmmoStare;	// Next time I'm allowed to give ammo to a player who is staring at me.
	
public:
	EHANDLE			m_hTargetingDevice; // pointer to the last targeting device I knew
	COutputEHANDLE	m_OnTargetingDevice;  /// trigger when targeting device changes
	COutputEvent	m_OnTargetingDeviceCancelled;  /// trigger when targeting device is removed

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

#endif	//NPC_COMPANIONBOT_H
