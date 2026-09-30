//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The downtrodden citizens of City 17.
//
//=============================================================================//

#include "cbase.h"

#include "weapon_rpg.h"
#include "hl2_player.h"
#include "eventqueue.h"
 
#include "ai_squad.h"
#include "ai_pathfinder.h"
#include "ai_route.h"
#include "ai_basenpc.h"
#include "sceneentity.h"
#include "gib.h"
#include "ammodef.h"
#include "npc_companionbot.h"
#include "weapon_targetgun.h"
#include "tier1/utlbuffer.h"
#include "choreoevent.h"
#include "sceneentity_class.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TLK_COMPANION_CAPTURED "TLK_COMPANION_CAPTURED"
#define TLK_COMPANION_RELEASED "TLK_COMPANION_RELEASED"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

ConVar	eoz_allow_respeak( "eoz_allow_respeak", "0", 0, "Allow bots to comment multiple times on the same entity" );
ConVar	eoz_hat_search_delay( "eoz_hat_search_delay", "15", 0, "Number of seconds between hat searches" );
ConVar	eoz_hat_search( "eoz_hat_search", "1", 0, "Allow the mbot to search for hats" );
ConVar	eoz_allow_looktargets( "eoz_allow_looktargets", "1", 0, "Allow bots to pick looktargets" );

ConVar	bot_released_from_camera_context_period("bot_released_from_camera_context_period", "2", 0, "Number of seconds to carry the 'recently release from camera' context" );

const int MAX_PLAYER_SQUAD = 4;
#define	CBOT_OBEY_FOLLOW_TIME	6.0f

ConVar	sk_companionbot_health				( "sk_companionbot_health",					"200");
ConVar	npc_companionbot_auto_player_squad( "npc_companionbot_auto_player_squad", "1" );
ConVar	npc_companionbot_auto_player_squad_allow_use( "npc_companionbot_auto_player_squad_allow_use", "0" );

ConVar	g_ai_bot_show_enemy( "g_ai_bot_show_enemy", "0" );

#define ShouldAutosquad() (npc_companionbot_auto_player_squad.GetBool())

ConVar	ai_bot_debug_commander( "ai_citizen_debug_commander", "1" );
#define DebuggingCommanderMode() (ai_bot_debug_commander.GetBool() && (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))

//-----------------------------------------------------------------------------
// Citizen expressions for the citizen expression types
//-----------------------------------------------------------------------------
#define STATES_WITH_EXPRESSIONS		3		// Idle, Alert, Combat
#define EXPRESSIONS_PER_STATE		1
struct model_t;
struct bot_expression_list_t
{
	char *szExpressions[EXPRESSIONS_PER_STATE];
};
// Scared
bot_expression_list_t ScaredExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_scared_idle_01.vcd" },
	{ "scenes/Expressions/citizen_scared_alert_01.vcd" },
	{ "scenes/Expressions/citizen_scared_combat_01.vcd" },
};
// Normal
bot_expression_list_t NormalExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_normal_idle_01.vcd" },
	{ "scenes/Expressions/citizen_normal_alert_01.vcd" },
	{ "scenes/Expressions/citizen_normal_combat_01.vcd" },
};
// Angry
bot_expression_list_t AngryExpressions[STATES_WITH_EXPRESSIONS] =
{
	{ "scenes/Expressions/citizen_angry_idle_01.vcd" },
	{ "scenes/Expressions/citizen_angry_alert_01.vcd" },
	{ "scenes/Expressions/citizen_angry_combat_01.vcd" },
};

//-----------------------------------------------------------------------------
// Override the expressor just for these bots.
//-----------------------------------------------------------------------------
class CAI_ExpresserWithFollowup : public CAI_Expresser
{
public:
	CAI_ExpresserWithFollowup( CBaseFlex *pOuter = NULL ) : CAI_Expresser(pOuter) {};
	virtual bool Speak( AIConcept_t concept, const char *modifiers = NULL, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL );
	virtual bool Speak( AIConceptHandle_t handle, const char *modifiers = NULL, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL );
	virtual bool SpeakDispatchResponse( AIConcept_t concept, AI_Response *response, IRecipientFilter *filter = NULL );
	virtual bool SpeakDispatchResponse( AIConceptHandle_t handle, AI_Response *response, IRecipientFilter *filter = NULL );
	virtual void SpeakDispatchFollowup( EOZ_Hacks::ResponseFollowup &followup );
	virtual void SpeakDispatchListen( EOZ_Hacks::ResponseFollowup &followup );
private:
};

CAI_Expresser *CNPC_CompanionBot::CreateExpresser()
{
	return (new CAI_ExpresserWithFollowup(this));
}

// This is a tiny helper function for below -- what I'd use a lambda for, usually
static void DispatchComeback( CAI_ExpresserWithFollowup *pExpress, CNPC_CompanionBot *pSpeaker, CNPC_CompanionBot *pRespondent, EOZ_Hacks::ResponseFollowup &followup, AIConceptHandle_t handle )
{
	AssertMsg(pSpeaker != NULL, "Response expressor somehow got called with a NULL Outer.\n");
	if ( !pRespondent )
	{
		return;
	}

// 	if ( pRespondent->m_bPlanted )
// 		return;

//	const EOZ_Hacks::ResponseFollowup &followup = response->GetParams()->m_followup;
	float duration = 0.f;//pExpress->GetSemaphoreAvailableTime(pSpeaker) - gpGlobals->curtime;
	float delay = followup.followup_delay;
	if (pSpeaker == pRespondent && delay < 0)
	{
		Warning("Response rule with an 'isay' specified negative delay, which isn't legal because that would make someone talk over himself.");
		delay = 0;
	}
	
//	Msg( "%s: Dispatch comeback about %s to %s\n", pSpeaker->GetBotString(), g_pConceptManager->GetTopicName( handle ), pRespondent->GetBotString() );

	// build an input event that we will use to force the bot to talk through the IO system
	variant_t value;
	// Don't send along null contexts
	if (followup.followup_contexts && followup.followup_contexts[0] != '\0')
	{
		value.SetString( MAKE_STRING( followup.followup_contexts ) );
		g_EventQueue.AddEvent( pRespondent, "AddContext", value, duration + delay - 0.01, pSpeaker, pSpeaker );
	}

	g_pConceptManager->SetConcept( handle, followup.followup_concept );
	g_pConceptManager->SetSpeaker( handle, pSpeaker );
	value.SetInt( (int)handle );
	g_EventQueue.AddEvent( pRespondent, "SpeakConversationResponseConcept", value, duration + delay , pSpeaker, pSpeaker );
}

// When the speaker's vcd is finished, the idler will then play the idle scene
static void DispatchIdleScene( CAI_ExpresserWithFollowup *pExpress, CNPC_CompanionBot *pSpeaker, CNPC_CompanionBot *pRespondent, CNPC_CompanionBot *pIdler, AIConceptHandle_t handle )
{
	if ( !pRespondent )
		return;

	float duration = 0.f;//pExpress->GetSemaphoreAvailableTime(pSpeaker) - gpGlobals->curtime;

//	Msg( "%s: Dispatch idle scene to %s\n", pSpeaker->GetBotString(), pIdler->GetBotString() );

	// build an input event that we will use to force the bot to talk through the IO system
	variant_t value;
	AIConceptHandle_t idleHandle = g_pConceptManager->CreateConcept( "TLK_LISTEN", g_pConceptManager->GetTopic( handle ), pRespondent, true );

	value.SetInt( (int)idleHandle );
	g_EventQueue.AddEvent( pIdler, "SpeakConversationListenConcept", value, duration , pSpeaker, pSpeaker );
}

//-----------------------------------------------------------------------------
// Purpose: Placeholder for rules based response system
// Input  : concept - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_ExpresserWithFollowup::Speak( AIConcept_t concept, const char *modifiers /*= NULL*/, char *pszOutResponseChosen /* = NULL*/, size_t bufsize /* = 0 */, IRecipientFilter *filter /* = NULL */ )
{
	AIConceptHandle_t handle = g_pConceptManager->CreateConcept( concept );
	bool bRet = Speak( handle, modifiers, pszOutResponseChosen, bufsize, filter );
	if ( !bRet )
	{
		g_pConceptManager->DestroyConcept( handle );
	}
	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Handles new concept objects
//-----------------------------------------------------------------------------
bool CAI_ExpresserWithFollowup::Speak( AIConceptHandle_t handle, const char *modifiers /*= NULL*/, char *pszOutResponseChosen /* = NULL*/, size_t bufsize /* = 0 */, IRecipientFilter *filter /* = NULL */ )
{
	AI_Response *result = SpeakFindResponse( handle, modifiers );
	if ( !result )
	{
		return false;
	}

	CNPC_CompanionBot *pBot = dynamic_cast<CNPC_CompanionBot *>(GetOuter());
	if ( pBot )
	{
		pBot->SetConversationTopic( g_pConceptManager->GetTopic( handle ) );
		pBot->SetLastSpeaker( g_pConceptManager->GetSpeaker( handle ) );
//		Msg( "%s: Conversing about %s\n", pBot->GetBotString(), g_pConceptManager->GetTopicName( handle ) );
	}

	SpeechMsg( GetOuter(), "%s (%x) spoke %s (%f)\n", STRING(GetOuter()->GetEntityName()), GetOuter(), g_pConceptManager->GetConcept( handle ), gpGlobals->curtime );

	bool spoke = SpeakDispatchResponse( handle, result, filter );
	if ( pszOutResponseChosen )
	{
		result->GetResponse( pszOutResponseChosen, bufsize );
	}

	return spoke;
}

//-----------------------------------------------------------------------------
// Purpose: Dispatches the result
// Input  : *response - 
//-----------------------------------------------------------------------------
bool CAI_ExpresserWithFollowup::SpeakDispatchResponse( AIConcept_t concept, AI_Response *response, IRecipientFilter *filter )
{
	AIConceptHandle_t handle = g_pConceptManager->CreateConcept( concept );
	bool bRet = SpeakDispatchResponse( handle, response, filter );
	if ( !bRet )
	{
		g_pConceptManager->DestroyConcept( handle );
	}
	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Handles the new concept objects
//-----------------------------------------------------------------------------
bool CAI_ExpresserWithFollowup::SpeakDispatchResponse( AIConceptHandle_t handle, AI_Response *response, IRecipientFilter *filter )
{
	// This gives the chance for the other bot to respond.
	bool bSuc = CAI_Expresser::SpeakDispatchResponse( handle, response, filter );
	if (!bSuc)
	{
		return false;
	}

	const EOZ_Hacks::ResponseFollowup &followup = response->GetParams()->m_followup;
	if ( !followup.IsValid() )
	{
//		g_pConceptManager->DestroyConcept( handle );
	}

	return bSuc;
}

//-----------------------------------------------------------------------------
// Purpose: Handles the new concept objects
//-----------------------------------------------------------------------------
void CAI_ExpresserWithFollowup::SpeakDispatchFollowup( EOZ_Hacks::ResponseFollowup &followup )
{
//	Assert(response && response->GetParams());
//	const EOZ_Hacks::ResponseFollowup &followup = response->GetParams()->m_followup;
	CNPC_CompanionBot *pBot = dynamic_cast<CNPC_CompanionBot *>(GetOuter());
	if ( followup.IsValid() )
	{
		if ( pBot )
		{
			// figure out who to respond to -- me, him, or both of us
			switch (followup.followup_target)
			{
			case EOZ_Hacks::ResponseFollowup::kYOU: // then you say
				DispatchComeback( this, pBot, pBot->GetOtherBot(), followup, followup.hConcept );
//				DispatchIdleScene( this, pBot, pBot->GetOtherBot(), pBot, followup.hConcept );
				break;
			case EOZ_Hacks::ResponseFollowup::kBOTHOFUS: // then we say
				DispatchComeback( this, pBot, pBot->GetOtherBot(), followup, followup.hConcept ); // (falls through)
			case EOZ_Hacks::ResponseFollowup::kMYSELF: // then I say
				DispatchComeback( this, pBot, pBot, followup, followup.hConcept );
				break;
			}
		}
	}
	else
	{
		g_pConceptManager->DestroyConcept( followup.hConcept );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the new concept objects
//-----------------------------------------------------------------------------
void CAI_ExpresserWithFollowup::SpeakDispatchListen( EOZ_Hacks::ResponseFollowup &followup )
{
	//	Assert(response && response->GetParams());
	//	const EOZ_Hacks::ResponseFollowup &followup = response->GetParams()->m_followup;
	CNPC_CompanionBot *pBot = dynamic_cast<CNPC_CompanionBot *>(GetOuter());
	if ( followup.IsValid() )
	{
		if ( pBot )
		{
			// figure out who to respond to -- me, him, or both of us
			switch (followup.followup_target)
			{
			case EOZ_Hacks::ResponseFollowup::kYOU: // then you say
//				DispatchComeback( this, pBot, pBot->GetOtherBot(), followup, followup.hConcept );
				DispatchIdleScene( this, pBot, pBot->GetOtherBot(), pBot, followup.hConcept );
				break;
// 			case EOZ_Hacks::ResponseFollowup::kBOTHOFUS: // then we say
// 				DispatchComeback( this, pBot, pBot->GetOtherBot(), followup, followup.hConcept ); // (falls through)
// 			case EOZ_Hacks::ResponseFollowup::kMYSELF: // then I say
// 				DispatchComeback( this, pBot, pBot, followup, followup.hConcept );
// 				break;
			}
		}
	}
	else
	{
		g_pConceptManager->DestroyConcept( followup.hConcept );
	}
}

//---------------------------------------------------------
// activities
//---------------------------------------------------------

int ACT_BOT_HEAL;
int ACT_BOT_PLANTED_HEAL;
int ACT_BOT_BUILD;

//---------------------------------------------------------

BEGIN_DATADESC( CNPC_CompanionBot )

	DEFINE_CUSTOM_FIELD( m_nInspectActivity,		ActivityDataOps() ),
	DEFINE_EMBEDDED(	m_AutoSummonTimer ),
	DEFINE_FIELD( 		m_flNextFearSoundTime, 		FIELD_TIME ),
	DEFINE_FIELD( 		m_iszOriginalSquad, 		FIELD_STRING ),
	DEFINE_FIELD( 		m_flTimeJoinedPlayerSquad,	FIELD_TIME ),
	DEFINE_FIELD( 		m_bWasInPlayerSquad,		FIELD_BOOLEAN ),
	DEFINE_FIELD( 		m_flTimeLastCloseToPlayer,	FIELD_TIME ),
	DEFINE_FIELD(		m_flNextHatSearchTime,		FIELD_TIME ),

	// Camera interactions
	DEFINE_FIELD(		m_flReleasedTime,			FIELD_TIME ),

	DEFINE_FIELD(		m_flTimePlayerStare,		FIELD_TIME ),

	DEFINE_EMBEDDED(	m_AutoSummonTimer ),
	DEFINE_FIELD(		m_vAutoSummonAnchor, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD(		m_iHead,					FIELD_INTEGER ),
	DEFINE_FIELD( 		m_hSavedFollowGoalEnt,		FIELD_EHANDLE ),
	DEFINE_FIELD( 		m_hTargetingDevice,			FIELD_EHANDLE ),

	DEFINE_KEYFIELD(	m_bNotifyNavFailBlocked,	FIELD_BOOLEAN, "notifynavfailblocked" ),
	DEFINE_KEYFIELD(	m_bNeverLeavePlayerSquad,	FIELD_BOOLEAN, "neverleaveplayersquad" ),

	DEFINE_OUTPUT(		m_OnJoinedPlayerSquad,	"OnJoinedPlayerSquad" ),
	DEFINE_OUTPUT(		m_OnLeftPlayerSquad,	"OnLeftPlayerSquad" ),
	DEFINE_OUTPUT(		m_OnFollowOrder,		"OnFollowOrder" ),
	DEFINE_OUTPUT(		m_OnStationOrder,		"OnStationOrder" ),
	DEFINE_OUTPUT(		m_OnPlayerUse,			"OnPlayerUse" ),
	DEFINE_OUTPUT(		m_OnNavFailBlocked,		"OnNavFailBlocked" ),
	DEFINE_OUTPUT(		m_OnPlanted,			"OnPlanted" ),
	DEFINE_OUTPUT(		m_OnUnplanted,			"OnUnplanted" ),
	DEFINE_OUTPUT(		m_iHealthOutput,		"OutHealth"),

	DEFINE_INPUTFUNC( FIELD_VOID,	"RemoveFromPlayerSquad",	InputRemoveFromPlayerSquad ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetLooktarget",			InputSetLooktarget ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"SetCommandable",			InputSetCommandable ),

	DEFINE_FIELD( m_vecHeardSound,				FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bHasHeardSound,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextAcknowledgeTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flObeyFollowTime,			FIELD_TIME ),
	DEFINE_FIELD( m_nHats,						FIELD_SHORT ),

	DEFINE_USEFUNC( CommanderUse ),
	DEFINE_USEFUNC( SimpleUse ),

END_DATADESC()

//---------------------------------------------------------
// FIXME-JDW:
/*
	IMPLEMENT_SERVERCLASS_ST(CNPC_CompanionBot, DT_NPC_CompanionBot)

	SendPropInt( SENDINFO( m_iHealth ) ),
	SendPropInt( SENDINFO( m_iMaxHealth ) ),
	SendPropInt( SENDINFO( m_iBotType ) ),
	SendPropBool( SENDINFO( m_bPlanted ) ),
	SendPropInt( SENDINFO( m_iStoredHealth ) ),
	SendPropInt( SENDINFO( m_iMaxStoredHealth ) ),
	SendPropInt( SENDINFO( m_iDefaultMaxHealth ) ),
	//m_nMetalCollected

	END_SEND_TABLE()
*/

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CSimpleSimTimer CNPC_CompanionBot::gm_PlayerSquadEvaluateTimer;

//---------------------------------------------------------
// anim events
//---------------------------------------------------------
int AE_SV_HAT_FLYOFF_FWD;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CNPC_CompanionBot::CNPC_CompanionBot() : m_flObeyFollowTime( 0.0f ), m_flNextAcknowledgeTime( 0.0f )
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::Precache()
{
	PrecacheModel( STRING( GetModelName() ) );
	//gibs and such
	PrecacheModel( "companionbot/gibs/gibArmL.mdl" );
	PrecacheModel( "companionbot/gibs/gibArmR.mdl" );

	PrecacheScriptSound( "NPC_Citizen.FootstepLeft" );
	PrecacheScriptSound( "NPC_Citizen.FootstepRight" );
	PrecacheScriptSound( "NPC_Citizen.Die" );

	PrecacheInstancedScene( "scenes/Expressions/CitizenIdle.vcd" );
	PrecacheInstancedScene( "scenes/Expressions/CitizenAlert_loop.vcd" );
	PrecacheInstancedScene( "scenes/Expressions/CitizenCombat_loop.vcd" );

	PrecacheParticleSystem( "medicgun_beam_blue" );
	PrecacheParticleSystem( "medicgun_beam_green" );
	PrecacheParticleSystem( "bot_damage_02" );
	PrecacheParticleSystem( "zombie_stomp_heavy" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::Spawn( void )
{
	BaseClass::Spawn();

	if ( ShouldAutosquad() )
	{
		if ( m_SquadName == GetPlayerSquadName() )
		{
			CAI_Squad *pPlayerSquad = g_AI_SquadManager.FindSquad( GetPlayerSquadName() );
			if ( pPlayerSquad && pPlayerSquad->NumMembers() >= MAX_PLAYER_SQUAD )
				m_SquadName = NULL_STRING;
		}
		gm_PlayerSquadEvaluateTimer.Force();
	}
	
	m_bOverrideLooktargetCvar = false;
	m_pSelectedHint = NULL;
	m_hHat = NULL;

	m_iHealth = sk_companionbot_health.GetFloat();

	m_iszOriginalSquad = m_SquadName;

	m_flTimePlayerStare = FLT_MAX;

	SetLastInteraction( INTERACT_NONE );

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	NPCInit();

	SetUse( &CNPC_CompanionBot::CommanderUse );
	Assert( !ShouldAutosquad() || !IsInPlayerSquad() );

	m_bWasInPlayerSquad = IsInPlayerSquad();

	// Use render bounds instead of human hull for guys sitting in chairs, etc.
	m_ActBusyBehavior.SetUseRenderBounds( HasSpawnFlags( SF_CITIZEN_USE_RENDER_BOUNDS ) );

	// Set personality values
	for ( int i = 0; i < PERSONALITY_COUNT; ++i )
	{
		m_nPersonalityValues[i] = random->RandomInt( 0, 100 );
	}

	// SetCollisionGroup( HL2COLLISION_GROUP_COMPANION_BOT );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::PostNPCInit()
{
	if ( IsInPlayerSquad() )
	{
		if ( m_pSquad->NumMembers() > MAX_PLAYER_SQUAD )
			DevMsg( "Error: Spawning citizen in player squad but exceeds squad limit of %d members\n", MAX_PLAYER_SQUAD );

		FixupPlayerSquad();
	}
	else
	{
		if ( ( m_spawnflags & SF_CITIZEN_FOLLOW ) && AI_IsSinglePlayer() )
		{
			SetFollowTarget( UTIL_GetLocalPlayer() );
			m_FollowBehavior.SetParameters( AIF_SIDEKICK );
		}
	}

	BaseClass::PostNPCInit();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::Activate()
{
	// Try to auto-join the player on activation
	AddToPlayerSquad();

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::OnRestore()
{
	gm_PlayerSquadEvaluateTimer.Force();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Class_T	CNPC_CompanionBot::Classify()
{
	// Possibly set bot type here...
	return CLASS_PLAYER_ALLY;
}

void CNPC_CompanionBot::TakeDamage( const CTakeDamageInfo &info )
{
	BaseClass::TakeDamage( info );
}

int CNPC_CompanionBot::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage_Alive( info );
}

void CNPC_CompanionBot::Event_Killed( const CTakeDamageInfo &info )
{
	CNPC_CompanionBot *pBuddy = GetOtherBot();
	if ( pBuddy )
	{
		pBuddy->OnAllyKilled( this );
	}

	if ( m_hHat )
	{
		// Drop the hat
		m_hHat->SetParent( NULL );
		m_hHat->RemoveSolidFlags( FSOLID_NOT_SOLID );
		m_hHat->VPhysicsGetObject()->RemoveShadowController();				
		m_hHat->SetMoveType( MOVETYPE_VPHYSICS );
		m_hHat->CollisionRulesChanged();
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SetLastInteraction( int type )
{
	m_iLastInteraction = type;
	m_fLastInteractionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::ShouldAlwaysThink() 
{ 
	return true; 
}
	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::PredictPlayerPush()
{
	if (!IgnorePlayerPushing())
		BaseClass::PredictPlayerPush();
}

void CNPC_CompanionBot::TestPlayerPushing( CBaseEntity *pPlayer )
{
	if (!IgnorePlayerPushing())
		return BaseClass::TestPlayerPushing( pPlayer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::SelectIdleSpeech( AISpeechSelection_t *pSelection )
{
	if ( BaseClass::SelectIdleSpeech( pSelection ) )
		return true;

// 	if ( !IsOkToSpeak( SPEECH_IDLE ) )
// 		return false;

	CBasePlayer *pTarget = assert_cast<CBasePlayer *>(FindSpeechTarget( AIST_PLAYERS | AIST_FACING_TARGET ));
	if ( pTarget )
	{
		if ( ShouldSpeakRandom( TLK_IDLE, 1 ) && SelectSpeechResponse( TLK_IDLE, NULL, pTarget, pSelection ) )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_CompanionBot::FindHat( const Vector &vecPosition, const Vector &range )
{
	/*
	CBaseEntity *list[1024];
	int count = UTIL_EntitiesInBox( list, 1024, vecPosition - range, vecPosition + range, 0 );

	for ( int i = 0; i < count; i++ )
	{
		CPhysicsProp *pItem = dynamic_cast< CPhysicsProp* >( list[ i ] );

		if( pItem && pItem->IsHatable() && pItem != m_hHat )
		{
			return pItem;
		}
	}
	*/

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::ShouldLookForHat()
{
	if( gpGlobals->curtime < m_flNextHatSearchTime )
		return false;

	if ( !eoz_hat_search.GetBool() )
		return false;

	if ( GetBotType() == CBOT_OFFENSIVE )
		return false;

	bool bRetval = true;
	AIConceptHandle_t handle = g_pConceptManager->CreateConcept( "TLK_WEARINGHAT", NULL, this );
	if ( !IsAllowedToSpeak( handle ) )
	{
		bRetval = false;
	}
	g_pConceptManager->DestroyConcept( handle );

	return bRetval;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::GatherConditions()
{
	if( ShouldLookForHat() )
	{
		if( FindHat( GetAbsOrigin(), Vector( 240, 240, 240 ) ) )
		{
			SetCondition( COND_HAT_AVAILABLE );
		}
		else
		{
			ClearCondition( COND_HAT_AVAILABLE );
		}

		m_flNextHatSearchTime = gpGlobals->curtime + eoz_hat_search_delay.GetInt();
	}

	// For off-ground handling
	if ( GetGroundEntity() == NULL )
	{
		SetCondition( COND_COMPANION_OFF_GROUND );
	}
	else
	{
		// If we were off the ground on the last frame, we were in the air!
		if ( HasCondition( COND_COMPANION_OFF_GROUND ) )
		{
			SetCondition( COND_COMPANION_HIT_GROUND );
		}

		ClearCondition( COND_COMPANION_OFF_GROUND );
	}

	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

 	// UpdatePlayerSquad();

	if( GetEnemy() && g_ai_bot_show_enemy.GetBool() )
	{
		NDebugOverlay::Line( EyePosition(), GetEnemy()->EyePosition(), 255, 0, 0, false, .1 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CompanionBot::SelectSchedule()
{
	if ( HasCondition( COND_COMPANION_OFF_GROUND ) )
		return SCHED_COMPANION_FALL;

	if ( HasCondition( COND_COMPANION_HIT_GROUND ) )
		return SCHED_COMPANION_HIT_GROUND;

	int schedule = SelectScheduleRetrieveItem();
	if ( schedule != SCHED_NONE )
		return schedule;

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CompanionBot::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CompanionBot::TranslateSchedule( int scheduleType ) 
{
	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CompanionBot::SelectScheduleRetrieveItem( void )
{
	if( HasCondition(COND_HAT_AVAILABLE) && m_FollowBehavior.GetFollowTarget() )
	{
		CBaseEntity *pBase = FindHat(m_FollowBehavior.GetFollowTarget()->GetAbsOrigin(), Vector( 120, 120, 120 ) );
		CPhysicsProp *pItem = dynamic_cast<CPhysicsProp *>( pBase );

		if( pItem )
		{
			SpeakIfAllowed( "TLK_FOUNDHAT" );
			SetTarget( pItem );
			return SCHED_GET_HAT;
		}
	}
	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::StartTask( const Task_t *pTask )
{

	switch( pTask->iTask )
	{
	// First step for hat bot
	case TASK_DROP_HAT:
		return;

	case TASK_COMPANION_IMPACT_EFFECT:
		{
			// Dust puff
			// DispatchParticleEffect( "zombie_stomp_heavy", GetAbsOrigin(), GetAbsOrigin(), GetAbsAngles() );
		}
		return;

	default:
		BaseClass::StartTask( pTask );
		return;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	// First step for hat bot
	case TASK_DROP_HAT:
		{
			if ( m_hHat )
			{
				// Drop the hat
				m_hHat->SetParent( NULL );
				m_hHat->RemoveSolidFlags( FSOLID_NOT_SOLID );
				m_hHat->VPhysicsGetObject()->RemoveShadowController();				
				m_hHat->SetMoveType( MOVETYPE_VPHYSICS );
				m_hHat->CollisionRulesChanged();
			}
			TaskComplete();
		}
		return;
	}

	BaseClass::RunTask( pTask );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_CompanionBot::PickupHat( CBaseHat *pItem )
{
	// Save the origin offset
	const CCollisionProperty *pCollide = pItem->CollisionProp();
	Vector min = pCollide->OBBMins();
	min.x = 0;
	min.y = 0;

	pItem->SetParent( this );
	pItem->SetParentAttachment( "input", "hat", true );
	pItem->AddSolidFlags( FSOLID_NOT_SOLID );
	pItem->SetMoveType( MOVETYPE_NONE );
	pItem->VPhysicsGetObject()->SetShadow( 1e4f, 1e4f, false, false );
	pItem->SetLocalOrigin( vec3_origin - min );
	pItem->SetLocalAngles( vec3_angle );
	pItem->CollisionRulesChanged();

	m_hHat = pItem;
	m_nHats++;

	SpeakIfAllowed( "TLK_WEARINGHAT" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::TaskFail( AI_TaskFailureCode_t code )
{
	if( code == FAIL_NO_ROUTE_BLOCKED && m_bNotifyNavFailBlocked )
	{
		m_OnNavFailBlocked.FireOutput( this, this );
	}

	BaseClass::TaskFail( code );
}

//-----------------------------------------------------------------------------
// Purpose: Override base class activities
//-----------------------------------------------------------------------------
Activity CNPC_CompanionBot::NPC_TranslateActivity( Activity activity )
{
	return BaseClass::NPC_TranslateActivity( activity );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_CompanionBot::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == NPC_EVENT_ITEM_PICKUP )
	{
		// Fill in something here?
	}
	
	if( pEvent->event == AE_SV_HAT_FLYOFF_FWD )
	{
		//knock hat off this npc, have it fly fwd and up a little
		if ( m_hHat )
		{
			// Drop the hat
			m_hHat->SetParent( NULL );
			m_hHat->RemoveSolidFlags( FSOLID_NOT_SOLID );
			m_hHat->VPhysicsGetObject()->RemoveShadowController();				
			m_hHat->SetMoveType( MOVETYPE_VPHYSICS );
			m_hHat->CollisionRulesChanged();
			//jake: i'm not figuring out the correct vector/force for max coolness, just doing whatever this does.  help yourself
			m_hHat->ApplyAbsVelocityImpulse( HeadDirection3D() * 100 );
			m_hHat->ApplyLocalAngularVelocityImpulse( RandomAngularImpulse( 250, 360 ) );
			
			//m_hHat->ApplyAbsVelocityImpulse( )
		}
		return;
	}
	
	switch( pEvent->event )
	{
	case NPC_EVENT_LEFTFOOT:
		{
			EmitSound( "NPC_Citizen.FootstepLeft", pEvent->eventtime );
		}
		break;

	case NPC_EVENT_RIGHTFOOT:
		{
			EmitSound( "NPC_Citizen.FootstepRight", pEvent->eventtime );
		}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::IgnorePlayerPushing( void )
{
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::SpeakIfAllowed( AIConcept_t concept, const char *modifiers, bool bRespondingToPlayer, char *pszOutResponseChosen, size_t bufsize ) 
{ 
	AIConceptHandle_t handle = g_pConceptManager->CreateConcept( concept );
	bool bRet = SpeakIfAllowed( handle, modifiers, bRespondingToPlayer, pszOutResponseChosen, bufsize );
	if ( !bRet )
	{
		g_pConceptManager->DestroyConcept( concept );
	}
	
	return bRet;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::SpeakIfAllowed( AIConceptHandle_t handle, const char *modifiers, bool bRespondingToPlayer, char *pszOutResponseChosen, size_t bufsize ) 
{ 
	if ( IsAllowedToSpeak( handle, bRespondingToPlayer ) )
		return Speak( handle, modifiers, pszOutResponseChosen, bufsize, NULL );

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::IsAllowedToSpeak( AIConceptHandle_t handle, bool bRespondingToPlayer ) 
{
	// HACK: Passing true bypasses the concept delays, which I haven't got a handle on yet
	return BaseClass::IsAllowedToSpeak( handle, true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::ForceNextLookTarget( CBaseEntity *pEntity )
{
	m_hForcedLookTarget = pEntity;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::InputSetLooktarget( inputdata_t &inputdata )
{
	string_t target = inputdata.value.StringID();
	CBaseEntity *pEntity = gEntList.FindEntityGeneric( NULL, STRING( target ), this, inputdata.pActivator );
	ForceNextLookTarget( pEntity );
}

#pragma warning(push)
#pragma warning(disable:4706)
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	Assert( info );
	Assert( info->m_pScene );
	Assert( info->m_pEvent );

	switch ( info->m_pEvent->GetType() )
	{
	case CChoreoEvent::GENERIC:
		const char *prams = event->GetParameters();
		// is this our new output-firing event?
		if ( V_stristr( prams, "ENT_FIRE" ) == prams )
		{
			// okay, tokenize this on spaces.
			char localParams[1024];
			V_strncpy(localParams,prams,1023); // need to copy as strtok stomps the string
			char *token = strtok(localParams," ");
			// token should be ENT_FIRE
			Assert( Q_stricmp(token,"ENT_FIRE") == 0 );
			// get name of object we are targeting
			token = strtok( NULL , " ");
			if (!token)
			{
				Warning("Generic scene event '%s' could not be parsed.\n", prams);
				return false;
			}
			CBaseEntity *pReferent = info->m_pSceneEntity->FindNamedEntity(token, this);
			if (!pReferent /*&& !(pReferent = FindNamedEntity(token, NULL))*/ )
			{
				Warning("Scene ENT_FIRE could not find entity %s\n", token);
				return false;
			}

			const char *action = "Use";
			variant_t value;
			int delay = 0;

			// get input name
			token = strtok( NULL , " ");
			if (token)
			{
				action = STRING( AllocPooledString(token) );

#if 1  // multiple parameter version
				// okay, now parse parameters. There are an arbirary number
				// of them and we need to tokenize them, test each as a procedural
				// name, then assemble them back into a string that we then pool
				// so it can be passed as entity io. Yuck.
				char ioParams[1024];
				int ioCharsWrit = 0;

				while ((token = strtok( NULL , " ")) && (ioCharsWrit < 1023) )
				{
					const char *parameter = token;
					// try parsing it as a procedural name
					if (token[0] == '!')
					{
						CBaseEntity *pProcedural = info->m_pSceneEntity->FindNamedEntity(token, this);
						if ( pProcedural )
						{
							parameter = pProcedural->GetEntityName().ToCStr()  ;
						}
					}
					ioCharsWrit += V_snprintf(ioParams + ioCharsWrit, 1024 - ioCharsWrit, "%s ", parameter ) ; // don't count the NULL at the end.
				}

				if (ioCharsWrit > 1)
				{
					// eat the final space. 
					ioParams[ioCharsWrit - 1] = '\0';
					value.SetString(AllocPooledString(ioParams));
				}
#else // one parameter only
				token = strtok( NULL , " ");
				if (token)
				{
					const char *parameter = token;
					// try parsing it as a procedural name
					if (token[0] == '!')
					{
						CBaseEntity *pProcedural = info->m_pSceneEntity->FindNamedEntity(token, this);
						if ( pProcedural )
						{
							parameter = pProcedural->GetEntityName().ToCStr()  ;
						}
					}

					value.SetString(AllocPooledString(parameter));
				}
#endif
			}
			g_EventQueue.AddEvent( pReferent, action, value, delay, this, this );

			/*
			const char *target = "", *action = "Use";
			variant_t value;
			int delay = 0;

			target = STRING( AllocPooledString(command.Arg( 1 ) ) );

			// Don't allow them to run anything on a point_servercommand unless they're the host player. Otherwise they can ent_fire
			// and run any command on the server. Admittedly, they can only do the ent_fire if sv_cheats is on, but 
			// people complained about users resetting the rcon password if the server briefly turned on cheats like this:
			//    give point_servercommand
			//    ent_fire point_servercommand command "rcon_password mynewpassword"
			if ( gpGlobals->maxClients > 1 && V_stricmp( target, "point_servercommand" ) == 0 )
			{
			if ( engine->IsDedicatedServer() )
			return;

			CBasePlayer *pHostPlayer = UTIL_GetListenServerHost();
			if ( pPlayer != pHostPlayer )
			return;
			}

			if ( command.ArgC() >= 3 )
			{
			action = STRING( AllocPooledString(command.Arg( 2 )) );
			}
			if ( command.ArgC() >= 4 )
			{
			value.SetString( AllocPooledString(command.Arg( 3 )) );
			}
			if ( command.ArgC() >= 5 )
			{
			delay = atoi( command.Arg( 4 ) );
			}

			g_EventQueue.AddEvent( target, action, value, delay, pPlayer, pPlayer );
			*/


			return true;
		}
		break;
	}

	return BaseClass::StartSceneEvent( info, scene, event, actor, pTarget );
}
#pragma warning(pop)

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::RemoveChoreoScene( CChoreoScene *scene, bool canceled )
{
	// Cleanup conversation info
	SetConversationTopic( NULL );
	SetLastSpeaker( NULL );

	BaseClass::RemoveChoreoScene( scene, canceled );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SetConversationTopic( CBaseEntity *pEntity )
{
	m_hConversationTopic = pEntity;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SetLastSpeaker( CBaseEntity *pEntity )
{
	m_hLastSpeaker = pEntity;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::PickTacticalLookTarget( AILookTargetArgs_t *pArgs )
{
	if ( m_hForcedLookTarget )
	{
		pArgs->hTarget = m_hForcedLookTarget;
		m_hForcedLookTarget = NULL;
		m_bOverrideLooktargetCvar = true;
		return true;
	}

	if( HasCondition( COND_SEE_ENEMY ) )
	{
		// Don't bother. We're dealing with our enemy.
		return false;
	}

	// Use hint nodes
	CAI_Hint *pHint;
	CHintCriteria hintCriteria;

	// hintCriteria.AddHintType( HINT_WORLD_READABLE_TEXT );
	hintCriteria.SetFlag( bits_HINT_NODE_NEAREST | bits_HINT_NODE_VISIBLE | bits_HINT_NODE_IN_VIEWCONE | bits_HINT_NPC_IN_NODE_FOV );
	pHint = CAI_HintManager::FindHint( this, hintCriteria );

	if( pHint )
	{
		pArgs->hTarget = pHint;
		return true;
	}

	// See what the base class thinks.
	return BaseClass::PickTacticalLookTarget( pArgs );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static bool AlreadyCommentedOnEntity( CBaseEntity *pEntity )
{
	if ( eoz_allow_respeak.GetBool() )
		return false;

	int idx = pEntity->FindContextByName( "ignore" );
	if ( idx != -1 )
	{
		const char *pValue = pEntity->GetContextValue( idx );
		if ( 1 == atoi( pValue ) )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void InitializeEntitySeen( CBaseEntity *pEntity )
{
	int idx = pEntity->FindContextByName( "ObjectCommentedOn" );
	if ( idx == -1 )
	{
		pEntity->AddContext( "ObjectCommentedOn:0" );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::TrackEntitySeen( CBaseEntity *pEntity )
{
	int idx = pEntity->FindContextByName( "entity_generic_name" );
	if ( idx != -1 )
	{
		const char *pValue = pEntity->GetContextValue( idx );
		int plyrIdx = FindContextByName( pValue );
		if ( plyrIdx == -1 )
		{
			// Add this context to the bot's memory
			AddContext( UTIL_VarArgs( "%s:1", pValue ) );
		}
		else
		{
			// Increment the context in memory
			const char *pCount = GetContextValue( plyrIdx );
			int nCount = atoi( pCount );
			AddContext( UTIL_VarArgs( "%s:%d", pValue, (nCount + 1) ) );
		}
	}
	else
	{
		Warning( "Entity %s doesn't have a generic name!\n", pEntity->GetModelName() );
	}

	int nSeenCount = 1;
	idx = pEntity->FindContextByName( "ObjectCommentedOn" );
	if ( idx != -1 )
	{
		const char *pValue = pEntity->GetContextValue( idx );
		nSeenCount = atoi( pValue ) + 1;
	}
	pEntity->AddContext( UTIL_VarArgs( "ObjectCommentedOn:%d", nSeenCount ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::OnSelectedLookTarget( AILookTargetArgs_t *pArgs )
{ 
	if ( !eoz_allow_looktargets.GetBool() && !m_bOverrideLooktargetCvar )
		return;

	m_bOverrideLooktargetCvar = false;

	if ( pArgs && pArgs->hTarget )
	{
/*
		CAI_Hint *pHint = dynamic_cast<CAI_Hint *>(pArgs->hTarget.Get());
		if ( pHint / *&& pHint->HintType() == HINT_WORLD_READABLE_TEXT* / )
		{
			m_pSelectedHint = pHint;
			variant_t var;
			if ( pHint->ReadKeyField( "ResponseContext", &var ) ) 
			{
				if ( SpeakIfAllowed( "TLK_READSIGN", var.String() ) )
				{
					pHint->Lock( this );
					SetCondition(COND_IDLE_INTERRUPT);
				}
			}
		}
		else
		*/
		{
			m_hLookTarget = pArgs->hTarget.Get();
			if ( m_hLookTarget /*&& !AlreadyCommentedOnEntity( m_hLookTarget )*/ )
			{
				InitializeEntitySeen( m_hLookTarget );
				AIConceptHandle_t handle = g_pConceptManager->CreateConcept( "TLK_LOOKTARGET", m_hLookTarget, this );
				if ( SpeakIfAllowed( handle ) )
				{
					TrackEntitySeen( m_hLookTarget );
				}
				else
				{
					g_pConceptManager->DestroyConcept( handle );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::FValidateHintType( CAI_Hint *pHint )
{
	return BaseClass::FValidateHintType( pHint );
}

//-----------------------------------------------------------------------------
// Purpose: returns our selected hint node
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_CompanionBot::FindNamedEntity( const char *pszName, IEntityFindFilter *pFilter )
{
	if ( !stricmp( pszName, "!hint" ) )
	{
		return m_pSelectedHint;
	}
	else if ( !stricmp( pszName, "!looktarget" ) )
	{
		return m_hLookTarget;
	}
	else if ( !stricmp( pszName, "!topic" ) )
	{
		// Bot can't look on his own head, so look at the last speaker instead
		if ( m_hConversationTopic != m_hHat )
		{
			return m_hConversationTopic;
		}
		return m_hLastSpeaker;
	}
	else if ( !stricmp( pszName, "!lastspeaker" ) )
	{
		return m_hLastSpeaker;
	}

	return BaseClass::FindNamedEntity( pszName, pFilter );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SimpleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Under these conditions, citizens will refuse to go with the player.
	// Robin: NPCs should always respond to +USE even if someone else has the semaphore.
	m_bDontUseSemaphore = true;

	// First, try to speak the +USE concept
	if ( !SelectPlayerUseSpeech() )
	{
		if ( HasSpawnFlags(SF_CITIZEN_NOT_COMMANDABLE) || IRelationType( pActivator ) == D_NU )
		{
			// If I'm denying commander mode because a level designer has made that decision,
			// then fire this output in case they've hooked it to an event.
			m_OnDenyCommanderUse.FireOutput( this, this );
		}
	}

	m_bDontUseSemaphore = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::CanJoinPlayerSquad()
{
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::WasInPlayerSquad()
{
	return m_bWasInPlayerSquad;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::OnAllyKilled( CBaseEntity *pDoomedBot )
{
	AssertMsg1( pDoomedBot != this, "%s notified itself of its own death?!\n", GetDebugName() );
}

//-----------------------------------------------------------------------------
// Purpose: Tack on extra criteria for responses
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

#ifdef DBGFLAG_ASSERT
	int botType = GetBotType();
	AssertMsg2(botType == CBOT_OFFENSIVE || botType == CBOT_DEFENSIVE, "Bot %s is unknown GetBotType() %d\n", GetDebugName(), botType );
#endif
	float fLengthOfLastCombat = 0.f;
	float fTotalCombatLength = m_fTotalCombatTime;
	if ( GetState() == NPC_STATE_COMBAT )
	{
		fLengthOfLastCombat = gpGlobals->curtime - m_fCombatStartTime;
		fTotalCombatLength += fLengthOfLastCombat;
	}
	else
	{
		fLengthOfLastCombat = m_fCombatEndTime - m_fCombatStartTime;
	}

	// some criteria are explicit to specific bots regardless of which one we're matching
	CNPC_CompanionBot *pMbot, *pObot;
	pMbot = CNPC_CompanionBot::GetBotInstance(CBOT_DEFENSIVE);
	pObot = CNPC_CompanionBot::GetBotInstance(CBOT_OFFENSIVE);

	set.AppendCriteria( "combat_length", UTIL_VarArgs( "%.3f", fLengthOfLastCombat ) );
	set.AppendCriteria( "combat_total", UTIL_VarArgs( "%.3f", fTotalCombatLength ) );

	set.AppendCriteria( "personality_optimism", UTIL_VarArgs( "%d", m_nPersonalityValues[PERSONALITY_OPTIMISM] ) );
	set.AppendCriteria( "personality_aggression", UTIL_VarArgs( "%d", m_nPersonalityValues[PERSONALITY_AGGRESSION] ) );
	set.AppendCriteria( "personality_intelligence", UTIL_VarArgs( "%d", m_nPersonalityValues[PERSONALITY_INTELLIGENCE] ) );

	float fTimeSinceLastInteraction = gpGlobals->curtime - m_fLastInteractionTime;
	set.AppendCriteria( "timesincelastinteraction", UTIL_VarArgs( "%.3f", fTimeSinceLastInteraction ) );

	// Number of hats
	if ( pMbot )
	{
		set.AppendCriteria( "num_hats_mbot_total", UTIL_VarArgs( "%d", pMbot->GetNumHats() ));
	}
	if ( pObot )
	{
		set.AppendCriteria( "num_hats_obot_total", UTIL_VarArgs( "%d", pObot->GetNumHats() ));
	}

	CNPC_CompanionBot *pOther = GetOtherBot();
	if ( pOther ) // if the other bot exists
	{
		set.AppendCriteria( "dist_to_other_bot", UTIL_VarArgs( "%.1f", pOther->GetAbsOrigin().DistTo(GetAbsOrigin())) );
		if ( FVisible(pOther) )
		{
			set.AppendCriteria( "other_bot_visible", "1" );
		}
		if ( HasCondition(COND_TOO_FAR_TO_ATTACK) )
		{
			set.AppendCriteria( "too_far_to_attack", "1" );
		}
	}

	// Add criteria from the current look target
	if ( m_hLookTarget )
	{
		bool bHasLookTarget = false;
		int count = m_hLookTarget->GetContextCount();
		if ( count )
		{
			for ( int i = 0; i < count; ++i )
			{
				const char *pName =  m_hLookTarget->GetContextName( i );
				const char *pValue = m_hLookTarget->GetContextValue( i );
				if (pName && pValue)
				{
					set.AppendCriteria( pName, pValue );
				}
			}

			// Can't talk about this item if it doesn't have a generic name!
			if ( m_hLookTarget->FindContextByName( "entity_generic_name" ) != -1 )
			{
				bHasLookTarget = true;
			}
		}
		
		set.AppendCriteria( "has_look_target", (bHasLookTarget ? "1" : "0") );
		if ( bHasLookTarget )
		{
			CBaseAnimating *pAnim = m_hLookTarget->GetBaseAnimating();
			if ( pAnim )
			{
				// Append the object size of the look target
				set.AppendCriteria( "look_target_size", UTIL_VarArgs( "%d", pAnim->GetObjectScaleLevel() ) );
			}
		}
	}

	AIEnemiesIter_t iter;
	int iNumEnemies = 0;
	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if ( pEMemory->hEnemy->IsAlive() && ( pEMemory->hEnemy->Classify() != CLASS_BULLSEYE ) )
		{
			iNumEnemies++;
		}
	}
	set.AppendCriteria( "num_enemies", UTIL_VarArgs( "%d", iNumEnemies ) );

	// Append size information
	set.AppendCriteria( "object_size", UTIL_VarArgs( "%d", GetObjectScaleLevel() ) );

	// If we were released recently, then append that context for comments
	if ( m_flReleasedTime > gpGlobals->curtime )
	{
		set.AppendCriteria( "camera_recently_released", "1" );
	}
}

//-----------------------------------------------------------------------------
// like CAI_PlayerAlly::IsValidSpeechTarget, but ignores visibility
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::IsValidSpeechTarget( int flags, CBaseEntity *pEntity )
{
	if ( pEntity == this )
		return false;

	if ( !(flags & AIST_IGNORE_RELATIONSHIP) )
	{
		if ( pEntity->IsPlayer() )
		{
			if ( !IsPlayerAlly( (CBasePlayer *)pEntity ) )
				return false;
		}
		else
		{
			if ( IRelationType( pEntity ) != D_LI )
				return false;
		}
	}		

	if ( !pEntity->IsAlive() )
		// don't dead people
		return false;

	// Ignore no-target entities
	if ( pEntity->GetFlags() & FL_NOTARGET )
		return false;

	CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
	if ( pNPC )
	{
		// If not a NPC for some reason, or in a script.
		if ( (pNPC->m_NPCState == NPC_STATE_SCRIPT || pNPC->m_NPCState == NPC_STATE_PRONE))
			return false;

		if ( pNPC->IsInAScript() )
			return false;

		// Don't bother people who don't want to be bothered
		if ( !pNPC->CanBeUsedAsAFriend() )
			return false;
	}

	if ( flags & AIST_FACING_TARGET )
	{
		if ( pEntity->IsPlayer() )
			return HasCondition( COND_SEE_PLAYER );
		else if ( !FInViewCone( pEntity ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::CommanderUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_OnPlayerUse.FireOutput( pActivator, pCaller );

	// Under these conditions, citizens will refuse to go with the player.
	// Robin: NPCs should always respond to +USE even if someone else has the semaphore.
	if ( !AI_IsSinglePlayer() || !CanJoinPlayerSquad() )
	{
		SimpleUse( pActivator, pCaller, useType, value );
		return;
	}
	
	if ( pActivator == UTIL_GetLocalPlayer() )
	{
		// Don't say hi after you've been addressed by the player
		SetSpokeConcept( TLK_HELLO, NULL );	

		if ( GetCurSchedule() && ConditionInterruptsCurSchedule( COND_IDLE_INTERRUPT ) )
		{
			if ( SpeakIfAllowed( TLK_QUESTION ) )
			{
				if ( random->RandomInt( 1, 4 ) < 4 )
				{
					CBaseEntity *pRespondant = FindSpeechTarget( AIST_NPCS );
					if ( pRespondant )
					{
						g_EventQueue.AddEvent( pRespondant, "SpeakIdleResponse", ( GetTimeSpeechComplete() - gpGlobals->curtime ) + .2, this, this );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::AddToPlayerSquad()
{
	Assert( !IsInPlayerSquad() );

	AddToSquad( AllocPooledString(PLAYER_SQUADNAME) );
	m_hSavedFollowGoalEnt = m_FollowBehavior.GetFollowGoal();
	m_FollowBehavior.SetFollowGoalDirect( NULL );

	FixupPlayerSquad();

	SetCondition( COND_PLAYER_ADDED_TO_SQUAD );

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	CHL2_Player *pHL2Player;
	pHL2Player = dynamic_cast<CHL2_Player*>(pPlayer);

	if ( pHL2Player )
	{
		// FIXME-JDW: pHL2Player->AddCompanionBotToSquad( this );
	}
	SpeakIfAllowed( "TLK_BUILD" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::RemoveFromPlayerSquad()
{
	Assert( IsInPlayerSquad() );

	ClearFollowTarget();

	if ( m_iszOriginalSquad != NULL_STRING && strcmp( STRING( m_iszOriginalSquad ), PLAYER_SQUADNAME ) != 0 )
		AddToSquad( m_iszOriginalSquad );
	else
		RemoveFromSquad();
	
	if ( m_hSavedFollowGoalEnt )
		m_FollowBehavior.SetFollowGoal( m_hSavedFollowGoalEnt );

	SetCondition( COND_PLAYER_REMOVED_FROM_SQUAD );

	// Don't evaluate the player squad for 2 seconds. 
	gm_PlayerSquadEvaluateTimer.Set( 2.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Deal with being captured by the camera
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::OnCaptured( void )
{
	RemoveFromPlayerSquad();

	// Let the other bot talk about this
	CNPC_CompanionBot *pOtherBot = GetOtherBot();
	if ( pOtherBot )
	{
		pOtherBot->SpeakIfAllowed( TLK_COMPANION_CAPTURED );
	}

	BaseClass::OnCaptured();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::OnReleased( void )
{
	m_flReleasedTime = gpGlobals->curtime + bot_released_from_camera_context_period.GetFloat();

	// Let the other bot talk about this
	CNPC_CompanionBot *pOtherBot = GetOtherBot();
	if ( pOtherBot )
	{
		pOtherBot->SpeakIfAllowed( TLK_COMPANION_RELEASED );
	}

	Activate();
	BaseClass::OnReleased();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::TogglePlayerSquadState()
{
	if ( !AI_IsSinglePlayer() )
		return;

	if ( !IsInPlayerSquad() )
	{
		AddToPlayerSquad();
	}
	else
	{
		RemoveFromPlayerSquad();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct SquadCandidate_t
{
	CNPC_CompanionBot *pCitizen;
	bool		  bIsInSquad;
	float		  distSq;
	int			  iSquadIndex;
};

void CNPC_CompanionBot::UpdatePlayerSquad()
{
	if ( !AI_IsSinglePlayer() )
		return;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( ( pPlayer->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D() ).LengthSqr() < Square(20*12) )
		m_flTimeLastCloseToPlayer = gpGlobals->curtime;

	if ( !gm_PlayerSquadEvaluateTimer.Expired() )
		return;

	gm_PlayerSquadEvaluateTimer.Set( 2.0 );

	// Remove stragglers
	CAI_Squad *pPlayerSquad = g_AI_SquadManager.FindSquad( MAKE_STRING( PLAYER_SQUADNAME ) );
	if ( pPlayerSquad )
	{
		CUtlVectorFixed<CNPC_CompanionBot *, MAX_PLAYER_SQUAD> squadMembersToRemove;
		AISquadIter_t iter;

		for ( CAI_BaseNPC *pPlayerSquadMember = pPlayerSquad->GetFirstMember(&iter); pPlayerSquadMember; pPlayerSquadMember = pPlayerSquad->GetNextMember(&iter) )
		{
			if ( pPlayerSquadMember->GetClassname() != GetClassname() )
				continue;

			CNPC_CompanionBot *pCitizen = assert_cast<CNPC_CompanionBot *>(pPlayerSquadMember);

			if ( !pCitizen->m_bNeverLeavePlayerSquad &&
				 pCitizen->m_FollowBehavior.GetFollowTarget() &&
				 !pCitizen->m_FollowBehavior.FollowTargetVisible() && 
				 pCitizen->m_FollowBehavior.GetNumFailedFollowAttempts() > 0 && 
				 gpGlobals->curtime - pCitizen->m_FollowBehavior.GetTimeFailFollowStarted() > 20 &&
				 ( fabsf(( pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().z - pCitizen->GetAbsOrigin().z )) > 196 ||
				   ( pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square(50*12) ) )
			{
				if ( DebuggingCommanderMode() )
				{
					DevMsg( "Player follower is lost (%d, %f, %d)\n", 
						 pCitizen->m_FollowBehavior.GetNumFailedFollowAttempts(), 
						 gpGlobals->curtime - pCitizen->m_FollowBehavior.GetTimeFailFollowStarted(), 
						 (int)((pCitizen->m_FollowBehavior.GetFollowTarget()->GetAbsOrigin().AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D() ).Length()) );
				}

				squadMembersToRemove.AddToTail( pCitizen );
			}
		}

		for ( int i = 0; i < squadMembersToRemove.Count(); i++ )
		{
			squadMembersToRemove[i]->RemoveFromPlayerSquad();
		}
	}

	// Autosquadding
	const float JOIN_PLAYER_XY_TOLERANCE_SQ = Square(36*12);
	const float UNCONDITIONAL_JOIN_PLAYER_XY_TOLERANCE_SQ = Square(12*12);
	const float UNCONDITIONAL_JOIN_PLAYER_Z_TOLERANCE = 5*12;
	const float SECOND_TIER_JOIN_DIST_SQ = Square(48*12);
	if ( pPlayer && ShouldAutosquad() && !(pPlayer->GetFlags() & FL_NOTARGET ) && pPlayer->IsAlive() )
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		CUtlVector<SquadCandidate_t> candidates;
		const Vector &vPlayerPos = pPlayer->GetAbsOrigin();
		bool bFoundNewGuy = false;
		int i;

		for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i]->GetState() == NPC_STATE_DEAD )
				continue;

			if ( ppAIs[i]->GetClassname() != GetClassname() )
				continue;

			CNPC_CompanionBot *pCitizen = assert_cast<CNPC_CompanionBot *>(ppAIs[i]);
			int iNew;

			if ( pCitizen->IsInPlayerSquad() )
			{
				iNew = candidates.AddToTail();
				candidates[iNew].pCitizen = pCitizen;
				candidates[iNew].bIsInSquad = true;
				candidates[iNew].distSq = 0;
				candidates[iNew].iSquadIndex = pCitizen->GetSquad()->GetSquadIndex( pCitizen );
			}
			else
			{
				float distSq = (vPlayerPos.AsVector2D() - pCitizen->GetAbsOrigin().AsVector2D()).LengthSqr(); 
				if ( distSq > JOIN_PLAYER_XY_TOLERANCE_SQ && 
					( pCitizen->m_flTimeJoinedPlayerSquad == 0 || gpGlobals->curtime - pCitizen->m_flTimeJoinedPlayerSquad > 60.0 ) && 
					( pCitizen->m_flTimeLastCloseToPlayer == 0 || gpGlobals->curtime - pCitizen->m_flTimeLastCloseToPlayer > 15.0 ) )
					continue;

				if ( !pCitizen->CanJoinPlayerSquad() )
					continue;

				bool bShouldAdd = false;

				if ( pCitizen->HasCondition( COND_SEE_PLAYER ) )
					bShouldAdd = true;
				else
				{
					bool bPlayerVisible = pCitizen->FVisible( pPlayer );
					if ( bPlayerVisible )
					{
						if ( pCitizen->HasCondition( COND_HEAR_PLAYER ) )
							bShouldAdd = true;
						else if ( distSq < UNCONDITIONAL_JOIN_PLAYER_XY_TOLERANCE_SQ && fabsf(vPlayerPos.z - pCitizen->GetAbsOrigin().z) < UNCONDITIONAL_JOIN_PLAYER_Z_TOLERANCE )
							bShouldAdd = true;
					}
				}

				if ( bShouldAdd )
				{
					// @TODO (toml 05-25-04): probably everyone in a squad should be a candidate if one of them sees the player
					AI_Waypoint_t *pPathToPlayer = pCitizen->GetPathfinder()->BuildRoute( pCitizen->GetAbsOrigin(), vPlayerPos, pPlayer, 5*12, NAV_NONE, bits_BUILD_GET_CLOSE );
					GetPathfinder()->UnlockRouteNodes( pPathToPlayer );

					if ( !pPathToPlayer )
						continue;

					CAI_Path tempPath;
					tempPath.SetWaypoints( pPathToPlayer ); // path object will delete waypoints

					iNew = candidates.AddToTail();
					candidates[iNew].pCitizen = pCitizen;
					candidates[iNew].bIsInSquad = false;
					candidates[iNew].distSq = distSq;
					candidates[iNew].iSquadIndex = -1;
					
					bFoundNewGuy = true;
				}
			}
		}
		
		if ( bFoundNewGuy )
		{
			// Look for second order guys
			int initialCount = candidates.Count();
			for ( i = 0; i < initialCount; i++ )
				candidates[i].pCitizen->AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ); // Prevents double-add
			for ( i = 0; i < initialCount; i++ )
			{
				if ( candidates[i].iSquadIndex == -1 )
				{
					for ( int j = 0; j < g_AI_Manager.NumAIs(); j++ )
					{
						if ( ppAIs[j]->GetState() == NPC_STATE_DEAD )
							continue;

						if ( ppAIs[j]->GetClassname() != GetClassname() )
							continue;

						if ( ppAIs[j]->HasSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ) )
							continue; 

						CNPC_CompanionBot *pCitizen = assert_cast<CNPC_CompanionBot *>(ppAIs[j]);

						float distSq = (vPlayerPos - pCitizen->GetAbsOrigin()).Length2DSqr(); 
						if ( distSq > JOIN_PLAYER_XY_TOLERANCE_SQ )
							continue;

						distSq = (candidates[i].pCitizen->GetAbsOrigin() - pCitizen->GetAbsOrigin()).Length2DSqr(); 
						if ( distSq > SECOND_TIER_JOIN_DIST_SQ )
							continue;

						if ( !pCitizen->CanJoinPlayerSquad() )
							continue;

						if ( !pCitizen->FVisible( pPlayer ) )
							continue;

						int iNew = candidates.AddToTail();
						candidates[iNew].pCitizen = pCitizen;
						candidates[iNew].bIsInSquad = false;
						candidates[iNew].distSq = distSq;
						candidates[iNew].iSquadIndex = -1;
						pCitizen->AddSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE ); // Prevents double-add
					}
				}
			}
			for ( i = 0; i < candidates.Count(); i++ )
				candidates[i].pCitizen->RemoveSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );

			if ( candidates.Count() )
			{
				CNPC_CompanionBot *pClosest = NULL;
				float closestDistSq = FLT_MAX;
				int nJoined = 0;

				for ( i = 0; i < candidates.Count() && i < MAX_PLAYER_SQUAD; i++ )
				{
					if ( !candidates[i].pCitizen->IsInPlayerSquad() )
					{
						candidates[i].pCitizen->AddToPlayerSquad();
						nJoined++;

						if ( candidates[i].distSq < closestDistSq )
						{
							pClosest = candidates[i].pCitizen;
							closestDistSq = candidates[i].distSq;
						}
					}
				}

				if ( pClosest )
				{
					if ( !pClosest->SpokeConcept( TLK_JOINPLAYER ) )
					{
						// pClosest->SpeakCommandResponse( TLK_JOINPLAYER, CFmtStr( "numjoining:%d", nJoined ) );
					}
					else
					{
						// pClosest->SpeakCommandResponse( TLK_STARTFOLLOW );
					}

					for ( i = 0; i < candidates.Count() && i < MAX_PLAYER_SQUAD; i++ )
					{
						candidates[i].pCitizen->SetSpokeConcept( TLK_JOINPLAYER, NULL ); 
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::FixupPlayerSquad()
{
	if ( !AI_IsSinglePlayer() )
		return;

	m_flTimeJoinedPlayerSquad = gpGlobals->curtime;
	m_bWasInPlayerSquad = true;
	if ( m_pSquad->NumMembers() > MAX_PLAYER_SQUAD )
	{
		CAI_BaseNPC *pFirstMember = m_pSquad->GetFirstMember(NULL);
		m_pSquad->RemoveFromSquad( pFirstMember );
	}
	ClearFollowTarget();

	CAI_BaseNPC *pLeader = NULL;
	AISquadIter_t iter;
	for ( CAI_BaseNPC *pAllyNpc = m_pSquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pSquad->GetNextMember(&iter) )
	{
		if ( pAllyNpc->IsCommandable() )
		{
			pLeader = pAllyNpc;
			break;
		}
	}

	if ( pLeader && pLeader != this )
	{
		const Vector &commandGoal = pLeader->GetCommandGoal();
		if ( commandGoal != vec3_invalid )
		{
			SetCommandGoal( commandGoal );
			SetCondition( COND_RECEIVED_ORDERS ); 
			OnMoveOrder();
		}
		else
		{
			CAI_FollowBehavior *pLeaderFollowBehavior;
			if ( pLeader->GetBehavior( &pLeaderFollowBehavior ) )
			{
				SetFollowTarget( pLeaderFollowBehavior->GetFollowTarget() );
				m_FollowBehavior.SetParameters( m_FollowBehavior.GetFormation() );
			}

		}
	}
	else
	{
		SetFollowTarget( UTIL_GetLocalPlayer() );
		m_FollowBehavior.SetParameters( AIF_SIDEKICK );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::ClearFollowTarget()
{
	SetFollowTarget( NULL );
	m_FollowBehavior.SetParameters( AIF_SIDEKICK );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTarget - 
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SetFollowTarget( CBaseEntity *pTarget )
{
	m_FollowBehavior.SetFollowTarget( pTarget );
	//m_hFollowTarget = pTarget;
	m_flObeyFollowTime = gpGlobals->curtime + CBOT_OBEY_FOLLOW_TIME;

	//SetCondition( COND_ANTLION_RECEIVED_ORDERS );

	// Play an acknowledgement noise
	//if ( m_flNextAcknowledgeTime < gpGlobals->curtime )
	//{
	//	EmitSound( "NPC_Antlion.Distracted" );
	//	m_flNextAcknowledgeTime = gpGlobals->curtime + 1.0f;
	//}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct SquadMemberInfo_t
{
	CNPC_CompanionBot *	pMember;
	bool			bSeesPlayer;
	float			distSq;
};

int __cdecl BotSquadSortFunc( const SquadMemberInfo_t *pLeft, const SquadMemberInfo_t *pRight )
{
	if ( pLeft->bSeesPlayer && !pRight->bSeesPlayer )
	{
		return -1;
	}

	if ( !pLeft->bSeesPlayer && pRight->bSeesPlayer )
	{
		return 1;
	}

	return ( pLeft->distSq - pRight->distSq );
}

CAI_BaseNPC *CNPC_CompanionBot::GetSquadCommandRepresentative()
{
	if ( !AI_IsSinglePlayer() )
		return NULL;

	if ( IsInPlayerSquad() )
	{
		static float lastTime;
		static AIHANDLE hCurrent;

		if ( gpGlobals->curtime - lastTime > 2.0 || !hCurrent || !hCurrent->IsInPlayerSquad() ) // hCurrent will be NULL after level change
		{
			lastTime = gpGlobals->curtime;
			hCurrent = NULL;

			CUtlVectorFixed<SquadMemberInfo_t, MAX_SQUAD_MEMBERS> candidates;
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

			if ( pPlayer )
			{
				AISquadIter_t iter;
				for ( CAI_BaseNPC *pAllyNpc = m_pSquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pSquad->GetNextMember(&iter) )
				{
					if ( pAllyNpc->IsCommandable() && dynamic_cast<CNPC_CompanionBot *>(pAllyNpc) )
					{
						int i = candidates.AddToTail();
						candidates[i].pMember = (CNPC_CompanionBot *)(pAllyNpc);
						candidates[i].bSeesPlayer = pAllyNpc->HasCondition( COND_SEE_PLAYER );
						candidates[i].distSq = ( pAllyNpc->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
					}
				}

				if ( candidates.Count() > 0 )
				{
					candidates.Sort( BotSquadSortFunc );
					hCurrent = candidates[0].pMember;
				}
			}
		}

		if ( hCurrent != NULL )
		{
			Assert( dynamic_cast<CNPC_CompanionBot *>(hCurrent.Get()) && hCurrent->IsInPlayerSquad() );
			return hCurrent;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::SetSquad( CAI_Squad *pSquad )
{
	bool bWasInPlayerSquad = IsInPlayerSquad();

	BaseClass::SetSquad( pSquad );

	if( IsInPlayerSquad() && !bWasInPlayerSquad )
	{
		m_OnJoinedPlayerSquad.FireOutput(this, this);
	}
	else if ( !IsInPlayerSquad() && bWasInPlayerSquad )
	{
		m_OnLeftPlayerSquad.FireOutput(this, this);
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_CompanionBot::InputSetCommandable( inputdata_t &inputdata )
{
	RemoveSpawnFlags( SF_CITIZEN_NOT_COMMANDABLE );
	gm_PlayerSquadEvaluateTimer.Force();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_CompanionBot::InputSpeakIdleResponse( inputdata_t &inputdata )
{
	SpeakIfAllowed( TLK_ANSWER );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CompanionBot::DeathSound( const CTakeDamageInfo &info )
{
	// Sentences don't play on dead NPCs
	SentenceStop();

	EmitSound( "NPC_Citizen.Die" );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CNPC_CompanionBot::FearSound( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::UseSemaphore( void )
{
	// Ignore semaphore if we're told to work outside it
	if ( HasSpawnFlags(SF_CITIZEN_IGNORE_SEMAPHORE) )
		return false;

	return BaseClass::UseSemaphore();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_companionbot, CNPC_CompanionBot )

	DECLARE_TASK( TASK_DROP_HAT )
	DECLARE_TASK( TASK_COMPANION_IMPACT_EFFECT )

	DECLARE_ANIMEVENT( AE_SV_HAT_FLYOFF_FWD )

	DECLARE_CONDITION( COND_HAT_AVAILABLE )
	DECLARE_CONDITION( COND_COMPANION_OFF_GROUND )
	DECLARE_CONDITION( COND_COMPANION_HIT_GROUND )

	DEFINE_SCHEDULE
	(
	SCHED_GET_HAT,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_GET_PATH_TO_TARGET_WEAPON	0"
	"		TASK_ITEM_RUN_PATH				0"
	"		TASK_DROP_HAT					0"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_TARGET				0"
	"		TASK_ITEM_PICKUP				0"
	""
	"	Interrupts"
	);

	//=========================================================
	DEFINE_SCHEDULE
	(
	SCHED_COMPANION_FALL,

	"	Tasks"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_GLIDE"
	""
	"	Interrupts"
	"		COND_COMPANION_HIT_GROUND"
	)

	//=========================================================
	DEFINE_SCHEDULE
	(
	SCHED_COMPANION_HIT_GROUND,

	"	Tasks"
	"		TASK_COMPANION_IMPACT_EFFECT	0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_LAND"
	""
	"	Interrupts"
	)

AI_END_CUSTOM_NPC()

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CNPC_CompanionBot::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::IsOkToSpeak( ConceptCategory_t category, bool fRespondingToPlayer )
{
	return BaseClass::IsOkToSpeak(category, fRespondingToPlayer);
}

//-----------------------------------------------------------------------------
// Purpose: cause a bot to always say a line.
//-----------------------------------------------------------------------------
bool CNPC_CompanionBot::SpeakForce( const char *ResponseConcept, bool bCancelScene )
{
	// We're being forced to respond to the event, probably because it's the
	// player dying or something equally important. 
	AI_Response *result = SpeakFindResponse( ResponseConcept, NULL );
	if ( result )
	{
		// We've got something to say. Stop any scenes we're in, and speak the response.
		if ( bCancelScene )
			RemoveActorFromScriptedScenes( this, false );

		return SpeakDispatchResponse( ResponseConcept, result );
	}

	return false;
}

namespace EOZ_Hacks
{
	EHANDLE g_pMbot, g_pObot;
	unsigned int g_incarnations_mbot = 0;
	unsigned int g_incarnations_obot = 0;
}

CNPC_CompanionBot * CNPC_CompanionBot::GetBotInstance(CompanionBotRole_t role)
{
	if (role == CBOT_DEFENSIVE)
	{
		return static_cast<CNPC_CompanionBot *>(EOZ_Hacks::g_pMbot.Get());
	}
	else if (role == CBOT_OFFENSIVE)
	{
		return static_cast<CNPC_CompanionBot *>(EOZ_Hacks::g_pObot.Get());
	}
	else
	{
		AssertMsg1(false,"Called GetBotInstance with invalid role %d\n", role);
	}
	return NULL;
}

// HACK!!!!!!!
CNPC_CompanionBot * CNPC_CompanionBot::GetOtherBot()
{
	return CNPC_CompanionBot::GetBotInstance(GetBotType() == CBOT_OFFENSIVE ? CBOT_DEFENSIVE : CBOT_OFFENSIVE );
}

extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );
//------------------------------------------------------------------------------
// Display the hull type of the specified NPC.
//------------------------------------------------------------------------------
void CC_LookTarget( const CCommand &args )
{
	if ( !g_pAINetworkManager )
		return;

	CBaseEntity *pEnt = NULL;

	const char *pBot = "obot";
	if ( args[1] && args[1][0] )
	{	
		pBot = args[1];
	}

	pEnt = FindPickerEntity( UTIL_GetCommandClient() );
	if ( !pEnt )
	{
		DevMsg( "No entity under the crosshair.\n" );
		return;
	}

	CompanionBotRole_t whichBot = (CompanionBotRole_t)-1;
	if ( !Q_stricmp( pBot, "obot" ) )
	{
		whichBot = CBOT_OFFENSIVE;
	}
	else if ( !Q_stricmp( pBot, "mbot" ) )
	{
		whichBot = CBOT_DEFENSIVE;
	}

	if ( whichBot == -1 )
	{
		DevMsg( "Bad bot name %s\n", pBot );
		return;
	}

	CNPC_CompanionBot *pInstance = CNPC_CompanionBot::GetBotInstance( whichBot );
	if ( !pInstance )
	{
		DevMsg( "%s not found!\n", pBot );
		return;
	}
	pInstance->ForceNextLookTarget( pEnt );
}
static ConCommand eoz_obot_looktarget("eoz_looktarget", CC_LookTarget, "Force the specified bot to look at the entity under the crosshair. eoz_lookat <obot|mbot>", FCVAR_CHEAT);
