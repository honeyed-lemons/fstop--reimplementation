//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Photos
//
//=====================================================================================//

#ifndef PHOTO_H
#define PHOTO_H
#ifdef _WIN32
#pragma once
#endif

#include "info_placement_helper.h"

// ==============================================
// Photo pick-up / placement interface

// ==============================================

class ICapturableObject 
{
public:
	virtual bool MayBeCaptured( void ) = 0;
	virtual void OnCaptured( void ) = 0;
	virtual void OnReleased( void ) = 0;
};

#define CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS 8
#define CAPTURE_INFO_MAX_CUSTOM_SCALE_DOWN_MULTIPLIERS 8

// Holds QC specified scale levels/multipliers (if any).
struct CameraInfo_ScaleData_t
{
	DECLARE_SIMPLE_DATADESC();

	CameraInfo_ScaleData_t( void ) {};
	CameraInfo_ScaleData_t( float *pOrderedScales, int iOrderedScaleCount ); //parses the scales. MUST CONTAIN A 1.0!!!!

	// Number of growth/shrink steps from the default size.
	int					nNumScaleUpLevels;
	int					nNumScaleDownLevels;

	// Holds entries of scale factors at each growth of the growth/shrinks steps.
	// index 0 will always be 1.0, index > nNumScaleLevels will be 1.0 for saftey, but should be assert guarded.
	// TODO: These should be CUtlVectors, but CaptureInfo_t structs are memset alot... needs refactoring?
	float				flScaleUpLevelMultipliers[CAPTURE_INFO_MAX_CUSTOM_SCALE_UP_MULTIPLIERS];
	float				flScaleDownLevelMultipliers[CAPTURE_INFO_MAX_CUSTOM_SCALE_DOWN_MULTIPLIERS];
};

struct CaptureInfo_t
{
	DECLARE_DATADESC();

	CaptureInfo_t( void ) : 
		bHasCustomScaleData(false), 
		hCapturedEnt(NULL) { }

	// Used with sv_camera_hide_captured_ents
	EHANDLE									hCapturedEnt;

	bool			bHasCustomScaleData;		
	CameraInfo_ScaleData_t	customScaleData;

	CBaseEntity::CPhotoPlacementQuery *pPlacementQuery;

	// Used to preview various sizes, stored here to support inventory switching
	int				nPreviewScaleLevel;

	// Restore to old velocity
	Vector			vecVelocity;
	Vector			vecAngVelocity;

	// Used to replace items that have passed through a photo eraser
	Vector			vecOldOrigin;
	QAngle			vecOldAngles;
	int				nOldScaleLevel;
};

struct CheckPlacementData_t //data that gets passed around in placement checks
{
	CheckPlacementData_t( void ) { memset( this, 0, sizeof( *this ) ); }
	CHandle<CInfoPlacementHelper> hPlacementHelper;

	trace_t Trace; //trace that passed initial placement testing
	ITraceFilter *pTraceFilter;
	Vector vTraceOrigin; //usually player eye position
	Vector vTraceDirection; //usually player forward
	QAngle qTraceAngles; //usually player view angles

	int nScaleStep;
	float fScale;

	CBaseEntity *pPlacedEntity; //replaces g_placedEntity
	Vector vPlacedPosition; //replaces g_placedPosition
};

// Global inventory accessors
bool Photo_CanAdd( void );
int Photo_Add( CaptureInfo_t *pInfo );
bool Photo_Get( int nIndex, CaptureInfo_t *pOut );
bool Photo_Remove( int nIndex );
unsigned int Photo_Count( void );
void Photo_Purge( void );
void Photo_Cycle( bool bForward );
bool Photo_IsValid( int nIndex );
bool Photo_Update( int nIndex, CaptureInfo_t &pIn );

// Object placement
bool UTIL_CreateScaledPhysObject( CBaseAnimating *pInstance, float flNewScale );
CBaseEntity *UTIL_RestoreCapturedObject( CaptureInfo_t pInfo, const Vector &vecPoint, const QAngle &vecAngles, int nScaleLevel, CInfoPlacementHelper *pHelper = NULL );
void UTIL_FailurePlacement( const Vector &vecEndPoint, Vector *pOriginOut, QAngle *pAnglesOut );

// implemented in weapon_camera.cpp
bool UTIL_InitCaptureInfo( CaptureInfo_t& CaptureInfo, CBaseEntity* pObject, CBasePlayer* pCapturingPlayer = NULL );

// this is for cleaner queries of a baseentity, handles the casting to base animating, and returns 1.0 if it won't cast.
// returns the multiplier used to shrink/grow the model ( a greater than zero multiplier )
float UTIL_GetEntityScaleFactor( CBaseEntity* pEnt );

// returns the integer scale level of an entity
int UTIL_GetEntityScaleLevel( CBaseEntity* pEnt, int* pOutNumScaleDownSteps = NULL, int* pOutNumScaleUpSteps = NULL );

// FIXME: Move
void UTIL_AlignBBox( Vector &vecMins, Vector &vecMaxs );

extern int g_CurMaxInvPhotos;

#define FLASH_INVENTORY_BASIC		0
#define FLASH_INVENTORY_FULL		1
#define FLASH_INVENTORY_ADDED		2
#define FLASH_INVENTORY_STRIPPED	3

#define FLASH_INDICATOR_INVALID		0
#define FLASH_INDICATOR_FULL		1

#define CONTROL_STATE_CAMERA 0
#define CONTROL_STATE_NEUTRAL 1
#define CONTROL_STATE_PICTURE 2

// extern ConVar camera_capture_distance;

// FIXME: Clean up
extern Vector g_placedPosition;
extern CBaseEntity *g_placedEntity;

#endif // PHOTO_H
